// Copyright Enva Division

#pragma once

// UE4 JSON module
#include "Dom/JsonObject.h"

// UE4 WebSocket module
#include "IWebSocket.h"


DECLARE_LOG_CATEGORY_EXTERN(LogJsonRpc, Log, All);

/**
 * At minimum, the error codes from the JSON-RPC specification. Add any
 * additional codes defined by the server or this client implementation.
 * (Note: server-defined codes are in the range -32000 to -32099.)
 */
enum EJsonRpcError {
    /** Parse error. Invalid JSON was received by the server. An error
     * occurred on the server while parsing the JSON text. */
    ParseError = -32700,

    /** Invalid Request. The JSON sent is not a valid Request object. */
    InvalidRequest = -32600,

    /** Method not found. The method does not exist or is not available. */
    MethodNotFound = -32601,

    /** Invalid params. Invalid method parameter(s). */
    InvalidParams = -32602,

    /** Internal error. Internal JSON - RPC error. */
    InternalError = -32603,

    /** The FJsonRpc instance was closed before a response could be received */
    Closed = 10,

    /** The handler encountered an error and could not complete the procedure call */
    HandlerError = 20,
};

/**
 * Represents a result or an error. The Call implementation returns a future of
 * this type. The Handle() method of the FJsonHandler type parameter returns a
 * future of this type as well.
 */
struct FJsonRpcResponse {
    /** Indicates whether this response is a result (false) or an error (true). */
    bool IsError;

    /** If IsError, Result is nullptr */
    TSharedPtr<FJsonValue> Result;

    /** Error must be non - null if IsError is true. See FJsonRpc::Error for a
     * convenience method to set this value. */
    TSharedPtr<FJsonObject> Error;
};


/**
 * The FJsonRpcChannel is an abstract class that helps implement a two-way
 * stream of string messages. The outbound messages are sent by calling
 * the Send method, and inbound messages can be received by binding a lambda
 * or some other handler to the delegate returned by the OnMessage() method.
 * Note that this is a subset of the IWebSocket interface.
 */
class FJsonRpcChannel {
public:
    virtual ~FJsonRpcChannel() {};

    /**
     * Send a String message across the channel. In most cases this will be
     * serialized JSON in the message, but note that the Heartbeat channel can
	 * also send non JSON data, but it is not made visible to the user of the
	 * channel.
     */
    virtual void Send(const FString& Message) = 0;

    /**
	 * @param Code The error code is one of the standard WebSocket error codes.
	 * The range 5000+ is open for definition by the application, other values
	 * are reserved. See https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent/code
	 * @param Reason is limited to 123 bytes of UTF-8 text. See https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/close).
     */
	virtual void Close(const int32 Code, const FString& Reason) = 0;

    /**
	 * The OnMessage delegate is called when a message is received from the
	 * remote end of the channel.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(FChannelMessage, const FString&);
    FChannelMessage OnMessage;
};

/**
 * This is a concrete class that adapts an IWebSocket instance to the
 * FJsonRpcChannel interface. Since the FJsonRpcChannel is a small subset of
 * the IWebSocket interface, this implementation is almost trivial.
 */
class FJsonRpcWebSocketChannel : public FJsonRpcChannel
{
public:
    explicit FJsonRpcWebSocketChannel(const TSharedPtr<IWebSocket> InWS);

    virtual void Send(const FString& Message) override;
	virtual void Close(const int32 Code, const FString& Reason) override;
private:

    TSharedPtr<IWebSocket> WS;
};

/**
 * The paired channel is for local testing. Since the FJsonRpc implementation
 * handles asynchronously waiting for futures to obtain values, I think this
 * should "just work" whether or not the handler implementations on either side
 * are synchronous. Here's hoping.
 */
class FJsonRpcPairedChannel : public FJsonRpcChannel
{
public:

    FJsonRpcPairedChannel() {};
    FJsonRpcPairedChannel(TSharedPtr<FJsonRpcPairedChannel> InOther);
    virtual ~FJsonRpcPairedChannel() override {};

    TSharedPtr<FJsonRpcPairedChannel> Other;

    struct FPair {
        TSharedPtr<FJsonRpcPairedChannel> Local;
        TSharedPtr<FJsonRpcPairedChannel> Remote;
    };

    static FPair Create();

    virtual void Send(const FString& Message) override;

    /**
     * Note that for the paired channel, which is used for testing, closing is
     * a no-op.
     */
	virtual void Close(const int32 Code, const FString& Reason) override {};

};

/**
 * @brief This class wraps another channel and sends the keyword HEARTBEAT at
 * the specified cadence. If there is some other message on the inner channel,
 * then the next HEARTBEAT message is delayed so that the keep-alive behavior
 * does not introduce more overhead than is necessary to achieve the response
 * time desired for otherwise undetectable channel closure events.
 */
class FJsonRpcHeartbeatChannel : public FJsonRpcChannel
{

public:

    FJsonRpcHeartbeatChannel();
    virtual ~FJsonRpcHeartbeatChannel() override {};

    /**
     * @brief Since the UObject requires a no-argument constructor, the Init
     * method takes care of getting the necessary parameters.
     * @param CurrentWorld A reference to the current world. Since this isn't a
     * UObject derivative, it needs to be handed a UWorld to use to access the
     * timer system.
     * @param InnerChannel The channel we're wrapping through which messages
     * will ultimately be sent.  You may need to save a reference to the
     * underlying channel to access its other methods, but you should send and
     * receive messages through this channel.
     * @param HeartbeatCadence The time in seconds between heartbeats. If we
     * send a message, then the timer is reset, in order to minimize the
     * overhead of presence indication.
     */
    void Init(UWorld* CurrentWorld, const TSharedPtr<FJsonRpcChannel> InnerChannel, const float HeartbeatCadence);

    /**
     * @brief Starts the timer.
     */
    void Start();

    /**
	 * @brief A convenience function to initialize the channel and start the
	 * timer all at once.
     */
    void Start(UWorld* CurrentWorld, const TSharedPtr<FJsonRpcChannel> InnerChannel, const float HeartbeatCadence);

    /**
	 * @brief Stops the timer without closing the underlying connection.
     */
	void Stop();

    virtual void Send(const FString& Message) override;

	virtual void Close(const int32 Code, const FString& Reason) override;

private:
	TObjectPtr<UWorld> World;
    TSharedPtr<FJsonRpcChannel> Inner;
    float Cadence;
    TFunction<void(void)> Heartbeat;
    FTimerHandle TimerHandle;
};

/**
 * The FJsonRpcHandler type is an interface that defines only the Handle
 * method. Since the Handle method is where the business logic happens, the
 * class that holds the FJsonRpc instance will most likely implement this
 * interface, or do so with a nested class if there's a name conflict.
 */
class FJsonRpcHandler {
public:
    virtual ~FJsonRpcHandler() {};

    /**
     * Accepts a method name as a string and the parameters of the method call
     * as a JSON value, which could be of any type. The return value is a
     * TSharedFuture so that the implementation can do asynchronous work if
     * that is required. Otherwise, the implementer can create and resolve
     * a TPromise immediately.
     */
    virtual TSharedFuture<FJsonRpcResponse> Handle(FString Method, TSharedPtr<FJsonValue> Params) = 0;
};


/**
 * If your JsonRpc instance does not need to handle incoming calls, then this
 * handler conveniently does nothing and returns an error value. Note that the
 * notify mechanism is handled by a delegate, so this only applies to calls
 * that return a value.
 */
class FJsonRpcEmptyHandler final : public FJsonRpcHandler
{
public:
    /**
    * The implementation ignores its arguments and returns null.
    */
    virtual TSharedFuture<FJsonRpcResponse> Handle(FString Method, TSharedPtr<FJsonValue> Params) override;
};


/**
 * The FJsonRpc instance brings together a Channel and a Handler to start to
 * form a working RPC connection. The Handler implements part of the business
 * logic and varies per implementation. I say "part of the business logic"
 * because the handler only takes care of incoming RPC calls. The Call, and 
 * Notify methods, and the OnNotify() delegate are also involved in
 * implementing business logic over the RPC connection.
 * 
 * The chief advantage of having a Channel interface is dependency injection,
 * which decouples the RPC interface from a particular transport technology
 * like a WebSocket. The same RPC implementation can select to send the same
 * messages over a WebRTC data channel, HTTP, between local processes, or
 * simply pass them to another FJsonRpc instance for testing purposes, all
 * by selecting a different channel.
 */
class PASSAGE_API FJsonRpc
{
public:

    FJsonRpc(const TSharedPtr<FJsonRpcChannel> InChannel, const TSharedPtr<FJsonRpcHandler> InHandler);
    ~FJsonRpc() {};

    /**
     * Close out the RPC session by unwinding any pending promises with errors
     * so that the underlying channel can be closed. Note, however, it does
	 * not close the channel itself.
     */
    void Close();

    /**
     * Call is the method to use when you expect a response from the server,
     * or even simply want to check if the call succeeded or resulted in an
     * error. If you don't care about those things, use the Notify method
     * instead.
     * 
     * @param Method The name of the method to call, which the remote side will
     * use to look up the implementation, as though the remote side were a
     * regular object and we're just calling a method on it.
     * 
     * @param Params By the JSON-RPC spec, this should either be an array or an
     * object, but this is not strictly enforced by the implementation. The
     * remote side will get something like `impl[Method](...Params)` when
     * Params is an array.
     * 
     * @returns A future representing the response. The FJsonRpcResponse type
     * is an envelope that indicates whether the call succeeded and has a
     * usable value, or if it failed an contains an error message. Since
     * awaiting a future blocks the thread, you'll need to use the Unreal Async
     * function to await the future in a non-main thread.
     */
    TSharedFuture<FJsonRpcResponse>Call(const FString Method, const TSharedPtr<FJsonValue> Params);

    /**
     * Notify is analogous to Call, but it does not expect any response from
     * the remote side. This is best used for events that repeat frequently
     * so that subsequent Notify calls will make earlier ones stale, thus the
     * repetition itself makes error checking redundant. The overhead of a
     * Notify is less than that of a Call since you don't need to use another
     * thread or track/pair the reply.
     */
    void Notify(const FString Method, const TSharedPtr<FJsonValue> Params) const;

    /**
     * When a Notify comes in from the remote, the message is broadcast to this
     * delegate. The parameters to the delegate are the same Method and Params
     * received by the Notify and Call methods, but these originate on the
     * remote side and the delegate is responsible for doing whatever the
     * remote side is requesting.
     */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FJsonRpcNotify, FString Method, TSharedPtr<FJsonValue> Params)
    FJsonRpcNotify& OnNotify() { return JsonRpcNotify; }

    /**
     * A convenience function to create an error response when no valid result can
     * be provided to a method invocation. Mainly we'll use this in implementing
     * client side methods that respond to calls from the server, i.e. in a
     * specific implementation of the FJsonHandler interface.
     */
    static FJsonRpcResponse Error(EJsonRpcError Code, const FString& Message);

    /**
     * A convenience to write an FJsonRpcResponse object to the LogJsonRpc error
     * log via the UE_LOG macro. When you get an error from the remote, this is
     * a convenient way to log it.
     */
    static void Log(const FJsonRpcResponse& Response);

    /**
     * @brief An ovlerload 
     * @param Message The message to log with this value
     * @param Value The FJsonValue to serialize and log
     */
    static void Log(const FString& Message, const TSharedPtr<FJsonObject> Value);



private:
    TSharedPtr<FJsonRpcChannel> Channel;
    TSharedPtr<FJsonRpcHandler> Handler;

    int PendingCounter;
    TMap< FString, TPromise<FJsonRpcResponse> > Pending;

    FCriticalSection CriticalSection;

    FJsonRpcNotify JsonRpcNotify;

    void ProcessIncoming(const FString& Message);
    void ProcessSingle(TSharedPtr<FJsonObject>);

    FString GetIdAsString(TSharedPtr<FJsonObject>);

    /*
     * Send an error message over the Channel in response to a particular Id.
     * @param Code The error code to send (as a bare integer).
     * @param Message The detail message to send.
     * @param Id The "id" field to include in the response. This version is an
     * FJsonValue, which, by spec, can either be a number or a string. You
     * probably want to call the overloaded method with the appropriate type
     * instead of making your own FJsonValue.
     */
    void SendError(EJsonRpcError Code, const FString& Message, TSharedPtr<FJsonValue> Id);

    /* Overload with FString Id */
    void SendError(EJsonRpcError Code, const FString& Message, const FString& Id);

    /* Overload with int Id */
    void SendError(EJsonRpcError Code, const FString& Message, const int& Id);

    /*
     * Like the above, but sends null for the "id" field. This is for errors
     * that cannot be correlated to particular requests, i.e. there was a parse
     * error in the last message and the "id" field is unknown.
     */
    void SendError(EJsonRpcError Code, const FString& Message);

    /*
     * Pulls the error out of the response object and sends it with the given
     * Id.
     */
    void SendError(FJsonRpcResponse Response, const TSharedPtr<FJsonValue> Id);

    /* Overload with FString Id */
    void SendError(FJsonRpcResponse Response, const FString& Id);

    /* Overload with int Id */
    void SendError(FJsonRpcResponse Response, const int Id);

    /**
     * This is the static version of SendError that just needs a Channel, which
     * is supplied by the non-static methods. Note that the compiler does not
     * like it when the static method has the same name as a non-static method,
     * at least when it's trying to deal with a lambda.
     */
    static void SendErrorStatic(TSharedPtr<FJsonRpcChannel> Channel, FJsonRpcResponse Response, TSharedPtr<FJsonValue> Id);

    /*
     * Pulls the error info out of the response object and sends it with a null
     * "id" field.
     */
    void SendError(FJsonRpcResponse Response);
};

// Copyright Enva Division

#include "JsonRpc.h"

// UE4 JSON module
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

// UE4 WebSocket module
#include "IWebSocket.h"


DEFINE_LOG_CATEGORY(LogJsonRpc);


FJsonRpcWebSocketChannel::FJsonRpcWebSocketChannel(const TSharedPtr<IWebSocket> InWS)
{
    WS = InWS;
    WS->OnMessage().AddLambda([this](const FString& Message)
        {
            UE_LOG(LogJsonRpc, VeryVerbose,
				TEXT("FJsonRpcWebSocketChannel::WS->OnMessage()"));
            OnMessage.Broadcast(Message);
        }
    );
}

void FJsonRpcWebSocketChannel::Send(const FString& Message)
{
    UE_LOG(LogJsonRpc, VeryVerbose,
        TEXT("FJsonRpcWebSocketChannel::Send()"));
    WS->Send(Message);
}

void FJsonRpcWebSocketChannel::Close(const int32 Code, const FString& Reason)
{
	UE_LOG(LogJsonRpc, Verbose, TEXT("FJsonRpcWebSocketChannel::Close() %d %s"), Code, *Reason);
    WS->Close(Code, Reason);
}

FJsonRpcPairedChannel::FPair FJsonRpcPairedChannel::Create()
{
    auto Local = MakeShared<FJsonRpcPairedChannel>();
    const auto Remote = MakeShared<FJsonRpcPairedChannel>(Local);
    Local->Other = Remote;
    return { Local, Remote };
}

FJsonRpcPairedChannel::FJsonRpcPairedChannel(TSharedPtr<FJsonRpcPairedChannel> InOther)
{
    Other = InOther;
}

void FJsonRpcPairedChannel::Send(const FString& Message)
{
    auto OtherPtr = Other;
    Async(EAsyncExecution::Thread, [OtherPtr, Message]()
        {
            OtherPtr->OnMessage.Broadcast(Message);
        });
    
}

FJsonRpcHeartbeatChannel::FJsonRpcHeartbeatChannel()
	: World(nullptr), Inner(nullptr), Cadence(-1)
{
}

void FJsonRpcHeartbeatChannel::Init(UWorld* CurrentWorld, const TSharedPtr<FJsonRpcChannel> InnerChannel, const float HeartbeatCadence)
{
    this->World = CurrentWorld;
    this->Inner = InnerChannel;
    this->Cadence = HeartbeatCadence;

    Heartbeat = [this]()
    {
        if (Inner.IsValid()) {
            Inner->Send(TEXT("HEARTBEAT"));
        }
    };
}

void FJsonRpcHeartbeatChannel::Start()
{
	UE_LOG(LogJsonRpc, Verbose, TEXT("FJsonRpcHeartbeatChannel::Start()"));
    World->GetTimerManager().SetTimer(
        TimerHandle, FTimerDelegate::CreateLambda(Heartbeat), Cadence, true);

    Inner->OnMessage.AddLambda([this](const FString& Message)
        {
			UE_LOG(LogJsonRpc, Verbose, TEXT("FJsonRpcHeartbeatChannel::OnMessage()"));
            if (Inner.IsValid())
            {
                UE_LOG(LogJsonRpc, Verbose, TEXT("FJsonRpcHeartbeatChannel::OnMessage() Inner.IsValid() succeded"));
                OnMessage.Broadcast(Message);
            }
        });
}

void FJsonRpcHeartbeatChannel::Start(
    UWorld* CurrentWorld, const TSharedPtr<FJsonRpcChannel> InnerChannel, const float HeartbeatCadence)
{
    Init(CurrentWorld, InnerChannel, HeartbeatCadence);
    Start();
}

void FJsonRpcHeartbeatChannel::Stop()
{
	World->GetTimerManager().ClearTimer(TimerHandle);
}

void FJsonRpcHeartbeatChannel::Send(const FString& Message)
{
    if (IsInGameThread()) {
        Inner->Send(Message);
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(TimerHandle);
        TimerManager.SetTimer(TimerHandle, FTimerDelegate::CreateLambda(Heartbeat), Cadence, true);
    }
    else
    {
		Async(EAsyncExecution::TaskGraphMainThread, [this, Message]()
			{
				Send(Message);
			});
    }
}

void FJsonRpcHeartbeatChannel::Close(const int32 Code, const FString& Reason)
{
    // May not be valid, if, for example, the Inner was never initialized
    if(Inner.IsValid())
    {
        World->GetTimerManager().ClearTimer(TimerHandle);
        Inner->Close(Code, Reason);
    }
}


TSharedFuture<FJsonRpcResponse> FJsonRpcEmptyHandler::Handle(
    FString Method, TSharedPtr<FJsonValue> Params)
{
    TPromise<FJsonRpcResponse> Promise;
    const FString Message = TEXT("This handler implements no callable methods");
    Promise.SetValue(FJsonRpc::Error(EJsonRpcError::MethodNotFound, Message));
    return Promise.GetFuture().Share();
}

FJsonRpc::FJsonRpc(const TSharedPtr<FJsonRpcChannel> InChannel, const TSharedPtr<FJsonRpcHandler> InHandler)
    : PendingCounter(0)
{
    Channel = InChannel;
    Handler = InHandler;
    Channel->OnMessage.AddLambda([this](const FString& Message){
            ProcessIncoming(Message);
        });
}


void FJsonRpc::Close()
{
    // We have to unwind all these promises or we end up with a crash on exit
    // from play-in-editor.
    const auto Error = FJsonRpc::Error(Closed,
        TEXT("No response received before FJsonRpc.Close()"));
    // We frequently see a memory access violation in the condition of this for
    // loop. Not exactly sure what could be invalid here. Is it possibly a race
    // condition on `this` itself if we're being deleted from another thread?
    for(auto& Entry : Pending)
    {
        Entry.Value.SetValue(Error);
    }
    
    Pending.Empty();
    UE_LOG(LogJsonRpc, VeryVerbose, TEXT("FJsonRpc::Close()"));
}

void FJsonRpc::Notify(const FString Method, const TSharedPtr<FJsonValue> Params) const
{
    const auto MsgObject = MakeShared<FJsonObject>();

    // ReSharper disable once StringLiteralTypo
    MsgObject->SetStringField("jsonrpc", "2.0");
    MsgObject->SetStringField("method", Method);
    MsgObject->SetField("params", Params);

    FString Message;
    const auto Writer = TJsonWriterFactory<>::Create(&Message);
    FJsonSerializer::Serialize(MsgObject, Writer);
    
    UE_LOG(LogJsonRpc, VeryVerbose, TEXT("FJsonRpc::Notify() sending JSON: %s"), *Message);

    Channel->Send(Message);
}

TSharedFuture<FJsonRpcResponse> FJsonRpc::Call(
        const FString Method, const TSharedPtr<FJsonValue> Params)
{
    UE_LOG(LogJsonRpc, Verbose, TEXT("FJsonRpc::Call() method = '%s'"), *Method);
    FString Id;
    {
        FScopeLock Lock(&CriticalSection);
        PendingCounter++;
        Id = FString::Printf(TEXT("%d"), PendingCounter);
    }

    const auto MsgObject = MakeShared<FJsonObject>();

    // ReSharper disable once StringLiteralTypo
    MsgObject->SetStringField("jsonrpc", "2.0");
    MsgObject->SetStringField("method", Method);
    MsgObject->SetField("params", Params);
    MsgObject->SetStringField("id", Id);

    FString Message;
    const auto Writer = TJsonWriterFactory<>::Create(&Message);
    FJsonSerializer::Serialize(MsgObject, Writer);

    UE_LOG(LogJsonRpc, VeryVerbose, 
        TEXT("FJsonRpc::Call() sending call with Id '%s' JSON:\n%s"), *Id, *Message);
    Channel->Send(Message);

    Pending.Add(Id, TPromise<FJsonRpcResponse>());
    return Pending[Id].GetFuture().Share();
}

FJsonRpcResponse FJsonRpc::Error(EJsonRpcError Code, const FString& Message)
{
    auto Error = MakeShared<FJsonObject>();
    Error->SetNumberField("code", Code);
    Error->SetStringField("message", Message);
    FJsonRpcResponse Response = {
        true,
        nullptr,
        Error,
    };
    return Response;
}

void FJsonRpc::Log(const FJsonRpcResponse& Response)
{
    if (Response.IsError) {
        FString Message = Response.Error->GetStringField("message");
        int Code = Response.Error->GetNumberField("code");
        UE_LOG(LogJsonRpc, Error, TEXT("JsonRpc Error (%d): %s"), Code, *Message);
    }
    else {
        UE_LOG(LogJsonRpc, Error, TEXT("Attempt to log non-error response"));
        FDebug::DumpStackTraceToLog(ELogVerbosity::Error);
    }
}

void FJsonRpc::Log(const FString& Message, const TSharedPtr<FJsonObject> Value)
{
    FString Payload;
    auto Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Value.ToSharedRef(), Writer);
    UE_LOG(LogJsonRpc, Verbose, TEXT("%s:\n%s"), *Message, *Payload);
}

/*
 * Note that this implementation doesn't really deal with incomming calls, so
 * it's not a full implementation of the JSON RPC protocol. But it's enough to
 * handle the subset used by Ion.
 */
void FJsonRpc::ProcessIncoming(const FString& Message)
{
    UE_LOG(LogJsonRpc, VeryVerbose,
        TEXT("FJsonRpc::ProcessIncoming() received %d chars:\n%s"),
        (Message.Len()), // the parens help JetBrains understand the method call
        *Message);

    const auto Reader = TJsonReaderFactory<>::Create(Message);

    if(TSharedPtr<FJsonValue> MsgValue; !FJsonSerializer::Deserialize(Reader, MsgValue))
    {
        // I belive we're supposed to send back a error with code -32700 (parse
        // error), with a null "id" property.
        UE_LOG(LogJsonRpc, Error,
            TEXT("FJsonRpc::ProcessIncoming(): Unable to deserialize JSON RPC message: '%s'"), *Message);
        SendError(ParseError, TEXT("Unable to deserialize request JSON"));
    }
    else 
    {
        TArray< TSharedPtr<FJsonValue> > const* Batch;
        TSharedPtr<FJsonObject> const* SingleMsg;
        if (MsgValue->TryGetArray(Batch)) {
            for (auto Item : *Batch) {
                auto MsgObject = Item->AsObject();
                ProcessSingle(MsgObject);
            }
        }
        else if (MsgValue->TryGetObject(SingleMsg))
        {
            ProcessSingle(*SingleMsg);
        }
        else
        {
            FString ErrMessage = TEXT("JSON-RPC request is neither an array nor an object");
            UE_LOG(LogJsonRpc, Error, TEXT("%s"), *ErrMessage);
            SendError(InvalidRequest, ErrMessage);
        }
        
    }
}

void FJsonRpc::ProcessSingle(TSharedPtr<FJsonObject> MsgObject)
{
    FJsonRpc::Log(TEXT("FJsonRpc::ProcessSingle() got MsgObject"), MsgObject);

    // This method uses early return, so be aware that the checks are
    // sequential and so have a kind of priority order.
    // 
    // First we check for error and log it. If we have a result, then we
    // resolve the corresponding pending promise. Next, if there's a method and
    // no id, we have a notify. If we have a method with a valid id, then we
    // have a call, and we pass it to the Handler.

    if (MsgObject->HasField(TEXT("error")))
    {
        // Will it be any help to also log the id?
        auto Error = MsgObject->GetObjectField("error");
        FJsonRpc::Log({ true, nullptr, Error });
        return;
    }

    // We have to use this more awkward syntax because we don't know the type
    // of the result field. We can't use the FJsonValue::GetField template.
    //const TSharedPtr<FJsonObject>* Result = nullptr;
    //if (MsgObject->TryGetObjectField(TEXT("result"), Result))
    if(MsgObject->HasField("result"))
    {
        const auto Result= MsgObject->Values["result"];
        if (MsgObject->HasField("id"))
        {
            const FString Id = FJsonRpc::GetIdAsString(MsgObject);

            if (Pending.Contains(Id))
            {
                const FJsonRpcResponse Response = {
                    false,
                    Result,
                    nullptr,
                };
                Pending[Id].SetValue(Response);
                Pending.Remove(Id);
                return;
            }
            else
            {
                const FString Message = FString::Printf(TEXT("Unmatched response id=%s"), *Id);
                UE_LOG(LogJsonRpc, Error, TEXT("%s"), *Message);
                SendError(InvalidRequest, Message);
                return;
            }
        }
        else
        {
            const auto ErrMessage = TEXT("No id field in the response");
            UE_LOG(LogJsonRpc, Error, TEXT("%s"), &*ErrMessage);
            SendError(InvalidRequest, ErrMessage);
            return;
        }
    }

    // If there's no `id` field, check for a method because we're handling
    // a notify instead.
    if (MsgObject->HasField(TEXT("method")))
    {
        auto Method = MsgObject->GetStringField("method");
        // If we have an Id, then it's a call
        if (MsgObject->HasField("id"))
        {
            UE_LOG(LogJsonRpc, Verbose, TEXT("Got CALL with method name '%s'"), *Method);
            auto Params = MsgObject->TryGetField("params");
            auto Id = MsgObject->TryGetField("id");
            if (Params)
            {
                auto Future = Handler->Handle(Method, Params);

                // A copy of the channel shared pointer so that the channel
                // won't be freed while awaiting the Future
                auto ChannelPtr = Channel;

                // setup an Async to send this when the promise resolves
                Async(EAsyncExecution::Thread, [Future, ChannelPtr, Id]() {

                    FJsonRpcResponse Response;
                    if(Future.WaitFor({0, 0, 30}))
                    {
                        Response = Future.Get();
                    }
                    else
                    {
                        UE_LOG(LogJsonRpc, Error,
                            TEXT("FJsonRpc::ProcessSingle() Async lambda: Timed out waiting for response"));
                        return;
                    }

                    if (Response.IsError)
                    {
                        FJsonRpc::Log(Response);
						SendErrorStatic(ChannelPtr, Response, Id);
                    }
                    else
                    {
                        auto MsgObject = MakeShared<FJsonObject>();

                        MsgObject->SetStringField(TEXT("jsonrpc"), "2.0");
                        MsgObject->SetField(TEXT("id"), Id);
                        MsgObject->SetField(TEXT("result"), Response.Result);

                        FString Payload;
                        auto Writer = TJsonWriterFactory<>::Create(&Payload);
                        FJsonSerializer::Serialize(MsgObject, Writer);

                        ChannelPtr->Send(Payload);
                    }
                });
            }
            else
            {
				UE_LOG(LogJsonRpc, Error, TEXT("FJsonRpc::ProcessSingle() No params field in the call"));
				SendError(InvalidRequest, TEXT("No params field in the call"));
            }
        }
        // Otherwise, we do not have an `id` field, so this is a notify and will not receive a response
        else
        {
            UE_LOG(LogJsonRpc, Verbose,
                TEXT("FJsonRpc::ProcessSingle() Got NOTIFY with method name '%s'"), *Method);
            if (const auto Params = MsgObject->TryGetField("params"))
            {
                OnNotify().Broadcast(Method, Params);
            }
            else
            {
				SendError(InvalidRequest, TEXT("No params field in the notify"));
            }
        }
    }
    else
    {
        UE_LOG(LogJsonRpc, Error,
            TEXT("FJsonRpc::ProcessSingle() no method field in message that is neither error nor response"));
    }
}

FString FJsonRpc::GetIdAsString(TSharedPtr<FJsonObject> Object)
{
    FString StringId;
    int IntId;
    if (Object->TryGetStringField("id", StringId))
    {
        return StringId;
    }
    else if (Object->TryGetNumberField("id", IntId))
    {
        return FString::Printf(TEXT("%d"), IntId);
    }
    else
    {
        UE_LOG(LogJsonRpc, Error, TEXT("No 'id' field"));
        return "";
    }
}

void FJsonRpc::SendError(EJsonRpcError Code, const FString& Message, TSharedPtr<FJsonValue> Id)
{
    auto MsgObject = MakeShared<FJsonObject>();
    auto Error = MakeShared<FJsonObject>();

    Error->SetNumberField("code", Code);
    Error->SetStringField("message", Message);

    MsgObject->SetStringField("jsonrpc", "2.0");
    MsgObject->SetField("id", Id);
    MsgObject->SetObjectField("error", Error);

    FString Payload;
    auto Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(MsgObject, Writer);

    UE_LOG(LogJsonRpc, VeryVerbose, TEXT("FJsonRpc::SendError() sending JSON: %s"), *Payload);
    Channel->Send(Payload);
}


void FJsonRpc::SendError(EJsonRpcError Code, const FString& Message, const FString& Id)
{
    auto IdValue = MakeShared<FJsonValueString>(Id);
    SendError(Code, Message, IdValue);
}

void FJsonRpc::SendError(EJsonRpcError Code, const FString& Message, const int& Id)
{
    auto IdValue = MakeShared<FJsonValueNumber>(Id);
    SendError(Code, Message, IdValue);
}

void FJsonRpc::SendError(EJsonRpcError Code, const FString& Message)
{
    auto IdValue = MakeShared<FJsonValueNull>();
    SendError(Code, Message, IdValue);
}

void FJsonRpc::SendError(FJsonRpcResponse Response, const TSharedPtr<FJsonValue> Id)
{
    FJsonRpc::SendErrorStatic(Channel, Response, Id);
}

void FJsonRpc::SendErrorStatic(TSharedPtr<FJsonRpcChannel> Channel, FJsonRpcResponse Response, TSharedPtr<FJsonValue> Id)
{
    auto MsgObject = MakeShared<FJsonObject>();

    MsgObject->SetStringField("jsonrpc", "2.0");
    MsgObject->SetField("id", Id);
    MsgObject->SetObjectField("error", Response.Error);

    FString Payload;
    auto Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(MsgObject, Writer);

    UE_LOG(LogJsonRpc, VeryVerbose, TEXT("FJsonRpc::SendError() sending JSON: %s"), *Payload);
    Channel->Send(Payload);
}

void FJsonRpc::SendError(FJsonRpcResponse Response, const FString& Id)
{
    auto IdValue = MakeShared<FJsonValueString>(Id);
    SendError(Response, IdValue);
}

void FJsonRpc::SendError(FJsonRpcResponse Response, const int Id)
{
    auto IdValue = MakeShared<FJsonValueNumber>(Id);
    SendError(Response, IdValue);
}

void FJsonRpc::SendError(FJsonRpcResponse Response)
{
    auto IdValue = MakeShared<FJsonValueNull>();
    SendError(Response, IdValue);
}
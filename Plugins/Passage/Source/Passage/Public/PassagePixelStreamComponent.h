// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "JsonRpc.h"
#include "Components/ActorComponent.h"
#include "PixelStreamingInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PassagePixelStreamComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPassagePixelStream, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPixelStreamInit);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PASSAGE_API UPassagePixelStreamComponent : public UActorComponent
{
	GENERATED_BODY()

		// Allow the PlayerController to call our private methods. Scandalous. This
		// enables the PlayerController to provide Exec commands that act like
		// incoming RPC messages.
		friend class APassagePlayerController;

private:

	/**
	 * We use the PixelStreamingInputComponent to send and receive RPC
	 * messages over the plugin's WebRTC data connection to the browser.
	 * This JsonRpc instance refers to that component.
	 */
	TSharedPtr<FJsonRpc> JsonRpc;

	TSharedPtr<FJsonRpcChannel> PixelStreamChannel;
	TSharedPtr<FJsonRpcHandler> PixelStreamRpcHandler;

	/**
	 * Our sibling component, a child of the PassagePlayerController. This
	 * is initialized in OnBeginPlay().
	 */
	UPROPERTY()
	UPixelStreamingInput* PixelStreamingInput;

	/**
	 * This is the auth token that we get from the browser during the Init()
	 * RPC call. The directory provider will get it from us when we connect to
	 * the directory.
	 */
	FString AuthToken;

public:
	/**
	 * Broadcast when we have an auth token.
	 */
	UPROPERTY(BlueprintAssignable)
	FPixelStreamInit OnPixelStreamInit;

	/**
	 * Sets default values for this component's properties
	 */
	UPassagePixelStreamComponent();
	virtual ~UPassagePixelStreamComponent() override;

	UFUNCTION(BlueprintCallable)
	const FString& GetAuthToken() const;

	UFUNCTION(BlueprintCallable)
	bool IsPixelStreamReady() const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:

	/**
	 * This method handles messages coming from the browser. The messages originate
	 * in Javascript from the emitUIInteraction() function. The descriptor string
	 * is parsed as JSON and dispatched to our RPC implementation if it is denoted
	 * as an RPC event, otherwise it is ignored, assumed to be handled by some
	 * other component.
	 */
	UFUNCTION()
	void OnInputEvent(const FString& Descriptor);

	/**
	 * Handles the OnStreamerDisconnected event. This handler is registered in
	 * this class's BeginPlay() method.
	 */
	UFUNCTION()
	void RestartOnDisconnect();

	/**
	 * @brief Handles the OnConnectedToSignallingServer event.
	 */
	UFUNCTION()
	void SendAvailableFlagToBackend();

	/**
	 * Handles the Init() RPC call from the browser. This is the first RPC call
	 * from the browser and passes us the authentication token that establishes
	 * us as being attached to a particular user (who already has a session with
	 * the backend).
	 */
	void HandleInit(const FString& InAuthToken);

	/**
	 * Handles the Teleport() RPC call from the browser. The
	 */
	void HandleTeleport(const FString& ParticipantId);

	void HandleSetInputMode(const FString& Mode, const bool IsTouch);

	void HandleSetResolution(const int32 Width, const int32 Height);


	/**
	 * This inner class implements the Send method that can send RPC messages
	 * to the browser over the pixel stream data channel. It also inherits a
	 * delegate that is called when the browser sends us an RPC message.
	 */
	class FPixelStreamChannel : public FJsonRpcChannel
	{
	public:

		FPixelStreamChannel(UPassagePixelStreamComponent* InParent)
			: Parent(InParent)
		{
		}

		/**
		 * Sends a message to the browser via the response mechanism.
		 */
		virtual void Send(const FString& Message) override;

		/**
		 * This does nothing since the underlying channel may remain open for other uses
		 */
		virtual void Close(const int32 Code, const FString& Reason) override {}

	private:

		TObjectPtr<UPassagePixelStreamComponent> Parent;

	};

	/**
	 * This inner class implements a handle method that calls back into the
	 * outer class.
	 */
	class FPixelStreamRpcHandler : public FJsonRpcHandler
	{
	public:
		FPixelStreamRpcHandler(UPassagePixelStreamComponent* InParent)
			: Parent(InParent)
		{
		}

		virtual TSharedFuture<FJsonRpcResponse> Handle(FString Method, TSharedPtr<FJsonValue> Params) override;

	private:

		TObjectPtr<UPassagePixelStreamComponent> Parent;

		/**
		 * This is a convenience function to create a temporary promise, set its
		 * value and return a shared future. This is for functions that do not
		 * actually need to be asynchronous.
		 */
		TSharedFuture<FJsonRpcResponse> ImmediateResponse(const FJsonRpcResponse Value);
	};
};


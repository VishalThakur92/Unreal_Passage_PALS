// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DirectoryProvider.h"
#include "JsonRpc.h"
#include "PassageWebSocketsModule.h"

#include "PassageDirectoryProvider.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPassageDirectoryProvider, Log, All);

/**
 * 
 */
UCLASS(BlueprintType)
class PASSAGE_API UPassageDirectoryProvider : public UDirectoryProvider
{
	GENERATED_BODY()
	
public:

	// This class is defined below the current class declaration
	class FRpcHandler;

	UPassageDirectoryProvider();
	virtual ~UPassageDirectoryProvider() override;;

	/**
	 * @brief Starts a connection to the directory on a backend instance.
	 * @param DirectoryUrl The URL of the directory server. This is the backend
	 * Express application in production, but it may be localhost in dev or
	 * test.
	 * @param DirectoryToken An auth token, which should be received from the
	 * frontend over the PixelStreaming connection.
	 */
	UFUNCTION(BlueprintCallable)
	void Connect(const FString& DirectoryUrl, const FString& DirectoryToken);

	UFUNCTION(BlueprintCallable)
	void Disconnect() const;

	// See UDirectoryProvider for documentation
	virtual UParticipant* GetLocalParticipant_Implementation() override;

	// See UDirectoryProvider for documentation
	virtual TArray< UParticipant* > GetParticipantsOnServer_Implementation() override;

	// This is used in testing, but maybe should be revised so that the tests
	// don't need to reach into the internals of the class.
	TSharedPtr<FJsonRpcHandler> GetHandler();

private:

	TSharedPtr<FJsonRpc> Rpc;
	TSharedPtr<FJsonRpcHandler> Handler;

	UPROPERTY()
	UParticipant* LocalParticipant;

	TSharedPtr<FJsonRpcHeartbeatChannel> HeartbeatChannel;

	/**
	 * This is called by the server immediately after the WebSocket connection
	 * is made. It informs this game instance which participant it represents.
	 */
	FJsonRpcResponse Joined(const TSharedPtr<FJsonObject>& Object);

	/**
	 * This is an RPC procedure called by the server. Receives new participants
	 * who have come online as well as the initial batch of participants just
	 * after joining. This also acts as an "upsert" meaning that if a given
	 * Participant already exsits (matched by Id), then its properties will be
	 * updated with the new values.
	 */
	FJsonRpcResponse AddParticipants(const TArray< TSharedPtr<FJsonValue> > Data);

	/**
	 * Receives an array of JSON strings representing the Participant IDs to
	 * remove. This is a remote procedure called by the server.
	 */
	FJsonRpcResponse RemoveParticipants(const TArray< TSharedPtr<FJsonValue> > Data);

	/**
	 * This is also remote procedure called by the server.
	 * Receives an array of triples representing updates to the properties of
	 * participants that we should already have locally. Each triplet is an
	 * object with "id", "property", and "value" keys for the Participant ID,
	 * property name, and property value respectively. The property names are
	 * "id", "isLocal", "active", "screenName", "serverLocation", and "data".
	 */
	FJsonRpcResponse UpdateParticipants(const TArray< TSharedPtr<FJsonValue> > ParticipantObjects);

	/**
	 * This is a helper function for the and and update procedures above.
	 */
	static void UpdateParticipant(UParticipant* Participant, TSharedPtr<FJsonObject> Object);

};

class UPassageDirectoryProvider::FRpcHandler : public FJsonRpcHandler
{
public:
	explicit FRpcHandler(UPassageDirectoryProvider* InParent);
	virtual ~FRpcHandler() override {};

	/**
	 * Implements the FJsonRpcHandler interface
	 */
	virtual TSharedFuture<FJsonRpcResponse> Handle(FString Method, TSharedPtr<FJsonValue> Params) override;

private:
	UPassageDirectoryProvider* Parent;
};

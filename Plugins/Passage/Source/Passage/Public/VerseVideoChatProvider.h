// Copyright Enva Division, 2022

#pragma once

#include "CoreMinimal.h"

#include "VerseConnection.h"
#include "VideoChatProvider.h"

#include "VerseVideoChatProvider.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVerseVideoChatProvider, Log, All);

USTRUCT()
struct FParticipantInfo
{
	GENERATED_BODY()

	UVerseConnection* VerseConnection;

	UMaterialInstanceDynamic* Material;
	FString MaterialParameter;
	UAudioComponent* AudioComponent;
};

UCLASS(Blueprintable)
class PASSAGE_API UVerseVideoChatProvider : public UVideoChatProvider
{
	GENERATED_BODY()

public:

	UVerseVideoChatProvider();

	/*
	 * See the defining interface method, VideoChatProvider::AttachMedia.
	 */
	void AttachMedia_Implementation(
		UPARAM() UParticipant* Participant,
		UPARAM() UMaterialInstanceDynamic* Material,
		UPARAM() const FString& ParameterName,
		UPARAM() UAudioComponent* AudioComponent
		);

	void DetachMedia_Implementation(UParticipant* Participant);

	/*
	 * The Url identifies the Ion SFU's signalling interface, which should
	 * be a WebSocket capable HTTP server. The protocol can be ws:// or wss://.
	 * The URL should not include a '/' at the end. This is passed unchanged
	 * as the first parameter to the VerseConnection::Connect method.
	 */
	UPROPERTY(EditAnywhere)
	FString Url;

	UFUNCTION(BlueprintCallable)
	void SetUrl(UPARAM(ref) FString& Value);


private:

	TMap<UParticipant*, FParticipantInfo> ParticipantInfo;
	UVerseConnectionFactory* ConnectionFactory;

};
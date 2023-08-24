// Copyright Enva Division, 2022

#pragma once

#include "CoreMinimal.h"

#include "ConnectionStatus.h"
#include "Participant.h"

#include "DirectoryProvider.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FParticipantEvent, UParticipant*, Participant);

	
UCLASS(Abstract, Blueprintable)
class PASSAGE_API UDirectoryProvider : public UObject
{
    GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadonly)
	TObjectPtr<UConnectionStatus> Connection;

	/**
	 * Returns the local participant, i.e. the Participant that represents the
	 * user currently controlling the game instance.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	UParticipant* GetLocalParticipant();

	/**
	 * Sends any property changes to the LocalParticipant to the directory
	 * service.
	 */
	// Deferring implementation because there's no need for this yet (I think)
	//UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	//void UpdateLocalParticipant();

	/**
	 * Returns all the participants currently known who share their
	 * ServerLocation with the local participant. This gets the latest local
	 * data, it does not actually query the directory server.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	TArray< UParticipant* > GetParticipantsOnServer();

	/**
	 * Returns the participant with the given Id. If the participant does not
	 * already exist, it will be created.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	UParticipant* GetParticipantById(const FString& Id);

	UPROPERTY(BlueprintAssignable, BlueprintCallable)
	FParticipantEvent OnParticipantJoined;

	UPROPERTY(BlueprintAssignable, BlueprintCallable)
	FParticipantEvent OnParticipantLeft;

	UPROPERTY(BlueprintAssignable, BlueprintCallable)
	FParticipantEvent OnParticipantUpdated;


protected:

	UPROPERTY()
	TMap<FString, UParticipant*>  ParticipantsById;

};
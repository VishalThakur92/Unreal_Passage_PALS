// Copyright Enva Division, 2022

#pragma once

#include "CoreMinimal.h"
#include "DirectoryProvider.h"

#include "StandaloneDirectoryProvider.generated.h"

/*
 * The StandaloneDirectoryProvider is an implementation of UDirectoryProvider
 * that only supports a local participant. This is useful for developer testing
 * where the game can be booted without having to connect to a remote directory
 * or to run a directory locally.
 */
UCLASS(Blueprintable)
class PASSAGE_API UStandaloneDirectoryProvider : public UDirectoryProvider
{
	GENERATED_BODY()

public:

	UStandaloneDirectoryProvider();
	
	virtual UParticipant* GetLocalParticipant_Implementation() override;

	virtual TArray< UParticipant* > GetParticipantsOnServer_Implementation() override;

	/*
	* For use only in testing, this adds a new participant with the given
	* RemoteId and broadcasts the OnParticipantJoined event as appropriate.
	*/
	UFUNCTION(BlueprintCallable)
	UParticipant* AddFakeParticipant(UPARAM(ref) const FString& RemoteId);

	/*
	* For use only in testing, this removes the given participant and
	* broadcasts the OnParticipantLeft event.
	*/
	UFUNCTION(BlueprintCallable)
	void RemoveFakeParticipant(UPARAM() UParticipant* Participant);

private:

	TObjectPtr<UParticipant> LocalParticipant;

	UPROPERTY()
	TArray< UParticipant* > AllParticipants;

	static FString RandomString(const size_t Length);
};
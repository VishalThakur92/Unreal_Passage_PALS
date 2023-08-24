// Fill out your copyright notice in the Description page of Project Settings.


#include "DirectoryProvider.h"


UParticipant* UDirectoryProvider::GetLocalParticipant_Implementation() {
	return nullptr;
}

//void UDirectoryProvider::UpdateLocalParticipant_Implementation()
//{
//	// nothing in default implementation
//}

TArray<UParticipant*> UDirectoryProvider::GetParticipantsOnServer_Implementation()
{
	TArray<UParticipant*> Empty;
	return Empty;
}

UParticipant* UDirectoryProvider::GetParticipantById_Implementation(const FString& Id)
{
	if (ParticipantsById.Contains(Id))
	{
		return ParticipantsById[Id];
	} else
	{
		const auto Participant = NewObject<UParticipant>(this);
		Participant->Id = Id;
		ParticipantsById.Add(Id, Participant);
		return Participant;
	}
}

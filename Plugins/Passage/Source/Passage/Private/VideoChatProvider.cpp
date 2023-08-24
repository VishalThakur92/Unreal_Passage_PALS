// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoChatProvider.h"

UVideoChatProvider::UVideoChatProvider()
{
	Status = NewObject<UConnectionStatus>();
}

void UVideoChatProvider::AttachMedia_Implementation(
	UParticipant* Participant,
	UMaterialInstanceDynamic* Material,
	const FString& ParameterName,
	UAudioComponent* AudioComponent
)
{
	// allow subclass to implement this
}

void UVideoChatProvider::DetachMedia_Implementation(UParticipant* Participant)
{
	// allow subclass to implement this
}
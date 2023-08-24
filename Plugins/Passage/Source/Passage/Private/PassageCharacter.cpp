// Fill out your copyright notice in the Description page of Project Settings.


#include "PassageCharacter.h"

DEFINE_LOG_CATEGORY(LogPassageCharacter)

// Sets default values
APassageCharacter::APassageCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

FString APassageCharacter::GetParticipantId_Implementation()
{
	UE_LOG(LogPassageCharacter, Error,
		TEXT("APassageCharacter::GetParticipantId_Implementation() Should be overridden by a Blueprint implementation"));
	return TEXT("ERROR");
}

void APassageCharacter::SetInputMode_Implementation(const EPassageInputMode Mode)
{
	UE_LOG(LogPassageCharacter, Error,
		TEXT("APassageCharacter::SetInputMode_Implementation() Should be overridden by a Blueprint implementation"));
}

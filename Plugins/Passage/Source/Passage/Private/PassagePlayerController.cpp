// Fill out your copyright notice in the Description page of Project Settings.


#include "PassagePlayerController.h"

#include "PassageCharacter.h"
#include "PassageGlobals.h"
#include "PassagePixelStreamComponent.h"
#include "GameFramework/GameUserSettings.h"

DEFINE_LOG_CATEGORY(LogPassagePlayerController);

APassagePlayerController::APassagePlayerController()
{
	PixelStreamingInput = CreateDefaultSubobject<UPixelStreamingInput>(TEXT("PixelStreamingInputComponent"), true);
	PassagePixelStream = CreateDefaultSubobject<UPassagePixelStreamComponent>(TEXT("PassagePixelStreamComponent"), true);
}

void APassagePlayerController::PassageInit(const FString& AuthToken) const
{
	PassagePixelStream->HandleInit(AuthToken);
}

void APassagePlayerController::PassageTeleport(const FString& ParticipantId) const
{
	UE_LOG(LogPassagePlayerController, Verbose, TEXT("APassagePlayerController::PassageTeleport %s"), *ParticipantId);
	ServerPassageTeleport(ParticipantId);
}

void APassagePlayerController::ServerPassageTeleport_Implementation(const FString& ParticipantId) const
{
	UE_LOG(LogPassagePlayerController, Verbose,
		TEXT("APassagePlayerController::ServerPassageTeleport_Implementation %s"), *ParticipantId);

	const auto MyPawn = GetPawn();
	if (!MyPawn)
	{
		UE_LOG(LogPassagePixelStream, Error, TEXT("No valid pawn reference %p"), MyPawn);
		return;
	}

	for (TObjectIterator<APassageCharacter> Target; Target; ++Target)
	{
		if (Target->GetWorld() != GetWorld() || *Target == MyPawn)
		{
			continue;
		}

		if (Target->GetParticipantId() == ParticipantId)
		{
			const auto Location = Target->GetActorLocation();

			if (MyPawn->GetActorLocation() == Location)
			{
				UE_LOG(LogPassagePixelStream, Error,
					TEXT("UPassagePixelStreamComponent::HandleTeleport teleporting to self, apparently"));
			}

			if (MyPawn->TeleportTo(Location, {}, true))
			{
				UE_LOG(LogPassagePixelStream, Verbose,
					TEXT("UPassagePixelStreamComponent::HandleTeleport %s's calling method for location change to %s"),
					*MyPawn->GetActorNameOrLabel(),
					*Location.ToCompactString());
				MyPawn->TeleportTo(Location, {}, false, true);
				return;
			}
			UE_LOG(LogPassagePixelStream, Error,
				TEXT("UPassagePixelStreamComponent::HandleTeleport unable to teleport. Avatar is unable to fit"));
			return;
		}
	}
	UE_LOG(LogPassagePixelStream, Error,
		TEXT("UPassagePixelStreamComponent::HandleTeleport unable to teleport. No participant found with id %s"),
		*ParticipantId);
}

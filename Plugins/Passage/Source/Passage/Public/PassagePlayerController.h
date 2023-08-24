// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "JsonRpc.h"
#include "GameFramework/PlayerController.h"
#include "PixelStreamingInputComponent.h"
#include "PassagePlayerController.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPassagePlayerController, Log, All);

/**
 * Games must inherit their PlayerController from the APassagePlayerController
 * rather than the built-in APlayerController class. This class handles RPC
 * messaging with the browser.
 */
UCLASS()
class PASSAGE_API APassagePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	APassagePlayerController();

	UPROPERTY(BlueprintReadOnly)
		UPixelStreamingInput* PixelStreamingInput;

	UPROPERTY(BlueprintReadOnly)
		UPassagePixelStreamComponent* PassagePixelStream;

	/**
	 * Calls the Init method of the PassagePixelStream so that we can test
	 * without running the pixel stream.
	 */
	UFUNCTION(Exec)
	void PassageInit(const FString& AuthToken) const;

	/**
	 * Calls the Teleport method of the PassagePixelStream so we can test
	 * teleporting without running the pixel stream.
	 */
	UFUNCTION(Exec)
	void PassageTeleport(const FString& ParticipantId) const;

	UFUNCTION(Server, Reliable)
	void ServerPassageTeleport(const FString& ParticipantId) const;
};
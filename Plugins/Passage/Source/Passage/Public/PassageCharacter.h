// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PassageCharacter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPassageCharacter, Log, Log);

UENUM(BlueprintType)
enum EPassageInputMode
{
	Pro,
	Simple,
};

UCLASS(Abstract, Blueprintable)
class PASSAGE_API APassageCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APassageCharacter();

	/**
	 * This method is implemented in BP_PassageCharacter, but it is defined
	 * here so that it can be called from C++.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	FString GetParticipantId();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SetInputMode(const EPassageInputMode Mode);
};

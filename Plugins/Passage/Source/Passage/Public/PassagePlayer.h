// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "PassagePlayer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPassagePlayer, Log, All);

/**
 * 
 */
UCLASS()
class PASSAGE_API UPassagePlayer : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;
};

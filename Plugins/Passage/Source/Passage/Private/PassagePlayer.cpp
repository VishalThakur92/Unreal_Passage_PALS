// Fill out your copyright notice in the Description page of Project Settings.

#include "PassagePlayer.h"

DEFINE_LOG_CATEGORY(LogPassagePlayer);

void UPassagePlayer::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogPassagePlayer, Log, TEXT("UPassagePlayer::Initialize"));
}

void UPassagePlayer::Deinitialize()
{
	UE_LOG(LogPassagePlayer, Log, TEXT("UPassagePlayer::Deinitialize"));
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Participant.h"

UParticipant::UParticipant()
	:
	Id(""),
	IsLocal(false),
	Active(false),
	ScreenName(""),
	ServerLocation(""),
	IsPublishingMedia(false),
	Pawn(nullptr)
{

}

bool UParticipant::HasProperty(const FString& Name) const
{
    return Data.Contains(Name);
}

FString UParticipant::GetProperty(const FString& Name)
{
    if (Data.Contains(Name))
    {
        return Data[Name];
    }
    return TEXT("");
}

void UParticipant::SetProperty(const FString& Name, const FString& Value)
{
    if (Data.Contains(Name))
    {
        Data[Name] = Value;
    }
    else
    {
        Data.Add(Name, Value);
    }
    OnData.Broadcast(Name, Value);
}

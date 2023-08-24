// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Participant.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FParticipantOnData, const FString&, Name, const FString&, Value);

/**
 * The Participant represents a user account that is defined by the directory
 * provider, i.e. the Passage web backend server.
 */
UCLASS(BlueprintType)
class PASSAGE_API UParticipant : public UObject
{
	GENERATED_BODY()


public:

	UParticipant();

	/**
	 * @brief The SetProperty method dispatches this event with the name/value
	 * whenever a property is set.
	 */
	UPROPERTY(BlueprintAssignable)
	FParticipantOnData OnData;

	/*
	 * A unique identifier assigned by the directory provider for the user
	 * account associated with this participant. This should include only URL
	 * safe characters because it is used as part of the Ion SFU's endpoint
	 * URL.
	 */
	UPROPERTY(BlueprintReadOnly)
	FString Id;

	/*
	 * The local participant is the participant who owns the player controller
	 * on this Unreal instance. For an Unreal dedicated server, none of the
	 * participants will be local on the server, only on the game instances
	 * themselves.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool IsLocal;

	/*
	 * A participant is active when they have recently interacted with the
	 * browser connected to the Passage services. They should become inactive
	 * after the browser drops the connection, or potentially when their session
	 * becomes stale. When the Active property changes, the UDirectoryProvider
	 * implementation will dispatch OnParticipantJoined or OnParticipantLeft
	 * events.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool Active;

	/*
	 * The human readable display name for the avatar.
	 */
	UPROPERTY(BlueprintReadOnly)
	FString ScreenName;

	/*
	 * For now this is the IP address of the game server that this participant
	 * is currently using. The intent is for others to be able to travel to
	 * another participant's location. This may need to be replaced by a struct
	 * eventually if server connections are more complex than can be 
	 * represented with a plain string.
	 */
	UPROPERTY(BlueprintReadOnly)
	FString ServerLocation;

	/*
	 * Is the player's browser at least attempting to send video/audio? This
	 * flag indicates the user's intent to make their video available, but
	 * doesn't make any claims about success. Muting is a different issue.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool IsPublishingMedia;

	/*
	 * Arbitrary additional attribute/value pairs can be transmitted from the
	 * directory server. This is open-ended to allow third party Passage creators
	 * to communicate high-latency, low-frequency data between game instances.
	 */
	UPROPERTY(BlueprintReadOnly)
	TMap<FString, FString> Data;

	/*
	 * If this participant is Active and their ServerLocation is the same as
	 * the local user's ServerLocation, then they should have a Pawn instance
	 * as their avatar in the current game instance. For an Unreal dedicated
	 * server, there is no particular local participant, but the dedicated
	 * server will still correlate to a particular ServerLocation.
	 * 
	 * In other cases, this will be a null pointer, so definitely check it with
	 * IsValid() before relying on it being defined.
	 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APawn> Pawn;

	UFUNCTION(BlueprintCallable)
	bool HasProperty(const FString& Name) const;

	UFUNCTION(BlueprintCallable)
	FString GetProperty(const FString& Name);

	UFUNCTION(BlueprintCallable)
	void SetProperty(const FString& Name, const FString& Value);
};

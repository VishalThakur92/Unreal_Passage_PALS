// Copyright Passage, 2022

#pragma once

#include "CoreMinimal.h"

#include "ConnectionStatus.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogConnection, Log, All);

/**
 The UConnectionStatus can be in any of the states specified by the EConnectionStatus enumeration.
*/
UENUM(BlueprintType)
enum class EConnectionStatus : uint8
{
	/**
	The initial state is Unconnected.The connection shouldn't go back to the unconnected state once it has been connected.
	*/
	Unconnected,

	/**
	Attempting to establish an initial connection.
	*/
	Connecting,

	/**
	Succeeded in establishing an initial connection.
	*/
	Connected,

	/**
	A connection was made, but then broken.Attempting to recover.
	*/
	Reconnecting,

	/**
	The connection was closed intentionally and is not expected to be re-connected.
	*/
	Closed,

	/**
	The connection could not be made or reconnected and no further automated attempts to connect will be made.
	*/
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStatusChange, EConnectionStatus, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FConnecting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FReconnecting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFailed);

/**
 * A UConnection represents the status of some other connection and provides a
 * consistent implementation with multicast delegates for monitoring the
 * lifecycle of the connection.
 */
UCLASS(BlueprintType)
class PASSAGE_API UConnectionStatus : public UObject
{
	GENERATED_BODY()

public:

	UConnectionStatus();

	/**
	Retrieve the current EConnectionStatus value. Initially this is Unconnected.
	*/
	UFUNCTION(BlueprintCallable)
	EConnectionStatus GetStatus();

	/**
	Transition to a new status value. The corresponding delegate will be broadcast when the status changes.

	@param Status The status starts as Unconnected but cannot be explicitly set to Unconnected. An attempt to set the status to Unconnected will do nothing and an error message will be logged. If the new status is equal to the old status, then nothing will be broadcast and no messages will be logged.
	*/
	UFUNCTION(BlueprintCallable)
	void SetStatus(UPARAM() EConnectionStatus Status);

	/**
	OnStatusChange is broadcast for any and all status changes.
	*/
	UPROPERTY(BlueprintAssignable)
	FStatusChange OnStatusChange;

	/*
	 * The remaining delegates broadcast for their specific status.
	 */

	UPROPERTY(BlueprintAssignable)
	FConnecting OnConnecting;

	UPROPERTY(BlueprintAssignable)
	FConnected OnConnected;

	UPROPERTY(BlueprintAssignable)
	FReconnecting OnReconnecting;

	UPROPERTY(BlueprintAssignable)
	FClosed OnClosed;

	UPROPERTY(BlueprintAssignable)
	FFailed OnFailed;

private:

	EConnectionStatus Status;
};

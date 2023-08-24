// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PassagePlayer.h"
#include "PassageGlobals.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPassageGlobals, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPassageGlobalEvent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPassagePixelStreamNewConnection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAgoraInfoUpdate);

/**
 * This is used in the DefaultPassage.ini config file to specify the class to
 * use for the video chat provider. The default value (0) is what we get when
 * the property is absent from the config file.
 */
UENUM()
enum class EVideoChatProviderClass: uint8
{
	Default					= 0,
	AgoraVideoChatProvider	= 10,
	VerseVideoChatProvider	= 20,
};

/**
 * 
 */
UCLASS(Config = Passage)
class PASSAGE_API UPassageGlobals : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	friend class UPassagePixelStreamComponent;

public:

	/**
	 * This is a workaround, not a feature.
	 *
	 * The UPixelStreamDelegates::OnNewConnection delegate is dispatched on
	 * some thread that is not the main thread, which renders it unusable in
	 * Blueprint. This adapter delegate will be run from the main thread only.
	 */
	UPROPERTY(BlueprintAssignable)
	FPassagePixelStreamNewConnection PixelStreamNewConnection;

	/**
	 * Our Ec2InstanceId is used by the backend in provisioning DNS records
	 * so that our signaling server can be addressed with https and wss URLs.
	 */
	UPROPERTY(BlueprintReadOnly)
	FString Ec2InstanceId;

	/**
	 * @brief The DirectoryAuthToken is received by the
	 * PassagePixelStreamComponent when the Frontend calls the Init() RPC
	 * method. It is stored here so it can be accessible to the
	 * DirectoryProvider during setup. Note that directory provider setup is
	 * delayed until after Init().
	 */
	UPROPERTY(BlueprintReadOnly)
	FString DirectoryAuthToken;


	/**
	 * This variable identifies the date and time that this version was
	 * compiled. Its value is logged during initialization so that we have an
	 * unambiguous record of the version info in case of logged errors, etc.
	 */
	inline static FString BuildTime = TEXT(__DATE__) TEXT(" ")	TEXT(__TIME__);

	/**
	 *
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 *
	 */
	virtual void Deinitialize() override;

	/**
	 * It's very easy to get this instance in Blueprint, but it's not so easy
	 * in C++, so this convenience function takes care of the boilerplate for
	 * you.
	 */
	static UPassageGlobals* GetPassageGlobals(const UObject* Context);

	/**
	 * This is another convenience function for getting the PassagePlayer in
	 * C++ with less boilerplate.
	 */
	bool GetPassagePlayer(UPassagePlayer*& PassagePlayer) const;

	/**
	 * When initialization has succeeded, this will no longer be nullptr. The
	 * OnDirectoryAvailable event will indicate when it has become non-null.
	 * Unfortunately I don't know a good C++ idiom for "do X now or when Y is
	 * available."
	 */
	UFUNCTION(BlueprintCallable)
	UDirectoryProvider* GetDirectoryProvider() const;

	/**
	 * 
	 */
	UFUNCTION(BlueprintCallable)
	void SetDirectoryProvider(UDirectoryProvider* NewDirectoryProvider);

	/**
	 * When initialization has succeeded, this will no longer be nullptr. The
	 * OnVideoChatAvailable event will indicate when it has become non-null.
	 */
	UFUNCTION(BlueprintCallable)
	UVideoChatProvider* GetVideoChatProvider() const;

	/**
	 *
	 */
	UFUNCTION(BlueprintCallable)
	void SetVideoChatProvider(UVideoChatProvider* NewVideoChatProvider);

	/**
	 * @brief Calls SendAvailableFlagToBackend() repeatedly until
	 * StopAvailabilityLoop() is called. This loop should be running whenever the
	 * game is not actually being played by a player.
	 */
	UFUNCTION()
	void StartAvailabilityLoop();

	FTimerDynamicDelegate AvailabilityLoopDelegate;
	FTimerHandle AvailabilityLoopHandle;

	/**
	 * @brief Stops sending the availability flag. The game should only be quiet
	 * like this while the game is in use by a player.
	 */
	UFUNCTION()
	void StopAvailabilityLoop();
	UFUNCTION()
	void StopAvailabilityLoop_DelegateHandler(FString _Trash, bool _AlsoTrash);


	/**
	 * Once we are able to connect to the signaling server, we are available
	 * for a participant to use. This accounts for DNS latency by the fact that
	 * we use a DNS name for the signaling server despite it running on
	 * localhost. Using the DNS name allows us to use SSL.
	 */
	UFUNCTION(BlueprintCallable)
	void SendAvailableFlagToBackend();

	/**
	 * @brief Get a delegate that will be triggered when the
	 * UPassageGlobals::BroadcastEvent static method is called with the same
	 * name.
	 * @param Context A UObject instance that can be used to call the static
	 * GetPassageGlobals(). This identifies the relevant game instance to which
	 * the UPassageGlobals class refers.
	 * @param EventName The name of the event to broadcast.
	 * @return A dynamic multi-cast delegate.
	 */
	UFUNCTION()
	static FPassageGlobalEvent& GlobalEvent(const UObject* Context, const FString& EventName);

	UFUNCTION(BlueprintCallable)
	static void BroadcastGlobalEvent(UObject* Context, const FString& EventName);

private:

	UPROPERTY(Config)
	EVideoChatProviderClass VideoChatProviderClass;

	/**
	 * @brief This either a StandaloneDirectoryProvider or a
	 * PassageDirectoryProvider instance. The standalone provides reasonable
	 * defaults for use when the game cannot connect to the Passage backend.
	 */
	UPROPERTY()
	UDirectoryProvider* DirectoryProvider;

	/**
	 * @brief The VideoChatProvider is either the AgoraVideoChatProvider or the
	 * VerseVideoChatProvider.
	 */
	UPROPERTY()
	UVideoChatProvider* VideoChatProvider;

	/**
	 * @brief Contains our delegates keyed by name.
	 */
	TMap<FString, FPassageGlobalEvent> Events;

	/**
	 * @brief Returned by the static GlobalEvent() method when the context is
	 * invalid. This will never be broadcast.
	 */
	inline static FPassageGlobalEvent ErrorGlobalEvent = {};

	/**
	 * Called during initialization to figure out what our machine ID is so
	 * that the Backend can update our DNS record. This will almost certainly
	 * fail on developer machines and will fallback to 'localhost' for the
	 * instance ID.
	 */
	void FetchEc2InstanceId();

	/**
	 * This lets the backend know that we are running and that we should be
	 * available soon. This happens once during startup. The effect is that the
	 * backend assigns us a new DNS record based on our instance ID.
	 */
	void SendInstanceIdToBackend();

	void BroadcastNewConnection(FString _Trash, bool _AlsoTrash);

};


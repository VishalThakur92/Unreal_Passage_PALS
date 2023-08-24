	// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AgoraImportGuards.h"
#include "VideoChatProvider.h"
#include "Sound/SoundWaveProcedural.h"

#include "AgoraVideoChatProvider.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAgora, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInitializeComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInitializeError);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWarningEvent, const FString&, Message);


class UAgoraVideoChatProvider;

class FFrameObserver :
	public agora::media::IAudioFrameObserver,
	public agora::media::IVideoFrameObserver
{
public:

	TObjectPtr<UAgoraVideoChatProvider> Parent;

	// Implementation of agora::media::IAudioFrameObserver

	virtual bool onRecordAudioFrame(const char* channelId, AudioFrame& audioFrame) override;
	virtual bool onPlaybackAudioFrame(const char* channelId, AudioFrame& audioFrame) override;
	virtual bool onMixedAudioFrame(const char* channelId, AudioFrame& audioFrame) override;
	virtual bool onEarMonitoringAudioFrame(AudioFrame& audioFrame) override;

	// This must return an integer code indicating where in the pipeline the,
	// observer should be inserted, e.g. before or after mixing
	virtual int getObservedAudioFramePosition() override;

	virtual AudioParams getPlaybackAudioParams() override;
	virtual AudioParams getRecordAudioParams() override;
	virtual AudioParams getMixedAudioParams() override;
	virtual AudioParams getEarMonitoringAudioParams() override;

	// this is where the audio data gets queued for the playback stream
	virtual bool
		onPlaybackAudioFrameBeforeMixing(const char* ChannelId, agora::rtc::uid_t Uid, AudioFrame& AudioFrame) override;

	// Implementation of agora::media::IVideoFrameObserver
	virtual bool getMirrorApplied() override;
	virtual uint32_t getObservedFramePosition() override;
	virtual bool getRotationApplied() override;
	virtual agora::media::base::VIDEO_PIXEL_FORMAT getVideoFormatPreference() override;
	virtual VIDEO_FRAME_PROCESS_MODE getVideoFrameProcessMode() override;
	virtual bool onMediaPlayerVideoFrame(VideoFrame& videoFrame, int mediaPlayerId) override;

	// This is where we write the video data to the texture
	virtual bool onRenderVideoFrame(
		const char* ChannelId, agora::rtc::uid_t RemoteUid, VideoFrame& VideoFrame) override;

	virtual bool onTranscodedVideoFrame(VideoFrame& videoFrame) override;
	virtual bool onCaptureVideoFrame(agora::rtc::VIDEO_SOURCE_TYPE sourceType, VideoFrame& videoFrame) override;
	virtual bool onPreEncodeVideoFrame(agora::rtc::VIDEO_SOURCE_TYPE sourceType, VideoFrame& videoFrame) override;
};


/**
 * Implements the VideoChatProvider interface.
 */
UCLASS()
class PASSAGE_API UAgoraVideoChatProvider :
	public UVideoChatProvider,
	public agora::rtc::IRtcEngineEventHandler
{
	GENERATED_BODY()

	friend FFrameObserver;

public:

	UAgoraVideoChatProvider();

	/**
	 * @brief See the defining interface method, VideoChatProvider::AttachMedia.
	 * @param Participant
	 * @param Material 
	 * @param ParameterName 
	 * @param AudioComponent 
	 */
	virtual void AttachMedia_Implementation(
		UParticipant* Participant, UMaterialInstanceDynamic* Material,
		const FString& ParameterName, UAudioComponent* AudioComponent) override;

	/**
	 * @brief See the defining interface method, VideoChatProvider::DetachMedia.
	 * @param Participant 
	 */
	virtual void DetachMedia_Implementation(UParticipant* Participant) override;

	UFUNCTION(BlueprintCallable)
	void AttachAudio(const FString& AgoraUid, UAudioComponent* AudioComponent);

	UFUNCTION(BlueprintCallable)
	void DetachAudio(const FString& AgoraUid);

	UFUNCTION(BlueprintCallable)
	void AttachVideo(const FString& AgoraUid, UMaterialInstanceDynamic* Material, const FString& ParameterName, UTexture2D* OfflineTexture);

	UFUNCTION(BlueprintCallable)
	void DetachVideo(const FString& AgoraUid);

	/**
	 * @brief This class must be explicitly started because it is not an Actor
	 * and it has no BeginPlay() lifecycle method. This will fetch the
	 * subscriber credentials from the backend and initialize the provider.
	 */
	UFUNCTION(BlueprintCallable)
	void Start();

	/**
	 * @brief Clean up resources allocated by Agora. Note that this object will
	 * be useless after this call.
	 */
	UFUNCTION(BlueprintCallable)
	void StopAndCleanup();

	/**
	 * @brief Leaves the current Agora channel, which should discontinue all
	 * streams. StopAndCleanup() is usually what you want though.
	 */
	UFUNCTION(BlueprintCallable)
	void Stop() const;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsInitialized() const;

	UPROPERTY(BlueprintAssignable);
	FInitializeComplete OnInitializeComplete;

	UPROPERTY(BlueprintAssignable);
	FInitializeError OnInitializeError;

	/**
	 * An event that communicates a single FString message parameter whenever a
	 * non-fatal error happens.
	 */
	UPROPERTY(BlueprintAssignable);
	FWarningEvent OnWarningEvent;

	// --------------------------------------------------------
	//  Implementation of the IRtcEngineEventHandler interface
	// --------------------------------------------------------

	virtual void onConnectionBanned() override;

	virtual void onConnectionInterrupted() override;

	virtual void onConnectionLost() override;

	virtual void onConnectionStateChanged(agora::rtc::CONNECTION_STATE_TYPE State, agora::rtc::CONNECTION_CHANGED_REASON_TYPE Reason) override;

	virtual void onRemoteAudioStateChanged(agora::rtc::uid_t Uid, agora::rtc::REMOTE_AUDIO_STATE State, agora::rtc::REMOTE_AUDIO_STATE_REASON Reason, int Elapsed) override;

	virtual void onRemoteVideoStateChanged(agora::rtc::uid_t Uid, agora::rtc::REMOTE_VIDEO_STATE State, agora::rtc::REMOTE_VIDEO_STATE_REASON Reason, int Elapsed) override;

	virtual void onRequestToken() override;

	// see https://api-ref.agora.io/en/voice-sdk/cpp/4.x/API/class_irtcengineeventhandler.html#callback_irtcengineeventhandler_ontokenprivilegewillexpire
	virtual void onTokenPrivilegeWillExpire(const char* Token) override;

	virtual void onUserJoined(agora::rtc::uid_t Uid, int Elapsed) override;

	virtual void onUserOffline(agora::rtc::uid_t Uid, agora::rtc::USER_OFFLINE_REASON_TYPE Reason) override;

	/**
	 * @brief A callback from Agora when we join the channel during initialization.
	 * See https://api-ref.agora.io/en/video-sdk/cpp/4.x/API/class_irtcengineeventhandler.html#callback_irtcengineeventhandler_onjoinchannelsuccess
	 * @param ChannelId 
	 * @param Uid 
	 * @param ElapsedTime A time measurement since joinChannel was called, expressed in milliseconds
	 */
	void onJoinChannelSuccess(const char* ChannelId,
	                          agora::rtc::uid_t Uid,
	                          int ElapsedTime) override;

	void onError(int ErrorCode, const char* Message) override;

	void onLeaveChannel(const agora::rtc::RtcStats& Stats) override;

	void onVideoSubscribeStateChanged(const char* channel, agora::rtc::uid_t uid, agora::rtc::STREAM_SUBSCRIBE_STATE oldState, agora::rtc::STREAM_SUBSCRIBE_STATE newState, int elapseSinceLastState) override;


private:

	TUniquePtr<agora::rtc::IRtcEngine> RtcEngine;
	agora::media::IMediaEngine* MediaEngine;
	agora::rtc::RtcEngineContext RtcEngineContext;
	agora::rtc::ChannelMediaOptions ChannelMediaOptions;

	FFrameObserver* FrameObserver;

	/**
	 * @brief Our Agora UID that we use for subscribing to other's video
	 * streams.
	 */
	FString SubscriberUid;

	/**
	 * @brief The Agora authorization token that we use for subscribing to other's
	 * video streams.
	 */
	FString SubscriberToken;

	/**
	 * @brief A flag that indicates whether or not we should call the renew
	 * token API when we've gotten a new SubscriberToken.
	 */
	bool AgoraNeedsRenewal = false;

	/**
	 * @brief These are the textures that get updated with incoming video.
	 * These are assigned to the material when media is attached.
	 */
	UPROPERTY()
	TMap<uint32, UTexture2D*> Textures;

	UPROPERTY()
	UTexture2D* DefaultTexture;

	// We store reference to the materials that receive the video texture here
	// so that we can display the default eyes or other visual signals of
	// the video status.
	UPROPERTY()
	TMap< uint32, UMaterialInstanceDynamic* > MaterialsByUid;
	UPROPERTY()
	TMap< uint32, UTexture2D* > OfflineTexturesByUid;
	TMap< uint32, FString > ParameterNamesByUid;

	typedef TSharedPtr< TQueue< TSharedPtr<TArray<uint8>> > > FQueueBuffer;

	// The same buffer is stored in each of these maps so that we can fill them
	// when we can identify them by Agora UID, versus the USoundWaveProcedural
	// pointer, which we can use to find the right queue in the FillAudio
	// method.
	TMap< USoundWaveProcedural*, FQueueBuffer > BuffersBySoundWave;
	TMap< uint32, FQueueBuffer > BuffersByUid;

	// We activate and de-activate the audio components based on whether we're
	// currently receiving and decoding audio.
	TMap< uint32, UAudioComponent* > AudioComponentsByUid;

	/**
	 * @brief This is called internally after BeginPlay();
	 *
	 * @param AppId The API key for the application/account that is billed for
	 * this Passage experience.
	 * @param ChannelId The string name of the Agora channel that the
	 * underlying RtcEngine will initially join.
	 * @param Token The subscriber token for the current user, likely obtained
	 * via `Participant->GetProperty(TEXT("AgoraSubscriberToken"))`.
	 */
	UFUNCTION(BlueprintCallable)
	void Initialize();

	bool IsInitializedFlag;

	/**
	 * @brief Convenience method to get and parse the Agora UID from the local
	 * participant obtained via the PassageGlobals singleton instance. If no
	 * value can be found, then 0 is returned.
	 * @return The unsigned 32 bit integer representing the Agora UID for the
	 * current game as a subscriber to other participant's streams.
	 */
	uint32 GetLocalAgoraSubscriberUid() const;

	// This is called by USoundWaveProcedural whenever it needs audio. We keep
	// a reference to the Wave pointer in BuffersBySoundWave so that we can
	// find its queue easily.
	void FillAudio(USoundWaveProcedural* Wave, const int32 SamplesNeeded);

	// Conditionally cleans up if we stopped without an explicit cleanup cue.
	void BeginDestroy() override;

	void LogError(const int ErrorCode, const FString& Message) const;

	/**
	 * @brief Called during initialization to get our Agora UID and token for
	 * this game instance.
	 */
	void FetchSubscriberInfo();
	void FetchSubscriberInfo_Retry();
};

// Fill out your copyright notice in the Description page of Project Settings.

#include "AgoraVideoChatProvider.h"

#include "PassageUtils.h"
#include "DirectoryProvider.h"
#include "LogThreadId.h"
#include "Components/AudioComponent.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "PassageGlobals.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DEFINE_LOG_CATEGORY(LogAgora);

UAgoraVideoChatProvider::UAgoraVideoChatProvider()
{
	FrameObserver = new FFrameObserver();
	MediaEngine = nullptr; // assigned in Initialize

	const ConstructorHelpers::FObjectFinder<UTexture2D> DefaultTextureFinder(
		TEXT("Texture2D'/Passage/Textures/Passage_eyes.Passage_eyes'"));
	DefaultTexture = DefaultTextureFinder.Object;
}

void UAgoraVideoChatProvider::AttachMedia_Implementation(
	UParticipant* Participant,
	UMaterialInstanceDynamic* Material,
	const FString& ParameterName,
	UAudioComponent* AudioComponent)
{
	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::AttachMedia_Implementation()"));

	if(Participant == nullptr || !IsValid(Participant))
	{
		UE_LOG(LogAgora, Error,
			TEXT("UAgoraVideoChatProvider::AttachMedia_Implementation() - Participant is not valid (address %p)"),
			Participant);
		return;
	}

	if (Material == nullptr || !IsValid(Material))
	{
		UE_LOG(LogAgora, Error,
			TEXT("UAgoraVideoChatProvider::AttachMedia_Implementation() - Material is not valid (address %p)"),
			Material);
		return;
	}


	if(RtcEngine == nullptr)
	{
		UE_LOG(LogAgora, Error,
			TEXT("UAgoraVideoChatProvider::AttachMedia_Implementation() RtcEngine is nullptr, will not attach media"));
		return;
	}

	uint32 AgoraUid;
	FString AgoraPublisherUid;
	if (Participant->HasProperty(TEXT("AgoraPublisherUid")))
	{
		AgoraPublisherUid = Participant->GetProperty(TEXT("AgoraPublisherUid"));
		if (!UPassageUtils::ParseUInt32(AgoraPublisherUid, AgoraUid))
		{
			UE_LOG(LogAgora, Error,
				TEXT("UAgoraVideoChatProvider::AttachMedia_Implementation() Invalid AgoraPublisherUid \"%s\" for Participant with Id \"%s\""),
				*(Participant->GetProperty("AgoraPublisherUid")),
				*(Participant->Id));
			return;
		}
	}
	else
	{
		UE_LOG(LogAgora, Error,
			TEXT("UAgoraVideoChatProvider::AttachMedia_Implementation() No AgoraUid in Data for Participant with Id \"%s\""),
			*(Participant->Id));
		return;
	}

	if (BuffersByUid.Contains(AgoraUid))
	{
		UE_LOG(LogAgora, Verbose, TEXT("Media already attached for AgoraUid %u"), AgoraUid);
		return;
	}

	AttachAudio(AgoraPublisherUid, AudioComponent);
	AttachVideo(AgoraPublisherUid, Material, ParameterName, nullptr);
}

void UAgoraVideoChatProvider::DetachMedia_Implementation(UParticipant* Participant)
{
	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::DetachMedia_Implementation()"));

	if (Participant->HasProperty(TEXT("AgoraPublisherUid")))
	{
		const FString AgoraUid = Participant->GetProperty((TEXT("AgoraPublisherUid")));
		DetachAudio(AgoraUid);
		DetachVideo(AgoraUid);
	}
	else
	{
		UE_LOG(LogAgora, Error,
			TEXT("UAgoraVideoChatProvider::DetachMedia_Implementation() Invalid/empty AgoraUid \"%s\" for Participant with Id \"%s\""),
			*(Participant->Data[TEXT("AgoraUid")]),
			*(Participant->Id));
	}
}

void UAgoraVideoChatProvider::AttachAudio(const FString& AgoraUid, UAudioComponent* AudioComponent)
{
	uint32 Uid;
	UPassageUtils::ParseUInt32(AgoraUid, Uid);

	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::AttachAudio() for uid %d"), Uid);

	const auto SoundStream = NewObject<USoundWaveProcedural>(AudioComponent);
	SoundStream->SetSampleRate(48000);
	SoundStream->NumChannels = 1;
	SoundStream->Duration = INDEFINITELY_LOOPING_DURATION;
	SoundStream->SoundGroup = SOUNDGROUP_Default;
	SoundStream->bLooping = false;
	SoundStream->OnSoundWaveProceduralUnderflow.BindUObject(this, &UAgoraVideoChatProvider::FillAudio);
	AudioComponent->SetSound(SoundStream);

	const auto AudioBuffer = MakeShared< TQueue< TSharedPtr<TArray<uint8>> > >();

	BuffersByUid.Add(Uid, AudioBuffer);
	BuffersBySoundWave.Add(SoundStream, AudioBuffer);
	AudioComponentsByUid.Add(Uid, AudioComponent);

	// Un-mute the stream
	if (const auto ErrorCode = RtcEngine->muteRemoteAudioStream(Uid, false);
		ErrorCode != 0)
	{
		LogError(ErrorCode,
			TEXT("Unable to unmute remote AUDIO stream for AgoraUid %d")
			+ Uid);
	}
}

void UAgoraVideoChatProvider::DetachAudio(const FString& AgoraUid)
{
	uint32 Uid;
	UPassageUtils::ParseUInt32(AgoraUid, Uid);
	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::DetachAudio() for uid %d"), Uid);

	if (RtcEngine)
	{
		if (const auto ErrorCode = RtcEngine->muteRemoteAudioStream(Uid, true);
			ErrorCode != 0)
		{
			LogError(ErrorCode,
				TEXT("UAgoraVideoChatProvider::DetachAudio() Unable to mute remote AUDIO stream for AgoraUid %d")
				+ Uid);
		}
	}

	if (BuffersByUid.Contains(Uid))
	{
		BuffersByUid.Remove(Uid);
	}
	else
	{
		UE_LOG(LogAgora, Warning,
			TEXT("UAgoraVideoChatProvider::DetachMedia_Implementation() No audio buffers for AgoraUid %u"),
			Uid);
	}
	if (AudioComponentsByUid.Contains(Uid))
	{
		AudioComponentsByUid.Remove(Uid);
	}
	else
	{
		UE_LOG(LogAgora, Warning,
			TEXT("UAgoraVideoChatProvider::DetachMedia_Implementation() No audio component for AgoraUid %u"),
			Uid);
	}
}


void UAgoraVideoChatProvider::AttachVideo(const FString& AgoraUid, UMaterialInstanceDynamic* Material,
                                          const FString& ParameterName, UTexture2D* OfflineTexture)
{
	uint32 Uid;
	UPassageUtils::ParseUInt32(AgoraUid, Uid);

	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::AttachVideo() for uid %d"), Uid);

	if (MaterialsByUid.Contains(Uid))
	{
		MaterialsByUid[Uid] = Material;
		ParameterNamesByUid[Uid] = ParameterName;
	}
	else
	{
		MaterialsByUid.Add(Uid, Material);
		ParameterNamesByUid.Add(Uid, ParameterName);
	}

	if (OfflineTexture)
	{
		if(OfflineTexturesByUid.Contains(Uid))
		{
			OfflineTexturesByUid[Uid] = OfflineTexture;
		}
		else
		{
			OfflineTexturesByUid.Add(Uid, OfflineTexture);
		}
	}

	if (const auto ErrorCode = RtcEngine->muteRemoteVideoStream(Uid, false);
		ErrorCode != 0)
	{
		LogError(ErrorCode,
			TEXT("Unable to unmute remote VIDEO stream for AgoraUid %d")
			+ Uid);
	}
}

void UAgoraVideoChatProvider::DetachVideo(const FString& AgoraUid)
{
	uint32 Uid;
	UPassageUtils::ParseUInt32(AgoraUid, Uid);

	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::DetachVideo() for uid %d"), Uid);

	if(RtcEngine)
	{
		if (const auto ErrorCode = RtcEngine->muteRemoteVideoStream(Uid, true);
			ErrorCode != 0)
		{
			LogError(ErrorCode,
				TEXT("UAgoraVideoChatProvider::DetachVideo() Unable to mute remote VIDEO stream for AgoraUid %d")
				+ Uid);
		}
	}

	if (MaterialsByUid.Contains(Uid))
	{
		MaterialsByUid.Remove(Uid);
	}
	else
	{
		UE_LOG(LogAgora, Warning,
			TEXT("UAgoraVideoChatProvider::DetachMedia_Implementation() No material for AgoraUid %u"),
			Uid);
	}
	if (ParameterNamesByUid.Contains(Uid))
	{
		ParameterNamesByUid.Remove(Uid);
	}
	else
	{
		UE_LOG(LogAgora, Warning,
			TEXT("UAgoraVideoChatProvider::DetachMedia_Implementation() No parameter name for AgoraUid %u"),
			Uid);
	}
}

void UAgoraVideoChatProvider::Start()
{
	FetchSubscriberInfo();
	if(const auto PassageGlobals = UPassageGlobals::GetPassageGlobals(this))
	{
		PassageGlobals->SetVideoChatProvider(this);
	}
}

void UAgoraVideoChatProvider::Initialize()
{
	if(RtcEngine.IsValid())
	{
		UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::Initialize() Skipping double initialization."));
		return;
	}

	FString AppId;
	UPassageUtils::GetConfigValue(
		TEXT("AgoraAppId"),
		TEXT("9cdaa7253de94c8c8160efda06015740"),
		AppId);

	FString Level;
	FString WaitForStreamer;
	UPassageUtils::GetConfigValue(
		TEXT("WaitForStreamer"),
		TEXT("Dev Channel"),
		WaitForStreamer);
	UPassageUtils::GetConfigValue(
		TEXT("Level"),
		WaitForStreamer,
		Level);
	const FString ChannelId = Level;

	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::Initialize() AppId = '%s', ChannelId = '%s', Token = '%s'"),
		*AppId,
		*ChannelId,
		*SubscriberToken);
	LogThreadId(TEXT("UAgoraVideoChatProvider::Initialize()"));

	RtcEngine = TUniquePtr<agora::rtc::IRtcEngine>(createAgoraRtcEngine());

	// See https://api-ref.agora.io/en/video-sdk/cpp/4.x/API/rtc_api_data_type.html#class_rtcengineconfig
	RtcEngineContext.appId = TCHAR_TO_ANSI(*AppId);
	RtcEngineContext.threadPriority = agora::rtc::THREAD_PRIORITY_TYPE::NORMAL;
	RtcEngineContext.eventHandler = this;
	RtcEngineContext.channelProfile = agora::CHANNEL_PROFILE_CLOUD_GAMING;
	RtcEngineContext.audioScenario = agora::rtc::AUDIO_SCENARIO_MEETING;

	// See https://api-ref.agora.io/en/video-sdk/cpp/4.x/API/rtc_api_data_type.html#class_channelmediaoptions
	ChannelMediaOptions.autoSubscribeAudio = false;
	ChannelMediaOptions.autoSubscribeVideo = false;
	ChannelMediaOptions.publishCameraTrack = false;
	ChannelMediaOptions.publishMicrophoneTrack = false;
	ChannelMediaOptions.publishRhythmPlayerTrack = false;

	if(const auto ErrorCode = RtcEngine->initialize(RtcEngineContext);
		ErrorCode != 0)
	{
		LogError(ErrorCode,
			TEXT("UAgoraVideoChatProvider::Initialize() Unable to initialize RtcEngine (check if the AppId is valid)"));
		OnInitializeError.Broadcast();
		return;
	}

	// TODO: enable this after upgrade
	//if (const auto ErrorCode = RtcEngine->enableInstantMediaRendering(); ErrorCode < 0)
	//{
	//	UE_LOG(LogAgora, Warning, TEXT("Unable to enable instant media rendering (%d)"), ErrorCode);
	//}

	int Build = - 1;
	const auto Version = RtcEngine->getVersion(&Build);
	UE_LOG(LogAgora, Log,
		TEXT("Using Agora SDK version %hs build %d"),
		Version,
		Build);


	if(const auto ErrorCode = RtcEngine->queryInterface(
		agora::rtc::AGORA_IID_MEDIA_ENGINE, (void**)&MediaEngine);
		ErrorCode < 0)
	{
		LogError(ErrorCode,
			TEXT("UAgoraVideoChatProvider::Initialize() Unable to retrieve a pointer to the Agora media engine"));
		StopAndCleanup();
		OnInitializeError.Broadcast();
		return;
	}

	FrameObserver->Parent = this;

	if(const auto ErrorCode = MediaEngine->registerAudioFrameObserver(FrameObserver);
		ErrorCode < 0)
	{
		LogError(ErrorCode,
			TEXT("UAgoraVideoChatProvider::Initialize() Unable to register AUDIO frame observer"));
		OnWarningEvent.Broadcast(TEXT("Audio did not connect"));
	}

	if(const auto ErrorCode = MediaEngine->registerVideoFrameObserver(FrameObserver);
		ErrorCode < 0)
	{
		LogError(ErrorCode,
			TEXT("UAgoraVideoChatProvider::Initialize() Unable to register VIDEO frame observer"));
		OnWarningEvent.Broadcast(TEXT("Video did not connect"));
	}

	const auto PassageGlobals = UPassageGlobals::GetPassageGlobals(this);
	if(!PassageGlobals)
	{
		UE_LOG(LogAgora, Error, TEXT("UAgoraVideoChatProvider::Initialize() Unable to get PassageGlobals"));
		StopAndCleanup();
		OnInitializeError.Broadcast();
		return;
	}
	uint32 AgoraUid;
	if(!UPassageUtils::ParseUInt32(SubscriberUid, AgoraUid))
	{
		UE_LOG(LogAgora, Error,
			TEXT("UAgoraVideoChatProvider::Initialize() Unable to parse SubscriberUid '%s'"),
			*SubscriberUid);
		StopAndCleanup();
		OnInitializeError.Broadcast();
		return;
	}

	if(const auto ErrorCode = RtcEngine->joinChannel(
		TCHAR_TO_ANSI(*SubscriberToken),
		TCHAR_TO_ANSI(*ChannelId),
		AgoraUid,
		ChannelMediaOptions
	); ErrorCode != 0)
	{
		LogError(ErrorCode, TEXT("UAgoraVideoChatProvider::Initialize() Unable to join channel"));
		StopAndCleanup();
		OnInitializeError.Broadcast();
		// no explicit return so the IDE doesn't complain at us about redundancy
	}

	UPassageGlobals::GlobalEvent(this, TEXT("RestartOnDisconnect")).AddDynamic(
		this, &UAgoraVideoChatProvider::StopAndCleanup);
}

void UAgoraVideoChatProvider::StopAndCleanup()
{
	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::StopAndCleanup()"));
	if(RtcEngine.IsValid())
	{

		if(const int ErrorCode = MediaEngine->registerAudioFrameObserver(nullptr); ErrorCode < 0)
		{
			UE_LOG(LogAgora, Error,
				TEXT("UAgoraVideoChatProvider::StopAndCleanup() Unable to un-register AUDIO frame observer"));
		}

		if (const int ErrorCode = MediaEngine->registerVideoFrameObserver(nullptr); ErrorCode < 0)
		{
			UE_LOG(LogAgora, Error,
				TEXT("UAgoraVideoChatProvider::StopAndCleanup() Unable to un-register VIDEO frame observer"));
		}

		RtcEngine->muteAllRemoteVideoStreams(true);
		RtcEngine->leaveChannel();

		// The Agora implemented release, which is asynchronous. The engine
		// cannot be restarted until the release thread completes, so don't try
		// to immediately re-create it. If we need an explicit restart for some
		// reason, we can do that on an Async thread.
		RtcEngine->release(true);

		// This releases the unique pointer, so we no longer have reference to
		// our entryway into Agora's implementation.
		RtcEngine.Release();
	}

	if(FrameObserver != nullptr)
	{
		delete FrameObserver;
	}
	FrameObserver = nullptr;
	MediaEngine = nullptr;
}

void UAgoraVideoChatProvider::Stop() const
{
	if(RtcEngine.IsValid())
	{
		UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::Stop() RtcEngine.IsValid() is true"));
		RtcEngine->muteAllRemoteVideoStreams(true);
		RtcEngine->leaveChannel();
	}
}

bool UAgoraVideoChatProvider::IsInitialized() const
{
	return IsInitializedFlag;
}

void UAgoraVideoChatProvider::onConnectionBanned()
{
	UE_LOG(LogAgora, Error, TEXT("UAgoraVideoChatProvider::onConnectionBanned()"));
}

void UAgoraVideoChatProvider::onConnectionInterrupted()
{
	UE_LOG(LogAgora, Error, TEXT("UAgoraVideoChatProvider::onConnectionInterrupted()"));
}

void UAgoraVideoChatProvider::onConnectionLost()
{
	UE_LOG(LogAgora, Error, TEXT("UAgoraVideoChatProvider::onConnectionLost()"));
}

void UAgoraVideoChatProvider::onConnectionStateChanged(agora::rtc::CONNECTION_STATE_TYPE State,
	agora::rtc::CONNECTION_CHANGED_REASON_TYPE Reason)
{
	FString StateString, ReasonString;

	switch(State)
	{
	case agora::rtc::CONNECTION_STATE_TYPE::CONNECTION_STATE_CONNECTED:
		StateString = TEXT("CONNECTED");
		Status->SetStatus(EConnectionStatus::Connected);
		break;
	case agora::rtc::CONNECTION_STATE_TYPE::CONNECTION_STATE_CONNECTING:
		StateString = TEXT("CONNECTING");
		Status->SetStatus(EConnectionStatus::Connecting);
		break;
	case agora::rtc::CONNECTION_STATE_TYPE::CONNECTION_STATE_DISCONNECTED:
		StateString = TEXT("DISCONNECTED");
		// This seems to correlate to our Unconnected
		Status->SetStatus(EConnectionStatus::Closed);
		break;
	case agora::rtc::CONNECTION_STATE_TYPE::CONNECTION_STATE_FAILED:
		StateString = TEXT("FAILED");
		Status->SetStatus(EConnectionStatus::Failed);
		break;
	case agora::rtc::CONNECTION_STATE_TYPE::CONNECTION_STATE_RECONNECTING:
		StateString = TEXT("RECONNECTING");
		break;
	default:
		StateString = FString::Printf(TEXT("UNKNOWN (%d)"), State);
	}

	switch(Reason)
	{
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_JOIN_SUCCESS:
		ReasonString = TEXT("JOIN_SUCCESS");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_INTERRUPTED:
		ReasonString = TEXT("INTERRUPTED");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_BANNED_BY_SERVER:
		ReasonString = TEXT("BANNED_BY_SERVER");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_JOIN_FAILED:
		ReasonString = TEXT("JOIN_FAILED");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_LEAVE_CHANNEL:
		ReasonString = TEXT("LEAVE_CHANNEL");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_INVALID_APP_ID:
		ReasonString = TEXT("INVALID_APP_ID");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_INVALID_CHANNEL_NAME:
		ReasonString = TEXT("INVALID_CHANNEL_NAME");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_INVALID_TOKEN:
		ReasonString = TEXT("INVALID_TOKEN");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_REJECTED_BY_SERVER:
		ReasonString = TEXT("REJECTED_BY_SERVER");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_SETTING_PROXY_SERVER:
		ReasonString = TEXT("SETTING_PROXY_SERVER");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_RENEW_TOKEN:
		ReasonString = TEXT("RENEW_TOKEN");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_CLIENT_IP_ADDRESS_CHANGED:
		ReasonString = TEXT("CLIENT_IP_ADDRESS_CHANGED");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_KEEP_ALIVE_TIMEOUT:
		ReasonString = TEXT("KEEP_ALIVE_TIMEOUT");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_REJOIN_SUCCESS:
		ReasonString = TEXT("REJOIN_SUCCESS");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_LOST:
		ReasonString = TEXT("LOST");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_ECHO_TEST:
		ReasonString = TEXT("ECHO_TEST");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_CLIENT_IP_ADDRESS_CHANGED_BY_USER:
		ReasonString = TEXT("CLIENT_IP_ADDRESS_CHANGED_BY_USER");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_SAME_UID_LOGIN:
		ReasonString = TEXT("SAME_UID_LOGIN");
		break;
	case agora::rtc::CONNECTION_CHANGED_REASON_TYPE::CONNECTION_CHANGED_TOO_MANY_BROADCASTERS:
		ReasonString = TEXT("TOO_MANY_BROADCASTERS");
		break;
	default: 
		ReasonString = FString::Printf(TEXT("UNKNOWN (%d)"), Reason);
	}

	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::onConnectionStateChanged() Connection state is now %s because %s"),
		*StateString,
		*ReasonString);
}

void UAgoraVideoChatProvider::onRemoteAudioStateChanged(agora::rtc::uid_t Uid, 
	agora::rtc::REMOTE_AUDIO_STATE State,
	agora::rtc::REMOTE_AUDIO_STATE_REASON Reason, int Elapsed)
{
	FString StateString, ReasonString;
	bool bDecoding = false;

	switch(State)
	{
	case agora::rtc::REMOTE_AUDIO_STATE::REMOTE_AUDIO_STATE_DECODING:
		StateString = TEXT("DECODING");
		bDecoding = true;
		break;
	case agora::rtc::REMOTE_AUDIO_STATE::REMOTE_AUDIO_STATE_FAILED:
		StateString = TEXT("FAILED");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE::REMOTE_AUDIO_STATE_FROZEN:
		StateString = TEXT("FROZEN");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE::REMOTE_AUDIO_STATE_STARTING:
		StateString = TEXT("STARTING");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE::REMOTE_AUDIO_STATE_STOPPED:
		StateString = TEXT("STOPPED");
		break;
	default:
		StateString = FString::Printf(TEXT("UNKNOWN (%d)"), State);
	}

	switch(Reason)
	{
	case agora::rtc::REMOTE_AUDIO_STATE_REASON::REMOTE_AUDIO_REASON_INTERNAL:
		ReasonString = TEXT("INTERNAL");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE_REASON::REMOTE_AUDIO_REASON_LOCAL_MUTED:
		ReasonString = TEXT("LOCAL_MUTED");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE_REASON::REMOTE_AUDIO_REASON_LOCAL_UNMUTED:
		ReasonString = TEXT("LOCAL_UNMUTED");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE_REASON::REMOTE_AUDIO_REASON_NETWORK_CONGESTION:
		ReasonString = TEXT("NETWORK_CONGESTION");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE_REASON::REMOTE_AUDIO_REASON_NETWORK_RECOVERY:
		ReasonString = TEXT("NETWORK_RECOVERY");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE_REASON::REMOTE_AUDIO_REASON_REMOTE_MUTED:
		ReasonString = TEXT("REMOTE_MUTED");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE_REASON::REMOTE_AUDIO_REASON_REMOTE_OFFLINE:
		ReasonString = TEXT("REMOTE_OFFLINE");
		break;
	case agora::rtc::REMOTE_AUDIO_STATE_REASON::REMOTE_AUDIO_REASON_REMOTE_UNMUTED:
		ReasonString = TEXT("REMOTE_UNMUTED");
		break;
	default:
		ReasonString = FString::Printf(TEXT("UNKOWN (%d)"), Reason);
	}

	// Activate or de-activate the corresponding audio component depending on
	// the current state. No other state is actually receiving audio, so we're
	// only active when decoding.
	if (AudioComponentsByUid.Contains(Uid))
	{
		const auto AudioComponent = AudioComponentsByUid[Uid];
		if(bDecoding)
		{
			Async(EAsyncExecution::TaskGraphMainThread, [AudioComponent]()
				{
					if (IsValid(AudioComponent))
					{
						AudioComponent->Activate();
					}
				});
		}
		else
		{
			const auto Buffer = BuffersByUid[Uid];
			Async(EAsyncExecution::TaskGraphMainThread, [AudioComponent, Buffer]()
				{
					if (IsValid(AudioComponent))
					{
						AudioComponent->Deactivate();
						// Since the main thread is the buffer consumer, we can
						// empty it here. If we didn't we'd accumulate a delay.
						Buffer->Empty();
					}
				});
		}
	}

	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::onRemoteAudioStateChanged() Remote AUDIO for Uid %u is now %s because %s"),
		Uid,
		*StateString,
		*ReasonString);
}

void UAgoraVideoChatProvider::onRemoteVideoStateChanged(agora::rtc::uid_t Uid, agora::rtc::REMOTE_VIDEO_STATE State,
	agora::rtc::REMOTE_VIDEO_STATE_REASON Reason, int Elapsed)
{
	FString StateString, ReasonString;
	bool bDecoding = false;

	switch (State)
	{
	case agora::rtc::REMOTE_VIDEO_STATE::REMOTE_VIDEO_STATE_DECODING:
		StateString = TEXT("DECODING");
		bDecoding = true;
		break;
	case agora::rtc::REMOTE_VIDEO_STATE::REMOTE_VIDEO_STATE_FAILED:
		StateString = TEXT("FAILED");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE::REMOTE_VIDEO_STATE_FROZEN:
		StateString = TEXT("FROZEN");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE::REMOTE_VIDEO_STATE_STARTING:
		StateString = TEXT("STARTING");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE::REMOTE_VIDEO_STATE_STOPPED:
		StateString = TEXT("STOPPED");
		break;
	default:
		StateString = FString::Printf(TEXT("UNKNOWN (%d)"), State);
	}

	switch (Reason)
	{
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_AUDIO_FALLBACK:
		ReasonString = TEXT("AUDIO_FALLBACK");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_AUDIO_FALLBACK_RECOVERY:
		ReasonString = TEXT("AUDIO_FALLBACK_RECOVERY");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_INTERNAL:
		ReasonString = TEXT("INTERNAL");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_LOCAL_MUTED:
		ReasonString = TEXT("LOCAL_MUTED");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_LOCAL_UNMUTED:
		ReasonString = TEXT("LOCAL_UNMUTED");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_NETWORK_CONGESTION:
		ReasonString = TEXT("NETWORK_CONGESTION");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_NETWORK_RECOVERY:
		ReasonString = TEXT("NETWORK_RECOVERY");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_REMOTE_MUTED:
		ReasonString = TEXT("REMOTE_MUTED");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_REMOTE_OFFLINE:
		ReasonString = TEXT("REMOTE_OFFLINE");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_REMOTE_UNMUTED:
		ReasonString = TEXT("REMOTE_UNMUTED");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_SDK_IN_BACKGROUND:
		ReasonString = TEXT("SDK_IN_BACKGROUND");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_VIDEO_STREAM_TYPE_CHANGE_TO_HIGH:
		ReasonString = TEXT("VIDEO_STREAM_TYPE_CHANGE_TO_HIGH");
		break;
	case agora::rtc::REMOTE_VIDEO_STATE_REASON::REMOTE_VIDEO_STATE_REASON_VIDEO_STREAM_TYPE_CHANGE_TO_LOW:
		ReasonString = TEXT("VIDEO_STREAM_TYPE_CHANGE_TO_LOW");
		break;
	default:
		ReasonString = FString::Printf(TEXT("UNKNOWN (%d)"), Reason);
	}

	if(MaterialsByUid.Contains(Uid))
	{
		const auto Material = MaterialsByUid[Uid];
		const FString ParameterName = ParameterNamesByUid[Uid];
		if (!bDecoding)
		{
			auto Texture = DefaultTexture;
			if(OfflineTexturesByUid.Contains(Uid))
			{
				Texture = OfflineTexturesByUid[Uid];
			}
			Async(EAsyncExecution::TaskGraphMainThread, [Material, ParameterName, Texture]()
				{
					if (IsValid(Material) && IsValid(Texture))
					{
						Material->SetTextureParameterValue(FName(ParameterName), Texture);
					}
				});
			
		}
	}

	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::onRemoteVideoStateChanged() Remote VIDEO for Uid %u is now %s because %s"),
		Uid,
		*StateString,
		*ReasonString);
}

void UAgoraVideoChatProvider::onRequestToken()
{
	UE_LOG(LogAgora, Error, TEXT("UAgoraVideoChatProvider::onRequestToken() Agora token expired"));
	AgoraNeedsRenewal = true;
	FetchSubscriberInfo();
}

void UAgoraVideoChatProvider::onTokenPrivilegeWillExpire(const char* Token)
{
	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::onTokenPrivilegeWillExpire() Token will expire in ~30 seconds, need to renew it now"));
	AgoraNeedsRenewal = true;
	FetchSubscriberInfo();
}

void UAgoraVideoChatProvider::onUserJoined(agora::rtc::uid_t Uid, int Elapsed)
{
	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::onUserJoined() Remote user with Uid %u JOINED"), 
		Uid);
	if(MaterialsByUid.Contains(Uid))
	{
		if (const auto ErrorCode = RtcEngine->muteRemoteVideoStream(Uid, false);
			ErrorCode != 0)
		{
			LogError(ErrorCode,
				TEXT("UAgoraVideoChatProvider::onUserJoined() Unable to unmute remote VIDEO stream for AgoraUid %d")
				+ Uid);
		}
	}
	if(AudioComponentsByUid.Contains(Uid))
	{
		if (const auto ErrorCode = RtcEngine->muteRemoteAudioStream(Uid, false);
			ErrorCode != 0)
		{
			LogError(ErrorCode,
				TEXT("UAgoraVideoChatProvider::onUserJoined() Unable to unmute remote AUDIO stream for AgoraUid %d")
				+ Uid);
		}
	}
}

void UAgoraVideoChatProvider::onUserOffline(agora::rtc::uid_t Uid, agora::rtc::USER_OFFLINE_REASON_TYPE Reason)
{
	FString ReasonString;
	switch(Reason)
	{
	case agora::rtc::USER_OFFLINE_REASON_TYPE::USER_OFFLINE_BECOME_AUDIENCE:
		ReasonString = TEXT("BECOME_AUDIENCE");
		break;
	case agora::rtc::USER_OFFLINE_REASON_TYPE::USER_OFFLINE_DROPPED:
		ReasonString = TEXT("DROPPED");
		break;
	case agora::rtc::USER_OFFLINE_REASON_TYPE::USER_OFFLINE_QUIT:
		ReasonString = TEXT("QUIT");
		break;
	default:
		ReasonString = FString::Printf(TEXT("UNKNOWN (%d)"), Reason);
	}
	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::onUserOffline() Remote user with Uid %u LEFT because %s"),
		Uid,
		*ReasonString);
}

void UAgoraVideoChatProvider::onJoinChannelSuccess(
	const char* ChannelId, agora::rtc::uid_t Uid, int ElapsedTime)
{
	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::onJoinChannelSuccess()"));
	LogThreadId(TEXT("UAgoraVideoChatProvider::onJoinChannelSuccess()"));

	// Events must be Broadcast from the main thread, but our current thread is
	// something internal to Agora.
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::OnInitializeComplete.Broadcast()"));
			IsInitializedFlag = true;
			OnInitializeComplete.Broadcast();
		});
}

void UAgoraVideoChatProvider::onError(int ErrorCode, const char* Message)
{
	LogError(ErrorCode, ANSI_TO_TCHAR(Message));
}

void UAgoraVideoChatProvider::onLeaveChannel(const agora::rtc::RtcStats& Stats)
{
	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::onLeaveChannel()"));
}

void UAgoraVideoChatProvider::onVideoSubscribeStateChanged(const char* channel, agora::rtc::uid_t uid,
	agora::rtc::STREAM_SUBSCRIBE_STATE oldState, agora::rtc::STREAM_SUBSCRIBE_STATE newState, int elapseSinceLastState)
{
	FString OldStateString;
	FString NewStateString;

	switch(oldState)
	{
	case agora::rtc::STREAM_SUBSCRIBE_STATE::SUB_STATE_IDLE:
		OldStateString = TEXT("IDLE");
		break;
	case agora::rtc::STREAM_SUBSCRIBE_STATE::SUB_STATE_NO_SUBSCRIBED:
		OldStateString = TEXT("NO_SUBSCRIBED");
		break;
	case agora::rtc::STREAM_SUBSCRIBE_STATE::SUB_STATE_SUBSCRIBING:
		OldStateString = TEXT("SUBSCRIBING");
		break;
	case agora::rtc::STREAM_SUBSCRIBE_STATE::SUB_STATE_SUBSCRIBED:
		OldStateString = TEXT("SUBSCRIBED");
		break;
	default:
		OldStateString = FString::Printf(TEXT("UNKNOWN (%d)"), oldState);
	}

	switch (newState)
	{
	case agora::rtc::STREAM_SUBSCRIBE_STATE::SUB_STATE_IDLE:
		NewStateString = TEXT("IDLE");
		break;
	case agora::rtc::STREAM_SUBSCRIBE_STATE::SUB_STATE_NO_SUBSCRIBED:
		NewStateString = TEXT("NO_SUBSCRIBED");
		break;
	case agora::rtc::STREAM_SUBSCRIBE_STATE::SUB_STATE_SUBSCRIBING:
		NewStateString = TEXT("SUBSCRIBING");
		break;
	case agora::rtc::STREAM_SUBSCRIBE_STATE::SUB_STATE_SUBSCRIBED:
		NewStateString = TEXT("SUBSCRIBED");
		break;
	default:
		NewStateString = FString::Printf(TEXT("UNKNOWN (%d)"), newState);
	}

	UE_LOG(LogAgora, Verbose,
		TEXT("UAgoraVideoChatProvider::onVideoSubscribeStateChanged() from %s to %s for uid %d"),
		*OldStateString,
		*NewStateString,
		uid
	);

}

uint32 UAgoraVideoChatProvider::GetLocalAgoraSubscriberUid() const
{
	if(const auto PassageGlobals = UPassageGlobals::GetPassageGlobals(this); PassageGlobals)
	{
		if(UDirectoryProvider* Directory = PassageGlobals->GetDirectoryProvider(); Directory)
		{
			if(UParticipant* Participant = Directory->GetLocalParticipant(); Participant)
			{
				if(Participant->HasProperty(TEXT("AgoraSubscriberUid")))
				{
					if(uint32 AgoraUid; UPassageUtils::ParseUInt32(Participant->GetProperty(TEXT("AgoraSubscriberUid")), AgoraUid))
					{
						return AgoraUid;
					}
				}
			}
		}
	}
	return 0;
}

void UAgoraVideoChatProvider::FillAudio(USoundWaveProcedural* Wave, const int32 SamplesNeeded)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("UAgoraVideoChatProvider::FillAudio()"));
	if(BuffersBySoundWave.Contains(Wave))
	{
		const auto Buffer = BuffersBySoundWave[Wave];
		if (TSharedPtr<TArray<uint8>> ByteArray; Buffer->Dequeue(ByteArray))
		{
			const auto ByteCount = ByteArray->Num();
			Wave->QueueAudio(ByteArray->GetData(), ByteCount);
		}
		else
		{
			UE_LOG(LogAgora, Verbose,
				TEXT("UAgoraVideoChatProvider::FillAudio() Unable to dequeue audio for Wave with address %p"), 
				Wave);
		}
	}
	else
	{
		UE_LOG(LogAgora, Error,
			TEXT("UAgoraVideoChatProvider::FillAudio() No buffer for Wave with address %p"), Wave);
	}
}

void UAgoraVideoChatProvider::BeginDestroy()
{
	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::BeginDestroy()"));
	Super::BeginDestroy();
	StopAndCleanup();
}

void UAgoraVideoChatProvider::LogError(const int ErrorCode, const FString& Message) const
{
	const FString ErrorDesc = ANSI_TO_TCHAR(RtcEngine->getErrorDescription(ErrorCode));

	UE_LOG(LogAgora, Error,
		TEXT("%s - Error# %d %s"),
		*Message,
		ErrorCode,
		*ErrorDesc
	);
}

void UAgoraVideoChatProvider::FetchSubscriberInfo()
{
	UE_LOG(LogAgora, Verbose, TEXT("UAgoraVideoChatProvider::FetchSubscriberInfo()"));
	if (UPassageUtils::HasConfigFlag(TEXT("SuppressActionsForServerInEditor")))
	{
		UE_LOG(LogAgora, Verbose,
			TEXT("UAgoraVideoChatProvider::FetchSubscriberInfo() Skipping because we appear to be running in-editor with -SuppressActionsForServerInEditor flag"));
		return;
	}
	const auto FetchRequest = FHttpModule::Get().CreateRequest();
	const auto BackendRoot = UPassageUtils::GetBackendRoot();
	FString DedicatedServer;
	UPassageUtils::GetConfigValue(
		TEXT("WaitForStreamer"),
		TEXT(""),
		DedicatedServer);
	UPassageUtils::GetConfigValue(
		TEXT("Level"),
		DedicatedServer,
		DedicatedServer);
	DedicatedServer = FGenericPlatformHttp::UrlEncode(DedicatedServer);
	const FString ChannelName = FGenericPlatformHttp::UrlEncode(DedicatedServer);
	auto Endpoint = FString::Printf(TEXT("%s/get-agora-subscriber-token?channelName=%s"), *BackendRoot, *ChannelName);

	// When we are renewing, we want to retain our existing UID. The backend
	// supports a uid query parameter for this purpose.
	if(!SubscriberUid.IsEmpty())
	{
		Endpoint = FString::Printf(TEXT("%s&uid=%s"), *Endpoint, *SubscriberUid);
	}

	UE_LOG(LogPassageGlobals, VeryVerbose,
		TEXT("UAgoraVideoChatProvider::FetchSubscriberInfo() requesting %s"),
		*Endpoint);
	FetchRequest->SetURL(Endpoint);
	FetchRequest->OnProcessRequestComplete().BindWeakLambda(this,
		[this]
		(const FHttpRequestPtr &Request, const FHttpResponsePtr &Response, bool Connected)
		{
			if (Request->GetStatus() == EHttpRequestStatus::Succeeded)
			{
				const auto Json = Response->GetContentAsString();
				const auto Reader = TJsonReaderFactory<>::Create(Json);
				TSharedPtr<FJsonValue> Value;

				if (!FJsonSerializer::Deserialize(Reader, Value))
				{
					UE_LOG(LogPassageGlobals, Error,
						TEXT("UAgoraVideoChatProvider::FetchSubscriberInfo() Failed to parse JSON response: %s"), *Json);
					FetchSubscriberInfo_Retry();
					return;
				}
				if (const auto Object = Value->AsObject();
					Object->HasTypedField<EJson::String>(TEXT("uid")))
				{
					SubscriberUid = Object->GetStringField(TEXT("uid"));
				}
				else
				{
					UE_LOG(LogPassageGlobals, Error,
						TEXT("UAgoraVideoChatProvider::FetchSubscriberInfo() No `uid` field in JSON: %s"), *Json);
					FetchSubscriberInfo_Retry();
					return;
				}
				if (const auto Object = Value->AsObject();
					Object->HasTypedField<EJson::String>(TEXT("token")))
				{
					SubscriberToken = Object->GetStringField(TEXT("token"));
				}
				else
				{
					UE_LOG(LogPassageGlobals, Error,
						TEXT("UAgoraVideoChatProvider::FetchSubscriberInfo() No `token` field in JSON: %s"), *Json);
					FetchSubscriberInfo_Retry();
					return;
				}
				Async(EAsyncExecution::TaskGraphMainThread,
					[this]()
					{
						UE_LOG(LogPassageGlobals, VeryVerbose,
						TEXT("UAgoraVideoChatProvider::FetchSubscriberInfo() Got AgoraUid(%s) and AgoraToken(%s)"),
						*SubscriberUid,
						*SubscriberToken);

						if (!RtcEngine.IsValid())
						{
							Initialize();
						}

						if(AgoraNeedsRenewal)
						{
							if(RtcEngine)
							{
								RtcEngine->renewToken(TCHAR_TO_ANSI(*SubscriberToken));
							}
							AgoraNeedsRenewal = false;
						}
					});
			}
		});
	FetchRequest->ProcessRequest();
}

void UAgoraVideoChatProvider::FetchSubscriberInfo_Retry()
{
	// We're going to discard the handle because we don't loop the timer
	FTimerHandle TempHandle;
	GetWorld()->GetTimerManager().SetTimer(
		TempHandle,
		this,
		&UAgoraVideoChatProvider::FetchSubscriberInfo,
		10.0,
		false);
}

bool FFrameObserver::onRecordAudioFrame(const char* channelId, AudioFrame& audioFrame)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onRecordAudioFrame()"));
	return true;
}

bool FFrameObserver::onPlaybackAudioFrame(const char* channelId, AudioFrame& audioFrame)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onPlaybackAudioFrame()"));
	return true;
}

bool FFrameObserver::onMixedAudioFrame(const char* channelId, AudioFrame& audioFrame)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onMixedAudioFrame()"));
	return true;
}

bool FFrameObserver::onEarMonitoringAudioFrame(AudioFrame& audioFrame)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onEarMonitoringAudioFrame()"));
	return true;
}

int FFrameObserver::getObservedAudioFramePosition()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getObservedAudioFramePosition()"));
	return AUDIO_FRAME_POSITION_BEFORE_MIXING;
}

agora::media::IAudioFrameObserverBase::AudioParams FFrameObserver::getPlaybackAudioParams()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getPlaybackAudioParams()"));
	return {};
}

agora::media::IAudioFrameObserverBase::AudioParams FFrameObserver::getRecordAudioParams()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getRecordAudioParams()"));
	return {};
}

agora::media::IAudioFrameObserverBase::AudioParams FFrameObserver::getMixedAudioParams()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getMixedAudioParams()"));
	return AudioParams(
		48000, 
		1,
		agora::rtc::RAW_AUDIO_FRAME_OP_MODE_TYPE::RAW_AUDIO_FRAME_OP_MODE_READ_ONLY,
		1024);
}

agora::media::IAudioFrameObserverBase::AudioParams FFrameObserver::getEarMonitoringAudioParams()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getEarMonitoringAudioParams()"));
	return {};
}

bool FFrameObserver::onPlaybackAudioFrameBeforeMixing(const char* ChannelId, agora::rtc::uid_t Uid,
	AudioFrame& AudioFrame)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onPlaybackAudioFrameBeforeMixing()"));

	//UE_LOG(LogTemp, Log,
	//	TEXT("channels(%d) bytesPerSample(%d) samplesPerSec(%d) samplesPerChannel(%d)"),
	//	AudioFrame.channels,
	//	AudioFrame.bytesPerSample,
	//	AudioFrame.samplesPerSec,
	//	AudioFrame.samplesPerChannel
	//	);

	if(IsValid(Parent))
	{
		if (Parent->BuffersByUid.Contains(Uid))
		{
			const auto Data = MakeShared< TArray<uint8> >();
			const auto Count =
				AudioFrame.channels * AudioFrame.bytesPerSample * AudioFrame.samplesPerChannel;
			Data->AddUninitialized(Count);
			FMemory::Memcpy(Data->GetData(), AudioFrame.buffer, Count);
			Parent->BuffersByUid[Uid]->Enqueue(Data);
		}
		else
		{
			UE_LOG(LogAgora, Verbose,
				TEXT("FFrameObserver::onPlaybackAudioFrameBeforeMixing() No buffer for Uid %u"),
				Uid);
		}
	}
	else
	{
		UE_LOG(LogAgora, Verbose,
			TEXT("FFrameObserver::onPlaybackAudioFrameBeforeMixing() Invalid Parent, will not buffer audio"));
	}
	return true;
}

bool FFrameObserver::getMirrorApplied()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getMirrorApplied()"));
	return IVideoFrameObserver::getMirrorApplied();
}

uint32_t FFrameObserver::getObservedFramePosition()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getObservedFramePosition()"));
	return agora::media::base::POSITION_PRE_RENDERER;
}

bool FFrameObserver::getRotationApplied()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getRotationApplied()"));
	return IVideoFrameObserver::getRotationApplied();
}

/**
 * @brief Sets our non-default preference to match the format we use with WebRTC.
 * @return VIDEO_PIXEL_BGRA
 */
agora::media::base::VIDEO_PIXEL_FORMAT FFrameObserver::getVideoFormatPreference()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getVideoFormatPreference()"));
	return agora::media::base::VIDEO_PIXEL_FORMAT::VIDEO_PIXEL_BGRA;
}

agora::media::IVideoFrameObserver::VIDEO_FRAME_PROCESS_MODE FFrameObserver::getVideoFrameProcessMode()
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::getVideoFrameProcessMode()"));
	//return IVideoFrameObserver::getVideoFrameProcessMode();
	return agora::media::IVideoFrameObserver::VIDEO_FRAME_PROCESS_MODE::PROCESS_MODE_READ_ONLY;
}

bool FFrameObserver::onMediaPlayerVideoFrame(VideoFrame& videoFrame, int mediaPlayerId)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onMediaPlayerVideoFrame()"));
	return false;
}



bool FFrameObserver::onRenderVideoFrame(
	const char* ChannelId, agora::rtc::uid_t RemoteUid, VideoFrame& VideoFrame)
{
	UE_LOG(LogAgora, VeryVerbose,
		TEXT("FFrameObserver::onRenderVideoFrame() FFrameObserver address %p"), this);
	if (VideoFrame.type != getVideoFormatPreference())
	{
		UE_LOG(LogAgora, Error,
			TEXT("FFrameObserver::onRenderVideoFrame() Unexpected VideoFrame.type: %d"),
			VideoFrame.type);
	}

	const auto FrameWidth = VideoFrame.width;
	const auto FrameHeight = VideoFrame.height;

	const auto Count = FrameWidth * FrameHeight * 4;
	// the ImgData will be deleted by UpdateVideoTexture in the lambda
	uint8* ImgData = new uint8[Count];
	// I believe we're doing this copy because Agora may reclaim VideoFrame.yBuffer after this method exits
	FMemory::Memcpy(ImgData, VideoFrame.yBuffer, Count);

	Async(EAsyncExecution::TaskGraphMainThread, [this, RemoteUid, ImgData, FrameWidth, FrameHeight]()
		{
			if(IsValid(Parent))
			{
				UTexture2D* Texture;

				// Update the Texture if the size has changed or if we simply don't have a texture yet
				if (Parent->Textures.Contains(RemoteUid))
				{
					Texture = Parent->Textures[RemoteUid];

					const auto Width = Texture->GetSizeX();
					const auto Height = Texture->GetSizeY();

					if (Width != FrameWidth || Height != FrameHeight)
					{
						Texture = UPassageUtils::CreateVideoTexture(FrameWidth, FrameHeight);
					}
					Parent->Textures[RemoteUid] = Texture;
				}
				else
				{
					Texture = UPassageUtils::CreateVideoTexture(FrameWidth, FrameHeight);
					Parent->Textures.Add(RemoteUid, Texture);
				}
				if(Parent->MaterialsByUid.Contains(RemoteUid))
				{
					const auto Material = Parent->MaterialsByUid[RemoteUid];
					const auto ParameterName = Parent->ParameterNamesByUid[RemoteUid];
					Material->SetTextureParameterValue(FName(ParameterName), Texture);

					UPassageUtils::UpdateVideoTexture(
						Parent->Textures[RemoteUid], ImgData,
						FrameWidth, FrameHeight);
				}

			} else
			{
				// This happens when we stop and clean up but there's still pending
				// updates on the render thread. There may be other circumstances where
				// this is an error, but it is normal to see this 0 or 1 times after
				// stopping.
				UE_LOG(LogAgora, Verbose, TEXT("FFrameObserver::onRenderVideoFrame() Async UpdateVideoTexture. Invalid Parent or Texture pointer (may happen normally when stopping)"));
			}
		});

	return true;
}


bool FFrameObserver::onTranscodedVideoFrame(VideoFrame& videoFrame)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onTranscodedVideoFrame()"));
	return false;
}

bool FFrameObserver::onCaptureVideoFrame(agora::rtc::VIDEO_SOURCE_TYPE sourceType, VideoFrame& videoFrame)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onCaptureVideoFrame()"));
	return true;
}

bool FFrameObserver::onPreEncodeVideoFrame(agora::rtc::VIDEO_SOURCE_TYPE sourceType, VideoFrame& videoFrame)
{
	UE_LOG(LogAgora, VeryVerbose, TEXT("FFrameObserver::onPreEncodeVideoFrame()"));
	return true;
}


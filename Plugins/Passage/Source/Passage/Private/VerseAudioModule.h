// Copyright Enva Division

#pragma once

#include "WebRtcGuards.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"


DECLARE_LOG_CATEGORY_EXTERN(LogVerseAudio, Log, All);

class FVerseAudioModule : public webrtc::AudioDeviceModule
{
    static constexpr int MS_PER_FRAME = 10;
    static constexpr uint8_t CHANNEL_COUNT = 2;
    static constexpr int SAMPLES_PER_SECOND = 48000;
    static constexpr uint32_t VERSE_MAX_VOLUME = 14392;
    static constexpr size_t SAMPLE_COUNT = MS_PER_FRAME * SAMPLES_PER_SECOND / 1000;
    static constexpr size_t BYTES_PER_SAMPLE = sizeof(uint16_t) * CHANNEL_COUNT;

public:

    explicit FVerseAudioModule(
            webrtc::TaskQueueFactory* TaskQueueFactory,
            UAudioComponent* AudioComponent,
            USoundWaveProcedural* SoundWave) noexcept;
    ~FVerseAudioModule() = default;

    static rtc::scoped_refptr<FVerseAudioModule> Create(
            webrtc::TaskQueueFactory* TaskQueueFactory,
            UAudioComponent* AudioComponent);


private:

    webrtc::AudioTransport* AudioTransport;
    rtc::TaskQueue TaskQueue;
    UAudioComponent* AudioComponent;
    USoundWaveProcedural* SoundWave;
    int64_t NextQueueAudioDataTime;
    uint8 AudioData[SAMPLE_COUNT*BYTES_PER_SAMPLE];


    mutable rtc::CriticalSection CriticalSection;

    // IsPlaying tracks our playing state
    bool IsPlaying;

    // IsStarted helps us track the QueueAudioData schedule
    bool IsStarted;

    // Debugging flag to track when we're getting audio or not
    bool ReceivingSilence;

    // Manages scheduling QueueAudioData repeatedly on the TaskQueue thread
    void ScheduleQueueAudioData();

    // Actually copies audio data from the WebRTC implementation into the 
    // SoundWave USoundProcedural instance
    void QueueAudioData();

    // This section implements the webrtc::AudioDeviceModule interface. Almost
    // everything is just statically implemented as a no-op. Anything that has
    // no implementation here is actually interesting and is implemnted in the
    // .cpp file.

public:

	int32 ActiveAudioLayer(AudioLayer* layer) const override
    {
	    *layer = AudioLayer::kDummyAudio;
    	return 0;
    }
    
	int32 RegisterAudioCallback(webrtc::AudioTransport* transport) override;

	// Main initialization and termination
	int32 Init() override { return 0; }
	int32 Terminate() override { return 0; }
	bool Initialized() const override { return true; }

	// Device enumeration
	int16 PlayoutDevices() override { return 0; }
	int16 RecordingDevices() override { return 0; }
	int32 PlayoutDeviceName(
            uint16 index,
            char name[webrtc::kAdmMaxDeviceNameSize],
            char guid[webrtc::kAdmMaxGuidSize]) override
    {
        return 0;
    }
	int32 RecordingDeviceName(
            uint16 index,
            char name[webrtc::kAdmMaxDeviceNameSize],
            char guid[webrtc::kAdmMaxGuidSize]) override
    {
        return 0;
    }

	// Device selection
	int32 SetPlayoutDevice(uint16 index) override { return 0; }
	int32 SetPlayoutDevice(WindowsDeviceType device) override { return 0; }
	int32 SetRecordingDevice(uint16 index) override { return 0; }
	int32 SetRecordingDevice(WindowsDeviceType device) override { return 0; }

	// Audio transport initialization
	int32 PlayoutIsAvailable(bool* available) override { return true; }
	int32 InitPlayout() override { return 0; }
	bool PlayoutIsInitialized() const override { return true; }
	int32 RecordingIsAvailable(bool* available) override { return false; }
	int32 InitRecording() override { return 0; }
	bool RecordingIsInitialized() const override { return true; }

	// Audio transport control
	virtual int32 StartPlayout() override;
	virtual int32 StopPlayout() override;

	// True when audio is being pulled by the instance.
	virtual bool Playing() const override;

	virtual int32 StartRecording() override { return 0; }
	virtual int32 StopRecording() override { return 0; }
	virtual bool Recording() const override { return false; }


	// Audio mixer initialization
	virtual int32 InitSpeaker() override { return 0; }
	virtual bool SpeakerIsInitialized() const override { return true; }
	virtual int32 InitMicrophone() override { return 0; }
	virtual bool MicrophoneIsInitialized() const override { return false; }

	// Speaker volume controls
	virtual int32 SpeakerVolumeIsAvailable(bool* available) override
	{
		return -1;
	}
	virtual int32 SetSpeakerVolume(uint32 volume) override
	{
		return -1;
	}
	virtual int32 SpeakerVolume(uint32* volume) const override
	{
		return -1;
	}
	virtual int32 MaxSpeakerVolume(uint32* maxVolume) const override
	{
		return -1;
	}
	virtual int32 MinSpeakerVolume(uint32* minVolume) const override
	{
		return -1;
	}

	// Microphone volume controls
	virtual int32 MicrophoneVolumeIsAvailable(bool* available) override
	{
		return 0;
	}
	virtual int32 SetMicrophoneVolume(uint32 volume) override
	{
		return 0;
	}
	virtual int32 MicrophoneVolume(uint32* volume) const override
	{
		return 0;
	}
	virtual int32 MaxMicrophoneVolume(uint32* max) const override
	{
		*max = VERSE_MAX_VOLUME;
		return 0;
	}
	virtual int32 MinMicrophoneVolume(uint32* minVolume) const override
	{
		return 0;
	}

	// Speaker mute control
	virtual int32 SpeakerMuteIsAvailable(bool* available) override
	{
		return -1;
	}
	virtual int32 SetSpeakerMute(bool enable) override
	{
		return -1;
	}
	virtual int32 SpeakerMute(bool* enabled) const override
	{
		return -1;
	}

	// Microphone mute control
	virtual int32 MicrophoneMuteIsAvailable(bool* available) override
	{
		*available = false;
		return -1;
	}
	virtual int32 SetMicrophoneMute(bool enable) override
	{
		return -1;
	}
	virtual int32 MicrophoneMute(bool* enabled) const override
	{
		return -1;
	}

	// Stereo support
    // TODO: do we want stereo playout? I feel like we'd prefer mono coming
    // from all clients since that would save bandwidth.
	virtual int32 StereoPlayoutIsAvailable(bool* available) const override
    {
        *available = true;
        return 0;
    }

	virtual int32 SetStereoPlayout(bool enable) override
    {
        return 0;
    }

	virtual int32 StereoPlayout(bool* enabled) const override
    {
        *enabled = true;
        return 0;
    }

	virtual int32 StereoRecordingIsAvailable(bool* available) const override
    {
    	*available = false;
    	return 0;
    }

	virtual int32 SetStereoRecording(bool enable) override { return 0; }
	virtual int32 StereoRecording(bool* enabled) const override { return 0; }

	// Playout delay
	virtual int32 PlayoutDelay(uint16* delayMS) const override
	{
		*delayMS = 0;
		return 0;
	}

	virtual bool BuiltInAECIsAvailable() const override
	{
		return false;
	}
	virtual bool BuiltInAGCIsAvailable() const override
	{
		return false;
	}
	virtual bool BuiltInNSIsAvailable() const override
	{
		return false;
	}

	// Enables the built-in audio effects. Only supported on Android.
	virtual int32 EnableBuiltInAEC(bool enable) override
	{
		return -1;
	}
	virtual int32 EnableBuiltInAGC(bool enable) override
	{
		return -1;
	}
	virtual int32 EnableBuiltInNS(bool enable) override
	{
		return -1;
	}
};

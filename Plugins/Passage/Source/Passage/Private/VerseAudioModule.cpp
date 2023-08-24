// Copyright Enva Division

#include "VerseAudioModule.h" 


DEFINE_LOG_CATEGORY(LogVerseAudio);

FVerseAudioModule::FVerseAudioModule(
        webrtc::TaskQueueFactory* TaskQueueFactory,
        UAudioComponent* AC,
        USoundWaveProcedural* SW) noexcept
	:
    AudioTransport(nullptr),
	TaskQueue(TaskQueueFactory->CreateTaskQueue(
                "FVerseAudioModuleTimer",
        		webrtc::TaskQueueFactory::Priority::NORMAL)),
    AudioComponent(AC),
    SoundWave(SW),
    IsPlaying(false),
    IsStarted(false),
    ReceivingSilence(false)
{
}

rtc::scoped_refptr<FVerseAudioModule> FVerseAudioModule::Create(
        webrtc::TaskQueueFactory* TaskQueueFactory,
        UAudioComponent* AC)
{
    UE_LOG(LogVerseAudio, Log, TEXT("FVerseAudioModule::Create()"));
    USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>();

    SoundWave->SetSampleRate(SAMPLES_PER_SECOND);
    SoundWave->NumChannels = CHANNEL_COUNT;
    SoundWave->SampleByteSize = BYTES_PER_SAMPLE;
    SoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
    SoundWave->SoundGroup = SOUNDGROUP_Default;

    AC->SetSound(SoundWave);

    rtc::scoped_refptr<FVerseAudioModule> VerseAudioModule(
            new rtc::RefCountedObject<FVerseAudioModule>(
                TaskQueueFactory, AC, SoundWave)
            );
    return VerseAudioModule;
}

int32 FVerseAudioModule::RegisterAudioCallback(webrtc::AudioTransport* transport) 
{
    UE_LOG(LogVerseAudio, Log, TEXT("FVerseAudioModule::RegisterAudioCallback()"));
    AudioTransport = transport;
    return 0;
}

int32 FVerseAudioModule::StartPlayout()
{
    UE_LOG(LogVerseAudio, Log, TEXT("FVerseAudioModule::StartPlayout()"));

    {
        // The CritScope automatically synchronizes for the current block by
        // releasing the mutex in the cs virable's destructor
        rtc::CritScope _cs(&CriticalSection);
        IsPlaying = true;
    }

    AsyncTask(ENamedThreads::GameThread, [this]() {
        SoundWave->ResetAudio();
        AudioComponent->Play();

        TaskQueue.PostTask([this](){ ScheduleQueueAudioData(); });
    });


    return 0;
}

int32 FVerseAudioModule::StopPlayout()
{
    UE_LOG(LogVerseAudio, Log, TEXT("FVerseAudioModule::StopPlayout()"));

    {
        // The CritScope automatically synchronizes for the current block by
        // releasing the mutex in the cs virable's destructor
        rtc::CritScope cs(&CriticalSection);
        IsPlaying = false;
        IsStarted = false;
    }

    // TODO: why is this doubly wrapped in lambdas? Shouldn't it be sufficient
    // simply to pos this to the GameThread? Why would we need the TaskQueue to
    // post this to the game thread?

    // I think we can get away without this for now and avoid crashing on exit
    // half the time?
//    auto AC = AudioComponent;
//    TaskQueue.PostTask([AC]() {
//		AsyncTask(ENamedThreads::GameThread, [AC]() {
//			if (IsValid(AC) && AC->IsPlaying())
//			{
//                UE_LOG(LogVerseAudio, Log, TEXT("FVerseAudioModule::StopPlayout lambda: AudioComponent->IsPlaying() was true, trying to stop..."));
//				AC->Stop();
//			}
//		});
//	});

    return 0;
}

bool FVerseAudioModule::Playing() const
{
    UE_LOG(LogVerseAudio, Log,
            TEXT("FVerseAudioModule::Playing(): %d"), IsPlaying);
    // The CritScope automatically synchronizes for the current block by
    // releasing the mutex in the cs virable's destructor
    rtc::CritScope cs(&CriticalSection);
    return IsPlaying;
}


/**
 * Runs QueueAudioData periodically with a delay so that it runs when there's
 * data avialable, and returns control to the task queue in between.
 */
void FVerseAudioModule::ScheduleQueueAudioData()
{
    rtc::CritScope _cs(&CriticalSection);

    if(!IsStarted)
    {
        UE_LOG(LogVerseAudio, Log, TEXT("FVerseAudioModule::ScheduleQueueAudioData() starting"));
        NextQueueAudioDataTime = rtc::TimeMillis();
        IsStarted = true;
    }

    if(IsPlaying)
    {
        QueueAudioData();
        NextQueueAudioDataTime += MS_PER_FRAME;
        const int64_t now = rtc::TimeMillis();
        int64_t delay = 0;
        if(NextQueueAudioDataTime > now)
        {
            delay = NextQueueAudioDataTime - now;
        }
        TaskQueue.PostDelayedTask(
            [this]() { ScheduleQueueAudioData(); },
            int32_t(delay)
        );
    }
    else
    {
        UE_LOG(LogVerseAudio, Log, TEXT("FVerseAudioModule::ScheduleQueueAudioData() stopping"));
    }
}

void FVerseAudioModule::QueueAudioData()
{
    // We don't really care about these values, but the NeedMorePlayData method
    // wants to write to these as output parameters
    size_t nSamplesOut;
    int64_t elapsed_time_ms, ntp_time_ms;

    AudioTransport->NeedMorePlayData(
            SAMPLE_COUNT,
            BYTES_PER_SAMPLE,
            CHANNEL_COUNT,
            SAMPLES_PER_SECOND,
            AudioData,
            nSamplesOut,
            &elapsed_time_ms,
            &ntp_time_ms
            );

    bool FoundNonZero = false;
    for(size_t i=0; i < SAMPLE_COUNT*BYTES_PER_SAMPLE; i++)
    {
        if(AudioData[i] != 0)
        {
            FoundNonZero = true;
        }
    }

    if(ReceivingSilence && FoundNonZero)
    {
        ReceivingSilence = false;
        UE_LOG(LogVerseAudio, Log,
            TEXT("FVerseAudioModule::QueueAudioData(): getting audio data"));
    }
    else if(!ReceivingSilence && !FoundNonZero)
    {
        ReceivingSilence = true;
        UE_LOG(LogVerseAudio, Log,
            TEXT("FVerseAudioModule::QueueAudioData(): getting silence"));
    }

    SoundWave->QueueAudio(AudioData, SAMPLE_COUNT*BYTES_PER_SAMPLE);
}

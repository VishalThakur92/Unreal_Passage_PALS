// Copyright Enva Division

#include "VerseConnection.h"
#include "LogThreadId.h"
#include "VerseAudioModule.h"

#include <string>
#include <vector>
#include <thread>

// UE4 JSON module
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

// UE4 WebSocket module
#include "IWebSocket.h"
#include "PassageWebSocketsModule.h"

DEFINE_LOG_CATEGORY(LogVerseConnection);


webrtc::PeerConnectionInterface::RTCConfiguration UVerseConnection::GetDefaultConfig()
{
	webrtc::PeerConnectionInterface::RTCConfiguration DefaultConfig(
            webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive);

	DefaultConfig.set_cpu_adaptation(false);
	DefaultConfig.combined_audio_video_bwe.emplace(true);
	DefaultConfig.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

    // TODO: move this to environment variables or Blueprint variables that can
    // be set per instance rather than hard coded in the game binary.
    webrtc::PeerConnectionInterface::IceServer Server;
    Server.urls = {"turn:stun-webrtc.neonexperiment.com:3478"};
    Server.username = "YZFcx38wW7S3cSSvvbCZ";
    Server.password = "q2D3BeyTMF7w3HqAAG*U";
        
    DefaultConfig.servers.push_back(Server);

	return DefaultConfig;
}

UVerseConnection::UVerseConnection()
    :
    AudioComponent(nullptr),
    Texture2D(nullptr)
{
    UE_LOG(LogVerseConnection, Log, TEXT("UVerseConnection() constructor"));
 
    Status = CreateDefaultSubobject<UConnectionStatus>(
        MakeUniqueObjectName(this, UConnectionStatus::StaticClass(), TEXT("ConnectionStatus")));
}

UVerseConnection::~UVerseConnection()
{
}

void UVerseConnection::SetAudioComponent(UAudioComponent* AC)
{
    UE_LOG(LogVerseConnection, Log, TEXT("UVerseConnection::SetAudioComponent()"));
    AudioComponent = AC;
}

// You can read this linearly to get a pretty good idea of the connection
// process.
void UVerseConnection::Connect(FString& Url, FString& InChannelName)
{
    UE_LOG(LogVerseConnection, Log, TEXT("UVerseConnection::Connect()"));
    LogThreadId("UVerseConnection::Connect this must be the game thread");

    ChannelName = InChannelName;

    Status->SetStatus(EConnectionStatus::Connecting);
    
    // Setup the AudioComponent, RPC, PeerConnectionFactory, SignalingThread,
    // PubPc, and SubPc objects.
    Init(Url);

    // Once we have the RPC connection established (the WebSocket is open),
    // then we can automatically proceed with creating the offer. These lambdas
    // will run through the end in the order show. Any failure will be logged.
    // TODO: pass the failure reason through to a generic error event
    WS->OnConnected().AddLambda([this]() {
        CreatePubOffer();
    });

    UE_LOG(LogVerseConnection, VeryVerbose,
            TEXT("Adding lambda for CreatePubLocalDesc"));
    CreatePubLocalDesc->OnSuccessEvent().AddLambda(
        [this](webrtc::SessionDescriptionInterface* Desc) {
            PubPc->SetLocalDescription(SetPubLocalDesc, Desc); 
        });

    SetPubLocalDesc->OnSuccessEvent().AddLambda([this]() {
        SendJoinRequest(ChannelName);
    });

    RPC->OnNotify().AddLambda([this](FString Method, TSharedPtr<FJsonValue> Params) {
        if(Method == "offer")
        {
            // This is the incomming offer for negotiating the subscribe channel
            HandleSubOffer(Params);
        }
        else if(Method == "trickle")
        {
            HandleTrickle(Params);
        }
    });

    SetSubRemoteDesc->OnSuccessEvent().AddLambda([this]() {
        CreateSubAnswer();
    });

    CreateSubLocalDesc->OnSuccessEvent().AddLambda(
        [this](webrtc::SessionDescriptionInterface* desc)
        {
            SetSubAnswer(desc);
        }
    );

    SetSubLocalDesc->OnSuccessEvent().AddLambda([this]() {
        SendSubAnswer();
    }); 

    SubPcObserver.OnTrackEvent().AddLambda(
        [this](rtc::scoped_refptr<webrtc::RtpTransceiverInterface> Transceiver)
        {
            UE_LOG(LogVerseConnection, VeryVerbose, TEXT("SubPcObserver.OnTrackEvent() received transceiver"));
            if(Transceiver->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO)
            {
                webrtc::VideoTrackInterface* Track =
                    static_cast<webrtc::VideoTrackInterface*>(
                        Transceiver->receiver()->track().get());
                Track->AddOrUpdateSink(this, rtc::VideoSinkWants());
                UE_LOG(LogVerseConnection, VeryVerbose, TEXT("Broadcasting to TextureReadyDelegate"));
                OnTextureReady.Broadcast(GetTexture2D());
            }
        }
    );

    // This connection's Status tracks the state changes of the PeerConnection,
    // specifically the subscriber since that one carries the video data and
    // the publisher is unused.
    SubPcObserver.OnConnectionChangeEvent().AddLambda(
        [this](webrtc::PeerConnectionInterface::PeerConnectionState State) {
            using StateEnum = webrtc::PeerConnectionInterface::PeerConnectionState;
            switch (State) {
            case StateEnum::kNew:
                // do nothing
                break;
            case StateEnum::kConnecting:
                // do nothing
                break;
            case StateEnum::kConnected:
                Status->SetStatus(EConnectionStatus::Connected);
                break;
            case StateEnum::kDisconnected:
                // presumably we eventually want to reconnect in this condition
                // but for now we're not handling reconnects
                break;
            case StateEnum::kFailed:
                Status->SetStatus(EConnectionStatus::Failed);
                break;
            case StateEnum::kClosed:
                // this is expected when we close, but we're setting our closed
                // state in our Close() method, so setting it here would be 
                // redundant
                break;
            }
        }
    );

    // Finally connect once all the setup is completed
    WS->Connect();
}

void UVerseConnection::Close()
{
    UE_LOG(LogVerseConnection, VeryVerbose, TEXT("UVerseConnection::Close()"));

    if(RPC)
    {
        RPC->Close();
        UE_LOG(LogVerseConnection, VeryVerbose,
            TEXT("UVerseConnection::Close() closed FJsonRpc instance"));
    }

    if(WS)
    {
        // TODO: add a closure code/reason
        WS->Close();
    }

    if(PubPc)
    {
        PubPc->Close();
    }

    if(SubPc)
    {
        SubPc->Close();
    }

    Status->SetStatus(EConnectionStatus::Closed);
}

void UVerseConnection::Init(FString& Url)
{
    UE_LOG(LogVerseConnection, VeryVerbose, TEXT("UVerseConnection::Init()"));

    CreatePubLocalDesc =
        new rtc::RefCountedObject<FCreateSessionDescriptionObserver>();
    SetPubLocalDesc =
        new rtc::RefCountedObject<FSetSessionDescriptionObserver>();
    SetPubRemoteDesc =
        new rtc::RefCountedObject<FSetSessionDescriptionObserver>();

    SetSubRemoteDesc =
        new rtc::RefCountedObject<FSetSessionDescriptionObserver>();
    CreateSubLocalDesc =
        new rtc::RefCountedObject<FCreateSessionDescriptionObserver>();
    SetSubLocalDesc =
        new rtc::RefCountedObject<FSetSessionDescriptionObserver>();

    UE_LOG(LogVerseConnection, VeryVerbose, TEXT("UVerseConnection::Init() got through making observer instances"));

    if(AudioComponent == nullptr)
    {
        UE_LOG(LogVerseConnection, Error,
            TEXT("UVerseConnection::Connect() Must call "
                "UVerseConnection::SetAudioComponent(...) before calling "
                "Connect. Aborting connection."));
        return;
    }

    WS = FPassageWebSocketsModule::Get().CreateWebSocket(Url);
    auto WSC = MakeShared<FJsonRpcWebSocketChannel>(WS);
    auto RpcHandler = MakeShared<FJsonRpcEmptyHandler>();
    RPC = MakeShared<FJsonRpc>(WSC, RpcHandler);
 
    SignalingThread = MakeUnique<rtc::Thread>(rtc::SocketServer::CreateDefault());
    SignalingThread->SetName("PassageSignalingThread", nullptr);
    
    bool bStarted  = SignalingThread->Start();
    UE_LOG(LogVerseConnection, Log, TEXT("UVerseConnection::Init() SignallingThread->Start returned %s"),
        bStarted ? TEXT("true") : TEXT("false"));

    rtc::InitializeSSL();

    std::unique_ptr<webrtc::TaskQueueFactory> TaskQueueFactory =
        webrtc::CreateDefaultTaskQueueFactory();
    AudioModule = FVerseAudioModule::Create(
        TaskQueueFactory.get(), AudioComponent);

    PeerConnectionFactory = webrtc::CreatePeerConnectionFactory(
            nullptr, nullptr, SignalingThread.Get(),
            AudioModule,
            webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory(),
            webrtc::CreateBuiltinVideoEncoderFactory(),
            webrtc::CreateBuiltinVideoDecoderFactory(),
            nullptr, nullptr
        ).release();

    if(! PeerConnectionFactory)
    {
        UE_LOG(LogVerseConnection, Error, TEXT("Failed to make the PeerConnectionFactory instance"));
    }

    // Could set options on the PeerConnectionFactory here if we needed to.
    
    PubPc = PeerConnectionFactory->CreatePeerConnection(
            GetDefaultConfig(), // default config
            nullptr, nullptr, 
            &PubPcObserver
            );
    SubPc = PeerConnectionFactory->CreatePeerConnection(
            GetDefaultConfig(), // default config
            nullptr, nullptr, 
            &SubPcObserver
            );
}

void UVerseConnection::CreatePubOffer()
{
    UE_LOG(LogVerseConnection, VeryVerbose, TEXT("UVerseConnection::CreatePubOffer()"));
    PubPc->CreateDataChannel("dummy-data", nullptr);
    LogThreadId("UVerseConnection::CreatePubOffer()");

    UE_LOG(LogVerseConnection, Log,
        TEXT("UVerseConnection::CreatePubOffer() SignalingThread->IsCurrent(): %s"),
        SignalingThread->IsCurrent() ? TEXT("true") : TEXT("false")
    );

    SignalingThread->PostTask(RTC_FROM_HERE, [this]() {
        LogThreadId("SignalingThread->PostTask(...) this should be the signaling thread");

        UE_LOG(LogVerseConnection, Log,
            TEXT("SignalingThread->PostTask(...) SignalingThread->IsCurrent(): %s"),
            SignalingThread->IsCurrent() ? TEXT("true") : TEXT("false")
        );

        // We only need the default options
        using OfferOptions =
            webrtc::PeerConnectionInterface::RTCOfferAnswerOptions;
        OfferOptions Options;

        PubPc->CreateOffer(CreatePubLocalDesc, Options);
    });
}

UTexture2D* UVerseConnection::GetTexture2D()
{
    if(Texture2D)
    {
        return Texture2D;
    }
    else
    {
        const uint32_t Width = 320;
        const uint32_t Height = 240;
        const uint32_t ImgDataLength = 320*240*4;

        // See the call to webrtc::ConfertFromI420(...) for the generator that
        // matches this format. The byte ordering ends up being reversed, for
        // some reason.
        Texture2D = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
        Texture2D->UpdateResource();

        // FUpdateTextureRegion2D(DestX, DestY, SrcX, SrcY, Width, Height) 
        auto Regions = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);

        uint8* ImgData = new uint8[ImgDataLength];
        for(auto i = 0; i < ImgDataLength; i+=4)
        {
            ImgData[i+0] = 0;
            ImgData[i+1] = 255;
            ImgData[i+2] = 0;
            ImgData[i+3] = 255;
        }

        Texture2D->UpdateTextureRegions(
            0, // update MipIndex 0, i.e. the most detailed level
            1, // only update the 0th level, we don't need mipmaps for video
            Regions,
            (uint32)320*4, // "pitch" of data", i.e. bytes/pixel * width
            (uint32)4, // the size of each pixel in bytes
            ImgData,
            [](uint8* ImgData, const FUpdateTextureRegion2D* Regions)
            {
                delete Regions;
                delete[] ImgData;
            }
            );

        return Texture2D;
    }
}

void UVerseConnection::SendJoinRequest(FString& RemoteId)
{
    UE_LOG(LogVerseConnection, VeryVerbose, TEXT("UVerseConnection::SendJoinRequest()"));

    const webrtc::SessionDescriptionInterface* Desc =
        PubPc->local_description();

    std::string sdp_string;
    Desc->ToString(&sdp_string);
    FString SDP(sdp_string.c_str());


    // Send the offer to the peer
    auto MsgObject = MakeShared<FJsonObject>();

    // SID value will identifiy the speaker.
    MsgObject->SetStringField("sid", RemoteId);
    // The UID field is essentially unused because we're doing one speaker
    // per room.
    MsgObject->SetStringField("uid", FGuid::NewGuid().ToString());

    auto OfferObject = MakeShared<FJsonObject>();
    OfferObject->SetStringField("type", "offer");
    OfferObject->SetStringField("sdp", *SDP);

    MsgObject->SetObjectField("offer", OfferObject);

    FString Message;
    auto Writer = TJsonWriterFactory<>::Create(&Message);
    FJsonSerializer::Serialize(MsgObject, Writer);

    auto MsgValue = MakeShared<FJsonValueObject>(MsgObject);
    auto Future = RPC->Call("join", MsgValue);

    Async(EAsyncExecution::ThreadPool, [this, Future](){
        auto Response = Future.Get();
        if (Response.IsError) {
            FJsonRpc::Log(Response);
            return;
        }
        auto ResultValue = Response.Result;
        auto Result = ResultValue->AsObject();
        UE_LOG(LogVerseConnection, VeryVerbose,
            TEXT("UVerseConnection::SendJoinRequest(): received result from"
                " JSON-RPC `join` call"));

        // get the SDP field, we know it's an answer
        FString SDP;
        if(Result->TryGetStringField("sdp", SDP))
        {
            const std::string sdp_string(TCHAR_TO_UTF8(*SDP));
            webrtc::SdpParseError Error;
            webrtc::SessionDescriptionInterface* Desc(
                    webrtc::CreateSessionDescription(
                        "answer", sdp_string, &Error));

            // now we set it as the remote description
            // TODO: do we need to do this on the signaling thread?
            PubPc->SetRemoteDescription(
                    SetPubRemoteDesc,
                    Desc);
        }
        else 
        {
            UE_LOG(LogVerseConnection, Error, TEXT("UVerseConnection::"
                "SendJoinRequest(): no sdp field in join response"));
        }
    });
}

void UVerseConnection::HandleSubOffer(TSharedPtr<FJsonValue> ParamsValue)
{
    auto Params = ParamsValue->AsObject();
    // get the SDP field, we know it's an offer
    FString SDP;
    if(Params->TryGetStringField("sdp", SDP))
    {
        const std::string sdp_string(TCHAR_TO_UTF8(*SDP));
        webrtc::SdpParseError Error;
        webrtc::SessionDescriptionInterface* Desc(
                webrtc::CreateSessionDescription(
                    "offer", sdp_string, &Error));

        // now we set it as the remote description
        // TODO: do we need to do this on the signaling thread?
        SubPc->SetRemoteDescription(
                SetSubRemoteDesc,
                Desc);
    }
}

void UVerseConnection::CreateSubAnswer()
{
    using OfferOptions =
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions;
    OfferOptions Options;
    Options.offer_to_receive_audio = OfferOptions::kOfferToReceiveMediaTrue;
    Options.offer_to_receive_video = OfferOptions::kOfferToReceiveMediaTrue;
    SignalingThread->PostTask(RTC_FROM_HERE, [this, Options]() {
        SubPc->CreateAnswer(
                CreateSubLocalDesc,
                Options);
    });
}

void UVerseConnection::SetSubAnswer(webrtc::SessionDescriptionInterface* Desc)
{
    SignalingThread->PostTask(RTC_FROM_HERE, [this, Desc]() {
        SubPc->SetLocalDescription(
                SetSubLocalDesc,
                Desc);
    });
}

void UVerseConnection::SendSubAnswer()
{
    const auto Desc = SubPc->local_description();

    std::string sdp_string;
    Desc->ToString(&sdp_string);
    FString SDP(sdp_string.c_str());


    // Send the offer to the peer
    auto MsgObject = MakeShared<FJsonObject>();

    MsgObject->SetStringField("sid", "ion");
    MsgObject->SetStringField("uid", "TODO: make me a UUID");

    auto AnswerObject = MakeShared<FJsonObject>();
    AnswerObject->SetStringField("type", "answer");
    AnswerObject->SetStringField("sdp", *SDP);

    MsgObject->SetObjectField("desc", AnswerObject);

    FString Message;
    auto Writer = TJsonWriterFactory<>::Create(&Message);
    FJsonSerializer::Serialize(MsgObject, Writer);

    auto MsgValue = MakeShared<FJsonValueObject>(MsgObject);

    UE_LOG(LogVerseConnection, Log,
        TEXT("UVerseConnection::SendSubAnswer about to notify with answer SDP"));
    RPC->Notify("answer", MsgValue);
}

bool UVerseConnection::TryUnwrapTrickleParams(TSharedPtr<FJsonObject> Params,
        std::string& SdpMid,
        int& SdpMLineIndex,
        std::string& Sdp)
{
    const TSharedPtr<FJsonObject>* Candidate = nullptr;
    if(!Params->TryGetObjectField("candidate", Candidate))
    {
        UE_LOG(LogVerseConnection, Error,
            TEXT("No candidate field in trickle params"));
        return false;
    }

    FString fSdp;
    if(!(*Candidate)->TryGetStringField("candidate", fSdp))
    {
        UE_LOG(LogVerseConnection, Error,
                TEXT("No 'candidate' field in trickle params.candidate"));
        return false;
    }
    Sdp = std::string(TCHAR_TO_UTF8(*fSdp));

    if(!(*Candidate)->TryGetNumberField("sdpMLineIndex", SdpMLineIndex))
    {
        UE_LOG(LogVerseConnection, Error,
                TEXT("No 'sdpMLineIndex' field in trickle params.candidate"));
        return false;
    }

    FString fSdpMid;
    if(!(*Candidate)->TryGetStringField("sdpMid", fSdpMid))
    {
        UE_LOG(LogVerseConnection, Error,
            TEXT("No 'sdpMid' field in trickle params.candidate"));
        return false;
    }
    SdpMid = std::string(TCHAR_TO_UTF8(*fSdpMid));

    return true;
}

void UVerseConnection::HandleTrickle(TSharedPtr<FJsonValue> ParamsValue)
{
    auto Params = ParamsValue->AsObject();
    int32_t Target;
    if(!Params->TryGetNumberField("target", Target))
    {
        UE_LOG(LogVerseConnection, Error,
            TEXT("No target field in trickle params"));
        return;
    }

    std::string SdpMid;
    int SdpMLineIndex;
    std::string Sdp;
    if(!TryUnwrapTrickleParams(Params, SdpMid, SdpMLineIndex, Sdp))
    {
        return;
    }

    webrtc::SdpParseError Error;
    std::unique_ptr<webrtc::IceCandidateInterface> Candidate(
        webrtc::CreateIceCandidate(
            SdpMid, SdpMLineIndex, Sdp, &Error));
    switch(Target){

        case 0: // Pub
            PubPc->AddIceCandidate(Candidate.release());
            break;
        case 1: // Sub
            SubPc->AddIceCandidate(Candidate.release());
            break;
        default:
            UE_LOG(LogVerseConnection, Error,
                TEXT("Target is %d, but it should be 0 for pub or 1 for sub"),
                Target);
    }
}

void UVerseConnection::OnFrame(const webrtc::VideoFrame& frame)
{
    //UE_LOG(LogVerseConnection, Log, TEXT("FRAME"));
    const uint32_t Width = 320;
    const uint32_t Height = 240;
    const uint32_t ImgDataLength = 320*240*4;
    UTexture2D* Texture = GetTexture2D();

    // FUpdateTextureRegion2D(DestX, DestY, SrcX, SrcY, Width, Height) 
    auto Regions = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);

    uint32_t Size = webrtc::CalcBufferSize(
            webrtc::VideoType::kARGB,
            frame.width(),
            frame.height());
    // TODO: promote this to a class member
    uint8* ImgData = new uint8[Size];

    // We request ARGB, but this seems to deliver BGRA. See the call to
    // UTexture2D::CreateTransient, which sets how this is interpreted by the
    // renderer. Somehow it is the reverse byte order, so maybe there are
    // mismatching assumptions about the ordering of ImgData.
    webrtc::ConvertFromI420(frame, webrtc::VideoType::kARGB, 0, ImgData);

    Texture->UpdateTextureRegions(
        0, // update MipIndex 0, i.e. the most detailed level
        1, // only update the 0th level, we don't need mipmaps for video
        Regions,
        (uint32)320*4, // "pitch" of data", i.e. bytes/pixel * width
        (uint32)4, // the size of each pixel in bytes
        ImgData,
        [](uint8* ImgData, const FUpdateTextureRegion2D* Regions)
        {
            delete Regions;
            delete[] ImgData;
        }
        );
}


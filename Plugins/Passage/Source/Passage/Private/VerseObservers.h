// Copyright Enva Division

#pragma once

#include "LogThreadId.h"
#include "WebRtcGuards.h"

// Note that this macro is slightly different to the usual
// DECLARE_LOG_CATEGORY_EXTERN. The verison ending in _CLASS is used here
// because this is a header-only implementation and we don't have a DEFINE
// macro call in a .cpp file somewhere.
DECLARE_LOG_CATEGORY_CLASS(LogVerseObservers, Log, All);


/**
 * The PeerConnectionObserver monitors the progress of establishing and running
 * WebRTC peer connections. This interface is defined by libwebrtc and this
 * implementation adapts its methods to UE4 events so that the caller can use
 * AddLambda or other UE4 idioms for handling those events.
 *
 * All the events are logged here to help with tracing so that the
 * implementation can be a bit cleaner without as many logging stamements.
 *
 * Due to the name conflict between the observer methods specified by
 * libwebrtc's PeerConnectionObserver class versus the typical event naming
 * scheme in UE4, the suffix 'Event' has been appended to each method name that
 * provides the delegate implementation. For example, you would call
 * OnSignalingChangeEvent().AddLamba(...) instead of the more typical UE4 idiom
 * OnSignalingChange().AddLamba(...)
 */
class FPeerConnectionObserver :
    public webrtc::PeerConnectionObserver
{
public:
    
    // OnSignalingChange
    DECLARE_MULTICAST_DELEGATE_OneParam( FSignalingChangeEvent,
        webrtc::PeerConnectionInterface::SignalingState )
    FSignalingChangeEvent& OnSignalingChangeEvent()
    {
        return SignalingChangeEvent;
    }
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state)
    {
        FString State;
        using StateEnum = webrtc::PeerConnectionInterface::SignalingState;
        switch(new_state) {
            case StateEnum::kStable:
                State = FString(TEXT("Stable"));
                break;
            case StateEnum::kHaveLocalOffer:
                State = FString(TEXT("HaveLocalOffer"));
                break;
            case StateEnum::kHaveLocalPrAnswer:
                State = FString(TEXT("HaveLocalPrAnswer"));
                break;
            case StateEnum::kHaveRemoteOffer:
                State = FString(TEXT("HaveRemoteOffer"));
                break;
            case StateEnum::kHaveRemotePrAnswer:
                State = FString(TEXT("HaveRemotePrAnswer"));
                break;
            case StateEnum::kClosed:
                State = FString(TEXT("kClosed"));
                break;
            default:
                State = FString(TEXT("ERROR - UNKNOWN SIGNALING STATE"));
        }
        UE_LOG(LogVerseObservers, Verbose,
            TEXT("FPeerConnectionObserver::OnSignalingChange('%s')"), *State);
        Async(EAsyncExecution::TaskGraphMainThread, [this, new_state](){
            OnSignalingChangeEvent().Broadcast(new_state);
        });
    }

    // OnAddStream
    DECLARE_MULTICAST_DELEGATE_OneParam( FAddStreamEvent,
        rtc::scoped_refptr<webrtc::MediaStreamInterface> )
    FAddStreamEvent& OnAddStreamEvent()
    {
        return AddStreamEvent;
    }
	void OnAddStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnAddStream()"));
        Async(EAsyncExecution::TaskGraphMainThread, [this, stream](){
            OnAddStreamEvent().Broadcast(stream);
        });
    }

    // OnRemoveStream
    DECLARE_MULTICAST_DELEGATE_OneParam( FRemoveStreamEvent,
        rtc::scoped_refptr<webrtc::MediaStreamInterface> )
    FRemoveStreamEvent& OnRemoveStreamEvent()
    {
        return RemoveStreamEvent;
    }
	void OnRemoveStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnRemoveStream()"));
        Async(EAsyncExecution::TaskGraphMainThread, [this, stream](){
            OnRemoveStreamEvent().Broadcast(stream);
        });
    }

    // OnAddTrack
    DECLARE_MULTICAST_DELEGATE_TwoParams( FAddTrackEvent,
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
    FAddTrackEvent& OnAddTrackEvent()
    {
        return AddTrackEvent;
    }
	void OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) 
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnAddTrack()"));
        Async(EAsyncExecution::TaskGraphMainThread, [this, receiver, streams](){
            OnAddTrackEvent().Broadcast(receiver, streams);
        });
    }

    // OnTrack
    DECLARE_MULTICAST_DELEGATE_OneParam( FTrackEvent,
        rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver )
    FTrackEvent& OnTrackEvent()
    {
        return TrackEvent;
    }
	void OnTrack(
        rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnTrack()"));
        Async(EAsyncExecution::TaskGraphMainThread, [this, transceiver](){
            OnTrackEvent().Broadcast(transceiver);
        });
    }

    // OnRemoveTrack
    DECLARE_MULTICAST_DELEGATE_OneParam( FRemoveTrackEvent,
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> )
    FRemoveTrackEvent& OnRemoveTrackEvent()
    {
        return RemoveTrackEvent;
    }
	void OnRemoveTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnRemoveTrack()"));
        Async(EAsyncExecution::TaskGraphMainThread, [this, receiver](){
            OnRemoveTrackEvent().Broadcast(receiver);
        });
    }

    // OnDataChannel
    DECLARE_MULTICAST_DELEGATE_OneParam( FDataChannelEvent,
        rtc::scoped_refptr<webrtc::DataChannelInterface> )
    FDataChannelEvent& OnDataChannelEvent()
    {
        return DataChannelEvent;
    }
	void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnDataChannel()"));
        Async(EAsyncExecution::TaskGraphMainThread, [this, channel](){
            OnDataChannelEvent().Broadcast(channel);
        });
    }

    // OnRenegotiationNeeded
    DECLARE_MULTICAST_DELEGATE( FRenegotiationNeededEvent )
    FRenegotiationNeededEvent& OnRenegotiationNeededEvent()
    {
        return RenegotiationNeededEvent;
    }
	void OnRenegotiationNeeded()
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnRenegotiationNeeded()"));
        Async(EAsyncExecution::TaskGraphMainThread, [this](){
            OnRenegotiationNeededEvent().Broadcast();
        });
    }

    // OnIceConnectionChange
    DECLARE_MULTICAST_DELEGATE_OneParam( FIceConnectionChangeEvent,
        webrtc::PeerConnectionInterface::IceConnectionState )
    FIceConnectionChangeEvent& OnIceConnectionChangeEvent()
    {
        return IceConnectionChangeEvent;
    }
	void OnIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState new_state)
    {
        FString State;
        using StateEnum = webrtc::PeerConnectionInterface::IceConnectionState;
        switch(new_state) {
            case StateEnum::kIceConnectionNew:
                State = FString(TEXT("New"));
                break;
            case StateEnum::kIceConnectionChecking:
                State = FString(TEXT("Checking"));
                break;
            case StateEnum::kIceConnectionConnected:
                State = FString(TEXT("Connected"));
                break;
            case StateEnum::kIceConnectionCompleted:
                State = FString(TEXT("Completed"));
                break;
            case StateEnum::kIceConnectionFailed:
                State = FString(TEXT("Failed"));
                break;
            case StateEnum::kIceConnectionDisconnected:
                State = FString(TEXT("Disconnected"));
                break;
            case StateEnum::kIceConnectionClosed:
                State = FString(TEXT("Closed"));
                break;
            case StateEnum::kIceConnectionMax:
                State = FString(TEXT("Max"));
                break;
            default:
                State = FString(TEXT("ERROR - UNKNOWN ICE CONNECTION STATE"));
        }
        UE_LOG(LogVerseObservers, Verbose,
            TEXT("FPeerConnectionObserver::OnIceConnectionChange('%s')"), *State);
        Async(EAsyncExecution::TaskGraphMainThread, [this, new_state](){
            OnIceConnectionChangeEvent().Broadcast(new_state);
        });
    }

    // OnConnectionChange
    DECLARE_MULTICAST_DELEGATE_OneParam( FConnectionChangeEvent,
        webrtc::PeerConnectionInterface::PeerConnectionState )
    FConnectionChangeEvent& OnConnectionChangeEvent()
    {
        return ConnectionChangeEvent;
    }
    void OnConnectionChange(
        webrtc::PeerConnectionInterface::PeerConnectionState new_state)
    {
        FString State;
        using StateEnum = webrtc::PeerConnectionInterface::PeerConnectionState;
        switch(new_state) {
            case StateEnum::kNew:
                State = FString(TEXT("New"));
                break;
            case StateEnum::kConnecting:
                State = FString(TEXT("Connecting"));
                break;
            case StateEnum::kConnected:
                State = FString(TEXT("Connected"));
                break;
            case StateEnum::kDisconnected:
                State = FString(TEXT("Disconnected"));
                break;
            case StateEnum::kFailed:
                State = FString(TEXT("Failed"));
                break;
            case StateEnum::kClosed:
                State = FString(TEXT("Closed"));
                break;
            default:
                State = FString(TEXT("ERROR - UNKNOWN PEER CONNECTION STATE"));
        }
        UE_LOG(LogVerseObservers, Verbose,
            TEXT("FPeerConnectionObserver::OnConnectionChange('%s')"), *State);
        Async(EAsyncExecution::TaskGraphMainThread, [this, new_state](){
            OnConnectionChangeEvent().Broadcast(new_state);
        });
    }
    
    // OnIceGatheringChange
    DECLARE_MULTICAST_DELEGATE_OneParam( FIceGatheringChangeEvent,
        webrtc::PeerConnectionInterface::IceGatheringState new_state )
    FIceGatheringChangeEvent& OnIceGatheringChangeEvent()
    {
        return IceGatheringChangeEvent;
    }
	void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state)
    {
        FString State;
        using StateEnum = webrtc::PeerConnectionInterface::IceGatheringState;
        switch(new_state) {
            case StateEnum::kIceGatheringNew:
                State = FString(TEXT("New"));
                break;
            case StateEnum::kIceGatheringGathering:
                State = FString(TEXT("Gathering"));
                break;
            case StateEnum::kIceGatheringComplete:
                State = FString(TEXT("Complete"));
                break;
            default:
                State = FString(TEXT("ERROR - UNKNOWN ICE GATHERING STATE"));
        }
        UE_LOG(LogVerseObservers, Verbose,
            TEXT("FPeerConnectionObserver::OnIceGatheringChange('%s')"),
            *State);
        Async(EAsyncExecution::TaskGraphMainThread, [this, new_state](){
            OnIceGatheringChangeEvent().Broadcast(new_state);
        });
    }

    // OnIceCandidate
    DECLARE_MULTICAST_DELEGATE_OneParam( FIceCandidateEvent,
        const webrtc::IceCandidateInterface* candidate )
    FIceCandidateEvent& OnIceCandidateEvent()
    {
        return IceCandidateEvent;
    }
	void OnIceCandidate(
        const webrtc::IceCandidateInterface* candidate)
    {
        std::string candidate_string;
        candidate->ToString(&candidate_string);
        FString CandidateString(candidate_string.c_str());
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnIceCandidate('%s')"),
            *CandidateString);
        Async(EAsyncExecution::TaskGraphMainThread, [this, candidate](){
            OnIceCandidateEvent().Broadcast(candidate);
        });
    }

    // OnIceConnectionReceivingChange
    DECLARE_MULTICAST_DELEGATE_OneParam(
        FIceConnectionReceivingChangeEvent, bool )
    FIceConnectionReceivingChangeEvent& OnIceConnectionReceivingChangeEvent()
    {
        return IceConnectionReceivingChangeEvent;
    }
	void OnIceConnectionReceivingChange(bool receiving)
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FPeerConnectionObserver::OnIceConnectionReceivingChange"
                "('%d')"),
            receiving);
        Async(EAsyncExecution::TaskGraphMainThread, [this, receiving](){
            OnIceConnectionReceivingChangeEvent().Broadcast(receiving);
        });
    }

private:
    FSignalingChangeEvent SignalingChangeEvent;
    FAddStreamEvent AddStreamEvent;
    FRemoveStreamEvent RemoveStreamEvent;
    FAddTrackEvent AddTrackEvent;
    FTrackEvent TrackEvent;
    FRemoveTrackEvent RemoveTrackEvent;
    FDataChannelEvent DataChannelEvent;
    FRenegotiationNeededEvent RenegotiationNeededEvent;
    FIceConnectionChangeEvent IceConnectionChangeEvent;
    FConnectionChangeEvent ConnectionChangeEvent;
    FIceGatheringChangeEvent IceGatheringChangeEvent;
    FIceCandidateEvent IceCandidateEvent;
    FIceConnectionReceivingChangeEvent IceConnectionReceivingChangeEvent;
};

/**
 * The SetSessionDescriptionObserver interface receives callbacks when the
 * session description (SDP) changes. Since there's both a local and a remote
 * description, we need two observer implementations. Each FPeerConnectionObserver
 * has only one member instance.
 */
class FSetSessionDescriptionObserver : 
    public webrtc::SetSessionDescriptionObserver 
{
public:

    DECLARE_MULTICAST_DELEGATE( FSuccessEvent )
    FSuccessEvent& OnSuccessEvent()
    {
        return SuccessEvent;
    }
    void OnSuccess()
    {
        UE_LOG(LogVerseObservers, VeryVerbose,
            TEXT("FSetSessionDescriptionObserver::OnSuccess()"));
        Async(EAsyncExecution::TaskGraphMainThread, [this](){
            OnSuccessEvent().Broadcast();
        });
    }

    DECLARE_MULTICAST_DELEGATE_OneParam( FFailureEvent,
        webrtc::RTCError error )
    FFailureEvent& OnFailureEvent()
    {
        return FailureEvent;
    }
    void OnFailure(webrtc::RTCError error)
    {
        FString Message = FString(UTF8_TO_TCHAR(error.message()));
        UE_LOG(LogVerseObservers, Error,
            TEXT("SetSesssionDescriptionObserver::OnFailure(): %s"), *Message);
        Async(EAsyncExecution::TaskGraphMainThread, [this, error](){
            OnFailureEvent().Broadcast(error);
        });
    }

private:
    FSuccessEvent SuccessEvent;
    FFailureEvent FailureEvent;
};

/**
 * This observer receives callbacks when the SDP is created, i.e. in generating
 * an offer or answer. The difference to the SetSessionDescriptionObserver is
 * the signature of the OnSuccess method, which actually accepts the SDP
 * itself.
 */
class FCreateSessionDescriptionObserver :
    public webrtc::CreateSessionDescriptionObserver
{
public:

    DECLARE_MULTICAST_DELEGATE_OneParam( FSuccessEvent,
        webrtc::SessionDescriptionInterface* )
    FSuccessEvent& OnSuccessEvent()
    {
        return SuccessEvent;
    }
    void OnSuccess(webrtc::SessionDescriptionInterface* desc)
    {
        // Note that we may be called from a third party thread, so we post
        // the broadcast back to the game thread. Anything coming out of
        Async(EAsyncExecution::TaskGraphMainThread, [this, desc](){
            UE_LOG(LogVerseObservers, VeryVerbose,
                TEXT("FCreateSessionDescriptionObserver::OnSuccess() in lambda"));

            SuccessEvent.Broadcast(desc);
        });
    }

    DECLARE_MULTICAST_DELEGATE_OneParam( FFailureEvent,
        webrtc::RTCError error )
    FFailureEvent& OnFailureEvent()
    {
        return FailureEvent;
    }
    void OnFailure(webrtc::RTCError error)
    {
        FString Message = FString(UTF8_TO_TCHAR(error.message()));
        UE_LOG(LogVerseObservers, Error,
            TEXT("FCreateSessionDescriptionObserver::OnFailure(): %s"),
            *Message);
        Async(EAsyncExecution::TaskGraphMainThread, [this, error](){
            OnFailureEvent().Broadcast(error);
        });
    }

private:
    FSuccessEvent SuccessEvent;
    FFailureEvent FailureEvent;
};


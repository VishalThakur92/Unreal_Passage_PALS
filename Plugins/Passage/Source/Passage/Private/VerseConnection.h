// Copyright Enva Division

#pragma once

#include "CoreMinimal.h"

#include "WebRtcGuards.h"

#include "ConnectionStatus.h"
#include "JsonRpc.h"
#include "VerseObservers.h"

#include "VerseConnection.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVerseConnection, Log, All);

class IWebSocket;
class UVerseConnection;

/**
 * This class is only used indirectly via the UVerseVideoChatProvider. To use
 * this, construct the instance and call the SetAudioComponent(...) method, 
 * passing it a UAudioComponent instance that will receive remote user's audio
 * data. After setting the UAudioComponent pointer, you can then call
 * Connect(Url) and pass the Ion JSON-RPC endpoint URL. The rest should happen
 * automatically after the call to Connect.
 *
 * At any time you can call the GetTexture2D() method and connect the resulting
 * UTexture2D to a material. That texture will be updated with video data
 * whenever it is available.
 *
 * A major assumption made in this implementation is that some other component
 * will handle player presence and identifying users/avatars by a unique
 * identifier.
 */
UCLASS()
class PASSAGE_API UVerseConnection :
    public UObject,
    public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
    GENERATED_BODY()

public:

	UVerseConnection();
	~UVerseConnection();

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnTextureReady, UTexture2D*)
    FOnTextureReady OnTextureReady;

    /**
     * Even though this class is not blueprintable, we may want to expose this
     * connection status to blueprint. ConnectionStatus instances are blueprint
     * types and have blueprint friendly dynamic delegates.
     */
    UPROPERTY()
    UConnectionStatus* Status;

    /**
     * You must set the AudioComponent before calling Connect. UObjects must have
     * default constructors, so this couldn't be a constructor parameter.
     */
    UFUNCTION()
    void SetAudioComponent(UPARAM() UAudioComponent* AudioComponent);

    /**
     * Creates a WebRTC peer connection to the Ion SFU using the Url as the base
     * and appending the RemoteId as the last URL element. Since this signifies
     * the "room" in Ion, we have an arrangement where each speaker is in their
     * own room with all their listeners; only one user is broadcasting, directly
     * from their browser, and all others, every Unreal game instance, are only
     * receiving what comes from the browser.
     * 
     * You can read the Connect method linearly to get an idea of the connection
     * negotiation process. All the events and AddLambda business is more or less
     * like `promise.then(...)` handlers in JavaScript.
     */
    UFUNCTION()
    void Connect(UPARAM(ref) FString& Url, UPARAM(ref) FString& ChannelName);

    UFUNCTION()
    void Close();


protected:

    // rtc::VideoSinkInterface<webrtc::VideoFrame> implementation
    void OnFrame(const webrtc::VideoFrame& frame) override;

private:    
    TSharedPtr<IWebSocket> WS;
    TSharedPtr<FJsonRpc> RPC;
    TUniquePtr<rtc::Thread> SignalingThread;

    UAudioComponent* AudioComponent;

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnectionFactory;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> PubPc;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> SubPc;

    /**
     * Each connection uses a different AudioModule instance since the "module"
     * connects to an Audio Actor in the 3D scene.
     */
    rtc::scoped_refptr<webrtc::AudioDeviceModule> AudioModule;

    FPeerConnectionObserver PubPcObserver;
    rtc::scoped_refptr<FCreateSessionDescriptionObserver> CreatePubLocalDesc;
    rtc::scoped_refptr<FSetSessionDescriptionObserver> SetPubLocalDesc;
    rtc::scoped_refptr<FSetSessionDescriptionObserver> SetPubRemoteDesc;

    FPeerConnectionObserver SubPcObserver;
    rtc::scoped_refptr<FSetSessionDescriptionObserver> SetSubRemoteDesc;
    rtc::scoped_refptr<FCreateSessionDescriptionObserver> CreateSubLocalDesc;
    rtc::scoped_refptr<FSetSessionDescriptionObserver> SetSubLocalDesc;

    void Init(FString& Url);
    void CreatePubOffer();
    void SendJoinRequest(FString& RemoteId);

    void HandleSubOffer(TSharedPtr<FJsonValue> Params);
    void CreateSubAnswer();
    void SetSubAnswer(webrtc::SessionDescriptionInterface* desc);
    void SendSubAnswer();

    void HandleTrickle(TSharedPtr<FJsonValue> Params);

    UTexture2D* Texture2D;
    UTexture2D* GetTexture2D();
    FString ChannelName;

    /**
     * We use the same RTCConfiguration for both the PubPc and SubPc, so this
     * returns the common configuration prameters.
     */
    static webrtc::PeerConnectionInterface::RTCConfiguration GetDefaultConfig();

    /**
     * A little helper function to unrwap the Params and log any necessary errors.
     */
    static bool TryUnwrapTrickleParams(
        TSharedPtr<FJsonObject> Params,
        std::string& SdpMid,
        int& SdpMLineIndex,
        std::string& Sdp);
};

// Copyright Enva Division, 2022

#pragma once

#include "CoreMinimal.h"

#include "ConnectionStatus.h"
#include "Participant.h"

#include "VideoChatProvider.generated.h"

UCLASS(Abstract, Blueprintable)
class PASSAGE_API UVideoChatProvider : public UObject
{
    GENERATED_BODY()

public:

    UVideoChatProvider();

    /**
     Indicates the availability/connectivity of the underlying service.
     */
    UPROPERTY(BlueprintReadonly)
    UConnectionStatus* Status;

    /**
     Notifies the provider that the given media components belong to the participant

     @note This method is telling the ViedoChatProvider implementation that we want media for this participant when it is available, and here's where to display it. The actual data may start flowing very soon after the call or much later when the participant's computer actually starts sending data.

     @param Participant The Participant, obtained from the DirectoryProvider implementation, to which the media belongs.
     @param Material The video chat provider will update this material instance with video data as it becomes available.
     @param ParameterName The texture parameter of the given Material that is designed to display the video data, i.e. the RGB base color.
     @param AudioComponent This audio component should be attached to the participant's avatar and setup with conventional Unreal audio attenuation.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void AttachMedia(
        UPARAM() UParticipant* Participant,
        UPARAM() UMaterialInstanceDynamic* Material,
        UPARAM() const FString& ParameterName,
        UPARAM() UAudioComponent* AudioComponent);

    /**
     This should immediately stop updating the participant's video/audio. If the caller takes no other action, most likely the video will appear frozen. This should be immediate so that the relevant components can be cleaned up and/or destroyed with confidence.

     @param Participant The same Participant instance you passed to AttachMedia.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void DetachMedia(UPARAM() UParticipant* Participant);

};

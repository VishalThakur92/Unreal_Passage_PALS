// Copyright Enva Division, 2022

#include "VerseVideoChatProvider.h"

DEFINE_LOG_CATEGORY(LogVerseVideoChatProvider);

UVerseVideoChatProvider::UVerseVideoChatProvider()
{
	ConnectionFactory = NewObject<UVerseConnectionFactory>();
}

void UVerseVideoChatProvider::AttachMedia_Implementation(
	UParticipant* Participant,
	UMaterialInstanceDynamic* Material,
	const FString& ParameterName,
	UAudioComponent* AudioComponent
	)
{
	FParticipantInfo Info;

	if (ParticipantInfo.Contains(Participant)) {
		Info = ParticipantInfo[Participant];
		// TODO: no we need to stop updating the old material?
		Info.Material = Material;
		Info.MaterialParameter = ParameterName;
		Info.AudioComponent = AudioComponent;
	}
	else {
		UVerseConnection* VerseConnection = ConnectionFactory->CreateConnection();
		Info = { VerseConnection, Material, ParameterName, AudioComponent };
		ParticipantInfo.Add(Participant, Info);
	}

	if (IsValid(Info.Material) &&
		!Info.MaterialParameter.IsEmpty() &&
		IsValid(Info.AudioComponent))
	{
		if (Url.IsEmpty()) {
			UE_LOG(LogVerseVideoChatProvider, Error, TEXT(
				"BaseUrl is empty, unable to initialize connection to Verse video server"
			));
		}
		else {
			Info.VerseConnection->SetAudioComponent(Info.AudioComponent);

			Info.VerseConnection->OnTextureReady.AddLambda(
				[this, Info](UTexture2D* Texture) {
					Info.Material->SetTextureParameterValue(
						FName(Info.MaterialParameter),
						Texture);
				}
			);

			// Note that this will allow you to connect to listen to a
			// participant on a different server. So callers should be careful
			// to only listen to participants that have the same server
			// location as the local participant.
			FString ChannelName = FString::Printf(TEXT("%s/%s"),
				*(Participant->ServerLocation), *(Participant->Id));

			UE_LOG(LogVerseVideoChatProvider, Log,
				TEXT("Connecting to Url '%s' with sid '%s'"),
				*Url, *ChannelName);

			Info.VerseConnection->Connect(Url, ChannelName);
		}
	}
	else {
		// TODO: a useful error message would indicate which is invalid
		if (!IsValid(Info.Material)) {
			UE_LOG(LogVerseVideoChatProvider, Error, TEXT(
				"VerseVideoChatProvider::AttachMedia(): Invalid Material"
			));
		}
		if (Info.MaterialParameter.IsEmpty()) {
			UE_LOG(LogVerseVideoChatProvider, Error, TEXT(
				"VerseVideoChatProvider::AttachMedia(): Empty MaterialParameter string"
			));
		}
		if (!IsValid(Info.AudioComponent)) {
			UE_LOG(LogVerseVideoChatProvider, Error, TEXT(
				"VerseVideoChatProvider::AttachMedia(): Invalid AudioComponent"
			));
		}
	}
}

void UVerseVideoChatProvider::DetachMedia_Implementation(UParticipant* Participant) {
	if (IsValid(Participant) && ParticipantInfo.Contains(Participant)) {
		FParticipantInfo Info = ParticipantInfo[Participant];
		Info.VerseConnection->Close();
		ParticipantInfo.Remove(Participant);
	}
	else if(IsValid(Participant)) {
		UE_LOG(LogVerseVideoChatProvider, Error,
			TEXT("Cannot detach media. No record of media for Participant Id(%s)"),
			*(Participant->Id)
		);
	}
	else {
		UE_LOG(LogVerseVideoChatProvider, Error,
			TEXT("Cannot detach media. Invalid Participant pointer."));
	}
}

void UVerseVideoChatProvider::SetUrl(FString& Value)
{
	Url = Value;
}
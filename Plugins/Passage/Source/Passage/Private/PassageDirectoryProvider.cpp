// Fill out your copyright notice in the Description page of Project Settings.


#include "PassageDirectoryProvider.h"

DEFINE_LOG_CATEGORY(LogPassageDirectoryProvider);

UPassageDirectoryProvider::UPassageDirectoryProvider()
    : LocalParticipant(nullptr)
{
    Connection = CreateDefaultSubobject<UConnectionStatus>(
        MakeUniqueObjectName(this, UConnectionStatus::StaticClass(), TEXT("ConnectionStatus")));
	HeartbeatChannel = MakeShared<FJsonRpcHeartbeatChannel>();
    Handler = MakeShared<FRpcHandler>(this);
}

UPassageDirectoryProvider::~UPassageDirectoryProvider()
{
}

void UPassageDirectoryProvider::Connect(const FString& DirectoryUrl, const FString& DirectoryToken)
{
    TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *DirectoryToken));
    const TArray<FString> NoProtocols; // empty
    auto WebSocket =
        FPassageWebSocketsModule::Get().CreateWebSocket(DirectoryUrl, NoProtocols, Headers);

    // Once connected to the server, it will call our Joined() method with the
    // LocalParticipant. That sets our connection status to Connected.

	WebSocket->OnConnectionError().AddWeakLambda(
        this, [this](const FString& Error)
	{
        UE_LOG(LogPassageDirectoryProvider, Error, TEXT("PassageDirectoryProvider WebSocket error: %s"), *Error);
		if (IsValid(this) && IsValid(Connection)) {
			Connection->SetStatus(EConnectionStatus::Failed);
		}
	});
	WebSocket->OnClosed().AddWeakLambda(
        this, [this](const int32 StatusCode, const FString& Reason, const bool bWasClean)
	{
        UE_LOG(LogPassageDirectoryProvider, Verbose, TEXT("PassageDirectoryProvider WebSocket closed: %d %s %d"), StatusCode, *Reason, bWasClean);
		if(IsValid(this) && IsValid(Connection))
        {
            Connection->SetStatus(EConnectionStatus::Closed);
        }
        if (HeartbeatChannel.IsValid())
        {
            HeartbeatChannel->Stop();
        }
	});

	const auto WebSocketChannel = MakeShared<FJsonRpcWebSocketChannel>(WebSocket);
	
	HeartbeatChannel->Start(GetWorld(), WebSocketChannel, 5.0f);
	Rpc = MakeShared<FJsonRpc>(HeartbeatChannel, Handler);

	Connection->SetStatus(EConnectionStatus::Connecting);
	WebSocket->Connect();
}

void UPassageDirectoryProvider::Disconnect() const
{
    Rpc->Close();
	HeartbeatChannel->Close(1001, TEXT("Unreal game is disconnecting"));
}


UParticipant* UPassageDirectoryProvider::GetLocalParticipant_Implementation()
{
    return LocalParticipant;
}

TArray<UParticipant*> UPassageDirectoryProvider::GetParticipantsOnServer_Implementation()
{
    TArray<UParticipant*> Result;
    ParticipantsById.GenerateValueArray(Result);
    return Result;
}

TSharedPtr<FJsonRpcHandler> UPassageDirectoryProvider::GetHandler()
{
    return Handler;
}


FJsonRpcResponse UPassageDirectoryProvider::Joined(const TSharedPtr<FJsonObject>& Object)
{
	UE_LOG(LogPassageDirectoryProvider, Verbose, TEXT("UPassageDirectoryProvider::Joined()"));

    if(IsValid(LocalParticipant))
    {
		UE_LOG(LogPassageDirectoryProvider, Error, TEXT("UPassageDirectoryProvider::Joined() LocalParticipant is already set"));
        return FJsonRpc::Error(HandlerError, TEXT("LocalParticipant is already set"));
    }

    const auto Participant = GetParticipantById(Object->GetStringField("id"));
    UpdateParticipant(Participant, Object);
	LocalParticipant = Participant;
    LocalParticipant->IsLocal = true;

	Connection->SetStatus(EConnectionStatus::Connected);
	OnParticipantJoined.Broadcast(Participant);

    const auto Result = MakeShared<FJsonValueBoolean>(true);
    return { false, Result, nullptr };
}

FJsonRpcResponse UPassageDirectoryProvider::AddParticipants(const TArray< TSharedPtr<FJsonValue> > Data)
{
	UE_LOG(LogPassageDirectoryProvider, Verbose, TEXT("UPassageDirectoryProvider::AddParticipants()"));
    for(const auto Value : Data)
    {
        
        if(Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            const auto Id = Object->GetStringField("id");
			const auto Participant = GetParticipantById(Id);
            UpdateParticipant(Participant, Object);
        	ParticipantsById.Add(Participant->Id, Participant);

            OnParticipantJoined.Broadcast(Participant);
        }
        else
        {
			return FJsonRpc::Error(HandlerError, TEXT("AddParticipants Value is not an Object"));
        }
    }
    const auto Result = MakeShared<FJsonValueBoolean>(true);
    return { false, Result, nullptr };
}

FJsonRpcResponse UPassageDirectoryProvider::RemoveParticipants(TArray< TSharedPtr<FJsonValue> > Data)
{
    TArray<FString> IdsNotFound;
    for(const auto Item : Data)
    {
        if(const auto Id = Item->AsString(); ParticipantsById.Contains(Id))
        {
            const auto Participant = ParticipantsById[Id];
            ParticipantsById.Remove(Id);
            OnParticipantLeft.Broadcast(Participant);
        }
    	else
        {
            IdsNotFound.Push(Id);
        }
    }
    if(IdsNotFound.Num() == 0)
    {
        return { false, MakeShared<FJsonValueBoolean>(true), nullptr };
    }
    else
    {
        const auto Message = TEXT("Participant Id(s) not found: ") +
            FString::Join(IdsNotFound, TEXT(", "));
        return FJsonRpc::Error(InvalidParams, Message);
    }
    
}

FJsonRpcResponse UPassageDirectoryProvider::UpdateParticipants(
    TArray< TSharedPtr<FJsonValue> > ParticipantObjects)
{
    TArray<FString> IdsNotFound;
    for(const auto Item : ParticipantObjects)
    {

        if (const auto Id = Item->AsObject()->GetStringField("Id"); ParticipantsById.Contains(Id))
        {
            if (Item->Type == EJson::Object)
            {
                const auto Object = Item->AsObject();
                const auto Participant = ParticipantsById[Id];

                UpdateParticipant(Participant, Object);
				OnParticipantUpdated.Broadcast(Participant);
            }
            else
            {
                return FJsonRpc::Error(InvalidParams, TEXT("Expected a Participant object"));
            }
		}
		else
		{
            IdsNotFound.Push(Id);
        }
    }
    if (IdsNotFound.Num() == 0)
    {
        return { false, MakeShared<FJsonValueBoolean>(true), nullptr };
    }
    else
    {
        const auto Message = TEXT("Participant Id(s) not found: ") +
            FString::Join(IdsNotFound, TEXT(", "));
        return FJsonRpc::Error(InvalidParams, Message);
    }

}

void UPassageDirectoryProvider::UpdateParticipant(UParticipant* Participant, TSharedPtr<FJsonObject> Object)
{
	UE_LOG(LogPassageDirectoryProvider, Verbose, TEXT("UPassageDirectoryProvider::UpdateParticipant()"));
    if (Object->HasField("active"))
    {
        Participant->Active = Object->GetBoolField("active");
    }
    if (Object->HasField("screenName"))
    {
        Participant->ScreenName = Object->GetStringField("screenName");
    }
    if (Object->HasField("serverLocation"))
    {
        Participant->ServerLocation = Object->GetStringField("serverLocation");
    }
    if (Object->HasField("isPublishingMedia"))
    {
        Participant->IsPublishingMedia = Object->GetBoolField("isPublishingMedia");
    }
    if (Object->HasField("data"))
    {
        const auto Data = Object->GetObjectField("data");
        for (const auto& Pair : Data->Values)
        {
            // TODO: this will "leak" values because the server has no way of
            // signalling for removal. I'm not sure that's really a problem,
            // even long term, provided that a participant's lifetime is a
            // reasonable duration and that Data is not used too frequently.
            Participant->SetProperty(Pair.Key, Pair.Value->AsString());
        }
    }

}

UPassageDirectoryProvider::FRpcHandler::FRpcHandler(UPassageDirectoryProvider* InParent)
{
    Parent = InParent;
}


TSharedFuture<FJsonRpcResponse> UPassageDirectoryProvider::FRpcHandler::Handle(
    FString Method, TSharedPtr<FJsonValue> Params)
{
    UE_LOG(LogPassageDirectoryProvider, Verbose, TEXT("UPassageDirectoryProvider::FRpcHandler::Handle()"));

    // In our case all our implementations are synchronous, so these promises
    // will be resolved immediately.
    TPromise<FJsonRpcResponse> Promise;

    if (IsValid(Parent)) {
        if( Method == TEXT("Joined"))
        {
            if (const auto Array = Params->AsArray(); Array.Num() == 1) {
                Promise.SetValue(Parent->Joined(Array[0]->AsObject()));
            } else
            {
				Promise.SetValue(
                    FJsonRpc::Error(InvalidParams,
                    TEXT("Expected a single Participant object")));
            }
        }
        else if (Method == TEXT("AddParticipants")) {
            Promise.SetValue(Parent->AddParticipants(Params->AsArray()));
        }
        else if (Method == TEXT("RemoveParticipants")) {
            Promise.SetValue(Parent->RemoveParticipants(Params->AsArray()));
        }
        else if (Method == TEXT("UpdateParticipants")) {
            Promise.SetValue(Parent->UpdateParticipants(Params->AsArray()));
        }
        else {
            Promise.SetValue(FJsonRpc::Error(MethodNotFound,
                TEXT("No method named '") + Method + TEXT("'")));
        }
    }
    else {
        Promise.SetValue(FJsonRpc::Error(InternalError,
            TEXT("UPassageDirectoryProvider pointer invalid")));
    }

    return Promise.GetFuture().Share();
}
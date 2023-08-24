// Fill out your copyright notice in the Description page of Project Settings.

#include "PassageGlobals.h"

#include "DirectoryProvider.h"
#include "VideoChatProvider.h"
#include "Kismet/GameplayStatics.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PassageUtils.h"
#include "GenericPlatform/GenericPlatformHttp.h"


DEFINE_LOG_CATEGORY(LogPassageGlobals);

void UPassageGlobals::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogPassageGlobals, Log,
		TEXT("UPassageGlobals::Initialize -- Build time: %s"),
		*(UPassageGlobals::BuildTime));

	FetchEc2InstanceId();

	//GetGameInstance()->OnLocalPlayerAddedEvent.AddLambda([this](ULocalPlayer* LocalPlayer)
	//	{
	//		UE_LOG(LogTemp, Log, TEXT("UPassageGlobals handling UGameInstance::OnLocalPlayerAddedEvent"));
	//	});
}

void UPassageGlobals::Deinitialize()
{
	UE_LOG(LogPassageGlobals, Log, TEXT("UPassageGlobals::Deinitialize"));
}

UPassageGlobals* UPassageGlobals::GetPassageGlobals(const UObject* Context)
{
	UE_LOG(LogPassageGlobals, VeryVerbose, TEXT("UPassageGlobals::GetPassageGlobals()"));
	if (const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(Context); IsValid(GameInstance))
	{
		return UGameInstance::GetSubsystem<UPassageGlobals>(GameInstance);
	}
	return nullptr;
}

bool UPassageGlobals::GetPassagePlayer(UPassagePlayer*& PassagePlayer) const
{
	UE_LOG(LogPassageGlobals, VeryVerbose, TEXT("UPassageGlobals::GetPassagePlayer()"));
	if (const auto PlayerController = UGameplayStatics::GetPlayerController(this, 0);
		IsValid(PlayerController))
	{
		if(const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
			IsValid(LocalPlayer))
		{
			PassagePlayer = LocalPlayer->GetSubsystem<UPassagePlayer>();
			return true;
		}
	}
	return false;
}

UDirectoryProvider* UPassageGlobals::GetDirectoryProvider() const
{
	return DirectoryProvider;
}

void UPassageGlobals::SetDirectoryProvider(UDirectoryProvider* NewDirectoryProvider)
{
	DirectoryProvider = NewDirectoryProvider;
}

UVideoChatProvider* UPassageGlobals::GetVideoChatProvider() const
{
	return VideoChatProvider;
}

void UPassageGlobals::SetVideoChatProvider(UVideoChatProvider* NewVideoChatProvider)
{
	VideoChatProvider = NewVideoChatProvider;
}

void UPassageGlobals::FetchEc2InstanceId()
{
	UE_LOG(LogPassageGlobals, Verbose, TEXT("UPassageGlobals::FetchEc2InstanceId()"));
	const auto FetchRequest = FHttpModule::Get().CreateRequest();
	// This IP address is defined by AWS EC2 as an official source for metadata
	FetchRequest->SetURL(TEXT("http://169.254.169.254/latest/meta-data/instance-id"));
	FetchRequest->OnProcessRequestComplete().BindWeakLambda(this,
        [this](const FHttpRequestPtr Request, const FHttpResponsePtr Response, bool Connected)
		{
			// Request should be the same as FetchRequest, but we get it from the callback
			//const auto Response = Request->GetResponse();
			if (Request->GetStatus() == EHttpRequestStatus::Succeeded)
			{
				Ec2InstanceId = Response->GetContentAsString();
				UE_LOG(LogPassageGlobals, Verbose,
					TEXT("UPassageGlobals::FetchEc2InstanceId() Got Instance ID '%s'"), *Ec2InstanceId);
			} else
			{
				UE_LOG(LogPassageGlobals, Warning,
					TEXT("UPassageGlobals::FetchEc2InstanceId() unable to fetch EC2 instance ID, defaulting to localhost"));
				Ec2InstanceId = TEXT("localhost");
			}

			SendInstanceIdToBackend();
		});
	FetchRequest->ProcessRequest();
}

void UPassageGlobals::SendInstanceIdToBackend()
{
	UE_LOG(LogPassageGlobals, Verbose, TEXT("UPassageGlobals::SendInstanceIdToBackend()"));
	if(UPassageUtils::HasConfigFlag(TEXT("SuppressActionsForServerInEditor")))
	{
		UE_LOG(LogPassageGlobals, Verbose,
			TEXT("UPassageGlobals::SendInstanceIdToBackend() Skipping because we appear to be running in-editor with -SuppressActionsForServerInEditor flag"));
		return;
	}

	FString SignalingPort;
	UPassageUtils::GetConfigValue(
		TEXT("SignalingPortNumber"),
		TEXT("8000"),
		SignalingPort);
	SignalingPort = FGenericPlatformHttp::UrlEncode(SignalingPort);

	const FString InstanceId = FGenericPlatformHttp::UrlEncode(Ec2InstanceId);

	const FString BackendRoot = UPassageUtils::GetBackendRoot();
	FString BackendEndpoint = FString::Printf(TEXT("%s/add-game-instance"), *BackendRoot);

	// The dedicated server is either the -WaitForStreamer or -Level command line argument
	FString DedicatedServer;
	UPassageUtils::GetConfigValue(
		TEXT("WaitForStreamer"),
		TEXT(""),
		DedicatedServer);
	UPassageUtils::GetConfigValue(
		TEXT("Level"),
		DedicatedServer,
		DedicatedServer	);
	DedicatedServer = FGenericPlatformHttp::UrlEncode(DedicatedServer);

	const FString ProjectName = FGenericPlatformHttp::UrlEncode(
		FPaths::GetBaseFilename(
			FPaths::GetProjectFilePath(), true));
	UE_LOG(LogPassageGlobals, Log, TEXT("Deduced correct project name?: %s"), *ProjectName);

	UE_LOG(LogPassageGlobals, VeryVerbose,
		TEXT("UPassageGlobals::SendInstanceIdToBackend() sending instanceId=%s and port=%s to %s"),
		*InstanceId,
		*SignalingPort,
		*BackendEndpoint);

	const auto PostRequest = FHttpModule::Get().CreateRequest();
	PostRequest->SetVerb(TEXT("POST"));
	PostRequest->SetURL(BackendEndpoint);
	PostRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	PostRequest->SetContentAsString(FString::Printf(
		TEXT("instanceId=%s&port=%s&projectName=%s&dedicatedServer=%s"),
		*InstanceId,
		*SignalingPort,
		*ProjectName,
		*DedicatedServer
		));
	PostRequest->OnProcessRequestComplete().BindWeakLambda(this,
		[this, BackendEndpoint]
				(const FHttpRequestPtr Request, const FHttpResponsePtr Response, bool Connected)
		{
			// Request should be the same as FetchRequest, but we get it from the callback
			//const auto Response = Request->GetResponse();
			if (Request->GetStatus() == EHttpRequestStatus::Succeeded)
			{
				UE_LOG(LogPassageGlobals, Verbose,
					TEXT("UPassageGlobals::SendInstanceIdToBackend() Succeeded"));
				BroadcastGlobalEvent(this, TEXT("InstanceIdSentToBackend"));
			}
			else
			{
				UE_LOG(LogPassageGlobals, Error,
					TEXT("Unable to send our Instance ID to backend at '%s'"), *BackendEndpoint);
			}

		}
	);
	PostRequest->ProcessRequest();
}

// Can't be const because it doesn't match the signature expected by AddUFunction
// ReSharper disable once CppMemberFunctionMayBeConst
void UPassageGlobals::BroadcastNewConnection(FString _Trash, bool _AlsoTrash)
{
	Async(EAsyncExecution::TaskGraphMainThread, [=]()
		{
			PixelStreamNewConnection.Broadcast();
		});

}

void UPassageGlobals::StartAvailabilityLoop()
{
	UE_LOG(LogPassageGlobals, Verbose, TEXT("UPassageGlobals::StartAvailabilityLoop (this %p)"), this);
	if (AvailabilityLoopDelegate.IsBound()) {
		AvailabilityLoopDelegate.Unbind();
	}
	AvailabilityLoopDelegate.BindDynamic(this, &UPassageGlobals::SendAvailableFlagToBackend);
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			GetWorld()->GetTimerManager().SetTimer(
				AvailabilityLoopHandle,
				AvailabilityLoopDelegate,
				15.0f,
				true,
				0);
		});
}

void UPassageGlobals::StopAvailabilityLoop()
{
	UE_LOG(LogPassageGlobals, Verbose, TEXT("UPassageGlobals::StopAvailabilityLoop (this %p)"), this);
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			GetWorld()->GetTimerManager().ClearTimer(AvailabilityLoopHandle);
		});
}

void UPassageGlobals::StopAvailabilityLoop_DelegateHandler(FString _Trash, bool _AlsoTrash)
{
	this->StopAvailabilityLoop();
}


void UPassageGlobals::SendAvailableFlagToBackend()
{
	UE_LOG(LogPassageGlobals, Verbose, TEXT("UPassageGlobals::SendAvailableFlagToBackend()"));
	FString EditorServerArg;
	UPassageUtils::GetConfigValue(
		TEXT("SuppressActionsForServerInEditor"),
		TEXT("FALSE"),
		EditorServerArg
	);
	if (EditorServerArg != TEXT("FALSE"))
	{
		UE_LOG(LogPassageGlobals, Verbose,
			TEXT("UPassageGlobals::SendAvailableFlagToBackend() Skipping because we appear to be running in-editor with -SuppressActionsForServerInEditor flag"));
		return;
	}

	// If we haven't set our InstanceId yet, then we wait for it to be sent
	// before we make ourselves available.
	if (Ec2InstanceId.IsEmpty())
	{
		UPassageGlobals::GlobalEvent(this, TEXT("InstanceIdSentToBackend")).AddDynamic(
			this, &UPassageGlobals::SendAvailableFlagToBackend);
		return;
	}

	FString SignalingPort;
	UPassageUtils::GetConfigValue(
		TEXT("SignalingPortNumber"),
		TEXT("8000"),
		SignalingPort);
	SignalingPort = FGenericPlatformHttp::UrlEncode(SignalingPort);

	const FString InstanceId = FGenericPlatformHttp::UrlEncode(Ec2InstanceId);

	const FString BackendRoot = UPassageUtils::GetBackendRoot();
	FString BackendEndpoint = FString::Printf(TEXT("%s/make-game-instance-available"), *BackendRoot);

	UE_LOG(LogPassageGlobals, VeryVerbose,
		TEXT("UPassageGlobals::SendAvailableFlagToBackend() sending instanceId=%s and port=%s to %s"),
		*InstanceId,
		*SignalingPort,
		*BackendEndpoint);

	const auto PostRequest = FHttpModule::Get().CreateRequest();
	PostRequest->SetVerb(TEXT("POST"));
	PostRequest->SetURL(BackendEndpoint);
	PostRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	PostRequest->SetContentAsString(FString::Printf(
		TEXT("instanceId=%s&port=%s"),
		*InstanceId,
		*SignalingPort
	));
	PostRequest->OnProcessRequestComplete().BindWeakLambda(this,
		[this, BackendEndpoint]
	(const FHttpRequestPtr Request, const FHttpResponsePtr Response, bool Connected)
		{
			// Request should be the same as FetchRequest, but we get it from the callback
			//const auto Response = Request->GetResponse();
			if (Request->GetStatus() == EHttpRequestStatus::Succeeded)
			{
				UE_LOG(LogPassageGlobals, Verbose,
					TEXT("UPassageGlobals::SendAvailableFlagToBackend() Succeeded"));
			}
			else
			{
				UE_LOG(LogPassageGlobals, Error,
					TEXT("Unable to make ourselves available to backend at '%s'"), *BackendEndpoint);
			}

		}
	);
	PostRequest->ProcessRequest();
}

FPassageGlobalEvent& UPassageGlobals::GlobalEvent(const UObject* Context, const FString& EventName)
{
	if(UPassageGlobals* Globals = GetPassageGlobals(Context))
	{
		if(!Globals->Events.Contains(EventName))
		{
			Globals->Events.Add(EventName, {});
		}

		return Globals->Events[EventName];
	}
	else
	{
		UE_LOG(LogPassageGlobals, Error,
			TEXT("UPassageGlobals::Event() Called with invalid context, unable to get UPassageGlobals instance. Returned FPassageGlobalEvent for name '%s' will never receive broadcast."),
			*EventName);
		
		return ErrorGlobalEvent;
	}
}

void UPassageGlobals::BroadcastGlobalEvent(UObject* Context, const FString& EventName)
{
	UE_LOG(LogPassageGlobals, Verbose, TEXT("UPassageGlobals::BroadcastGlobalEvent() broadcasting event named '%s'"), *EventName);
	if (UPassageGlobals* Globals = GetPassageGlobals(Context))
	{
		if (Globals->Events.Contains(EventName))
		{
			Globals->Events[EventName].Broadcast();
		}
		else
		{
			UE_LOG(LogPassageGlobals, Warning,
				TEXT("UPassageGlobals::Event() Dropping broadcast of '%s' event because it has never been registered."),
				*EventName);
		}
	}
	else
	{
		UE_LOG(LogPassageGlobals, Error,
			TEXT("UPassageGlobals::Event() Called with invalid context, unable to get UPassageGlobals instance. Event '%s' will not be broadcast."),
			*EventName);
	}
}

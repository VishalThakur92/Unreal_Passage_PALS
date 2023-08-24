// Fill out your copyright notice in the Description page of Project Settings.


#include "PassagePixelStreamComponent.h"

#include "DirectoryProvider.h"
#include "PassageCharacter.h"
#include "PassageGlobals.h"
#include "PassagePlayerController.h"
#include "PixelStreamingInputComponent.h"
#include "PixelStreamingDelegates.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogPassagePixelStream);

// Sets default values for this component's properties
UPassagePixelStreamComponent::UPassagePixelStreamComponent()
{

}

UPassagePixelStreamComponent::~UPassagePixelStreamComponent()
{
}

const FString& UPassagePixelStreamComponent::GetAuthToken() const
{
	return AuthToken;
}

bool UPassagePixelStreamComponent::IsPixelStreamReady() const
{
	return AuthToken.Len() > 0;
}


// Called when the game starts
void UPassagePixelStreamComponent::BeginPlay()
{
	Super::BeginPlay();

	GEngine->Exec(GetWorld(), TEXT("PixelStreaming.WebRTC.DisableResolutionChange false"));

	const auto Controller = StaticCast<APassagePlayerController*>(GetOwner());
	PixelStreamingInput = Controller->PixelStreamingInput;

	// It's called InputEvent, but it comes from the JavaScript emitUIInteraction() function
	PixelStreamingInput->OnInputEvent.AddDynamic(this, &UPassagePixelStreamComponent::OnInputEvent);

	PixelStreamChannel = MakeShared<FPixelStreamChannel>(this);
	PixelStreamRpcHandler = MakeShared<FPixelStreamRpcHandler>(this);
	JsonRpc = MakeShared<FJsonRpc>(PixelStreamChannel, PixelStreamRpcHandler);

	const auto PixelStreamingDelegates = UPixelStreamingDelegates::GetPixelStreamingDelegates();
	PixelStreamingDelegates->OnClosedConnectionNative.AddUFunction(this, TEXT("RestartOnDisconnect"));

	if(UPassageGlobals* PassageGlobals = UPassageGlobals::GetPassageGlobals(this))
	{
		if(! PixelStreamingDelegates->OnNewConnectionNative.IsBoundToObject(PassageGlobals))
		{
			PixelStreamingDelegates->OnNewConnectionNative.AddUObject(
				PassageGlobals, &UPassageGlobals::BroadcastNewConnection);

			PixelStreamingDelegates->OnNewConnectionNative.AddUObject(
				PassageGlobals, &UPassageGlobals::StopAvailabilityLoop_DelegateHandler);

			PixelStreamingDelegates->OnConnectedToSignallingServer.AddDynamic(
				PassageGlobals, &UPassageGlobals::StartAvailabilityLoop);
		}
	}
}

// Since this in bound to a delegate, the compiler doesn't like it being const,
// so we ask ReSharper to shut up about it.
// ReSharper disable once CppMemberFunctionMayBeConst
void UPassagePixelStreamComponent::OnInputEvent(const FString& Descriptor)
{
	UE_LOG(LogPassagePixelStream, VeryVerbose,
		TEXT("UPassagePixelStreamComponent::OnInputEvent received descriptor: %s"),
		*Descriptor);

	const auto MsgReader = TJsonReaderFactory<>::Create(Descriptor);
	if (TSharedPtr<FJsonValue> MsgValue; FJsonSerializer::Deserialize(MsgReader, MsgValue))
	{
		auto const MsgObject = MsgValue->AsObject();

		if (FString Type;
			MsgObject->TryGetStringField("type", Type)
			&& Type == "PassageRPC")
		{
			if (FString Data; MsgObject->TryGetStringField("data", Data))
			{
				PixelStreamChannel->OnMessage.Broadcast(Data);
			}
			else
			{
				UE_LOG(LogPassagePixelStream, Verbose,
					TEXT("UPassagePixelStreamComponent::OnInputEvent no data field in descriptor: %s"),
					*Descriptor);
			}
		}
		else
		{
			// This is an expected condition for no
			UE_LOG(LogPassagePixelStream, VeryVerbose,
				TEXT("UPassagePixelStreamComponent::OnInputEvent ignoring descriptor with Type != 'PassageRPC' (expected for non-RPC messages): %s"),
				*Descriptor);
		}
	}
	else
	{
		UE_LOG(LogPassagePixelStream, Error,
			TEXT("UPassagePixelStreamComponent::OnInputEvent failed to parse descriptor: %s"),
			*Descriptor);
	}
}

void UPassagePixelStreamComponent::RestartOnDisconnect()
{
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			if(const auto PassageGlobals = UPassageGlobals::GetPassageGlobals(this))
			{
				PassageGlobals->StartAvailabilityLoop();
			}
			UPassageGlobals::BroadcastGlobalEvent(this, TEXT("RestartOnDisconnect"));
			UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
		});
}


// Despite ReSharper's advice, making this const breaks the AddDynamic macro
// that uses this as a delegate handler
// ReSharper disable once CppMemberFunctionMayBeConst
void UPassagePixelStreamComponent::SendAvailableFlagToBackend()
{
	UE_LOG(LogPassagePixelStream, Verbose, TEXT("UPassagePixelStreamComponent::SendAvailableFlagToBackend"));
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			if (const auto PassageGlobals = UPassageGlobals::GetPassageGlobals(this))
			{
				PassageGlobals->SendAvailableFlagToBackend();
			}
		});
}


void UPassagePixelStreamComponent::HandleInit(const FString& InAuthToken)
{
	UE_LOG(LogPassagePixelStream, Verbose,
		TEXT("UPassagePixelStreamComponent::HandleInit received AuthToken: %s"),
		*InAuthToken);

	if(const auto PassageGlobals = UPassageGlobals::GetPassageGlobals(this); PassageGlobals)
	{
		AuthToken = InAuthToken;
		PassageGlobals->DirectoryAuthToken = InAuthToken;
	}
	else
	{
		UE_LOG(LogPassagePixelStream, Error,
			TEXT("No UPassageGlobals instance available for storing DirectoryAuthToken, Init() will not broadcast to delegate"));
	}
	OnPixelStreamInit.Broadcast();
}

void UPassagePixelStreamComponent::HandleTeleport(const FString& ParticipantId)
{
	UE_LOG(LogPassagePixelStream, Verbose,
		TEXT("UPassagePixelStreamComponent::HandleTeleport received ParticipantId: %s"),
		*ParticipantId);

	const auto Controller = StaticCast<APassagePlayerController*>(GetOwner());
	Controller->PassageTeleport(ParticipantId);
}

void UPassagePixelStreamComponent::HandleSetInputMode(const FString& Mode, const bool IsTouch)
{
	const auto Controller = StaticCast<APassagePlayerController*>(GetOwner());
	const auto Character = StaticCast<APassageCharacter*>(Controller->GetPawn());
	if(Mode == TEXT("Pro"))
	{
		Character->SetInputMode(EPassageInputMode::Pro);
	} else
	{
		Character->SetInputMode(EPassageInputMode::Simple);
	}
}

void UPassagePixelStreamComponent::HandleSetResolution(const int32 Width, const int32 Height)
{
	const FString Command = FString::Printf(TEXT("r.SetRes %dx%d"), Width, Height);
	const auto World = GetWorld();
	Async(EAsyncExecution::TaskGraphMainThread, [Command, World]()
		{
			if (IsValid(GEngine) && IsValid(World))
			{
				GEngine->Exec(World, *Command);
			}
		});
}

void UPassagePixelStreamComponent::FPixelStreamChannel::Send(const FString& Message)
{
	if (IsValid(Parent) && IsValid(Parent->PixelStreamingInput))
	{
		const auto MsgObject = MakeShared<FJsonObject>();
		MsgObject->SetStringField("type", "PassageRPC");
		MsgObject->SetStringField("data", Message);

		FString Serialized;
		const auto Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(MsgObject, Writer);

		Parent->PixelStreamingInput->SendPixelStreamingResponse(Serialized);
	}
}

TSharedFuture<FJsonRpcResponse> UPassagePixelStreamComponent::FPixelStreamRpcHandler::Handle(
	FString Method, TSharedPtr<FJsonValue> Params)
{
	UE_LOG(LogPassagePixelStream, Verbose,
		TEXT("UPassagePixelStreamComponent::FPixelStreamRpcHandler::Handle received Method: %s"),
		*Method);

	if (IsValid(Parent)) {
		if (Method == TEXT("Init"))
		{
			if (const auto& ParamsArray = Params->AsArray(); ParamsArray.Num() == 1)
			{
				const auto ParamAuthToken = ParamsArray[0]->AsString();
				Parent->HandleInit(ParamAuthToken);
				return ImmediateResponse({ false, MakeShared<FJsonValueBoolean>(true), nullptr });
			}
			else
			{
				return ImmediateResponse(FJsonRpc::Error(InvalidParams,
					TEXT("UPassagePixelStreamComponent::FPixelStreamRpcHandler::Handle Expected one parameter (authorization token) with the Init() call.")));
			}
		}
		else if (Method == TEXT("Teleport"))
		{
			if (const auto& ParamsArray = Params->AsArray(); ParamsArray.Num() == 1)
			{
				const auto ParticipantId = ParamsArray[0]->AsString();
				Parent->HandleTeleport(ParticipantId);
				return ImmediateResponse({ false, MakeShared<FJsonValueBoolean>(true), nullptr });
			}
			else
			{
				return ImmediateResponse(FJsonRpc::Error(InvalidParams,
					TEXT("UPassagePixelStreamComponent::FPixelStreamRpcHandler::Handle Expected one parameter (participant ID) with the Teleport() call.")));
			}
		}
		else if (Method == TEXT("SetInputMode"))
		{
			if (const auto& ParamsArray = Params->AsArray(); ParamsArray.Num() == 2)
			{
				const auto InputMode = ParamsArray[0]->AsString();
				const auto IsTouch = ParamsArray[1]->AsBool();
				if (InputMode != TEXT("Pro") && InputMode != TEXT("Simple"))
				{
					return ImmediateResponse(FJsonRpc::Error(InvalidParams,
						TEXT("UPassagePixelStreamComponent::FPixelStreamRpcHandler::Handle SetInputMode() Expects first parameter (input mode) to be either 'Pro' or 'Simple'.")));
				}
				Parent->HandleSetInputMode(InputMode, IsTouch);
				return ImmediateResponse({ false, MakeShared<FJsonValueBoolean>(true), nullptr });
			}
			else
			{
				return ImmediateResponse(FJsonRpc::Error(InvalidParams,
					TEXT("UPassagePixelStreamComponent::FPixelStreamRpcHandler::Handle Expected two parameters (input mode, is touch) with the SetInputMode() call.")));
			}
		}
		else if (Method == TEXT("SetResolution"))
		{
			if (const auto& ParamsArray = Params->AsArray(); ParamsArray.Num() == 2)
			{
				const auto Width = StaticCast<int32>(ParamsArray[0]->AsNumber());
				const auto Height = StaticCast<int32>(ParamsArray[1]->AsNumber());
				Parent->HandleSetResolution(Width, Height);
				return ImmediateResponse({ false, MakeShared<FJsonValueBoolean>(true), nullptr });
			}
			else
			{
				return ImmediateResponse(FJsonRpc::Error(InvalidParams,
					TEXT("UPassagePixelStreamComponent::FPixelStreamRpcHandler::Handle Expected two parameters (width, height) with the SetResolution() call.")));
			}
		}
		else
		{
			return ImmediateResponse(FJsonRpc::Error(MethodNotFound,
				TEXT("UPassagePixelStreamComponent::FPixelStreamRpcHandler::Handle Method not found.")));
		}
	}
	else
	{
		return ImmediateResponse(FJsonRpc::Error(InternalError, TEXT("APassagePlayerController::FPixelStreamRpcHandler::Handle Invalid parent.")));
	}
}

TSharedFuture<FJsonRpcResponse> UPassagePixelStreamComponent::FPixelStreamRpcHandler::ImmediateResponse(
	const FJsonRpcResponse Value)
{
	TPromise<FJsonRpcResponse> Promise;
	Promise.SetValue(Value);
	return Promise.GetFuture().Share();
}
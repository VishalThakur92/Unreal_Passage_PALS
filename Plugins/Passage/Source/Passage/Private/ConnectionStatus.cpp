// Copyright Passage, 2022

#include "ConnectionStatus.h"

DEFINE_LOG_CATEGORY(LogConnection);

UConnectionStatus::UConnectionStatus() :
	Status(EConnectionStatus::Unconnected)
{ }

EConnectionStatus UConnectionStatus::GetStatus()
{ 
	return Status;
}

void UConnectionStatus::SetStatus(EConnectionStatus NewStatus)
{
	if (Status == NewStatus)
	{
		return;
	}

	if (NewStatus == EConnectionStatus::Unconnected) {
		UE_LOG(LogConnection, Warning,
			TEXT("Reset a UConnection to Unconnected. The Failed and "
				"Closed statuses are the intended terminal states."));
		return;
	}

	Status = NewStatus;

	switch (Status) {
		case EConnectionStatus::Connecting:
			OnConnecting.Broadcast();
			break;
		case EConnectionStatus::Connected:
			OnConnected.Broadcast();
			break;
		case EConnectionStatus::Reconnecting:
			OnReconnecting.Broadcast();
			break;
		case EConnectionStatus::Closed:
			OnClosed.Broadcast();
			break;
		case EConnectionStatus::Failed:
			OnFailed.Broadcast();
			break;
	}

	OnStatusChange.Broadcast(Status);
}
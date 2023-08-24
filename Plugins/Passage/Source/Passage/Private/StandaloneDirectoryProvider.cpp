// Copyright Enva Division, 2022

#include "StandaloneDirectoryProvider.h"

#include "ConnectionStatus.h"
#include "Participant.h"

UStandaloneDirectoryProvider::UStandaloneDirectoryProvider()
{
	Connection = NewObject<UConnectionStatus>();
	LocalParticipant = NewObject<UParticipant>();
	LocalParticipant->Id = RandomString(25);
	LocalParticipant->IsLocal = true;
	LocalParticipant->Active = true;
	LocalParticipant->ScreenName = TEXT("Fake Local Participant");
	LocalParticipant->ServerLocation = TEXT("127.0.0.1");

	AllParticipants.Add(LocalParticipant);
}

UParticipant* UStandaloneDirectoryProvider::GetLocalParticipant_Implementation()
{
	return LocalParticipant;
}

TArray< UParticipant* > UStandaloneDirectoryProvider::GetParticipantsOnServer_Implementation()
{
	return AllParticipants;
}

UParticipant* UStandaloneDirectoryProvider::AddFakeParticipant(const FString& RemoteId)
{
	UParticipant* Participant = NewObject<UParticipant>();
	Participant->Id = RemoteId;
	Participant->Active = true;
	Participant->ScreenName = FString::Printf(TEXT("Fake Participant (%s)"), *RemoteId);
	Participant->ServerLocation = TEXT("127.0.0.1");
	AllParticipants.Add(Participant);

	OnParticipantJoined.Broadcast(Participant);

	return Participant;
}

void UStandaloneDirectoryProvider::RemoveFakeParticipant(UParticipant* Participant)
{
	if (AllParticipants.Contains(Participant)) {
		AllParticipants.Remove(Participant);
		Participant->Active = false;
		OnParticipantLeft.Broadcast(Participant);
	}
}

FString UStandaloneDirectoryProvider::RandomString(const size_t Length)
{
	const FString Chars = TEXT("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	const size_t Range = Chars.Len();
	FString Buffer;
	Buffer.Reserve(Length);
	for (size_t i = 0; i < Length; ++i)
	{
		Buffer.AppendChar(Chars[FMath::RandHelper(Range)]);
	}
	return Buffer;
}

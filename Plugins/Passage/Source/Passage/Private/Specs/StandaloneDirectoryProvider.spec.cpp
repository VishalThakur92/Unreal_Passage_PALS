#include "StandaloneDirectoryProvider.h"
#include "StandaloneDirectoryProviderWrapper.h"

DEFINE_SPEC(FStandaloneDirectoryProviderSpec, "Passage.StandaloneDirectoryProvider",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

void FStandaloneDirectoryProviderSpec::Define()
{
	Describe("AddFakeParticipant()", [this]() {
		LatentIt("should broadcast event and add participant", 100, [this](const FDoneDelegate& Done) {
			const auto Directory = NewObject<UStandaloneDirectoryProvider>();

			const auto DoneWrapper = NewObject<UStandaloneDirectoryProviderWrapper>();
			DoneWrapper->Delegate = Done;

			const FString RemoteId(TEXT("Fake RemoteId"));

			Directory->OnParticipantJoined.AddDynamic(
				DoneWrapper, 
				&UStandaloneDirectoryProviderWrapper::ParticipantDone);
			const auto Participant = Directory->AddFakeParticipant(RemoteId);
			auto LocalParticipant = Directory->GetLocalParticipant();
			const auto AllParticipants = Directory->GetParticipantsOnServer();

			TestEqual("Has expected Id", Participant->Id, RemoteId);
			TestEqual("Correct Participant pointer", DoneWrapper->Participant, Participant);

			TestTrue("In server list", AllParticipants.Contains(Participant));
			});
		});

	Describe("RemoveFakeParticipant()", [this]() {
		// Totally unrelated to the matter at hand (except that delegates and
		// lambdas give me a headache). I'm leaving this here as a memorial
		// to the time that I realized this bananas line noise is valid
		// syntax:
		// TFunction<void()> X = []() -> void {};

		LatentIt("should broadcast event and remove participant", 100, [this](const FDoneDelegate& Done) {
			const auto Directory = NewObject<UStandaloneDirectoryProvider>();

			const auto DoneWrapper = NewObject<UStandaloneDirectoryProviderWrapper>();
			DoneWrapper->Delegate = Done;

			const FString RemoteId(TEXT("Fake RemoteId"));

			Directory->OnParticipantLeft.AddDynamic(
				DoneWrapper,
				&UStandaloneDirectoryProviderWrapper::ParticipantDone);
			const auto Participant = Directory->AddFakeParticipant(RemoteId);
			auto LocalParticipant = Directory->GetLocalParticipant();
			Directory->RemoveFakeParticipant(Participant);

			const auto AllParticipants = Directory->GetParticipantsOnServer();

			TestEqual("Correct Participant pointer", DoneWrapper->Participant, Participant);
			TestFalse("Not in server list", AllParticipants.Contains(Participant));
			});

		});
}
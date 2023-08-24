#include "Participant.h"
#include "StandaloneDirectoryProviderWrapper.generated.h"

UCLASS()
class UStandaloneDirectoryProviderWrapper : public UObject
{
	GENERATED_BODY()

public:
	public:

	FDoneDelegate Delegate;
	UParticipant* Participant;

	UFUNCTION()
	void Done() { Delegate.Execute(); }

	UFUNCTION()
	void ParticipantDone(UPARAM() UParticipant* ResultParticipant) {
		Participant = ResultParticipant;
		Delegate.Execute();
	}
};

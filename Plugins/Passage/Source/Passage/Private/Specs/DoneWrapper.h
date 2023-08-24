
#include "DoneWrapper.generated.h"

UCLASS()
class UDoneWrapper : public UObject
{
	GENERATED_BODY()
public:

	FDoneDelegate Delegate;

	UFUNCTION()
	void Done() { Delegate.Execute(); }

	UFUNCTION()
	void Done_UParticipant(UParticipant* Arbitrary) { Delegate.Execute(); }
};

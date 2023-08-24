#include "ConnectionStatusSpecWrapper.generated.h"

UCLASS()
class UConnectionStatusSpecWrapper : public UObject
{
	GENERATED_BODY()
public:

	FDoneDelegate Delegate;

	UFUNCTION()
	void Done() { Delegate.Execute(); }

	UFUNCTION()
	void StatusDone(UPARAM() EConnectionStatus Status) { Delegate.Execute(); }
};
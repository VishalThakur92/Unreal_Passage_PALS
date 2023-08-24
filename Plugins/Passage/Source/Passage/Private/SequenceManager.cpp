// Fill out your copyright notice in the Description page of Project Settings.


#include "SequenceManager.h"

DEFINE_LOG_CATEGORY(LogSequenceManager);

FSequenceStep::FSequenceStep(const int Order, const FString& Name,
	const FStartStepHandler Handler, const float Timeout)
{
	this->Order = Order;
	this->Name = Name;
	this->Handler = Handler;
	this->Timeout = Timeout;
}


void operator+= (FString& Base, const TSharedPtr<FSequenceStep>& Item)
{
	const auto Representation = FString::Printf(TEXT("{Name(%s) Order(%d) Bound(%s)}"),
		*Item->Name,
		Item->Order,
		(Item->Handler.IsBound() ? TEXT("true") : TEXT("false"))
	);
	Base.Append(*Representation);
}

bool operator< (const TSharedPtr<FSequenceStep>& Left, const TSharedPtr<FSequenceStep>& Right)
{
	return Left->Order < Right->Order;
}

void ASequenceManager::AddStep(const int Order, const FString& Name,
	const FStartStepHandler& Handler, const float Timeout)
{
	if (Name == "")
	{
		UE_LOG(LogSequenceManager, Error,
			TEXT("ASequenceManager::AddStep() Name cannot be the empty string (skipping)"));
	}
	else if (Names.Contains(Name)) {
		UE_LOG(LogSequenceManager, Error,
			TEXT("ASequenceManager::AddStep() Duplicate sequence step '%s' (skipping)"), *Name);
	}
	else {
		const auto Step = MakeShared<FSequenceStep>(Order, Name, Handler, Timeout);
		Queue.HeapPush(Step);
		Names.Add(Name);

		UE_LOG(LogSequenceManager, VeryVerbose,
			TEXT("ASequenceManager::AddStep() added step '%s', queue: %s"),
			*Name,
			*(FString::Join(Queue, TEXT(", "))));
	}
}

void ASequenceManager::Start()
{
	UE_LOG(LogSequenceManager, VeryVerbose,
		TEXT("ASequenceManager::Start() queue: %s"),
		*(FString::Join(Queue, TEXT(", "))));
	// If we have anything left in the queue, go ahead and execute the top one.
	if (Queue.Num() > 0) {
		TSharedPtr<FSequenceStep> Next;
		Queue.HeapPop(Next);
		Pending.Add(Next->Name);

		FTimerDelegate Delegate;
		Delegate.BindUFunction(this, "HandleTimeout", Next->Name);
		Timers.Add(Next->Name, {});
		GetWorld()->GetTimerManager().SetTimer(
			Timers[Next->Name], Delegate, Next->Timeout, false);

		// ReSharper disable once CppExpressionWithoutSideEffects
		Next->Handler.ExecuteIfBound();

		// Note the order of the one we executed
		const int Order = Next->Order;

		// If we have any more left, start going through them one by one.
		while (Queue.Num() > 0) {
			Next = Queue.HeapTop();

			// If we're at the same order level, go ahead and consume
			// this one, and then test the next one.
			if (Next->Order <= Order) {
				Queue.HeapPopDiscard();
				Pending.Add(Next->Name);
				// ReSharper disable once CppExpressionWithoutSideEffects
				Next->Handler.ExecuteIfBound();
			}
			else {
				// Otherwise we've run into different priorities, so let's
				// end it here.
				break;
			}
		}
	}
}

void ASequenceManager::Complete(const FString& Name, const bool Success)
{
	UE_LOG(LogSequenceManager, VeryVerbose,
		TEXT("ASequenceManager::Complete() Name(%s), Success(%s)"),
		*Name,
		*(Success ? TEXT("true") : TEXT("false")));

	if(!Names.Contains(Name))
	{
		UE_LOG(LogSequenceManager, Error,
			TEXT("ASequenceManager::Complete() Step named '%s' was not added to this instance"),
			*Name);
		return;
	}

	if(! Success)
	{
		UE_LOG(LogSequenceManager, Error,
			TEXT("ASequenceManager::Complete() The step named '%s' failed"),
			*Name);
	}

	if (Pending.Contains(Name)) {
		Pending.Remove(Name);
		Statuses.Add(Name, Success);
		OnComplete.Broadcast(Name, Success);
	}
	else {
		UE_LOG(LogSequenceManager, Error, TEXT("ASequenceManager::Complete() No pending step with name '%s' (completed twice?)"), *Name);
	}

	if (Timers.Contains(Name))
	{
		GetWorld()->GetTimerManager().ClearTimer(Timers[Name]);
		Timers.Remove(Name);
	}
	else
	{
		// This may indicate a race condition in which completion is achieved
		// in the same frame that the timeout expires, but the timeout handler
		// runs first. If this is seen frequently, extending the timeout should
		// make it less common.
		UE_LOG(LogSequenceManager, Error,
			TEXT("ASequenceManager::Complete() No timer for step named '%s' (completed twice?)"),
			*Name);
	}

	// There *may* be a callback for the item. Since we're completing now, we can
	// toss these out; they shall not be used again.
	if (SuccessCallbacks.Contains(Name))
	{
		SuccessCallbacks.Remove(Name);
	}
	if (FailureCallbacks.Contains(Name))
	{
		FailureCallbacks.Remove(Name);
	}

	// If we've completed everything at this priority level, then go to the
	// next priority level
	if (Pending.Num() == 0)
	{
		Start();
	}
}

void ASequenceManager::CompletionCallbacks(
	const FString& Name, UDelegateCallback*& SuccessCallback, UDelegateCallback*& FailureCallback)
{
	if(SuccessCallbacks.Contains((Name)))
	{
		UE_LOG(LogSequenceManager, Error,
			TEXT("Duplicate SuccessCallback for step named '%s'"), *Name);
		return;
	}
	// We're unlikely to see this one since they're added in pairs, but let's do it anyway.
	if (FailureCallbacks.Contains((Name)))
	{
		UE_LOG(LogSequenceManager, Error,
			TEXT("Duplicate FailureCallback for step named '%s'"), *Name);
		return;
	}

	SuccessCallback = NewObject<UDelegateCallback>();
	SuccessCallback->OnDone.BindLambda([this, Name]()
		{
			Success(Name);
		});
	SuccessCallbacks.Add(Name, SuccessCallback);

	FailureCallback = NewObject<UDelegateCallback>();
	FailureCallback->OnDone.BindLambda([this, Name]()
		{
			Failure(Name);
		});
	FailureCallbacks.Add(Name, FailureCallback);
}

void ASequenceManager::Success(const FString& Name)
{
	Complete(Name, true);
}

void ASequenceManager::Failure(const FString& Name)
{
	Complete(Name, false);
}

bool ASequenceManager::Succeeded(const FString& Name)
{
	TArray<FString> Stats;
	Statuses.GetKeys(Stats);
	UE_LOG(LogSequenceManager, VeryVerbose,
		TEXT("ASequenceManager::Succeeded() Name(%s) Statuses: "),
		*Name,
		*(FString::Join(Stats, TEXT(", "))));

	if (Statuses.Contains(Name)) {
		return Statuses[Name];
	}

	if(Pending.Contains(Name))
	{
		UE_LOG(LogSequenceManager, Error,
			TEXT("ASequenceManager::Succeeded() The step named '%s' is still pending."),
			*Name);
	} else if(Names.Contains(Name))
	{
		UE_LOG(LogSequenceManager, Error,
			TEXT("ASequenceManager::Succeeded() The step named '%s' has probably not started yet."),
			*Name);
	} else
	{
		UE_LOG(LogSequenceManager, Error, TEXT("Cannot find a step named '%s'"), *Name);
	}
	return false;
}

void ASequenceManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	auto& Manager = GetWorld()->GetTimerManager();
	for(auto Item : Timers)
	{
		Manager.ClearTimer(Item.Value);
	}
	Timers.Empty();
}

void ASequenceManager::HandleTimeout(const FString& Name)
{
	UE_LOG(LogSequenceManager, Error,
		TEXT("ASequenceManager::HandleTimeout() Reached timeout interval for step named '%s'"),
		*Name);
	Failure(Name);
}

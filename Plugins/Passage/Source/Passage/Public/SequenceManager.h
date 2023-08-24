// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "DelegateCallback.h"
#include "SequenceManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSequenceManager, Log, All);

/**
 * This is a sort of placeholder type for any no-argument delegate that the
 * Blueprint subclass of ASequenceManager can pass to AddStep.
 */
DECLARE_DYNAMIC_DELEGATE(FStartStepHandler);


/**
 * This delegate is used as the OnComplete member of the sequence manager.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStepCompleted, FString, Name, bool, Success);


/**
 * @brief The sequence step is a tiny container class that's little more that a
 * struct.
 */
class PASSAGE_API FSequenceStep
{
public:
	/**
	 * @brief A default constructor, dumb as can be.
	 * @param Order @see AddStep#Order
	 * @param Name @see AddStep#Name
	 * @param Handler @see AddStep#Handler
	 * @param Timeout @see AddStep#Timeout
	 */
	FSequenceStep(const int Order, const FString& Name,
	              const FStartStepHandler Handler, const float Timeout);

	int Order;
	FString Name;
	FStartStepHandler Handler;
	float Timeout;
};


/**
 * The sequence manager is an attempt to bulletproof startup and initialization
 * processes. The intent is for the user to subclass the ASequenceManager class
 * in Blueprint and to add steps on the BeginPlay event, then call Start. Each
 * step's handler is called when its spot in the sequence has completed. If a
 * later step depends on an earlier step, the success of the earlier step can
 * be queried with the Succeeded method. The sequence will not stop when a step
 * fails. Subsequent steps that depend on earlier steps should fall back to the
 * best, most sensible defaults that preserve the most interactivity possible.
 */
UCLASS(Blueprintable)
class PASSAGE_API ASequenceManager : public AInfo
{
	GENERATED_BODY()

public:
	/**
	 * @brief Adds an event or function to be executed in the sequence order.
     * @param Order The order in which the added step should be executed. Note
     * that you can leave gaps in the order, and the order in which the steps
     * are added does not matter, only the order number given here. Two or more
     * steps with the same Order will run in parallel.
     * @param Name This string is used to key operations on this step, meaning
     * that when you finish the step, the handler signals success or failure by
     * calling the Success or Failure method with the name of the step.
     * @param Handler In practice this will be a Blueprint event or function
     * that you tie into the AddStep node. The handler will be called when the
     * numeric Order of the step has been reached. The handler should indicate
     * it is finished by calling Success or Failure with the step name.
     * @param Timeout Specify a duration in Seconds after which the step will
     * automatically fail. The default is 5.0 seconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Passage")
		void AddStep(const int Order, const FString& Name,
			const FStartStepHandler& Handler, const float Timeout = 5.0);

	/**
     * @brief Starting the SequenceManager causes the step(s) with the lowest
     * Order to be executed. When they have all completed, then the set of
     * steps with the next numerically greater Order are executed. This
     * continues until the sequence has no steps remaining.
	 */
	UFUNCTION(BlueprintCallable, Category = "Passage")
		void Start();

	/**
     * @brief Used by a handler to indicate that the step has been completed,
     * either in success or in failure. Complete(Name, true) is equivalent to
     * calling Success(Name), and Complete(Name, false) is equivalent to
     * Failure(Name).
     * @param Name The name of the step that has finished, i.e. the Name
     * parameter passed to AddStep.
	 * @param Success True indicates Success and false indicates Failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Passage")
		void Complete(const FString& Name, bool Success);

	/**
	 * @brief The completion callback is a generic way to pass a callback from
	 * C++ to a Blueprint and expect the Blueprint to signal that it has
	 * finished some asynchronous task.
	 * @param Name The name of the step previously added by AddStep, that will
	 * be completed when these callbacks are called.
	 * @param SuccessCallback The callback to call when the process has
	 * succeeded. Calling the Done method of this callback is equivalent to
	 * calling Succeeded(Name) on this object.
	 * @param FailureCallback The callback to call when the process has failed.
	 * Calling the Done method of this callback is equivalent to calling
	 * Failure(Name) on this object.
	 */
	UFUNCTION(BlueprintCallable)
		void CompletionCallbacks(
			const FString& Name, UDelegateCallback*& SuccessCallback, UDelegateCallback*& FailureCallback);

	/**
	 * @brief Used by a handler to indicate that the named step succeeded. This
	 * is equivalent to calling Complete(Name, true).
	 * @param Name The name of the step passed to AddStep earlier.
	 */
	UFUNCTION(BlueprintCallable, Category = "Passage")
		void Success(const FString& Name);

	/**
	 * @brief Used by a handler to indicate that the named step failed. This is
	 * equivalent to calling Complete(Name, false).
	 * @param Name The name of the step passed to AddStep earlier.
	 */
	UFUNCTION(BlueprintCallable, Category = "Passage")
		void Failure(const FString& Name);

	/**
     * @brief Check if a previous step succeeded or not. An error will be
     * logged if the previous step never ran, is still running, or wasn't added
     * to the sequence manager.
	 * @param Name The name of the step passed to AddStep earlier.
	 * @return True if the step ended with success, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Passage")
		bool Succeeded(const FString& Name);

    /**
     * This delegate is executed every time a step completes. The delegate will
     * receive the step's Name and a boolean indicating if it succeeded. This
     * is intended to help build debugging or display tools for a startup
     * process.
     */
	UPROPERTY(BlueprintAssignable)
	FStepCompleted OnComplete;

	/**
	 * Clears any timers when the game ends so they don't overrun the lifetime
	 * of referenced objects.
	 */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// A priority queue (heap) of steps sorted by order. We pop steps off the
	// heap as they are completed, so the top of the heap should always be the
	// next step.
	TArray< TSharedPtr<FSequenceStep> > Queue;

	// A record of all the step names added to this manager regardless of
	// state or status.
	TSet<FString> Names;

	// The names of the steps we are currently awaiting. When this set in empty
	// we look for more steps with a higher order
	TSet<FString> Pending;

	// A record of the outcomes. Since we discard the steps from the heap we
	// record the outcome here.
	TMap<FString, bool> Statuses;

	// All of the timers for this
	TMap<FString, FTimerHandle> Timers;

	// These little completion objects have a method that the dynamic delegate
	// can call. The UPROPERTY specifier is required so the callbacks aren't
	// garbage collected prematurely
	UPROPERTY()
	TMap<FString, UDelegateCallback*> SuccessCallbacks;

	UPROPERTY()
	TMap<FString, UDelegateCallback*> FailureCallbacks;

	/**
	 * @brief This handles timeouts when the timeout has not already been
	 * cleared by calling Complete, Success, or Failure.
	 * @param Name The name of the step passed to AddStep earlier.
	 */
	UFUNCTION()
	void HandleTimeout(const FString& Name);
};

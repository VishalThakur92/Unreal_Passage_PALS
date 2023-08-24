// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DelegateCallback.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDelegateCallback, Log, All);

/**
 *
 */
DECLARE_DELEGATE(FDelegateCallback);


/**
 * This utility class allows C++ classes to await actions taken by Blueprint
 * scripts without tightly coupling the Blueprint to themselves. The Blueprint
 * should implement a function or custom event that accepts a UDelegateCallback
 * instance, stores that object for the duration of the activity, then calls
 * the Done method and discards the object.
 *
 * This class addresses an apparently un-covered Blueprint/C++ interaction:
 * returning a bound delegate from C++ that is later called by Blueprint.
 * Furthermore, this allows the C++ side to deal with a plain delegate while
 * the blueprint side deals with a plain UObject.
 */
UCLASS(BlueprintType)
class UDelegateCallback : public UObject
{
	GENERATED_BODY()
	
public:
	/**
	 * @brief The receiving Blueprint should call this method when it has
	 * completed its task, notifying the C++ implementation that it can
	 * continue whatever it is doing.
	 */
	UFUNCTION(BlueprintCallable)
	void Done() const;

	/**
	 * The C++ implementation should bind something to the OnDone delegate so
	 * that it can respond when the Done method is called.
	 */
	FDelegateCallback OnDone;

};

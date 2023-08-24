// Fill out your copyright notice in the Description page of Project Settings.

#include "DelegateCallback.h"

DEFINE_LOG_CATEGORY(LogDelegateCallback);

void UDelegateCallback::Done() const
{
	if(OnDone.IsBound())
	{
		OnDone.Execute();
	}
	else
	{
		UE_LOG(LogDelegateCallback, Error,
			TEXT("UDelegateCallback::Done() OnDone delegate is not bound"));
	}
	
}

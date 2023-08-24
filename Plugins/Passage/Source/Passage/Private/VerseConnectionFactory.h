// Copyright Enva Division

#pragma once

#include "CoreMinimal.h"

#include "VerseConnection.h"

#include "VerseConnectionFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVerseConnectionFactory, Log, All);

UCLASS(BlueprintType)
class PASSAGE_API UVerseConnectionFactory:
    public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Passage")
    UVerseConnection* CreateConnection();

private:

    // TUniquePtr<rtc::Thread> SignalingThread;

};

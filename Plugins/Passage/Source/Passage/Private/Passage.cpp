// Copyright Enva Division

#include "Passage.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "FPassageModule"

DEFINE_LOG_CATEGORY(LogPassage);


void FPassageModule::StartupModule()
{
	UE_LOG(LogPassage, Verbose, TEXT("FPassageModule::StartupModule()"));
}

void FPassageModule::ShutdownModule()
{
	UE_LOG(LogPassage, Verbose, TEXT("FPassageModule::StartupModule()"));
}


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPassageModule, Passage)

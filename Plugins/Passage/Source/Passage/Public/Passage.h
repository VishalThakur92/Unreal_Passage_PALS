// Copyright Enva Division

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "PassageGlobals.h"


DECLARE_LOG_CATEGORY_EXTERN(LogPassage, Log, All);


class FPassageModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};


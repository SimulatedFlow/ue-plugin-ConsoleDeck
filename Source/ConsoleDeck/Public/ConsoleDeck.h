// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ConsoleDeck - a cheat and debug menu that builds itself from the functions your project already has.
 *
 * The module itself does almost nothing: the catalog lives in UConsoleDeckSubsystem, the drawing and the
 * input in AConsoleDeckHUD, and the console commands register themselves statically.
 */
class FConsoleDeckModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};

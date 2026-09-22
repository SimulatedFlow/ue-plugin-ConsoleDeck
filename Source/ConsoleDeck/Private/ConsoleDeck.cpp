// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ConsoleDeck.h"
#include "ConsoleDeckLog.h"

DEFINE_LOG_CATEGORY(LogConsoleDeck);

#define LOCTEXT_NAMESPACE "FConsoleDeckModule"

void FConsoleDeckModule::StartupModule()
{
	UE_LOG(LogConsoleDeck, Log, TEXT("ConsoleDeck started."));
}

void FConsoleDeckModule::ShutdownModule()
{
	UE_LOG(LogConsoleDeck, Log, TEXT("ConsoleDeck shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConsoleDeckModule, ConsoleDeck)

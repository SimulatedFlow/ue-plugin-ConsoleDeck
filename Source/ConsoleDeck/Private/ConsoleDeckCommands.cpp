// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ConsoleDeckEntry.h"
#include "ConsoleDeckLog.h"
#include "ConsoleDeckSettings.h"
#include "ConsoleDeckSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

/**
 * The console surface.
 *
 * ConsoleDeck does not replace the console, and these commands are the proof: everything the menu does is
 * also reachable by typing, for whoever has a keyboard and prefers one. The menu exists for the case where
 * nobody does.
 *
 * None of these is editor-only. They work in play-in-editor and in a packaged build in the same way, which
 * is what makes a bug report reproducible from a line somebody can paste.
 */
namespace ConsoleDeckCommands
{
	static UConsoleDeckSubsystem* FindDeck(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UConsoleDeckSubsystem* Deck = GameInstance ? GameInstance->GetSubsystem<UConsoleDeckSubsystem>() : nullptr;

		if (!Deck)
		{
			UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: no deck here - these commands need a running game."));
		}

		return Deck;
	}

	static FAutoConsoleCommandWithWorldAndArgs GOpen(
		TEXT("ConsoleDeck.Open"),
		TEXT("ConsoleDeck.Open - open the menu. Refused when the deck is locked."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UConsoleDeckSubsystem* Deck = FindDeck(World))
			{
				Deck->Open();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GClose(
		TEXT("ConsoleDeck.Close"),
		TEXT("ConsoleDeck.Close - close the menu."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UConsoleDeckSubsystem* Deck = FindDeck(World))
			{
				Deck->Close();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GRebuild(
		TEXT("ConsoleDeck.Rebuild"),
		TEXT("ConsoleDeck.Rebuild - walk the classes again. For the function you added since the game started."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UConsoleDeckSubsystem* Deck = FindDeck(World);
			if (!Deck)
			{
				return;
			}

			Deck->Rebuild();

			const FConsoleDeckStats& Stats = Deck->GetStats();
			UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck: %d entry(s) in %d section(s), %d unavailable, scan %.2f ms."),
				Stats.Entries, Stats.Sections, Stats.Unavailable, Stats.ScanMilliseconds);
		}));

	static FAutoConsoleCommandWithWorldAndArgs GUnlock(
		TEXT("ConsoleDeck.Unlock"),
		TEXT("ConsoleDeck.Unlock <word> - unlock the deck in a build where it is shut."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UConsoleDeckSubsystem* Deck = FindDeck(World);
			if (!Deck)
			{
				return;
			}

			if (Args.Num() == 0)
			{
				UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck.Unlock <word>"));
				return;
			}

			Deck->TryUnlock(Args[0]);
		}));

	static FAutoConsoleCommandWithWorldAndArgs GLock(
		TEXT("ConsoleDeck.Lock"),
		TEXT("ConsoleDeck.Lock - shut the deck again for this session."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UConsoleDeckSubsystem* Deck = FindDeck(World))
			{
				Deck->Lock();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GList(
		TEXT("ConsoleDeck.List"),
		TEXT("ConsoleDeck.List - write the whole catalog to the log, greyed-out reasons included."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UConsoleDeckSubsystem* Deck = FindDeck(World))
			{
				Deck->RefreshAvailability();
				Deck->DumpToLog();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GStats(
		TEXT("ConsoleDeck.Stats"),
		TEXT("ConsoleDeck.Stats - what the catalog cost and what it found."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UConsoleDeckSubsystem* Deck = FindDeck(World);
			if (!Deck)
			{
				return;
			}

			const FConsoleDeckStats& Stats = Deck->GetStats();
			UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck: %d entry(s), %d section(s), %d unavailable."),
				Stats.Entries, Stats.Sections, Stats.Unavailable);
			UE_LOG(LogConsoleDeck, Display, TEXT("  scan %.2f ms over %d class(es), %d marked function(s), source: %s."),
				Stats.ScanMilliseconds, Stats.ClassesScanned, Stats.FunctionsMarked,
				Stats.bFromBakedCatalog ? TEXT("baked catalog") : TEXT("live metadata"));
			UE_LOG(LogConsoleDeck, Display, TEXT("  baked catalog holds %d entry(s). Deck is %s."),
				Stats.BakedEntries, Deck->IsUnlocked() ? TEXT("unlocked") : TEXT("locked"));
		}));

	static FAutoConsoleCommandWithWorldAndArgs GInvoke(
		TEXT("ConsoleDeck.Invoke"),
		TEXT("ConsoleDeck.Invoke <Section|Name> [args] - call an entry. Wrong arguments are refused, not guessed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UConsoleDeckSubsystem* Deck = FindDeck(World);
			if (!Deck)
			{
				return;
			}

			if (Args.Num() == 0)
			{
				UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck.Invoke <Section|Name> [args]"));
				return;
			}

			TArray<FString> Rest(Args);
			const FString Path = Rest[0];
			Rest.RemoveAt(0);

			FString Result;
			const bool bOk = Deck->InvokeByPath(Path, Rest, Result);

			UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck: %s -> %s"), *Path, *Result);
			if (!bOk)
			{
				UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: %s was not called."), *Path);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GSet(
		TEXT("ConsoleDeck.Set"),
		TEXT("ConsoleDeck.Set <text> - put text into the focused parameter. For a string on a machine with a keyboard."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UConsoleDeckSubsystem* Deck = FindDeck(World);
			if (!Deck)
			{
				return;
			}

			// Joined back together with spaces: a marker called "boss room two" arrives as three arguments
			// and has to reach the parameter as one sentence.
			const FString Text = FString::Join(Args, TEXT(" "));

			FString Error;
			if (!Deck->SetFocusedParamText(Text, Error))
			{
				UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: %s"), *Error);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GBake(
		TEXT("ConsoleDeck.Bake"),
		TEXT("ConsoleDeck.Bake - write the catalog into DefaultGame.ini so it survives a cook. Editor only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UConsoleDeckSubsystem* Deck = FindDeck(World);
			if (!Deck)
			{
				return;
			}

			const int32 Count = Deck->BakeCatalog();
			if (Count >= 0)
			{
				UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck: the baked catalog holds %d entry(s)."), Count);
			}
		}));
}

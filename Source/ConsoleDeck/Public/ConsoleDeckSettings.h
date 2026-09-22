// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "ConsoleDeckTypes.h"
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ConsoleDeckSettings.generated.h"

/**
 * Project-wide settings for the deck.
 *
 * Project Settings > Plugins > ConsoleDeck, stored in DefaultGame.ini. Read once when the subsystem comes
 * up; the keys and the panel geometry can be moved afterwards through the console without touching config.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ConsoleDeck"))
class CONSOLEDECK_API UConsoleDeckSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UConsoleDeckSettings();

	//~ Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings interface

	/** Convenience accessor. Never returns null. */
	static const UConsoleDeckSettings& Get();

	// --------------------------------------------------------------------------------------------------
	// Opening
	// --------------------------------------------------------------------------------------------------

	/**
	 * Any of these opens the deck.
	 *
	 * Two by default, and deliberately not one: F8 for whoever is sitting at the machine, and the View /
	 * Back button for whoever is holding the pad. The pad entry is the one that matters - a tester on a
	 * couch with a packaged build has no keyboard, and that is the case the console cannot serve at all.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Opening")
	TArray<FKey> OpenKeys;

	/** Pressing the open key again closes the deck. Off means only Back / B / Escape closes it. */
	UPROPERTY(Config, EditAnywhere, Category = "Opening")
	bool bOpenKeyAlsoCloses = true;

	/**
	 * Pause the game while the deck is open.
	 *
	 * Off by default. Half of what a cheat menu is for is watching what a value does while the game runs;
	 * the other half is fiddling with a value while nothing is coming at you. Your project, your choice.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Opening")
	bool bPauseGameWhileOpen = false;

	/**
	 * Swallow move and look input while the deck is open.
	 *
	 * This is SetIgnoreMoveInput / SetIgnoreLookInput and nothing more, so it is honest about its limits:
	 * your own action bindings still fire. If a face button does something in your game as well as in the
	 * deck, gate it on UConsoleDeckStatics::IsDeckOpen - there is no way for a plugin to guess which of
	 * your bindings should lose.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Opening")
	bool bBlockMoveAndLookWhileOpen = true;

	// --------------------------------------------------------------------------------------------------
	// Navigation
	// --------------------------------------------------------------------------------------------------

	/** Previous entry. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> UpKeys;

	/** Next entry. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> DownKeys;

	/** Value down. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> LeftKeys;

	/** Value up. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> RightKeys;

	/** Previous section. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> PrevSectionKeys;

	/** Next section. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> NextSectionKeys;

	/** Run the focused entry. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> ActivateKeys;

	/** Close the deck, or leave the text field first. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> CloseKeys;

	/** Rebuild the catalog without leaving the menu. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> RebuildKeys;

	/** Put the focused entry's parameters back to their defaults. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FKey> ResetKeys;

	/**
	 * How long a direction has to be held before it starts repeating, in seconds.
	 *
	 * Without this a d-pad is unusable for numbers: one press moves one step, and nobody is going to press
	 * a button two hundred times to get a speed from 600 to 1400.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float RepeatDelaySeconds = 0.4f;

	/** Seconds between two repeats once repeating has started. */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RepeatRateSeconds = 0.07f;

	/**
	 * Multiplier on the step size while a direction is repeating.
	 *
	 * One means a held direction moves at the same rate as tapping. Above one, holding accelerates, which
	 * is what makes a range of 0 to 10000 crossable without letting go of the pad.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float RepeatStepMultiplier = 4.0f;

	// --------------------------------------------------------------------------------------------------
	// Discovery
	// --------------------------------------------------------------------------------------------------

	/**
	 * Walk every loaded UClass looking for the ConsoleDeck metadata key.
	 *
	 * This is the setting that makes the plugin work with no configuration at all, and it is also the one
	 * that costs. TObjectIterator<UClass> over a large project walks tens of thousands of classes and their
	 * functions; on this machine that is single-digit milliseconds, on a project with ten thousand
	 * Blueprints loaded it is more. It runs once, at startup, never on opening the menu, and the cost is
	 * printed in the header - if the number bothers you, narrow it with ScanPathFilters below.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Discovery")
	bool bScanAllLoadedClasses = true;

	/**
	 * Only scan classes whose package path starts with one of these. Empty means no filter.
	 *
	 * "/Script/MyGame" and "/Game/" between them cut the walk down to your own code and your own content,
	 * which is where your marked functions are. The engine's own classes carry no ConsoleDeck key.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Discovery")
	TArray<FString> ScanPathFilters;

	/**
	 * Extra classes to search, loaded if they are not in memory yet.
	 *
	 * This is how a subsystem, a GameMode or a manager that has not been touched yet still shows up. A
	 * class nobody has loaded has no UClass, and a scan cannot find what does not exist - so name it here
	 * and the deck loads it before it looks.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Discovery", meta = (MetaClass = "/Script/CoreUObject.Object"))
	TArray<FSoftClassPath> ScanClasses;

	/** Build the catalog when the game instance comes up, rather than waiting for the first open. */
	UPROPERTY(Config, EditAnywhere, Category = "Discovery")
	bool bBuildCatalogOnStartup = true;

	/** Include functions marked on Blueprint-generated classes, not just native ones. */
	UPROPERTY(Config, EditAnywhere, Category = "Discovery")
	bool bIncludeBlueprintClasses = true;

	// --------------------------------------------------------------------------------------------------
	// Baked catalog
	// --------------------------------------------------------------------------------------------------

	/**
	 * In the editor, write what the scan found into this config file after every rebuild.
	 *
	 * Metadata does not survive a cook. Without a baked catalog the deck in a packaged build is empty, and
	 * an empty menu is worse than no menu. With it, the last catalog the editor saw ships in DefaultGame.ini
	 * and the packaged build resolves each line back to a real UFunction by name.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Baked Catalog")
	bool bAutoBakeInEditor = true;

	/**
	 * The baked catalog itself. Written by ConsoleDeck.Bake or by the editor; read in cooked builds.
	 *
	 * Checking this into source control is the point: it is the difference between "the menu works on my
	 * machine" and "the menu works on the build the testers were given".
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Baked Catalog")
	TArray<FConsoleDeckBakedEntry> BakedCatalog;

	// --------------------------------------------------------------------------------------------------
	// Shipping lock
	// --------------------------------------------------------------------------------------------------

	/**
	 * The word ConsoleDeck.Unlock has to be given in a Shipping build.
	 *
	 * Empty in a Shipping build means the deck cannot be unlocked at all: no word, no key, closed. That is
	 * deliberate - the failure mode of a forgotten setting has to be the safe one.
	 *
	 * This is a lock on a door, not a safe. The word sits in a packaged ini file and anybody willing to
	 * open it will find it. It stops a player stumbling into the menu; it does not stop somebody who is
	 * looking for it. If your cheats must be unreachable in a public build, do not ship this plugin in that
	 * build - that is what the plugin's PlatformAllowList and your target rules are for. The documentation
	 * says this in the same words.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Shipping Lock")
	FString UnlockWord;

	/**
	 * Allow the deck to open in a Shipping build without the unlock word.
	 *
	 * For an internal build that goes to testers and nowhere else. Off by default.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Shipping Lock")
	bool bAllowInShippingWithoutUnlock = false;

	/**
	 * Require the unlock word in Development and Debug builds too.
	 *
	 * Off by default - in a development build the deck is open, because that is the build you are debugging
	 * in and one more hurdle there helps nobody.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Shipping Lock")
	bool bRequireUnlockOutsideShipping = false;

	/** Write a line to the log every time an entry is called. */
	UPROPERTY(Config, EditAnywhere, Category = "Shipping Lock")
	bool bLogInvocations = true;

	// --------------------------------------------------------------------------------------------------
	// Panel
	// --------------------------------------------------------------------------------------------------

	/** Top left corner of the panel, in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Panel", meta = (ClampMin = "0.0"))
	FVector2D PanelPosition = FVector2D(48.0f, 60.0f);

	/** Panel width in pixels, before the scale below. */
	UPROPERTY(Config, EditAnywhere, Category = "Panel", meta = (ClampMin = "240.0", ClampMax = "2000.0"))
	float PanelWidth = 640.0f;

	/**
	 * Everything the deck draws is multiplied by this.
	 *
	 * The engine's small font is 9 pixels tall. That is fine on a monitor and unreadable on a television
	 * across a room, which is exactly where a pad-driven menu gets used - so this exists and defaults to
	 * something bigger than one.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Panel", meta = (ClampMin = "0.5", ClampMax = "6.0"))
	float PanelScale = 1.5f;

	/** How many entries are visible at once before the list scrolls. */
	UPROPERTY(Config, EditAnywhere, Category = "Panel", meta = (ClampMin = "3", ClampMax = "40"))
	int32 VisibleRows = 12;

	/** Draw the header with entry count, section count and scan time. */
	UPROPERTY(Config, EditAnywhere, Category = "Panel")
	bool bShowStatsHeader = true;

	/** Draw the footer with the current key bindings. */
	UPROPERTY(Config, EditAnywhere, Category = "Panel")
	bool bShowKeyHintFooter = true;
};

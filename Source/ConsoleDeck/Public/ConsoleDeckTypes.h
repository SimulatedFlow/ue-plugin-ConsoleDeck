// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConsoleDeckTypes.generated.h"

/**
 * What a parameter is, from the menu's point of view.
 *
 * The engine has far more property types than this. The deck deliberately supports only the ones that can
 * be edited with a d-pad and four buttons; anything else makes the entry Unsupported and says so, which is
 * more useful than a control that pretends to edit an FVector and silently sends zeroes.
 */
UENUM(BlueprintType)
enum class EConsoleDeckParamKind : uint8
{
	/** bool - a switch. */
	Bool,

	/** int32 / int64 / uint8 without an enum - a whole number with optional limits. */
	Int,

	/** float / double - a number with a step derived from its limits. */
	Float,

	/** FString - typed on a keyboard, or picked from the presets in ConsoleDeckArgs on a pad. */
	String,

	/** FName - same as String, converted on the way in. */
	Name,

	/** FText - same as String. Not localised; a debug menu is not shipped to players. */
	Text,

	/** enum class / TEnumAsByte - a choice, cycled with left and right. */
	Enum,

	/** Everything else. The entry stays in the list, greyed out, with the type name in the reason. */
	Unsupported
};

/**
 * Why an entry can or cannot be used right now.
 *
 * An entry is never dropped from the catalog because its target is missing. A button that is not there
 * looks like a bug in the plugin; a button that is there and says "no GameMode of this class in this
 * world" tells you something true about the level you are standing in.
 */
UENUM(BlueprintType)
enum class EConsoleDeckAvailability : uint8
{
	/** A target instance was found and the function can be called. */
	Ready,

	/** The class exists but there is no instance of it in this world. */
	NoInstance,

	/** No world, no game instance, no player controller - the deck was asked too early. */
	NoWorld,

	/** The signature has a parameter the deck cannot edit. */
	UnsupportedSignature,

	/** The function or its class could not be resolved from the baked catalog. */
	NotFound
};

/**
 * One parameter of one entry, with everything needed to draw it and to check what was typed into it.
 *
 * Values are carried as text on purpose. Text is what a console command hands over, what a config file
 * stores and what an automation test can compare; the conversion into the packed parameter buffer happens
 * once, at the call, and it is the only place that has to know about FProperty.
 */
USTRUCT(BlueprintType)
struct CONSOLEDECK_API FConsoleDeckParam
{
	GENERATED_BODY()

	/** The parameter name from the C++ or Blueprint signature. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FName Name;

	/** What kind of control this gets. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	EConsoleDeckParamKind Kind = EConsoleDeckParamKind::Unsupported;

	/** The current value, as text. This is what the menu shows and what CoerceArgs writes. */
	UPROPERTY(BlueprintReadWrite, Category = "ConsoleDeck")
	FString Value;

	/** The value the entry starts with and the one Reset goes back to. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString DefaultValue;

	/** True when a lower limit was found in ClampMin or in ConsoleDeckArgs. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	bool bHasMin = false;

	/** True when an upper limit was found in ClampMax or in ConsoleDeckArgs. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	bool bHasMax = false;

	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	double Min = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	double Max = 0.0;

	/**
	 * How far one press of left or right moves the value.
	 *
	 * Zero means "work it out": one for whole numbers, a twentieth of the range for a number with limits,
	 * and a tenth of the current magnitude for one without - so an unbounded float is still usable on a pad
	 * without asking anybody to configure anything.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	double Step = 0.0;

	/** Enum entries, or the presets written into ConsoleDeckArgs for a string. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	TArray<FString> Options;

	/** The C++ type, for the greyed-out reason line. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString TypeName;

	bool IsNumeric() const
	{
		return Kind == EConsoleDeckParamKind::Int || Kind == EConsoleDeckParamKind::Float;
	}
};

/**
 * The part of an entry that BuildSections needs, and nothing else.
 *
 * Grouping is pure arithmetic on strings, so it is kept away from UObjects: that is what makes it testable
 * without a world, a game instance or a loaded project.
 */
USTRUCT(BlueprintType)
struct CONSOLEDECK_API FConsoleDeckEntryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ConsoleDeck")
	FString Section;

	UPROPERTY(BlueprintReadWrite, Category = "ConsoleDeck")
	FString DisplayName;

	/** Index into whatever array this view was made from. Carried through so the caller can find its way back. */
	UPROPERTY(BlueprintReadWrite, Category = "ConsoleDeck")
	int32 SourceIndex = INDEX_NONE;
};

/** One tab of the menu: a name and the entries that carry it, in display order. */
USTRUCT(BlueprintType)
struct CONSOLEDECK_API FConsoleDeckSection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	TArray<int32> EntryIndices;
};

/**
 * What the catalog cost and what it found.
 *
 * This is drawn in the header of the menu. Walking every UClass in a large project is not free, and a tool
 * that spends time on your behalf should print the number rather than leave you to guess it.
 */
USTRUCT(BlueprintType)
struct CONSOLEDECK_API FConsoleDeckStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	int32 Entries = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	int32 Sections = 0;

	/** How many UClasses the scan looked at, after the path filters. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	int32 ClassesScanned = 0;

	/** How many functions carried the ConsoleDeck key. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	int32 FunctionsMarked = 0;

	/** Entries that exist but have no target in this world right now. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	int32 Unavailable = 0;

	/** How long the scan took, in milliseconds. On screen, in the header, every time. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	float ScanMilliseconds = 0.0f;

	/** True when the catalog came from the baked list rather than from live metadata - i.e. in a cooked build. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	bool bFromBakedCatalog = false;

	/** How many entries the baked catalog holds, whether or not it was used. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	int32 BakedEntries = 0;
};

/**
 * One line of the baked catalog.
 *
 * Function metadata is editor-only data. WITH_METADATA follows WITH_EDITORONLY_DATA, so in a cooked game
 * the meta=(ConsoleDeck=...) key that the whole plugin is built on is simply not in memory any more. The
 * honest way out is not to pretend otherwise: the editor writes what it found into the project config, and
 * the packaged build reads that instead of scanning. Same catalog, no metadata required, and if the two
 * ever disagree the header of the menu says which one you are looking at.
 */
USTRUCT(BlueprintType)
struct CONSOLEDECK_API FConsoleDeckBakedEntry
{
	GENERATED_BODY()

	/** /Script/MyGame.MyPawn, or /Game/Blueprints/BP_Thing.BP_Thing_C for a Blueprint class. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "ConsoleDeck")
	FString ClassPath;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "ConsoleDeck")
	FName FunctionName;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "ConsoleDeck")
	FString Section;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "ConsoleDeck")
	FString DisplayName;

	/** The ConsoleDeckArgs value as written, so defaults, limits and presets survive the cook too. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "ConsoleDeck")
	FString Args;
};

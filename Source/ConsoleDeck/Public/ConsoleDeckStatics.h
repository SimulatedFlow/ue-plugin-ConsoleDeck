// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "ConsoleDeckTypes.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConsoleDeckStatics.generated.h"

class UCanvas;
class UConsoleDeckEntry;

/**
 * The Blueprint surface, and the part of the deck that has no world.
 *
 * The second half is the more interesting one. Parsing a metadata value, grouping entries into sections
 * and turning typed text into a value are pure string arithmetic; they do not need a game instance, a
 * player controller or a loaded level. Keeping them static and free of UObjects is what makes them
 * testable in a commandlet on a build server - which is where the five ConsoleDeck.* tests run.
 */
UCLASS()
class CONSOLEDECK_API UConsoleDeckStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// --------------------------------------------------------------------------------------------------
	// Pure logic - no world, no UObjects, fully testable
	// --------------------------------------------------------------------------------------------------

	/**
	 * Split a ConsoleDeck metadata value into a section and a display name.
	 *
	 * "Player|God Mode" gives Player and God Mode. "God Mode" - no bar - gives General and God Mode,
	 * because an entry without a section still has to go somewhere, and dropping it would be worse. Only
	 * the first bar splits: "World|Spawn|Wave" is the section World and the name "Spawn|Wave", so a name
	 * that happens to contain a bar survives instead of being silently cut in three.
	 */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck|Logic")
	static void ParseMeta(const FString& MetaValue, FString& OutSection, FString& OutDisplayName);

	/**
	 * Group entries into sections.
	 *
	 * Same section name, one section - matched case-insensitively, so "Player" and "player" do not become
	 * two tabs that look identical. Sections come out in alphabetical order and entries inside a section
	 * in alphabetical order, with the exact string as the tie-break, so the same catalog always produces
	 * the same menu no matter what order the class walk happened to find things in. A menu whose entries
	 * move between runs is a menu nobody builds muscle memory for.
	 */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck|Logic")
	static TArray<FConsoleDeckSection> BuildSections(const TArray<FConsoleDeckEntryView>& Entries);

	/**
	 * Turn one piece of text into a value for one parameter, or refuse it.
	 *
	 * Refusing matters more than converting. FCString::Atoi("twelve") is 0, and a cheat menu that spawns
	 * zero enemies because somebody typed a word is worse than one that says "twelve is not a whole
	 * number". Numbers are clamped to ClampMin / ClampMax afterwards; an enum is matched by name or by a
	 * valid index and by nothing else.
	 */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck|Logic")
	static bool CoerceValue(const FConsoleDeckParam& Param, const FString& Text, FString& OutValue, FString& OutError);

	/**
	 * Coerce a whole argument list against a whole signature.
	 *
	 * Missing arguments keep the value the parameter already has, which is what makes
	 * "ConsoleDeck.Invoke Player|Speed" mean "run it with whatever is dialled in". Too many arguments is an
	 * error, because it almost always means the caller has the wrong function in mind.
	 */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck|Logic")
	static bool CoerceArgs(const TArray<FConsoleDeckParam>& Params, const TArray<FString>& Args, TArray<FString>& OutValues, FString& OutError);

	/** Apply ClampMin / ClampMax. Separate from CoerceValue so the menu's own arrows go through it too. */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck|Logic")
	static double ClampToParam(const FConsoleDeckParam& Param, double Value);

	/**
	 * Read the ConsoleDeckArgs value into the parameters it names.
	 *
	 * "Count=10:0..64; Marker=checkpoint|boss|end" - a default after the equals sign, optional limits after
	 * a colon, optional presets separated by bars. Names that match nothing in the signature are ignored
	 * rather than fatal; a typo in a metadata string must not stop a project from starting.
	 */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck|Logic")
	static void ApplyArgSpec(const FString& ArgSpec, UPARAM(ref) TArray<FConsoleDeckParam>& Params);

	/**
	 * Describe a function's parameters in the terms the menu draws.
	 *
	 * Native: the kinds come from the FProperty types, which survive a cook; the limits come from
	 * ClampMin / ClampMax metadata, which does not, and from ConsoleDeckArgs, which the baked catalog
	 * carries for exactly that reason.
	 */
	static TArray<FConsoleDeckParam> DescribeFunction(const UFunction* Function, bool& bOutSupported, FString& OutUnsupportedReason);

	/** Coerce a text argument list straight against a function. The convenience form of the two above. */
	static bool CoerceArgsForFunction(const UFunction* Function, const TArray<FString>& Args, TArray<FString>& OutValues, FString& OutError);

	// --------------------------------------------------------------------------------------------------
	// Blueprint surface
	// --------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static bool OpenDeck(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static void CloseDeck(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static bool ToggleDeck(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static bool IsDeckOpen(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static bool IsDeckUnlocked(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static bool UnlockDeck(const UObject* WorldContextObject, const FString& Word);

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static void RebuildDeck(const UObject* WorldContextObject);

	/** Call an entry by "Section|Name" with arguments as text. The same path the console command takes. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Args"))
	static bool InvokeEntry(const UObject* WorldContextObject, const FString& Path, const TArray<FString>& Args, FString& OutResult);

	UFUNCTION(BlueprintPure, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static FConsoleDeckStats GetDeckStats(const UObject* WorldContextObject);

	/**
	 * Draw the deck from a HUD of your own.
	 *
	 * Two calls make any AHUD a ConsoleDeck host: this one from DrawHUD, and TickDeckInput from Tick.
	 * Deriving from AConsoleDeckHUD is the shortcut, not the requirement - most projects already have a
	 * HUD class and are not going to give it up for a debug menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static void DrawDeck(const UObject* WorldContextObject, UCanvas* Canvas);

	/** Feed the deck one frame of input from a player controller. See DrawDeck. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static void TickDeckInput(const UObject* WorldContextObject, APlayerController* PlayerController, float DeltaSeconds);
};

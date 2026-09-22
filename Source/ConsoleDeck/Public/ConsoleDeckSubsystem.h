// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "ConsoleDeckTypes.h"
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ConsoleDeckSubsystem.generated.h"

class APlayerController;
class UCanvas;
class UConsoleDeckEntry;

/** Fired after an entry was called, with whatever it returned or the error that stopped it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FConsoleDeckEntryInvoked, UConsoleDeckEntry*, Entry, bool, bSucceeded, const FString&, Result);

/** Fired when the deck opens or closes. Useful for muting your own input while it is up. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConsoleDeckVisibilityChanged, bool, bIsOpen);

/**
 * The catalog, the lock and the menu state.
 *
 * One per game instance, so the catalog survives a level change: the classes do not move when a map does,
 * only the instances do, and those are resolved again every time the deck opens.
 *
 * The scan runs once, at startup. TObjectIterator<UClass> over a large project is not a free operation -
 * it walks every loaded class and every function on it - and doing that every time somebody presses F8
 * would make the menu feel broken on exactly the machines where it is most needed. So it happens once, the
 * result is kept, and the time it took is drawn in the header of the menu where anybody can see it. If the
 * project grows a class after startup, ConsoleDeck.Rebuild or the rebuild key picks it up on demand.
 */
UCLASS(DisplayName = "ConsoleDeck")
class CONSOLEDECK_API UConsoleDeckSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** The deck of the world this object lives in, or null outside a game. */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck", meta = (WorldContext = "WorldContextObject"))
	static UConsoleDeckSubsystem* Get(const UObject* WorldContextObject);

	// --------------------------------------------------------------------------------------------------
	// Catalog
	// --------------------------------------------------------------------------------------------------

	/**
	 * Build the catalog: scan for marked functions, or read the baked list when metadata is gone.
	 *
	 * Safe to call at any time. Selection is kept where it can be kept, so rebuilding from inside the open
	 * menu does not throw you back to the first entry of the first section.
	 */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	void Rebuild();

	/** Look up every entry's target instance again and refresh the greyed-out reasons. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	void RefreshAvailability();

	/** Every entry in the catalog, in the order it was found. Blueprint-facing, so it copies. */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck", DisplayName = "Get Entries")
	TArray<UConsoleDeckEntry*> GetEntriesForBlueprint() const;

	/** The native accessor. No copy. */
	const TArray<TObjectPtr<UConsoleDeckEntry>>& GetEntries() const { return Entries; }

	UFUNCTION(BlueprintPure, Category = "ConsoleDeck", DisplayName = "Get Sections")
	TArray<FConsoleDeckSection> GetSectionsForBlueprint() const { return Sections; }

	const TArray<FConsoleDeckSection>& GetSections() const { return Sections; }

	UFUNCTION(BlueprintPure, Category = "ConsoleDeck", DisplayName = "Get Stats")
	FConsoleDeckStats GetStatsForBlueprint() const { return Stats; }

	const FConsoleDeckStats& GetStats() const { return Stats; }

	/** Find an entry by "Section|Name", case-insensitive. Null if there is no such entry. */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck")
	UConsoleDeckEntry* FindEntry(const FString& Path) const;

	/** Write the whole catalog to the log, one line per entry, greyed-out reasons included. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	void DumpToLog() const;

	/**
	 * Write the current catalog into the project settings so it survives a cook.
	 *
	 * Editor only, because that is the only place metadata exists and the only place writing to
	 * DefaultGame.ini makes sense. Returns how many entries were written, or -1 outside the editor.
	 */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	int32 BakeCatalog();

	// --------------------------------------------------------------------------------------------------
	// Calling
	// --------------------------------------------------------------------------------------------------

	/** Call an entry with the values currently dialled into it. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	bool Invoke(UConsoleDeckEntry* Entry);

	/**
	 * Call an entry by path, with arguments as text.
	 *
	 * The text goes through the same coercion the menu uses, so a wrong argument is refused with a message
	 * rather than quietly turned into a zero. This is the surface a test, a build script or a bug report
	 * repro step talks to.
	 */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	bool InvokeByPath(const FString& Path, const TArray<FString>& Args, FString& OutResult);

	// --------------------------------------------------------------------------------------------------
	// The lock
	// --------------------------------------------------------------------------------------------------

	/**
	 * May the deck be opened here?
	 *
	 * Development and Debug: yes, unless the project asked for the word there too. Shipping: only after
	 * ConsoleDeck.Unlock with the right word, or when the project explicitly allowed it. An empty word in a
	 * Shipping build means locked, because a setting somebody forgot must fail closed.
	 */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck")
	bool IsUnlocked() const;

	/** Try the word. True if the deck is unlocked afterwards. Wrong words are logged, not explained. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	bool TryUnlock(const FString& Word);

	/** Lock it again for this session. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	void Lock();

	// --------------------------------------------------------------------------------------------------
	// Open, close, navigate
	// --------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	bool Open();

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck")
	bool Toggle();

	UFUNCTION(BlueprintPure, Category = "ConsoleDeck")
	bool IsOpen() const { return bOpen; }

	/** Move the focus down (+1) or up (-1) inside the current section. Wraps. */
	void MoveSelection(int32 Direction);

	/** Change section. Wraps, and puts the focus on the first entry of the new one. */
	void MoveSection(int32 Direction);

	/** Left or right on the focused entry. StepScale > 1 while a direction is being held. */
	void AdjustValue(int32 Direction, float StepScale);

	/** The A button: call the focused entry, or toggle it if it is a single switch. */
	void ActivateSelected();

	/** Put the focused entry's parameters back to their defaults. */
	void ResetSelected();

	/** The focused entry, or null when the section is empty. */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck")
	UConsoleDeckEntry* GetFocusedEntry() const;

	int32 GetSelectedSection() const { return SelectedSection; }
	int32 GetSelectedRow() const { return SelectedRow; }
	int32 GetSelectedParam() const { return SelectedParam; }

	/** Move between the parameters of the focused entry, for a function that takes more than one. */
	void MoveParam(int32 Direction);

	// --------------------------------------------------------------------------------------------------
	// Typing
	// --------------------------------------------------------------------------------------------------

	/** True while a text parameter is being typed into. */
	bool IsTyping() const { return bTyping; }

	/** Start typing into the focused text parameter, seeded with its current value. */
	void BeginTyping();

	/** Keep what was typed. */
	void CommitTyping();

	/** Throw away what was typed. */
	void CancelTyping();

	void AppendTypedText(const FString& Text);
	void BackspaceTypedText();
	const FString& GetTypedText() const { return TypedText; }

	/** Set the focused text parameter directly - what ConsoleDeck.Set does, for people who have a console. */
	bool SetFocusedParamText(const FString& Text, FString& OutError);

	// --------------------------------------------------------------------------------------------------
	// Drawing and input
	// --------------------------------------------------------------------------------------------------

	/**
	 * Draw the whole panel onto a canvas.
	 *
	 * AConsoleDeckHUD calls this from DrawHUD. A project with its own HUD class calls the same function
	 * from its own DrawHUD and gets the identical menu - the deck is not tied to owning the HUD.
	 */
	void DrawDeck(UCanvas* Canvas);

	/**
	 * Poll the pad and the keyboard, once per frame, from whoever is ticking.
	 *
	 * Polling rather than binding: an input binding needs an input component, a mapping context and a
	 * priority, all of which are project decisions, and none of which are available in a level that has
	 * only a floor and a pawn. Raw key state through APlayerController works under both input stacks, in a
	 * packaged build, while the game is paused, with no setup at all.
	 */
	void TickInput(APlayerController* PC, float DeltaSeconds);

	// --------------------------------------------------------------------------------------------------
	// Events
	// --------------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "ConsoleDeck")
	FConsoleDeckEntryInvoked OnEntryInvoked;

	UPROPERTY(BlueprintAssignable, Category = "ConsoleDeck")
	FConsoleDeckVisibilityChanged OnVisibilityChanged;

private:
	/** The reflection walk. Fills Entries; returns how many classes it looked at. */
	int32 ScanLoadedClasses(TArray<TObjectPtr<UConsoleDeckEntry>>& OutEntries);

	/** The cooked-build path: turn the baked catalog back into entries. */
	int32 LoadBakedCatalog(TArray<TObjectPtr<UConsoleDeckEntry>>& OutEntries);

	/** Build one entry from a function and the value of its ConsoleDeck metadata key. */
	UConsoleDeckEntry* MakeEntry(UClass* OwnerClass, UFunction* Function, const FString& MetaValue, const FString& ArgSpec);

	/** Group Entries into Sections and put the selection back where it can go. */
	void RegroupSections();

	/** Apply move / look blocking and pausing for the current open state. */
	void ApplyInputSuppression(bool bDeckOpen);

	/** The local player controller this deck listens to. */
	APlayerController* GetLocalPlayerController() const;

	/** One key set, one edge-triggered answer, with repeat. */
	bool PollAction(APlayerController* PC, const TArray<FKey>& Keys, float DeltaSeconds, int32 ActionId, bool& bOutRepeating);

	/** Read the letters and digits a keyboard is sending this frame into the text buffer. */
	void PollTypedCharacters(APlayerController* PC);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UConsoleDeckEntry>> Entries;

	UPROPERTY(Transient)
	TArray<FConsoleDeckSection> Sections;

	UPROPERTY(Transient)
	FConsoleDeckStats Stats;

	bool bOpen = false;
	bool bUnlockedThisSession = false;
	bool bCatalogBuilt = false;

	int32 SelectedSection = 0;
	int32 SelectedRow = 0;
	int32 SelectedParam = 0;
	int32 ScrollOffset = 0;

	bool bTyping = false;
	FString TypedText;

	/** Status line under the list: the last thing that happened, and whether it went well. */
	FString StatusLine;
	bool bStatusIsError = false;

	/** Per-action key repeat state. Indexed by the ActionId passed to PollAction. */
	TMap<int32, float> HeldSeconds;
	TMap<int32, float> RepeatCountdown;

	/** Debounce for the open key, which is polled while the deck is closed as well. */
	bool bOpenKeyWasDown = false;

	/** True while this deck changed the pause state, so it can put it back exactly as it was. */
	bool bPausedByDeck = false;
};

// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "ConsoleDeckTypes.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ConsoleDeckEntry.generated.h"

class UWorld;

/**
 * One line in the menu: a function of yours, the class it lives on, and the values it will be called with.
 *
 * An entry owns no state of the game. It holds a UFunction, a weak pointer to whichever instance it found
 * last time it looked, and the parameter values somebody has dialled in - nothing else. That is what makes
 * it safe to keep the catalog alive across a level change: the function does not move, the instance does,
 * and the instance is looked up again every time the deck is opened.
 */
UCLASS(BlueprintType)
class CONSOLEDECK_API UConsoleDeckEntry : public UObject
{
	GENERATED_BODY()

public:
	/** Section this entry was filed under. Everything left of the first '|' in the metadata, or "General". */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString Section;

	/** What the menu shows. Everything right of the first '|', or the function name if the metadata was empty. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString DisplayName;

	/** The class the function was found on. The target instance is looked up from this. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	TObjectPtr<UClass> TargetClass;

	/** Path of TargetClass, kept as text for the log, the baked catalog and the reason lines. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString ClassPath;

	/** The function itself. A UFunction is a UObject, so a UPROPERTY is all the lifetime management needed. */
	UPROPERTY()
	TObjectPtr<UFunction> Function;

	/** The signature, described in terms the menu can draw. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	TArray<FConsoleDeckParam> Params;

	/** Whether this entry can be used right now, and if not, why not. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	EConsoleDeckAvailability Availability = EConsoleDeckAvailability::NoWorld;

	/** Plain language, drawn next to the greyed-out entry. "no BP_Boss in this world", not "0x0". */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString UnavailableReason;

	/** What the last call returned, or the error that stopped it. Drawn in the status line. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString LastResult;

	/** True when this entry came out of the baked catalog rather than out of live metadata. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	bool bFromBakedCatalog = false;

	/**
	 * False when the signature has something the deck cannot edit.
	 *
	 * Kept separately from Availability because the two are different problems: a missing instance may be
	 * there again in the next level, an FVector parameter never will be, and the reason line has to say
	 * which of the two the person is looking at.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	bool bSignatureSupported = true;

	/** Why the signature is unsupported, if it is. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString SignatureReason;

	/** The raw ConsoleDeckArgs value, carried so the entry can be baked again unchanged. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	FString ArgSpec;

	/** The instance found the last time the deck looked. Weak: the deck must never keep an actor alive. */
	UPROPERTY(BlueprintReadOnly, Category = "ConsoleDeck")
	TWeakObjectPtr<UObject> CachedTarget;

	/** Can this be called right now? */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck")
	bool IsReady() const { return Availability == EConsoleDeckAvailability::Ready; }

	/** "Player|God Mode" - how an entry is named on the console and in the log. */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck")
	FString GetPath() const;

	/**
	 * Find the instance to call the function on, and set Availability and UnavailableReason accordingly.
	 *
	 * Called when the deck opens, on rebuild, and once more immediately before every call - an actor can be
	 * destroyed between opening the menu and pressing the button, and the deck must notice that rather than
	 * dereference what it found a minute ago.
	 */
	UObject* ResolveTarget(UWorld* World);

	/**
	 * Call the function on its target with the current parameter values.
	 *
	 * The values are packed into a parameter buffer laid out by the function's own properties and handed to
	 * ProcessEvent, which is what the engine does for a Blueprint call - so a native function, a Blueprint
	 * function and a Blueprint-implementable event all behave the same here. Returns false and writes
	 * LastResult when there is nothing to call it on.
	 */
	bool Invoke(UWorld* World);

	/** The value column of this entry, drawn as one string: "[ on ]", "< 1200 >", "Wireframe". */
	FString GetValueText() const;

	/**
	 * Left and right on the d-pad. Direction is -1 or +1; a bool or an enum wraps, a number is clamped.
	 *
	 * StepScale is what a held direction multiplies the step by. Without it a range of nought to ten
	 * thousand is not crossable on a pad; with it, holding right for a second gets you there.
	 */
	void AdjustParam(int32 ParamIndex, int32 Direction, float StepScale = 1.0f);

	/** Put every parameter back to the default the signature or ConsoleDeckArgs asked for. */
	void ResetParams();

	/** Index of the first parameter that can be edited, or INDEX_NONE if the entry takes none. */
	int32 GetFirstEditableParam() const;
};

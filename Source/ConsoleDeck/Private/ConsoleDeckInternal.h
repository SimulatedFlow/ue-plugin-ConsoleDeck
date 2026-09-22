// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UEnum;

/**
 * The small pieces the deck's own files share and nobody else needs.
 *
 * Private on purpose: these are string helpers with opinions - what counts as a number, what counts as a
 * yes - and those opinions belong to this plugin, not to a project that happens to include its headers.
 */
namespace ConsoleDeck
{
	/** meta=(ConsoleDeck="Section|Name") - the key the whole plugin hangs off. */
	extern const TCHAR* MetaKey;

	/** meta=(ConsoleDeckArgs="Count=10:0..64") - defaults, limits and presets. */
	extern const TCHAR* ArgsMetaKey;

	/** Where an entry goes when its metadata names no section. */
	extern const TCHAR* DefaultSection;

	/** True only for a string that is entirely a whole number. FCString::Atoi is not an answer here. */
	bool ParseWholeNumber(const FString& Text, int64& OutValue);

	/** True only for a string that is entirely a decimal number, exponent allowed. */
	bool ParseDecimal(const FString& Text, double& OutValue);

	/** true / 1 / yes / on / enabled, and the five opposites. Anything else is not a boolean. */
	bool ParseBoolean(const FString& Text, bool& OutValue);

	/** Whole numbers without a decimal point, decimals without a tail of zeroes. */
	FString FormatNumber(double Value, bool bWhole);

	/** Every pickable name of an enum, in declaration order, without _MAX and without the hidden ones. */
	void CollectEnumOptions(const UEnum* Enum, TArray<FString>& OutOptions);
}

// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ConsoleDeckStatics.h"

#include "ConsoleDeckEntry.h"
#include "ConsoleDeckInternal.h"
#include "ConsoleDeckLog.h"
#include "ConsoleDeckSubsystem.h"
#include "Engine/Engine.h"
// TextProperty.h and not just UnrealType.h: FTextProperty lives in its own header, and IsA<FTextProperty>
// needs the complete type rather than the forward declaration UnrealType.h leaves behind.
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace ConsoleDeck
{
	const TCHAR* MetaKey = TEXT("ConsoleDeck");
	const TCHAR* ArgsMetaKey = TEXT("ConsoleDeckArgs");
	const TCHAR* DefaultSection = TEXT("General");

	/**
	 * A whole number and nothing else.
	 *
	 * FCString::Atoi("twelve") is zero, and that is the trap this exists to avoid: a menu that spawns no
	 * enemies because somebody typed a word, and no message anywhere saying why.
	 */
	bool ParseWholeNumber(const FString& Text, int64& OutValue)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return false;
		}

		int32 Index = (Trimmed[0] == TEXT('+') || Trimmed[0] == TEXT('-')) ? 1 : 0;
		if (Index >= Trimmed.Len())
		{
			return false;
		}

		for (; Index < Trimmed.Len(); ++Index)
		{
			if (!FChar::IsDigit(Trimmed[Index]))
			{
				return false;
			}
		}

		OutValue = FCString::Atoi64(*Trimmed);
		return true;
	}

	/** A decimal number, with an optional exponent, and nothing else. */
	bool ParseDecimal(const FString& Text, double& OutValue)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return false;
		}

		int32 Index = (Trimmed[0] == TEXT('+') || Trimmed[0] == TEXT('-')) ? 1 : 0;
		bool bAnyDigit = false;
		bool bSeenDot = false;
		bool bSeenExponent = false;

		for (; Index < Trimmed.Len(); ++Index)
		{
			const TCHAR Char = Trimmed[Index];

			if (FChar::IsDigit(Char))
			{
				bAnyDigit = true;
				continue;
			}

			if (Char == TEXT('.') && !bSeenDot && !bSeenExponent)
			{
				bSeenDot = true;
				continue;
			}

			if ((Char == TEXT('e') || Char == TEXT('E')) && bAnyDigit && !bSeenExponent)
			{
				bSeenExponent = true;
				bAnyDigit = false;
				if (Index + 1 < Trimmed.Len() && (Trimmed[Index + 1] == TEXT('+') || Trimmed[Index + 1] == TEXT('-')))
				{
					++Index;
				}
				continue;
			}

			return false;
		}

		if (!bAnyDigit)
		{
			return false;
		}

		OutValue = FCString::Atod(*Trimmed);
		return true;
	}

	/** One of the words a person actually types when they mean yes or no. */
	bool ParseBoolean(const FString& Text, bool& OutValue)
	{
		const FString Trimmed = Text.TrimStartAndEnd();

		static const TCHAR* TrueWords[] = { TEXT("true"), TEXT("1"), TEXT("yes"), TEXT("on"), TEXT("enabled") };
		static const TCHAR* FalseWords[] = { TEXT("false"), TEXT("0"), TEXT("no"), TEXT("off"), TEXT("disabled") };

		for (const TCHAR* Word : TrueWords)
		{
			if (Trimmed.Equals(Word, ESearchCase::IgnoreCase))
			{
				OutValue = true;
				return true;
			}
		}

		for (const TCHAR* Word : FalseWords)
		{
			if (Trimmed.Equals(Word, ESearchCase::IgnoreCase))
			{
				OutValue = false;
				return true;
			}
		}

		return false;
	}

	/** Numbers on screen: whole ones without a point, decimals with as few digits as still say something. */
	FString FormatNumber(double Value, bool bWhole)
	{
		if (bWhole)
		{
			return FString::Printf(TEXT("%lld"), static_cast<int64>(FMath::RoundToDouble(Value)));
		}

		FString Text = FString::Printf(TEXT("%.4f"), Value);

		// Trim the zeroes a fixed format leaves behind, but never the last digit: "12." reads like a typo.
		if (Text.Contains(TEXT(".")))
		{
			int32 LastKept = Text.Len() - 1;
			while (LastKept > 0 && Text[LastKept] == TEXT('0'))
			{
				--LastKept;
			}
			if (LastKept > 0 && Text[LastKept] == TEXT('.'))
			{
				++LastKept;
			}
			Text.LeftInline(LastKept + 1, EAllowShrinking::No);
		}

		return Text;
	}

	/** All the names of an enum a person may pick, in declaration order, without the generated _MAX. */
	void CollectEnumOptions(const UEnum* Enum, TArray<FString>& OutOptions)
	{
		if (!Enum)
		{
			return;
		}

		const int32 Count = Enum->NumEnums();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FString Name = Enum->GetNameStringByIndex(Index);
			if (Name.IsEmpty() || Name.EndsWith(TEXT("_MAX"), ESearchCase::CaseSensitive))
			{
				continue;
			}

#if WITH_METADATA
			if (Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				continue;
			}
#endif

			OutOptions.Add(Name);
		}
	}
}

// ------------------------------------------------------------------------------------------------------
// Pure logic
// ------------------------------------------------------------------------------------------------------

void UConsoleDeckStatics::ParseMeta(const FString& MetaValue, FString& OutSection, FString& OutDisplayName)
{
	const FString Trimmed = MetaValue.TrimStartAndEnd();

	int32 BarIndex = INDEX_NONE;
	if (Trimmed.FindChar(TEXT('|'), BarIndex))
	{
		OutSection = Trimmed.Left(BarIndex).TrimStartAndEnd();
		OutDisplayName = Trimmed.Mid(BarIndex + 1).TrimStartAndEnd();
	}
	else
	{
		OutSection.Empty();
		OutDisplayName = Trimmed;
	}

	// No section, or a bar with nothing in front of it: the entry still has to be somewhere a person can
	// reach it, and "General" is a great deal better than dropping it on the floor.
	if (OutSection.IsEmpty())
	{
		OutSection = ConsoleDeck::DefaultSection;
	}
}

TArray<FConsoleDeckSection> UConsoleDeckStatics::BuildSections(const TArray<FConsoleDeckEntryView>& Entries)
{
	// Sort a list of indices rather than the entries themselves: the caller's array must not be reordered
	// underneath it, and the index is what identifies an entry afterwards.
	TArray<int32> Order;
	Order.Reserve(Entries.Num());
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Order.Add(Index);
	}

	Order.Sort([&Entries](int32 A, int32 B)
	{
		const FConsoleDeckEntryView& Left = Entries[A];
		const FConsoleDeckEntryView& Right = Entries[B];

		// Case-insensitive first, so "Player" and "player" land next to each other and become one section.
		// Then the exact string, then the name, then the index the caller gave: four keys, no ties left.
		// Stability is not decoration here - a menu whose rows swap places between two runs of the same
		// project is a menu nobody ever learns by heart.
		int32 Compare = Left.Section.Compare(Right.Section, ESearchCase::IgnoreCase);
		if (Compare != 0)
		{
			return Compare < 0;
		}

		Compare = Left.Section.Compare(Right.Section, ESearchCase::CaseSensitive);
		if (Compare != 0)
		{
			return Compare < 0;
		}

		Compare = Left.DisplayName.Compare(Right.DisplayName, ESearchCase::IgnoreCase);
		if (Compare != 0)
		{
			return Compare < 0;
		}

		Compare = Left.DisplayName.Compare(Right.DisplayName, ESearchCase::CaseSensitive);
		if (Compare != 0)
		{
			return Compare < 0;
		}

		return A < B;
	});

	TArray<FConsoleDeckSection> Result;

	for (const int32 Index : Order)
	{
		const FConsoleDeckEntryView& View = Entries[Index];
		const FString SectionName = View.Section.IsEmpty() ? FString(ConsoleDeck::DefaultSection) : View.Section;

		if (Result.Num() == 0 || !Result.Last().Name.Equals(SectionName, ESearchCase::IgnoreCase))
		{
			FConsoleDeckSection& NewSection = Result.AddDefaulted_GetRef();
			NewSection.Name = SectionName;
		}

		Result.Last().EntryIndices.Add(View.SourceIndex != INDEX_NONE ? View.SourceIndex : Index);
	}

	return Result;
}

double UConsoleDeckStatics::ClampToParam(const FConsoleDeckParam& Param, double Value)
{
	if (Param.bHasMin)
	{
		Value = FMath::Max(Value, Param.Min);
	}
	if (Param.bHasMax)
	{
		Value = FMath::Min(Value, Param.Max);
	}
	return Value;
}

bool UConsoleDeckStatics::CoerceValue(const FConsoleDeckParam& Param, const FString& Text, FString& OutValue, FString& OutError)
{
	OutError.Reset();

	switch (Param.Kind)
	{
	case EConsoleDeckParamKind::Bool:
	{
		bool bValue = false;
		if (!ConsoleDeck::ParseBoolean(Text, bValue))
		{
			OutError = FString::Printf(TEXT("'%s' is not a yes or a no for %s"), *Text, *Param.Name.ToString());
			return false;
		}
		OutValue = bValue ? TEXT("true") : TEXT("false");
		return true;
	}

	case EConsoleDeckParamKind::Int:
	{
		int64 Value = 0;
		if (!ConsoleDeck::ParseWholeNumber(Text, Value))
		{
			OutError = FString::Printf(TEXT("'%s' is not a whole number for %s"), *Text, *Param.Name.ToString());
			return false;
		}
		OutValue = ConsoleDeck::FormatNumber(ClampToParam(Param, static_cast<double>(Value)), true);
		return true;
	}

	case EConsoleDeckParamKind::Float:
	{
		double Value = 0.0;
		if (!ConsoleDeck::ParseDecimal(Text, Value))
		{
			OutError = FString::Printf(TEXT("'%s' is not a number for %s"), *Text, *Param.Name.ToString());
			return false;
		}
		OutValue = ConsoleDeck::FormatNumber(ClampToParam(Param, Value), false);
		return true;
	}

	case EConsoleDeckParamKind::Enum:
	{
		const FString Trimmed = Text.TrimStartAndEnd();

		for (const FString& Option : Param.Options)
		{
			if (Option.Equals(Trimmed, ESearchCase::IgnoreCase))
			{
				OutValue = Option;
				return true;
			}
		}

		// An index is accepted too, because that is what a script tends to have. Out of range is refused,
		// not wrapped: an enum with three entries told to take number nine means somebody is wrong about
		// which enum this is.
		int64 AsIndex = 0;
		if (ConsoleDeck::ParseWholeNumber(Trimmed, AsIndex) && Param.Options.IsValidIndex(static_cast<int32>(AsIndex)))
		{
			OutValue = Param.Options[static_cast<int32>(AsIndex)];
			return true;
		}

		OutError = FString::Printf(TEXT("'%s' is not one of %s"), *Text, *FString::Join(Param.Options, TEXT(", ")));
		return false;
	}

	case EConsoleDeckParamKind::String:
	case EConsoleDeckParamKind::Name:
	case EConsoleDeckParamKind::Text:
		OutValue = Text;
		return true;

	default:
		OutError = FString::Printf(TEXT("%s is a %s, which the deck cannot set"), *Param.Name.ToString(), *Param.TypeName);
		return false;
	}
}

bool UConsoleDeckStatics::CoerceArgs(const TArray<FConsoleDeckParam>& Params, const TArray<FString>& Args, TArray<FString>& OutValues, FString& OutError)
{
	OutError.Reset();
	OutValues.Reset();

	if (Args.Num() > Params.Num())
	{
		OutError = FString::Printf(TEXT("%d argument(s) given, %d expected"), Args.Num(), Params.Num());
		return false;
	}

	OutValues.Reserve(Params.Num());

	for (int32 Index = 0; Index < Params.Num(); ++Index)
	{
		// Fewer arguments than parameters is not an error: it means "use what is already dialled in", which
		// is what makes calling an entry from the console as short as pressing the button in the menu.
		if (!Args.IsValidIndex(Index))
		{
			OutValues.Add(Params[Index].Value);
			continue;
		}

		FString Coerced;
		if (!CoerceValue(Params[Index], Args[Index], Coerced, OutError))
		{
			OutValues.Reset();
			return false;
		}

		OutValues.Add(Coerced);
	}

	return true;
}

void UConsoleDeckStatics::ApplyArgSpec(const FString& ArgSpec, TArray<FConsoleDeckParam>& Params)
{
	if (ArgSpec.IsEmpty() || Params.Num() == 0)
	{
		return;
	}

	TArray<FString> Segments;
	ArgSpec.ParseIntoArray(Segments, TEXT(";"), true);

	for (const FString& RawSegment : Segments)
	{
		const FString Segment = RawSegment.TrimStartAndEnd();
		if (Segment.IsEmpty())
		{
			continue;
		}

		FString NamePart;
		FString ValuePart;
		if (!Segment.Split(TEXT("="), &NamePart, &ValuePart))
		{
			// A name with nothing after it says nothing. Ignored rather than treated as an error: a typo in
			// a metadata string must never be the reason a project fails to start.
			continue;
		}

		NamePart.TrimStartAndEndInline();
		ValuePart.TrimStartAndEndInline();

		FConsoleDeckParam* Param = Params.FindByPredicate([&NamePart](const FConsoleDeckParam& Candidate)
		{
			return Candidate.Name.ToString().Equals(NamePart, ESearchCase::IgnoreCase);
		});

		if (!Param)
		{
			continue;
		}

		// "10:0..64" - the default in front of the colon, the limits behind it. The colon only counts as a
		// separator when what follows really looks like a range, so a string default containing one is safe.
		FString RangePart;
		int32 ColonIndex = INDEX_NONE;
		if (ValuePart.FindLastChar(TEXT(':'), ColonIndex))
		{
			const FString Candidate = ValuePart.Mid(ColonIndex + 1);
			if (Candidate.Contains(TEXT("..")))
			{
				RangePart = Candidate;
				ValuePart.LeftInline(ColonIndex, EAllowShrinking::No);
				ValuePart.TrimStartAndEndInline();
			}
		}

		if (!RangePart.IsEmpty())
		{
			FString MinText;
			FString MaxText;
			if (RangePart.Split(TEXT(".."), &MinText, &MaxText))
			{
				double Value = 0.0;
				if (ConsoleDeck::ParseDecimal(MinText, Value))
				{
					Param->bHasMin = true;
					Param->Min = Value;
				}
				if (ConsoleDeck::ParseDecimal(MaxText, Value))
				{
					Param->bHasMax = true;
					Param->Max = Value;
				}
			}
		}

		if (ValuePart.IsEmpty())
		{
			continue;
		}

		// Bars make a list of presets. That is the only way a string parameter is usable on a pad, where
		// there is no keyboard to type "checkpoint" with.
		if (ValuePart.Contains(TEXT("|")))
		{
			TArray<FString> Options;
			ValuePart.ParseIntoArray(Options, TEXT("|"), true);
			for (FString& Option : Options)
			{
				Option.TrimStartAndEndInline();
			}

			if (Options.Num() > 0)
			{
				Param->Options = Options;
				Param->DefaultValue = Options[0];
				Param->Value = Options[0];
			}
			continue;
		}

		FString Coerced;
		FString Error;
		if (CoerceValue(*Param, ValuePart, Coerced, Error))
		{
			Param->DefaultValue = Coerced;
			Param->Value = Coerced;
		}
		else
		{
			UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: ConsoleDeckArgs for '%s' - %s. Ignored."),
				*NamePart, *Error);
		}
	}
}

TArray<FConsoleDeckParam> UConsoleDeckStatics::DescribeFunction(const UFunction* Function, bool& bOutSupported, FString& OutUnsupportedReason)
{
	bOutSupported = true;
	OutUnsupportedReason.Reset();

	TArray<FConsoleDeckParam> Params;

	if (!Function)
	{
		bOutSupported = false;
		OutUnsupportedReason = TEXT("no function");
		return Params;
	}

	// Function-level ClampMin / ClampMax. UHT has no syntax for per-parameter limits on a UFUNCTION, so a
	// function-level clamp applies to every number in the signature - which is right for the overwhelmingly
	// common case of one number. Anything finer than that is what ConsoleDeckArgs is for.
	bool bFunctionHasMin = false;
	bool bFunctionHasMax = false;
	double FunctionMin = 0.0;
	double FunctionMax = 0.0;

#if WITH_METADATA
	if (const FString* MinText = Function->FindMetaData(TEXT("ClampMin")))
	{
		bFunctionHasMin = ConsoleDeck::ParseDecimal(*MinText, FunctionMin);
	}
	if (const FString* MaxText = Function->FindMetaData(TEXT("ClampMax")))
	{
		bFunctionHasMax = ConsoleDeck::ParseDecimal(*MaxText, FunctionMax);
	}

	// A latent function needs a FLatentActionInfo that only the Blueprint VM knows how to fill in.
	if (Function->HasMetaData(TEXT("Latent")))
	{
		bOutSupported = false;
		OutUnsupportedReason = TEXT("latent function");
		return Params;
	}
#endif

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Property = *It;

		// The return value and pure out parameters are read back after the call, not edited before it.
		if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}
		if (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			continue;
		}

		FConsoleDeckParam Param;
		Param.Name = Property->GetFName();
		Param.TypeName = Property->GetCPPType();

		if (Property->IsA<FBoolProperty>())
		{
			Param.Kind = EConsoleDeckParamKind::Bool;
			Param.DefaultValue = TEXT("false");
		}
		else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Param.Kind = EConsoleDeckParamKind::Enum;
			ConsoleDeck::CollectEnumOptions(EnumProperty->GetEnum(), Param.Options);
			Param.DefaultValue = Param.Options.Num() > 0 ? Param.Options[0] : FString();
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			if (ByteProperty->Enum)
			{
				Param.Kind = EConsoleDeckParamKind::Enum;
				ConsoleDeck::CollectEnumOptions(ByteProperty->Enum, Param.Options);
				Param.DefaultValue = Param.Options.Num() > 0 ? Param.Options[0] : FString();
			}
			else
			{
				Param.Kind = EConsoleDeckParamKind::Int;
				Param.bHasMin = true;
				Param.Min = 0.0;
				Param.bHasMax = true;
				Param.Max = 255.0;
				Param.DefaultValue = TEXT("0");
			}
		}
		else if (Property->IsA<FIntProperty>() || Property->IsA<FInt64Property>() || Property->IsA<FInt16Property>()
			|| Property->IsA<FInt8Property>() || Property->IsA<FUInt16Property>() || Property->IsA<FUInt32Property>()
			|| Property->IsA<FUInt64Property>())
		{
			Param.Kind = EConsoleDeckParamKind::Int;
			Param.DefaultValue = TEXT("0");
		}
		else if (Property->IsA<FFloatProperty>() || Property->IsA<FDoubleProperty>())
		{
			Param.Kind = EConsoleDeckParamKind::Float;
			Param.DefaultValue = TEXT("0");
		}
		else if (Property->IsA<FStrProperty>())
		{
			Param.Kind = EConsoleDeckParamKind::String;
		}
		else if (Property->IsA<FNameProperty>())
		{
			Param.Kind = EConsoleDeckParamKind::Name;
		}
		else if (Property->IsA<FTextProperty>())
		{
			Param.Kind = EConsoleDeckParamKind::Text;
		}
		else
		{
			// A struct, an object, an array, a delegate. The entry is kept and shown greyed out with this
			// sentence next to it - "the deck cannot edit an FVector" is a fact somebody can act on,
			// whereas an entry that is quietly missing looks like the plugin is broken.
			Param.Kind = EConsoleDeckParamKind::Unsupported;
			bOutSupported = false;
			OutUnsupportedReason = FString::Printf(TEXT("%s is a %s, which the deck cannot edit"),
				*Param.Name.ToString(), *Param.TypeName);
		}

		if (Param.IsNumeric())
		{
			if (bFunctionHasMin)
			{
				Param.bHasMin = true;
				Param.Min = FunctionMin;
			}
			if (bFunctionHasMax)
			{
				Param.bHasMax = true;
				Param.Max = FunctionMax;
			}

#if WITH_METADATA
			// Property-level metadata wins over the function-level clamp when a project sets both.
			double Value = 0.0;
			if (const FString* MinText = Property->FindMetaData(TEXT("ClampMin")))
			{
				if (ConsoleDeck::ParseDecimal(*MinText, Value))
				{
					Param.bHasMin = true;
					Param.Min = Value;
				}
			}
			if (const FString* MaxText = Property->FindMetaData(TEXT("ClampMax")))
			{
				if (ConsoleDeck::ParseDecimal(*MaxText, Value))
				{
					Param.bHasMax = true;
					Param.Max = Value;
				}
			}
#endif

			if (Param.bHasMin)
			{
				Param.DefaultValue = ConsoleDeck::FormatNumber(Param.Min, Param.Kind == EConsoleDeckParamKind::Int);
			}
		}

		Param.Value = Param.DefaultValue;
		Params.Add(Param);
	}

	return Params;
}

bool UConsoleDeckStatics::CoerceArgsForFunction(const UFunction* Function, const TArray<FString>& Args, TArray<FString>& OutValues, FString& OutError)
{
	bool bSupported = true;
	FString Reason;
	const TArray<FConsoleDeckParam> Params = DescribeFunction(Function, bSupported, Reason);

	if (!bSupported)
	{
		OutError = Reason;
		return false;
	}

	return CoerceArgs(Params, Args, OutValues, OutError);
}

// ------------------------------------------------------------------------------------------------------
// Blueprint surface
// ------------------------------------------------------------------------------------------------------

bool UConsoleDeckStatics::OpenDeck(const UObject* WorldContextObject)
{
	UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject);
	return Deck ? Deck->Open() : false;
}

void UConsoleDeckStatics::CloseDeck(const UObject* WorldContextObject)
{
	if (UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject))
	{
		Deck->Close();
	}
}

bool UConsoleDeckStatics::ToggleDeck(const UObject* WorldContextObject)
{
	UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject);
	return Deck ? Deck->Toggle() : false;
}

bool UConsoleDeckStatics::IsDeckOpen(const UObject* WorldContextObject)
{
	const UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject);
	return Deck ? Deck->IsOpen() : false;
}

bool UConsoleDeckStatics::IsDeckUnlocked(const UObject* WorldContextObject)
{
	const UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject);
	return Deck ? Deck->IsUnlocked() : false;
}

bool UConsoleDeckStatics::UnlockDeck(const UObject* WorldContextObject, const FString& Word)
{
	UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject);
	return Deck ? Deck->TryUnlock(Word) : false;
}

void UConsoleDeckStatics::RebuildDeck(const UObject* WorldContextObject)
{
	if (UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject))
	{
		Deck->Rebuild();
	}
}

bool UConsoleDeckStatics::InvokeEntry(const UObject* WorldContextObject, const FString& Path, const TArray<FString>& Args, FString& OutResult)
{
	UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject);
	if (!Deck)
	{
		OutResult = TEXT("no ConsoleDeck here");
		return false;
	}

	return Deck->InvokeByPath(Path, Args, OutResult);
}

FConsoleDeckStats UConsoleDeckStatics::GetDeckStats(const UObject* WorldContextObject)
{
	const UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject);
	return Deck ? Deck->GetStats() : FConsoleDeckStats();
}

void UConsoleDeckStatics::DrawDeck(const UObject* WorldContextObject, UCanvas* Canvas)
{
	if (UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject))
	{
		Deck->DrawDeck(Canvas);
	}
}

void UConsoleDeckStatics::TickDeckInput(const UObject* WorldContextObject, APlayerController* PlayerController, float DeltaSeconds)
{
	if (UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(WorldContextObject))
	{
		Deck->TickInput(PlayerController, DeltaSeconds);
	}
}

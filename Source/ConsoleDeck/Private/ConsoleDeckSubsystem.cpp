// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ConsoleDeckSubsystem.h"

#include "CanvasItem.h"
#include "ConsoleDeckEntry.h"
#include "ConsoleDeckInternal.h"
#include "ConsoleDeckLog.h"
#include "ConsoleDeckSettings.h"
#include "ConsoleDeckStatics.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/StringBuilder.h"
#include "SceneTypes.h"
#include "UObject/UObjectIterator.h"

namespace ConsoleDeckDraw
{
	const FLinearColor Title(0.55f, 0.85f, 1.00f, 1.0f);
	const FLinearColor Body(0.90f, 0.90f, 0.90f, 1.0f);
	const FLinearColor Faint(0.58f, 0.58f, 0.64f, 1.0f);
	const FLinearColor Accent(1.00f, 0.82f, 0.35f, 1.0f);
	const FLinearColor Good(0.55f, 0.95f, 0.55f, 1.0f);
	const FLinearColor Bad(1.00f, 0.45f, 0.40f, 1.0f);

	/** Ids for the repeat bookkeeping. Only the direction keys repeat; the rest are edge-triggered. */
	enum class EAction : int32
	{
		Up = 1,
		Down = 2,
		Left = 3,
		Right = 4
	};

	bool AnyKeyDown(const APlayerController* PC, const TArray<FKey>& Keys)
	{
		for (const FKey& Key : Keys)
		{
			if (Key.IsValid() && PC->IsInputKeyDown(Key))
			{
				return true;
			}
		}
		return false;
	}

	bool AnyKeyJustPressed(const APlayerController* PC, const TArray<FKey>& Keys)
	{
		for (const FKey& Key : Keys)
		{
			if (Key.IsValid() && PC->WasInputKeyJustPressed(Key))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * The keys a text parameter can be typed with, worked out once from the engine's own key list.
	 *
	 * Built rather than written out: every letter key carries its own letter as its name, so twenty-six
	 * lines of table become one loop that cannot fall out of step with the engine.
	 */
	struct FTypedKey
	{
		FKey Key;
		TCHAR Character = 0;
	};

	const TArray<FTypedKey>& GetTypedKeys()
	{
		static TArray<FTypedKey> Keys;

		if (Keys.Num() == 0)
		{
			TArray<FKey> AllKeys;
			EKeys::GetAllKeys(AllKeys);

			for (const FKey& Key : AllKeys)
			{
				const FString Name = Key.GetFName().ToString();
				if (Name.Len() == 1 && FChar::IsAlpha(Name[0]))
				{
					Keys.Add({ Key, FChar::ToLower(Name[0]) });
				}
			}

			Keys.Add({ EKeys::Zero, TEXT('0') });
			Keys.Add({ EKeys::One, TEXT('1') });
			Keys.Add({ EKeys::Two, TEXT('2') });
			Keys.Add({ EKeys::Three, TEXT('3') });
			Keys.Add({ EKeys::Four, TEXT('4') });
			Keys.Add({ EKeys::Five, TEXT('5') });
			Keys.Add({ EKeys::Six, TEXT('6') });
			Keys.Add({ EKeys::Seven, TEXT('7') });
			Keys.Add({ EKeys::Eight, TEXT('8') });
			Keys.Add({ EKeys::Nine, TEXT('9') });
			Keys.Add({ EKeys::SpaceBar, TEXT(' ') });
			Keys.Add({ EKeys::Period, TEXT('.') });
			Keys.Add({ EKeys::Hyphen, TEXT('-') });
			Keys.Add({ EKeys::Underscore, TEXT('_') });
		}

		return Keys;
	}
}

// ------------------------------------------------------------------------------------------------------
// Lifetime
// ------------------------------------------------------------------------------------------------------

void UConsoleDeckSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UConsoleDeckSettings::Get().bBuildCatalogOnStartup)
	{
		Rebuild();
	}

	UE_LOG(LogConsoleDeck, Log, TEXT("ConsoleDeck: %d entry(s) in %d section(s), scan %.2f ms%s."),
		Stats.Entries, Stats.Sections, Stats.ScanMilliseconds,
		Stats.bFromBakedCatalog ? TEXT(" (from the baked catalog)") : TEXT(""));
}

void UConsoleDeckSubsystem::Deinitialize()
{
	if (bOpen)
	{
		ApplyInputSuppression(false);
	}

	bOpen = false;
	Entries.Reset();
	Sections.Reset();

	Super::Deinitialize();
}

UConsoleDeckSubsystem* UConsoleDeckSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UConsoleDeckSubsystem>() : nullptr;
}

APlayerController* UConsoleDeckSubsystem::GetLocalPlayerController() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

// ------------------------------------------------------------------------------------------------------
// Catalog
// ------------------------------------------------------------------------------------------------------

void UConsoleDeckSubsystem::Rebuild()
{
	const double StartSeconds = FPlatformTime::Seconds();

	// Remember where the focus was so a rebuild from inside the open menu does not throw somebody back to
	// the first row of the first tab. They pressed rebuild because they added a function, not because they
	// wanted to lose their place.
	const FString PreviousPath = GetFocusedEntry() ? GetFocusedEntry()->GetPath() : FString();

	TArray<TObjectPtr<UConsoleDeckEntry>> NewEntries;

	Stats = FConsoleDeckStats();
	Stats.BakedEntries = UConsoleDeckSettings::Get().BakedCatalog.Num();

	const int32 ClassesScanned = ScanLoadedClasses(NewEntries);
	Stats.ClassesScanned = ClassesScanned;
	Stats.FunctionsMarked = NewEntries.Num();

	// No metadata in memory means a cooked build. Fall back to what the editor wrote down, and say in the
	// header which of the two the menu on screen came from - the difference matters when a function is
	// missing and somebody has to work out whether it was never marked or never baked.
	if (NewEntries.Num() == 0)
	{
		const int32 FromBaked = LoadBakedCatalog(NewEntries);
		if (FromBaked > 0)
		{
			Stats.bFromBakedCatalog = true;
			Stats.FunctionsMarked = FromBaked;
		}
	}

	Entries = MoveTemp(NewEntries);
	RegroupSections();
	RefreshAvailability();

	Stats.Entries = Entries.Num();
	Stats.Sections = Sections.Num();
	Stats.ScanMilliseconds = static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);
	bCatalogBuilt = true;

	// Put the focus back on the entry that had it, by name rather than by index: the list may have grown.
	if (!PreviousPath.IsEmpty())
	{
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			for (int32 Row = 0; Row < Sections[SectionIndex].EntryIndices.Num(); ++Row)
			{
				const int32 EntryIndex = Sections[SectionIndex].EntryIndices[Row];
				if (Entries.IsValidIndex(EntryIndex) && Entries[EntryIndex]->GetPath() == PreviousPath)
				{
					SelectedSection = SectionIndex;
					SelectedRow = Row;
					SelectedParam = 0;
				}
			}
		}
	}

#if WITH_EDITOR
	if (UConsoleDeckSettings::Get().bAutoBakeInEditor && !Stats.bFromBakedCatalog)
	{
		BakeCatalog();
	}
#endif
}

int32 UConsoleDeckSubsystem::ScanLoadedClasses(TArray<TObjectPtr<UConsoleDeckEntry>>& OutEntries)
{
#if WITH_METADATA
	const UConsoleDeckSettings& Settings = UConsoleDeckSettings::Get();

	TArray<UClass*> Candidates;

	// The classes somebody named explicitly are loaded first. A class nobody has touched has no UClass in
	// memory, and no walk over what is loaded can find what is not there - that is the whole reason this
	// list exists, and why a subsystem or a GameMode that has not been used yet still turns up in the menu.
	for (const FSoftClassPath& ClassPath : Settings.ScanClasses)
	{
		if (UClass* LoadedClass = ClassPath.TryLoadClass<UObject>())
		{
			Candidates.AddUnique(LoadedClass);
		}
		else if (!ClassPath.ToString().IsEmpty())
		{
			UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: could not load '%s' from the scan list."), *ClassPath.ToString());
		}
	}

	if (Settings.bScanAllLoadedClasses)
	{
		// The expensive line in the whole plugin, and the reason the scan runs once at startup rather than
		// every time the menu opens. TObjectIterator<UClass> walks every loaded class; on a large project
		// with thousands of Blueprints in memory that is milliseconds, not microseconds, and milliseconds
		// spent while somebody is holding a button down is what makes a tool feel broken. The measurement
		// ends up in the header of the menu, so nobody has to take this comment on trust.
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* Class = *ClassIt;

			if (!IsValid(Class) || Class->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				continue;
			}

			// The editor keeps skeleton and reinstanced copies of every Blueprint class alive. They carry
			// the same metadata as the real thing and would double every Blueprint entry in the list.
			const FString ClassName = Class->GetName();
			if (ClassName.StartsWith(TEXT("SKEL_")) || ClassName.StartsWith(TEXT("REINST_"))
				|| ClassName.StartsWith(TEXT("TRASHCLASS_")) || ClassName.StartsWith(TEXT("PLACEHOLDER-CLASS")))
			{
				continue;
			}

			if (!Settings.bIncludeBlueprintClasses && !Class->HasAnyClassFlags(CLASS_Native))
			{
				continue;
			}

			if (Settings.ScanPathFilters.Num() > 0)
			{
				const FString Path = Class->GetPathName();
				const bool bAllowed = Settings.ScanPathFilters.ContainsByPredicate([&Path](const FString& Filter)
				{
					return !Filter.IsEmpty() && Path.StartsWith(Filter, ESearchCase::IgnoreCase);
				});

				if (!bAllowed)
				{
					continue;
				}
			}

			Candidates.AddUnique(Class);
		}
	}

	TSet<FString> Seen;

	for (UClass* Class : Candidates)
	{
		// ExcludeSuper: a function is filed under the class that declares it, once. Without this, a base
		// class with one marked function would produce one entry for every subclass in the project, and the
		// menu would be a wall of the same name.
		for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;

			const FString* MetaValue = Function->FindMetaData(ConsoleDeck::MetaKey);
			if (!MetaValue)
			{
				continue;
			}

			const FString Key = FString::Printf(TEXT("%s:%s"), *Class->GetPathName(), *Function->GetName());
			if (Seen.Contains(Key))
			{
				continue;
			}
			Seen.Add(Key);

			const FString* ArgsValue = Function->FindMetaData(ConsoleDeck::ArgsMetaKey);
			if (UConsoleDeckEntry* Entry = MakeEntry(Class, Function, *MetaValue, ArgsValue ? *ArgsValue : FString()))
			{
				OutEntries.Add(Entry);
			}
		}
	}

	return Candidates.Num();
#else
	// Cooked build: WITH_METADATA follows WITH_EDITORONLY_DATA, so there is nothing to scan. Not an error,
	// and not a reason to be quiet about it - the baked catalog takes over and the header says so.
	(void)OutEntries;
	return 0;
#endif
}

int32 UConsoleDeckSubsystem::LoadBakedCatalog(TArray<TObjectPtr<UConsoleDeckEntry>>& OutEntries)
{
	const UConsoleDeckSettings& Settings = UConsoleDeckSettings::Get();

	int32 Loaded = 0;

	for (const FConsoleDeckBakedEntry& Baked : Settings.BakedCatalog)
	{
		UClass* Class = FSoftClassPath(Baked.ClassPath).TryLoadClass<UObject>();
		UFunction* Function = Class ? Class->FindFunctionByName(Baked.FunctionName) : nullptr;

		UConsoleDeckEntry* Entry = NewObject<UConsoleDeckEntry>(this);
		Entry->Section = Baked.Section.IsEmpty() ? FString(ConsoleDeck::DefaultSection) : Baked.Section;
		Entry->DisplayName = Baked.DisplayName.IsEmpty() ? Baked.FunctionName.ToString() : Baked.DisplayName;
		Entry->ClassPath = Baked.ClassPath;
		Entry->TargetClass = Class;
		Entry->Function = Function;
		Entry->ArgSpec = Baked.Args;
		Entry->bFromBakedCatalog = true;

		if (Function)
		{
			bool bSupported = true;
			FString Reason;
			Entry->Params = UConsoleDeckStatics::DescribeFunction(Function, bSupported, Reason);
			Entry->bSignatureSupported = bSupported;
			Entry->SignatureReason = Reason;

			// Limits and presets are metadata too, so they are gone in a cooked build as well. They ride
			// along in the baked line for exactly that reason - without them a slider in the packaged build
			// would have no ends.
			UConsoleDeckStatics::ApplyArgSpec(Baked.Args, Entry->Params);
		}
		else
		{
			Entry->Availability = EConsoleDeckAvailability::NotFound;
			Entry->UnavailableReason = FString::Printf(TEXT("%s has no function %s in this build - rebake the catalog"),
				*Baked.ClassPath, *Baked.FunctionName.ToString());
		}

		OutEntries.Add(Entry);
		++Loaded;
	}

	return Loaded;
}

UConsoleDeckEntry* UConsoleDeckSubsystem::MakeEntry(UClass* OwnerClass, UFunction* Function, const FString& MetaValue, const FString& ArgSpec)
{
	if (!OwnerClass || !Function)
	{
		return nullptr;
	}

	UConsoleDeckEntry* Entry = NewObject<UConsoleDeckEntry>(this);

	FString Section;
	FString DisplayName;
	UConsoleDeckStatics::ParseMeta(MetaValue, Section, DisplayName);

	// meta=(ConsoleDeck="") is a legal thing to write and means "in the menu, name it after the function".
	if (DisplayName.IsEmpty())
	{
		DisplayName = Function->GetName();
	}

	Entry->Section = Section;
	Entry->DisplayName = DisplayName;
	Entry->TargetClass = OwnerClass;
	Entry->ClassPath = OwnerClass->GetPathName();
	Entry->Function = Function;
	Entry->ArgSpec = ArgSpec;

	bool bSupported = true;
	FString Reason;
	Entry->Params = UConsoleDeckStatics::DescribeFunction(Function, bSupported, Reason);
	Entry->bSignatureSupported = bSupported;
	Entry->SignatureReason = Reason;

	UConsoleDeckStatics::ApplyArgSpec(ArgSpec, Entry->Params);

	return Entry;
}

void UConsoleDeckSubsystem::RegroupSections()
{
	TArray<FConsoleDeckEntryView> Views;
	Views.Reserve(Entries.Num());

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FConsoleDeckEntryView& View = Views.AddDefaulted_GetRef();
		View.Section = Entries[Index]->Section;
		View.DisplayName = Entries[Index]->DisplayName;
		View.SourceIndex = Index;
	}

	Sections = UConsoleDeckStatics::BuildSections(Views);

	SelectedSection = Sections.Num() > 0 ? FMath::Clamp(SelectedSection, 0, Sections.Num() - 1) : 0;
	const int32 RowCount = Sections.IsValidIndex(SelectedSection) ? Sections[SelectedSection].EntryIndices.Num() : 0;
	SelectedRow = RowCount > 0 ? FMath::Clamp(SelectedRow, 0, RowCount - 1) : 0;
	SelectedParam = 0;
	ScrollOffset = 0;
}

void UConsoleDeckSubsystem::RefreshAvailability()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;

	int32 Unavailable = 0;
	for (const TObjectPtr<UConsoleDeckEntry>& Entry : Entries)
	{
		if (!Entry->ResolveTarget(World))
		{
			++Unavailable;
		}
	}

	Stats.Unavailable = Unavailable;
}

TArray<UConsoleDeckEntry*> UConsoleDeckSubsystem::GetEntriesForBlueprint() const
{
	TArray<UConsoleDeckEntry*> Result;
	Result.Reserve(Entries.Num());
	for (const TObjectPtr<UConsoleDeckEntry>& Entry : Entries)
	{
		Result.Add(Entry);
	}
	return Result;
}

UConsoleDeckEntry* UConsoleDeckSubsystem::FindEntry(const FString& Path) const
{
	for (const TObjectPtr<UConsoleDeckEntry>& Entry : Entries)
	{
		if (Entry->GetPath().Equals(Path, ESearchCase::IgnoreCase))
		{
			return Entry;
		}
	}

	// A bare name works too, as long as it is not ambiguous: typing "Player|God Mode" into a console that
	// treats the bar specially is more trouble than it is worth.
	UConsoleDeckEntry* NameMatch = nullptr;
	for (const TObjectPtr<UConsoleDeckEntry>& Entry : Entries)
	{
		if (Entry->DisplayName.Equals(Path, ESearchCase::IgnoreCase))
		{
			if (NameMatch)
			{
				return nullptr;
			}
			NameMatch = Entry;
		}
	}

	return NameMatch;
}

void UConsoleDeckSubsystem::DumpToLog() const
{
	UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck: %d entry(s) in %d section(s), scan %.2f ms, source %s."),
		Stats.Entries, Stats.Sections, Stats.ScanMilliseconds,
		Stats.bFromBakedCatalog ? TEXT("baked catalog") : TEXT("live metadata"));

	for (const FConsoleDeckSection& Section : Sections)
	{
		UE_LOG(LogConsoleDeck, Display, TEXT("  [%s]"), *Section.Name);

		for (const int32 EntryIndex : Section.EntryIndices)
		{
			if (!Entries.IsValidIndex(EntryIndex))
			{
				continue;
			}

			const UConsoleDeckEntry* Entry = Entries[EntryIndex];

			TStringBuilder<512> Line;
			Line.Appendf(TEXT("    %-28s %-34s"), *Entry->DisplayName, *Entry->GetValueText());
			Line.Appendf(TEXT(" %s::%s"), *Entry->ClassPath, Entry->Function ? *Entry->Function->GetName() : TEXT("?"));

			if (!Entry->IsReady())
			{
				Line.Appendf(TEXT("  - unavailable: %s"), *Entry->UnavailableReason);
			}

			UE_LOG(LogConsoleDeck, Display, TEXT("%s"), Line.ToString());
		}
	}
}

int32 UConsoleDeckSubsystem::BakeCatalog()
{
#if WITH_EDITOR
	UConsoleDeckSettings* Settings = GetMutableDefault<UConsoleDeckSettings>();
	check(Settings);

	TArray<FConsoleDeckBakedEntry> Baked;
	Baked.Reserve(Entries.Num());

	for (const TObjectPtr<UConsoleDeckEntry>& Entry : Entries)
	{
		if (Entry->bFromBakedCatalog || !Entry->Function)
		{
			continue;
		}

		FConsoleDeckBakedEntry& Line = Baked.AddDefaulted_GetRef();
		Line.ClassPath = Entry->ClassPath;
		Line.FunctionName = Entry->Function->GetFName();
		Line.Section = Entry->Section;
		Line.DisplayName = Entry->DisplayName;
		Line.Args = Entry->ArgSpec;
	}

	// Sorted, so that two people who rebuild the catalog on two machines produce the same file and source
	// control has nothing to argue about.
	Baked.Sort([](const FConsoleDeckBakedEntry& A, const FConsoleDeckBakedEntry& B)
	{
		const int32 BySection = A.Section.Compare(B.Section, ESearchCase::IgnoreCase);
		if (BySection != 0)
		{
			return BySection < 0;
		}
		return A.DisplayName.Compare(B.DisplayName, ESearchCase::IgnoreCase) < 0;
	});

	// Only write when something actually changed. Otherwise every press of Play would touch
	// DefaultGame.ini, and a config file that changes on its own is a config file people stop trusting.
	bool bChanged = Baked.Num() != Settings->BakedCatalog.Num();
	for (int32 Index = 0; !bChanged && Index < Baked.Num(); ++Index)
	{
		const FConsoleDeckBakedEntry& A = Baked[Index];
		const FConsoleDeckBakedEntry& B = Settings->BakedCatalog[Index];
		bChanged = A.ClassPath != B.ClassPath || A.FunctionName != B.FunctionName
			|| A.Section != B.Section || A.DisplayName != B.DisplayName || A.Args != B.Args;
	}

	if (bChanged)
	{
		Settings->BakedCatalog = MoveTemp(Baked);
		Settings->TryUpdateDefaultConfigFile();
		UE_LOG(LogConsoleDeck, Log, TEXT("ConsoleDeck: baked %d entry(s) into the project settings."), Settings->BakedCatalog.Num());
	}

	return Settings->BakedCatalog.Num();
#else
	UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: baking needs the editor - metadata only exists there."));
	return -1;
#endif
}

// ------------------------------------------------------------------------------------------------------
// Calling
// ------------------------------------------------------------------------------------------------------

bool UConsoleDeckSubsystem::Invoke(UConsoleDeckEntry* Entry)
{
	if (!Entry)
	{
		return false;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const bool bSucceeded = Entry->Invoke(World);

	StatusLine = Entry->LastResult;
	bStatusIsError = !bSucceeded;

	if (UConsoleDeckSettings::Get().bLogInvocations)
	{
		UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck: %s -> %s"), *Entry->GetPath(), *Entry->LastResult);
	}

	OnEntryInvoked.Broadcast(Entry, bSucceeded, Entry->LastResult);
	return bSucceeded;
}

bool UConsoleDeckSubsystem::InvokeByPath(const FString& Path, const TArray<FString>& Args, FString& OutResult)
{
	UConsoleDeckEntry* Entry = FindEntry(Path);
	if (!Entry)
	{
		OutResult = FString::Printf(TEXT("no entry called '%s'"), *Path);
		StatusLine = OutResult;
		bStatusIsError = true;
		return false;
	}

	if (Args.Num() > 0)
	{
		TArray<FString> Coerced;
		FString Error;

		// The same coercion the menu uses, so a wrong argument on the console is refused with the same
		// sentence rather than turned into a zero somewhere deep in a parameter buffer.
		if (!UConsoleDeckStatics::CoerceArgs(Entry->Params, Args, Coerced, Error))
		{
			OutResult = Error;
			StatusLine = Error;
			bStatusIsError = true;
			return false;
		}

		for (int32 Index = 0; Index < Coerced.Num() && Index < Entry->Params.Num(); ++Index)
		{
			Entry->Params[Index].Value = Coerced[Index];
		}
	}

	const bool bSucceeded = Invoke(Entry);
	OutResult = Entry->LastResult;
	return bSucceeded;
}

// ------------------------------------------------------------------------------------------------------
// The lock
// ------------------------------------------------------------------------------------------------------

bool UConsoleDeckSubsystem::IsUnlocked() const
{
	const UConsoleDeckSettings& Settings = UConsoleDeckSettings::Get();

#if UE_BUILD_SHIPPING
	// Shipping. The deck is compiled in - deliberately, because the bugs that only happen in Shipping are
	// exactly the ones somebody needs a cheat menu for - but it stays shut until it is told otherwise. An
	// empty unlock word means it cannot be opened at all, because a setting somebody forgot has to fail the
	// safe way round.
	if (Settings.bAllowInShippingWithoutUnlock)
	{
		return true;
	}
	return bUnlockedThisSession;
#else
	if (Settings.bRequireUnlockOutsideShipping)
	{
		return bUnlockedThisSession;
	}
	return true;
#endif
}

bool UConsoleDeckSubsystem::TryUnlock(const FString& Word)
{
	const UConsoleDeckSettings& Settings = UConsoleDeckSettings::Get();

	if (Settings.UnlockWord.IsEmpty())
	{
		UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: no unlock word is set in the project settings, so there is nothing to unlock with."));
		return IsUnlocked();
	}

	if (Word.Equals(Settings.UnlockWord, ESearchCase::CaseSensitive))
	{
		bUnlockedThisSession = true;
		UE_LOG(LogConsoleDeck, Display, TEXT("ConsoleDeck: unlocked."));
		return true;
	}

	// Wrong words are counted, not explained. There is no hint to give that would not also be a hint to
	// somebody who should not be here.
	UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: that is not the word."));
	return IsUnlocked();
}

void UConsoleDeckSubsystem::Lock()
{
	bUnlockedThisSession = false;
	if (bOpen)
	{
		Close();
	}
}

// ------------------------------------------------------------------------------------------------------
// Open, close
// ------------------------------------------------------------------------------------------------------

bool UConsoleDeckSubsystem::Open()
{
	if (bOpen)
	{
		return true;
	}

	if (!IsUnlocked())
	{
		UE_LOG(LogConsoleDeck, Warning, TEXT("ConsoleDeck: locked. Use ConsoleDeck.Unlock <word> or allow it in the project settings."));
		return false;
	}

	if (!bCatalogBuilt)
	{
		Rebuild();
	}
	else
	{
		// The catalog does not change when a level does, but the instances behind it do. Looking them up
		// again on every open is what keeps the greyed-out reasons true.
		RefreshAvailability();
	}

	bOpen = true;
	bTyping = false;
	TypedText.Reset();
	HeldSeconds.Reset();
	RepeatCountdown.Reset();

	ApplyInputSuppression(true);
	OnVisibilityChanged.Broadcast(true);
	return true;
}

void UConsoleDeckSubsystem::Close()
{
	if (!bOpen)
	{
		return;
	}

	bOpen = false;
	bTyping = false;
	TypedText.Reset();

	ApplyInputSuppression(false);
	OnVisibilityChanged.Broadcast(false);
}

bool UConsoleDeckSubsystem::Toggle()
{
	if (bOpen)
	{
		Close();
		return false;
	}

	return Open();
}

void UConsoleDeckSubsystem::ApplyInputSuppression(bool bDeckOpen)
{
	const UConsoleDeckSettings& Settings = UConsoleDeckSettings::Get();

	if (APlayerController* PC = GetLocalPlayerController())
	{
		if (Settings.bBlockMoveAndLookWhileOpen)
		{
			// SetIgnoreMoveInput and SetIgnoreLookInput count, they do not toggle, so the pair has to stay
			// balanced. Anything else and a project that also suppresses input somewhere ends up with a
			// pawn that never moves again after the menu is closed.
			PC->SetIgnoreMoveInput(bDeckOpen);
			PC->SetIgnoreLookInput(bDeckOpen);
		}
	}

	if (Settings.bPauseGameWhileOpen)
	{
		UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
		if (World)
		{
			if (bDeckOpen && !UGameplayStatics::IsGamePaused(World))
			{
				bPausedByDeck = UGameplayStatics::SetGamePaused(World, true);
			}
			else if (!bDeckOpen && bPausedByDeck)
			{
				// Only unpause what this deck paused. A game that was already paused when the menu opened
				// stays paused when it closes.
				UGameplayStatics::SetGamePaused(World, false);
				bPausedByDeck = false;
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------
// Navigation
// ------------------------------------------------------------------------------------------------------

UConsoleDeckEntry* UConsoleDeckSubsystem::GetFocusedEntry() const
{
	if (!Sections.IsValidIndex(SelectedSection))
	{
		return nullptr;
	}

	const TArray<int32>& Rows = Sections[SelectedSection].EntryIndices;
	if (!Rows.IsValidIndex(SelectedRow))
	{
		return nullptr;
	}

	const int32 EntryIndex = Rows[SelectedRow];
	return Entries.IsValidIndex(EntryIndex) ? Entries[EntryIndex].Get() : nullptr;
}

void UConsoleDeckSubsystem::MoveSelection(int32 Direction)
{
	if (!Sections.IsValidIndex(SelectedSection))
	{
		return;
	}

	const int32 Count = Sections[SelectedSection].EntryIndices.Num();
	if (Count == 0)
	{
		return;
	}

	SelectedRow = ((SelectedRow + Direction) % Count + Count) % Count;
	SelectedParam = 0;

	const int32 VisibleRows = FMath::Max(1, UConsoleDeckSettings::Get().VisibleRows);
	if (SelectedRow < ScrollOffset)
	{
		ScrollOffset = SelectedRow;
	}
	else if (SelectedRow >= ScrollOffset + VisibleRows)
	{
		ScrollOffset = SelectedRow - VisibleRows + 1;
	}
	ScrollOffset = FMath::Clamp(ScrollOffset, 0, FMath::Max(0, Count - VisibleRows));
}

void UConsoleDeckSubsystem::MoveSection(int32 Direction)
{
	if (Sections.Num() == 0)
	{
		return;
	}

	SelectedSection = ((SelectedSection + Direction) % Sections.Num() + Sections.Num()) % Sections.Num();
	SelectedRow = 0;
	SelectedParam = 0;
	ScrollOffset = 0;
}

void UConsoleDeckSubsystem::MoveParam(int32 Direction)
{
	UConsoleDeckEntry* Entry = GetFocusedEntry();
	if (!Entry || Entry->Params.Num() <= 1)
	{
		return;
	}

	const int32 Count = Entry->Params.Num();
	SelectedParam = ((SelectedParam + Direction) % Count + Count) % Count;
}

void UConsoleDeckSubsystem::AdjustValue(int32 Direction, float StepScale)
{
	UConsoleDeckEntry* Entry = GetFocusedEntry();
	if (!Entry || Entry->Params.Num() == 0)
	{
		return;
	}

	SelectedParam = FMath::Clamp(SelectedParam, 0, Entry->Params.Num() - 1);
	Entry->AdjustParam(SelectedParam, Direction, StepScale);

	StatusLine = FString::Printf(TEXT("%s %s"), *Entry->DisplayName, *Entry->GetValueText());
	bStatusIsError = false;
}

void UConsoleDeckSubsystem::ActivateSelected()
{
	UConsoleDeckEntry* Entry = GetFocusedEntry();
	if (!Entry)
	{
		return;
	}

	if (!Entry->bSignatureSupported)
	{
		StatusLine = Entry->SignatureReason;
		bStatusIsError = true;
		return;
	}

	// A switch with one bool flips and then runs, in one press. Asking somebody to press left and then A to
	// turn God Mode on would be one press too many for the single most used entry in any cheat menu.
	if (Entry->Params.Num() == 1 && Entry->Params[0].Kind == EConsoleDeckParamKind::Bool)
	{
		Entry->AdjustParam(0, 1);
	}

	// A text parameter with no presets needs a keyboard, and that means going into typing mode rather than
	// calling the function with whatever was in the field.
	const int32 FirstParam = Entry->GetFirstEditableParam();
	if (Entry->Params.Num() == 1 && FirstParam != INDEX_NONE)
	{
		const FConsoleDeckParam& Param = Entry->Params[FirstParam];
		const bool bIsFreeText = (Param.Kind == EConsoleDeckParamKind::String || Param.Kind == EConsoleDeckParamKind::Name
			|| Param.Kind == EConsoleDeckParamKind::Text) && Param.Options.Num() == 0;

		if (bIsFreeText && !bTyping)
		{
			SelectedParam = FirstParam;
			BeginTyping();
			return;
		}
	}

	Invoke(Entry);
}

void UConsoleDeckSubsystem::ResetSelected()
{
	if (UConsoleDeckEntry* Entry = GetFocusedEntry())
	{
		Entry->ResetParams();
		StatusLine = FString::Printf(TEXT("%s back to defaults"), *Entry->DisplayName);
		bStatusIsError = false;
	}
}

// ------------------------------------------------------------------------------------------------------
// Typing
// ------------------------------------------------------------------------------------------------------

void UConsoleDeckSubsystem::BeginTyping()
{
	UConsoleDeckEntry* Entry = GetFocusedEntry();
	if (!Entry || !Entry->Params.IsValidIndex(SelectedParam))
	{
		return;
	}

	bTyping = true;
	TypedText = Entry->Params[SelectedParam].Value;
	StatusLine = TEXT("typing - Enter keeps it, Escape throws it away");
	bStatusIsError = false;
}

void UConsoleDeckSubsystem::CommitTyping()
{
	if (!bTyping)
	{
		return;
	}

	FString Error;
	if (!SetFocusedParamText(TypedText, Error))
	{
		StatusLine = Error;
		bStatusIsError = true;
	}

	bTyping = false;
	TypedText.Reset();
}

void UConsoleDeckSubsystem::CancelTyping()
{
	bTyping = false;
	TypedText.Reset();
	StatusLine.Reset();
	bStatusIsError = false;
}

void UConsoleDeckSubsystem::AppendTypedText(const FString& Text)
{
	if (bTyping)
	{
		TypedText += Text;
	}
}

void UConsoleDeckSubsystem::BackspaceTypedText()
{
	if (bTyping && TypedText.Len() > 0)
	{
		TypedText.LeftChopInline(1, EAllowShrinking::No);
	}
}

bool UConsoleDeckSubsystem::SetFocusedParamText(const FString& Text, FString& OutError)
{
	UConsoleDeckEntry* Entry = GetFocusedEntry();
	if (!Entry || !Entry->Params.IsValidIndex(SelectedParam))
	{
		OutError = TEXT("nothing is focused");
		return false;
	}

	FString Coerced;
	if (!UConsoleDeckStatics::CoerceValue(Entry->Params[SelectedParam], Text, Coerced, OutError))
	{
		return false;
	}

	Entry->Params[SelectedParam].Value = Coerced;
	StatusLine = FString::Printf(TEXT("%s %s"), *Entry->DisplayName, *Entry->GetValueText());
	bStatusIsError = false;
	return true;
}

// ------------------------------------------------------------------------------------------------------
// Input
// ------------------------------------------------------------------------------------------------------

bool UConsoleDeckSubsystem::PollAction(APlayerController* PC, const TArray<FKey>& Keys, float DeltaSeconds, int32 ActionId, bool& bOutRepeating)
{
	bOutRepeating = false;

	float& Held = HeldSeconds.FindOrAdd(ActionId);
	float& Countdown = RepeatCountdown.FindOrAdd(ActionId);

	if (!ConsoleDeckDraw::AnyKeyDown(PC, Keys))
	{
		Held = 0.0f;
		Countdown = 0.0f;
		return false;
	}

	const UConsoleDeckSettings& Settings = UConsoleDeckSettings::Get();

	bool bTriggered = false;

	if (Held <= 0.0f)
	{
		// The press itself always counts, before any waiting: a tap has to move exactly one step.
		bTriggered = true;
		Countdown = Settings.RepeatDelaySeconds;
	}
	else
	{
		Countdown -= DeltaSeconds;
		if (Countdown <= 0.0f)
		{
			bTriggered = true;
			Countdown = Settings.RepeatRateSeconds;
			bOutRepeating = true;
		}
	}

	Held += FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
	return bTriggered;
}

void UConsoleDeckSubsystem::PollTypedCharacters(APlayerController* PC)
{
	if (PC->WasInputKeyJustPressed(EKeys::BackSpace))
	{
		BackspaceTypedText();
		return;
	}

	const bool bShift = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);

	for (const ConsoleDeckDraw::FTypedKey& Typed : ConsoleDeckDraw::GetTypedKeys())
	{
		if (PC->WasInputKeyJustPressed(Typed.Key))
		{
			const TCHAR Character = bShift ? FChar::ToUpper(Typed.Character) : Typed.Character;
			AppendTypedText(FString::Chr(Character));
		}
	}
}

void UConsoleDeckSubsystem::TickInput(APlayerController* PC, float DeltaSeconds)
{
	if (!PC)
	{
		return;
	}

	const UConsoleDeckSettings& Settings = UConsoleDeckSettings::Get();

	// The open key is polled whether the deck is up or not - it is the one binding that has to work from a
	// cold start, with nothing else set up.
	if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.OpenKeys))
	{
		if (!bOpen)
		{
			Open();
			return;
		}

		if (Settings.bOpenKeyAlsoCloses)
		{
			Close();
			return;
		}
	}

	if (!bOpen)
	{
		return;
	}

	if (bTyping)
	{
		if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.CloseKeys))
		{
			CancelTyping();
			return;
		}
		if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.ActivateKeys))
		{
			CommitTyping();
			return;
		}

		PollTypedCharacters(PC);
		return;
	}

	if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.CloseKeys))
	{
		Close();
		return;
	}

	if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.RebuildKeys))
	{
		Rebuild();
		StatusLine = FString::Printf(TEXT("rebuilt: %d entry(s) in %.2f ms"), Stats.Entries, Stats.ScanMilliseconds);
		bStatusIsError = false;
		return;
	}

	if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.ResetKeys))
	{
		ResetSelected();
		return;
	}

	if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.ActivateKeys))
	{
		ActivateSelected();
		return;
	}

	if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.PrevSectionKeys))
	{
		MoveSection(-1);
	}
	if (ConsoleDeckDraw::AnyKeyJustPressed(PC, Settings.NextSectionKeys))
	{
		MoveSection(1);
	}

	bool bRepeating = false;

	if (PollAction(PC, Settings.UpKeys, DeltaSeconds, static_cast<int32>(ConsoleDeckDraw::EAction::Up), bRepeating))
	{
		MoveSelection(-1);
	}
	if (PollAction(PC, Settings.DownKeys, DeltaSeconds, static_cast<int32>(ConsoleDeckDraw::EAction::Down), bRepeating))
	{
		MoveSelection(1);
	}

	const float StepScale = Settings.RepeatStepMultiplier;

	if (PollAction(PC, Settings.LeftKeys, DeltaSeconds, static_cast<int32>(ConsoleDeckDraw::EAction::Left), bRepeating))
	{
		AdjustValue(-1, bRepeating ? StepScale : 1.0f);
	}
	if (PollAction(PC, Settings.RightKeys, DeltaSeconds, static_cast<int32>(ConsoleDeckDraw::EAction::Right), bRepeating))
	{
		AdjustValue(1, bRepeating ? StepScale : 1.0f);
	}
}

// ------------------------------------------------------------------------------------------------------
// Drawing
// ------------------------------------------------------------------------------------------------------

void UConsoleDeckSubsystem::DrawDeck(UCanvas* Canvas)
{
	if (!bOpen || !Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	using namespace ConsoleDeckDraw;

	const UConsoleDeckSettings& Settings = UConsoleDeckSettings::Get();
	const float Scale = FMath::Clamp(Settings.PanelScale, 0.5f, 6.0f);
	const float LineHeight = 12.0f * Scale;
	const float Width = Settings.PanelWidth * Scale;
	const float Left = static_cast<float>(Settings.PanelPosition.X);
	const float Top = static_cast<float>(Settings.PanelPosition.Y);
	const float Padding = 8.0f * Scale;

	const FConsoleDeckSection* Section = Sections.IsValidIndex(SelectedSection) ? &Sections[SelectedSection] : nullptr;
	const int32 RowCount = Section ? Section->EntryIndices.Num() : 0;
	const int32 VisibleRows = FMath::Min(FMath::Max(1, Settings.VisibleRows), FMath::Max(1, RowCount));

	const int32 HeaderLines = (Settings.bShowStatsHeader ? 2 : 1) + 1;
	const int32 FooterLines = 1 + (Settings.bShowKeyHintFooter ? 1 : 0);
	const float PanelHeight = (HeaderLines + VisibleRows + FooterLines) * LineHeight + Padding * 2.0f;

	FCanvasTileItem Background(
		FVector2D(Left - Padding, Top - Padding),
		GWhiteTexture,
		FVector2D(Width + Padding * 2.0f, PanelHeight),
		FLinearColor(0.02f, 0.02f, 0.04f, 0.82f));
	Background.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Background);

	// One place that knows how text is drawn, so every line shares the scale and returns its own width -
	// which is what makes a section bar with a highlighted tab possible without a fixed-width font.
	auto DrawTextAt = [&](const FString& Text, float X, float Y, const FLinearColor& Colour) -> float
	{
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Colour);
		Item.Scale = FVector2D(Scale, Scale);
		Canvas->DrawItem(Item);

		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		Canvas->TextSize(Font, Text, TextWidth, TextHeight, Scale, Scale);
		return TextWidth;
	};

	float Y = Top;

	// Title.
	const FString LockText = IsUnlocked() ? FString() : TEXT("  (locked)");
	DrawTextAt(FString::Printf(TEXT("ConsoleDeck%s"), *LockText), Left, Y, Title);
	Y += LineHeight;

	// The header line. This is the sentence the store screenshot is built around, and it is the honest one:
	// what was found, how long it took to find it, and how much of it cannot be used right now.
	if (Settings.bShowStatsHeader)
	{
		TStringBuilder<256> Header;
		Header.Appendf(TEXT("%d entries in %d sections, scan %.1f ms"), Stats.Entries, Stats.Sections, Stats.ScanMilliseconds);
		if (Stats.Unavailable > 0)
		{
			Header.Appendf(TEXT(" | %d unavailable"), Stats.Unavailable);
		}
		Header.Append(Stats.bFromBakedCatalog ? TEXT(" | baked catalog") : TEXT(" | live metadata"));
		DrawTextAt(Header.ToString(), Left, Y, Faint);
		Y += LineHeight;
	}

	// The section bar.
	{
		float X = Left;
		for (int32 Index = 0; Index < Sections.Num(); ++Index)
		{
			const bool bActive = Index == SelectedSection;
			const FString Text = bActive
				? FString::Printf(TEXT("[%s]"), *Sections[Index].Name)
				: FString::Printf(TEXT(" %s "), *Sections[Index].Name);

			X += DrawTextAt(Text, X, Y, bActive ? Accent : Faint) + 6.0f * Scale;

			if (X > Left + Width)
			{
				break;
			}
		}
		Y += LineHeight;
	}

	if (!Section || RowCount == 0)
	{
		DrawTextAt(TEXT("nothing marked yet - add meta=(ConsoleDeck=\"Player|God Mode\") to a UFUNCTION"), Left, Y, Faint);
		return;
	}

	const int32 FirstRow = FMath::Clamp(ScrollOffset, 0, FMath::Max(0, RowCount - VisibleRows));
	const float NameColumn = Left + 8.0f * Scale;
	const float ValueColumn = Left + Width * 0.52f;

	for (int32 Row = FirstRow; Row < FMath::Min(FirstRow + VisibleRows, RowCount); ++Row)
	{
		const int32 EntryIndex = Section->EntryIndices[Row];
		if (!Entries.IsValidIndex(EntryIndex))
		{
			continue;
		}

		const UConsoleDeckEntry* Entry = Entries[EntryIndex];
		const bool bFocused = Row == SelectedRow;
		const bool bUsable = Entry->IsReady();

		if (bFocused)
		{
			// The focus frame. On a pad there is no cursor, so the only way to know what a button press is
			// about to do is to see the row it will do it to.
			FCanvasTileItem Highlight(
				FVector2D(Left - 4.0f * Scale, Y - 1.0f * Scale),
				GWhiteTexture,
				FVector2D(Width + 8.0f * Scale, LineHeight),
				FLinearColor(0.20f, 0.35f, 0.55f, 0.55f));
			Highlight.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Highlight);
		}

		const FLinearColor NameColour = bUsable ? (bFocused ? Body : Body * 0.92f) : Faint;
		DrawTextAt(FString::Printf(TEXT("%s%s"), bFocused ? TEXT("> ") : TEXT("  "), *Entry->DisplayName), NameColumn, Y, NameColour);

		if (bUsable)
		{
			const FString ValueText = (bTyping && bFocused)
				? FString::Printf(TEXT("%s_"), *TypedText)
				: Entry->GetValueText();
			DrawTextAt(ValueText, ValueColumn, Y, bFocused ? Accent : Body);
		}
		else
		{
			// The greyed-out entry keeps its place and says why. That sentence is the honest part of this
			// plugin: a row that vanished would look like the deck lost the function.
			DrawTextAt(Entry->UnavailableReason, ValueColumn, Y, Faint);
		}

		Y += LineHeight;
	}

	// Status line: what the last press did, or what stopped it.
	if (!StatusLine.IsEmpty())
	{
		DrawTextAt(StatusLine, NameColumn, Y, bStatusIsError ? Bad : Good);
	}
	else if (RowCount > VisibleRows)
	{
		DrawTextAt(FString::Printf(TEXT("%d of %d"), SelectedRow + 1, RowCount), NameColumn, Y, Faint);
	}
	Y += LineHeight;

	if (Settings.bShowKeyHintFooter)
	{
		const FString Hint = bTyping
			? TEXT("type  |  Enter keep  |  Esc discard")
			: TEXT("D-Pad move  |  LB/RB section  |  Left/Right value  |  A run  |  X rebuild  |  Y reset  |  B close");
		DrawTextAt(Hint, NameColumn, Y, Faint);
	}
}

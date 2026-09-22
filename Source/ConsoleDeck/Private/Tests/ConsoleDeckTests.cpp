// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "ConsoleDeckEntry.h"
#include "ConsoleDeckStatics.h"
#include "ConsoleDeckTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ConsoleDeckTests
{
	// CommandletContext as well as EditorContext: what is tested here is what a packaged build does with a
	// typed argument, and a test that only runs when somebody has the editor open is a test that will not
	// be there on the build server when it matters.
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter;

	FConsoleDeckEntryView MakeView(const TCHAR* Section, const TCHAR* Name, int32 SourceIndex)
	{
		FConsoleDeckEntryView View;
		View.Section = Section;
		View.DisplayName = Name;
		View.SourceIndex = SourceIndex;
		return View;
	}

	FConsoleDeckParam MakeNumber(const TCHAR* Name, EConsoleDeckParamKind Kind, double Min, double Max)
	{
		FConsoleDeckParam Param;
		Param.Name = FName(Name);
		Param.Kind = Kind;
		Param.bHasMin = true;
		Param.Min = Min;
		Param.bHasMax = true;
		Param.Max = Max;
		Param.Value = TEXT("0");
		Param.DefaultValue = TEXT("0");
		return Param;
	}
}

//
// (1) ParseMeta. Section and name, and somewhere sensible to put an entry that names no section.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConsoleDeckParseMetaTest,
	"ConsoleDeck.Logic.ParseMetaSplitsSectionAndFallsBackToGeneral",
	ConsoleDeckTests::TestFlags)

bool FConsoleDeckParseMetaTest::RunTest(const FString& Parameters)
{
	FString Section;
	FString Name;

	UConsoleDeckStatics::ParseMeta(TEXT("Player|God Mode"), Section, Name);
	TestEqual(TEXT("section before the bar"), Section, FString(TEXT("Player")));
	TestEqual(TEXT("name after the bar"), Name, FString(TEXT("God Mode")));

	// Whitespace around a metadata value is invisible in the header it was typed in and must not become a
	// section called " Player ".
	UConsoleDeckStatics::ParseMeta(TEXT("  World  |  Spawn Wave  "), Section, Name);
	TestEqual(TEXT("section is trimmed"), Section, FString(TEXT("World")));
	TestEqual(TEXT("name is trimmed"), Name, FString(TEXT("Spawn Wave")));

	// No bar at all. The entry still has to be reachable, so it goes to General rather than nowhere.
	UConsoleDeckStatics::ParseMeta(TEXT("Toggle Wireframe"), Section, Name);
	TestEqual(TEXT("no bar means General"), Section, FString(TEXT("General")));
	TestEqual(TEXT("the whole value is the name"), Name, FString(TEXT("Toggle Wireframe")));

	// A bar with nothing in front of it is the same case written differently.
	UConsoleDeckStatics::ParseMeta(TEXT("|Orphan"), Section, Name);
	TestEqual(TEXT("empty section means General"), Section, FString(TEXT("General")));
	TestEqual(TEXT("the name survives"), Name, FString(TEXT("Orphan")));

	// Only the first bar splits: a name containing one is kept whole instead of being cut in three.
	UConsoleDeckStatics::ParseMeta(TEXT("World|Spawn|Wave"), Section, Name);
	TestEqual(TEXT("only the first bar splits"), Section, FString(TEXT("World")));
	TestEqual(TEXT("the rest stays together"), Name, FString(TEXT("Spawn|Wave")));

	// An empty value is legal and means "name it after the function". ParseMeta hands back an empty name
	// and the caller substitutes; what it must not do is invent one.
	UConsoleDeckStatics::ParseMeta(FString(), Section, Name);
	TestEqual(TEXT("an empty value still lands in General"), Section, FString(TEXT("General")));
	TestTrue(TEXT("an empty value gives an empty name"), Name.IsEmpty());

	return true;
}

//
// (2) BuildSections. Same section, one tab - and the same catalog produces the same menu every time.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConsoleDeckBuildSectionsTest,
	"ConsoleDeck.Logic.BuildSectionsGroupsAndIsStable",
	ConsoleDeckTests::TestFlags)

bool FConsoleDeckBuildSectionsTest::RunTest(const FString& Parameters)
{
	using namespace ConsoleDeckTests;

	TArray<FConsoleDeckEntryView> Views;
	Views.Add(MakeView(TEXT("World"), TEXT("Spawn Cubes"), 0));
	Views.Add(MakeView(TEXT("Player"), TEXT("Speed"), 1));
	Views.Add(MakeView(TEXT("player"), TEXT("God Mode"), 2));
	Views.Add(MakeView(TEXT("World"), TEXT("Time Of Day"), 3));
	Views.Add(MakeView(TEXT("Player"), TEXT("Ammo"), 4));

	const TArray<FConsoleDeckSection> Sections = UConsoleDeckStatics::BuildSections(Views);

	if (!TestEqual(TEXT("two sections, not three"), Sections.Num(), 2))
	{
		return false;
	}

	TestEqual(TEXT("sections are alphabetical"), Sections[0].Name, FString(TEXT("Player")));
	TestEqual(TEXT("sections are alphabetical"), Sections[1].Name, FString(TEXT("World")));

	// "player" and "Player" are one section. Two tabs with the same word on them would be a bug somebody
	// spends an afternoon on.
	TestEqual(TEXT("case does not split a section"), Sections[0].EntryIndices.Num(), 3);
	TestEqual(TEXT("the other section keeps its two"), Sections[1].EntryIndices.Num(), 2);

	// Entries inside a section are ordered by name, and the index the caller gave comes back untouched.
	TestEqual(TEXT("Ammo first"), Sections[0].EntryIndices[0], 4);
	TestEqual(TEXT("then God Mode"), Sections[0].EntryIndices[1], 2);
	TestEqual(TEXT("then Speed"), Sections[0].EntryIndices[2], 1);

	// The same catalog found in a different order has to produce the same menu. A class walk gives no
	// promise about the order it finds things in, so this is the property that keeps the menu learnable.
	TArray<FConsoleDeckEntryView> Shuffled;
	Shuffled.Add(Views[4]);
	Shuffled.Add(Views[3]);
	Shuffled.Add(Views[0]);
	Shuffled.Add(Views[2]);
	Shuffled.Add(Views[1]);

	const TArray<FConsoleDeckSection> FromShuffled = UConsoleDeckStatics::BuildSections(Shuffled);

	if (!TestEqual(TEXT("same number of sections"), FromShuffled.Num(), Sections.Num()))
	{
		return false;
	}

	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		TestEqual(TEXT("same section names"), FromShuffled[Index].Name, Sections[Index].Name);
		TestTrue(TEXT("same entries in the same order"), FromShuffled[Index].EntryIndices == Sections[Index].EntryIndices);
	}

	return true;
}

//
// (3) CoerceValue. Converting is the easy half; refusing is the half that matters.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConsoleDeckCoerceTest,
	"ConsoleDeck.Logic.CoerceConvertsAndRefusesInsteadOfGuessing",
	ConsoleDeckTests::TestFlags)

bool FConsoleDeckCoerceTest::RunTest(const FString& Parameters)
{
	FString Value;
	FString Error;

	FConsoleDeckParam Flag;
	Flag.Name = FName(TEXT("bEnabled"));
	Flag.Kind = EConsoleDeckParamKind::Bool;

	TestTrue(TEXT("true is true"), UConsoleDeckStatics::CoerceValue(Flag, TEXT("true"), Value, Error));
	TestEqual(TEXT("true is normalised"), Value, FString(TEXT("true")));
	TestTrue(TEXT("on is true"), UConsoleDeckStatics::CoerceValue(Flag, TEXT("ON"), Value, Error));
	TestEqual(TEXT("on is normalised"), Value, FString(TEXT("true")));
	TestTrue(TEXT("0 is false"), UConsoleDeckStatics::CoerceValue(Flag, TEXT("0"), Value, Error));
	TestEqual(TEXT("0 is normalised"), Value, FString(TEXT("false")));

	// The whole point. "maybe" is not a boolean, and turning it into false would be a lie the person who
	// typed it never gets to see.
	TestFalse(TEXT("maybe is not a boolean"), UConsoleDeckStatics::CoerceValue(Flag, TEXT("maybe"), Value, Error));
	TestTrue(TEXT("and it says so"), !Error.IsEmpty());

	FConsoleDeckParam Count;
	Count.Name = FName(TEXT("Count"));
	Count.Kind = EConsoleDeckParamKind::Int;

	TestTrue(TEXT("12 is a whole number"), UConsoleDeckStatics::CoerceValue(Count, TEXT("12"), Value, Error));
	TestEqual(TEXT("and stays 12"), Value, FString(TEXT("12")));
	TestTrue(TEXT("-3 is a whole number"), UConsoleDeckStatics::CoerceValue(Count, TEXT("-3"), Value, Error));
	TestEqual(TEXT("and stays -3"), Value, FString(TEXT("-3")));

	// FCString::Atoi("twelve") is 0 and FCString::Atoi("12abc") is 12. Both are wrong answers to give a
	// cheat menu, and both are refused here.
	TestFalse(TEXT("twelve is not a whole number"), UConsoleDeckStatics::CoerceValue(Count, TEXT("twelve"), Value, Error));
	TestFalse(TEXT("12abc is not a whole number"), UConsoleDeckStatics::CoerceValue(Count, TEXT("12abc"), Value, Error));
	TestFalse(TEXT("1.5 is not a whole number"), UConsoleDeckStatics::CoerceValue(Count, TEXT("1.5"), Value, Error));

	FConsoleDeckParam Speed;
	Speed.Name = FName(TEXT("Speed"));
	Speed.Kind = EConsoleDeckParamKind::Float;

	TestTrue(TEXT("1.5 is a number"), UConsoleDeckStatics::CoerceValue(Speed, TEXT("1.5"), Value, Error));
	TestEqual(TEXT("and keeps its half"), Value, FString(TEXT("1.5")));
	TestTrue(TEXT("an exponent is a number"), UConsoleDeckStatics::CoerceValue(Speed, TEXT("1e3"), Value, Error));
	TestEqual(TEXT("and means a thousand"), Value, FString(TEXT("1000.0")));
	TestFalse(TEXT("fast is not a number"), UConsoleDeckStatics::CoerceValue(Speed, TEXT("fast"), Value, Error));

	FConsoleDeckParam Mode;
	Mode.Name = FName(TEXT("Mode"));
	Mode.Kind = EConsoleDeckParamKind::Enum;
	Mode.Options = { TEXT("Walking"), TEXT("Falling"), TEXT("Flying") };

	TestTrue(TEXT("a name matches"), UConsoleDeckStatics::CoerceValue(Mode, TEXT("flying"), Value, Error));
	TestEqual(TEXT("and comes back spelled properly"), Value, FString(TEXT("Flying")));
	TestTrue(TEXT("an index matches"), UConsoleDeckStatics::CoerceValue(Mode, TEXT("1"), Value, Error));
	TestEqual(TEXT("index one is the second one"), Value, FString(TEXT("Falling")));

	// Out of range is refused rather than wrapped: an enum with three entries told to take number nine
	// means somebody has the wrong function in mind.
	TestFalse(TEXT("nine is not one of three"), UConsoleDeckStatics::CoerceValue(Mode, TEXT("9"), Value, Error));
	TestFalse(TEXT("swimming is not one of them"), UConsoleDeckStatics::CoerceValue(Mode, TEXT("Swimming"), Value, Error));

	// A whole argument list, and the rule that makes calling from the console short: fewer arguments than
	// parameters keeps what is already dialled in, more arguments than parameters is an error.
	TArray<FConsoleDeckParam> Params = { Count, Flag };
	Params[0].Value = TEXT("7");
	Params[1].Value = TEXT("true");

	TArray<FString> Values;
	TestTrue(TEXT("one argument for two parameters is fine"),
		UConsoleDeckStatics::CoerceArgs(Params, { TEXT("3") }, Values, Error));
	TestEqual(TEXT("the given one is used"), Values[0], FString(TEXT("3")));
	TestEqual(TEXT("the missing one keeps its value"), Values[1], FString(TEXT("true")));

	TestFalse(TEXT("three arguments for two parameters is not"),
		UConsoleDeckStatics::CoerceArgs(Params, { TEXT("3"), TEXT("false"), TEXT("extra") }, Values, Error));

	return true;
}

//
// (4) Limits. ClampMin and ClampMax hold, whether the value came from a keyboard or from the d-pad.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConsoleDeckClampTest,
	"ConsoleDeck.Logic.ClampMinAndClampMaxAreHeld",
	ConsoleDeckTests::TestFlags)

bool FConsoleDeckClampTest::RunTest(const FString& Parameters)
{
	using namespace ConsoleDeckTests;

	FConsoleDeckParam Count = MakeNumber(TEXT("Count"), EConsoleDeckParamKind::Int, 0.0, 64.0);

	FString Value;
	FString Error;

	TestTrue(TEXT("a number inside the range is taken"), UConsoleDeckStatics::CoerceValue(Count, TEXT("10"), Value, Error));
	TestEqual(TEXT("unchanged"), Value, FString(TEXT("10")));

	// Over the top and under the bottom are clamped, not refused: a slider that stops at its end is what
	// somebody expects, and refusing 999 would just mean typing it again.
	TestTrue(TEXT("999 is accepted"), UConsoleDeckStatics::CoerceValue(Count, TEXT("999"), Value, Error));
	TestEqual(TEXT("and held at the top"), Value, FString(TEXT("64")));
	TestTrue(TEXT("-5 is accepted"), UConsoleDeckStatics::CoerceValue(Count, TEXT("-5"), Value, Error));
	TestEqual(TEXT("and held at the bottom"), Value, FString(TEXT("0")));

	TestEqual(TEXT("ClampToParam holds the top"), UConsoleDeckStatics::ClampToParam(Count, 1000.0), 64.0);
	TestEqual(TEXT("ClampToParam holds the bottom"), UConsoleDeckStatics::ClampToParam(Count, -1000.0), 0.0);

	// The d-pad goes through the same clamp. Twenty presses cross the whole range and the twenty-first
	// changes nothing - which is what an end stop means.
	TStrongObjectPtr<UConsoleDeckEntry> Entry(NewObject<UConsoleDeckEntry>());
	Entry->Params.Add(MakeNumber(TEXT("Count"), EConsoleDeckParamKind::Int, 0.0, 64.0));

	for (int32 Press = 0; Press < 40; ++Press)
	{
		Entry->AdjustParam(0, 1);
	}
	TestEqual(TEXT("holding right stops at the top"), Entry->Params[0].Value, FString(TEXT("64")));

	for (int32 Press = 0; Press < 80; ++Press)
	{
		Entry->AdjustParam(0, -1);
	}
	TestEqual(TEXT("holding left stops at the bottom"), Entry->Params[0].Value, FString(TEXT("0")));

	// A parameter with no limits is not clamped into nothing. An unbounded float has to stay unbounded.
	FConsoleDeckParam Free;
	Free.Name = FName(TEXT("Free"));
	Free.Kind = EConsoleDeckParamKind::Float;
	TestEqual(TEXT("no limits, no clamping"), UConsoleDeckStatics::ClampToParam(Free, 12345.0), 12345.0);

	// The limits in ConsoleDeckArgs reach the same place the ones in ClampMin do.
	TArray<FConsoleDeckParam> Params;
	Params.Add(MakeNumber(TEXT("Amount"), EConsoleDeckParamKind::Int, 0.0, 0.0));
	Params[0].bHasMin = false;
	Params[0].bHasMax = false;

	UConsoleDeckStatics::ApplyArgSpec(TEXT("Amount=10:0..64"), Params);
	TestTrue(TEXT("ConsoleDeckArgs sets a lower limit"), Params[0].bHasMin);
	TestTrue(TEXT("ConsoleDeckArgs sets an upper limit"), Params[0].bHasMax);
	TestEqual(TEXT("and the default in front of the colon"), Params[0].Value, FString(TEXT("10")));
	TestEqual(TEXT("the upper limit is the one that was written"), Params[0].Max, 64.0);

	return true;
}

//
// (5) An entry with no target instance. Greyed out, with a reason, and not called.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConsoleDeckMissingTargetTest,
	"ConsoleDeck.Catalog.MissingTargetIsGreyedOutWithAReasonAndNotCalled",
	ConsoleDeckTests::TestFlags)

bool FConsoleDeckMissingTargetTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UConsoleDeckEntry> Entry(NewObject<UConsoleDeckEntry>());
	Entry->Section = TEXT("World");
	Entry->DisplayName = TEXT("End Match");
	Entry->TargetClass = AGameModeBase::StaticClass();
	Entry->ClassPath = AGameModeBase::StaticClass()->GetPathName();
	Entry->Function = AActor::StaticClass()->FindFunctionByName(TEXT("K2_DestroyActor"));

	if (!TestNotNull(TEXT("the test needs a real UFunction to point at"), Entry->Function.Get()))
	{
		return false;
	}

	// No world at all. This is the state the catalog is in between being built and a level being ready, and
	// it must not be mistaken for "the class is gone".
	TestNull(TEXT("no world, no target"), Entry->ResolveTarget(nullptr));
	TestEqual(TEXT("and the deck says which of the two it is"), Entry->Availability, EConsoleDeckAvailability::NoWorld);
	TestTrue(TEXT("with a reason a person can read"), !Entry->UnavailableReason.IsEmpty());

	// A real world with nothing in it. This is the case the plugin exists to be honest about: a GameMode
	// function on a client build, where the entry is there and cannot be used.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ConsoleDeckTestWorld"));
	if (!TestNotNull(TEXT("a throwaway world for the test"), World))
	{
		return false;
	}

	TestNull(TEXT("an empty world has no GameMode"), Entry->ResolveTarget(World));
	TestEqual(TEXT("so the entry is unavailable, not missing"), Entry->Availability, EConsoleDeckAvailability::NoInstance);
	TestTrue(TEXT("and the reason names what is missing"), Entry->UnavailableReason.Contains(TEXT("GameMode")));
	TestFalse(TEXT("it is not ready"), Entry->IsReady());

	// And it is not called. An entry that is greyed out on screen but still runs when the button is pressed
	// would be worse than no greying at all.
	const FString ReasonBefore = Entry->UnavailableReason;
	TestFalse(TEXT("invoking an unavailable entry does nothing"), Entry->Invoke(World));
	TestEqual(TEXT("and the reason is what comes back"), Entry->LastResult, ReasonBefore);

	World->DestroyWorld(false);

	return true;
}

//
// (6) ConsoleDeckArgs. Defaults, limits and presets - the three things a cook would otherwise eat.
//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConsoleDeckArgSpecTest,
	"ConsoleDeck.Logic.ArgSpecCarriesDefaultsLimitsAndPresets",
	ConsoleDeckTests::TestFlags)

bool FConsoleDeckArgSpecTest::RunTest(const FString& Parameters)
{
	TArray<FConsoleDeckParam> Params;

	FConsoleDeckParam Count;
	Count.Name = FName(TEXT("Count"));
	Count.Kind = EConsoleDeckParamKind::Int;
	Count.Value = TEXT("0");
	Params.Add(Count);

	FConsoleDeckParam Marker;
	Marker.Name = FName(TEXT("Marker"));
	Marker.Kind = EConsoleDeckParamKind::String;
	Params.Add(Marker);

	UConsoleDeckStatics::ApplyArgSpec(TEXT("Count=10:0..64; Marker=checkpoint|boss|end; Nonsense=7"), Params);

	TestEqual(TEXT("the default is read"), Params[0].Value, FString(TEXT("10")));
	TestEqual(TEXT("the lower limit is read"), Params[0].Min, 0.0);
	TestEqual(TEXT("the upper limit is read"), Params[0].Max, 64.0);

	// Presets are what makes a string usable on a pad, where there is no keyboard to type one with.
	if (TestEqual(TEXT("three presets"), Params[1].Options.Num(), 3))
	{
		TestEqual(TEXT("the first preset is the default"), Params[1].Value, FString(TEXT("checkpoint")));
		TestEqual(TEXT("and the last one is there too"), Params[1].Options[2], FString(TEXT("end")));
	}

	// A name that matches no parameter is ignored. A typo in a metadata string must never be the reason a
	// project fails to start.
	TestEqual(TEXT("nothing else was touched"), Params.Num(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

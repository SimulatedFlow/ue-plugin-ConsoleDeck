// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ConsoleDeckEntry.h"

#include "ConsoleDeckInternal.h"
#include "ConsoleDeckLog.h"
#include "ConsoleDeckStatics.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace ConsoleDeckEntryLocal
{
	/** The numeric value behind an enum entry's short name, or 0 if the name is not in that enum. */
	int64 EnumValueFromName(const UEnum* Enum, const FString& Name)
	{
		if (!Enum)
		{
			return 0;
		}

		const int32 Count = Enum->NumEnums();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Enum->GetNameStringByIndex(Index).Equals(Name, ESearchCase::IgnoreCase))
			{
				return Enum->GetValueByIndex(Index);
			}
		}

		return 0;
	}

	/**
	 * Write one dialled-in value into the packed parameter buffer.
	 *
	 * Type by type rather than through ImportText. ImportText would be shorter, but it also parses quotes,
	 * commas and escapes, which means a marker string containing a comma would arrive at the function cut
	 * in half. What a person typed has to reach the function exactly as they typed it.
	 */
	void WriteParam(FProperty* Property, void* Buffer, const FConsoleDeckParam& Param)
	{
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Buffer);

		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool bValue = false;
			ConsoleDeck::ParseBoolean(Param.Value, bValue);
			BoolProperty->SetPropertyValue(ValuePtr, bValue);
			return;
		}

		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			const int64 Value = EnumValueFromName(EnumProperty->GetEnum(), Param.Value);
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Value);
			return;
		}

		if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			if (ByteProperty->Enum)
			{
				const int64 Value = EnumValueFromName(ByteProperty->Enum, Param.Value);
				ByteProperty->SetPropertyValue(ValuePtr, static_cast<uint8>(FMath::Clamp<int64>(Value, 0, 255)));
				return;
			}
		}

		if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			double AsDouble = 0.0;
			ConsoleDeck::ParseDecimal(Param.Value, AsDouble);

			if (NumericProperty->IsFloatingPoint())
			{
				NumericProperty->SetFloatingPointPropertyValue(ValuePtr, AsDouble);
			}
			else
			{
				NumericProperty->SetIntPropertyValue(ValuePtr, static_cast<int64>(FMath::RoundToDouble(AsDouble)));
			}
			return;
		}

		if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
		{
			StrProperty->SetPropertyValue(ValuePtr, Param.Value);
			return;
		}

		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			NameProperty->SetPropertyValue(ValuePtr, FName(*Param.Value));
			return;
		}

		if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			TextProperty->SetPropertyValue(ValuePtr, FText::FromString(Param.Value));
			return;
		}
	}
}

FString UConsoleDeckEntry::GetPath() const
{
	return FString::Printf(TEXT("%s|%s"), *Section, *DisplayName);
}

int32 UConsoleDeckEntry::GetFirstEditableParam() const
{
	for (int32 Index = 0; Index < Params.Num(); ++Index)
	{
		if (Params[Index].Kind != EConsoleDeckParamKind::Unsupported)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

UObject* UConsoleDeckEntry::ResolveTarget(UWorld* World)
{
	CachedTarget = nullptr;

	if (!bSignatureSupported)
	{
		Availability = EConsoleDeckAvailability::UnsupportedSignature;
		UnavailableReason = SignatureReason;
		return nullptr;
	}

	if (!Function || !TargetClass)
	{
		Availability = EConsoleDeckAvailability::NotFound;
		UnavailableReason = FString::Printf(TEXT("%s is not loaded in this build"), *ClassPath);
		return nullptr;
	}

	if (!World)
	{
		Availability = EConsoleDeckAvailability::NoWorld;
		UnavailableReason = TEXT("no world yet");
		return nullptr;
	}

	UClass* Class = TargetClass;
	UGameInstance* GameInstance = World->GetGameInstance();
	APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;

	UObject* Found = nullptr;

	// Subsystems first. They are the case the console handles worst - a subsystem has no console target at
	// all - and the case a search list in the project settings exists for.
	if (Class->IsChildOf(UGameInstanceSubsystem::StaticClass()))
	{
		Found = GameInstance ? GameInstance->GetSubsystemBase(Class) : nullptr;
		if (!Found)
		{
			Availability = EConsoleDeckAvailability::NoInstance;
			UnavailableReason = FString::Printf(TEXT("game instance subsystem %s is not running"), *Class->GetName());
			return nullptr;
		}
	}
	else if (Class->IsChildOf(UWorldSubsystem::StaticClass()))
	{
		Found = World->GetSubsystemBase(Class);
		if (!Found)
		{
			Availability = EConsoleDeckAvailability::NoInstance;
			UnavailableReason = FString::Printf(TEXT("world subsystem %s does not run in this world"), *Class->GetName());
			return nullptr;
		}
	}
	else if (Class->IsChildOf(ULocalPlayerSubsystem::StaticClass()))
	{
		ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		Found = LocalPlayer ? LocalPlayer->GetSubsystemBase(Class) : nullptr;
		if (!Found)
		{
			Availability = EConsoleDeckAvailability::NoInstance;
			UnavailableReason = FString::Printf(TEXT("no local player with subsystem %s"), *Class->GetName());
			return nullptr;
		}
	}
	else if (Class->IsChildOf(UEngineSubsystem::StaticClass()))
	{
		Found = GEngine ? GEngine->GetEngineSubsystemBase(Class) : nullptr;
		if (!Found)
		{
			Availability = EConsoleDeckAvailability::NoInstance;
			UnavailableReason = FString::Printf(TEXT("engine subsystem %s is not running"), *Class->GetName());
			return nullptr;
		}
	}
	else if (Class->IsChildOf(UGameInstance::StaticClass()))
	{
		Found = (GameInstance && GameInstance->IsA(Class)) ? GameInstance : nullptr;
		if (!Found)
		{
			Availability = EConsoleDeckAvailability::NoInstance;
			UnavailableReason = FString::Printf(TEXT("the game instance is not a %s"), *Class->GetName());
			return nullptr;
		}
	}
	else if (Class->IsChildOf(AActor::StaticClass()))
	{
		// The five actors a debug function is most often on, checked by hand and in the order somebody
		// would look for them, before falling back to walking the level.
		if (PC && PC->IsA(Class))
		{
			Found = PC;
		}
		else if (PC && PC->GetPawn() && PC->GetPawn()->IsA(Class))
		{
			Found = PC->GetPawn();
		}
		else if (PC && PC->GetHUD() && PC->GetHUD()->IsA(Class))
		{
			Found = PC->GetHUD();
		}
		else if (PC && PC->PlayerState && PC->PlayerState->IsA(Class))
		{
			Found = PC->PlayerState;
		}
		else if (World->GetAuthGameMode() && World->GetAuthGameMode()->IsA(Class))
		{
			Found = World->GetAuthGameMode();
		}
		else if (World->GetGameState() && World->GetGameState()->IsA(Class))
		{
			Found = World->GetGameState();
		}
		else
		{
			for (TActorIterator<AActor> It(World, Class); It; ++It)
			{
				if (IsValid(*It))
				{
					Found = *It;
					break;
				}
			}
		}

		if (!Found)
		{
			// The reason is the product here. "no GameMode of this class in this world" tells somebody on a
			// client build why the button is grey and what to do about it; a missing row tells them nothing
			// and looks like the plugin lost their function.
			Availability = EConsoleDeckAvailability::NoInstance;

			if (Class->IsChildOf(AGameModeBase::StaticClass()))
			{
				UnavailableReason = TEXT("no GameMode of this class in this world - a client has none");
			}
			else if (Class->IsChildOf(APawn::StaticClass()))
			{
				UnavailableReason = FString::Printf(TEXT("no %s spawned in this world"), *Class->GetName());
			}
			else
			{
				UnavailableReason = FString::Printf(TEXT("no %s in this world"), *Class->GetName());
			}
			return nullptr;
		}
	}
	else if (Class->IsChildOf(UActorComponent::StaticClass()))
	{
		const TSubclassOf<UActorComponent> ComponentClass = Class;

		if (PC && PC->GetPawn())
		{
			Found = PC->GetPawn()->GetComponentByClass(ComponentClass);
		}
		if (!Found && PC)
		{
			Found = PC->GetComponentByClass(ComponentClass);
		}
		if (!Found)
		{
			for (TActorIterator<AActor> It(World); It && !Found; ++It)
			{
				Found = It->GetComponentByClass(ComponentClass);
			}
		}

		if (!Found)
		{
			Availability = EConsoleDeckAvailability::NoInstance;
			UnavailableReason = FString::Printf(TEXT("no actor in this world has a %s"), *Class->GetName());
			return nullptr;
		}
	}
	else
	{
		// A plain UObject the deck cannot go looking for without walking every object in memory. Saying so
		// is better than a search that takes a second and finds the wrong one.
		Availability = EConsoleDeckAvailability::NoInstance;
		UnavailableReason = FString::Printf(TEXT("%s is not an actor, a component or a subsystem - the deck cannot find an instance of it"), *Class->GetName());
		return nullptr;
	}

	CachedTarget = Found;
	Availability = EConsoleDeckAvailability::Ready;
	UnavailableReason.Reset();
	return Found;
}

bool UConsoleDeckEntry::Invoke(UWorld* World)
{
	// Resolved again, right now, even if it was resolved a moment ago when the menu opened. An actor can be
	// destroyed between the two, and a cheat menu that crashes the game it is meant to debug has failed at
	// the only job it had.
	UObject* Target = ResolveTarget(World);
	if (!Target || !Function)
	{
		LastResult = UnavailableReason;
		return false;
	}

	const int32 ParmsSize = Function->ParmsSize;
	uint8* Buffer = nullptr;

	if (ParmsSize > 0)
	{
		// The buffer is laid out by the function's own properties, aligned the way the function wants it,
		// and every parameter is constructed before anything is written into it - an FString parameter that
		// is only memzeroed is a crash waiting for the first call.
		Buffer = static_cast<uint8*>(FMemory::Malloc(ParmsSize, Function->GetMinAlignment()));
		FMemory::Memzero(Buffer, ParmsSize);

		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buffer);
		}

		int32 ParamIndex = 0;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* Property = *It;

			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			if (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				continue;
			}
			if (!Params.IsValidIndex(ParamIndex))
			{
				break;
			}

			ConsoleDeckEntryLocal::WriteParam(Property, Buffer, Params[ParamIndex]);
			++ParamIndex;
		}
	}

	Target->ProcessEvent(Function, Buffer);

	// Whatever came back. A bool return is the difference between "God Mode on" and "God Mode refused",
	// and the status line is where somebody looks for that.
	FString ResultText;
	if (Buffer)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
			{
				continue;
			}

			FString Exported;
			Property->ExportTextItem_Direct(Exported, Property->ContainerPtrToValuePtr<void>(Buffer), nullptr, nullptr, PPF_None);

			if (!ResultText.IsEmpty())
			{
				ResultText += TEXT(", ");
			}
			ResultText += Property->HasAnyPropertyFlags(CPF_ReturnParm)
				? Exported
				: FString::Printf(TEXT("%s=%s"), *Property->GetName(), *Exported);
		}

		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Buffer);
		}

		FMemory::Free(Buffer);
	}

	LastResult = ResultText.IsEmpty() ? FString::Printf(TEXT("%s called on %s"), *DisplayName, *Target->GetName()) : ResultText;
	return true;
}

FString UConsoleDeckEntry::GetValueText() const
{
	if (Params.Num() == 0)
	{
		return TEXT("run");
	}

	TArray<FString> Pieces;
	Pieces.Reserve(Params.Num());

	for (const FConsoleDeckParam& Param : Params)
	{
		FString Piece;

		switch (Param.Kind)
		{
		case EConsoleDeckParamKind::Bool:
		{
			bool bValue = false;
			ConsoleDeck::ParseBoolean(Param.Value, bValue);
			Piece = bValue ? TEXT("[ on  ]") : TEXT("[ off ]");
			break;
		}

		case EConsoleDeckParamKind::Int:
		case EConsoleDeckParamKind::Float:
		case EConsoleDeckParamKind::Enum:
			Piece = FString::Printf(TEXT("< %s >"), *Param.Value);
			break;

		case EConsoleDeckParamKind::Unsupported:
			Piece = FString::Printf(TEXT("(%s)"), *Param.TypeName);
			break;

		default:
			Piece = FString::Printf(TEXT("\"%s\""), *Param.Value);
			break;
		}

		Pieces.Add(Params.Num() > 1 ? FString::Printf(TEXT("%s %s"), *Param.Name.ToString(), *Piece) : Piece);
	}

	return FString::Join(Pieces, TEXT("  "));
}

void UConsoleDeckEntry::AdjustParam(int32 ParamIndex, int32 Direction, float StepScale)
{
	if (!Params.IsValidIndex(ParamIndex) || Direction == 0)
	{
		return;
	}

	FConsoleDeckParam& Param = Params[ParamIndex];

	switch (Param.Kind)
	{
	case EConsoleDeckParamKind::Bool:
	{
		bool bValue = false;
		ConsoleDeck::ParseBoolean(Param.Value, bValue);
		Param.Value = bValue ? TEXT("false") : TEXT("true");
		return;
	}

	case EConsoleDeckParamKind::Enum:
	case EConsoleDeckParamKind::String:
	case EConsoleDeckParamKind::Name:
	case EConsoleDeckParamKind::Text:
	{
		if (Param.Options.Num() == 0)
		{
			return;
		}

		int32 Current = Param.Options.IndexOfByPredicate([&Param](const FString& Option)
		{
			return Option.Equals(Param.Value, ESearchCase::IgnoreCase);
		});

		if (Current == INDEX_NONE)
		{
			Current = 0;
		}

		// Wrapping, not clamping. A list of four options with a d-pad is a ring; stopping at the end just
		// makes somebody press the other direction four times.
		const int32 Count = Param.Options.Num();
		Current = ((Current + Direction) % Count + Count) % Count;
		Param.Value = Param.Options[Current];
		return;
	}

	case EConsoleDeckParamKind::Int:
	case EConsoleDeckParamKind::Float:
	{
		const bool bWhole = Param.Kind == EConsoleDeckParamKind::Int;

		double Current = 0.0;
		ConsoleDeck::ParseDecimal(Param.Value, Current);

		double Step = Param.Step;
		if (Step <= 0.0)
		{
			if (Param.bHasMin && Param.bHasMax)
			{
				// Twenty presses from one limit to the other. Enough resolution to aim, few enough presses
				// to get there.
				Step = (Param.Max - Param.Min) / 20.0;
			}
			else
			{
				Step = bWhole ? 1.0 : FMath::Max(0.1, FMath::Abs(Current) * 0.1);
			}
		}

		if (bWhole)
		{
			Step = FMath::Max(1.0, FMath::RoundToDouble(Step));
		}

		Current += Direction * Step * FMath::Max(1.0f, StepScale);
		Param.Value = ConsoleDeck::FormatNumber(UConsoleDeckStatics::ClampToParam(Param, Current), bWhole);
		return;
	}

	default:
		return;
	}
}

void UConsoleDeckEntry::ResetParams()
{
	for (FConsoleDeckParam& Param : Params)
	{
		Param.Value = Param.DefaultValue;
	}
}

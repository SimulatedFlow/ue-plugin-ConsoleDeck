// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ConsoleDeckSampleCheats.h"

#include "ConsoleDeckLog.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AConsoleDeckSampleCheats::AConsoleDeckSampleCheats()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AConsoleDeckSampleCheats::BeginPlay()
{
	Super::BeginPlay();

	SetTimeOfDay(HourOfDay);
	SetFlySpeed(FlySpeed);

	if (TargetsAtStart > 0)
	{
		SpawnTargets(TargetsAtStart);
	}
}

void AConsoleDeckSampleCheats::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FMath::IsNearlyZero(TargetSpin))
	{
		return;
	}

	const FRotator Step(0.0f, TargetSpin * DeltaSeconds, 0.0f);
	for (const TObjectPtr<AActor>& Target : SpawnedTargets)
	{
		if (IsValid(Target))
		{
			Target->AddActorLocalRotation(Step);
		}
	}
}

// ----------------------------------------------------------------------------------------------------
// Player
// ----------------------------------------------------------------------------------------------------

void AConsoleDeckSampleCheats::SetGodMode(bool bEnabled)
{
	bGodMode = bEnabled;

	// A sample has nothing to be invulnerable to. The flag is what a real project would read; here it is
	// drawn on the HUD so that pressing the switch is visibly doing something rather than nothing.
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Pawn->SetCanBeDamaged(!bGodMode);
	}

	UE_LOG(LogConsoleDeck, Log, TEXT("ConsoleDeck sample: god mode %s."), bGodMode ? TEXT("on") : TEXT("off"));
}

void AConsoleDeckSampleCheats::SetFlySpeed(float Speed)
{
	FlySpeed = Speed;

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;

	if (UFloatingPawnMovement* Floating = Cast<UFloatingPawnMovement>(Movement))
	{
		Floating->MaxSpeed = FlySpeed;
	}
}

void AConsoleDeckSampleCheats::RespawnPlayer()
{
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Pawn->SetActorLocation(RespawnLocation);
	}
}

// ----------------------------------------------------------------------------------------------------
// World
// ----------------------------------------------------------------------------------------------------

void AConsoleDeckSampleCheats::SpawnTargets(int32 Count)
{
	UWorld* World = GetWorld();
	if (!World || !TargetClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	const int32 PerRow = FMath::Max(1, TargetsPerRow);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// The grid is laid out from the count alone, so one number on one row of the menu changes what the
		// level looks like - which is the point of putting a spawn count in a cheat menu in the first place.
		const int32 Row = Index / PerRow;
		const int32 Column = Index % PerRow;

		const FVector Location = TargetGridOrigin
			+ FVector(Row * TargetGridSpacing, Column * TargetGridSpacing, 0.0f);

		if (AActor* Spawned = World->SpawnActor<AActor>(TargetClass, Location, FRotator::ZeroRotator, Params))
		{
			SpawnedTargets.Add(Spawned);
		}
	}

	UE_LOG(LogConsoleDeck, Log, TEXT("ConsoleDeck sample: spawned %d target(s), %d alive."),
		Count, SpawnedTargets.Num());
}

void AConsoleDeckSampleCheats::ClearTargets()
{
	for (const TObjectPtr<AActor>& Target : SpawnedTargets)
	{
		if (IsValid(Target))
		{
			Target->Destroy();
		}
	}

	SpawnedTargets.Reset();
}

void AConsoleDeckSampleCheats::SetTimeOfDay(float Hour)
{
	HourOfDay = Hour;

	AActor* Sun = ResolveSunLight();
	if (!Sun)
	{
		return;
	}

	// Six in the morning puts the sun on the horizon and noon puts it overhead, which makes the number on
	// the row mean the thing its name says rather than an angle somebody has to translate in their head.
	const float Pitch = -15.0f * (Hour - 6.0f);
	Sun->SetActorRotation(FRotator(Pitch, 35.0f, 0.0f));
}

void AConsoleDeckSampleCheats::SetTargetSpin(float DegreesPerSecond)
{
	TargetSpin = DegreesPerSecond;
}

// ----------------------------------------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------------------------------------

void AConsoleDeckSampleCheats::SetMarker(const FString& Label)
{
	Marker = Label;
	UE_LOG(LogConsoleDeck, Log, TEXT("ConsoleDeck sample: marker set to '%s'."), *Marker);
}

FString AConsoleDeckSampleCheats::ReportTargets() const
{
	int32 Alive = 0;
	for (const TObjectPtr<AActor>& Target : SpawnedTargets)
	{
		Alive += IsValid(Target) ? 1 : 0;
	}

	return FString::Printf(TEXT("%d target(s) alive, spin %.0f deg/s"), Alive, TargetSpin);
}

// ----------------------------------------------------------------------------------------------------
// HUD and setup
// ----------------------------------------------------------------------------------------------------

FString AConsoleDeckSampleCheats::GetSampleStatus() const
{
	int32 Alive = 0;
	for (const TObjectPtr<AActor>& Target : SpawnedTargets)
	{
		Alive += IsValid(Target) ? 1 : 0;
	}

	return FString::Printf(
		TEXT("God Mode: %s    Fly Speed: %.0f    Targets: %d    Spin: %.0f deg/s    Hour: %.1f    Marker: %s"),
		bGodMode ? TEXT("ON") : TEXT("off"), FlySpeed, Alive, TargetSpin, HourOfDay, *Marker);
}

AActor* AConsoleDeckSampleCheats::ResolveSunLight()
{
	if (IsValid(SunLight))
	{
		return SunLight;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		SunLight = *It;
		return SunLight;
	}

	return nullptr;
}

// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "ConsoleDeckSampleCheats.generated.h"

/**
 * The sample the demo map is built on: nine ordinary functions, marked where they live.
 *
 * There is nothing special about this class. Every function below is a plain UFUNCTION doing a plain
 * thing, and the only reason any of them turns up in the menu is the one metadata key on the line above
 * it - meta=(ConsoleDeck="Section|Name"). Delete the key and the entry is gone; add the key to a function
 * of your own and it is there the next time the game starts. That is the whole contract, and this class
 * exists to show it working rather than to describe it.
 *
 * ConsoleDeckArgs carries what UHT has no syntax for: a default, a range, a list of presets. "Count=12:1..40"
 * is a spawn count that starts at twelve and cannot be dialled past forty; "Label=arena|boss|checkpoint|exit"
 * turns a string parameter into four choices, which is the only form a string is usable in on a pad.
 *
 * Drop it in a level, point TargetClass and SunLight at something, and the deck can drive it. In your own
 * project you would delete this class and mark your own functions - and if you would rather the sample did
 * not show up in your menu at all, ScanPathFilters in Project Settings > Plugins > ConsoleDeck narrows the
 * scan to your own code, which is worth doing anyway for what it saves at startup.
 */
UCLASS(Blueprintable, DisplayName = "ConsoleDeck Sample Cheats")
class CONSOLEDECK_API AConsoleDeckSampleCheats : public AActor
{
	GENERATED_BODY()

public:
	AConsoleDeckSampleCheats();

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	// --------------------------------------------------------------------------------------------------
	// The marked functions - one metadata key each, nothing else
	// --------------------------------------------------------------------------------------------------

	/** A bool parameter is drawn as a switch, so this is one row that toggles rather than a menu of two. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|Player",
		meta = (ConsoleDeck = "Player|God Mode", ConsoleDeckArgs = "bEnabled=true"))
	void SetGodMode(bool bEnabled);

	/** The range comes from ConsoleDeckArgs, so left and right on the pad move in sensible steps. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|Player",
		meta = (ConsoleDeck = "Player|Fly Speed", ConsoleDeckArgs = "Speed=600:150..3000"))
	void SetFlySpeed(float Speed);

	/** No parameters at all: the deck draws it as a button and calls it when A is pressed. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|Player",
		meta = (ConsoleDeck = "Player|Respawn At Start"))
	void RespawnPlayer();

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|World",
		meta = (ConsoleDeck = "World|Spawn Targets", ConsoleDeckArgs = "Count=12:1..40"))
	void SpawnTargets(int32 Count);

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|World",
		meta = (ConsoleDeck = "World|Clear Targets"))
	void ClearTargets();

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|World",
		meta = (ConsoleDeck = "World|Time Of Day", ConsoleDeckArgs = "Hour=13:0..24"))
	void SetTimeOfDay(float Hour);

	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|World",
		meta = (ConsoleDeck = "World|Target Spin", ConsoleDeckArgs = "DegreesPerSecond=45:0..720"))
	void SetTargetSpin(float DegreesPerSecond);

	/** Presets instead of typing. Four bars in the metadata, four choices on the row. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|Debug",
		meta = (ConsoleDeck = "Debug|Set Marker", ConsoleDeckArgs = "Label=arena|boss|checkpoint|exit"))
	void SetMarker(const FString& Label);

	/** A return value is read back after the call and shown on the status line under the list. */
	UFUNCTION(BlueprintCallable, Category = "ConsoleDeck Sample|Debug",
		meta = (ConsoleDeck = "Debug|Report Targets"))
	FString ReportTargets() const;

	// --------------------------------------------------------------------------------------------------
	// What the demo HUD draws
	// --------------------------------------------------------------------------------------------------

	/** The current state of everything above, one line per value, for the demo HUD to draw. */
	UFUNCTION(BlueprintPure, Category = "ConsoleDeck Sample")
	FString GetSampleStatus() const;

	// --------------------------------------------------------------------------------------------------
	// Setup
	// --------------------------------------------------------------------------------------------------

	/** What Spawn Targets spawns. Any actor will do; the demo uses a cube with the sample material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck Sample|Setup")
	TSubclassOf<AActor> TargetClass;

	/** The light Time Of Day turns. Left empty, that entry finds the first directional light in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck Sample|Setup")
	TObjectPtr<AActor> SunLight;

	/** Corner of the grid the targets are spawned on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck Sample|Setup")
	FVector TargetGridOrigin = FVector(-220.0f, -700.0f, 150.0f);

	/** Centre-to-centre distance between two spawned targets, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck Sample|Setup",
		meta = (ClampMin = "50.0", ClampMax = "600.0"))
	float TargetGridSpacing = 200.0f;

	/** How many targets go in a row before the grid starts a new one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck Sample|Setup",
		meta = (ClampMin = "1", ClampMax = "16"))
	int32 TargetsPerRow = 8;

	/** Where Respawn At Start puts the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck Sample|Setup")
	FVector RespawnLocation = FVector(-940.0f, 0.0f, 200.0f);

	/** How many targets the level starts with, so the demo map is not empty before anything is pressed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck Sample|Setup",
		meta = (ClampMin = "0", ClampMax = "40"))
	int32 TargetsAtStart = 12;

	// --------------------------------------------------------------------------------------------------
	// State the cheats above change
	// --------------------------------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ConsoleDeck Sample|State")
	bool bGodMode = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ConsoleDeck Sample|State")
	float FlySpeed = 600.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ConsoleDeck Sample|State")
	float HourOfDay = 13.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ConsoleDeck Sample|State")
	float TargetSpin = 35.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ConsoleDeck Sample|State")
	FString Marker = TEXT("none");

private:
	/** The light Time Of Day turns, resolved once from SunLight or from the level. */
	AActor* ResolveSunLight();

	/** Spawned by Spawn Targets, rotated by Tick, destroyed by Clear Targets. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedTargets;
};

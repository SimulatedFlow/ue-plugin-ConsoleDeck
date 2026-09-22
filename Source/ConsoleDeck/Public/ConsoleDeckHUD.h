// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ConsoleDeckHUD.generated.h"

/**
 * The default host: draws the deck and feeds it input.
 *
 * Set this as the HUD class of your GameMode and the deck works with no further wiring. It is a thin
 * actor on purpose - the drawing and the input handling live in the subsystem, so a project that already
 * has a HUD class does not have to choose between its own HUD and this one. Two calls in your own HUD
 * (UConsoleDeckStatics::DrawDeck and TickDeckInput) get you the identical menu.
 *
 * Ticks even when paused, because a menu that can pause the game has to keep working afterwards.
 */
UCLASS(Blueprintable, DisplayName = "ConsoleDeck HUD")
class CONSOLEDECK_API AConsoleDeckHUD : public AHUD
{
	GENERATED_BODY()

public:
	AConsoleDeckHUD();

	//~ Begin AActor interface
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	//~ Begin AHUD interface
	virtual void DrawHUD() override;
	//~ End AHUD interface

	/** Poll the pad and the keyboard for the deck. Off if something else in your project does it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck")
	bool bHandleDeckInput = true;

	/** Draw the deck when it is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConsoleDeck")
	bool bDrawDeck = true;
};

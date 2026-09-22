// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ConsoleDeckHUD.h"

#include "ConsoleDeckSubsystem.h"
#include "GameFramework/PlayerController.h"

AConsoleDeckHUD::AConsoleDeckHUD()
{
	PrimaryActorTick.bCanEverTick = true;

	// The deck can pause the game. If this actor stopped ticking at the same moment, the menu would freeze
	// the frame it was opened in and never see another button press - so it ticks through the pause.
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AConsoleDeckHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bHandleDeckInput)
	{
		return;
	}

	if (UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(this))
	{
		Deck->TickInput(PlayerOwner, DeltaSeconds);
	}
}

void AConsoleDeckHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!bDrawDeck)
	{
		return;
	}

	if (UConsoleDeckSubsystem* Deck = UConsoleDeckSubsystem::Get(this))
	{
		Deck->DrawDeck(Canvas);
	}
}

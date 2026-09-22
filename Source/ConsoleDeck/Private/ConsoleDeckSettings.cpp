// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ConsoleDeckSettings.h"

UConsoleDeckSettings::UConsoleDeckSettings()
{
	// The defaults are a working gamepad layout, not a placeholder. Somebody who installs the plugin,
	// marks one function and presses View on a pad has to end up in the menu without opening a settings
	// page first - if that does not happen, the plugin has failed at the thing it exists for.
	OpenKeys = { EKeys::F8, EKeys::Gamepad_Special_Left };

	UpKeys = { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up, EKeys::Up };
	DownKeys = { EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down, EKeys::Down };
	LeftKeys = { EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left, EKeys::Left };
	RightKeys = { EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right, EKeys::Right };

	// Shoulder buttons for the section bar. Thumb for the list, index finger for the tabs - the layout
	// every console menu in the last twenty years has used, because it is the one people already know.
	PrevSectionKeys = { EKeys::Gamepad_LeftShoulder, EKeys::PageUp };
	NextSectionKeys = { EKeys::Gamepad_RightShoulder, EKeys::PageDown };

	ActivateKeys = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Enter };
	CloseKeys = { EKeys::Gamepad_FaceButton_Right, EKeys::Escape };
	RebuildKeys = { EKeys::Gamepad_FaceButton_Left, EKeys::F5 };
	ResetKeys = { EKeys::Gamepad_FaceButton_Top, EKeys::Delete };
}

FName UConsoleDeckSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

FName UConsoleDeckSettings::GetSectionName() const
{
	return FName(TEXT("ConsoleDeck"));
}

const UConsoleDeckSettings& UConsoleDeckSettings::Get()
{
	const UConsoleDeckSettings* Settings = GetDefault<UConsoleDeckSettings>();
	check(Settings);
	return *Settings;
}

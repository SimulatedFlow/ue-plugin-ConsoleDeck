// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class ConsoleDeck : ModuleRules
{
	public ConsoleDeck(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// One runtime module, and deliberately no editor module.
		//
		// A cheat menu that only exists in the editor is a cheat menu that is missing exactly where it is
		// needed: on a pad, in a packaged build, in the hands of somebody who is not a programmer. So this
		// module is Runtime, it is compiled into Shipping, and everything it draws is drawn on UCanvas from
		// an AHUD - the same decision NetLens and StreamGuard made, for the same reason. No UMG: a widget
		// tree would have to be cooked, referenced and kept alive by the project, and the menu has to work
		// in a level that contains nothing but a floor.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",

			// InputCore: FKey crosses the public settings header. Which button opens the deck is a project
			// setting, not a constant, and it has to be writable as "Gamepad_Special_Left" in an ini.
			"InputCore",

			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// RenderCore: GWhiteTexture, the background tile behind the panel.
			"RenderCore",
		});

		// Deliberately NOT here either: EnhancedInput.
		//
		// The deck reads raw key state through APlayerController::IsInputKeyDown and WasInputKeyJustPressed.
		// That works under the legacy stack and under Enhanced Input alike, needs no mapping context, no
		// input action assets and no priority to be agreed with your project - and a debug menu that asks
		// you to author an input action before you can open it is a menu nobody opens while hunting a bug.
		// Linking Enhanced Input would have added a plugin dependency your project has to carry for exactly
		// nothing: not one symbol of it would be called.
	}
}

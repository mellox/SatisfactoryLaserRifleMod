// LaserRifleMod — native module build rules (SML / FactoryGame, UE 5.6.1-CSS).

using UnrealBuildTool;

public class LaserRifleMod : ModuleRules
{
	public LaserRifleMod(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore",
			"FactoryGame", "SML",
			"AssetRegistry",
			"SlateCore"   // schematic / descriptor icons (FSlateBrush) used later
		});
	}
}

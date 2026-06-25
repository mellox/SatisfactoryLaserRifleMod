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
			"AkAudio",                     // Wwise: play laser fire sound (UAkAudioEvent)
			"UMG", "Slate", "SlateCore"   // UUserWidget crosshair + FSlateBrush icons
		});
	}
}

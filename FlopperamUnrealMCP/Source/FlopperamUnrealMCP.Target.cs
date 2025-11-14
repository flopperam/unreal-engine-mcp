// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class FlopperamUnrealMCPTarget : TargetRules
{
	public FlopperamUnrealMCPTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

		// Use Latest to automatically match the engine's default build and include order settings
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("FlopperamUnrealMCP");
	}
}

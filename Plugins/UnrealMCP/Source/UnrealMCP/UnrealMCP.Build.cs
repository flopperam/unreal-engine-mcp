// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDefinitions.Add("UNREALMCP_EXPORTS=1");

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Public"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph/Nodes")
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Private"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph/Nodes")
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"HTTP",
				"Json",
				"JsonUtilities",
				"GameplayTags",       // For Enhanced Input input modes
				"AIModule",           // For AIController and gameplay framework
				"DeveloperSettings",
				"PhysicsCore",
				"EngineSettings",     // For Project/Game Maps settings
				"EnhancedInput",      // For UE5 Enhanced Input assets/subsystems
				"UnrealEd",           // For Blueprint editing
				"BlueprintGraph",     // For K2Node classes (F15-F22)
				"KismetCompiler",     // For Blueprint compilation (F15-F22)
				"GeometryScriptingCore",
				"DynamicMesh",
				"GeometryFramework",
				"Renderer",
				"RHI",
				"CinematicCamera"     // For ACineCameraActor and UCineCameraComponent
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"Kismet",
				"Projects",
				"AssetRegistry",
				"AssetTools",
				"NavigationSystem",
				"Navmesh",
				"AudioEditor",			// For USoundFactory (WAV/OGG import)
				"RenderCore",			// For FlushRenderingCommands()
				"ImageWrapper",			// For IImageWrapper texture export fallback
				"UMGEditor",				// For UWidgetBlueprint and Editor Utility Widget creation
				"UMG",					// For UUserWidget and WidgetTree operations
				"MovieScene",			// For UWidgetAnimation MovieScene assets
			"LevelSequence",		// For ULevelSequence and Sequencer tracks
				"CommonUI",				// For Common UI widget class support
				"CommonInput"			// For Common UI input routing dependencies
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GeometryCore",
				"GeometryScriptingCore",
				"GeometryFramework",
				"DynamicMesh",
				"MeshDescription",     // For FMeshDescription
				"StaticMeshDescription" // For FStaticMeshAttributes
			}
		);
		
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",      // For property editing
					"ToolMenus",           // For editor UI
					"BlueprintEditorLibrary", // For Blueprint utilities
					"LevelEditor",         // For level/map management operations
					"StaticMeshEditor",     // For StaticMeshEditorSubsystem
					"GraphEditor",          // For graph node manipulation (Comment, Reroute, Format)
					"InputBlueprintNodes",   // For UK2Node_EnhancedInputAction
					"EditorWidgets",        // For editor widget utilities
					"ApplicationCore",      // For SlateApplication window management
					"WorkspaceMenuStructure", // For editor workspace access
					"DataLayerEditor"      // For UE5.7 Data Layer editor operations
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}

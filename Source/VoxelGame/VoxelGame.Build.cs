// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;
using System;

public class VoxelGame : ModuleRules
{
	private string ModulePath
	{
		get { return ModuleDirectory; }
	}

	private string RootPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../../")); }
	}

	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(RootPath, "3rdparty")); }
	}

	public VoxelGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent", "Json", "JsonUtilities" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		CppStandard = CppStandardVersion.Latest;
		bEnableUndefinedIdentifierWarnings = false;

		PublicIncludePaths.Add(ThirdPartyPath);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

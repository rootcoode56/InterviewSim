// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class InterviewSim : ModuleRules
{
	public InterviewSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        string PopplerBinDirectory = Path.GetFullPath(
    Path.Combine(
        ModuleDirectory,
        "..",
        "..",
        "ThirdParty",
        "Poppler",
        "bin"
    )
);

        if (Target.Platform == UnrealTargetPlatform.Win64 &&
            Directory.Exists(PopplerBinDirectory))
        {
            foreach (string PopplerFile in
         Directory.GetFiles(PopplerBinDirectory))
            {
                string PopplerFileName =
                    Path.GetFileName(PopplerFile);

                RuntimeDependencies.Add(
                    "$(TargetOutputDir)/ThirdParty/Poppler/bin/" +
                    PopplerFileName,
                    PopplerFile,
                    StagedFileType.NonUFS
                );
            }
        }

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemLibraries.Add("Comdlg32.lib");
        }

        PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"HTTP",
			"Json",
			"JsonUtilities",
			"Convai"
		});

		PublicIncludePaths.AddRange(new string[] {
			"InterviewSim",
			"InterviewSim/Variant_Platforming",
			"InterviewSim/Variant_Platforming/Animation",
			"InterviewSim/Variant_Combat",
			"InterviewSim/Variant_Combat/AI",
			"InterviewSim/Variant_Combat/Animation",
			"InterviewSim/Variant_Combat/Gameplay",
			"InterviewSim/Variant_Combat/Interfaces",
			"InterviewSim/Variant_Combat/UI",
			"InterviewSim/Variant_SideScrolling",
			"InterviewSim/Variant_SideScrolling/AI",
			"InterviewSim/Variant_SideScrolling/Gameplay",
			"InterviewSim/Variant_SideScrolling/Interfaces",
			"InterviewSim/Variant_SideScrolling/UI"
		});

        string InterviewEvalServerExe = Path.GetFullPath(
    Path.Combine(
        ModuleDirectory,
        "..",
        "..",
        "ThirdParty",
        "InterviewEvalServer",
        "InterviewEvalServer.exe"
    )
);

        if (Target.Platform == UnrealTargetPlatform.Win64 &&
            File.Exists(InterviewEvalServerExe))
        {
            RuntimeDependencies.Add(
                "$(TargetOutputDir)/InterviewEvalServer/InterviewEvalServer.exe",
                InterviewEvalServerExe,
                StagedFileType.NonUFS
            );
        }

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true


    }
}

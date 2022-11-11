
using UnrealBuildTool;

public class UnrealToUnity : ModuleRules
{
	public UnrealToUnity( ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange( new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay",
         //Extra
           "RenderCore",
           "MaterialEditor",
           "UnrealEd",
            "RHI",
            "Slate",
            "InputCore",
            "SlateCore",
            "EditorStyle",
            "Foliage",
            "ProceduralMeshComponent",
            "Landscape",
            "MeshDescription",
            "StaticMeshDescription",
            "RawMesh"
        } );

        PrivateDependencyModuleNames.AddRange( new string[] {"Core", "CoreUObject", "Engine", "InputCore", 
            "RenderCore",
            "MaterialEditor",
            "UnrealEd",
            "RHI",
            "Slate",
            "InputCore",
            "SlateCore",
            "EditorStyle",
            "Foliage",
            "ProceduralMeshComponent",
            "Landscape",
            "MeshDescription",
            "StaticMeshDescription",
            "RawMesh"
        } );

        AddEngineThirdPartyPrivateStaticDependencies( Target,
            "FBX"
        );
        //PrivateIncludePaths.Add( "Runtime/Engine/Classes/" );
    }
}

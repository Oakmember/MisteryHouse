// Copyright 2018 GiM s.r.o. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include <functional>
#include "Module.generated.h"

UENUM()
enum class RenderPipeline : uint8
{
	RP_BUILTIN,
	RP_URP,
	RP_HDRP,
};

UCLASS()
class UClassForFunctionParams : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void Parameters( RenderPipeline ScriptableRenderPipeline, bool UseStandardShaders, bool ShowFBXExportDialog,
					 bool ExportVertexShader, FString OutputDirectory );
};
struct ParamsStructure
{
	ParamsStructure();
	RenderPipeline ScriptableRenderPipeline;
	bool UseStandardShaders;
	bool ShowFBXExportDialog;
	bool ExportVertexShader;
	uint8 Empty[ 4 ];
	FString OutputDirectory;
};

class FUnrealToUnityModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void BindEditorCommands();
	void AddCustomMenu( FToolBarBuilder& ToolbarBuilder );
	TSharedRef<SWidget> CreateMenuContent();
	void Export();
	static void DoExport( RenderPipeline Pipeline, FString OutputFolder );
	static void RemoveVirtualTextures();
	static void DoRemoveNanite();
	static void RemoveRVTs();
	static void RemoveVertexInterpolators();
	static void RemoveUnsupportedMaterialNodes();
		static void RemoveDitherTemporalAA();
		static void RemoveParallaxMapping();
		static void RemoveDepthFade();
	static void MoveFoliageToActors();
	static void FixVolumeMaterials();
	static void ConvertISMToActors();
	void PlaceAllAssetsInScene();
	void ConvertLandscapesToStaticMeshes();
	static void DetachMultipleComponentActors();
	static void ConvertSplineMeshesToStaticActor( AActor* Actor );
	static void ConvertSplineMeshesToStaticActors();
	static void ConvertSkeletalMeshesToStaticActors();
	void FixLandscapeMaterials();
	void PrepareForExport();
	static void DoPrepareForExport();
	void RemoveNanite();
	void DetectMaterialsWithWPO();

	TSharedPtr<FUICommandList> EditorCommands;
};


class FUnrealToUnity_Commands : public TCommands<FUnrealToUnity_Commands>
{
public:
	FUnrealToUnity_Commands()
		: TCommands<FUnrealToUnity_Commands>( "UnrealToUnityEditor", NSLOCTEXT( "Contexts", "UnrealToUnityEditor", "UnrealToUnityEditor" ), NAME_None, FEditorStyle::GetStyleSetName() )
	{
	}

	TSharedPtr<FUICommandInfo> Export;
	TSharedPtr<FUICommandInfo> PlaceAllAssetsInScene;
	TSharedPtr<FUICommandInfo> ConvertLandscapesToStaticMeshes;
	TSharedPtr<FUICommandInfo> PrepareForExport;
	TSharedPtr<FUICommandInfo> DetectMaterialsWithWPO;
	
	virtual void RegisterCommands() override;
};
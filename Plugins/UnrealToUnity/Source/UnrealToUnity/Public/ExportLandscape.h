#pragma once

#include "ExportToUnity.h"

#include "UnityScene.h"
#include "Module.h"

#include "Runtime/Core/Public/GenericPlatform/GenericPlatformFile.h"
#include "Runtime/Core/Public/HAL/PlatformFilemanager.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "ImageUtils.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"

#include "Runtime/Engine/Classes/Components/LightComponent.h"
#include "Runtime/Engine/Classes/Components/PointLightComponent.h"
#include "Runtime/Engine/Classes/Components/SpotLightComponent.h"
#include "Runtime/Engine/Classes/Components/RectLightComponent.h"
#include "Runtime/Engine/Classes/Components/DirectionalLightComponent.h"
#include "Runtime/Engine/Classes/Components/SkeletalMeshComponent.h"
#include "Runtime/Engine/Classes/Components/SplineComponent.h"
#include "Runtime/Engine/Classes/Components/SplineMeshComponent.h"
#include "Runtime/Engine/Classes/Components/SphereReflectionCaptureComponent.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "Runtime/Engine/Classes/Materials/Material.h"
#include "Runtime/Engine/Private/Materials/MaterialUniformExpressions.h"
#include "Runtime/Engine/Classes/Materials/MaterialInstance.h"

#include "Runtime/Engine/Classes/Materials/MaterialExpressionMultiply.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionConstant.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionCollectionParameter.h"
#include "Runtime/Engine/Classes/Materials/MaterialParameterCollection.h"

#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/Paths.h"

#include "Runtime/Engine/Classes/Materials/MaterialInstanceDynamic.h"

//Landscape
#include "Runtime/Landscape/Classes/Landscape.h"
#include "Runtime/Landscape/Classes/LandscapeStreamingProxy.h"
#include "LandscapeDataAccess.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "AssetToolsModule.h"
#include "Runtime/Engine/Classes/Engine/StaticMeshActor.h"

class LandscapeDataForPatch
{
public:

	UPROPERTY()
	UTexture2D* HeightMap = nullptr;
	FVector LandscapePosition;
	FVector LandscapeScale;
	FVector LandscapeSize;
};

class LandscapeMeshData
{
public:
	LandscapeMeshData()
	{
		MeshCenter = FVector( 0, 0, 0 );
	};
	~LandscapeMeshData()
	{
		if( MeshDescription )
			delete MeshDescription;
		ProceduralMesh = nullptr;
		StaticMesh = nullptr;
	}	
	
	ALandscapeProxy* Landscape = nullptr;
	FVector MeshCenter;
	FString MeshAssetName;
	UPROPERTY()
	UProceduralMeshComponent* ProceduralMesh = nullptr;
	UPROPERTY()
	UStaticMesh* StaticMesh = nullptr;
	FMeshDescription* MeshDescription = nullptr;
	TMap<UMaterialInterface*, FPolygonGroupID> UniqueMaterials;	
};

class FLandscapeTools
{
public:
	FLandscapeTools()
	{
		MeshesGenerated = 0;
	}
	void CreateAssets( UWorld* world );
	void GetDataFromlandscape( ALandscapeProxy* Landscape, int TessellationLevel, bool Winding );	
	UProceduralMeshComponent* CreateNewProceduralComponent( UObject* Outer );
	UStaticMesh* CreateStaticMeshFromProceduralMesh( FString UserPackageName, LandscapeMeshData* MeshData );
	
	void FinalizeMesh( LandscapeMeshData* MeshData );	

	UMaterialInterface* LandscapeMaterial = nullptr;
	ALandscapeProxy* Landscape = nullptr;
	bool Winding;
	
	FTransform LandscapeTransform;

	FVector PlayerPosition;
	LandscapeDataForPatch Data;

	TArray<FVector> InitialVertices;
	TArray<FVector> normals;
	TArray<FVector2D> UV0;
	TArray<FVector2D> UV1;
	TArray<FProcMeshTangent> tangents;
	TArray<FLinearColor> vertexColors;
	
	UWorld* world = nullptr;
	std::atomic<int> MeshesGenerated;

	TArray< FGraphEventRef> TaskHandles;
	TArray< LandscapeMeshData*> Patches;
};

extern FLandscapeTools LandscapeTools;
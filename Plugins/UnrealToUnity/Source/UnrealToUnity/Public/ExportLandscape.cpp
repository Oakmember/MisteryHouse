

#include "ExportLandscape.h"

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
#include "Runtime/Landscape/Classes/LandscapeInfo.h"
#include "Runtime/Landscape/Public/LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "AssetToolsModule.h"
#include "Runtime/Engine/Classes/Engine/StaticMeshActor.h"
#include "Runtime/Landscape/Classes/Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "Runtime/Landscape/Classes/Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Runtime/Landscape/Classes/Materials/MaterialExpressionLandscapeLayerSample.h"
#include "Runtime/Landscape/Classes/Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionTextureCoordinate.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionScalarParameter.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionVectorParameter.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionComponentMask.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionAdd.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionCustom.h"
#include "Editor/MaterialEditor/Public/MaterialEditingLibrary.h"
#include "Engine/Classes/Materials/MaterialExpressionConstant4Vector.h"

LandscapeDataForPatch GetData( ALandscapeProxy* Landscape )
{
	LandscapeDataForPatch Data;

	if( Landscape )
	{
		for( int32 ComponentIndex = 0; ComponentIndex < Landscape->LandscapeComponents.Num(); ComponentIndex++ )
		{
			ULandscapeComponent* Component = Landscape->LandscapeComponents[ ComponentIndex ];

			Data.HeightMap = Component->GetHeightmap();
		}

		FTransform LandscapeTransform = Landscape->GetActorTransform();
		Data.LandscapePosition = LandscapeTransform.GetLocation();
		Data.LandscapeScale = LandscapeTransform.GetScale3D();

		int NumComponentsPerAxis = FMath::Sqrt( Landscape->LandscapeComponents.Num() );
		Data.LandscapeSize = FVector( Landscape->ComponentSizeQuads * NumComponentsPerAxis, Landscape->ComponentSizeQuads * NumComponentsPerAxis, 0 );
	}

	return Data;
}
FLandscapeTools LandscapeTools;


FAssetToolsModule* AssetToolsModule = nullptr;
void FLandscapeTools::CreateAssets( UWorld* InWorld )
{
	AssetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );

	int TessellationLevel = 0;	
	bool bWinding = true;

	this->world = InWorld;

	for( TActorIterator<AActor> iterator( world ); iterator; ++iterator )
	{
		AActor* Actor = *iterator;
		ALandscapeProxy* Proxy = Cast< ALandscapeProxy>( Actor );
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor>( Actor );

		if( Proxy )
			GetDataFromlandscape( Proxy, TessellationLevel, bWinding );
	}
}

UTexture2D* CreateTexture( int X, int Y, EPixelFormat PixelFormat, void* PixelData, FName Name, UObject* Outer )
{
	UTexture2D* NewTexture = NewObject<UTexture2D>(
		Outer,
		Name,
		RF_Public | RF_Standalone | RF_Transactional | RF_WasLoaded | RF_LoadCompleted
		);
	ETextureSourceFormat Formats[] = { TSF_G8 };
	NewTexture->Source.InitLayered(
		X,
		Y,
		1,
		1,
		1,
		Formats,
		(uint8*)PixelData
	);

	NewTexture->PlatformData = new FTexturePlatformData();
	NewTexture->PlatformData->SizeX = X;
	NewTexture->PlatformData->SizeY = Y;
	NewTexture->PlatformData->PixelFormat = PixelFormat;

	// Allocate first mipmap.
	int32 NumBlocksX = X / GPixelFormats[ PixelFormat ].BlockSizeX;
	int32 NumBlocksY = Y / GPixelFormats[ PixelFormat ].BlockSizeY;
	FTexture2DMipMap* Mip = new FTexture2DMipMap();
	NewTexture->PlatformData->Mips.Add( Mip );
	Mip->SizeX = X;
	Mip->SizeY = Y;
	Mip->BulkData.Lock( LOCK_READ_WRITE );
	Mip->BulkData.Realloc( NumBlocksX * NumBlocksY * GPixelFormats[ PixelFormat ].BlockBytes );
	Mip->BulkData.Unlock();

	 //UTexture2D::CreateTransient( X, Y, PixelFormat );
	
	 NewTexture->SRGB = 0;
	 NewTexture->CompressionSettings = TextureCompressionSettings::TC_EditorIcon;
	 NewTexture->MipGenSettings = TMGS_NoMipmaps;
	
	int TargetMips = 1;
	for( int i = 0; i < TargetMips; i++ )
	{
		FTexture2DMipMap& targetMip = NewTexture->PlatformData->Mips[ i ];

		int32 size = targetMip.BulkData.GetBulkDataSize();

		FColor* targetData = (FColor*)( targetMip.BulkData.Lock( LOCK_READ_WRITE ) );

		FMemory::Memcpy( targetData, PixelData, size );

		targetMip.BulkData.Unlock();
	}
	NewTexture->UpdateResource();
	return NewTexture;
}
FString GetSafeLayerName( FString OriginalName )
{
	FString InvalidCharacters = TEXT( "~!@#$%^&*()+-[]{}|\\;\'\"<>/?,. " );
	const int NumInvalidKeywords = 2;
	FString InvalidKeywords[ NumInvalidKeywords ] = { TEXT( "auto" ), TEXT( "float" ) };

	for( int i = 0; i < NumInvalidKeywords; i++ )
	{
		if( OriginalName.Compare( InvalidKeywords[ i ] ) == 0 )
		{
			OriginalName += TEXT( "_renamed" );
		}
	}
	for( int i = 0; i < InvalidCharacters.Len(); i++ )
	{
		FString Chr;
		Chr.AppendChar( InvalidCharacters[ i ] );
		OriginalName = OriginalName.Replace( *Chr, TEXT( "_" ) );
	}

	for( int i = 0; i < 10; i++ )
	{
		FString Figure = FString::Printf( TEXT( "%d" ), i );
		if( OriginalName.StartsWith( Figure ) )
		{
			OriginalName = FString( TEXT( "Layer" ) ) + OriginalName;
			break;
		}
	}

	return OriginalName;
}
FString GetSafeParameterName( UMaterial* Material, FString OriginalName )
{
	FMaterialParameterInfo Info;
	Info.Name = FName( OriginalName );
	UTexture* OutTexture = nullptr;
	bool Ret = Material->GetTextureParameterValue( Info, OutTexture );
	if( Ret )
		return OriginalName + TEXT( "_safe" );
	else
		return OriginalName;
}
void ExportLayerTextures( ALandscapeProxy* Proxy, UMaterialInterface* MatInterface, TArray<FString> SafeLayerNames )
{
	UMaterialInstanceDynamic* DynamicMaterial = Cast< UMaterialInstanceDynamic>( MatInterface );
	ULandscapeInfo* Info = Proxy->GetLandscapeInfo();

	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;

	if( !Info->GetLandscapeExtent( MinX, MinY, MaxX, MaxY ) )
	{
		return;
	}

	ULandscapeLayerInfoObject* LayerInfo = nullptr;
	for( int i = 0; i < Info->Layers.Num(); i++ )
	{
		FLandscapeInfoLayerSettings& LayerSettings = Info->Layers[ i ];
		LayerInfo = LayerSettings.LayerInfoObj;

		TArray<uint8> WeightData;
		WeightData.AddZeroed( ( MaxX - MinX + 1 ) * ( MaxY - MinY + 1 ) );

		FLandscapeEditDataInterface LandscapeEdit( Info );
		LandscapeEdit.GetWeightDataFast( LayerInfo, MinX, MinY, MaxX, MaxY, WeightData.GetData(), 0 );
		
		FString SafeLayerName = SafeLayerNames[ i ];//GetSafeLayerName( LayerSettings.LayerName.ToString() );
		FString NewTexturePath = FString::Printf( TEXT( "/Game/%s" ), *SafeLayerName );

		FString NewNameSuggestion = SafeLayerName;
		FString PackageName = FString( TEXT( "/Game/" ) ) + NewNameSuggestion;
		FString Name;

		AssetToolsModule->Get().CreateUniqueAssetName( PackageName, TEXT( "" ), PackageName, Name );

		#if ENGINE_MINOR_VERSION >= 26
			UPackage* NewPackage = CreatePackage( *NewTexturePath );
		#else
			UPackage* NewPackage = CreatePackage( nullptr, *NewTexturePath );
		#endif

		int X = MaxX - MinX + 1;
		int Y = MaxY - MinY + 1;
		
		#if ENGINE_MINOR_VERSION >= 26
			EPixelFormat Format = EPixelFormat::PF_R8;
		#else
			EPixelFormat Format = EPixelFormat::PF_G8;
		#endif
		UTexture2D* LayerTex = CreateTexture( X, Y, Format, &WeightData[ 0 ], FName(Name), NewPackage );
		//LayerTex->Source.LayerFormat[ 0 ] = TSF_G8;
		NewPackage->Modify();

		if ( DynamicMaterial )
			DynamicMaterial->SetTextureParameterValue( FName(SafeLayerName), LayerTex );
	}
}
void GetExpressionConnections( UMaterialExpression* Exp, UMaterial* Material, UMaterialFunction* MaterialFunction, TArray<UMaterialExpression*>& Connections,
							   TArray<FExpressionInput*>& OutputConnections );
void ConnectExpressionOutputToOtherExpression( UMaterial* BaseMaterial, UMaterialFunction* MaterialFunction, UMaterialExpression* TargetExpression,
											   UMaterialExpression* OtherExpression, int OutputIndex = 0, int Mask = 0, int MaskR = 0, int MaskG = 0, int MaskB = 0, int MaskA = 0 );
ULandscapeLayerInfoObject* GetLayer( ULandscapeInfo* Info, FName LayerName, int& IndexOut );
bool IsLayerValid( ULandscapeInfo* Info, FName LayerName );
UMaterialExpressionCustom* GenerateCustomExpForBlending( UMaterialExpressionLandscapeLayerBlend* LayerBlendExp, UMaterial* BaseMaterial,
														 FVector2D Location, ULandscapeInfo* Info, TArray<FString> SafeLayerNames )
{
	UMaterialExpression* NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, nullptr, UMaterialExpressionCustom::StaticClass(), nullptr, Location.X, Location.Y );
	UMaterialExpressionCustom* CustomExp = Cast< UMaterialExpressionCustom>( NewExp );

	FString Code = TEXT("float  lerpres;\n"
						"float  Local0;\n");
	
	bool OutputIsFloat4 = false;
	if( OutputIsFloat4 )
		CustomExp->OutputType = CMOT_Float4;
	else
		CustomExp->OutputType = CMOT_Float3;
	CustomExp->Inputs.Empty();

	TArray<FString> WeightmapVars;
	TArray<FString> ColorVars;
	TArray<FString> HeightVars;
	TArray<FString> FinalHeightVars;
	WeightmapVars.AddDefaulted( LayerBlendExp->Layers.Num() );
	ColorVars.AddDefaulted( LayerBlendExp->Layers.Num() );
	HeightVars.AddDefaulted( LayerBlendExp->Layers.Num() );
	FinalHeightVars.AddDefaulted( LayerBlendExp->Layers.Num() );

	for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
	{
		auto Layer = LayerBlendExp->Layers[ i ];
		if( !IsLayerValid( Info, Layer.LayerName ) )
		{
			continue;
		}
		FString SafeLayerName = GetSafeLayerName( Layer.LayerName.ToString() );
		FString Str = FString::Printf( TEXT( "%sWeight" ), *SafeLayerName );
		WeightmapVars[ i ] = Str;
		CustomExp->Inputs.Add( { FName( Str ) } );
	}
	for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
	{
		auto Layer = LayerBlendExp->Layers[ i ];
		if( !IsLayerValid( Info, Layer.LayerName ) )
		{
			continue;
		}
		FString SafeLayerName = GetSafeLayerName( Layer.LayerName.ToString() );
		ColorVars[ i ] = SafeLayerName;
		CustomExp->Inputs.Add( { FName( SafeLayerName ) } );
	}
	for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
	{
		auto Layer = LayerBlendExp->Layers[ i ];
		if( !IsLayerValid( Info, Layer.LayerName ) )
		{
			continue;
		}
		if( Layer.BlendType == LB_HeightBlend )
		{
			FString SafeLayerName = GetSafeLayerName( Layer.LayerName.ToString() );
			SafeLayerName += TEXT( "Height" );
			HeightVars[ i ] = SafeLayerName;
			CustomExp->Inputs.Add( { FName( SafeLayerName ) } );
		}
	}
	
	FString AllWeightsAndHeights = TEXT( "float  AllWeightsAndHeights = " );
	//Height calculations
	for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
	{
		auto Layer = LayerBlendExp->Layers[ i ];
		if( !IsLayerValid( Info, Layer.LayerName ) )
		{
			continue;
		}
		FString Str;
		if( Layer.BlendType == LB_HeightBlend )
		{
			FinalHeightVars[i] = FString::Printf(TEXT("Layer%dWithHeight"), i);

			Str = FString::Printf( TEXT(
				"lerpres = lerp( -1.0, 1.0, %s );\n"
				"Local0 = ( lerpres + %s );\n"
				"float %s = clamp(Local0, 0.0001, 1.0);\n" ), *WeightmapVars[i], *HeightVars[i], *FinalHeightVars[ i ] );

			AllWeightsAndHeights += FString::Printf( TEXT( "Layer%dWithHeight + " ), i );
		}
		else
		{
			AllWeightsAndHeights += FString::Printf( TEXT( "%s.r + "), *WeightmapVars[ i ] );
		}

		Str += "\n";
		Code += Str;
	}

	AllWeightsAndHeights += TEXT( "0;" );
	Code += FString::Printf( TEXT( "%s\n" ), *AllWeightsAndHeights );
	Code += FString::Printf( TEXT( "float  Divider = ( 1.0 / AllWeightsAndHeights );\n" ) );

	//Contributions
	FString Result = TEXT( "float3  Result = " );
	for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
	{
		auto Layer = LayerBlendExp->Layers[ i ];
		if( !IsLayerValid( Info, Layer.LayerName ) )
		{
			continue;
		}

		FString WeightVar = WeightmapVars[ i ];
		if( Layer.BlendType == LB_HeightBlend )
		{
			WeightVar = FinalHeightVars[ i ];
		}

		Code += FString::Printf( TEXT( "float3  Layer%dContribution = Divider.rrr * %s.rrr * %s;\n" ), i, *WeightVar, *ColorVars[ i ] );
		Result += FString::Printf( TEXT( "Layer%dContribution + " ), i );
	}

	Result += TEXT( "float3(0,0,0);\n" );

	Code += Result;
	if ( CustomExp->OutputType == CMOT_Float4 )
		Code += TEXT( "return float4( Result, 1.0 );" );
	else
		Code += TEXT( "return Result;" );

	CustomExp->Code = Code;

	return CustomExp;
}
UMaterialExpressionCustom* GenerateCustomExpForLayerWeight( UMaterialExpressionLandscapeLayerWeight* LayerWeightExp, UMaterial* BaseMaterial,
															FVector2D Location )
{
	UMaterialExpression* NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, nullptr, UMaterialExpressionCustom::StaticClass(), nullptr, Location.X, Location.Y );
	UMaterialExpressionCustom* CustomExp = Cast< UMaterialExpressionCustom>( NewExp );

	CustomExp->Description = TEXT( "LayerWeight" );
	CustomExp->OutputType = CMOT_Float3;
	CustomExp->Inputs.Empty();
	CustomExp->Inputs.Add( { FName( TEXT( "Base" ) ) } );
	CustomExp->Inputs.Add( { FName( TEXT( "Weight" ) ) } );
	CustomExp->Inputs.Add( { FName( TEXT( "Layer" ) ) } );

	FString Code = TEXT( "return Base.rgb + Weight.rrr * Layer;" );
	CustomExp->Code = Code;

	return CustomExp;
}
UMaterialExpressionConstant4Vector* CreateVector4Expression( UMaterial* BaseMaterial, UMaterialFunction* MaterialFunction, UMaterialExpression* ParentExpression )
{
	FVector2D Location( ParentExpression->MaterialExpressionEditorX - 150, ParentExpression->MaterialExpressionEditorY - 200 );
	auto NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, MaterialFunction, UMaterialExpressionConstant4Vector::StaticClass(), nullptr, Location.X, Location.Y );
	UMaterialExpressionConstant4Vector* NewExpC = Cast < UMaterialExpressionConstant4Vector>( NewExpression );
	return NewExpC;
}
UMaterialExpressionConstant* CreateConstantExpression( UMaterial* BaseMaterial, UMaterialFunction* MaterialFunction, UMaterialExpression* ParentExpression )
{
	FVector2D Location( ParentExpression->MaterialExpressionEditorX - 150, ParentExpression->MaterialExpressionEditorY - 200 );
	auto NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, MaterialFunction, UMaterialExpressionConstant::StaticClass(), nullptr, Location.X, Location.Y );
	UMaterialExpressionConstant* NewExpC = Cast < UMaterialExpressionConstant>( NewExpression );
	return NewExpC;
}
const TArray<UMaterialExpression*> GetFunctionExpressions( UMaterialFunctionInterface* MaterialFunction );
TArray<UMaterialFunction*> GetAllMaterialFunctions( UMaterial* BaseMaterial )
{
	TArray<UMaterialFunction*> MaterialFunctions;
	TArray<UMaterialExpression*> AllExpressions = BaseMaterial->Expressions;
	for( int i = 0; i < AllExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = AllExpressions[ i ];
		UMaterialExpressionMaterialFunctionCall* MFCall = Cast< UMaterialExpressionMaterialFunctionCall >( Exp );
		if( MFCall )
		{
			UMaterialFunction* MF = Cast< UMaterialFunction>( MFCall->MaterialFunction );
			if ( MF )
				MaterialFunctions.Add( MF );
		}
	}

	return MaterialFunctions;
}
int ReplaceLandscapeCoordsWithTexcoord( UMaterial* BaseMaterial, UMaterialFunction* MaterialFunction, FVector2D Resolution )
{
	TArray<UMaterialExpression*> AllExpressions;
	if( BaseMaterial )
	{
		AllExpressions = BaseMaterial->Expressions;

		TArray<UMaterialFunction*> AllMaterialFunctions = GetAllMaterialFunctions( BaseMaterial );
		for( int i = 0; i < AllMaterialFunctions.Num(); i++ )
		{
			int FunctionReplacements = ReplaceLandscapeCoordsWithTexcoord( nullptr, AllMaterialFunctions[ i ], Resolution );
			if ( FunctionReplacements > 0 )
				UMaterialEditingLibrary::UpdateMaterialFunction( AllMaterialFunctions[ i ], nullptr );
		}
	}
	else if( MaterialFunction )
	{
		AllExpressions = GetFunctionExpressions( MaterialFunction );
	}

	int Replacements = 0;
	for( int i = 0; i < AllExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = AllExpressions[ i ];
		UMaterialExpressionLandscapeLayerCoords* LandscapeCoords = Cast< UMaterialExpressionLandscapeLayerCoords>( Exp );
		UMaterialExpressionTextureCoordinate* OriginalTexcoordNode = Cast< UMaterialExpressionTextureCoordinate>( Exp );
		if( LandscapeCoords || OriginalTexcoordNode )
		{
			Replacements++;
			FVector2D Location( Exp->MaterialExpressionEditorX, Exp->MaterialExpressionEditorY - 200 );
			UMaterialExpression* NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, MaterialFunction, UMaterialExpressionTextureCoordinate::StaticClass(), nullptr, Location.X, Location.Y );
			UMaterialExpressionTextureCoordinate* NewTexcoordExp = Cast< UMaterialExpressionTextureCoordinate>( NewExp );

			UMaterialExpressionConstant4Vector* AddOffsetValueNode = CreateVector4Expression( BaseMaterial, MaterialFunction, Exp );
			float Offset = 1.0f / FMath::Min( Resolution.X, Resolution.Y );
			AddOffsetValueNode->Constant = FLinearColor( 2 * Offset, 2 * Offset, 2 * Offset, 2 * Offset );

			Location += FVector2D( 0, 70 );
			NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, MaterialFunction, UMaterialExpressionComponentMask::StaticClass(), nullptr, Location.X, Location.Y );
			UMaterialExpressionComponentMask* ComponentMaskForAddOffsetValue = Cast< UMaterialExpressionComponentMask>( NewExp );
			ComponentMaskForAddOffsetValue->R = 1;
			ComponentMaskForAddOffsetValue->G = 1;
			ComponentMaskForAddOffsetValue->B = 0;
			ComponentMaskForAddOffsetValue->A = 0;
			ComponentMaskForAddOffsetValue->GetInput( 0 )->Expression = AddOffsetValueNode;

			Location += FVector2D( 150, 0 );
			NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, MaterialFunction, UMaterialExpressionAdd::StaticClass(), nullptr, Location.X, Location.Y );
			UMaterialExpressionAdd* AddOffset = Cast< UMaterialExpressionAdd>( NewExp );
			AddOffset->GetInput( 0 )->Expression = NewTexcoordExp;
			AddOffset->GetInput( 1 )->Expression = ComponentMaskForAddOffsetValue;

			Location += FVector2D( 150, 0 );
			NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, MaterialFunction, UMaterialExpressionMultiply::StaticClass(), nullptr, Location.X, Location.Y );
			UMaterialExpressionMultiply* MultiplyExp = Cast< UMaterialExpressionMultiply>( NewExp );

			Location += FVector2D( 0, 70 );
			NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, MaterialFunction, UMaterialExpressionComponentMask::StaticClass(), nullptr, Location.X, Location.Y );
			UMaterialExpressionComponentMask* ComponentMaskForTexcoordScale = Cast< UMaterialExpressionComponentMask>( NewExp );
			ComponentMaskForTexcoordScale->R = 1;
			ComponentMaskForTexcoordScale->G = 1;
			ComponentMaskForTexcoordScale->B = 0;
			ComponentMaskForTexcoordScale->A = 0;

			Location += FVector2D( 0, 70 );
			NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, MaterialFunction, UMaterialExpressionVectorParameter::StaticClass(), nullptr, Location.X, Location.Y );
			UMaterialExpressionVectorParameter* TexcoordScaleParam = Cast< UMaterialExpressionVectorParameter>( NewExp );

			float MappingScale = 1.0f;
			if( LandscapeCoords )
			{
				MappingScale = LandscapeCoords->MappingScale;
				if( LandscapeCoords->MappingScale == 0.0f )
					MappingScale = 1.0f;
			}
			TexcoordScaleParam->DefaultValue = FLinearColor( Resolution.X / MappingScale, Resolution.Y / MappingScale, 1, 1 );
			TexcoordScaleParam->SetParameterName( "TexcoordScale" );

			MultiplyExp->GetInput( 0 )->Expression = AddOffset;
			MultiplyExp->GetInput( 1 )->Expression = ComponentMaskForTexcoordScale;

			ComponentMaskForTexcoordScale->GetInput( 0 )->Expression = TexcoordScaleParam;

			ConnectExpressionOutputToOtherExpression( BaseMaterial, MaterialFunction, Exp, MultiplyExp);
		}
	}

	return Replacements;
}
ULandscapeLayerInfoObject* GetLayer( ULandscapeInfo* Info, FName LayerName, int& IndexOut)
{
	for( int i = 0; i < Info->Layers.Num(); i++ )
	{
		FLandscapeInfoLayerSettings& LayerSettings = Info->Layers[ i ];
		ULandscapeLayerInfoObject* LayerInfo = LayerSettings.LayerInfoObj;
		if( !LayerInfo )
			continue;

		if( LayerInfo->LayerName.Compare( LayerName ) == 0 )
		{
			IndexOut = i;
			return LayerInfo;
		}
	}

	IndexOut = -1;
	return nullptr;
}
int GetInputIndexForLayer( ULandscapeInfo* Info, UMaterialExpressionLandscapeLayerBlend* LayerBlendExp, int LayerIndex )
{
	int Index = 0;
	for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
	{
		if( i == LayerIndex )
			return Index;
		FLayerBlendInput& Layer = LayerBlendExp->Layers[ i ];
		if (!IsLayerValid( Info, Layer.LayerName ))		
			continue;
		Index++;
	}

	return Index;
}
int GetNumValidLayers( ULandscapeInfo* Info, UMaterialExpressionLandscapeLayerBlend* LayerBlendExp )
{
	int Index = 0;
	for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
	{
		FLayerBlendInput& Layer = LayerBlendExp->Layers[ i ];
		if( !IsLayerValid( Info, Layer.LayerName ) )
			continue;
		Index++;
	}

	return Index;
}
UMaterialExpressionTextureSampleParameter2D* GetWeightMapParameter( TArray< UMaterialExpressionTextureSampleParameter2D*> WeightMapParameters, FString LayerName )
{
	for( int i = 0; i < WeightMapParameters.Num(); i++ )
	{
		if( WeightMapParameters[i] && WeightMapParameters[i]->ParameterName.Compare(FName(LayerName)) == 0 )
			return WeightMapParameters[ i ];
	}

	return nullptr;
}
bool IsLayerValid( ULandscapeInfo* Info, FName LayerName )
{
	int IndexOut;
	ULandscapeLayerInfoObject* LayerObject = GetLayer( Info, LayerName, IndexOut );
	if( LayerObject )
		return true;

	return false;
}

void ModifyTerrainMaterial( ALandscapeProxy* Proxy, UMaterialInterface* MatInterface, FVector2D Resolution, TArray<FString> SafeLayerNames )
{
	UMaterialInstanceDynamic* DynamicMaterial = Cast< UMaterialInstanceDynamic>( MatInterface );
	UMaterial* BaseMaterial = MatInterface->GetBaseMaterial();
	
	ReplaceLandscapeCoordsWithTexcoord( BaseMaterial, nullptr, Resolution );

	ULandscapeInfo* Info = Proxy->GetLandscapeInfo();
	FVector2D TreeOffset = FVector2D( 0, -300 );

	FVector2D Location( -300, 0 );
	Location += TreeOffset;
	UMaterialExpression* NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, nullptr, UMaterialExpressionTextureCoordinate::StaticClass(), nullptr, Location.X, Location.Y );
	UMaterialExpressionTextureCoordinate* WeightTexcoordExp = Cast< UMaterialExpressionTextureCoordinate>( NewExp );

	Location = FVector2D( -100, 0 ) + TreeOffset;
	NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, nullptr, UMaterialExpressionMultiply::StaticClass(), nullptr, Location.X, Location.Y );
	UMaterialExpressionMultiply* MultiplyExp = Cast< UMaterialExpressionMultiply>( NewExp );

	Location = FVector2D( -300, 100 ) + TreeOffset;
	NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, nullptr, UMaterialExpressionScalarParameter::StaticClass(), nullptr, Location.X, Location.Y );
	UMaterialExpressionScalarParameter* WeightMapScaleParam = Cast< UMaterialExpressionScalarParameter>( NewExp );

	TArray<UMaterialExpressionLandscapeLayerBlend*> LayerBlendExpressions;
	TArray<UMaterialExpressionLandscapeLayerSample*> LayerSampleExpressions;
	TArray<UMaterialExpressionLandscapeLayerWeight*> LayerWeightExpressions;
	
	for( int i = 0; i < BaseMaterial->Expressions.Num(); i++ )
	{
		UMaterialExpression* Exp = BaseMaterial->Expressions[ i ];
		UMaterialExpressionLandscapeLayerBlend* LayerBlendExp = Cast< UMaterialExpressionLandscapeLayerBlend>( Exp );
		if( LayerBlendExp )
		{
			LayerBlendExpressions.Add( LayerBlendExp );
		}
		UMaterialExpressionLandscapeLayerSample* LayerSampleExp = Cast< UMaterialExpressionLandscapeLayerSample>( Exp );
		if( LayerSampleExp )
		{
			LayerSampleExpressions.Add( LayerSampleExp );
		}
		UMaterialExpressionLandscapeLayerWeight* LayerWeightExp = Cast< UMaterialExpressionLandscapeLayerWeight>( Exp );
		if( LayerWeightExp )
		{
			LayerWeightExpressions.Add( LayerWeightExp );
		}
	}
	
	Location = FVector2D( 300, 100 ) + TreeOffset;
	TArray<UMaterialExpressionCustom*> CustomExpressions;
	for( int i = 0; i < LayerBlendExpressions.Num(); i++ )
	{
		UMaterialExpressionLandscapeLayerBlend* LayerBlendExp = LayerBlendExpressions[ i ];
		if( LayerBlendExp )
		{
			Location = FVector2D( LayerBlendExp->MaterialExpressionEditorX + 50, LayerBlendExp->MaterialExpressionEditorY );
			UMaterialExpressionCustom* CustomExp = GenerateCustomExpForBlending( LayerBlendExp, BaseMaterial, Location, Info, SafeLayerNames );
			CustomExpressions.Add( CustomExp );
		}
	}

	WeightMapScaleParam->DefaultValue = 1.0f;
	WeightMapScaleParam->SetParameterName( "WeightMapScale" );

	MultiplyExp->GetInput( 0 )->Expression = WeightTexcoordExp;
	MultiplyExp->GetInput( 1 )->Expression = WeightMapScaleParam;

	TArray< UMaterialExpressionTextureSampleParameter2D*> WeightMapParameters;
	for( int i = 0; i < Info->Layers.Num(); i++ )
	{
		FLandscapeInfoLayerSettings& LayerSettings = Info->Layers[ i ];
		ULandscapeLayerInfoObject* LayerInfo = LayerSettings.LayerInfoObj;
		if( !LayerInfo )
		{
			WeightMapParameters.Add( nullptr );
			continue;
		}
		
		Location = FVector2D( 50, i * 200 ) + TreeOffset;
		
		NewExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( BaseMaterial, nullptr, UMaterialExpressionTextureSampleParameter2D::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionTextureSampleParameter2D* SampleExpression = Cast< UMaterialExpressionTextureSampleParameter2D>( NewExp );
		FString SafeLayerName = SafeLayerNames[ i ];// GetSafeLayerName( LayerInfo->LayerName.ToString() );
		SampleExpression->SetParameterName( FName(SafeLayerName) );
		if ( SampleExpression && SampleExpression->GetInput( 0 ) )
			SampleExpression->GetInput( 0 )->Expression = MultiplyExp;
		SampleExpression->SamplerSource = ESamplerSourceMode::SSM_Clamp_WorldGroupSettings;

		//for( int u = 0; u < CustomExpressions.Num(); u++ )
		//{
		//	auto CustomExp = CustomExpressions[ u ];
		//	CustomExp->GetInput( NumValidLayers )->Expression = SampleExpression;
		//	CustomExp->GetInput( NumValidLayers )->OutputIndex = 1;
		//}

		WeightMapParameters.Add( SampleExpression );
	}

	for( int i = 0; i < LayerSampleExpressions.Num(); i++ )
	{
		UMaterialExpressionLandscapeLayerSample* LayerSampleExp = LayerSampleExpressions[ i ];

		int IndexOut = -1;
		ULandscapeLayerInfoObject* Layer = GetLayer( Info, LayerSampleExp->ParameterName, IndexOut );
		if( Layer )
		{
			UMaterialExpressionTextureSampleParameter2D* SampleTexExp = WeightMapParameters[ IndexOut ];
			if ( SampleTexExp )
				ConnectExpressionOutputToOtherExpression( BaseMaterial, nullptr, LayerSampleExp, SampleTexExp, 1, 1, 1, 0, 0, 0 );//OutputIndex=1, Mask=1, MaskR=1
		}
	}

	for( int i = 0; i < LayerWeightExpressions.Num(); i++ )
	{
		UMaterialExpressionLandscapeLayerWeight* LayerWeightExp = LayerWeightExpressions[ i ];

		int IndexOut = -1;
		ULandscapeLayerInfoObject* Layer = GetLayer( Info, LayerWeightExp->ParameterName, IndexOut );
		if( Layer )
		{
			FVector2D NewExpLocation( LayerWeightExp->MaterialExpressionEditorX , LayerWeightExp->MaterialExpressionEditorY );
			UMaterialExpressionCustom*  CustomExp = GenerateCustomExpForLayerWeight( LayerWeightExp, BaseMaterial, NewExpLocation );
			UMaterialExpressionTextureSampleParameter2D* SampleTexExp = WeightMapParameters[ IndexOut ];

			if ( LayerWeightExp->Base.Expression )
				*CustomExp->GetInput( 0 ) = LayerWeightExp->Base;
			else
			{
				UMaterialExpressionConstant4Vector* ZeroExp = CreateVector4Expression( BaseMaterial, nullptr, CustomExp );
				ZeroExp->Constant = FLinearColor( 0,0,0,0 );
				CustomExp->GetInput( 0 )->Expression = ZeroExp;
			}
			CustomExp->GetInput( 1 )->Expression = SampleTexExp;
			*CustomExp->GetInput( 2 ) = LayerWeightExp->Layer;

			ConnectExpressionOutputToOtherExpression( BaseMaterial, nullptr, LayerWeightExp, CustomExp, 0, 0, 0, 0, 0, 0 );
		}
	}

	UMaterialExpression* NullVector4Expression = nullptr;
	UMaterialExpressionConstant* ZeroExpression = nullptr;
	for( int i = 0; i < LayerBlendExpressions.Num(); i++ )
	{		
		UMaterialExpressionLandscapeLayerBlend* LayerBlendExp = LayerBlendExpressions[i];
		if( LayerBlendExp )
		{
			auto CustomExp = CustomExpressions[ i ];
			int HeightIndex = 0;
			for( int u = 0; u < LayerBlendExp->Layers.Num(); u++ )
			{
				FLayerBlendInput& Layer = LayerBlendExp->Layers[ u ];
				int IndexOut;
				auto LayerObject = GetLayer( Info, Layer.LayerName, IndexOut );
				if( !LayerObject )
					continue;
				int AllLayers = GetNumValidLayers( Info, LayerBlendExp );

				int WeightInputIndex = GetInputIndexForLayer( Info, LayerBlendExp, u );
				FString SafeLayerName = GetSafeLayerName( Layer.LayerName.ToString() );
				auto Weightmap = GetWeightMapParameter( WeightMapParameters, SafeLayerName );
				CustomExp->GetInput( WeightInputIndex )->Expression = Weightmap;
				CustomExp->GetInput( WeightInputIndex )->OutputIndex = 1;
				CustomExp->GetInput( WeightInputIndex )->Mask  = 1;
				CustomExp->GetInput( WeightInputIndex )->MaskR = 1;
				CustomExp->GetInput( WeightInputIndex )->MaskG = 0;
				CustomExp->GetInput( WeightInputIndex )->MaskB = 0;
				CustomExp->GetInput( WeightInputIndex )->MaskA = 0;

				int ColorInputIndex = AllLayers + WeightInputIndex;
				if( ColorInputIndex != -1 )
				{
					if( Layer.LayerInput.Expression )
					{
						*CustomExp->GetInput( ColorInputIndex ) = Layer.LayerInput;
					}
					else
					{
						if (!NullVector4Expression )
							NullVector4Expression = CreateVector4Expression( BaseMaterial, nullptr, LayerBlendExp );
						CustomExp->GetInput( ColorInputIndex )->Expression = NullVector4Expression;
					}
					if( Layer.BlendType == LB_HeightBlend )
					{
						int HeightInputIndex = 2 * AllLayers + HeightIndex;
						HeightIndex++;
						
						if( Layer.HeightInput.Expression )
						{
							*CustomExp->GetInput( HeightInputIndex ) = Layer.HeightInput;
						}
						else
						{
							if( !ZeroExpression )
							{
								ZeroExpression = CreateConstantExpression( BaseMaterial, nullptr, LayerBlendExp );
								ZeroExpression->R = 0.0f;
							}
							CustomExp->GetInput( HeightInputIndex )->Expression = ZeroExpression;
						}
					}
				}
			}
			
			ConnectExpressionOutputToOtherExpression( BaseMaterial, nullptr, LayerBlendExp, CustomExp );
		}
	}

	BaseMaterial->PostEditChange();
	BaseMaterial->MarkPackageDirty();

	UMaterialEditingLibrary::RecompileMaterial( BaseMaterial );
}
void FLandscapeTools::GetDataFromlandscape( ALandscapeProxy* InLandscape, int TessellationLevel, bool InWinding )
{
	Landscape = InLandscape;
	Winding = InWinding;

	int32 MinX = MAX_int32, MinY = MAX_int32;
	int32 MaxX = MIN_int32, MaxY = MIN_int32;

	// Find range of entire landscape
	for( int32 ComponentIndex = 0; ComponentIndex < Landscape->LandscapeComponents.Num(); ComponentIndex++ )
	{
		ULandscapeComponent* Component = Landscape->LandscapeComponents[ ComponentIndex ];

		Component->GetComponentExtent( MinX, MinY, MaxX, MaxY );
	}

	const int32 ComponentSizeQuads = ( ( Landscape->ComponentSizeQuads + 1 ) >> Landscape->ExportLOD ) - 1;
	const float ScaleFactor = (float)Landscape->ComponentSizeQuads / (float)ComponentSizeQuads;
	const int32 NumComponents = Landscape->LandscapeComponents.Num();
	const int32 VertexCountPerComponent = FMath::Square( ComponentSizeQuads + 1 );
	const int32 VertexCount = NumComponents * VertexCountPerComponent;
	const int32 TriangleCount = NumComponents * FMath::Square( ComponentSizeQuads ) * 2;
	const FVector2D UVScale = FVector2D( 1.0f, 1.0f ) / FVector2D( ( MaxX - MinX ) + 1, ( MaxY - MinY ) + 1 );
	const FVector2D Resolution = FVector2D( ( MaxX - MinX ) + 1, ( MaxY - MinY ) + 1 );

	UE_LOG( LogTemp, Log, TEXT( "GetDataFromlandscape TriangleCount %d" ), TriangleCount );

	if( TriangleCount > 10000000 )
	{
		FString Text = FString::Printf( TEXT( "Terrain has %d Triangles. Over 10M triangles it will need over 100GB of RAM and performance will be abysmal in Unity as well.\n"
											  "Do you still want to continue with the export ?"), TriangleCount );
		FText Txt = FText::FromString( Text );

		EAppReturnType::Type Result = FMessageDialog::Open( EAppMsgType::YesNo, Txt );
		if( Result == EAppReturnType::Type::No )
			return;
	}

	InitialVertices.Reset();
	normals.Reset();
	UV0.Reset();
	UV1.Reset();
	tangents.Reset();
	
	InitialVertices.Reserve( VertexCount );
	normals.Reserve( VertexCount );
	UV0.Reserve( VertexCount );
	UV1.Reserve( VertexCount );
	tangents.Reserve( VertexCount );

	TArray<uint8> VisibilityData;
	VisibilityData.Empty( VertexCount );
	VisibilityData.AddZeroed( VertexCount );
	bool FlipWinding = false;
	
	for( int32 ComponentIndex = 0, SelectedComponentIndex = 0; ComponentIndex < Landscape->LandscapeComponents.Num(); ComponentIndex++ )
	{
		ULandscapeComponent* Component = Landscape->LandscapeComponents[ ComponentIndex ];

		int LOD = 0;
		FLandscapeComponentDataInterface CDI( Component, LOD );
		const int32 BaseVertIndex = SelectedComponentIndex++ * VertexCountPerComponent;

		TArray<FWeightmapLayerAllocationInfo>& ComponentWeightmapLayerAllocations = Component->GetWeightmapLayerAllocations();
		TArray<uint8> CompVisData;
		for( int32 AllocIdx = 0; AllocIdx < ComponentWeightmapLayerAllocations.Num(); AllocIdx++ )
		{
			FWeightmapLayerAllocationInfo& AllocInfo = ComponentWeightmapLayerAllocations[ AllocIdx ];
			if( AllocInfo.LayerInfo == ALandscapeProxy::VisibilityLayer )
			{
				CDI.GetWeightmapTextureData( AllocInfo.LayerInfo, CompVisData );
			}
		}

		if( CompVisData.Num() > 0 )
		{
			for( int32 i = 0; i < VertexCountPerComponent; ++i )
			{
				VisibilityData[ BaseVertIndex + i ] = CompVisData[ CDI.VertexIndexToTexel( i ) ];
			}
		}
		FVector Scale = Component->GetComponentTransform().GetScale3D();
		if( Scale.X < 0 || Scale.Y < 0 || Scale.Z < 0 )
			FlipWinding = true;
		for( int32 VertIndex = 0; VertIndex < VertexCountPerComponent; VertIndex++ )
		{
			int32 VertX, VertY;
			CDI.VertexIndexToXY( VertIndex, VertX, VertY );

			FVector Scale3D = Component->GetComponentTransform().GetScale3D();
			FVector Position = CDI.GetLocalVertex( VertX, VertY ) + Component->GetRelativeLocation();
			Position *= Scale3D;
			bool MakeItFlat = false;// true;
			if( MakeItFlat )
				Position.Z = 0;
			InitialVertices.Add( Position );

			FVector Normal, TangentX, TangentY;
			CDI.GetLocalTangentVectors( VertX, VertY, TangentX, TangentY, Normal );

			Normal /= Component->GetComponentTransform().GetScale3D(); Normal.Normalize();
			TangentX /= Component->GetComponentTransform().GetScale3D(); TangentX.Normalize();
			TangentY /= Component->GetComponentTransform().GetScale3D(); TangentY.Normalize();

			if( MakeItFlat )
			{
				normals.Add( FVector( 0, 0, 1 ) );
			}
			else
				normals.Add( Normal );

			tangents.Add( FProcMeshTangent( TangentX.X, TangentX.Y, TangentX.Z ) );

			FVector2D TextureUV = FVector2D( VertX * ScaleFactor + Component->GetSectionBase().X, VertY * ScaleFactor + Component->GetSectionBase().Y );


			FVector2D WeightmapUV = ( TextureUV - FVector2D( MinX, MinY ) ) * UVScale;
			bool UseWeightmap = true;
			//if( UseWeightmap )
				UV0.Add( WeightmapUV );
			//else
				UV1.Add( TextureUV );
		}
	}
	
	LandscapeTransform = Landscape->GetActorTransform();

	Data = GetData( InLandscape );

	LandscapeMeshData MeshData;
	TArray<int32> PatchTriangles;	

	const int32 VisThreshold = 170;
	
	// Copy over the index buffer into the FBX polygons set.
	for( int32 ComponentIndex = 0; ComponentIndex < NumComponents; ComponentIndex++ )
	{
		int32 BaseVertIndex = ComponentIndex * VertexCountPerComponent;

		for( int32 Y = 0; Y < ComponentSizeQuads; Y++ )
		{
			for( int32 X = 0; X < ComponentSizeQuads; X++ )
			{
				if( VisibilityData[ BaseVertIndex + Y * ( ComponentSizeQuads + 1 ) + X ] < VisThreshold )
				{
					if( FlipWinding )
					{
						PatchTriangles.Add( BaseVertIndex + ( X + 0 ) + ( Y + 0 ) * ( ComponentSizeQuads + 1 ) );						
						PatchTriangles.Add( BaseVertIndex + ( X + 1 ) + ( Y + 0 ) * ( ComponentSizeQuads + 1 ) );
						PatchTriangles.Add( BaseVertIndex + ( X + 1 ) + ( Y + 1 ) * ( ComponentSizeQuads + 1 ) );

						PatchTriangles.Add( BaseVertIndex + ( X + 0 ) + ( Y + 0 ) * ( ComponentSizeQuads + 1 ) );						
						PatchTriangles.Add( BaseVertIndex + ( X + 1 ) + ( Y + 1 ) * ( ComponentSizeQuads + 1 ) );
						PatchTriangles.Add( BaseVertIndex + ( X + 0 ) + ( Y + 1 ) * ( ComponentSizeQuads + 1 ) );
					}
					else
					{
						PatchTriangles.Add( BaseVertIndex + ( X + 0 ) + ( Y + 0 ) * ( ComponentSizeQuads + 1 ) );
						PatchTriangles.Add( BaseVertIndex + ( X + 1 ) + ( Y + 1 ) * ( ComponentSizeQuads + 1 ) );
						PatchTriangles.Add( BaseVertIndex + ( X + 1 ) + ( Y + 0 ) * ( ComponentSizeQuads + 1 ) );

						PatchTriangles.Add( BaseVertIndex + ( X + 0 ) + ( Y + 0 ) * ( ComponentSizeQuads + 1 ) );
						PatchTriangles.Add( BaseVertIndex + ( X + 0 ) + ( Y + 1 ) * ( ComponentSizeQuads + 1 ) );
						PatchTriangles.Add( BaseVertIndex + ( X + 1 ) + ( Y + 1 ) * ( ComponentSizeQuads + 1 ) );
					}
				}
			}
		}
	}
	
	ULandscapeInfo* Info = InLandscape->GetLandscapeInfo();
	TArray<FString> SafeLayerNames;
	SafeLayerNames.AddDefaulted( Info->Layers.Num() );

	for( int i = 0; i < Info->Layers.Num(); i++ )
	{
		FLandscapeInfoLayerSettings& LayerSettings = Info->Layers[ i ];
		
		FString SafeLayerName = GetSafeLayerName( LayerSettings.LayerName.ToString() );
		SafeLayerName = GetSafeParameterName( InLandscape->LandscapeMaterial->GetBaseMaterial(), SafeLayerName );
		SafeLayerNames[ i ] = SafeLayerName;
	}

	ModifyTerrainMaterial( InLandscape, InLandscape->LandscapeMaterial, Resolution, SafeLayerNames );
	LandscapeMaterial = UMaterialInstanceDynamic::Create( InLandscape->LandscapeMaterial, nullptr );
	ExportLayerTextures( InLandscape, LandscapeMaterial, SafeLayerNames );
	static bool QuickReturn = true;
	//if ( QuickReturn )
		//return;
	UProceduralMeshComponent* mesh = CreateNewProceduralComponent( world );

	mesh->SetWorldTransform( LandscapeTransform );

	TArray<FVector2D> EmptyArray;
	mesh->CreateMeshSection_LinearColor( 0, InitialVertices, PatchTriangles, normals, UV0, UV1, EmptyArray, EmptyArray, vertexColors, tangents, false );

	mesh->SetMaterial( 0, LandscapeMaterial );
	
	MeshData.MeshAssetName = FString::Printf( TEXT( "/Game/SM_%s" ), *InLandscape->GetName());
	MeshData.Landscape = InLandscape;
	FMeshDescription MeshDescription = BuildMeshDescription( mesh );

	MeshData.ProceduralMesh = mesh;
	MeshData.MeshDescription = &MeshDescription;
	FinalizeMesh( &MeshData );
	MeshData.MeshDescription = nullptr;
}
void FLandscapeTools::FinalizeMesh( LandscapeMeshData* MeshData )
{
	UStaticMesh* StaticMesh = CreateStaticMeshFromProceduralMesh( MeshData->MeshAssetName, MeshData );

	FTransform MeshTransform;
	MeshTransform.SetLocation( LandscapeTransform.GetLocation() );

	int NumComponentsPerAxis = FMath::Sqrt( Landscape->LandscapeComponents.Num() );
	int NumPatchesPerAxis = 1 * NumComponentsPerAxis;
	int x = 0; int y = 0;
	
	float LandscapeLengthX = ( Landscape->ComponentSizeQuads * NumComponentsPerAxis + 0 ) * LandscapeTransform.GetScale3D().X;
	float LandscapeLengthY = ( Landscape->ComponentSizeQuads * NumComponentsPerAxis + 0 ) * LandscapeTransform.GetScale3D().Y;

	FVector Offset = FVector( ( LandscapeLengthX / (float)NumPatchesPerAxis ) * x, y * ( LandscapeLengthY / (float)NumPatchesPerAxis ), 0 );

	FTransform PatchTransform = MeshTransform;
	PatchTransform.SetLocation( PatchTransform.GetLocation() + Offset );

	AStaticMeshActor* StaticMeshActor = world->SpawnActor<AStaticMeshActor>( AStaticMeshActor::StaticClass(), PatchTransform );
	FString Name = FString::Printf( TEXT( "SMA_%s" ), *Landscape->GetName() );
	//StaticMeshActor->Rename( *PatchName, StaticMeshActor->GetOuter(), REN_ForceNoResetLoaders );
	auto Transform = MeshData->Landscape->GetActorTransform();
	Transform.SetScale3D( FVector( 1, 1, 1 ) );
	StaticMeshActor->SetActorTransform( Transform );
	StaticMeshActor->SetActorLabel( Name );

	StaticMeshActor->GetStaticMeshComponent()->SetStaticMesh( StaticMesh );
	StaticMeshActor->GetStaticMeshComponent()->SetMaterial( 0, LandscapeMaterial );	
}

UProceduralMeshComponent* FLandscapeTools::CreateNewProceduralComponent( UObject* Outer )
{
	UProceduralMeshComponent* mesh = nullptr;
	
	FString Name;
	Name = Name.Printf( TEXT( "GeneratedMesh%d" ), MeshesGenerated++ );
	mesh = NewObject<UProceduralMeshComponent>( Outer, *Name );

	// New in UE 4.17, multi-threaded PhysX cooking.
	mesh->bUseAsyncCooking = true;
	return mesh;
}

const TArray<FStaticMaterial>& GetStaticMaterials( const UStaticMesh* StaticMesh );
UStaticMesh* FLandscapeTools::CreateStaticMeshFromProceduralMesh( FString UserPackageName, LandscapeMeshData* MeshData )
{	
	FMeshDescription* MeshDescription = MeshData->MeshDescription;

	FString NewNameSuggestion = FString( TEXT( "ProcMesh" ) );
	FString PackageName = FString( TEXT( "/Game/Meshes/" ) ) + NewNameSuggestion;
	FString Name;

	AssetToolsModule->Get().CreateUniqueAssetName( PackageName, TEXT( "" ), PackageName, Name );

	{
		// Get the full name of where we want to create the physics asset.
		//FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
		FName MeshName( *FPackageName::GetLongPackageAssetName( UserPackageName ) );

		// Check if the user inputed a valid asset name, if they did not, give it the generated default name
		if( MeshName == NAME_None )
		{
			// Use the defaults that were already generated.
			UserPackageName = PackageName;
			MeshName = *Name;
		}

		// If we got some valid data.
		if( MeshDescription->Polygons().Num() > 0 )
		{
			// Then find/create it.
			#if ENGINE_MINOR_VERSION >= 26
				UPackage* Package = CreatePackage( *UserPackageName );
			#else
				UPackage* Package = CreatePackage( nullptr, *UserPackageName );
			#endif
			check( Package );

			// Create StaticMesh object
			UStaticMesh* StaticMesh = NewObject<UStaticMesh>( Package, MeshName, RF_Public | RF_Standalone );
			StaticMesh->InitResources();

			StaticMesh->SetLightingGuid();			

			// Add source to new StaticMesh
			FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
			SrcModel.BuildSettings.bRecomputeNormals = true;//Recomputing Normals & Tangents fixes odd normals that are originally generated
			SrcModel.BuildSettings.bRecomputeTangents = true; //false;
			SrcModel.BuildSettings.bRemoveDegenerates = false;
			SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;			
			SrcModel.BuildSettings.bGenerateLightmapUVs = false;
			SrcModel.BuildSettings.SrcLightmapIndex = 0;
			SrcModel.BuildSettings.DstLightmapIndex = 1;
			SrcModel.BuildSettings.DistanceFieldResolutionScale = 0.0f;
			SrcModel.BuildSettings.bUseFullPrecisionUVs = true;
			FMeshDescription* OriginalMeshDescription = StaticMesh->GetMeshDescription( 0 );
			if( OriginalMeshDescription == nullptr )
			{
				OriginalMeshDescription = StaticMesh->CreateMeshDescription( 0 );
			}
			*OriginalMeshDescription = *MeshDescription;
			StaticMesh->CommitMeshDescription( 0 );

			auto ProcMeshComp = MeshData->ProceduralMesh;

			TSet<UMaterialInterface*> UniqueMaterials;
			const int32 NumSections = ProcMeshComp->GetNumSections();
			for( int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++ )
			{
				FProcMeshSection* ProcSection =
					ProcMeshComp->GetProcMeshSection( SectionIdx );
				UMaterialInterface* Material = ProcMeshComp->GetMaterial( SectionIdx );
				UniqueMaterials.Add( Material );
			}

			TArray<FStaticMaterial> LocalMaterials = GetStaticMaterials( StaticMesh );
			
			// Copy materials to new mesh
			for( auto* Material : UniqueMaterials )
			{
				FStaticMaterial NewStaticMaterial( Material, Material->GetFName(), Material->GetFName() );
				LocalMaterials.Add( NewStaticMaterial );
			}

			#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
				StaticMesh->SetStaticMaterials( LocalMaterials );
			#else
				StaticMesh->StaticMaterials = LocalMaterials;
			#endif

			//Set the Imported version before calling the build
			StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

			// Build mesh from source
			//StaticMesh->Build( false );

			UBodySetup* BodySetup = NewObject<UBodySetup>();// StaticMesh->GetBodySetup();
			BodySetup->bNeverNeedsCookedCollisionData = true;

			#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5				
				StaticMesh->SetBodySetup( BodySetup );
			#else
				StaticMesh->BodySetup = BodySetup;
			#endif
			
			StaticMesh->PostEditChange();

			Package->Modify();
			//Package->PostEditChange();

			// Notify asset registry of new asset
			//FAssetRegistryModule::AssetCreated(StaticMesh);

			return StaticMesh;
		}
	}

	return nullptr;
}
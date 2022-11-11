

#include "ExportToUnity.h"

#include "UnityScene.h"
#include "Module.h"

#include "Runtime/Core/Public/GenericPlatform/GenericPlatformFile.h"
#include "Runtime/Core/Public/HAL/PlatformFilemanager.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "ImageUtils.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"
#include "Engine/Classes/Kismet/KismetStringLibrary.h"

#include "Runtime/Engine/Classes/Components/LightComponent.h"
#include "Runtime/Engine/Classes/Components/PointLightComponent.h"
#include "Runtime/Engine/Classes/Components/SpotLightComponent.h"
#include "Runtime/Engine/Classes/Components/RectLightComponent.h"
#include "Runtime/Engine/Classes/Components/DirectionalLightComponent.h"
#include "Runtime/Engine/Classes/Components/SkeletalMeshComponent.h"
#include "Runtime/Engine/Classes/Components/SplineComponent.h"
#include "Runtime/Engine/Classes/Components/SplineMeshComponent.h"
#include "Runtime/Engine/Classes/Components/DecalComponent.h"
#include "Runtime/Engine/Classes/Components/SphereReflectionCaptureComponent.h"
#include "Runtime/Engine/Classes/Components/BoxReflectionCaptureComponent.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "Runtime/Engine/Classes/Materials/Material.h"
#include "Runtime/Engine/Private/Materials/MaterialUniformExpressions.h"
#include "Runtime/Engine/Classes/Materials/MaterialInstance.h"
#include "Runtime/Engine/Classes/Engine/TextureCube.h"
#include "Runtime/Engine/Classes/Exporters/Exporter.h"
#include "Editor/UnrealEd/Classes/Exporters/TextureCubeExporterHDR.h"
#include "Editor/UnrealEd/Classes/Exporters/TextureExporterHDR.h"
#include "Runtime/Engine/Public/AssetExportTask.h"
#include "CoreUObject/Public/UObject/GCObjectScopeGuard.h"
#include "ObjectTools.h"

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

#include "PxConvexMesh.h"
#include "Runtime/Engine/Public/Rendering/SkeletalMeshRenderData.h"

#include "Runtime/Engine/Classes/Engine/LODActor.h"
#include "Runtime/Engine/Classes/Engine/VolumeTexture.h"
#include "Runtime/Engine/Classes/Engine/Texture2DArray.h"
#define LOCTEXT_NAMESPACE "FUnrealToUnityModule"

ERHIFeatureLevel::Type GlobalFeatureLevel = ERHIFeatureLevel::SM5;
EMaterialQualityLevel::Type GlobalQualityLevel = EMaterialQualityLevel::High;

TAutoConsoleVariable<int> CVarExportLODs(
	TEXT( "utu.ExportLODs" ),
	1,
	TEXT( "Export all Lods, default is 1" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarExportVertexColors(
	TEXT( "utu.ExportVertexColors" ),
	1,
	TEXT( "Export VertexColors, default is 1" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarUseOriginalPaths(
	TEXT( "utu.UseOriginalPaths" ),
#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
	1,
#else
	0,
#endif
	TEXT( "Use Original Unreal paths just in case there's 2 files with the same name, default is 1" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarUsePostImportTool(
	TEXT( "utu.UsePostImportTool" ),
	1,
	TEXT( "Uses the PostImportTool to fix UV sets and submeshes" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarRenderPipeline(
	TEXT( "utu.RenderPipeline" ),
	0,
	TEXT( "Selects Render pipeline shaders to be generated" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarAssetsWithYUp(
	TEXT( "utu.AssetsWithYUp" ),
	1,
	TEXT( "By Default assets have Z up (Unreal Default). If 1 assets will have Y up but the scene won't look right." ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarExportTangents(
	TEXT( "utu.ExportTangents" ),
	1,
	TEXT( "By Tangents are exported. Disable if you have meshes with black parts in unity" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarUsePrefabs(
	TEXT( "utu.UsePrefabs" ),
	1,
	TEXT( "Generate and use prefab instances where appliable. If there's errors with this feature, turn it off" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarFBXPerActor(
	TEXT( "utu.FBXPerActor" ),
	1,
	TEXT( "Do FBXs per actor instead of OBJs" ),
	ECVF_Cheat );
TAutoConsoleVariable<int> CVarUseStandardMaterial(
	TEXT( "utu.UseStandardMaterial" ),
	0,
	TEXT( "Make all exported materials use the standard material." ),
	ECVF_Cheat );

#if WITH_EDITOR


#include "MaterialShared.generated.h"
#include "Editor/MaterialEditor/Public/MaterialEditorUtilities.h"

#include "UnrealEdGlobals.h"//for GUnrealEd
#include "Editor/UnrealEdEngine.h"

void ConvertTransform( FTransform Trans, TransformComponent* Transform, bool IsRelative = false, bool HasDecals = false );

#if PLATFORM_WINDOWS

//maybe TCHAR_TO_UTF8 ?

std::wstring ToWideString( std::string str )
{
	std::wstring w;
	for( int i = 0; i < str.length(); i++ )
	{
		w += str[ i ];
	}
	return w;
}
std::string ToANSIString( std::wstring w )
{
	std::string s;
	for( int i = 0; i < w.length(); i++ )
	{
		s += w[ i ];
	}
	return s;
}
#else
std::u16string ToWideString( std::string str )
{
	std::u16string w;
	for( int i = 0; i < str.length(); i++ )
	{
		w += str[ i ];
	}
	return w;
}
std::string ToANSIString( std::u16string w )
{
	std::string s;
	for( int i = 0; i < w.length(); i++ )
	{
		s += w[ i ];
	}
	return s;
}

//#define _snscanf_s scanf
#define _ftelli64 ftell
#endif


int TotalMeshes = 0;
int TotalTriangles = 0;

bool ExportMeshes = true;// false;
bool ExportTextures = true;
bool ForceShaderRecompilation = false;

FString UnityProjectFolder;

FString GetUnityAssetsFolder()
{
	FString Folder = UnityProjectFolder;
	Folder += TEXT("Assets/" );

	return Folder;
}
int CreateAllDirectories( FString FileName )
{
	int created = 0;
	//detects directories, hierarchly and creates them	
	int len = FileName.Len();
	for( int y = 0; y < len; y++ )
	{
		if( FileName[ y ] == '\\' || FileName[ y ] == '/' )
		{
			FString DirName = UKismetStringLibrary::GetSubstring( FileName, 0, y );

			created += VerifyOrCreateDirectory( DirName );
		}
	}

	return created;
}

Scene *UnityScene = nullptr;

class TextureBinding
{
public:
	UnityTexture *UnityTex = nullptr;
	UTexture *UnrealTexture = nullptr;
	bool IsNormalMap = false;
};

std::vector< TextureBinding* > AllTextures;

FString GetAssetPathFolder( UObject* Obj )
{
	FString Path;
	if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
	{
	#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION == 5
		FString AssetPath = Obj->GetPackage()->GetPathName();
		AssetPath = AssetPath.Replace( TEXT( "/Game/" ), TEXT( "" ) );
		//AssetPath = AssetPath.Replace( TEXT( "/" ), TEXT( "\\" ) );
		AssetPath.RemoveFromEnd( Obj->GetName() );
		Path += *AssetPath;
	#endif
	}
	return Path;
}
FString GenerateTexturePath( UTexture *Tex )
{
	UTextureCube* CubeTex = Cast<UTextureCube>( Tex );
	FString TextureFileName = GetUnityAssetsFolder();

	if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
	{
		TextureFileName += GetAssetPathFolder( Tex );
		CreateAllDirectories( TextureFileName );
	}
	else
	{
		TextureFileName += TEXT("Textures/" );
	}

	TextureFileName += *Tex->GetName();
	if ( CubeTex )
		TextureFileName += TEXT(".hdr" );
	else
		TextureFileName += TEXT(".png" );

	return TextureFileName;
}

void UseUEExporter( UExporter* ExporterToUse, UObject* ObjectToExport, FString Filename )
{
	//UExporter* ExporterToUse = NULL;
	{
		//const FScopedBusyCursor BusyCursor;

		//if( !UsedExporters.Contains( ExporterToUse ) )
		{
			ExporterToUse->SetBatchMode( false );
			ExporterToUse->SetCancelBatch( false );
			ExporterToUse->SetShowExportOption( true );
			ExporterToUse->AddToRoot();
			//UsedExporters.Add( ExporterToUse );
		}

		UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
		FGCObjectScopeGuard ExportTaskGuard( ExportTask );
		ExportTask->Object = ObjectToExport;
		ExportTask->Exporter = ExporterToUse;
		ExportTask->Filename = Filename;
		ExportTask->bSelected = false;
		ExportTask->bReplaceIdentical = true;
		ExportTask->bPrompt = false;
		ExportTask->bUseFileArchive = ObjectToExport->IsA( UPackage::StaticClass() );
		ExportTask->bWriteEmptyFiles = false;

		UExporter::RunAssetExportTask( ExportTask );
	}

	ExporterToUse->SetBatchMode( false );
	ExporterToUse->SetCancelBatch( false );
	ExporterToUse->SetShowExportOption( true );
	ExporterToUse->RemoveFromRoot();
}
void ExportCubeMap( UTextureCube* Cubemap, FString Filename )
{
	TArray<UExporter*> Exporters;
	ObjectTools::AssembleListOfExporters( Exporters );
	for( int i = 0; i < Exporters.Num(); i++ )
	{
		UTextureCubeExporterHDR* CubeExporter = Cast<UTextureCubeExporterHDR>( Exporters[i] );
		if( CubeExporter )
		{
			UseUEExporter( CubeExporter, Cubemap, Filename );
			return;
		}
	}	
}
void ExportTextureHDR( UTexture2D* Tex, FString Filename )
{
	TArray<UExporter*> Exporters;
	ObjectTools::AssembleListOfExporters( Exporters );
	for( int i = 0; i < Exporters.Num(); i++ )
	{
		UTextureExporterHDR* TargetExporter = Cast<UTextureExporterHDR>( Exporters[ i ] );
		if( TargetExporter )
		{
			UseUEExporter( TargetExporter, Tex, Filename );
			return;
		}
	}
}
class UETextureExporter
{
public:	
	static void DoExport( UTexture *Tex )
	{
		UTextureCube* CubeTex = Cast<UTextureCube>( Tex );
		UVolumeTexture* VolTex = Cast< UVolumeTexture>( Tex );
		UTexture2DArray* Tex2DArray = Cast< UTexture2DArray>( Tex );
		FString TextureFileName = GenerateTexturePath( Tex );
		
		if( !ExportTextures )
			return;
		//Don't save duplicates !
		if ( !FPlatformFileManager::Get().GetPlatformFile().FileExists( *TextureFileName ) )
		{
			if( CubeTex )
			{
				ExportCubeMap( CubeTex, TextureFileName );
				return;
			}
			if( VolTex )
			{
				#if ENGINE_MAJOR_VERSION == 4
					Tex = VolTex->Source2DTexture;
				#else
					Tex = VolTex->Source2DTexture.Get();
				#endif
			}
			if( Tex2DArray )
			{
			}
			TArray64<uint8> OutMipData;
			ETextureSourceFormat Format = Tex->Source.GetFormat();

			if ( Format != TSF_BGRA8 && Format != TSF_RGBA8 && Format != TSF_RGBA16 && Format != TSF_G8 &&
				 Format != TSF_BGRE8 &&
				 Format != TSF_RGBA16F )
			{
				UE_LOG( LogClass, Log, TEXT("UETextureExporter::DoExport Format %d is not standard!"), Format );
				return;
			}

			int32 Width = Tex->Source.GetSizeX();
			int32 Height = Tex->Source.GetSizeY();
			int32 Depth = Tex->Source.GetNumSlices();

			Tex->Source.GetMipData( OutMipData, 0 );

			#if ENGINE_MAJOR_VERSION == 4
				TArray<uint8> PNG_Compressed_ImageData;
			#else
				TArray64<uint8> PNG_Compressed_ImageData;
			#endif

			TArray<FColor> InputImageData;

			if( Format == TSF_G8 )
			{
				for( int i = 0; i < OutMipData.Num(); i++ )
				{					
					FColor Color( OutMipData[ i ], OutMipData[ i ], OutMipData[ i ], OutMipData[ i ] );
					InputImageData.Add( Color );
				}
			}
			else if( Format == TSF_RGBA16F )
			{
				FFloat16Color* Pixels = (FFloat16Color*)OutMipData.GetData();
				int NumPixels = Width * Height;
				for( int i = 0; i < NumPixels; i++ )
				{					
					FLinearColor Color( Pixels[ i ] );
					//Color.R = Pixels[ i ].R;
					//Color.G = Pixels[ i ].G;
					//Color.B = Pixels[ i ].B;
					//Color.A = Pixels[ i ].A;
					FColor C = Color.ToFColor( true );
					InputImageData.Add( C );
				}
			}
			else if( Format == TSF_RGBA16 )
			{
				for( int i = 0; i < OutMipData.Num(); i += 8 )
				{
					FColor Color;
					uint16 R16 = OutMipData[ i + 0 ] * 256 + OutMipData[ i + 1 ];
					uint16 G16 = OutMipData[ i + 2 ] * 256 + OutMipData[ i + 3 ];
					uint16 B16 = OutMipData[ i + 4 ] * 256 + OutMipData[ i + 5 ];
					uint16 A16 = OutMipData[ i + 6 ] * 256 + OutMipData[ i + 7 ];
					//Color.R = R16 / 256;
					//Color.G = G16 / 256;
					//Color.B = B16 / 256;
					//Color.A = A16 / 256;
					Color.B = OutMipData[ i + 5 ];
					Color.G = OutMipData[ i + 3 ];
					Color.R = OutMipData[ i + 1 ];
					Color.A = OutMipData[ i + 7 ];
					InputImageData.Add( Color );
				}
			}
			else
			{
				auto AddPixel = [&]( int x, int y, int z )
				{
					int i = ( z * Width * Height + y * Width + x ) * 4;
					FColor Color( OutMipData[ i + 2 ], OutMipData[ i + 1 ], OutMipData[ i + 0 ], OutMipData[ i + 3 ] );
					InputImageData.Add( Color );
				};
				bool UpsideDown = false;// true;
				if( UpsideDown )
				{
					for( int y = Height - 1; y >= 0; y-- )
					{
						for( int x = 0; x < Width; x++ )
						{
							AddPixel( x, y, 0 );
						}
					}
				}
				else
				{
					for( int z = 0; z < Depth; z++ )
					{
						for( int y = 0; y < Height; y++ )
						{
							for( int x = 0; x < Width; x++ )
							{
								AddPixel( x, y, z );
							}
						}
					}
				}
			}

			//~~~~~~~~~~~~~~~~
			// Compress to PNG
			//~~~~~~~~~~~~~~~~
			if( InputImageData.Num() == 0 )
			{
				UE_LOG( LogClass, Log, TEXT( "InputImageData.Num() == 0 Tex->Source.GetMipData probably failed ?" ) );
				return;
			}

			#if ENGINE_MAJOR_VERSION == 4
				FImageUtils::CompressImageArray(
					Width,
					Height,
					InputImageData,
					PNG_Compressed_ImageData
				);
			#else
				FImageUtils::PNGCompressImageArray(
					Width,
					Height,
					InputImageData,
					PNG_Compressed_ImageData
				);
			#endif
			

			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			//                    Save binary array PNG image file to disk! 


			FFileHelper::SaveArrayToFile(
				PNG_Compressed_ImageData,
				*TextureFileName
				);
		}
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		//~~~ Empty PNG Buffer ~~~
		//PNG_Compressed_ImageData.Empty();
		//~~~~~~~~~~~~~~~~~~~~~~~~

		//~~~ Empty Color Buffer ~~~
		//PNGScreenShot_ColorBuffer.Empty();
		//~~~~~~~~~~~~~~~~~~~~~~~~
	}
	static TextureBinding *Find( UTexture *Tex )
	{
		for( int i = 0; i < AllTextures.size(); i++ )
		{
			auto Binding = AllTextures[ i ];
			if( Binding->UnrealTexture == Tex )
				return Binding;
		}

		return nullptr;
	}
	static TextureBinding *Export( UTexture *T )
	{
		auto Existent = Find( T );
		if( Existent )
			return Existent;

		DoExport( T );

		TextureBinding *NewTextureBinding = new TextureBinding;
		NewTextureBinding->UnrealTexture = T;
		AllTextures.push_back( NewTextureBinding );

		return NewTextureBinding;
	}
};

std::vector< MaterialBinding *> AllMaterials;
MaterialBinding *GetMaterialIfAlreadyExported( UMaterialInterface *M )
{
	for ( int i = 0; i < AllMaterials.size(); i++ )
	{
		if ( AllMaterials[ i ]->MaterialInterface == M )
			return AllMaterials[ i ];
	}

	return nullptr;
}
MaterialBinding *ProcessMaterialReference( UMaterialInterface *M )
{
	MaterialBinding *Mat = GetMaterialIfAlreadyExported( M );

	if ( !Mat )
	{
		Mat = new MaterialBinding();
		Mat->MaterialInterface = M;
		Mat->ID = AllMaterials.size();
		AllMaterials.push_back( Mat );
	}
	
	return Mat;
}
FString GetProjectName()
{
	FString ProjectFilePath = FPaths::GetProjectFilePath();
	int PointStart = ProjectFilePath.Find( TEXT( "." ), ESearchCase::IgnoreCase, ESearchDir::FromEnd );
	int NameStart = ProjectFilePath.Find( TEXT( "/" ), ESearchCase::IgnoreCase, ESearchDir::FromEnd );
	FString ProjectName = ProjectFilePath.Mid( NameStart + 1, PointStart - NameStart - 1 );
	return ProjectName;
}
FString GetResourceDir()
{
	FString ProjectDir = FPaths::ProjectDir();

	ProjectDir.Append( TEXT( "/Plugins/UnrealToUnity/Resources/" ) );

	return ProjectDir;
}
void CopyToOutput( FString SourceFile, FString OutFilePath, bool Overwrite = true )
{
	FString ProjectDir = FPaths::ProjectContentDir();
	
	FString ResourceDir = GetResourceDir();
	FString SourceFilePath = ResourceDir;
	SourceFilePath += SourceFile;
	FString To = UnityProjectFolder;
	To += OutFilePath;

	if( !Overwrite )
	{
		if( FPlatformFileManager::Get().GetPlatformFile().FileExists( *To ) )
			return;
	}

	bool Result = FPlatformFileManager::Get().GetPlatformFile().CopyFile( *To, *SourceFilePath );
	if( !Result )
	{
		UE_LOG( LogTemp, Error, TEXT( "CopyToOutput Error on SourceFile=%s OutFilePath=%s" ), *SourceFilePath, *To );
	}
}
void ProcessMaterial_RenderThread( MaterialBinding *Mat, const char *ProxyShaderString, const char* GraphShaderString );
void ProcessAllMaterials( )
{
	FString ResourceDir = GetResourceDir();
	FString ProxyShader = ResourceDir;
	FString GraphShader = ResourceDir;
	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP  ||
		CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
	{
		ProxyShader += TEXT("/ProxyURP_HDRP.hlsl" );
	}
	else
		ProxyShader += TEXT("/Proxy.shader" );

	if( CVarUseStandardMaterial.GetValueOnAnyThread() == 0 )
	{
		CopyToOutput( TEXT("UnrealCommon.cginc" ), TEXT("Assets/Shaders/UnrealCommon.cginc" ), false );
	#if ENGINE_MAJOR_VERSION >= 5
		CopyToOutput( TEXT("LargeWorldCoordinates.hlsl" ), TEXT("Assets/Shaders/LargeWorldCoordinates.hlsl" ), false );
		CopyToOutput( TEXT("LWCOperations.hlsl" ), TEXT("Assets/Shaders/LWCOperations.hlsl" ), false );
	#endif
	}
	
	const char *ProxyShaderString = nullptr;
	int Size = LoadFile( ToANSIString( *ProxyShader ).c_str(), (uint8**)&ProxyShaderString );
	if ( Size < 0 )
	{
		//ERROR!
		return;
	}
	FString DecalGraphShader;
	const char* GraphShaderData = nullptr;
	const char* DecalGraphShaderData = nullptr;
	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP )
	{
		DecalGraphShader = GraphShader;
		GraphShader += TEXT("URP/URPGraphShader.shader" );
		Size = LoadFile( ToANSIString( *GraphShader ).c_str(), (uint8**)&GraphShaderData );
		
		DecalGraphShader += TEXT("URP/URPDecalGraphShader.shader" );
		Size = LoadFile( ToANSIString( *DecalGraphShader ).c_str(), (uint8**)&DecalGraphShaderData );
	}
	else if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
	{
		DecalGraphShader = GraphShader;
		GraphShader += TEXT("HDRP/HDRPGraphShader.shader");
		Size = LoadFile( ToANSIString( *GraphShader ).c_str(), (uint8**)&GraphShaderData );

		DecalGraphShader += TEXT("HDRP/HDRPDecalGraphShader.shader");
		Size = LoadFile( ToANSIString( *DecalGraphShader ).c_str(), (uint8**)&DecalGraphShaderData );
	}
	
	for ( int i = 0; i < AllMaterials.size(); i++ )
	{
		auto Mat = AllMaterials[ i ];
		const char* SelectedGraphShaderData = GraphShaderData;
		if( Mat->BaseMaterial->MaterialDomain == MD_DeferredDecal )
			SelectedGraphShaderData = DecalGraphShaderData;
		ProcessMaterial_RenderThread( Mat, ProxyShaderString, SelectedGraphShaderData );
	}

	if ( GraphShaderData )
		delete[]GraphShaderData;
	if ( DecalGraphShaderData )
		delete[]DecalGraphShaderData;
}
FString GetOutputFile( FString SubFolder, FString Name, FString Ext )
{
	FString OutFolder = GetUnityAssetsFolder();
	OutFolder += *SubFolder;
	OutFolder += *Name;
	OutFolder += Ext;

	return OutFolder;
}
class FUniformExpressionSet_Override : public FUniformExpressionSet
{
public:
	
	#if ENGINE_MAJOR_VERSION >= 5
	TMemoryImageArray<FMaterialNumericParameterInfo>& GetUniformNumericParameters()
	{
		return this->UniformNumericParameters;
	}
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& GetUniformPreshaders()
	{
		return this->UniformPreshaders;
	}
	UE::Shader::FPreshaderData& GetUniformPreshaderData()
	{
		return UniformPreshaderData;
	}
	#else
	TMemoryImageArray<FMaterialScalarParameterInfo>& GetUniformScalarParameters()
	{		
		return this->UniformScalarParameters;
	}
	TMemoryImageArray<FMaterialVectorParameterInfo>& GetUniformVectorParameters()
	{		
		return this->UniformVectorParameters;
	}
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& GetVectorPreshaders()
	{
		return this->UniformVectorPreshaders;
	}
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& GetScalarPreshaders()
	{
		return this->UniformScalarPreshaders;
	}
	FMaterialPreshaderData& GetUniformPreshaderData()
	{
		return UniformPreshaderData;
	}
	#endif
};

class ShaderProperty
{
public:
	std::string Name;
	int ScalarIndex = -1;
	int VectorIndex = -1;
	//int BufferOffset= -1;
};
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26
using FPreshaderStack = TArray<FLinearColor, TInlineAllocator<64u>>;
void EvaluatePreshader2( const FUniformExpressionSet* UniformExpressionSet, const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data, FLinearColor& OutValue,
						 uint16& LastVectorIndex, uint16& LastScalarIndex, int& NumOpCodes );
void GetPropertyNames( FUniformExpressionSet_Override* Set_Override, FMaterialRenderContext MaterialRenderContext, std::vector< ShaderProperty>& Properties, uint8* TempBuffer, int TempBufferSize )
{
	void* BufferCursor = TempBuffer;
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& VectorPreshaders = Set_Override->GetVectorPreshaders();
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& ScalarPreshaders = Set_Override->GetScalarPreshaders();
	TMemoryImageArray<FMaterialScalarParameterInfo>& UniformScalarParameters = Set_Override->GetUniformScalarParameters();
	TMemoryImageArray<FMaterialVectorParameterInfo>& UniformVectorParameters = Set_Override->GetUniformVectorParameters();

	FPreshaderStack PreshaderStack;
	FPreshaderDataContext PreshaderBaseContext( Set_Override->GetUniformPreshaderData() );
	for( int32 VectorIndex = 0; VectorIndex < VectorPreshaders.Num(); ++VectorIndex )
	{
		FLinearColor VectorValue( 0, 0, 0, 0 );

		const FMaterialUniformPreshaderHeader& Preshader = VectorPreshaders[ VectorIndex ];
		FPreshaderDataContext PreshaderContext( PreshaderBaseContext, Preshader );
		uint16 LastVectorIndex;
		uint16 LastScalarIndex;
		int NumOpCodes;
		EvaluatePreshader2( Set_Override, MaterialRenderContext, PreshaderStack, PreshaderContext, VectorValue, LastVectorIndex, LastScalarIndex, NumOpCodes );
		if( NumOpCodes == 1 && LastVectorIndex != (uint16)-1 )
		{
			ShaderProperty NewShaderProperty;
			const FMaterialVectorParameterInfo& Parameter = Set_Override->GetVectorParameter( LastVectorIndex );
			FString UStr = Parameter.ParameterInfo.Name.ToString();
			NewShaderProperty.Name = ToANSIString( *UStr );
			NewShaderProperty.VectorIndex = VectorIndex;// LastVectorIndex;
			//NewShaderProperty.BufferOffset = VectorIndex;
			Properties.push_back( NewShaderProperty );
		}

		FLinearColor* DestAddress = (FLinearColor*)BufferCursor;
		*DestAddress = VectorValue;
		BufferCursor = DestAddress + 1;
		check( BufferCursor <= TempBuffer + TempBufferSize );
	}

	// Dump scalar expression into the buffer.
	for( int32 ScalarIndex = 0; ScalarIndex < ScalarPreshaders.Num(); ++ScalarIndex )
	{
		FLinearColor VectorValue( 0, 0, 0, 0 );

		const FMaterialUniformPreshaderHeader& Preshader = ScalarPreshaders[ ScalarIndex ];
		FPreshaderDataContext PreshaderContext( PreshaderBaseContext, Preshader );
		uint16 LastVectorIndex;
		uint16 LastScalarIndex;
		int NumOpCodes;
		EvaluatePreshader2( Set_Override, MaterialRenderContext, PreshaderStack, PreshaderContext, VectorValue, LastVectorIndex, LastScalarIndex, NumOpCodes );
		if( NumOpCodes == 1 && LastScalarIndex != (uint16)-1 )
		{
			ShaderProperty NewShaderProperty;
			const FMaterialScalarParameterInfo& Parameter = Set_Override->GetScalarParameter( LastScalarIndex );
			FString UStr = Parameter.ParameterInfo.Name.ToString();
			NewShaderProperty.Name = ToANSIString( *UStr );
			NewShaderProperty.ScalarIndex = ScalarIndex;// LastScalarIndex;
			//NewShaderProperty.BufferOffset = ScalarIndex;
			Properties.push_back( NewShaderProperty );
		}
		float* DestAddress = (float*)BufferCursor;
		*DestAddress = VectorValue.R;
		BufferCursor = DestAddress + 1;
		check( BufferCursor <= TempBuffer + TempBufferSize );
	}
}
#endif
FLinearColor RemoveNans( FLinearColor Value )
{
	if( FMath::IsNaN( Value.R ) )
		Value.R = 0.0f;
	if( FMath::IsNaN( Value.G ) )
		Value.G = 0.0f;
	if( FMath::IsNaN( Value.B ) )
		Value.B = 0.0f;
	if( FMath::IsNaN( Value.A ) )
		Value.A = 0.0f;

	return Value;
}
FVector RemoveNans( FVector Value )
{
	if( FMath::IsNaN( Value.X ) )
		Value.X = 0.0f;
	if( FMath::IsNaN( Value.Y ) )
		Value.Y = 0.0f;
	if( FMath::IsNaN( Value.Z ) )
		Value.Z = 0.0f;

	return Value;
}
bool HasNans( FVector Value )
{
	if( FMath::IsNaN( Value.X ) || !FMath::IsFinite( Value.X ) )
		return true;
	if( FMath::IsNaN( Value.Y ) || !FMath::IsFinite( Value.Y ) )
		return true;
	if( FMath::IsNaN( Value.Z ) || !FMath::IsFinite( Value.Z ) )
		return true;

	return false;
}
std::string GetPropertyName( int Vector, int Scalar, const std::vector< ShaderProperty > & Properties )
{
	for( int i = 0; i < Properties.size(); i++ )
	{
		if( Vector != -1 && Vector == Properties[ i ].VectorIndex )
		{
			return Properties[ i ].Name;
		}
		if( Scalar != -1 && Scalar == Properties[ i ].ScalarIndex )
		{
			return Properties[ i ].Name;
		}
	}

	return "(Unknown)";
}
std::string GenerateInitializeExpressions( MaterialBinding *Mat, int & NumVectorExpressions, int & NumScalarExpressions )
{
	std::string DataString;

	if( !Mat || !Mat->MaterialResource )
	{
		UE_LOG( LogTemp, Error, TEXT( "ERROR! !Mat || !Mat->MaterialResource" ) );
		GLog->Flush();
		return "";
	}

	FMaterialResource* MaterialResource = Mat->MaterialResource;// M->GetMaterialResource( ERHIFeatureLevel::SM5 );
	const FMaterialShaderMap* ShaderMapToUse = MaterialResource->GetRenderingThreadShaderMap();
	
	if( !ShaderMapToUse )
	{
		UE_LOG( LogTemp, Error, TEXT( "ERROR! !ShaderMapToUse" ) );
		GLog->Flush();
		return "";
	}
	
	const FUniformExpressionSet* Set = &ShaderMapToUse->GetUniformExpressionSet();
	if( !Set )
	{
		UE_LOG( LogTemp, Error, TEXT( "ERROR! !Set" ) );
		GLog->Flush();
		return "";
	}
	FUniformExpressionSet_Override* Set_Override = (FUniformExpressionSet_Override*)Set;

	#if ENGINE_MAJOR_VERSION >= 5
		TMemoryImageArray<FMaterialUniformPreshaderHeader>& Preshaders = Set_Override->GetUniformPreshaders();	

		NumVectorExpressions = Preshaders.Num();
		NumScalarExpressions = 0;
	#else
		TMemoryImageArray<FMaterialUniformPreshaderHeader>& VectorPreshaders = Set_Override->GetVectorPreshaders();
		TMemoryImageArray<FMaterialUniformPreshaderHeader>& ScalarPreshaders = Set_Override->GetScalarPreshaders();

		NumVectorExpressions = VectorPreshaders.Num();
		NumScalarExpressions = ScalarPreshaders.Num();
	#endif

	int NumScalarVectors = ( NumScalarExpressions + 3 ) / 4;

	int ExtraSpaceForResources = 300;
	int BaseSize = ( NumVectorExpressions + NumScalarExpressions ) * 16;
	int TempBufferSize = BaseSize + ExtraSpaceForResources;

	int FinalTempBufferSize = TempBufferSize + ExtraSpaceForResources;
	uint8* TempBuffer = new uint8[ FinalTempBufferSize ];

	//Prevent nans from appearing in ScalarExpressions values
	float* FloatArray = (float*)TempBuffer;
	for( int i = 0; i < FinalTempBufferSize / 4; i++ )
	{
		FloatArray[ i ] = 0.0f;
	}

	FMaterialRenderProxy* RenderProxy = Mat->RenderProxy;
	if( !RenderProxy )
	{
		UE_LOG( LogTemp, Error, TEXT( "ERROR! !RenderProxy" ) );
		GLog->Flush();
		return "";
	}
	auto Cache = RenderProxy->UniformExpressionCache[ (int32)ERHIFeatureLevel::SM5 ];
	if( Cache.AllocatedVTs.Num() > 0 )
	{
		UE_LOG( LogTemp, Error, TEXT( "[UTU] ! Cache.AllocatedVTs.Num() > 0" ) );
		//Fix for crash in FillUniformBuffer
		return "";
	}
	FMaterialRenderContext DummyContext( RenderProxy, *MaterialResource, nullptr );

	std::vector< ShaderProperty > Properties;
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26
	GetPropertyNames( Set_Override, DummyContext, Properties, TempBuffer, TempBufferSize );
#endif

	#if ENGINE_MAJOR_VERSION >= 5
		const FRHIUniformBufferLayout* UniformBufferLayout = ShaderMapToUse->GetUniformBufferLayout();
		if( UniformBufferLayout->Resources.Num() > 0 )
		{
			int Offset = UniformBufferLayout->Resources[ UniformBufferLayout->Resources.Num() - 1 ].MemberOffset;
			TempBufferSize = FMath::Max( TempBufferSize, Offset + BaseSize );
		}
		Set->FillUniformBuffer( DummyContext, Cache, UniformBufferLayout, TempBuffer, TempBufferSize );
	#else
		Set->FillUniformBuffer( DummyContext, Cache, TempBuffer, TempBufferSize );
	#endif

	char ValueStr[ 256 ];
	snprintf( ValueStr, sizeof( ValueStr ),
			  "void InitializeExpressions()\r\n"
			  "{\r\n" );// , VectorExpressions.Num(), ScalarExpressions.Num() );
	DataString += ValueStr;

	uint8* BufferCursor = TempBuffer;
	
	for( int i = 0; i < NumVectorExpressions; i++ )
	{
		FLinearColor OutValue = *(FLinearColor*)BufferCursor;
		BufferCursor += sizeof( FLinearColor );
		OutValue = RemoveNans( OutValue );
		std::string Name = GetPropertyName( i, -1, Properties );
	#if ENGINE_MAJOR_VERSION >= 5
		snprintf( ValueStr, sizeof( ValueStr), "\tMaterial.PreshaderBuffer[%d] = float4(%f,%f,%f,%f);//%s\r\n", i, OutValue.R, OutValue.G, OutValue.B, OutValue.A, Name.c_str());
	#else
		snprintf( ValueStr, sizeof( ValueStr ), "\tMaterial.VectorExpressions[%d] = float4(%f,%f,%f,%f);//%s\r\n", i, OutValue.R, OutValue.G, OutValue.B, OutValue.A, Name.c_str() );
	#endif
		DataString += ValueStr;
	}

	for( int i = 0; i < NumScalarVectors; i++ )
	{
		FLinearColor ScalarValues = *(FLinearColor*)BufferCursor;
		BufferCursor += sizeof( FLinearColor );
		ScalarValues = RemoveNans( ScalarValues );
		std::string Name1 = GetPropertyName( -1, i * 4 + 0, Properties );
		std::string Name2 = GetPropertyName( -1, i * 4 + 1, Properties );
		std::string Name3 = GetPropertyName( -1, i * 4 + 2, Properties );
		std::string Name4 = GetPropertyName( -1, i * 4 + 3, Properties );
		snprintf( ValueStr, sizeof( ValueStr ), "\tMaterial.ScalarExpressions[%d] = float4(%f,%f,%f,%f);//%s %s %s %s\r\n", i,
					ScalarValues.R, ScalarValues.G, ScalarValues.B, ScalarValues.A, Name1.c_str(), Name2.c_str(), Name3.c_str(), Name4.c_str() );
		DataString += ValueStr;
	}	

	DataString += "}\r\n";
	delete[] TempBuffer;
	
	return DataString;	
}

std::string AddNumberInString( std::string OriginalString, const char *Prefix, int Number )
{
	int PrefixOffset = OriginalString.find( Prefix );
	int NumberStart = PrefixOffset + strlen( Prefix );

	char NumberStr[ 32 ];
	snprintf( NumberStr, sizeof( NumberStr ), "%d", Number );
	//Erases the existing "1"
	OriginalString.replace( NumberStart, 1, "" );
	OriginalString.insert( NumberStart, NumberStr );
	return OriginalString;
}
void StringReplacement( std::string & OriginalString, const char *What, const char* Replace, int StartPos = 0 )
{
	int WhatLen = strlen( What );
	int ReplaceLen = strlen( Replace );

	int pos = StartPos;
	pos = OriginalString.find( What, StartPos );
	while( pos != -1 )
	{
		OriginalString.replace( pos, WhatLen, Replace );
		pos = OriginalString.find( What, pos + ReplaceLen );
	}
}
const int MaxTextures = 32;
void DetectNumTexturesUsed( std::string GeneratedShader, int* Actual2DTexturesUsed, int* ActualCubeTexturesUsed,
							int* ActualTextureArraysUsed, int* ActualCubeArraysUsed, int* Actual3DTexturesUsed, const char* BeginMarker, const char* EndMarker)
{
	int Offset = 0;
	if ( BeginMarker )
		Offset = GeneratedShader.find( BeginMarker );
	int End = GeneratedShader.length();
	if ( EndMarker )
		End = GeneratedShader.find( EndMarker );

	for( int i = 0; i < MaxTextures; i++ )
	{
		char Tex2DRef[ 256 ];
		snprintf( Tex2DRef, sizeof( Tex2DRef ), "Material_Texture2D_%d", i );
		int pos = GeneratedShader.find( Tex2DRef, Offset );
		if( pos != -1 && pos < End )
		{
			Actual2DTexturesUsed[ i ] = 1;
			continue;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char TexCubeRef[ 256 ];
		snprintf( TexCubeRef, sizeof( TexCubeRef ), "Material_TextureCube_%d", i );
		int pos = GeneratedShader.find( TexCubeRef, Offset );
		if( pos != -1 && pos < End )
		{
			ActualCubeTexturesUsed[ i ] = 1;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char Tex2DRef[ 256 ];
		snprintf( Tex2DRef, sizeof( Tex2DRef ), "Material_Texture2DArray_%d", i );
		int pos = GeneratedShader.find( Tex2DRef, Offset );
		if( pos != -1 && pos < End )
		{
			ActualTextureArraysUsed[ i ] = 1;
			continue;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char TexCubeRef[ 256 ];
		snprintf( TexCubeRef, sizeof( TexCubeRef ), "Material_TextureCubeArray_%d", i );
		int pos = GeneratedShader.find( TexCubeRef, Offset );
		if( pos != -1 && pos < End )
		{
			ActualCubeArraysUsed[ i ] = 1;
		}
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		char Tex3DRef[ 256 ];
		snprintf( Tex3DRef, sizeof( Tex3DRef ), "Material_VolumeTexture_%d", i );
		int pos = GeneratedShader.find( Tex3DRef, Offset );
		if( pos != -1 && pos < End )
		{
			Actual3DTexturesUsed[ i ] = 1;
			continue;
		}
	}
}
void AddTexProperty( int& Cursor, std::string* TargetString, int i, int ParamIndex, MaterialBinding* Mat, const char* Format1, const char* Format2 )
{
	char Line[ 1024 ];
	snprintf( Line, sizeof( Line ), Format1, i );
	FString TexParamName( Line );
	if( ParamIndex < Mat->TextureParamNames.Num() )
		TexParamName = Mat->TextureParamNames[ ParamIndex ];
	std::string TexParamNameCStr = ToANSIString( *TexParamName );

	snprintf( Line, sizeof( Line ), Format2, i, TexParamNameCStr.c_str() );
	int LineLen = strlen( Line );
	TargetString->insert( Cursor, Line );
	Cursor += LineLen;
}
void AddTexDecalaration( int& Cursor, std::string* TargetString, int i, std::string& VS, std::string& PS, const char* Format1, const char* Format2,
						 const char* Format3, const char* Format4 )
{
	char Line[ 1024 ];
	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP ||
		CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
	{
		char SamplerNameOriginal[ 1024 ] = "";
		snprintf( SamplerNameOriginal, sizeof( SamplerNameOriginal ), Format1, i );
		char SamplerNameReplacement[ 1024 ] = "";
		snprintf( SamplerNameReplacement, sizeof( SamplerNameReplacement ), Format2, i );

		StringReplacement( PS, SamplerNameOriginal, SamplerNameReplacement );
		StringReplacement( VS, SamplerNameOriginal, SamplerNameReplacement );

		snprintf( Line, sizeof( Line ), Format3, i, i );
	}
	else
	{
		snprintf( Line, sizeof( Line ), Format4, i, i );
	}
	int LineLen = strlen( Line );
	TargetString->insert( Cursor, Line );
	Cursor += LineLen;
}
void GeneratePropertyFields( std::string & GeneratedShader, MaterialBinding* Mat, std::string& GraphShader, std::string& VSStr )
{
	std::string* TargetString = &GeneratedShader;
	
	int Actual2DTexturesUsed[ MaxTextures ] = { 0 };
	int ActualCubeTexturesUsed[ MaxTextures ] = { 0 };
	int ActualTextureArraysUsed[ MaxTextures ] = { 0 };
	int ActualCubeArraysUsed[ MaxTextures ] = { 0 };
	int Actual3DTexturesUsed[ MaxTextures ] = { 0 };
	DetectNumTexturesUsed( *TargetString, Actual2DTexturesUsed, ActualCubeTexturesUsed, ActualTextureArraysUsed, ActualCubeArraysUsed, Actual3DTexturesUsed,
						   "CalcPixelMaterialInputs", "PixelMaterialInputs.PixelDepthOffset =" );
	if( VSStr.length() > 0 )
	{
		DetectNumTexturesUsed( VSStr, Actual2DTexturesUsed, ActualCubeTexturesUsed, ActualTextureArraysUsed, ActualCubeArraysUsed, Actual3DTexturesUsed,
							   nullptr, nullptr );
	}

	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP || CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
		TargetString = &GraphShader;

	int MainTexStart = TargetString->find( "_MainTex" );
	if( MainTexStart == -1 )
	{
		UE_LOG( LogTemp, Error, TEXT( "ERROR! GeneratePropertyFields pos == -1 wtf ?" ) );
		return;
	}

	int Cursor = TargetString->find( "\n", MainTexStart );
	Cursor++;

	int ParamIndex = 0;
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( Actual2DTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexProperty( Cursor, TargetString, i, ParamIndex, Mat, "Tex%d", "\t\tMaterial_Texture2D_%d( \"%s\", 2D ) = \"white\" {}\n" );
			ParamIndex++;
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualCubeTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexProperty( Cursor, TargetString, i, ParamIndex, Mat, "TexCube%d", "\t\tMaterial_TextureCube_%d( \"%s\", Cube ) = \"white\" {}\n" );
			ParamIndex++;
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( Actual3DTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexProperty( Cursor, TargetString, i, ParamIndex, Mat, "Tex3D%d", "\t\tMaterial_VolumeTexture_%d( \"%s\", 3D ) = \"white\" {}\n" );
			ParamIndex++;
		}
	}

	TargetString = &GeneratedShader;	

	const char* SamplersStart = "//samplers start";
	Cursor = TargetString->find( SamplersStart, 0 );
	Cursor = TargetString->find( "\n", Cursor );
	Cursor++;

	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP ||
		CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
	{
		const char* SamplerLines = "SAMPLER( SamplerState_Linear_Repeat );\n"
								   "SAMPLER( SamplerState_Linear_Clamp );\n";
		TargetString->insert( Cursor, SamplerLines );
		Cursor += strlen( SamplerLines );
	}

	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = (Actual2DTexturesUsed[ i ] == 1);
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_Texture2D_%dSampler", "sampler_Material_Texture2D_%d",
								"TEXTURE2D(       Material_Texture2D_%d );\n"
								"SAMPLER( sampler_Material_Texture2D_%d);\n",
								"\t\t\tuniform sampler2D    Material_Texture2D_%d;\n"
								"\t\t\tuniform SamplerState Material_Texture2D_%dSampler;\n" );
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualCubeTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_TextureCube_%dSampler", "sampler_Material_TextureCube_%d",
								"TEXTURECUBE(     Material_TextureCube_%d );\n"
								"SAMPLER( sampler_Material_TextureCube_%d);\n",
								"\t\t\tuniform samplerCUBE  Material_TextureCube_%d;\n"
								"\t\t\tuniform SamplerState Material_TextureCube_%dSampler;\n" );
		}
	}	
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualTextureArraysUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_Texture2DArray_%dSampler", "sampler_Material_Texture2DArray_%d",
								"TEXTURE2D_ARRAY( Material_Texture2DArray_%d );\n"
								"SAMPLER( sampler_Material_Texture2DArray_%d);\n",
								"\t\t\tuniform sampler2DArray    Material_Texture2DArray_%d;\n"
								"\t\t\tuniform SamplerState Material_Texture2DArray_%dSampler;\n" );
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( ActualCubeArraysUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_TextureCubeArray_%dSampler", "sampler_Material_TextureCubeArray_%d",
								"TEXTURECUBE_ARRAY( Material_TextureCubeArray_%d );\n"
								"SAMPLER( sampler_Material_TextureCubeArray_%d);\n",
								"\t\t\tuniform samplerCUBEArray  Material_TextureCubeArray_%d;\n"
								"\t\t\tuniform SamplerState Material_TextureCubeArray_%dSampler;\n" );
		}
	}
	for( int i = 0; i < MaxTextures; i++ )
	{
		bool Used = ( Actual3DTexturesUsed[ i ] == 1 );
		if( Used )
		{
			AddTexDecalaration( Cursor, TargetString, i, VSStr, GeneratedShader, "Material_VolumeTexture_%dSampler", "sampler_Material_VolumeTexture_%d",
								"TEXTURE3D(       Material_VolumeTexture_%d );\n"
								"SAMPLER( sampler_Material_VolumeTexture_%d);\n",
								"\t\t\tuniform sampler3D    Material_VolumeTexture_%d;\n"
								"\t\t\tuniform SamplerState Material_VolumeTexture_%dSampler;\n" );
		}
	}
}

MaterialBinding::MaterialBinding()
{
	UnityMat = new UnityMaterial();
	UnityScene->Materials.push_back( UnityMat );
}
FString MaterialBinding::GenerateName()
{
	FString MatName = MaterialInterface->GetName();
	
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>( MaterialInterface );
	if( MID )
	{
		int Index = -1;
		FString ParentName = MID->Parent->GetName();
		FString FinalName = ParentName + TEXT("(" ) + MatName + TEXT(")" );
		UnityMat->Name = ToANSIString( *FinalName );
		return FinalName;
	}
	else
	{
		UnityMat->Name = ToANSIString( *MatName );
		return MatName;
	}
}
bool IsTextureNormalMap( UMaterial* Mat, UTexture *Tex )
{
	TArray<UMaterialExpression*> OutExpressions;
	Mat->GetAllReferencedExpressions( OutExpressions, nullptr );
	for( int i = 0; i < OutExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = OutExpressions[ i ];
		UMaterialExpressionTextureSample* SampleExpression = Cast< UMaterialExpressionTextureSample>( Exp );
		if( SampleExpression )
		{
			if( SampleExpression->Texture == Tex )
			{
				if( SampleExpression->SamplerType == SAMPLERTYPE_Normal )
					return true;
				else
					return false;
				//if( SampleExpression->SamplerType == SAMPLERTYPE_Color )
			}
		}
	}

	return false;
}
bool IsTextureNormalMap( UMaterialInstance* MaterialInstance, UTexture* Tex )
{
	UMaterial* BaseMaterial = MaterialInstance->GetBaseMaterial();
	bool IsNormalMap = false;
	TArray<FMaterialParameterInfo> OutParameterInfo;
	TArray<FGuid> OutParameterIds;
	MaterialInstance->GetAllTextureParameterInfo( OutParameterInfo, OutParameterIds );
	for( int i = 0; i < OutParameterInfo.Num(); i++ )
	{
		FMaterialParameterInfo& Info = OutParameterInfo[ i ];

		UTexture* OutValue = nullptr;
		if( MaterialInstance->GetTextureParameterValue( Info, OutValue ) && OutValue )
		{
			if( OutValue == Tex )
			{
				UTexture* DefaultTexture = nullptr;
				if( BaseMaterial->GetTextureParameterDefaultValue( Info, DefaultTexture ) && DefaultTexture )
				{
					IsNormalMap = IsTextureNormalMap( BaseMaterial, DefaultTexture );
					return IsNormalMap;
				}
			}
		}
	}

	return false;
}
void GetMaterialTextures( UMaterialInterface* UnrealMat, TArray<UTexture*>& OutTextures, TArray<FString>& TextureParamNames )
{	
	const FMaterialResource* MaterialResource = UnrealMat->GetMaterialResource( GlobalFeatureLevel, GlobalQualityLevel );
	if( MaterialResource )
	{
		const FUniformExpressionSet& UniformExpressions = MaterialResource->GetUniformExpressions();
		for( int32 TypeIndex = 0; TypeIndex < NumMaterialTextureParameterTypes; TypeIndex++ )
		{
			int NumTextures = UniformExpressions.GetNumTextures( (EMaterialTextureParameterType)TypeIndex );
			for( int32 TextureIndex = 0; TextureIndex < NumTextures; TextureIndex++ )
			{
				// Evaluate the expression in terms of this material instance.
				UTexture* Texture = NULL;
				UniformExpressions.GetGameThreadTextureValue( (EMaterialTextureParameterType)TypeIndex, TextureIndex, UnrealMat, *MaterialResource, Texture, true );
				if( !Texture )
				{
					//Edge case when material instance texture is overriden but is null and UE uses the base texture in this case
					UniformExpressions.GetGameThreadTextureValue( (EMaterialTextureParameterType)TypeIndex, TextureIndex, UnrealMat->GetBaseMaterial(), *MaterialResource, Texture, true);
				}
				OutTextures.Add( Texture );
				FMaterialTextureParameterInfo Param = UniformExpressions.GetTextureParameter( (EMaterialTextureParameterType)TypeIndex, TextureIndex );
				#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
					FString ParamName = Param.ParameterInfo.Name.ToString();
				#else
					FString ParamName = Param.ParameterName;
				#endif
				if( ParamName.Equals( TEXT("None") ) && Texture)
				{
					ParamName = Texture->GetName();
				}
				TextureParamNames.Add( ParamName );
			}
		}
	}
	else
	{
		TArray< TArray<int32> > TextureIndices;
		UnrealMat->GetUsedTexturesAndIndices( OutTextures, TextureIndices, GlobalQualityLevel, GlobalFeatureLevel );
	}
}
void CountTextures( MaterialBinding* Mat, TArray<UTexture*>& AllUsedTextures )
{
	UMaterialInterface* UnrealMat = Mat->MaterialInterface;
	TArray<UTexture*> UsedTextures;
	TArray< TArray<int32> > TextureIndices;
	GetMaterialTextures( UnrealMat, UsedTextures, Mat->TextureParamNames );

	for( int i = 0; i < UsedTextures.Num(); i++ )
	{
		if( AllUsedTextures.Find( UsedTextures[ i ] ) == INDEX_NONE )
			AllUsedTextures.Add( UsedTextures[ i ] );
	}

	Mat->TextureParamNames.Reset( 0 );
}
TextureSemantic MaterialPropertyToTextureSemantic( EMaterialProperty MP )
{
	switch( MP )
	{
		case MP_EmissiveColor	: return TS_EmissionMap;
		case MP_Opacity			: return TS_Unknown;
		case MP_OpacityMask		: return TS_Unknown;
		case MP_BaseColor		: return TS_BaseMap;
		case MP_Metallic		: return TS_MetallicGlossMap;
		case MP_Specular		: return TS_MetallicGlossMap;
		case MP_Roughness		: return TS_Unknown;
		case MP_Normal			: return TS_BumpMap;
		case MP_AmbientOcclusion: return TS_OcclusionMap;
		//Unsure about this
		case MP_MaterialAttributes: return TS_BaseMap;

		default: return TS_Unknown;
	}
}
class SemanticArray
{
public:
	TArray<TextureSemantic> Array;
	TextureSemantic Selection = TS_Unknown;
};

void EvaluateSemantics( TArray<SemanticArray>& TextureSemantics, TArray<FString> TextureParamNames )
{
	//int SemanticCount[ TS_MaxSemantics ] = { 0 };
	//
	//for( int i = 0; i < TextureSemantics.Num(); i++ )
	//{
	//	SemanticCount[ TextureSemantics[ i ] ]++;
	//}

	const char* Keywords[ TS_MaxSemantics ][ 3 ] = {
		{"","",""},
		{"Diffuse","Albedo","Base"},//TS_BaseMap
		{"Normal","Nrm","Bump"},//TS_BumpMap
		{"Detail","",""},//TS_DetailAlbedoMap
		{"Detail","",""},//TS_DetailMask
		{"Detail","",""},//TS_DetailNormalMap
		{"Emission","Emissive",""},//TS_EmissionMap
		{"","",""},//TS_MainTex		
		{"Metallic","Gloss",""},//TS_MetallicGlossMap
		{"Occlusion","ORM",""},//TS_OcclusionMap
		{"Parallax","",""},//TS_ParallaxMap
		{"","",""},//TS_SpecGlossMap
	};
	for( int i = 1; i < TS_MaxSemantics; i++ )
	{
		TextureSemantic Semantic = (TextureSemantic)i;
		TArray<int> TexturesWithSemantic;
		for( int u = 0; u < TextureSemantics.Num(); u++ )
		{
			for( int t = 0; t < TextureSemantics[ u ].Array.Num(); t++ )
			{
				if( TextureSemantics[ u ].Array[ t ] == Semantic )
				{
					TexturesWithSemantic.Add( u );
				}
			}
		}

		for( int u = 0; u < TexturesWithSemantic.Num(); u++ )
		{
			bool Matches = false;
			for( int k = 0; k < 3; k++ )
			{
				if( TextureParamNames[ TexturesWithSemantic[ u ] ].Contains( Keywords[ Semantic ][ k ] ) )
				{
					//keep it
					Matches = true;
					break;
				}
			}
			if( Matches )
				TextureSemantics[ TexturesWithSemantic[ u ] ].Selection = Semantic;
		}
	}

	for( int u = 0; u < TextureSemantics.Num(); u++ )
	{
		if( TextureSemantics[ u ].Selection == TS_Unknown && TextureSemantics[ u ].Array.Num() > 0 )
			TextureSemantics[ u ].Selection = TextureSemantics[ u ].Array[ 0 ];
	}
}
void ExportMaterialTextures( MaterialBinding *Mat, TArray<UTexture*>& AllUsedTextures, int InitialTotalTextures )
{
	UMaterialInterface *UnrealMat = Mat->MaterialInterface;

	TArray<UTexture*> UsedTextures;
	TArray<SemanticArray> TextureSemantics;
	GetMaterialTextures( UnrealMat, UsedTextures, Mat->TextureParamNames );

	for( int i = 0; i < UsedTextures.Num(); i++ )
	{
		SemanticArray EmptyArray;
		TextureSemantics.Add( EmptyArray );
	}

	EMaterialProperty PropertiesToTrace[10] =
	{
		MP_EmissiveColor ,
		MP_Opacity ,
		MP_OpacityMask ,
		MP_BaseColor ,
		MP_Metallic ,
		MP_Specular ,
		MP_Roughness ,
		MP_Normal ,
		MP_AmbientOcclusion,
		MP_MaterialAttributes
	};
	auto BaseMaterial = UnrealMat->GetBaseMaterial();
	//if( UnrealMat->GetBaseMaterial()->MaterialAttributes.Expression )
	//{
	//	TArray<FExpressionInput*> ProcessedInputs;
	//	if( StartingExpression->Expression )
	//	{
	//		ProcessedInputs.AddUnique( StartingExpression );
	//
	//		EShaderFrequency ShaderFrequency = SF_NumFrequencies;
	//		// These properties are "special", attempting to pass them to FMaterialAttributeDefinitionMap::GetShaderFrequency() will generate log spam
	//		if( !( InProperty == MP_MaterialAttributes || InProperty == MP_CustomOutput ) )
	//		{
	//			ShaderFrequency = FMaterialAttributeDefinitionMap::GetShaderFrequency( InProperty );
	//		}
	//
	//		BaseMaterial->RecursiveGetExpressionChain( StartingExpression->Expression, ProcessedInputs, OutExpressions, InStaticParameterSet, InFeatureLevel, InQuality, InShadingPath, ShaderFrequency );
	//	}
	//}
	//else
	{
		int NumProps = sizeof( PropertiesToTrace ) / sizeof(PropertiesToTrace[ 0 ]);
		for( int32 MPIdx = 0; MPIdx < NumProps; MPIdx++ )
		{
			EMaterialProperty MaterialProp = PropertiesToTrace[ MPIdx ];
			TArray<UMaterialExpression*> MPRefdExpressions;
			FStaticParameterSet* InStaticParameterSet = nullptr;
			ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num;
			EMaterialQualityLevel::Type InQuality = EMaterialQualityLevel::Num;
			ERHIShadingPath::Type InShadingPath = ERHIShadingPath::Num;

			if( BaseMaterial->GetExpressionsInPropertyChain( MaterialProp, MPRefdExpressions, InStaticParameterSet, InFeatureLevel, InQuality, InShadingPath ) == true )
			{
				for( int i = 0; i < MPRefdExpressions.Num(); i++ )
				{
					UMaterialExpression* Exp = MPRefdExpressions[ i ];
					UMaterialExpressionTextureBase* TexBase = Cast< UMaterialExpressionTextureBase>( Exp );
					if( TexBase )
					{
						UMaterialExpressionTextureSampleParameter2D* SampleParam2D = Cast< UMaterialExpressionTextureSampleParameter2D>( Exp );
						int ReferencedIndex = -1;
						if( SampleParam2D )
						{
							for( int t = 0; t < UsedTextures.Num(); t++ )
							{
								if( SampleParam2D->ParameterName.ToString().Compare( Mat->TextureParamNames[ t ] ) == 0 )
								{
									ReferencedIndex = t;
								}
							}
						}
						else
						{
							for( int t = 0; t < UsedTextures.Num(); t++ )
							{
								if( TexBase->Texture == UsedTextures[ t ] )
								{
									ReferencedIndex = t;
								}
							}
						}
						if( ReferencedIndex != -1 )
						{
							TextureSemantic Semantic = MaterialPropertyToTextureSemantic( MaterialProp );
							TextureSemantics[ ReferencedIndex ].Array.Add( Semantic );
						}
					}
				}
			}
		}
	}

	EvaluateSemantics( TextureSemantics, Mat->TextureParamNames );
	
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( UnrealMat );

	int NumCubes = 0;
	int Num3D = 0;
	for( int i = 0; i < UsedTextures.Num(); i++ )
	{
		UTexture* Tex = UsedTextures[ i ];

		if( Tex )
		{
			bool IsNormalMap = false;
			if( MaterialInstance )
			{
				IsNormalMap = IsTextureNormalMap( MaterialInstance, Tex );
			}
			else
				IsNormalMap = IsTextureNormalMap( BaseMaterial, Tex );

			AllUsedTextures.Remove( Tex );
			int CurrentCount = InitialTotalTextures - AllUsedTextures.Num();
			UE_LOG( LogTemp, Log, TEXT( "Exporting Texture %s (%d/%d)" ), *Tex->GetName(), CurrentCount, InitialTotalTextures );
			TextureBinding* Binding = UETextureExporter::Export( Tex );

			Binding->IsNormalMap = IsNormalMap;
			if( !Binding->UnityTex )
			{
				Binding->UnityTex = new UnityTexture;
				FString TexturePath = GenerateTexturePath( Tex );
				Binding->UnityTex->File = ToANSIString( *TexturePath );
				Binding->UnityTex->GenerateGUID();
				UTextureCube* IsCube = Cast<UTextureCube>( Tex );
				if( IsCube )
				{
					Binding->UnityTex->Shape = UTS_Cube;
					Binding->UnityTex->SpecificIndex = NumCubes;
					NumCubes++;
				}
				UVolumeTexture* IsVolume = Cast<UVolumeTexture>( Tex );
				if( IsVolume )
				{
					Binding->UnityTex->Shape = UTS_3D;
					Binding->UnityTex->SpecificIndex = Num3D;
					Num3D++;
				}
				Binding->UnityTex->Semantic = TextureSemantics[ i ].Selection;
			}
			Mat->UnityMat->Textures.push_back( Binding->UnityTex );
		}
		else
			Mat->UnityMat->Textures.push_back( nullptr );
	}	
}

void FDebugViewModeMaterialProxy_AddShader( UMaterialInterface* InMaterialInterface, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, bool bSynchronousCompilation, EMaterialShaderMapUsage::Type InUsage )
{
	if( !InMaterialInterface ) return;

	const FMaterial* Material = InMaterialInterface->GetMaterialResource( FeatureLevel );
	if( !Material ) 
		return;
}

void GetMaterialShaderSource( MaterialBinding *Mat, bool IsOnGamethread )
{
	UMaterialInterface *M = Mat->MaterialInterface;
	bool WantsGameThread = false;
	UMaterialInstance *MatInstance = dynamic_cast<UMaterialInstance*>( M );
	if( MatInstance )
	{
		Mat->IsInstance = true;
		WantsGameThread = true;
		
		if ( IsOnGamethread )
		{
			FString ProjectDir = FPaths::ProjectContentDir();
			ProjectDir += TEXT("../");
			
			FString ShaderFile = ProjectDir;
			ShaderFile += TEXT("/Saved/ShaderDebugInfo/PCD3D_SM5/FDebugViewModeMaterialProxy " );
			ShaderFile += M->GetName();
			ShaderFile += TEXT("/FLocalVertexFactory/FMaterialTexCoordScalePS/MaterialTexCoordScalesPixelShader.usf" );

			ShaderFile = FPaths::ConvertRelativePathToFull( ShaderFile );

			if ( !FPlatformFileManager::Get().GetPlatformFile().FileExists( *ShaderFile ) || ForceShaderRecompilation )
			{
				FMaterialEditorUtilities::BuildTextureStreamingData( MatInstance );
			}

			FString ShaderFileWStr = ShaderFile;
			std::string ShaderFileStr = ToANSIString( *ShaderFileWStr );
			const char *ShaderFile2 = ShaderFileStr.c_str();
			uint8* Buffer = nullptr;
			int Size = LoadFile( ShaderFile2, &Buffer );
			if ( Size > 0 )
			{
				FString ShaderSource = ToWideString( ( char* )Buffer ).c_str();
				Mat->ShaderSource = ShaderSource;
				delete[] Buffer;
			}
		}		
	}

	if( WantsGameThread == IsOnGamethread )
	{
		Mat->MaterialResource = M->GetMaterialResource( GlobalFeatureLevel );
		Mat->RenderProxy = M->GetRenderProxy( );
		Mat->MaterialResource->GetMaterialExpressionSource( Mat->ShaderSource );

		//Set default material if shader couldn't compile for this material
		if( Mat->ShaderSource.Len() == 0 )
		{
			UObject* LoadedObject = StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, TEXT( "/Engine/EngineMaterials/DefaultMaterial" ) );
			UMaterialInterface* DefaultMaterial = Cast<UMaterialInterface>( LoadedObject );

			Mat->MaterialInterface = DefaultMaterial;

			Mat->MaterialResource = Mat->MaterialInterface->GetMaterialResource( GlobalFeatureLevel );
			Mat->RenderProxy = Mat->MaterialInterface->GetRenderProxy();
			Mat->MaterialResource->GetMaterialExpressionSource( Mat->ShaderSource );
		}

		auto ReferencedTextures = Mat->MaterialResource->GetReferencedTextures();
	}
}

std::string GetLine( std::string String, int position )
{
	int LineStart = String.rfind( '\n', position );
	int LineEnd = String.find( '\n', position );
	if( LineStart == LineEnd )
		return "";
	if( LineStart != -1 && LineEnd != -1 )
	{
		return String.substr( LineStart + 1, LineEnd - LineStart - 1 );
	}
	else
		return "";
}

int GetNumTexcoords( std::string UnrealHLSL )
{
	int pos = UnrealHLSL.find( "#define NUM_TEX_COORD_INTERPOLATORS" );
	if( pos != -1 )
	{
		std::string Line = GetLine( UnrealHLSL, pos );
		if( Line.length() > 0 )
		{
			int NumTexcoords = 0;
			if( sscanf( Line.c_str(), "#define NUM_TEX_COORD_INTERPOLATORS %d", &NumTexcoords ) == 1 )
			{
				return NumTexcoords;
			}
		}
	}

	return 0;
}
int GetNumTexcoordsVertex( std::string UnrealHLSL )
{
	int pos = UnrealHLSL.find( "#define NUM_MATERIAL_TEXCOORDS_VERTEX" );
	if( pos != -1 )
	{
		std::string Line = GetLine( UnrealHLSL, pos );
		if( Line.length() > 0 )
		{
			int NumTexcoords = 0;
			if( sscanf( Line.c_str(), "#define NUM_MATERIAL_TEXCOORDS_VERTEX %d", &NumTexcoords ) == 1 )
			{
				return NumTexcoords;
			}
		}
	}

	return 0;
}

int GetEndBraceOffset( std::string AllCode, int StartPos )
{
	int ExpStart = AllCode.rfind( '\n', StartPos );
	int FirstBrace = AllCode.find( '{', ExpStart );

	int Depth = 0;
	const char* str = AllCode.c_str();
	for( int i = FirstBrace + 1; i < AllCode.length(); i++ )
	{
		if( AllCode[ i ] == '{' )
		{
			Depth++;
		}
		if( AllCode[ i ] == '}' )
		{
			Depth--;
		}
		if( Depth < 0 )
		{			
			return i;
		}
	}

	UE_LOG( LogTemp, Error, TEXT( "ERROR! GetEndBraceOffset didn't found the end of the function, wtf ?" ) );

	return ExpStart;
}
//Convert HLSL Samples to CG
void ReplaceHLSLSampleCall( std::string & Exp, const char* strToFind, const char* ReplacementFunc )
{
	int FindLen = strlen( strToFind );

	int pos = -1;
	do
	{
		pos = Exp.find( strToFind );
		if( pos != -1 )
		{
			std::string TexName;
			int TexBegin = -1;
			for( int i = pos - 1; i > 0; i-- )
			{
				char c = Exp[ i ];
				if( c == ' ' || c == '\t' || c == '\n' || c == ',' || c == '(' || c == ')' || c == '{' || c == '}' )
				{
					TexName = Exp.substr( i + 1, pos - i - 1 );
					TexBegin = i + 1;
					break;
				}
			}
			if( TexBegin == -1 )
			{
				UE_LOG( LogTemp, Error, TEXT( "ERROR! ProcessCustomExpression couldn't detect TexBegin !" ) );
				return;
			}

			std::string Replacement = ReplacementFunc;
			Replacement += TexName;
			Replacement += ", ";
			Exp.replace( TexBegin, TexName.length() + FindLen, Replacement.c_str() );
		}
	} while( pos != -1 );
}
void ProcessCustomExpression( std::string& Exp )
{
	ReplaceHLSLSampleCall( Exp, ".SampleGrad(", "Texture2DSampleGrad( ");
	ReplaceHLSLSampleCall( Exp, ".Sample(", "Texture2DSample( ");
	ReplaceHLSLSampleCall( Exp, ".SampleLevel(", "Texture2DSampleLevel( ");
	ReplaceHLSLSampleCall( Exp, ".SampleBias(", "Texture2DSampleBias( " );

	StringReplacement( Exp, "Material.", "Material_" );
	StringReplacement( Exp, "const", " " );

	StringReplacement( Exp, "View.BufferSizeAndInvSize", "View_BufferSizeAndInvSize" );
	StringReplacement( Exp, "View.MaterialTextureBilinearWrapedSampler", "View_MaterialTextureBilinearWrapedSampler" );
	StringReplacement( Exp, "View.MaterialTextureBilinearClampedSampler", "View_MaterialTextureBilinearClampedSampler" );

	StringReplacement( Exp, "while", "LOOP\nwhile" );
}
bool ReplaceVertexParametersWithPixelParameters( std::string& Code )
{
	const char* FindStr = "FMaterialVertexParameters";
	const char* ReplaceStr = "FMaterialPixelParameters";
	int ParamPos = Code.find( FindStr );
	if( ParamPos != -1 )
	{
		Code.replace( ParamPos, strlen( FindStr ), ReplaceStr );
		return true;
	}
	else
		return false;
}
void AddCustomExpressions( std::string & MaterialCode, std::string AllCode )
{
	int Index = 0;
	char Text[ 64 ];
	size_t pos = 0;
	do
	{
		snprintf( Text, sizeof( Text ), "CustomExpression%d", Index );
		pos = AllCode.find( Text );
		if ( pos != -1 )
		{
			int ExpStart = AllCode.rfind( '\n', pos );
			int EndBrace = GetEndBraceOffset( AllCode, pos );

			std::string ExpFunction = AllCode.substr( ExpStart, EndBrace + 1 - ExpStart );

			//ReplaceVertexParametersWithPixelParameters( ExpFunction );
			ProcessCustomExpression( ExpFunction );

			std::string MaterialCodeWithExpressions = ExpFunction;
			MaterialCodeWithExpressions += '\n';
			MaterialCodeWithExpressions += MaterialCode;
			MaterialCode = MaterialCodeWithExpressions;
		}
		Index++;
	}
	while ( pos != -1 );
}
void ExecuteStringReplacements( std::string& ShaderCode )
{
	StringReplacement( ShaderCode, "Material.Texture", "Material_Texture" );
	StringReplacement( ShaderCode, "Material.VolumeTexture", "Material_VolumeTexture" );
	StringReplacement( ShaderCode, "Material.VirtualTexturePhysical", "Material_VirtualTexturePhysical" );
	StringReplacement( ShaderCode, "Material.Wrap_WorldGroupSettings", "Material_Wrap_WorldGroupSettings" );
	StringReplacement( ShaderCode, "Material.Clamp_WorldGroupSettings", "Material_Clamp_WorldGroupSettings" );
	
	StringReplacement( ShaderCode, "Material_VectorExpressions", "Material.VectorExpressions" );
	StringReplacement( ShaderCode, "Material_ScalarExpressions", "Material.ScalarExpressions" );
	
	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP ||
		CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
	{
		StringReplacement( ShaderCode, "View.MaterialTextureBilinearWrapedSampler", "View_MaterialTextureBilinearWrapedSampler" );
		StringReplacement( ShaderCode, "View.MaterialTextureBilinearClampedSampler", "View_MaterialTextureBilinearClampedSampler" );
	}
}
std::string GetMaterialCollectionString( const UMaterialParameterCollection* Collection, int CollectionIndex)
{
	int NumVectorsForScalars = ( Collection->ScalarParameters.Num() + 3 ) / 4;
	int TotalVectors = NumVectorsForScalars + Collection->VectorParameters.Num();

	FLinearColor* ScalarVectors = new FLinearColor[ NumVectorsForScalars ];
	std::string ValuesStr;
	std::string* ParamNames = new std::string[ NumVectorsForScalars * 4 ];
	for( int i = 0; i < Collection->ScalarParameters.Num(); i++ )
	{
		FCollectionScalarParameter Param = Collection->ScalarParameters[ i ];
		float* FloatArray = (float*)ScalarVectors;
		FloatArray[ i ] = Param.DefaultValue;
		ParamNames[ i ] = ToANSIString( *Param.ParameterName.ToString());
	}
	for( int i = 0; i < NumVectorsForScalars; i++ )
	{
		char Line[ 1024 ] = "";
		ScalarVectors[ i ] = RemoveNans( ScalarVectors[ i ] );
		snprintf( Line, sizeof( Line ), "\tMaterialCollection%d.Vectors[%d] = float4(%f,%f,%f,%f);//%s,%s,%s,%s\n", CollectionIndex, i, ScalarVectors[i].R, ScalarVectors[ i ].G, ScalarVectors[ i ].B, ScalarVectors[ i ].A,
				   ParamNames[ i * 4 + 0].c_str(), ParamNames[ i * 4 + 1 ].c_str(), ParamNames[ i * 4 + 2 ].c_str(), ParamNames[ i * 4 + 3 ].c_str() );
		ValuesStr += Line;
	}
	for( int i = 0; i < Collection->VectorParameters.Num(); i++ )
	{
		FCollectionVectorParameter Param = Collection->VectorParameters[ i ];
		FLinearColor V = Param.DefaultValue;
		char Line[ 1024 ] = "";
		std::string str = ToANSIString( *Param.ParameterName.ToString() );
		snprintf( Line, sizeof( Line ), "\tMaterialCollection%d.Vectors[%d] = float4(%f,%f,%f,%f);//%s\n", CollectionIndex, NumVectorsForScalars + i, V.R, V.G, V.B, V.A, str.c_str() );
		ValuesStr += Line;
	}

	FString CollectionName = Collection->GetName();
	auto CollectionNameStr = ToANSIString( *CollectionName );
	char Text[ 1024 ] = "";
	snprintf( Text, sizeof( Text ), "struct MaterialCollection%dType\n"
		"{\n"
		"\tfloat4 Vectors[%d];\n"
		"};\n"
		"//%s\n"
		"MaterialCollection%dType MaterialCollection%d;\n"
		"void Initialize_MaterialCollection%d()\n"
		"{\n", CollectionIndex, TotalVectors, CollectionNameStr.c_str(), CollectionIndex, CollectionIndex, CollectionIndex);
	std::string Ret = Text;
	Ret += ValuesStr;

	Ret += "}\n";

	delete[] ScalarVectors;
	delete[] ParamNames;
	return Ret;
}
void UncommentLine( std::string & ShaderString, const char *Marker )
{
	while( 1 )
	{
		size_t Pos = ShaderString.find( Marker );
		if( Pos != ShaderString.npos )
			ShaderString.replace( Pos, strlen( Marker ), "" );//uncomment it
		else
			break;
	}
}
void RemoveComment( std::string& ShaderString, const char* Marker )
{
	while( 1 )
	{
		size_t Pos = ShaderString.find( Marker );
		if( Pos != ShaderString.npos )
			ShaderString.replace( Pos, 2, "" );//remove just "//"
		else
			break;
	}
}
void CommentLine( std::string& ShaderString, const char* Marker )
{
	int Offset = 0;
	while( 1 )
	{
		size_t Pos = ShaderString.find( Marker, Offset );
		if( Pos != ShaderString.npos )
		{
			ShaderString.insert( Pos, "//" );//add comment
			Offset = Pos + 3;
		}
		else
			break;
	}
}
std::vector<std::string> GetLines( std::string String, int position, int NumLines )
{
	std::vector<std::string> Lines;

	int FirstLineStart = String.rfind( '\n', position );
	int Cursor = FirstLineStart + 1;
	for( int i = 0; i < NumLines; i++ )
	{
		std::string Line = GetLine( String, Cursor );
		Cursor += Line.length() + 2;
		Lines.push_back( Line );
	}

	return Lines;
}
void FindAndAddInterpolator( const char* Identifier, std::string HLSL, std::string& GeneratedShader, int InterpStart, int& NextLineMarker )
{
	int DefineCursor = HLSL.find( Identifier, InterpStart );
	if( DefineCursor != -1 )
	{
		std::string Line = GetLine( HLSL, DefineCursor );
		Line += "\n";
		GeneratedShader.insert( NextLineMarker, Line );
		NextLineMarker += Line.length();
	}
}
void ProcessVertexInterpolators( std::string HLSL, std::string & GeneratedShader )
{
	int NumInterpolators = -1;
	int InterpStart = HLSL.find( "NUM_CUSTOM_VERTEX_INTERPOLATORS" );
	if( InterpStart > 0 )
	{
		std::string Line = GetLine( HLSL, InterpStart );
		if( Line.length() > 0 )
		{			
			sscanf( Line.c_str(), "#define NUM_CUSTOM_VERTEX_INTERPOLATORS %d", &NumInterpolators);
		}
	}

	if( NumInterpolators > 0 )
	{
		const char* NUM_CUSTOM_VERTEX_INTERPOLATORS = "NUM_CUSTOM_VERTEX_INTERPOLATORS";
		int NumInterpolatorsCursor = GeneratedShader.find( NUM_CUSTOM_VERTEX_INTERPOLATORS );
		if( NumInterpolatorsCursor == -1 )
			return;

		char Text[ 256 ];
		snprintf( Text, sizeof( Text ), "%d", NumInterpolators );
		GeneratedShader.replace( NumInterpolatorsCursor + strlen( NUM_CUSTOM_VERTEX_INTERPOLATORS ) + 1, strlen( Text ), Text );
		int NextLineMarker = GeneratedShader.find( '\n', NumInterpolatorsCursor );
		NextLineMarker++;

		for( int i = 0; i < NumInterpolators; i++ )
		{
			snprintf( Text, sizeof( Text ), "#define VERTEX_INTERPOLATOR_%d_TEXCOORDS_X", i );
			FindAndAddInterpolator( Text, HLSL, GeneratedShader, InterpStart, NextLineMarker );
			snprintf( Text, sizeof( Text ), "#define VERTEX_INTERPOLATOR_%d_TEXCOORDS_Y", i );
			FindAndAddInterpolator( Text, HLSL, GeneratedShader, InterpStart, NextLineMarker );
			snprintf( Text, sizeof( Text ), "#define VERTEX_INTERPOLATOR_%d_TEXCOORDS_Z", i );
			FindAndAddInterpolator( Text, HLSL, GeneratedShader, InterpStart, NextLineMarker );
			snprintf( Text, sizeof( Text ), "#define VERTEX_INTERPOLATOR_%d_TEXCOORDS_W", i );
			FindAndAddInterpolator( Text, HLSL, GeneratedShader, InterpStart, NextLineMarker );
		}		
	}
}
void ReplaceShaderName( std::string & GeneratedShader, const char * InsertMarker, std::string MatNameStr )
{
	int Pos = GeneratedShader.find( InsertMarker );
	GeneratedShader.replace( Pos, strlen( InsertMarker ), MatNameStr.c_str() );
}
const TArray<UMaterialExpression*> GetFunctionExpressions( UMaterialFunctionInterface* MaterialFunction)
{
	#if ENGINE_MAJOR_VERSION >= 5
		TArray<UMaterialExpression*> Ret;
		const TArray<TObjectPtr<UMaterialExpression>>* FunctionExpressions = MaterialFunction->GetFunctionExpressions();
		for( int i = 0; i < FunctionExpressions->Num(); i++ )
		{
			Ret.Add( ( *FunctionExpressions )[ i ] );
		}	
		return Ret;
	#else
		return *MaterialFunction->GetFunctionExpressions();
	#endif
}
bool ExportVS = true;
std::string ExtractFunction( const char* CSource, const char *Marker )
{
	const char* ptr = strstr( CSource, Marker );
	if( ptr )
	{
		int CropStart = (int64_t)ptr - (int64_t)CSource;
		int EndBrace = GetEndBraceOffset( CSource, CropStart );
		int CropEnd = EndBrace;

		int CropSize = CropEnd - CropStart + 1;
		if( CropSize < 0 )
		{
			return "";
		}
		char* StringContents = new char[ CropSize ];
		StringContents[ CropSize ] = 0;
		memcpy( StringContents, ptr, CropSize );
		std::string RetStr = StringContents;
		RetStr += "\n";

		return RetStr;
	}
	else
		return "";
}
const char* PixelToVertexParameters =
"\n"
"	FMaterialVertexParameters Parameters = (FMaterialVertexParameters)0;\n"
"	Parameters.TangentToWorld = PixelParameters.TangentToWorld;\n"
"	Parameters.WorldPosition = PixelParameters.AbsoluteWorldPosition;\n"
"	Parameters.VertexColor = PixelParameters.VertexColor;\n"
"#if NUM_MATERIAL_TEXCOORDS_VERTEX > 0\n"
"	for( int i = 0; i < NUM_MATERIAL_TEXCOORDS_VERTEX; i++ )\n"
"	{\n"
"		Parameters.TexCoords[i] = PixelParameters.TexCoords[i];\n"
"	}\n"
"#endif\n"
"	Parameters.PrimitiveId = PixelParameters.PrimitiveId;\n";
void SetupCustomizedUVs( std::string& CustomizedUVs )
{
	if( CustomizedUVs.length() == 0 )
		return;

	CustomizedUVs = std::string("\n") + CustomizedUVs;

	const char* Marker = "FMaterialVertexParameters Parameters";
	int MarkerPos = CustomizedUVs.find( Marker );
	
	const char* PixelParameters = "FMaterialPixelParameters PixelParameters";
	CustomizedUVs.replace( MarkerPos, strlen( Marker ), PixelParameters );
	
	int FirstBraceStart = CustomizedUVs.find( "{" );
	CustomizedUVs.insert( FirstBraceStart + 1, PixelToVertexParameters );

	ExecuteStringReplacements( CustomizedUVs );

	//if( !ReplaceVertexParametersWithPixelParameters( CustomizedUVs ) )
	//{
	//	UE_LOG( LogTemp, Warning, TEXT( "ERROR! FMaterialVertexParameters was not found, WTF ?" ) );
	//	CustomizedUVs = "";
	//}	
}
void SetupTexcoordInterpolators( std::string& ShaderSourceANSI, std::string& GeneratedShader )
{
	int NumTexcoords = GetNumTexcoords( ShaderSourceANSI );
	const char* NumTexcoordsMarker = "NUM_TEX_COORD_INTERPOLATORS";
	char NumTexcoordsStr[ 16 ];
	snprintf( NumTexcoordsStr, sizeof( NumTexcoordsStr ), "%d", NumTexcoords );
	int Pos = GeneratedShader.find( NumTexcoordsMarker );
	if( Pos != -1 )
		GeneratedShader.replace( Pos + strlen( NumTexcoordsMarker ) + 1, strlen( NumTexcoordsStr ), NumTexcoordsStr );
}
void SetupVertexTexcoordInterpolators( std::string& ShaderSourceANSI, std::string& GeneratedShader )
{
	int NumTexcoords = GetNumTexcoordsVertex( ShaderSourceANSI );
	const char* NumTexcoordsMarker = "NUM_MATERIAL_TEXCOORDS_VERTEX";
	char NumTexcoordsStr[ 16 ];
	snprintf( NumTexcoordsStr, sizeof( NumTexcoordsStr ), "%d", NumTexcoords );
	int Pos = GeneratedShader.find( NumTexcoordsMarker );
	if ( Pos != -1 )
		GeneratedShader.replace( Pos + strlen( NumTexcoordsMarker ) + 1, strlen( NumTexcoordsStr ), NumTexcoordsStr );	
}
void GetMaterialCollectionIndices( UMaterialInterface* UnrealMat, TArray< UMaterialParameterCollection* >& Collections )
{	
	EMaterialProperty Properties[] =
	{
		MP_Normal,
		MP_EmissiveColor,
		MP_DiffuseColor,
		MP_SpecularColor,
		MP_BaseColor,
		MP_Metallic,
		MP_Specular,
		MP_Roughness,
		MP_Anisotropy,
		MP_Opacity,
		MP_OpacityMask,
		MP_Tangent,
		MP_WorldPositionOffset,
		MP_ShadingModel,
		#if ENGINE_MAJOR_VERSION >= 5
			MP_FrontMaterial,
		#endif
		MP_SubsurfaceColor,
		MP_CustomData0,
		MP_CustomData1,
		MP_AmbientOcclusion,
		MP_Refraction,

		MP_PixelDepthOffset
	};

	auto BaseMaterial = UnrealMat->GetBaseMaterial();
		
	int ArraySize = sizeof( Properties ) / sizeof( Properties[ 0 ] );
	for( int32 MPIdx = 0; MPIdx < ArraySize; MPIdx++ )
	{
		EMaterialProperty MaterialProp = Properties[ MPIdx ];
		TArray<UMaterialExpression*> MPRefdExpressions;
		FStaticParameterSet* InStaticParameterSet = nullptr;
		ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num;
		EMaterialQualityLevel::Type InQuality = EMaterialQualityLevel::Num;
		ERHIShadingPath::Type InShadingPath = ERHIShadingPath::Num;

		if( BaseMaterial->GetExpressionsInPropertyChain( MaterialProp, MPRefdExpressions, InStaticParameterSet, InFeatureLevel, InQuality, InShadingPath ) == true )
		{
			for( int i = 0; i < MPRefdExpressions.Num(); i++ )
			{
				UMaterialExpression* Exp = MPRefdExpressions[ i ];
				UMaterialExpressionCollectionParameter* ExpCollectionParameter = Cast<UMaterialExpressionCollectionParameter>( Exp );

				if( ExpCollectionParameter )
				{
					if( Collections.Find( ExpCollectionParameter->Collection ) == INDEX_NONE )
					{
						Collections.Add( ExpCollectionParameter->Collection );
					}
				}
			}
		}
	}	
}
void ProcessMaterial_RenderThread( MaterialBinding *Mat, const char *ProxyShaderString, const char* GraphShaderString )
{
	GetMaterialShaderSource( Mat, false );

	bool UsesAlphaTest = false;
	bool IsTransparent = false;

	auto OpacityMaskExp = Mat->BaseMaterial->OpacityMask.Expression;
	
	auto AttributesExp = Mat->BaseMaterial->MaterialAttributes.Expression;
	if ( AttributesExp )
	{
		UMaterialExpressionMakeMaterialAttributes *MakeMaterialAttributes = dynamic_cast<UMaterialExpressionMakeMaterialAttributes*>( AttributesExp );
		if ( MakeMaterialAttributes )
		{
			OpacityMaskExp = MakeMaterialAttributes->OpacityMask.Expression;
		}		
	}
	
	EBlendMode BlendMode = Mat->BaseMaterial->BlendMode;
	if( Mat->MaterialInstance )
		BlendMode = Mat->MaterialInstance->GetBlendMode();

	if ( BlendMode == BLEND_Masked )
		UsesAlphaTest = true;
	if( BlendMode == BLEND_Translucent || BlendMode == BLEND_Additive || BlendMode == BLEND_Modulate )
		IsTransparent = true;

	std::string MaterialCollectionDefinitions;
	TArray<UMaterialExpression*> OutExpressions = Mat->BaseMaterial->Expressions;
	TArray<UMaterialParameterCollection*> Collections;
	TArray<bool> CollectionsInitializations;
	GetMaterialCollectionIndices( Mat->BaseMaterial, Collections );
	for( int i = 0; i < Collections.Num(); i++ )
		CollectionsInitializations.Add( false );

	for( int i = 0; i < OutExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = OutExpressions[ i ];

		UMaterialExpressionMaterialFunctionCall* ExpMFC = dynamic_cast<UMaterialExpressionMaterialFunctionCall*>( Exp );
		if( ExpMFC && ExpMFC->MaterialFunction )
		{
			TArray<UMaterialExpression*> FunctionExpressions = GetFunctionExpressions( ExpMFC->MaterialFunction );

			for( int u = 0; u < FunctionExpressions.Num(); u++ )
			{
				Exp = FunctionExpressions[ u ];
				OutExpressions.Add( Exp );
			}
			continue;
		}
		UMaterialExpressionCollectionParameter* ExpCollectionParameter = Cast<UMaterialExpressionCollectionParameter>( Exp );

		if( ExpCollectionParameter && ExpCollectionParameter->Collection )
		{
			int Index = Collections.Find( ExpCollectionParameter->Collection );
			bool WriteDefinition = false;
			if( Index == INDEX_NONE )
			{
				Index = Collections.Num();
				Collections.Add( ExpCollectionParameter->Collection );				
				WriteDefinition = true;
				CollectionsInitializations.Add( true );
			}
			else if( !CollectionsInitializations[ Index ] )
			{
				CollectionsInitializations[ Index ] = true;
				WriteDefinition = true;
			}
			if ( WriteDefinition )
			{
				const UMaterialParameterCollection* Collection = ExpCollectionParameter->Collection;
				std::string MatCollectionDef = GetMaterialCollectionString( Collection, Index );
				MaterialCollectionDefinitions += MatCollectionDef;
			}
		}
	}

	UMaterialInterface *M = Mat->MaterialInterface;

	if ( Mat->ShaderSource.Len() > 0 )
	{
		FString MatName = Mat->GenerateName();

		//Don't write shaders if using standard material
		if( CVarUseStandardMaterial.GetValueOnAnyThread() == 1 )
			return;

		int Len = MatName.Len();
		if ( Len > 0 )
		{
			FString ShaderFileName;
			ShaderFileName = GetOutputFile( TEXT("Shaders/"), MatName, TEXT(".shader") );
			
			FString UnrealHLSLFile = GetOutputFile( TEXT("Shaders/"), MatName, TEXT(".hlsl") );

			FString ShaderOutDir = GetUnityAssetsFolder();
			ShaderOutDir += TEXT("Shaders/");
			VerifyOrCreateDirectory( ShaderOutDir );

			FString ShaderSourceW = Mat->ShaderSource;
			std::string ShaderSourceANSI = ToANSIString( *ShaderSourceW );
			const char *CSource = ShaderSourceANSI.c_str();

			//Debug original UE HLSL in case of issues
			bool DebugUnrealHLSL = false;
			if ( DebugUnrealHLSL )
				SaveFile( ToANSIString( *UnrealHLSLFile ).c_str(), (uint32* )CSource, strlen( CSource ) );
			const char *ptr = strstr( CSource, "void CalcPixelMaterialInputs" );
			if ( ptr )
			{
				int CropStart = ( int64_t )ptr - ( int64_t )CSource;
				const char *EndPtr = strstr( ptr, "}" );
				if ( !EndPtr )
					return;
				int CropEnd = ( int64_t )EndPtr - ( int64_t )CSource;

				int CropSize = CropEnd - CropStart + 1;
				if ( CropSize < 0 )
				{
					return;
				}
				char *CalcPixelMaterialInputsStr = new char[ CropSize ];
				CalcPixelMaterialInputsStr[ CropSize ] = 0;
				memcpy( CalcPixelMaterialInputsStr, ptr, CropSize );

				std::string CalcPixelMaterialInputsFinal = CalcPixelMaterialInputsStr;

				//Fix Normal related calculations
				int BracketStart = CalcPixelMaterialInputsFinal.find( "{" );
				CalcPixelMaterialInputsFinal.insert( BracketStart + 1, "\n\tfloat3 WorldNormalCopy = Parameters.WorldNormal;\n" );

				const char *AfterNormalsMarker = "// Now the rest of the inputs";
				int AfterNormalCodeStart = CalcPixelMaterialInputsFinal.find( AfterNormalsMarker );

				if( AfterNormalCodeStart != -1 )
				{
					const char* TangentToWorldAxisFlip =
						"\n"
						"\t//WorldAligned texturing & others use normals & stuff that think Z is up\n"
						"\tParameters.TangentToWorld[0] = Parameters.TangentToWorld[0].xzy;\n"
						"\tParameters.TangentToWorld[1] = Parameters.TangentToWorld[1].xzy;\n"
						"\tParameters.TangentToWorld[2] = Parameters.TangentToWorld[2].xzy;\n";

					CalcPixelMaterialInputsFinal.insert( AfterNormalCodeStart + strlen(AfterNormalsMarker), TangentToWorldAxisFlip );
					StringReplacement( CalcPixelMaterialInputsFinal, "Parameters.WorldNormal", "WorldNormalCopy", AfterNormalCodeStart );
				}
				else
					UE_LOG( LogTemp, Error, TEXT( "AfterNormalCodeStart wasn't found !" ));
				//<-

				ExecuteStringReplacements( CalcPixelMaterialInputsFinal );
				
				std::string GeneratedShader = ProxyShaderString;
				std::string GraphShaderStr;
				if ( GraphShaderString )
					GraphShaderStr = GraphShaderString;

				const char *InsertMarker = "M_Chair";
				std::string MatNameStr = ToANSIString( *MatName );

				if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP || CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
				{
					ReplaceShaderName( GraphShaderStr, InsertMarker, MatNameStr );
				}
				else
				{
					ReplaceShaderName( GeneratedShader, InsertMarker, MatNameStr );
				}

				SetupTexcoordInterpolators( ShaderSourceANSI, GeneratedShader );
				SetupVertexTexcoordInterpolators( ShaderSourceANSI, GeneratedShader );
				int Pos = -1;

				//Need to add alpha here if opacity/opacity mask is used ! + render que transparent
				//#pragma surface surf Standard

				//opacity = blending
				//opacity mask = alpha test
				
				if( Mat->MaterialInterface->IsTwoSided() )
				{
					const char *CullingMarker = "//Cull Off";
					
					std::string* TargetCode = &GeneratedShader;
					if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP || CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
						TargetCode = &GraphShaderStr;

					while( 1 )
					{
						Pos = TargetCode->find( CullingMarker );
						if( Pos != TargetCode->npos )
							TargetCode->replace( Pos, 2, "" );//uncomment it
						else
							break;
					}
				}

				std::string* TransparencyFixTarget = &GeneratedShader;
				if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP || CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
				{
					TransparencyFixTarget = &GraphShaderStr;
				}
				if( IsTransparent )
				{
					UncommentLine( *TransparencyFixTarget, "//BLEND_ON" );

					if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
					{
						StringReplacement( GraphShaderStr, "_SurfaceType(\"Float\", Float) = 0", "_SurfaceType(\"Float\", Float ) = 1" );
					}
				}
				else
				{
					UncommentLine( *TransparencyFixTarget, "//BLEND_OFF" );
				}
				if( BlendMode == BLEND_Additive )
				{
					UncommentLine( *TransparencyFixTarget, "//BLEND_ADDITIVE" );
				}

				#if ENGINE_MAJOR_VERSION >= 5
					RemoveComment( GeneratedShader, "//#define UE5" );
				#else
					CommentLine( GeneratedShader, "#include \"LargeWorldCoordinates.hlsl\"" );
				#endif

				if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP)
				{
					RemoveComment( GeneratedShader, "//#define URP" );
				}
				else if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
				{
					RemoveComment( GeneratedShader, "//#define HDRP" );
				}
				if ( UsesAlphaTest )
				{
					const char *OpacitMaskMarker = "if( PixelMaterialInputs.OpacityMask";
					Pos = GeneratedShader.find( OpacitMaskMarker );
					GeneratedShader.replace( Pos - 2, 2, "" );//uncomment it
				}
				if( Mat->BaseMaterial->bTangentSpaceNormal == 0 )
				{
					StringReplacement( GeneratedShader, "HAS_WORLDSPACE_NORMAL 0", "HAS_WORLDSPACE_NORMAL 1" );
				}

				//move to render thread always!
				int NumVectorExpressions = 0;
				int NumScalarExpressions = 0;
				std::string ExpressionsData = GenerateInitializeExpressions( Mat, NumVectorExpressions, NumScalarExpressions );

				NumVectorExpressions = FMath::Max( NumVectorExpressions, 1 );
				NumScalarExpressions = FMath::Max( NumScalarExpressions, 1 );
				
				int NumScalarVectors = ( NumScalarExpressions + 3 ) / 4;
				
				GeneratedShader = AddNumberInString( GeneratedShader, "VectorExpressions[", NumVectorExpressions );
				GeneratedShader = AddNumberInString( GeneratedShader, "ScalarExpressions[", NumScalarVectors );
				#if ENGINE_MAJOR_VERSION >= 5
					StringReplacement( GeneratedShader, "VectorExpressions[", "PreshaderBuffer[" );
				#endif

				InsertMarker = "MaterialStruct Material;";
				Pos = GeneratedShader.find( InsertMarker );
				Pos += strlen( InsertMarker );

				GeneratedShader.insert( Pos + 1, ExpressionsData );				
				Pos += ExpressionsData.length();

				if( MaterialCollectionDefinitions.length() > 0 )
				{
					GeneratedShader.insert( Pos, MaterialCollectionDefinitions );
					Pos += MaterialCollectionDefinitions.length();

					int Offset = 0;
					while( 1 )//Do this so we initialize the data in VS and PS
					{
						InsertMarker = "InitializeExpressions();";
						int InitPos = GeneratedShader.find( InsertMarker, Offset );
						if( InitPos == -1 )
							break;
						InitPos += strlen( InsertMarker );

						std::string MaterialCollectionInits;
						for( int i = 0; i < Collections.Num(); i++ )
						{
							char Text[ 1024 ] = "";
							snprintf( Text, sizeof( Text ), "\n\tInitialize_MaterialCollection%d();\n", i );
							MaterialCollectionInits += Text;
						}

						GeneratedShader.insert( InitPos, MaterialCollectionInits );
						Offset = InitPos + MaterialCollectionInits.length();
					}
				}

				if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP )
				{
					StringReplacement( GraphShaderStr, "ProxyURP", MatNameStr.c_str() );
				}
				else if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
				{
					StringReplacement( GraphShaderStr, "ProxyHDRP", MatNameStr.c_str() );
				}
				
				#if ENGINE_MAJOR_VERSION >= 5
					const char* VSMarker = "WPO_PRECISE(float3) GetMaterialWorldPositionOffset(";
				#else
					const char* VSMarker = "float3 GetMaterialWorldPositionOffset(";
				#endif
				std::string VSStr = ExtractFunction( CSource, VSMarker );
				ExecuteStringReplacements( VSStr );
				
				std::string CustomizedUVs;
				if( Mat->BaseMaterial->NumCustomizedUVs > 0 )
				{
					CustomizedUVs = ExtractFunction( CSource, "void GetMaterialCustomizedUVs" );
					SetupCustomizedUVs( CustomizedUVs );
				}
				
				size_t posPS = CalcPixelMaterialInputsFinal.find( "CustomExpression" );
				size_t posVS = VSStr.find( "CustomExpression" );
				if( posPS != -1 || posVS != -1 )
				{
					if( ExportVS )
					{
						AddCustomExpressions( VSStr, CSource );
						if( CustomizedUVs.length() > 0 )
						{
							VSStr += CustomizedUVs;
						}
					}
					else
					{
						AddCustomExpressions( CalcPixelMaterialInputsFinal, CSource );
						if( CustomizedUVs.length() > 0 )
						{
							CalcPixelMaterialInputsFinal += CustomizedUVs;
						}
					}
				}
				else
				{
					if ( CustomizedUVs.length() > 0 )
						GeneratedShader.insert( Pos, CustomizedUVs );
				}

				GeneratedShader.insert( Pos, CalcPixelMaterialInputsFinal );

				GeneratePropertyFields( GeneratedShader, Mat, GraphShaderStr, VSStr );

				ProcessVertexInterpolators( ShaderSourceANSI, GeneratedShader );

				if( ExportVS )
				{
					int Offset = GeneratedShader.find( "void CalcPixelMaterialInputs" );
					if( Offset != -1 )
					{
						GeneratedShader.insert( Offset, VSStr );
						#if ENGINE_MAJOR_VERSION >= 5
							GeneratedShader.insert( Offset, "\n#define WPO_PRECISE(T) T\n" );
						#endif
					}

					RemoveComment( GraphShaderStr, "//float3 WPO = PrepareAndGetWPO" );
					CommentLine( GraphShaderStr, "float3 WPO = float3( 0, 0, 0 );" );
				}
				else
				{
					CommentLine( GeneratedShader, "Offset = GetMaterialWorldPositionOffset" );
				}
				if( CustomizedUVs.length() > 0 )
				{
					RemoveComment( GeneratedShader, "//#define HAS_CUSTOMIZED_UVS" );
				}

				std::string ShaderFileNameANSI = ToANSIString( *ShaderFileName );

				if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP || CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
				{
					SaveFile( ShaderFileNameANSI.c_str(), (void*)GraphShaderStr.c_str(), GraphShaderStr.length() );
					std::string MatNameHLSL = MatNameStr;
					FString MatHLSLWide = GetOutputFile( TEXT("Shaders/" ), FString( ToWideString( MatNameHLSL ).c_str()), TEXT(".hlsl" ));

					SaveFile( ToANSIString( *MatHLSLWide).c_str(), (void*)GeneratedShader.c_str(), GeneratedShader.length() );
				}
				else
					SaveFile( ShaderFileNameANSI.c_str(), (void* )GeneratedShader.c_str(), GeneratedShader.length() );
				Mat->UnityMat->ShaderFileName = ShaderFileNameANSI;

				Mat->UnityMat->GenerateShaderGUID();
				std::string ShaderMetaContents = GenerateShaderMeta( Mat->UnityMat->ShaderGUID );
				std::string ShaderMetaFile = ShaderFileNameANSI;
				ShaderMetaFile += ".meta";
				SaveFile( ShaderMetaFile.c_str(), (void*)ShaderMetaContents.c_str(), ShaderMetaContents.length() );

				
				Mat->UnityMat->ShaderContents = GeneratedShader;
			}			
		}		
	}	
}

class MeshBinding
{
public:
	UnityMesh *TheUnityMesh = nullptr;
	UStaticMesh *UnrealStaticMesh = nullptr;
	USkeletalMesh* UnrealSkeletalMesh = nullptr;
	UPrimitiveComponent* MeshComp = nullptr;
	AActor* OwnerActor = nullptr;
	int LOD = 0;
	int TotalLods = -1;
};

std::vector< MeshBinding* > MeshList;


MeshBinding* GetMeshBinding( UnityMesh* Mesh )
{
	for( int i = 0; i < MeshList.size(); i++ )
	{
		if( MeshList[ i ]->TheUnityMesh == Mesh )
			return MeshList[ i ];
	}

	return nullptr;
}
const FStaticMeshRenderData* GetRenderData( const UStaticMesh* StaticMesh )
{
	if( !StaticMesh )
		return nullptr;
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		const FStaticMeshRenderData* MeshRenderData = StaticMesh->GetRenderData();
	#else
		const FStaticMeshRenderData* MeshRenderData = StaticMesh->RenderData.Get();
	#endif

	return MeshRenderData;
}
UBodySetup* GetBodySetup( const UStaticMesh* StaticMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	#else
		UBodySetup* BodySetup = StaticMesh->BodySetup;
	#endif

	return BodySetup;
}
const USkeleton* GetSkeleton( const USkeletalMesh* SkeletalMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		const USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	#else
		const USkeleton* Skeleton = SkeletalMesh->Skeleton;
	#endif

	return Skeleton;
}
TArray<UMorphTarget*> GetMorphTargets( const USkeletalMesh* SkeletalMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		TArray<UMorphTarget*> MorphTargets = SkeletalMesh->GetMorphTargets();
	#else
		TArray<UMorphTarget*> MorphTargets = SkeletalMesh->MorphTargets;
	#endif

	return MorphTargets;
}
const TArray<FSkeletalMaterial>& GetMaterials( const USkeletalMesh* SkeletalMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION >= 5
		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
	#else
		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->Materials;
	#endif

	return Materials;
}
bool RequiresNewMeshDueToVertexColors( UPrimitiveComponent* MeshComp, int LOD )
{
	auto StaticMeshComp = Cast< UStaticMeshComponent>( MeshComp );
	if (!StaticMeshComp )
		return false;
	if( CVarExportVertexColors.GetValueOnAnyThread() == 1 )
	{
		if( StaticMeshComp->LODData.Num() > LOD )
		{
			if( StaticMeshComp->LODData[ LOD ].OverrideVertexColors )
			{
				return true;
			}
		}
	}

	return false;
}
UnityMesh * GetUnityMesh( const UStaticMesh *UnrealStaticMesh, USkeletalMesh* SkeletalMesh,
						  const char *Name, int LOD, UPrimitiveComponent* MeshComp )
{
	//OverridenVertexColors creates new mesh all the time
	int OverrideIndex = 0;
	
	for( int i = 0; i < MeshList.size(); i++ )
	{
		MeshBinding* M = MeshList[ i ];
		bool HasSameLOD = ( M->LOD == LOD );
		if( CVarFBXPerActor.GetValueOnAnyThread() )
			HasSameLOD = true;
		bool SameComponent = true;
		if( MeshComp )
		{
			SameComponent = ( MeshComp == M->MeshComp );
		}
		bool SameMesh = true;
		if( SkeletalMesh )
		{
			SameMesh = ( M->UnrealSkeletalMesh == SkeletalMesh );
		}
		else
		{
			SameMesh = (M->UnrealStaticMesh == UnrealStaticMesh && M->TheUnityMesh->Name.compare( Name ) == 0);
		}
		if( SameMesh && HasSameLOD && SameComponent )
		{
			return M->TheUnityMesh;
		}
	}

	MeshBinding *NewMeshBinding = new MeshBinding;
	NewMeshBinding->TheUnityMesh = new UnityMesh;
	NewMeshBinding->TheUnityMesh->Name = Name;
	NewMeshBinding->LOD = LOD;
	NewMeshBinding->UnrealStaticMesh = ( UStaticMesh*)UnrealStaticMesh;
	NewMeshBinding->UnrealSkeletalMesh = SkeletalMesh;
	
	if( UnrealStaticMesh )
	{
		const FStaticMeshRenderData* MeshRenderData = GetRenderData( (UStaticMesh*)UnrealStaticMesh );
		NewMeshBinding->TotalLods = MeshRenderData->LODResources.Num();
	}
	else if( SkeletalMesh )
	{
		NewMeshBinding->TotalLods = SkeletalMesh->GetResourceForRendering()->LODRenderData.Num();
	}

	NewMeshBinding->MeshComp = MeshComp;

	//Generate Mesh File output
	FString MeshFile = GetUnityAssetsFolder();

	if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
	{
		if ( NewMeshBinding->UnrealStaticMesh )
			MeshFile += GetAssetPathFolder( NewMeshBinding->UnrealStaticMesh );
		else if( NewMeshBinding->UnrealSkeletalMesh )
			MeshFile += GetAssetPathFolder( NewMeshBinding->UnrealSkeletalMesh );
	}
	else
	{
		MeshFile += TEXT("Meshes/");
	}

	CreateAllDirectories( MeshFile );
	MeshFile += ToWideString( Name ).c_str();
	if( MeshComp && RequiresNewMeshDueToVertexColors( MeshComp, LOD ) )
	{
		char Text[ 32 ];
		snprintf( Text, sizeof( Text ), "@%p", MeshComp );
		MeshFile += ToWideString( Text ).c_str();
	}
	if ( CVarFBXPerActor.GetValueOnAnyThread() )
		MeshFile += TEXT( ".fbx");
	else
		MeshFile += TEXT( ".obj" );

	NewMeshBinding->TheUnityMesh->File = ToANSIString( *MeshFile );

	MeshList.push_back( NewMeshBinding );
	return NewMeshBinding->TheUnityMesh;
}

void ProcessSkeletalMesh( USkeletalMeshComponent *SkeletalMeshComponent )
{
	MaterialBinding **MaterialsUsed = nullptr;
	TArray<UMaterialInterface*> Materials = SkeletalMeshComponent->GetMaterials();

	MaterialsUsed = new MaterialBinding*[ Materials.Num()];
	for( int i = 0; i < Materials.Num(); i++ )
		MaterialsUsed[ i ] = nullptr;
	
	UObject* LoadedObject = StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, TEXT( "/Engine/EngineMaterials/DefaultMaterial" ) );
	UMaterialInterface* DefaultMaterial = Cast<UMaterialInterface>( LoadedObject );
	if( Materials.Num() > 0 )
	{
		for( int i = 0; i < Materials.Num(); i++ )
		{
			MaterialsUsed[ i ] = nullptr;
			UMaterialInterface *MaterialInterface = Materials[ i ];
			if( !MaterialInterface )
			{
				Materials[ i ] = DefaultMaterial;
			}

			MaterialsUsed[ i ] = ProcessMaterialReference( MaterialInterface );
		}
	}
}
void MoveComponentsToNewGO( GameObject* GO )
{
	GameObject* NewGameObject = new GameObject;
	
	for( int i = 0; i < GO->Components.size(); i++ )
	{
		UnityComponent* Comp = GO->Components[ i ];
		
		{
			NewGameObject->Components.push_back( Comp );
			Comp->Owner = NewGameObject;

			GO->Components.erase( GO->Components.begin() + i );
			i--;
		}
	}
	
	{
		MeshRenderer* MR = ( MeshRenderer * )NewGameObject->GetComponent( CT_MESHRENDERER );
		if ( MR )
			NewGameObject->Name = MR->Name;

		GO->AddChild( NewGameObject );
		GO->OwnerScene->Add( NewGameObject );
	}	
}
MaterialBinding** GetMaterialsUsed( UStaticMeshComponent* StaticMeshComp, USkeletalMeshComponent* SkeletalMeshComponent,
									bool GetComponentMaterials, int& NumMaterials );
bool BaseMeshHasDuplicateMaterials( MaterialBinding** MeshMaterials, int MaxMeshMaterials )
{
	for( int i = 0; i < MaxMeshMaterials - 1; i++ )
		for( int u = i + 1; u < MaxMeshMaterials; u++ )
		{
			if( MeshMaterials[ i ]->UnityMat == MeshMaterials[ u ]->UnityMat )
			{
				return true;
			}
		}

	return false;
}
Renderer* ExportLOD( UStaticMeshComponent* StaticMeshComp, USkeletalMeshComponent* SkeletalMeshComp,
					 GameObject* ComponentGO, AActor* Actor, int LOD, int TotalLODs )
{
	const UStaticMesh* StaticMesh = nullptr;
	const FStaticMeshLODResources* LODResources = nullptr;
	if( StaticMeshComp )
	{
		StaticMesh = StaticMeshComp->GetStaticMesh();
		LODResources = &GetRenderData( StaticMesh )->LODResources[ LOD ];
	}
	USkeletalMesh* SkeletalMesh = nullptr;
	FSkeletalMeshLODRenderData* SkeletalLODRenderData = nullptr;
	if( SkeletalMeshComp )
	{
		SkeletalMesh = SkeletalMeshComp->SkeletalMesh;
		SkeletalLODRenderData = &SkeletalMesh->GetResourceForRendering()->LODRenderData[LOD];
	}

	FString OriginalMeshName;
	FString SourceFile;
	if( StaticMeshComp )
	{
		OriginalMeshName = StaticMesh->GetName();
		SourceFile = StaticMesh->GetName();
	}
	else if( SkeletalMeshComp )
	{
		OriginalMeshName = SkeletalMesh->GetName();
		SourceFile = SkeletalMesh->GetName();
	}

	if( SourceFile.Len() == 0 )
	{
		SourceFile = Actor->GetName();
	}

	if( SourceFile.Len() > 0 && LOD > 0 && !CVarFBXPerActor.GetValueOnAnyThread() )
	{
		SourceFile = FString::Printf( TEXT( "%s_LOD%d" ), *SourceFile, LOD );
	}

	std::string Name;
	Name = ToANSIString( *SourceFile );

	UStaticMeshComponent* CompareComp = nullptr;

	if( RequiresNewMeshDueToVertexColors( StaticMeshComp, LOD ) )
	{
		ComponentGO->CanBePrefab = false;
		if( ComponentGO->Parent )
			ComponentGO->Parent->CanBePrefab = false;
		CompareComp = StaticMeshComp;
	}

	UnityMesh* NewMesh = GetUnityMesh( StaticMesh, SkeletalMesh, Name.c_str(), LOD, CompareComp);
	MeshBinding* Binding = GetMeshBinding( NewMesh );
	if( Binding )
	{
		if ( StaticMeshComp )
			Binding->OwnerActor = StaticMeshComp->GetOwner();
		else if ( SkeletalMeshComp )
			Binding->OwnerActor = SkeletalMeshComp->GetOwner();
	}

	int MaxMeshMaterials = 0;
	MaterialBinding** MeshMaterials = GetMaterialsUsed( StaticMeshComp, SkeletalMeshComp, false, MaxMeshMaterials );

	if( NewMesh->Sections.size() == 0 )
	{
		if( LODResources )
		{
			for( int i = 0; i < LODResources->Sections.Num(); i++ )
			{
				FStaticMeshSection Section = LODResources->Sections[ i ];
				MeshSection* NewMeshSection = new MeshSection;
				NewMeshSection->MaterialIndex = Section.MaterialIndex;
				NewMeshSection->IndexOffset = Section.FirstIndex;
				NewMeshSection->NumIndices = Section.NumTriangles * 3;

				NewMesh->Sections.push_back( NewMeshSection );
			}
		}
		else if( SkeletalLODRenderData )
		{
			for( int i = 0; i < SkeletalLODRenderData->RenderSections.Num(); i++ )
			{
				FSkelMeshRenderSection* Section = &SkeletalLODRenderData->RenderSections[ i ];
				MeshSection* NewMeshSection = new MeshSection;
				NewMeshSection->MaterialIndex = Section->MaterialIndex;
				NewMeshSection->IndexOffset = Section->BaseIndex;
				NewMeshSection->NumIndices = Section->NumTriangles * 3;

				NewMesh->Sections.push_back( NewMeshSection );
			}
		}
		
		if( CVarFBXPerActor.GetValueOnAnyThread() == 0 )
		{
			int NumVertices = LODResources->VertexBuffers.PositionVertexBuffer.GetNumVertices();
			int NumTexcoordChannels = LODResources->VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
			int NumIndices = LODResources->IndexBuffer.GetNumIndices();

			uint32* Indices = new uint32[ NumIndices ];
			FVector* Vertices = new FVector[ NumVertices ];
			FVector* Normals = new FVector[ NumVertices ];
			FVector4* Tangents = new FVector4[ NumVertices ];
			FVector2D* Texcoords = new FVector2D[ NumVertices ];
			FVector2D* Texcoords1 = nullptr;
			if( NumTexcoordChannels > 1 )
				Texcoords1 = new FVector2D[ NumVertices ];
			FColor* Colors = nullptr;

			if( NumIndices > 0 )
			{
				FIndexArrayView ArrayView = LODResources->IndexBuffer.GetArrayView();

				for( int i = 0; i < NumIndices; i++ )
				{
					uint32 Index32 = ArrayView[ i ];
					Indices[ i ] = Index32;

					if( LODResources->IndexBuffer.Is32Bit() )
					{

					}
				}

				TotalTriangles += NumIndices / 3;
			}
			else
				TotalTriangles += NumVertices / 3;

			for( int i = 0; i < LODResources->Sections.Num(); i++ )
			{
				FStaticMeshSection Section = LODResources->Sections[ i ];
				int MatIndex = Section.MaterialIndex;
				int VertexStart = Section.MinVertexIndex;
				int VertexEnd = Section.MaxVertexIndex;
				int IndexStart = Section.FirstIndex;
				int NumTriangles = Section.NumTriangles;
			}

			TArray<FColor> ColorArray;
			TArray<FPaintedVertex> PaintedVertices;

			if( CVarExportVertexColors.GetValueOnAnyThread() == 1 )
			{
				if( StaticMeshComp->LODData.Num() > LOD )
				{
					if( StaticMeshComp->LODData[ LOD ].OverrideVertexColors )
					{
						StaticMeshComp->LODData[ LOD ].OverrideVertexColors->GetVertexColors( ColorArray );
					}
				}

				if( ColorArray.Num() == 0 )
				{
					LODResources->VertexBuffers.ColorVertexBuffer.GetVertexColors( ColorArray );
				}
				if( ColorArray.Num() > 0 && ColorArray.Num() == NumVertices )
				{
					Colors = new FColor[ ColorArray.Num() ];
				}
			}
			for( int i = 0; i < NumVertices; i++ )
			{
				FVector Pos = (FVector)LODResources->VertexBuffers.PositionVertexBuffer.VertexPosition( i );
				//Scale down from Unreal's cm units to Unity's m
				Pos /= 100.0f;

				FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 0, 0 ) );

				if( CVarAssetsWithYUp.GetValueOnAnyThread() == 1 )
				{
					Pos = Rotator.RotateVector( Pos );
				}
				if( CVarUsePostImportTool.GetValueOnAnyThread() == 0 )
				{
					Pos.X *= -1;
				}

				Vertices[ i ] = Pos;

				//#define USE_OBJ_EXPORT
				#ifdef USE_OBJ_EXPORT
				FVector4 TangentX = LODResources->VertexBuffers.StaticMeshVertexBuffer.VertexTangentX( i );//tangent
				FVector4 TangentY = LODResources->VertexBuffers.StaticMeshVertexBuffer.VertexTangentY( i );//tangent
				FVector4 TangentZ = LODResources->VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( i );//Normal

				if( CVarAssetsWithYUp.GetValueOnAnyThread() == 1 )
				{
					TangentZ = Rotator.RotateVector( TangentZ );
					TangentY = Rotator.RotateVector( TangentY );
				}

				Normals[ i ] = TangentZ;
				Tangents[ i ] = TangentY;

				FVector2D UV = LODResources->VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( i, 0 );
				Texcoords[ i ] = FVector2D( UV.X, 1.0 - UV.Y );

				if( NumTexcoordChannels > 1 )
				{
					FVector2D UV1 = LODResources->VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( i, 1 );
					Texcoords1[ i ] = FVector2D( UV1.X, 1.0 - UV1.Y );
				}

				if( Colors )
				{
					Colors[ i ] = ColorArray[ i ];
				}
				#endif
			}

			NewMesh->Vertices = Vertices;
			NewMesh->Normals = Normals;
			NewMesh->Tangents = Tangents;
			NewMesh->Texcoords = Texcoords;
			NewMesh->Texcoords1 = Texcoords1;
			NewMesh->Colors = Colors;
			NewMesh->AllIndices = Indices;
			NewMesh->NumVertices = NumVertices;
			NewMesh->NumIndices = NumIndices;
		}

		for( int i = 0; i < MaxMeshMaterials; i++ )
		{
			if( MeshMaterials[ i ] )
				NewMesh->Materials.push_back( MeshMaterials[ i ]->UnityMat );
			else
				NewMesh->Materials.push_back( nullptr );
		}
	}
	
	FString  LodNameStr = FString::Printf( TEXT( "%s_LOD%d" ), *OriginalMeshName, LOD );
	std::string LodName = ToANSIString( *LodNameStr );

	MeshRenderer* MR = nullptr;
	MeshFilter* MF = nullptr;
	SkinnedMeshRenderer* SMR = nullptr;

	GameObject* LODGO = new GameObject;
	ComponentGO->AddChild( LODGO );
	ComponentGO->OwnerScene->Add( LODGO );
	
	if( StaticMeshComp )
	{
		MR = (MeshRenderer*)LODGO->AddComponent( CT_MESHRENDERER );
		MF = (MeshFilter*)LODGO->AddComponent( CT_MESHFILTER );
	}
	else if ( SkeletalMeshComp )
		SMR = (SkinnedMeshRenderer*)LODGO->AddComponent( CT_SKINNEDMESHRENDERER );	

	LODGO->Name = LodName;

	if( MF )
		MF->Mesh = NewMesh;
	else if( SMR )
		SMR->Mesh = NewMesh;

	//Only FBX has different FileID for every LOD
	if( CVarFBXPerActor.GetValueOnAnyThread() == 1 )
	{
		if( MF )
		{
			if( TotalLODs > 1 )
				MF->SubMeshID = 4300000 + LOD * 2;
			else
				MF->SubMeshID = 4300002;
		}
		else if( SMR )
		{
			if( TotalLODs > 1 )
				SMR->SubMeshID = 4300000 + LOD * 2;
			else
				SMR->SubMeshID = 4300002;
		}
	}

	if( MR )
	{
		MR->Name = LodName;
		if( StaticMeshComp )
			MR->CastShadows = StaticMeshComp->CastShadow;
	}
	else if( SMR )
	{
		if( SkeletalMeshComp )
			SMR->CastShadows = SkeletalMeshComp->CastShadow;
	}

	int MaxMaterials = 0;
	MaterialBinding** MaterialsUsed = GetMaterialsUsed( StaticMeshComp, SkeletalMeshComp, true, MaxMaterials );
	//bool HasDuplicateMaterials = BaseMeshHasDuplicateMaterials( MeshMaterials, MaxMeshMaterials );
	//Add materials based on that section's MaterialIndex
	if( LODResources )
	{
		for( int i = 0; i < LODResources->Sections.Num(); i++ )
		{
			FStaticMeshSection Section = LODResources->Sections[ i ];
			if( Section.MaterialIndex >= 0 && Section.MaterialIndex < MaxMaterials )
			{
				MaterialBinding* MatBinding = MaterialsUsed[ Section.MaterialIndex ];
				if( MatBinding )
				{
					MR->AddMaterial( MatBinding->UnityMat, false );// HasDuplicateMaterials );
				}
				else
					MR->AddMaterial( nullptr, false );// HasDuplicateMaterials );
			}
		}
	}
	else if ( SkeletalLODRenderData )
	{
		for( int i = 0; i < SkeletalLODRenderData->RenderSections.Num(); i++ )
		{
			FSkelMeshRenderSection* Section = &SkeletalLODRenderData->RenderSections[ i ];
			if( Section->MaterialIndex >= 0 && Section->MaterialIndex < MaxMaterials )
			{
				MaterialBinding* MatBinding = MaterialsUsed[ Section->MaterialIndex ];
				if( MatBinding )
				{
					SMR->AddMaterial( MatBinding->UnityMat, false );// HasDuplicateMaterials );
				}
				else
					SMR->AddMaterial( nullptr, false );// HasDuplicateMaterials );
			}
		}
	}

	if( MR )
		return MR;
	else// if( SMR )
		return SMR;
}
int GetNumRelevantComponents( AActor* A )
{
	TArray<UActorComponent*> comps;
	A->GetComponents( comps );
	int Num = 0;
	for( int i = 0; i < comps.Num(); i++ )
	{
		UActorComponent* AC = comps[ i ];
		ULightComponent* LightComp = dynamic_cast<ULightComponent*>( AC );
		if( LightComp )
		{
			Num++;
		}
		USkeletalMeshComponent* SkelMeshComp = dynamic_cast<USkeletalMeshComponent*>( AC );
		if( SkelMeshComp )
		{
			Num++;
		}
		USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( AC );
		if( SplineMeshComponent )
		{
			
		}
		UStaticMeshComponent* StaticMeshComp = dynamic_cast<UStaticMeshComponent*>( AC );
		if( StaticMeshComp )
		{
			Num++;
		}		
		USphereReflectionCaptureComponent* SphereReflection = Cast<USphereReflectionCaptureComponent>( AC );
		if( SphereReflection )
		{
			Num++;
		}
		UBoxReflectionCaptureComponent* BoxReflection = Cast<UBoxReflectionCaptureComponent>( AC );
		if( BoxReflection )
		{
			Num++;
		}
		UDecalComponent* DecalComponent = Cast<UDecalComponent>( AC );
		if( DecalComponent )
		{			
			Num++;
		}
	}

	return Num;
}
void RotateForYUp( FVector& Vector )
{
	FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 0, 0 ) );	
	Vector = Rotator.RotateVector( Vector );	
}
void SwitchYWithZ( FVector& Vector )
{
	float Temp = Vector.Y;
	Vector.Y = Vector.Z;
	Vector.Z = Temp;
}
void ConvertScaleForUnity( FVector& Vector )
{
	float Temp = Vector.Y;
	Vector.Y = Vector.Z;
	Vector.Z = Temp;

	Temp = Vector.Z;
	Vector.Z = Vector.X;
	Vector.X = Temp;
}
//Figure out if component or base mesh has unique materials since we need the unique materials variant because unity merges meshes
TArray<UMaterialInterface*> GetMaterialsToBeUsedInFBX( UStaticMeshComponent* StaticMeshComp )
{
	TArray<UMaterialInterface*> Materials = StaticMeshComp->GetMaterials();
	TArray<FStaticMaterial> StaticMaterials = GetStaticMaterials( StaticMeshComp->GetStaticMesh() );
	bool ComponentHasDuplicates = false;
	bool BaseMeshHasDuplicates = false;
	for( int i = 0; i < Materials.Num() - 1; i++ )
	{
		for( int u = i + 1; u < Materials.Num(); u++ )
		{
			if( Materials[ i ] == Materials[ u ] )
				ComponentHasDuplicates = true;
		}
	}
	for( int i = 0; i < StaticMaterials.Num() - 1; i++ )
	{
		for( int u = i + 1; u < StaticMaterials.Num(); u++ )
		{
			if( StaticMaterials[ i ].MaterialInterface == StaticMaterials[ u ].MaterialInterface )
				BaseMeshHasDuplicates = true;
		}
	}
	
	TArray<UMaterialInterface*> OutMaterials;
	int NumMaterials = StaticMeshComp->GetNumMaterials();
	
	for( int i = 0; i < NumMaterials; i++ )
	{
		UMaterialInterface* Mat = nullptr;
		if( StaticMeshComp )
		{
			if( !ComponentHasDuplicates )
				Mat = StaticMeshComp->GetMaterial( i );
			else
				Mat = StaticMeshComp->GetStaticMesh()->GetMaterial( i );
		}
		OutMaterials.Add( Mat );
	}

	return OutMaterials;
}
MaterialBinding** GetMaterialsUsed( UStaticMeshComponent* StaticMeshComp, USkeletalMeshComponent* SkeletalMeshComponent,
									bool GetComponentMaterials, int & NumMaterials )
{
	TArray<UMaterialInterface*> OutMaterials;
	
	if( StaticMeshComp )
		NumMaterials = StaticMeshComp->GetNumMaterials();
	if( SkeletalMeshComponent )
		NumMaterials = SkeletalMeshComponent->GetNumMaterials();
	for( int i = 0; i < NumMaterials; i++ )
	{
		UMaterialInterface* Mat = nullptr;
		if( StaticMeshComp )
		{
			if ( GetComponentMaterials )
				Mat = StaticMeshComp->GetMaterial( i );
			else
				Mat = StaticMeshComp->GetStaticMesh()->GetMaterial( i );
		}
		if( SkeletalMeshComponent )
		{
			if ( GetComponentMaterials )
				Mat = SkeletalMeshComponent->GetMaterial( i );
			else
				Mat = GetMaterials( SkeletalMeshComponent->SkeletalMesh )[i].MaterialInterface;
		}
		OutMaterials.Add( Mat );
	}
	NumMaterials = OutMaterials.Num();

	MaterialBinding** MaterialsUsed = nullptr;
	MaterialsUsed = new MaterialBinding * [ NumMaterials ];
	for( int i = 0; i < NumMaterials; i++ )
		MaterialsUsed[ i ] = nullptr;

	UObject* LoadedObject = StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, TEXT( "/Engine/EngineMaterials/DefaultMaterial" ) );
	UMaterialInterface* DefaultMaterial = Cast<UMaterialInterface>( LoadedObject );
	if( OutMaterials.Num() > 0 )
	{
		for( int i = 0; i < OutMaterials.Num(); i++ )
		{
			MaterialsUsed[ i ] = nullptr;
			if( !OutMaterials[ i ] )
			{
				OutMaterials[ i ] = DefaultMaterial;
			}
			MaterialsUsed[ i ] = ProcessMaterialReference( OutMaterials[ i ] );
		}
	}

	return MaterialsUsed;
}
void ProcessMeshComponent( AActor* Actor, UStaticMeshComponent* StaticMeshComp, USkeletalMeshComponent* SkeletalMeshComponent, GameObject* ActorGO )
{
	const UStaticMesh* StaticMesh = nullptr;
	const USkeletalMesh* SkeletalMesh = nullptr;
	bool IsVisible = false;
	if( StaticMeshComp )
	{
		StaticMesh = StaticMeshComp->GetStaticMesh();
		//ShouldRender is more specific and includes visible layers, export what's actually shown in the editor
		IsVisible = StaticMeshComp->ShouldRender();//IsVisible()
	}
	if( SkeletalMeshComponent )
	{
		SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
		IsVisible = SkeletalMeshComponent->ShouldRender();
	}

	if( (StaticMeshComp && !StaticMesh ) || (SkeletalMeshComponent && !SkeletalMesh) || !IsVisible )
		return;

	if( !ExportMeshes )
		return;

	GameObject* ComponentGO = nullptr;
	int RelevantComponents = GetNumRelevantComponents( Actor );
	if( RelevantComponents <= 1 )
	{
		ComponentGO = ActorGO;
		ActorGO->CanBePrefab = true;
	}
	else
	{
		ComponentGO = new GameObject;
		ActorGO->AddChild( ComponentGO );
		ActorGO->OwnerScene->Add( ComponentGO );
	}
	FString CompNameStr = "";
	if( StaticMeshComp )
		CompNameStr = StaticMeshComp->GetName();
	if( SkeletalMeshComponent )
		CompNameStr = SkeletalMeshComponent->GetName();

	std::string CompName = ToANSIString( *CompNameStr );
	if( RelevantComponents > 1 )
		ComponentGO->Name = CompName;

	TransformComponent* ActorTransformComp = (TransformComponent*)ActorGO->GetComponent( CT_TRANSFORM );
	ActorTransformComp->Reset();

	TransformComponent* ComponentTransform = (TransformComponent*)ComponentGO->GetComponent( CT_TRANSFORM );

	FTransform ComponentTrans;
	if ( StaticMeshComp )
		ComponentTrans = StaticMeshComp->GetComponentTransform();
	if( SkeletalMeshComponent )
		ComponentTrans = SkeletalMeshComponent->GetComponentTransform();
	ConvertTransform( ComponentTrans, ComponentTransform, false );// true );

	const FStaticMeshRenderData* MeshRenderData = nullptr;
	FSkeletalMeshRenderData* SkeletalMeshRenderData = nullptr;
	int NumLods = 0;
	if( StaticMesh )
	{
		MeshRenderData = GetRenderData( (UStaticMesh*)StaticMesh );
		NumLods = MeshRenderData->LODResources.Num();
	}
	if( SkeletalMesh )
	{
		SkeletalMeshRenderData = SkeletalMesh->GetResourceForRendering();
		NumLods = SkeletalMeshRenderData->LODRenderData.Num();
	}

	LodGroup* ComponentLodGroup = nullptr;
	if( CVarExportLODs.GetValueOnAnyThread() == 1 && NumLods > 1 && ( MeshRenderData  || SkeletalMeshRenderData ) )
	{
		ComponentLodGroup = (LodGroup*)ComponentGO->AddComponent( CT_LODGROUP );
	}

	UBodySetup* BodySetup = nullptr;
	if ( StaticMesh )
		BodySetup = GetBodySetup( StaticMesh );

	MeshCollider* NewMeshCollider = nullptr;
	if( BodySetup )
	{
		int ConvexHulls = BodySetup->AggGeom.ConvexElems.Num();
		int Spheres = BodySetup->AggGeom.SphereElems.Num();
		int Boxes = BodySetup->AggGeom.BoxElems.Num();
		int Capsules = BodySetup->AggGeom.SphylElems.Num();
		int TaperedCapsules = BodySetup->AggGeom.TaperedCapsuleElems.Num();
		int TotalShapes = ConvexHulls + Spheres + Boxes + Capsules + TaperedCapsules;

		if( TotalShapes > 0 )
		{
			FRotator RotatorForUnity = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );
			char Text[ 128 ];
			for( int i = 0; i < Boxes; i++ )
			{
				FKBoxElem& Box = BodySetup->AggGeom.BoxElems[ i ];

				GameObject* PrimitiveGO = ComponentGO;
				FVector Euler = Box.Rotation.Euler();
				bool NeedsTransform = false;

				FVector Center = Box.Center / 100.0f;

				Center = RotatorForUnity.RotateVector( Center );
				FVector SizeNoTransform = FVector( Box.X / 100.0f, Box.Y / 100.0f, Box.Z / 100.0f );
				SizeNoTransform = RotatorForUnity.RotateVector( SizeNoTransform );

				if( !Euler.IsNearlyZero() )
				{
					PrimitiveGO = new GameObject();
					ComponentGO->AddChild( PrimitiveGO );
					snprintf( Text, sizeof( Text ), "Box%d", i );
					PrimitiveGO->Name = Text;
					TransformComponent* TC = (TransformComponent*)PrimitiveGO->GetComponent( CT_TRANSFORM );

					FVector DefaultRotation = RotatorForUnity.Euler();
					FVector BoxEuler = Box.Rotation.Euler();
					FVector FinalEuler = DefaultRotation + FVector( BoxEuler.X, -BoxEuler.Z, BoxEuler.Y );
					auto FinalRotation = FRotator::MakeFromEuler( FinalEuler );

					TC->Set( Center, FinalRotation.Quaternion(), FVector( 1, 1, 1 ) );

					NeedsTransform = true;
				}

				BoxCollider* NewBoxCollider = (BoxCollider*)PrimitiveGO->AddComponent( CT_BOXCOLLIDER );
				NewBoxCollider->Size = FVector( Box.X / 100.0f, Box.Y / 100.0f, Box.Z / 100.0f );
				if( !NeedsTransform )
				{
					NewBoxCollider->Center = Center;
					ConvertScaleForUnity( NewBoxCollider->Size );
				}
				else
				{
					RotateForYUp( NewBoxCollider->Size );
					SwitchYWithZ( NewBoxCollider->Size );
				}
			}
			for( int i = 0; i < Spheres; i++ )
			{
				FKSphereElem& Sphere = BodySetup->AggGeom.SphereElems[ i ];

				SphereCollider* NewSphereCollider = (SphereCollider*)ComponentGO->AddComponent( CT_SPHERECOLLIDER );

				FVector Center = Sphere.Center / 100;
				Center = RotatorForUnity.RotateVector( Center );
				NewSphereCollider->Center = Center;
				NewSphereCollider->Radius = Sphere.Radius / 100.0f;
				//RotateForYUp( NewSphereCollider->Center );
			}
			for( int i = 0; i < Capsules; i++ )
			{
				FKSphylElem& Capsule = BodySetup->AggGeom.SphylElems[ i ];

				bool NeedsTransform = false;
				GameObject* PrimitiveGO = ComponentGO;
				FVector Center = Capsule.Center / 100.0f;
				FVector Euler = Capsule.Rotation.Euler();
				Center = RotatorForUnity.RotateVector( Center );
				if( !Euler.IsNearlyZero() )
				{
					PrimitiveGO = new GameObject();
					ComponentGO->AddChild( PrimitiveGO );
					snprintf( Text, sizeof( Text ), "Capsule%d", i );
					PrimitiveGO->Name = Text;
					TransformComponent* TC = (TransformComponent*)PrimitiveGO->GetComponent( CT_TRANSFORM );

					//FVector DefaultRotation = RotatorForUnity.Euler();
					FVector CapsuleEuler = Capsule.Rotation.Euler();
					FVector FinalEuler = FVector( CapsuleEuler.Z, CapsuleEuler.Y, -CapsuleEuler.X );// +FVector( 0, 270, 0 );
					//FinalEuler = FVector( FinalEuler.X, FinalEuler.Z, FinalEuler.Y );
					auto FinalRotation = FRotator::MakeFromEuler( FinalEuler );

					TC->Set( Center, FinalRotation.Quaternion(), FVector( 1, 1, 1 ) );

					NeedsTransform = true;
				}

				CapsuleCollider* NewCapsuleCollider = (CapsuleCollider*)PrimitiveGO->AddComponent( CT_CAPSULECOLLIDER );

				NewCapsuleCollider->Radius = Capsule.Radius / 100.0f;
				//Unity starts height at 0, Unreal starts with height=radius
				NewCapsuleCollider->Height = Capsule.Length / 100.0f + NewCapsuleCollider->Radius;
				NewCapsuleCollider->Direction = 1;

				if( !NeedsTransform )
				{
					NewCapsuleCollider->Center = Capsule.Center / 100.0f;
					RotateForYUp( NewCapsuleCollider->Center );
				}
			}
			if( ConvexHulls > 0 )
			{
				NewMeshCollider = (MeshCollider*)ComponentGO->AddComponent( CT_MESHCOLLIDER );
			}
		}
	}

	if( (MeshRenderData || SkeletalMeshRenderData ) && NumLods > 0 )
	{
		for( int i = 0; i < NumLods; i++ )
		{
			Renderer* LODMeshRenderer = ExportLOD( StaticMeshComp, SkeletalMeshComponent, ComponentGO, Actor, i, NumLods );
			if( ComponentLodGroup )
			{
				LOD* NewLOD = new LOD;
				if( i == NumLods - 1 )
					NewLOD->screenRelativeTransitionHeight = 0.01f;
				else
				{
					float ScreenSize = 1.0;
					if ( MeshRenderData && i + 1 < MAX_STATIC_MESH_LODS )//Fix for a reported crash
						ScreenSize = MeshRenderData->ScreenSize[ i + 1 ].Default;
					NewLOD->screenRelativeTransitionHeight = FMath::Min( ScreenSize, 1.0f );
				}
				NewLOD->renderers.push_back( LODMeshRenderer );
				ComponentLodGroup->Lods.push_back( NewLOD );
				if( StaticMesh )
				{
					ComponentLodGroup->Size = StaticMesh->GetBounds().SphereRadius / 100.0f * 2;
					ComponentLodGroup->LocalReferencePoint = StaticMesh->GetBounds().Origin / 100.0f;
				}
				else if ( SkeletalMesh )
				{
					ComponentLodGroup->Size = SkeletalMesh->GetBounds().SphereRadius / 100.0f * 2;
					ComponentLodGroup->LocalReferencePoint = SkeletalMesh->GetBounds().Origin / 100.0f;
				}

				FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );//Rotate for Y Up
				ComponentLodGroup->LocalReferencePoint = Rotator.RotateVector( ComponentLodGroup->LocalReferencePoint );
			}
			if( NewMeshCollider && !NewMeshCollider->Mesh )
			{
				auto MF = (MeshFilter*)LODMeshRenderer->Owner->GetComponent( CT_MESHFILTER );
				if( MF && MF->Mesh )
				{					
					NewMeshCollider->Mesh = MF->Mesh;
					//It's always the last mesh
					if( NumLods > 1 )
						NewMeshCollider->SubMeshID = 4300000 + NumLods * 2;
					else
						NewMeshCollider->SubMeshID = 4300002 + NumLods * 2;

					MF->Mesh->HasConvexHulls = true;
					MF->Mesh->ConvexHullMeshID = NewMeshCollider->SubMeshID;
				}
			}

			if( CVarExportLODs.GetValueOnAnyThread() == 0 )
				break;
		}
		
	}
}
UnityLightType GetUnityLightComponentType( ELightComponentType Type )
{
	switch( Type )
	{
		case LightType_Directional:return LT_DIRECTIONAL;
		default:
		case LightType_Point:return LT_POINT;
		case LightType_Spot: return LT_SPOT;
		case LightType_Rect: return LT_AREA;
	}
}
float ConvertSpotAngle( float OuterConeAngle )
{
	float MaxUEAngle = 80;
	float MaxUnityAngle = 179;

	float AngleRatio = OuterConeAngle / MaxUEAngle;
	return AngleRatio * MaxUnityAngle;
}
void ConvertTransform( FTransform Trans, TransformComponent* Transform, bool IsRelative, bool HasDecals )
{
	FVector OriginalTranslation = Trans.GetTranslation();
	OriginalTranslation /= 100.0f;

	FVector Translation = OriginalTranslation;

	FQuat Quat = Trans.GetRotation();
	if( CVarFBXPerActor.GetValueOnAnyThread() == 1 )
	{
		FRotator Rotator = FRotator::MakeFromEuler( FVector( -90, 0, 90 ) );//fixes FBX preview angle
		Quat = Quat * Rotator.Quaternion();

		if( HasDecals )
		{
			FRotator DecalRotator = FRotator::MakeFromEuler( FVector( 0, 0, 90 ) );
			Quat = Quat * DecalRotator.Quaternion();
		}
	}
	else if( CVarAssetsWithYUp.GetValueOnAnyThread() == 1 )
	{
		FRotator Rotator = FRotator::MakeFromEuler( FVector( -90, 0, 0 ) );//works with OBJ
		Quat = Quat * Rotator.Quaternion();
	}
	FVector Scale = Trans.GetScale3D();
	FVector Euler = Quat.Euler();
	FVector EulerAfter = Euler;

	if( CVarFBXPerActor.GetValueOnAnyThread() == 1 )
	{
		float Temp = Scale.Y;
		Scale.Y = Scale.Z;
		Scale.Z = Temp;

		Temp = Scale.Z;
		Scale.Z = Scale.X;
		Scale.X = Temp;

		if( HasDecals )
		{
			Temp = Scale.X;
			Scale.X = Scale.Y;
			Scale.Y = Temp;
		}
	}
	else if( CVarAssetsWithYUp.GetValueOnAnyThread() == 1 )
	{
		float Temp = Scale.Y;
		Scale.Y = Scale.Z;
		Scale.Z = Temp;
	}
	

	Transform->LocalPosition[ 0 ] = Translation.X;
	Transform->LocalPosition[ 1 ] = Translation.Y;
	Transform->LocalPosition[ 2 ] = Translation.Z;

	Transform->LocalRotation[ 0 ] = Quat.X;
	Transform->LocalRotation[ 1 ] = Quat.Y;
	Transform->LocalRotation[ 2 ] = Quat.Z;
	Transform->LocalRotation[ 3 ] = Quat.W;

	Transform->LocalScale[ 0 ] = Scale.X;
	Transform->LocalScale[ 1 ] = Scale.Y;
	Transform->LocalScale[ 2 ] = Scale.Z;

	if( IsRelative )
	{
		Transform->LocalPosition[ 0 ] *= -1;
	}
}
FVector SplineRotation( 90, 270, 0 );
float ClampTo1( float Val )
{
	if( Val > 1 )
		Val = 1.0f;
	
	return Val;
}
bool ActorHasDecals( AActor* A )
{
	TArray<UActorComponent*> comps;
	A->GetComponents( comps );
	for( int i = 0; i < comps.Num(); i++ )
	{
		UActorComponent* AC = comps[ i ];
		UDecalComponent* Decal = Cast<UDecalComponent>( AC );
		if( Decal )
		{
			return true;
		}
	}

	return false;
}
void ProcessActor( AActor *A, Scene *S)
{
	if ( !A )
		return;	

	TArray<UActorComponent*> comps;
	A->GetComponents( comps );

	FTransform Trans = A->GetTransform();	

	std::string ActorName = ToANSIString( *A->GetName() );
	
	GameObject *GO = new GameObject();
	S->Add( GO );

	GO->Name = ActorName;
	
	FQuat Quat = Trans.GetRotation();
	TransformComponent* Transform = (TransformComponent*)GO->GetComponent( CT_TRANSFORM );

	bool HasDecals = ActorHasDecals( A );
	ConvertTransform( Trans, Transform, false, HasDecals );
	for ( int i = 0; i < comps.Num(); i++ )
	{
		UActorComponent *AC = comps[ i ];
		ULightComponent *LightComp = dynamic_cast<ULightComponent*>( AC );
		if ( LightComp )
		{
			LightComponent* NewLightComponent = new LightComponent;
			TransformComponent* LightTransform = Transform;
			if( comps.Num() > 1 )
			{
				Transform->Reset();
				GameObject* ChildGO = new GameObject;
				ChildGO->Name = ToANSIString( *LightComp->GetName() );
				ChildGO->Components.push_back( NewLightComponent );
				NewLightComponent->Owner = ChildGO;
				LightTransform = (TransformComponent*)ChildGO->GetComponent( CT_TRANSFORM );
				
				FTransform ComponentTrans = LightComp->GetComponentTransform();
				ConvertTransform( ComponentTrans, LightTransform, false );

				GO->AddChild( ChildGO );
				GO->OwnerScene->Add( ChildGO );
			}
			else
			{
				NewLightComponent->Owner = GO;
				GO->Components.push_back( NewLightComponent );
			}

			FQuat FixQuat( FVector( 0, 1, 0 ), PI / 2 );
			FQuat FinalQuat = Quat * FixQuat;
			LightTransform->LocalRotation[ 0 ] = FinalQuat.X;
			LightTransform->LocalRotation[ 1 ] = FinalQuat.Y;
			LightTransform->LocalRotation[ 2 ] = FinalQuat.Z;
			LightTransform->LocalRotation[ 3 ] = FinalQuat.W;

			ELightComponentType Type = LightComp->GetLightType();
			if( Type == LightType_Directional )
			{
				UDirectionalLightComponent* DirectionalLightComp = Cast< UDirectionalLightComponent>( LightComp );

				float Brightness = DirectionalLightComp->ComputeLightBrightness();
				NewLightComponent->Intensity = ClampTo1( DirectionalLightComp->Intensity );// Brightness / PI;
			}
			else if( Type == LightType_Point )
			{
				UPointLightComponent* PointLightComp = Cast< UPointLightComponent>( LightComp );
				NewLightComponent->Range = PointLightComp->AttenuationRadius / 100.0f;
				
				if( PointLightComp->IntensityUnits != ELightUnits::Candelas )
				{
					float LastLightBrigtness = PointLightComp->ComputeLightBrightness();
					PointLightComp->IntensityUnits = ELightUnits::Candelas;
					PointLightComp->SetLightBrightness( LastLightBrigtness );
				}

				NewLightComponent->Intensity = PointLightComp->Intensity / 3.0f;
			}
			else if( Type == LightType_Spot )
			{
				USpotLightComponent* SpotLightComp = Cast< USpotLightComponent>( LightComp );
				NewLightComponent->Range = SpotLightComp->AttenuationRadius / 100.0f;
				NewLightComponent->SpotAngle = ConvertSpotAngle( SpotLightComp->OuterConeAngle );

				if( SpotLightComp->IntensityUnits != ELightUnits::Candelas )
				{
					float LastLightBrigtness = SpotLightComp->ComputeLightBrightness();
					SpotLightComp->IntensityUnits = ELightUnits::Candelas;
					SpotLightComp->SetLightBrightness( LastLightBrigtness );
				}

				NewLightComponent->Intensity = SpotLightComp->Intensity / 3.0f;
			}
			else if( Type == LightType_Rect )
			{
				URectLightComponent* RectLightComponent = Cast< URectLightComponent>( LightComp );
				
				NewLightComponent->Width = RectLightComponent->SourceWidth / 100.0f;
				NewLightComponent->Height = RectLightComponent->SourceHeight / 100.0f;

				if( RectLightComponent->IntensityUnits != ELightUnits::Candelas )
				{
					float LastLightBrigtness = RectLightComponent->ComputeLightBrightness();
					RectLightComponent->IntensityUnits = ELightUnits::Candelas;
					RectLightComponent->SetLightBrightness( LastLightBrigtness );
				}

				NewLightComponent->Intensity = RectLightComponent->Intensity / 3.0f;
			}

			NewLightComponent->Color = LightComp->LightColor;
			NewLightComponent->Type = GetUnityLightComponentType( Type );			
			NewLightComponent->Shadows = LightComp->CastShadows;			
			NewLightComponent->Enabled = LightComp->bAffectsWorld;
			
		}
		UInstancedStaticMeshComponent* InstancedMeshComp = Cast<UInstancedStaticMeshComponent>( AC );
		//Continue because I already spawned the instances as individual actors
		if( InstancedMeshComp )
			continue;
		USkeletalMeshComponent *SkelMeshComp = dynamic_cast<USkeletalMeshComponent*>( AC );		
		if ( SkelMeshComp )
		{
			ProcessMeshComponent( A, nullptr, SkelMeshComp, GO );
		}		
		USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( AC );
		if( SplineMeshComponent )
		{
			//auto Pos = SplineMeshComponent->SplineParams.StartPos;
			//FVector SplineExtents = SplineMeshComponent->SplineParams.EndPos - SplineMeshComponent->SplineParams.StartPos;			
			//UStaticMesh* StaticMesh = SplineMeshComponent->GetStaticMesh();
			//if( StaticMesh )
			//{
			//	auto MeshBounds = StaticMesh->GetBounds();
			//	FVector MeshSizes = MeshBounds.BoxExtent * 2;
			//	if( FMath::IsNearlyZero( SplineExtents.X, 1.0f ) )
			//		SplineExtents.X = MeshSizes.X;
			//	if( FMath::IsNearlyZero( SplineExtents.Y, 1.0f ) )
			//		SplineExtents.Y = MeshSizes.Y;
			//	if( FMath::IsNearlyZero( SplineExtents.Z, 1.0f ) )
			//		SplineExtents.Z = MeshSizes.Z;
			//
			//	//Scale based on Z up, needs fixing
			//	FVector Scale = SplineExtents / MeshSizes;
			//	//auto Average = (SplineMeshComponent->SplineParams.StartPos + SplineMeshComponent->SplineParams.EndPos)/2;
			//
			//	FTransform& CompTransform = (FTransform&)SplineMeshComponent->GetComponentToWorld();
			//	CompTransform.SetLocation( Pos );
			//	//rotate from UE to Unity space
			//	FRotator Rotator = FRotator::MakeFromEuler( SplineRotation );
			//	CompTransform.SetRotation( FQuat( Rotator ));
			//	CompTransform.SetScale3D( Scale );
			//}
		}
		UStaticMeshComponent *StaticMeshComp = dynamic_cast<UStaticMeshComponent*>( AC );
		if ( StaticMeshComp )
		{	
			ProcessMeshComponent( A, StaticMeshComp, nullptr, GO );
		}
		USplineComponent* SplineComponent = Cast<USplineComponent>( AC );
		if( SplineComponent )
		{
			int32 Points = SplineComponent->GetNumberOfSplinePoints();
		}
		USphereReflectionCaptureComponent* SphereReflection = Cast<USphereReflectionCaptureComponent>( AC );
		if( SphereReflection )
		{
			float Influence = SphereReflection->InfluenceRadius / 100.0f;
			ReflectionProbeComponent* NewReflectionProbeComponent = ( ReflectionProbeComponent*)GO->AddComponent( CT_REFLECTIONPROBE );
			NewReflectionProbeComponent->Intensity = 1.0f;// SphereReflection->Brightness;
			NewReflectionProbeComponent->Size = FVector( Influence, Influence, Influence );
		}
		UBoxReflectionCaptureComponent* BoxReflection = Cast<UBoxReflectionCaptureComponent>( AC );
		if( BoxReflection )
		{
			FTransform BoxTransform = BoxReflection->GetComponentTransform();
			ReflectionProbeComponent* NewReflectionProbeComponent = (ReflectionProbeComponent*)GO->AddComponent( CT_REFLECTIONPROBE );
			NewReflectionProbeComponent->Intensity = 1.0f;// BoxReflection->Brightness;
			//Also convert to cm
			NewReflectionProbeComponent->Size = BoxTransform.GetScale3D() / 100.0f;
		}
		UDecalComponent* Decal = Cast<UDecalComponent>( AC );
		if( Decal && ( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP ||
					   CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP ) )
		{
			DecalComponent* NewDecalComponent = (DecalComponent*)GO->AddComponent( CT_DECAL );

			UMaterialInterface* DecalMat = Decal->GetDecalMaterial();
			MaterialBinding* Binding = ProcessMaterialReference( DecalMat );
			NewDecalComponent->Material = Binding->UnityMat;
			NewDecalComponent->Size = Decal->DecalSize;
			NewDecalComponent->Size /= 100.0f;
			NewDecalComponent->Size *= 2.0f;
			ConvertScaleForUnity( NewDecalComponent->Size );
		}
	}

	S->EvaluatePrefab( GO );
}

bool VerifyOrCreateDirectory( const FString& TestDir )
{
	//Directory Exists?
	if ( !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *TestDir ) )
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectory( *TestDir );

		if ( !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *TestDir ) )
		{
			return false;
		}
	}
	return true;
}

bool SaveFile( const char *File, void *data, int64_t size )
{
	FILE *f = nullptr;
	if( File && strcmp( File, "" ) != 0 )
		f = fopen( File, "wb" );

	if( !f )
		return false;

	fwrite( data, sizeof( char ), (int)size, f );
	fclose( f );
	return true;
}
int64_t GetFileSize( const char *FileName )
{
	if( !FileName || !FileName[ 0 ] )
		return -1;
	int64_t size = 0;
	FILE* f = nullptr;
	f = fopen( FileName, "rb" );
	if( !f )
		return -1;

	fseek( f, 0, SEEK_END );
	size = ftell( f );

	if( size == -1 )
	{
		//RELog( "The file may be over 2GB, use _ftelli64 !" );
		size = _ftelli64( f );

	}
	fclose( f );
	return size;
}
int64_t LoadFile( const char *File, uint8** data )
{
	if( !File || !data || File[ 0 ] == 0 )
		return -1;

	int64_t size = 0;
	size = GetFileSize( File );

	FILE *f = nullptr;
	f = fopen( File, "rb" );
	if( !f )
	{
		int Ret = -1;	
		return Ret;
	}

	//Add '0' character (if presumed to be acting like a string)
	//might impply allocation errors ?
	*data = new uint8[ (size_t)size + 1 ];
	if( !( *data ) )
	{
		//RELog( "LoadFile Allocation Error" );
		return -1;
	}
	( *data )[ size ] = 0;//last character should be 0 in case it's a string

	size_t BytesRead = fread( *data, sizeof( uint8 ), size, f );
	fclose( f );
	if( BytesRead == 0 )
	{
		BytesRead = 0;
	}
	return BytesRead;
}

#include <atomic>

UWorld *CurrentWorld = nullptr;

std::atomic<int> ProcessingState;
void RenderThreadExport()
{
	ProcessAllMaterials();

	ProcessingState = 1;
}

class RenderThreadTaskInitializer
{
public:
	void QueCommand(  bool UseSync = false )
	{
		if( UseSync )
		{
			RenderThreadExport();
		}
		else
		{
			RenderThreadTaskInitializer* This = this;
			ENQUEUE_RENDER_COMMAND( Command_RenderThreadExport )(
				[This]( FRHICommandListImmediate& RHICmdList )
			{
				RenderThreadExport();
			} );
		}
	}
};
class UICallback
{
public:
	virtual void AddToolbarExtension()
	{

	}
};
RenderThreadTaskInitializer *RTTI = new RenderThreadTaskInitializer;

#endif

Scene* AllocateScene()
{
	return new Scene;
}

#include "Editor/UnrealEd/Private/FbxExporter.h"
#include "Editor/UnrealEd/Classes/Exporters/FbxExportOption.h"
#include "Misc/FeedbackContext.h"
#include "Engine/Classes/Camera/CameraActor.h"
#include "Engine/Classes/Camera/CameraComponent.h"
#include "Engine/Classes/Engine/Light.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionVectorParameter.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionConstant3Vector.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionConstant2Vector.h"
#include "Runtime/Engine/Public/Rendering/SkeletalMeshModel.h"
#include "Runtime/CoreUObject/Public/UObject/Metadata.h"

using namespace UnFbx;
using namespace fbxsdk;

FbxManager* SdkManager = nullptr;
FbxAnimStack* AnimStack = nullptr;
FbxAnimLayer* AnimLayer = nullptr;
FbxCamera* DefaultCamera = nullptr;
FbxScene* TheScene = nullptr;
bool bSceneGlobalTimeLineSet = false;
bool bKeepHierarchy = true;

TMap<FString, int32> FbxNodeNameToIndexMap;
TMap<const AActor*, FbxNode*> FbxActors;
TMap<const USkeletalMeshComponent*, FbxNode*> FbxSkeletonRoots;
TMap<const UMaterialInterface*, FbxSurfaceMaterial*> FbxMaterials;
TMap<const UStaticMesh*, FbxMesh*> FbxMeshes;
TMap<const UStaticMesh*, FbxMesh*> FbxCollisionMeshes;
FFbxDataConverter Converter;
void DetermineVertsToWeld( TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshLODResources& RenderMesh )
{
	const int32 VertexCount = RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts 
	VertRemap.Empty( VertexCount );
	VertRemap.AddUninitialized( VertexCount );

	// List of Unreal Verts to keep
	UniqueVerts.Empty( VertexCount );

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector, int32> HashedVerts;
	for( int32 a = 0; a < VertexCount; a++ )
	{
		const FVector& PositionA = (FVector&)RenderMesh.VertexBuffers.PositionVertexBuffer.VertexPosition( a );
		const int32* FoundIndex = HashedVerts.Find( PositionA );
		if( !FoundIndex )
		{
			int32 NewIndex = UniqueVerts.Add( a );
			VertRemap[ a ] = NewIndex;
			HashedVerts.Add( PositionA, NewIndex );
		}
		else
		{
			VertRemap[ a ] = *FoundIndex;
		}
	}
}
void DetermineUVsToWeld( TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshVertexBuffer& VertexBuffer, int32 TexCoordSourceIndex )
{
	const int32 VertexCount = VertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts
	VertRemap.Empty( VertexCount );
	VertRemap.AddUninitialized( VertexCount );

	// List of Unreal Verts to keep
	UniqueVerts.Empty( VertexCount );

	// Combine matching verts using hashed search to maintain good performance
	#if ENGINE_MAJOR_VERSION == 4
		TMap<FVector2D, int32> HashedVerts;
	#else
		TMap<FVector2f, int32> HashedVerts;
	#endif
	for( int32 Vertex = 0; Vertex < VertexCount; Vertex++ )
	{
		#if ENGINE_MAJOR_VERSION == 4
			const FVector2D& PositionA = VertexBuffer.GetVertexUV( Vertex, TexCoordSourceIndex );
		#else
			const FVector2f& PositionA = VertexBuffer.GetVertexUV( Vertex, TexCoordSourceIndex );
		#endif
		const int32* FoundIndex = HashedVerts.Find( PositionA );
		if( !FoundIndex )
		{
			int32 NewIndex = UniqueVerts.Add( Vertex );
			VertRemap[ Vertex ] = NewIndex;
			HashedVerts.Add( PositionA, NewIndex );
		}
		else
		{
			VertRemap[ Vertex ] = *FoundIndex;
		}
	}
}
FbxDouble3 SetMaterialComponent( FColorMaterialInput& MatInput, bool ToLinear )
{
	FColor RGBColor;
	FLinearColor LinearColor;
	bool LinearSet = false;

	if( MatInput.Expression )
	{
		if( Cast<UMaterialExpressionConstant>( MatInput.Expression ) )
		{
			UMaterialExpressionConstant* Expr = Cast<UMaterialExpressionConstant>( MatInput.Expression );
			RGBColor = FColor( Expr->R );
		}
		else if( Cast<UMaterialExpressionVectorParameter>( MatInput.Expression ) )
		{
			UMaterialExpressionVectorParameter* Expr = Cast<UMaterialExpressionVectorParameter>( MatInput.Expression );
			LinearColor = Expr->DefaultValue;
			LinearSet = true;
			//Linear to sRGB color space conversion
			RGBColor = Expr->DefaultValue.ToFColor( true );
		}
		else if( Cast<UMaterialExpressionConstant3Vector>( MatInput.Expression ) )
		{
			UMaterialExpressionConstant3Vector* Expr = Cast<UMaterialExpressionConstant3Vector>( MatInput.Expression );
			RGBColor.R = Expr->Constant.R;
			RGBColor.G = Expr->Constant.G;
			RGBColor.B = Expr->Constant.B;
		}
		else if( Cast<UMaterialExpressionConstant4Vector>( MatInput.Expression ) )
		{
			UMaterialExpressionConstant4Vector* Expr = Cast<UMaterialExpressionConstant4Vector>( MatInput.Expression );
			RGBColor.R = Expr->Constant.R;
			RGBColor.G = Expr->Constant.G;
			RGBColor.B = Expr->Constant.B;
			//RGBColor.A = Expr->A;
		}
		else if( Cast<UMaterialExpressionConstant2Vector>( MatInput.Expression ) )
		{
			UMaterialExpressionConstant2Vector* Expr = Cast<UMaterialExpressionConstant2Vector>( MatInput.Expression );
			RGBColor.R = Expr->R;
			RGBColor.G = Expr->G;
			RGBColor.B = 0;
		}
		else
		{
			RGBColor.R = MatInput.Constant.R;
			RGBColor.G = MatInput.Constant.G;
			RGBColor.B = MatInput.Constant.B;
		}
	}
	else
	{
		RGBColor.R = MatInput.Constant.R;
		RGBColor.G = MatInput.Constant.G;
		RGBColor.B = MatInput.Constant.B;
	}

	if( ToLinear )
	{
		if( !LinearSet )
		{
			//sRGB to linear color space conversion
			LinearColor = FLinearColor( RGBColor );
		}
		return FbxDouble3( LinearColor.R, LinearColor.G, LinearColor.B );
	}
	return FbxDouble3( RGBColor.R, RGBColor.G, RGBColor.B );
}
bool FillFbxTextureProperty( const char* PropertyName, const FExpressionInput& MaterialInput, FbxSurfaceMaterial* FbxMaterial )
{
	if( TheScene == NULL )
	{
		return false;
	}

	FbxProperty FbxColorProperty = FbxMaterial->FindProperty( PropertyName );
	if( FbxColorProperty.IsValid() )
	{
		if( MaterialInput.IsConnected() && MaterialInput.Expression )
		{
			if( MaterialInput.Expression->IsA( UMaterialExpressionTextureSample::StaticClass() ) )
			{
				UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>( MaterialInput.Expression );
				if( TextureSample && TextureSample->Texture && TextureSample->Texture->AssetImportData )
				{
					FString TextureSourceFullPath = TextureSample->Texture->AssetImportData->GetFirstFilename();
					//Create a fbx property
					FbxFileTexture* lTexture = FbxFileTexture::Create( TheScene, "EnvSamplerTex" );
					lTexture->SetFileName( TCHAR_TO_UTF8( *TextureSourceFullPath ) );
					lTexture->SetTextureUse( FbxTexture::eStandard );
					lTexture->SetMappingType( FbxTexture::eUV );
					lTexture->ConnectDstProperty( FbxColorProperty );
					return true;
				}
			}
		}
	}
	return false;
}
FbxSurfaceMaterial* CreateDefaultMaterial()
{
	// TODO(sbc): the below cast is needed to avoid clang warning.  The upstream
	// signature in FBX should really use 'const char *'.
	FbxSurfaceMaterial* FbxMaterial = TheScene->GetMaterial( const_cast<char*>( "Fbx Default Material" ) );

	if( !FbxMaterial )
	{
		FbxMaterial = FbxSurfaceLambert::Create( TheScene, "Fbx Default Material" );
		( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( FbxDouble3( 0.72, 0.72, 0.72 ) );
	}

	return FbxMaterial;
}
bool SetFBXTextureProperty( FbxSurfaceMaterial* FbxMaterial, FString TextureName, FString TextureSourceFullPath, const char* FbxPropertyName, FbxScene* InScene)
{
	FbxProperty FbxColorProperty = FbxMaterial->FindProperty( FbxPropertyName );
	if( FbxColorProperty.IsValid() )
	{
		if( !TextureSourceFullPath.IsEmpty() )
		{
			//Create a fbx property
			FbxFileTexture* lTexture = FbxFileTexture::Create( InScene, TCHAR_TO_UTF8( *TextureName ) );
			lTexture->SetFileName( TCHAR_TO_UTF8( *TextureSourceFullPath ) );
			lTexture->SetTextureUse( FbxTexture::eStandard );
			lTexture->SetMappingType( FbxTexture::eUV );
			lTexture->ConnectDstProperty( FbxColorProperty );
			return true;
		}
	}
	return false;
};
FbxSurfaceMaterial* ExportMaterial_UTU( UMaterialInterface* MaterialInterface, MaterialBinding* Binding )
{
	if( TheScene == nullptr || MaterialInterface == nullptr || MaterialInterface->GetMaterial() == nullptr ) return nullptr;

	// Verify that this material has not already been exported:
	if( FbxMaterials.Find( MaterialInterface ) )
	{
		return *FbxMaterials.Find( MaterialInterface );
	}

	// Create the Fbx material
	FbxSurfaceMaterial* FbxMaterial = nullptr;

	UMaterial* Material = MaterialInterface->GetMaterial();
	if( !Material )
	{
		//Nothing to export
		return nullptr;
	}

	// Set the shading model
	if( Material->GetShadingModels().HasOnlyShadingModel( MSM_DefaultLit ) )
	{
		FbxMaterial = FbxSurfacePhong::Create( TheScene, TCHAR_TO_UTF8( *MaterialInterface->GetName() ) );
		//((FbxSurfacePhong*)FbxMaterial)->Specular.Set(Material->Specular));
		//((FbxSurfacePhong*)FbxMaterial)->Shininess.Set(Material->SpecularPower.Constant);
	}
	else // if (Material->ShadingModel == MSM_Unlit)
	{
		FbxMaterial = FbxSurfaceLambert::Create( TheScene, TCHAR_TO_UTF8( *MaterialInterface->GetName() ) );
	}

	for( int i = 0; i < Binding->UnityMat->Textures.size(); i++ )
	{
		UnityTexture* Tex = Binding->UnityMat->Textures[ i ];
		switch( Tex->Semantic )
		{
			case TS_MainTex			:
			case TS_BaseMap			:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			case TS_BumpMap			:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sNormalMap, TheScene ); break;
			//case TS_DetailAlbedoMap	:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_DetailMask		:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_DetailNormalMap	:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			case TS_EmissionMap		:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sEmissive, TheScene ); break;
			//case TS_MetallicGlossMap:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_OcclusionMap	:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_ParallaxMap		:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
			//case TS_SpecGlossMap	:SetFBXTextureProperty( FbxMaterial, Binding->GenerateName(), Tex->File.c_str(), FbxSurfaceMaterial::sDiffuse, TheScene ); break;
		}
	}

	FbxMaterials.Add( MaterialInterface, FbxMaterial );

	return FbxMaterial;
}
FbxSurfaceMaterial* ExportMaterial( UMaterialInterface* MaterialInterface )
{
	if( TheScene == nullptr || MaterialInterface == nullptr || MaterialInterface->GetMaterial() == nullptr ) return nullptr;

	// Verify that this material has not already been exported:
	if( FbxMaterials.Find( MaterialInterface ) )
	{
		return *FbxMaterials.Find( MaterialInterface );
	}

	// Create the Fbx material
	FbxSurfaceMaterial* FbxMaterial = nullptr;

	UMaterial* Material = MaterialInterface->GetMaterial();
	if( !Material )
	{
		//Nothing to export
		return nullptr;
	}

	// Set the shading model
	if( Material->GetShadingModels().HasOnlyShadingModel( MSM_DefaultLit ) )
	{
		FbxMaterial = FbxSurfacePhong::Create( TheScene, TCHAR_TO_UTF8( *MaterialInterface->GetName() ) );
		//((FbxSurfacePhong*)FbxMaterial)->Specular.Set(Material->Specular));
		//((FbxSurfacePhong*)FbxMaterial)->Shininess.Set(Material->SpecularPower.Constant);
	}
	else // if (Material->ShadingModel == MSM_Unlit)
	{
		FbxMaterial = FbxSurfaceLambert::Create( TheScene, TCHAR_TO_UTF8( *MaterialInterface->GetName() ) );
	}


	//Get the base material connected expression parameter name for all the supported fbx material exported properties
	//We only support material input where the connected expression is a parameter of type (constant, scalar, vector, texture, TODO virtual texture)

	FName BaseColorParamName = ( !Material->BaseColor.UseConstant && Material->BaseColor.Expression ) ? Material->BaseColor.Expression->GetParameterName() : NAME_None;
	bool BaseColorParamSet = false;
	FName EmissiveParamName = ( !Material->EmissiveColor.UseConstant && Material->EmissiveColor.Expression ) ? Material->EmissiveColor.Expression->GetParameterName() : NAME_None;
	bool EmissiveParamSet = false;
	FName NormalParamName = Material->Normal.Expression ? Material->Normal.Expression->GetParameterName() : NAME_None;
	bool NormalParamSet = false;
	FName OpacityParamName = ( !Material->Opacity.UseConstant && Material->Opacity.Expression ) ? Material->Opacity.Expression->GetParameterName() : NAME_None;
	bool OpacityParamSet = false;
	FName OpacityMaskParamName = ( !Material->OpacityMask.UseConstant && Material->OpacityMask.Expression ) ? Material->OpacityMask.Expression->GetParameterName() : NAME_None;
	bool OpacityMaskParamSet = false;

	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( MaterialInterface );
	EBlendMode BlendMode = MaterialInstance != nullptr && MaterialInstance->BasePropertyOverrides.bOverride_BlendMode ? MaterialInstance->BasePropertyOverrides.BlendMode : Material->BlendMode;

	// Loop through all types of parameters for this base material and add the supported one to the fbx material.
	//The order is important we search Texture, Vector, Scalar.

	TArray<FMaterialParameterInfo> ParameterInfos;
	TArray<FGuid> Guids;
	//Query all base material texture parameters.
	Material->GetAllTextureParameterInfo( ParameterInfos, Guids );
	for( int32 ParameterIdx = 0; ParameterIdx < ParameterInfos.Num(); ParameterIdx++ )
	{
		const FMaterialParameterInfo& ParameterInfo = ParameterInfos[ ParameterIdx ];
		FName ParameterName = ParameterInfo.Name;
		UTexture* Value;
		//Query the material instance parameter overridden value
		if( MaterialInterface->GetTextureParameterValue( ParameterInfo, Value ) && Value && Value->AssetImportData )
		{
			//This lambda extract the source file path and create the fbx file texture node and connect it to the fbx material
			auto SetTextureProperty = [&FbxMaterial, &Value]( const char* FbxPropertyName, FbxScene* InScene )->bool
			{
				FbxProperty FbxColorProperty = FbxMaterial->FindProperty( FbxPropertyName );
				if( FbxColorProperty.IsValid() )
				{
					const FString TextureName = Value->GetName();
					const FString TextureSourceFullPath = Value->AssetImportData->GetFirstFilename();
					if( !TextureSourceFullPath.IsEmpty() )
					{
						//Create a fbx property
						FbxFileTexture* lTexture = FbxFileTexture::Create( InScene, TCHAR_TO_UTF8( *TextureName ) );
						lTexture->SetFileName( TCHAR_TO_UTF8( *TextureSourceFullPath ) );
						lTexture->SetTextureUse( FbxTexture::eStandard );
						lTexture->SetMappingType( FbxTexture::eUV );
						lTexture->ConnectDstProperty( FbxColorProperty );
						return true;
					}
				}
				return false;
			};

			if( BaseColorParamName != NAME_None && ParameterName == BaseColorParamName )
			{
				BaseColorParamSet = SetTextureProperty( FbxSurfaceMaterial::sDiffuse, TheScene );
			}
			if( EmissiveParamName != NAME_None && ParameterName == EmissiveParamName )
			{
				EmissiveParamSet = SetTextureProperty( FbxSurfaceMaterial::sEmissive, TheScene );
			}

			if( BlendMode == BLEND_Translucent )
			{
				if( OpacityParamName != NAME_None && ParameterName == OpacityParamName )
				{
					OpacityParamSet = SetTextureProperty( FbxSurfaceMaterial::sTransparentColor, TheScene );
				}
				if( OpacityMaskParamName != NAME_None && ParameterName == OpacityMaskParamName )
				{
					OpacityMaskParamSet = SetTextureProperty( FbxSurfaceMaterial::sTransparencyFactor, TheScene );
				}
			}
			else
			{
				//There is no normal input in Blend translucent mode
				if( NormalParamName != NAME_None && ParameterName == NormalParamName )
				{
					NormalParamSet = SetTextureProperty( FbxSurfaceMaterial::sNormalMap, TheScene );
				}
			}
		}
	}

	Material->GetAllVectorParameterInfo( ParameterInfos, Guids );
	//Query all base material vector parameters.
	for( int32 ParameterIdx = 0; ParameterIdx < ParameterInfos.Num(); ParameterIdx++ )
	{
		const FMaterialParameterInfo& ParameterInfo = ParameterInfos[ ParameterIdx ];
		FName ParameterName = ParameterInfo.Name;
		FLinearColor LinearColor;
		//Query the material instance parameter overridden value
		if( MaterialInterface->GetVectorParameterValue( ParameterInfo, LinearColor ) )
		{
			FbxDouble3 LinearFbxValue( LinearColor.R, LinearColor.G, LinearColor.B );
			if( !BaseColorParamSet && BaseColorParamName != NAME_None && ParameterName == BaseColorParamName )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( LinearFbxValue );
				BaseColorParamSet = true;
			}
			if( !EmissiveParamSet && EmissiveParamName != NAME_None && ParameterName == EmissiveParamName )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->Emissive.Set( LinearFbxValue );
				EmissiveParamSet = true;
			}
		}
	}

	//Query all base material scalar parameters.
	Material->GetAllScalarParameterInfo( ParameterInfos, Guids );
	for( int32 ParameterIdx = 0; ParameterIdx < ParameterInfos.Num(); ParameterIdx++ )
	{
		const FMaterialParameterInfo& ParameterInfo = ParameterInfos[ ParameterIdx ];
		FName ParameterName = ParameterInfo.Name;
		float Value;
		//Query the material instance parameter overridden value
		if( MaterialInterface->GetScalarParameterValue( ParameterInfo, Value ) )
		{
			FbxDouble FbxValue = (FbxDouble)Value;
			FbxDouble3 FbxVector( FbxValue, FbxValue, FbxValue );
			if( !BaseColorParamSet && BaseColorParamName != NAME_None && ParameterName == BaseColorParamName )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( FbxVector );
				BaseColorParamSet = true;
			}
			if( !EmissiveParamSet && EmissiveParamName != NAME_None && ParameterName == EmissiveParamName )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->Emissive.Set( FbxVector );
				EmissiveParamSet = true;
			}
			if( BlendMode == BLEND_Translucent )
			{
				if( !OpacityParamSet && OpacityParamName != NAME_None && ParameterName == OpacityParamName )
				{
					( (FbxSurfaceLambert*)FbxMaterial )->TransparentColor.Set( FbxVector );
					OpacityParamSet = true;
				}

				if( !OpacityMaskParamSet && OpacityMaskParamName != NAME_None && ParameterName == OpacityMaskParamName )
				{
					( (FbxSurfaceLambert*)FbxMaterial )->TransparencyFactor.Set( FbxValue );
					OpacityMaskParamSet = true;
				}
			}
		}
	}

	//TODO: export virtual texture to fbx
	//Query all base material runtime virtual texture parameters.
// 	Material->GetAllRuntimeVirtualTextureParameterInfo(ParameterInfos, Guids);
// 	for (int32 ParameterIdx = 0; ParameterIdx < ParameterInfos.Num(); ParameterIdx++)
// 	{
// 		const FMaterialParameterInfo& ParameterInfo = ParameterInfos[ParameterIdx];
// 		FName ParameterName = ParameterInfo.Name;
// 		URuntimeVirtualTexture* Value;
//		//Query the material instance parameter overridden value
// 		if (MaterialInterface->GetRuntimeVirtualTextureParameterValue(ParameterInfo, Value, bOveriddenOnly))
// 		{
// 		}
// 	}

	//
	//In case there is no material instance override, we extract the value from the base material
	//

	//The OpacityMaskParam set the transparency factor, so set it only if it was not set
	if( !OpacityMaskParamSet )
	{
		( (FbxSurfaceLambert*)FbxMaterial )->TransparencyFactor.Set( Material->Opacity.Constant );
	}

	// Fill in the profile_COMMON effect with the material information.
	//Fill the texture or constant
	if( !BaseColorParamSet )
	{
		if( !FillFbxTextureProperty( FbxSurfaceMaterial::sDiffuse, Material->BaseColor, FbxMaterial ) )
		{
			( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( SetMaterialComponent( Material->BaseColor, true ) );
		}
	}

	if( !EmissiveParamSet )
	{
		if( !FillFbxTextureProperty( FbxSurfaceMaterial::sEmissive, Material->EmissiveColor, FbxMaterial ) )
		{
			( (FbxSurfaceLambert*)FbxMaterial )->Emissive.Set( SetMaterialComponent( Material->EmissiveColor, true ) );
		}
	}

	//Always set the ambient to zero since we dont have ambient in unreal we want to avoid default value in DCCs
	( (FbxSurfaceLambert*)FbxMaterial )->Ambient.Set( FbxDouble3( 0.0, 0.0, 0.0 ) );

	if( BlendMode == BLEND_Translucent )
	{
		if( !OpacityParamSet )
		{
			if( !FillFbxTextureProperty( FbxSurfaceMaterial::sTransparentColor, Material->Opacity, FbxMaterial ) )
			{
				FbxDouble3 OpacityValue( (FbxDouble)( Material->Opacity.Constant ), (FbxDouble)( Material->Opacity.Constant ), (FbxDouble)( Material->Opacity.Constant ) );
				( (FbxSurfaceLambert*)FbxMaterial )->TransparentColor.Set( OpacityValue );
			}
		}

		if( !OpacityMaskParamSet )
		{
			if( !FillFbxTextureProperty( FbxSurfaceMaterial::sTransparencyFactor, Material->OpacityMask, FbxMaterial ) )
			{
				( (FbxSurfaceLambert*)FbxMaterial )->TransparencyFactor.Set( Material->OpacityMask.Constant );
			}
		}
	}
	else
	{
		//There is no normal input in Blend translucent mode
		if( !NormalParamSet )
		{
			//Set the Normal map only if there is a texture sampler
			FillFbxTextureProperty( FbxSurfaceMaterial::sNormalMap, Material->Normal, FbxMaterial );
		}
	}

	FbxMaterials.Add( MaterialInterface, FbxMaterial );

	return FbxMaterial;
}
#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
#include "PxVec3.h"
FVector P2UVector( const PxVec3& PVec )
{
	return FVector( PVec.x, PVec.y, PVec.z );
}
#endif

#if WITH_CHAOS
	#include "Chaos/Convex.h"
#endif
#if (WITH_PHYSX && PHYSICS_INTERFACE_PHYSX) || WITH_CHAOS
class FCollisionFbxExporter
{
public:
	FCollisionFbxExporter( const UStaticMesh* StaticMeshToExport, FbxMesh* ExportMesh, int32 ActualMatIndexToExport )
	{
		BoxPositions[ 0 ] = FVector( -1, -1, +1 );
		BoxPositions[ 1 ] = FVector( -1, +1, +1 );
		BoxPositions[ 2 ] = FVector( +1, +1, +1 );
		BoxPositions[ 3 ] = FVector( +1, -1, +1 );

		BoxFaceRotations[ 0 ] = FRotator( 0, 0, 0 );
		BoxFaceRotations[ 1 ] = FRotator( 90.f, 0, 0 );
		BoxFaceRotations[ 2 ] = FRotator( -90.f, 0, 0 );
		BoxFaceRotations[ 3 ] = FRotator( 0, 0, 90.f );
		BoxFaceRotations[ 4 ] = FRotator( 0, 0, -90.f );
		BoxFaceRotations[ 5 ] = FRotator( 180.f, 0, 0 );

		DrawCollisionSides = 16;

		SpherNumSides = DrawCollisionSides;
		SphereNumRings = DrawCollisionSides / 2;
		SphereNumVerts = ( SpherNumSides + 1 ) * ( SphereNumRings + 1 );

		CapsuleNumSides = DrawCollisionSides;
		CapsuleNumRings = ( DrawCollisionSides / 2 ) + 1;
		CapsuleNumVerts = ( CapsuleNumSides + 1 ) * ( CapsuleNumRings + 1 );

		CurrentVertexOffset = 0;

		StaticMesh = StaticMeshToExport;
		Mesh = ExportMesh;
		ActualMatIndex = ActualMatIndexToExport;
	}

	void ExportCollisions()
	{
		const FKAggregateGeom& AggGeo = GetBodySetup( StaticMesh )->AggGeom;

		int32 VerticeNumber = 0;
		for( const FKConvexElem& ConvexElem : AggGeo.ConvexElems )
		{
			VerticeNumber += GetConvexVerticeNumber( ConvexElem );
		}
		//for( const FKBoxElem& BoxElem : AggGeo.BoxElems )
		//{
		//	VerticeNumber += GetBoxVerticeNumber();
		//}
		//for( const FKSphereElem& SphereElem : AggGeo.SphereElems )
		//{
		//	VerticeNumber += GetSphereVerticeNumber();
		//}
		//for( const FKSphylElem& CapsuleElem : AggGeo.SphylElems )
		//{
		//	VerticeNumber += GetCapsuleVerticeNumber();
		//}

		Mesh->InitControlPoints( VerticeNumber );
		ControlPoints = Mesh->GetControlPoints();
		CurrentVertexOffset = 0;
		//////////////////////////////////////////////////////////////////////////
		// Set all vertex
		for( const FKConvexElem& ConvexElem : AggGeo.ConvexElems )
		{
			AddConvexVertex( ConvexElem );
		}

		//for( const FKBoxElem& BoxElem : AggGeo.BoxElems )
		//{
		//	AddBoxVertex( BoxElem );
		//}
		//
		//for( const FKSphereElem& SphereElem : AggGeo.SphereElems )
		//{
		//	AddSphereVertex( SphereElem );
		//}
		//
		//for( const FKSphylElem& CapsuleElem : AggGeo.SphylElems )
		//{
		//	AddCapsuleVertex( CapsuleElem );
		//}

		// Set the normals on Layer 0.
		FbxLayer* Layer = Mesh->GetLayer( 0 );
		if( Layer == nullptr )
		{
			Mesh->CreateLayer();
			Layer = Mesh->GetLayer( 0 );
		}
		// Create and fill in the per-face-vertex normal data source.
		LayerElementNormal = FbxLayerElementNormal::Create( Mesh, "" );
		// Set the normals per polygon instead of storing normals on positional control points
		LayerElementNormal->SetMappingMode( FbxLayerElement::eByPolygonVertex );
		// Set the normal values for every polygon vertex.
		LayerElementNormal->SetReferenceMode( FbxLayerElement::eDirect );

		//////////////////////////////////////////////////////////////////////////
		//Set the Normals
		for( const FKConvexElem& ConvexElem : AggGeo.ConvexElems )
		{
			AddConvexNormals( ConvexElem );
		}
		//for( const FKBoxElem& BoxElem : AggGeo.BoxElems )
		//{
		//	AddBoxNormal( BoxElem );
		//}

		//int32 SphereIndex = 0;
		//for( const FKSphereElem& SphereElem : AggGeo.SphereElems )
		//{
		//	AddSphereNormals( SphereElem, SphereIndex );
		//	SphereIndex++;
		//}
		//
		//int32 CapsuleIndex = 0;
		//for( const FKSphylElem& CapsuleElem : AggGeo.SphylElems )
		//{
		//	AddCapsuleNormals( CapsuleElem, CapsuleIndex );
		//	CapsuleIndex++;
		//}

		Layer->SetNormals( LayerElementNormal );

		//////////////////////////////////////////////////////////////////////////
		// Set polygons
		// Build list of polygon re-used multiple times to lookup Normals, UVs, other per face vertex information
		CurrentVertexOffset = 0; //Reset the current VertexCount
		for( const FKConvexElem& ConvexElem : AggGeo.ConvexElems )
		{
			AddConvexPolygon( ConvexElem );
		}

		//for( const FKBoxElem& BoxElem : AggGeo.BoxElems )
		//{
		//	AddBoxPolygons();
		//}
		//
		//for( const FKSphereElem& SphereElem : AggGeo.SphereElems )
		//{
		//	AddSpherePolygons();
		//}
		//
		//for( const FKSphylElem& CapsuleElem : AggGeo.SphylElems )
		//{
		//	AddCapsulePolygons();
		//}

		//////////////////////////////////////////////////////////////////////////
		//Free the sphere resources
		for( FDynamicMeshVertex* DynamicMeshVertex : SpheresVerts )
		{
			FMemory::Free( DynamicMeshVertex );
		}
		SpheresVerts.Empty();

		//////////////////////////////////////////////////////////////////////////
		//Free the capsule resources
		for( FDynamicMeshVertex* DynamicMeshVertex : CapsuleVerts )
		{
			FMemory::Free( DynamicMeshVertex );
		}
		CapsuleVerts.Empty();
	}

private:
	uint32 GetConvexVerticeNumber( const FKConvexElem& ConvexElem )
	{
	#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
		return ConvexElem.GetConvexMesh() != nullptr ? ConvexElem.GetConvexMesh()->getNbVertices() : 0;
	#elif WITH_CHAOS
		return ConvexElem.VertexData.Num();
	#endif
	}

	uint32 GetBoxVerticeNumber() { return 24; }

	uint32 GetSphereVerticeNumber() { return SphereNumVerts; }

	uint32 GetCapsuleVerticeNumber() { return CapsuleNumVerts; }

	void AddConvexVertex( const FKConvexElem& ConvexElem )
	{
	#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if( ConvexMesh == nullptr )
		{
			return;
		}
		FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );//match mesh rotations
		const PxVec3* VertexArray = ConvexMesh->getVertices();
		for( uint32 PosIndex = 0; PosIndex < ConvexMesh->getNbVertices(); ++PosIndex )
		{
			FVector Position = P2UVector( VertexArray[ PosIndex ] );
			Position = Rotator.RotateVector( Position );
			ControlPoints[ CurrentVertexOffset + PosIndex ] = FbxVector4( -Position.X, Position.Y, Position.Z );
		}
		CurrentVertexOffset += ConvexMesh->getNbVertices();
	#elif WITH_CHAOS
		const TArray<FVector>& VertexArray = ConvexElem.VertexData;
		FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );//match mesh rotations
		for( int32 PosIndex = 0; PosIndex < VertexArray.Num(); ++PosIndex )
		{
			FVector Position = VertexArray[ PosIndex ];
			Position = Rotator.RotateVector( Position );
			ControlPoints[ CurrentVertexOffset + PosIndex ] = FbxVector4( -Position.X, Position.Y, Position.Z );
		}
		CurrentVertexOffset += VertexArray.Num();
	#endif
	}

	void AddConvexNormals( const FKConvexElem& ConvexElem )
	{
	#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if( ConvexMesh == nullptr )
		{
			return;
		}
		const PxU8* PIndexBuffer = ConvexMesh->getIndexBuffer();
		int32 PolygonNumber = ConvexMesh->getNbPolygons();
		for( int32 PolyIndex = 0; PolyIndex < PolygonNumber; ++PolyIndex )
		{
			PxHullPolygon PolyData;
			if( !ConvexMesh->getPolygonData( PolyIndex, PolyData ) )
			{
				continue;
			}
			const PxVec3 PPlaneNormal( PolyData.mPlane[ 0 ], PolyData.mPlane[ 1 ], PolyData.mPlane[ 2 ] );
			FVector Normal = P2UVector( PPlaneNormal.getNormalized() );
			FbxVector4 FbxNormal = FbxVector4( Normal.X, -Normal.Y, Normal.Z );
			// add vertices 
			for( PxU32 j = 0; j < PolyData.mNbVerts; ++j )
			{
				LayerElementNormal->GetDirectArray().Add( FbxNormal );
			}
		}
	#elif WITH_CHAOS
		//Needs a link dependency to Chaos me thinks
		//const auto& ConvexMesh = ConvexElem.GetChaosConvexMesh();
		//if (!ConvexMesh.IsValid())
		//{
		//	return;
		//}
		//const TArray<Chaos::FConvex::FPlaneType>& Faces = ConvexMesh->GetFaces();
		//for (int32 PolyIndex = 0; PolyIndex < Faces.Num(); ++PolyIndex)
		//{
		//	FVector Normal = (FVector)Faces[PolyIndex].Normal().GetSafeNormal();
		//	FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		//	// add vertices 
		//	for (int32 j = 0; j < 3; ++j)
		//	{
		//		LayerElementNormal->GetDirectArray().Add(FbxNormal);
		//	}
		//}
	#endif
	}

	void AddConvexPolygon( const FKConvexElem& ConvexElem )
	{
	#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
		const physx::PxConvexMesh* ConvexMesh = ConvexElem.GetConvexMesh();
		if( ConvexMesh == nullptr )
		{
			return;
		}
		const PxU8* PIndexBuffer = ConvexMesh->getIndexBuffer();
		int32 PolygonNumber = ConvexMesh->getNbPolygons();
		for( int32 PolyIndex = 0; PolyIndex < PolygonNumber; ++PolyIndex )
		{
			PxHullPolygon PolyData;
			if( !ConvexMesh->getPolygonData( PolyIndex, PolyData ) )
			{
				continue;
			}
			Mesh->BeginPolygon( ActualMatIndex );
			const PxU8* PolyIndices = PIndexBuffer + PolyData.mIndexBase;
			// add vertices 
			//for( PxU32 j = 0; j < PolyData.mNbVerts; ++j )
			for( int j = PolyData.mNbVerts - 1; j >= 0; j-- )
			{
				const uint32 VertIndex = CurrentVertexOffset + PolyIndices[ j ];
				Mesh->AddPolygon( VertIndex );
			}
			Mesh->EndPolygon();
		}
		CurrentVertexOffset += ConvexMesh->getNbVertices();
	#elif WITH_CHAOS
		const auto& ConvexMesh = ConvexElem.GetChaosConvexMesh();
		if( !ConvexMesh.IsValid() )
		{
			return;
		}
		TArray<int32> IndexData( ConvexElem.IndexData );
		if( IndexData.Num() == 0 )
		{
			IndexData = ConvexElem.GetChaosConvexIndices();
		}
		check( IndexData.Num() % 3 == 0 );
		for( int32 VertexIndex = 0; VertexIndex < IndexData.Num(); VertexIndex += 3 )
		{
			Mesh->BeginPolygon( ActualMatIndex );
			Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex ] );
			Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex + 1 ] );
			Mesh->AddPolygon( CurrentVertexOffset + IndexData[ VertexIndex + 2 ] );
			Mesh->EndPolygon();
		}

		CurrentVertexOffset += ConvexElem.VertexData.Num();
	#endif
	}

	//////////////////////////////////////////////////////////////////////////
	//Box data
	FVector BoxPositions[ 4 ];
	FRotator BoxFaceRotations[ 6 ];


	int32 DrawCollisionSides;
	//////////////////////////////////////////////////////////////////////////
	//Sphere data
	int32 SpherNumSides;
	int32 SphereNumRings;
	int32 SphereNumVerts;
	TArray<FDynamicMeshVertex*> SpheresVerts;

	//////////////////////////////////////////////////////////////////////////
	//Capsule data
	int32 CapsuleNumSides;
	int32 CapsuleNumRings;
	int32 CapsuleNumVerts;
	TArray<FDynamicMeshVertex*> CapsuleVerts;

	//////////////////////////////////////////////////////////////////////////
	//Mesh Data
	uint32 CurrentVertexOffset;

	const UStaticMesh* StaticMesh;
	FbxMesh* Mesh;
	int32 ActualMatIndex;
	FbxVector4* ControlPoints;
	FbxLayerElementNormal* LayerElementNormal;
};
FbxNode* ExportCollisionMesh( FFbxExporter* Exporter, const UStaticMesh* StaticMesh, const TCHAR* MeshName, FbxNode* ParentActor )
{
	const FKAggregateGeom& AggGeo = GetBodySetup( StaticMesh )->AggGeom;
	if( AggGeo.GetElementCount() <= 0 )
	{
		return nullptr;
	}
	//No idea but FbxMeshes doesn't add vertex painted meshes, but returning here misses ConvexHulls for vertex painted objects
	//FbxMesh* Mesh = FbxMeshes.FindRef( StaticMesh );
	//if( !Mesh )
	//{
	//	//We export collision only if the mesh is already exported
	//	return nullptr;
	//}

	//Name the node with the actor name
	FString MeshCollisionName = MeshName;// TEXT( "UCX_" );
	MeshCollisionName += TEXT( "_ConvexHulls" ); //UTF8_TO_TCHAR( ParentActor->GetName() ); //-V595
	FbxNode* FbxActor = FbxNode::Create( TheScene, TCHAR_TO_UTF8( *MeshCollisionName ) );

	if( ParentActor != nullptr )
	{
		// Collision meshes are added directly to the scene root, so we need to use the global transform instead of the relative one.
		FbxAMatrix& GlobalTransform = ParentActor->EvaluateGlobalTransform();

		const FStaticMeshRenderData* RenderData = GetRenderData( StaticMesh );
		if( RenderData->LODResources.Num() == 1 )
		{
			//This is apparently needed to have 0,0,0 rotation for the collider but only when no LODs
			FbxActor->LclTranslation.Set( GlobalTransform.GetT() );
			FbxActor->LclRotation.Set( GlobalTransform.GetR() );
			FbxActor->LclScaling.Set( GlobalTransform.GetS() );
		}

		if ( ParentActor->GetParent())
			ParentActor->GetParent()->AddChild( FbxActor );
		else
			TheScene->GetRootNode()->AddChild( FbxActor );
	}

	FbxMesh* CollisionMesh = FbxCollisionMeshes.FindRef( StaticMesh );
	if( !CollisionMesh )
	{
		//Name the mesh attribute with the mesh name
		//MeshCollisionName = TEXT( "UCX_" );
		MeshCollisionName = MeshName;
		MeshCollisionName += TEXT( "_ConvexHulls" );
		CollisionMesh = FbxMesh::Create( TheScene, TCHAR_TO_UTF8( *MeshCollisionName ) );
		//Export all collision elements in one mesh
		FbxSurfaceMaterial* FbxMaterial = nullptr;
		int32 ActualMatIndex = FbxActor->AddMaterial( FbxMaterial );
		FCollisionFbxExporter CollisionFbxExporter( StaticMesh, CollisionMesh, ActualMatIndex );
		CollisionFbxExporter.ExportCollisions();
		FbxCollisionMeshes.Add( StaticMesh, CollisionMesh );
	}

	if( CollisionMesh->GetPolygonCount() > 0 )
	{
		//Set the original meshes in case it was already existing
		FbxActor->SetNodeAttribute( CollisionMesh );
	}
	return FbxActor;
}
#endif
FbxNode* ExportStaticMeshToFbx( FFbxExporter* Exporter, const UStaticMesh* StaticMesh, int32 ExportLOD, const TCHAR* MeshName, FbxNode* FbxActor, int32 LightmapUVChannel /*= -1*/, const FColorVertexBuffer* ColorBuffer /*= NULL*/, const TArray<FStaticMaterial>* MaterialOrderOverride /*= NULL*/, const TArray<UMaterialInterface*>* OverrideMaterials /*= NULL*/ )
{
	FbxMesh* Mesh = nullptr;
	if( ( ExportLOD == 0 || ExportLOD == -1 ) && LightmapUVChannel == -1 && ColorBuffer == nullptr && MaterialOrderOverride == nullptr )
	{
		Mesh = FbxMeshes.FindRef( StaticMesh );
	}

	if( !Mesh )
	{
		Mesh = FbxMesh::Create( TheScene, TCHAR_TO_UTF8( MeshName ) );

		const FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport( ExportLOD );

		// Verify the integrity of the static mesh.
		if( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0 )
		{
			return nullptr;
		}

		if( RenderMesh.Sections.Num() == 0 )
		{
			return nullptr;
		}

		// Remaps an Unreal vert to final reduced vertex list
		TArray<int32> VertRemap;
		TArray<int32> UniqueVerts;

		// Weld verts
		DetermineVertsToWeld( VertRemap, UniqueVerts, RenderMesh );

		// Create and fill in the vertex position data source.
		// The position vertices are duplicated, for some reason, retrieve only the first half vertices.
		const int32 VertexCount = VertRemap.Num();
		const int32 PolygonsCount = RenderMesh.Sections.Num();

		Mesh->InitControlPoints( UniqueVerts.Num() );

		FbxVector4* ControlPoints = Mesh->GetControlPoints();
		
		FRotator Rotator = FRotator::MakeFromEuler( FVector( 90, 90, 0 ) );//preview FBX the same as UE

		for( int32 PosIndex = 0; PosIndex < UniqueVerts.Num(); ++PosIndex )
		{
			int32 UnrealPosIndex = UniqueVerts[ PosIndex ];
			FVector Position = (FVector)RenderMesh.VertexBuffers.PositionVertexBuffer.VertexPosition( UnrealPosIndex );
			ControlPoints[ PosIndex ] = FbxVector4( Position.X, -Position.Y, Position.Z );
			if( CVarAssetsWithYUp.GetValueOnAnyThread() == 1 )
			{
				Position = Rotator.RotateVector( Position );
				ControlPoints[ PosIndex ] = FbxVector4( -Position.X, Position.Y, Position.Z );
			}
		}

		// Set the normals on Layer 0.
		FbxLayer* Layer = Mesh->GetLayer( 0 );
		if( Layer == nullptr )
		{
			Mesh->CreateLayer();
			Layer = Mesh->GetLayer( 0 );
		}

		// Build list of Indices re-used multiple times to lookup Normals, UVs, other per face vertex information
		TArray<uint32> Indices;
		for( int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex )
		{
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			const FStaticMeshSection& Polygons = RenderMesh.Sections[ PolygonsIndex ];
			const uint32 TriangleCount = Polygons.NumTriangles;
			for( uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex )
			{
				//for( int PointIndex = 2; PointIndex >=0; PointIndex-- )
				for( int PointIndex = 0; PointIndex < 3; PointIndex++ )
				{
					uint32 UnrealVertIndex = RawIndices[ Polygons.FirstIndex + ( ( TriangleIndex * 3 ) + PointIndex ) ];
					Indices.Add( UnrealVertIndex );
				}
			}
		}

		// Create and fill in the per-face-vertex normal data source.
		// We extract the Z-tangent and the X/Y-tangents which are also stored in the render mesh.
		FbxLayerElementNormal* LayerElementNormal = FbxLayerElementNormal::Create( Mesh, "" );
		FbxLayerElementTangent* LayerElementTangent = FbxLayerElementTangent::Create( Mesh, "" );
		FbxLayerElementBinormal* LayerElementBinormal = FbxLayerElementBinormal::Create( Mesh, "" );

		// Set 3 NTBs per triangle instead of storing on positional control points
		LayerElementNormal->SetMappingMode( FbxLayerElement::eByPolygonVertex );
		LayerElementTangent->SetMappingMode( FbxLayerElement::eByPolygonVertex );
		LayerElementBinormal->SetMappingMode( FbxLayerElement::eByPolygonVertex );

		// Set the NTBs values for every polygon vertex.
		LayerElementNormal->SetReferenceMode( FbxLayerElement::eDirect );
		LayerElementTangent->SetReferenceMode( FbxLayerElement::eDirect );
		LayerElementBinormal->SetReferenceMode( FbxLayerElement::eDirect );

		TArray<FbxVector4> FbxNormals;
		TArray<FbxVector4> FbxTangents;
		TArray<FbxVector4> FbxBinormals;

		FbxNormals.AddUninitialized( VertexCount );
		FbxTangents.AddUninitialized( VertexCount );
		FbxBinormals.AddUninitialized( VertexCount );

		for( int32 NTBIndex = 0; NTBIndex < VertexCount; ++NTBIndex )
		{
			FVector Normal = (FVector)( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( NTBIndex ) );
			if( CVarAssetsWithYUp.GetValueOnAnyThread() == 1 )
			{
				Normal.Y *= -1;
				Normal = Rotator.RotateVector( Normal );
			}
			FbxVector4& FbxNormal = FbxNormals[ NTBIndex ];
			FbxNormal = FbxVector4( Normal.X, Normal.Y, Normal.Z );
			FbxNormal.Normalize();

			FVector Tangent = (FVector)( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX( NTBIndex ) );
			if( CVarAssetsWithYUp.GetValueOnAnyThread() == 1 )
			{
				Tangent.Y *= -1;
				Tangent = Rotator.RotateVector( Tangent );
			}
			FbxVector4& FbxTangent = FbxTangents[ NTBIndex ];
			FbxTangent = FbxVector4( Tangent.X, Tangent.Y, Tangent.Z );
			FbxTangent.Normalize();

			FVector Binormal = -(FVector)( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY( NTBIndex ) );
			if( CVarAssetsWithYUp.GetValueOnAnyThread() == 1 )
			{
				Binormal.Y *= -1;
				Binormal = Rotator.RotateVector( Binormal );
			}
			FbxVector4& FbxBinormal = FbxBinormals[ NTBIndex ];
			FbxBinormal = FbxVector4( Binormal.X, Binormal.Y, Binormal.Z );
			FbxBinormal.Normalize();
		}

		// Add one normal per each face index (3 per triangle)
		for( int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++ )
		{
			uint32 UnrealVertIndex = Indices[ FbxVertIndex ];
			LayerElementNormal->GetDirectArray().Add( FbxNormals[ UnrealVertIndex ] );
			LayerElementTangent->GetDirectArray().Add( FbxTangents[ UnrealVertIndex ] );
			LayerElementBinormal->GetDirectArray().Add( FbxBinormals[ UnrealVertIndex ] );
		}

		Layer->SetNormals( LayerElementNormal );
		Layer->SetTangents( LayerElementTangent );
		Layer->SetBinormals( LayerElementBinormal );

		FbxNormals.Empty();
		FbxTangents.Empty();
		FbxBinormals.Empty();

		// Create and fill in the per-face-vertex texture coordinate data source(s).
		// Create UV for Diffuse channel.
		int32 TexCoordSourceCount = ( LightmapUVChannel == -1 ) ? RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() : LightmapUVChannel + 1;
		int32 TexCoordSourceIndex = ( LightmapUVChannel == -1 ) ? 0 : LightmapUVChannel;
		for( ; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex )
		{
			FbxLayer* UVsLayer = ( LightmapUVChannel == -1 ) ? Mesh->GetLayer( TexCoordSourceIndex ) : Mesh->GetLayer( 0 );
			if( UVsLayer == NULL )
			{
				Mesh->CreateLayer();
				UVsLayer = ( LightmapUVChannel == -1 ) ? Mesh->GetLayer( TexCoordSourceIndex ) : Mesh->GetLayer( 0 );
			}
			check( UVsLayer );

			FString UVChannelNameBuilder = TEXT( "UVmap_" ) + FString::FromInt( TexCoordSourceIndex );
			const char* UVChannelName = TCHAR_TO_UTF8( *UVChannelNameBuilder ); // actually UTF8 as required by Fbx, but can't use UE4's UTF8CHAR type because that's a uint8 aka *unsigned* char
			//if( ( LightmapUVChannel >= 0 ) || ( ( LightmapUVChannel == -1 ) && ( TexCoordSourceIndex == StaticMesh->GetLightMapCoordinateIndex() ) ) )
			//{
			//	UVChannelName = "LightMapUV";
			//}

			FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create( Mesh, UVChannelName );

			// Note: when eINDEX_TO_DIRECT is used, IndexArray must be 3xTriangle count, DirectArray can be smaller
			UVDiffuseLayer->SetMappingMode( FbxLayerElement::eByPolygonVertex );
			UVDiffuseLayer->SetReferenceMode( FbxLayerElement::eIndexToDirect );

			TArray<int32> UvsRemap;
			TArray<int32> UniqueUVs;
			// Weld UVs
			DetermineUVsToWeld( UvsRemap, UniqueUVs, RenderMesh.VertexBuffers.StaticMeshVertexBuffer, TexCoordSourceIndex );

			// Create the texture coordinate data source.
			for( int32 FbxVertIndex = 0; FbxVertIndex < UniqueUVs.Num(); FbxVertIndex++ )
			{
				int32 UnrealVertIndex = UniqueUVs[ FbxVertIndex ];
				#if ENGINE_MAJOR_VERSION == 4
					const FVector2D& TexCoord = RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( UnrealVertIndex, TexCoordSourceIndex );
				#else
					const FVector2f& TexCoord = RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( UnrealVertIndex, TexCoordSourceIndex );
				#endif
				//UVDiffuseLayer->GetDirectArray().Add( FbxVector2( TexCoord.X, TexCoord.Y ) );
				UVDiffuseLayer->GetDirectArray().Add( FbxVector2( TexCoord.X, -TexCoord.Y + 1.0 ) );//Why this Epic ?
			}

			// For each face index, point to a texture uv
			UVDiffuseLayer->GetIndexArray().SetCount( Indices.Num() );
			for( int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++ )
			{
				uint32 UnrealVertIndex = Indices[ FbxVertIndex ];
				int32 NewVertIndex = UvsRemap[ UnrealVertIndex ];
				UVDiffuseLayer->GetIndexArray().SetAt( FbxVertIndex, NewVertIndex );
			}

			UVsLayer->SetUVs( UVDiffuseLayer, FbxLayerElement::eTextureDiffuse );
		}

		FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create( Mesh, "" );
		MatLayer->SetMappingMode( FbxLayerElement::eByPolygon );
		MatLayer->SetReferenceMode( FbxLayerElement::eIndexToDirect );
		Layer->SetMaterials( MatLayer );

		// Keep track of the number of tri's we export
		uint32 AccountedTriangles = 0;
		for( int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex )
		{
			const FStaticMeshSection& Polygons = RenderMesh.Sections[ PolygonsIndex ];
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			UMaterialInterface* Material = nullptr;

			if( OverrideMaterials && OverrideMaterials->IsValidIndex( Polygons.MaterialIndex ) )
			{
				Material = ( *OverrideMaterials )[ Polygons.MaterialIndex ];
			}
			else
			{
				Material = StaticMesh->GetMaterial( Polygons.MaterialIndex );
			}

			MaterialBinding* Binding = GetMaterialIfAlreadyExported( Material );
			FbxSurfaceMaterial* FbxMaterial = nullptr;
			if( Binding )
				FbxMaterial = ExportMaterial_UTU( Material, Binding );
			else
				FbxMaterial = ExportMaterial( Material );
			//FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial( Material ) : NULL;
			if( !FbxMaterial )
			{
				FbxMaterial = CreateDefaultMaterial();
			}
			int ExistentMaterialIndex = -1;
			if( FbxMaterial )
			{
				ExistentMaterialIndex = FbxActor->GetMaterialIndex( FbxMaterial->GetName() );
			}
			if ( ExistentMaterialIndex == -1 )
				ExistentMaterialIndex = FbxActor->AddMaterial( FbxMaterial );

			// Determine the actual material index
			int32 ActualMatIndex = ExistentMaterialIndex;

			if( MaterialOrderOverride )
			{
				ActualMatIndex = MaterialOrderOverride->Find( Material );
			}
			// Static meshes contain one triangle list per element.
			// [GLAFORTE] Could it occasionally contain triangle strips? How do I know?
			uint32 TriangleCount = Polygons.NumTriangles;

			// Copy over the index buffer into the FBX polygons set.
			for( uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex )
			{
				Mesh->BeginPolygon( ActualMatIndex );
				//for( int PointIndex = 2; PointIndex >=0; PointIndex-- )
				for( int PointIndex = 0; PointIndex < 3; PointIndex++ )
				{
					uint32 OriginalUnrealVertIndex = RawIndices[ Polygons.FirstIndex + ( ( TriangleIndex * 3 ) + PointIndex ) ];
					int32 RemappedVertIndex = VertRemap[ OriginalUnrealVertIndex ];
					Mesh->AddPolygon( RemappedVertIndex );
				}
				Mesh->EndPolygon();
			}

			AccountedTriangles += TriangleCount;
		}

	#ifdef TODO_FBX
		// Throw a warning if this is a lightmap export and the exported poly count does not match the raw triangle data count
		if( LightmapUVChannel != -1 && AccountedTriangles != RenderMesh.RawTriangles.GetElementCount() )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT( "UnrealEd", "StaticMeshEditor_LightmapExportFewerTriangles", "Fewer polygons have been exported than the raw triangle count.  This Lightmapped UV mesh may contain fewer triangles than the destination mesh on import." ) );
		}

		// Create and fill in the smoothing data source.
		FbxLayerElementSmoothing* SmoothingInfo = FbxLayerElementSmoothing::Create( Mesh, "" );
		SmoothingInfo->SetMappingMode( FbxLayerElement::eByPolygon );
		SmoothingInfo->SetReferenceMode( FbxLayerElement::eDirect );
		FbxLayerElementArrayTemplate<int>& SmoothingArray = SmoothingInfo->GetDirectArray();
		Layer->SetSmoothing( SmoothingInfo );

		// This is broken. We are exporting the render mesh but providing smoothing
		// information from the source mesh. The render triangles are not in the
		// same order. Therefore we should export the raw mesh or not export
		// smoothing group information!
		int32 TriangleCount = RenderMesh.RawTriangles.GetElementCount();
		FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)RenderMesh.RawTriangles.Lock( LOCK_READ_ONLY );
		for( int32 TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++ )
		{
			FStaticMeshTriangle* Triangle = ( RawTriangleData++ );

			SmoothingArray.Add( Triangle->SmoothingMask );
		}
		RenderMesh.RawTriangles.Unlock();
	#endif // #if TODO_FBX

		// Create and fill in the vertex color data source.
		const FColorVertexBuffer* ColorBufferToUse = ColorBuffer ? ColorBuffer : &RenderMesh.VertexBuffers.ColorVertexBuffer;
		uint32 ColorVertexCount = ColorBufferToUse->GetNumVertices();

		// Only export vertex colors if they exist
		if( Exporter->GetExportOptions()->VertexColor && ColorVertexCount > 0 )
		{
			FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create( Mesh, "" );
			VertexColor->SetMappingMode( FbxLayerElement::eByPolygonVertex );
			VertexColor->SetReferenceMode( FbxLayerElement::eIndexToDirect );
			FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
			Layer->SetVertexColors( VertexColor );

			for( int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++ )
			{
				FLinearColor VertColor( 1.0f, 1.0f, 1.0f );
				uint32 UnrealVertIndex = Indices[ FbxVertIndex ];
				if( UnrealVertIndex < ColorVertexCount )
				{
					VertColor = ColorBufferToUse->VertexColor( UnrealVertIndex ).ReinterpretAsLinear();
				}

				VertexColorArray.Add( FbxColor( VertColor.R, VertColor.G, VertColor.B, VertColor.A ) );
			}

			VertexColor->GetIndexArray().SetCount( Indices.Num() );
			for( int32 FbxVertIndex = 0; FbxVertIndex < Indices.Num(); FbxVertIndex++ )
			{
				VertexColor->GetIndexArray().SetAt( FbxVertIndex, FbxVertIndex );
			}
		}

		if( ( ExportLOD == 0 || ExportLOD == -1 ) && LightmapUVChannel == -1 && ColorBuffer == nullptr && MaterialOrderOverride == nullptr )
		{
			FbxMeshes.Add( StaticMesh, Mesh );
		}
	}
	else
	{
		//Materials in fbx are store in the node and not in the mesh, so even if the mesh was already export
		//we have to find and assign the mesh material.
		const FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport( ExportLOD );
		const int32 PolygonsCount = RenderMesh.Sections.Num();
		uint32 AccountedTriangles = 0;
		for( int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex )
		{
			const FStaticMeshSection& Polygons = RenderMesh.Sections[ PolygonsIndex ];
			FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
			UMaterialInterface* Material = nullptr;

			if( OverrideMaterials && OverrideMaterials->IsValidIndex( Polygons.MaterialIndex ) )
			{
				Material = ( *OverrideMaterials )[ Polygons.MaterialIndex ];
			}
			else
			{
				Material = StaticMesh->GetMaterial( Polygons.MaterialIndex );
			}

			FbxSurfaceMaterial* FbxMaterial =  Material ? ExportMaterial( Material ) : NULL;
			if( !FbxMaterial )
			{
				FbxMaterial = CreateDefaultMaterial();
			}
			FbxActor->AddMaterial( FbxMaterial );
		}
	}

#if (WITH_PHYSX && PHYSICS_INTERFACE_PHYSX) || WITH_CHAOS
	if( ( ExportLOD == 0 || ExportLOD == -1 ) && Exporter->GetExportOptions()->Collision )
	{
		ExportCollisionMesh( Exporter, StaticMesh, MeshName, FbxActor );
	}
#endif


	//Set the original meshes in case it was already existing
	FbxActor->SetNodeAttribute( Mesh );

	//ExportObjectMetadata( StaticMesh, FbxActor );

	return FbxActor;
}
FbxNode* FindActor( AActor* Actor, INodeNameAdapter* NodeNameAdapter )
{
	if( NodeNameAdapter )
	{
		FbxNode* ActorNode = NodeNameAdapter->GetFbxNode( Actor );

		if( ActorNode )
		{
			return ActorNode;
		}
	}

	if( FbxActors.Find( Actor ) )
	{
		return *FbxActors.Find( Actor );
	}
	else
	{
		return NULL;
	}
}
FString GetFbxObjectName( const FString& FbxObjectNode, INodeNameAdapter& NodeNameAdapter )
{
	FString FbxTestName = FbxObjectNode;
	int32* NodeIndex = FbxNodeNameToIndexMap.Find( FbxTestName );
	if( NodeIndex )
	{
		FbxTestName = FString::Printf( TEXT( "%s%d" ), *FbxTestName, *NodeIndex );
		++( *NodeIndex );
	}
	else
	{
		FbxNodeNameToIndexMap.Add( FbxTestName, 1 );
	}
	return FbxTestName;
}
FbxNode* CreateSkeleton( const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes )
{
	#if ENGINE_MINOR_VERSION >= 27
	const FReferenceSkeleton& RefSkeleton = SkelMesh->GetRefSkeleton();
	#else
	const FReferenceSkeleton& RefSkeleton = SkelMesh->RefSkeleton;
	#endif
	

	if( RefSkeleton.GetRawBoneNum() == 0 )
	{
		return NULL;
	}

	// Create a list of the nodes we create for each bone, so that children can 
	// later look up their parent
	BoneNodes.Reserve( RefSkeleton.GetRawBoneNum() );

	for( int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetRawBoneNum(); ++BoneIndex )
	{
		const FMeshBoneInfo& CurrentBone = RefSkeleton.GetRefBoneInfo()[ BoneIndex ];
		const FTransform& BoneTransform = RefSkeleton.GetRefBonePose()[ BoneIndex ];

		FbxString BoneName = Converter.ConvertToFbxString( CurrentBone.ExportName );


		// Create the node's attributes
		FbxSkeleton* SkeletonAttribute = FbxSkeleton::Create( TheScene, BoneName.Buffer() );
		if( BoneIndex )
		{
			SkeletonAttribute->SetSkeletonType( FbxSkeleton::eLimbNode );
			//SkeletonAttribute->Size.Set(1.0);
		}
		else
		{
			SkeletonAttribute->SetSkeletonType( FbxSkeleton::eRoot );
			//SkeletonAttribute->Size.Set(1.0);
		}


		// Create the node
		FbxNode* BoneNode = FbxNode::Create( TheScene, BoneName.Buffer() );
		BoneNode->SetNodeAttribute( SkeletonAttribute );

		// Set the bone node's local orientation
		FVector UnrealRotation = BoneTransform.GetRotation().Euler();
		FbxVector4 LocalPos = Converter.ConvertToFbxPos( BoneTransform.GetTranslation() );
		FbxVector4 LocalRot = Converter.ConvertToFbxRot( UnrealRotation );
		FbxVector4 LocalScale = Converter.ConvertToFbxScale( BoneTransform.GetScale3D() );

		BoneNode->LclTranslation.Set( LocalPos );
		BoneNode->LclRotation.Set( LocalRot );
		BoneNode->LclScaling.Set( LocalScale );


		// If this is not the root bone, attach it to its parent
		if( BoneIndex )
		{
			BoneNodes[ CurrentBone.ParentIndex ]->AddChild( BoneNode );
		}


		// Add the node to the list of nodes, in bone order
		BoneNodes.Push( BoneNode );
	}

	return BoneNodes[ 0 ];
}
bool SetupAnimStack( const UAnimSequence* AnimSeq )
{
	if( AnimSeq->SequenceLength == 0.f )
	{
		// something is wrong
		return false;
	}

	const float FrameRate = FMath::TruncToFloat( ( ( AnimSeq->GetRawNumberOfFrames() - 1 ) / AnimSeq->SequenceLength ) + 0.5f );
	//Configure the scene time line
	{
		FbxGlobalSettings& SceneGlobalSettings = TheScene->GetGlobalSettings();
		double CurrentSceneFrameRate = FbxTime::GetFrameRate( SceneGlobalSettings.GetTimeMode() );
		if( !bSceneGlobalTimeLineSet || FrameRate > CurrentSceneFrameRate )
		{
			FbxTime::EMode ComputeTimeMode = FbxTime::ConvertFrameRateToTimeMode( FrameRate );
			FbxTime::SetGlobalTimeMode( ComputeTimeMode, ComputeTimeMode == FbxTime::eCustom ? FrameRate : 0.0 );
			SceneGlobalSettings.SetTimeMode( ComputeTimeMode );
			if( ComputeTimeMode == FbxTime::eCustom )
			{
				SceneGlobalSettings.SetCustomFrameRate( FrameRate );
			}
			bSceneGlobalTimeLineSet = true;
		}
	}

	// set time correctly
	FbxTime ExportedStartTime, ExportedStopTime;
	ExportedStartTime.SetSecondDouble( 0.f );
	ExportedStopTime.SetSecondDouble( AnimSeq->SequenceLength );

	FbxTimeSpan ExportedTimeSpan;
	ExportedTimeSpan.Set( ExportedStartTime, ExportedStopTime );
	AnimStack->SetLocalTimeSpan( ExportedTimeSpan );

	return true;
}
void IterateInsideAnimSequence( const UAnimSequence* AnimSeq, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime, TFunctionRef<void( float, FbxTime, bool )> IterationLambda )
{
	float AnimTime = AnimStartOffset;
	float AnimEndTime = ( AnimSeq->SequenceLength - AnimEndOffset );
	// Subtracts 1 because NumFrames includes an initial pose for 0.0 second
	double TimePerKey = ( AnimSeq->SequenceLength / ( AnimSeq->GetRawNumberOfFrames() - 1 ) );
	const float AnimTimeIncrement = TimePerKey * AnimPlayRate;
	uint32 AnimFrameIndex = 0;

	FbxTime ExportTime;
	ExportTime.SetSecondDouble( StartTime );

	FbxTime ExportTimeIncrement;
	ExportTimeIncrement.SetSecondDouble( TimePerKey );

	// Step through each frame and add custom curve data
	bool bLastKey = false;
	while( !bLastKey )
	{
		bLastKey = ( AnimTime + KINDA_SMALL_NUMBER ) > AnimEndTime;

		IterationLambda( AnimTime, ExportTime, bLastKey );

		ExportTime += ExportTimeIncrement;
		AnimFrameIndex++;
		AnimTime = AnimStartOffset + ( (float)AnimFrameIndex * AnimTimeIncrement );
	}
}
void ExportCustomAnimCurvesToFbx( const TMap<FName, FbxAnimCurve*>& CustomCurves, const UAnimSequence* AnimSeq,
												float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime, float ValueScale = 1.f )
{
	// stack allocator for extracting curve
	FMemMark Mark( FMemStack::Get() );
	const USkeleton* Skeleton = AnimSeq->GetSkeleton();
	const FSmartNameMapping* SmartNameMapping = Skeleton ? Skeleton->GetSmartNameContainer( USkeleton::AnimCurveMappingName ) : nullptr;

	if( !Skeleton || !SmartNameMapping || !SetupAnimStack( AnimSeq ) )
	{
		//Something is wrong.
		return;
	}

	TArray<SmartName::UID_Type> AnimCurveUIDs;
	{
		//We need to recreate the UIDs array manually so that we keep the empty entries otherwise the BlendedCurve won't have the correct mapping.
		TArray<FName> UID_ToNameArray;
		SmartNameMapping->FillUIDToNameArray( UID_ToNameArray );
		AnimCurveUIDs.Reserve( UID_ToNameArray.Num() );
		for( int32 NameIndex = 0; NameIndex < UID_ToNameArray.Num(); ++NameIndex )
		{
			AnimCurveUIDs.Add( NameIndex );
		}
	}

	for( auto CustomCurve : CustomCurves )
	{
		CustomCurve.Value->KeyModifyBegin();
	}

	auto ExportLambda = [&]( float AnimTime, FbxTime ExportTime, bool bLastKey )
	{
		FBlendedCurve BlendedCurve;
		BlendedCurve.InitFrom( &AnimCurveUIDs );
		AnimSeq->EvaluateCurveData( BlendedCurve, AnimTime, true );
		if( BlendedCurve.IsValid() )
		{
			//Loop over the custom curves and add the actual keys
			for( auto CustomCurve : CustomCurves )
			{
				SmartName::UID_Type NameUID = Skeleton->GetUIDByName( USkeleton::AnimCurveMappingName, CustomCurve.Key );
				if( NameUID != SmartName::MaxUID )
				{
					float CurveValueAtTime = BlendedCurve.Get( NameUID ) * ValueScale;
					int32 KeyIndex = CustomCurve.Value->KeyAdd( ExportTime );
					CustomCurve.Value->KeySetValue( KeyIndex, CurveValueAtTime );
				}
			}
		}
	};

	IterateInsideAnimSequence( AnimSeq, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime, ExportLambda );

	for( auto CustomCurve : CustomCurves )
	{
		CustomCurve.Value->KeyModifyEnd();
	}
}
void ExportAnimSequenceToFbx( FFbxExporter* Exporter, const UAnimSequence* AnimSeq,
											const USkeletalMesh* SkelMesh,
											TArray<FbxNode*>& BoneNodes,
											FbxAnimLayer* InAnimLayer,
											float AnimStartOffset,
											float AnimEndOffset,
											float AnimPlayRate,
											float StartTime )
{
	// stack allocator for extracting curve
	FMemMark Mark( FMemStack::Get() );

	USkeleton* Skeleton = AnimSeq->GetSkeleton();

	if( Skeleton == nullptr || !SetupAnimStack( AnimSeq ) )
	{
		// something is wrong
		return;
	}

	//Prepare root anim curves data to be exported
	TArray<FName> AnimCurveNames;
	TMap<FName, FbxAnimCurve*> CustomCurveMap;
	if( BoneNodes.Num() > 0 )
	{
		const FSmartNameMapping* AnimCurveMapping = Skeleton->GetSmartNameContainer( USkeleton::AnimCurveMappingName );

		if( AnimCurveMapping )
		{
			AnimCurveMapping->FillNameArray( AnimCurveNames );

			const UFbxExportOption* ExportOptions = Exporter->GetExportOptions();
			const bool bExportMorphTargetCurvesInMesh = ExportOptions && ExportOptions->bExportPreviewMesh && ExportOptions->bExportMorphTargets;

			for( auto AnimCurveName : AnimCurveNames )
			{
				const FCurveMetaData* CurveMetaData = AnimCurveMapping->GetCurveMetaData( AnimCurveName );

				//Only export the custom curve if it is not used in a MorphTarget that will be exported latter on.
				if( !( bExportMorphTargetCurvesInMesh && CurveMetaData && CurveMetaData->Type.bMorphtarget ) )
				{
					FbxProperty AnimCurveFbxProp = FbxProperty::Create( BoneNodes[ 0 ], FbxDoubleDT, TCHAR_TO_ANSI( *AnimCurveName.ToString() ) );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
					AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
					CustomCurveMap.Add( AnimCurveName, AnimFbxCurve );
				}
			}
		}
	}

	ExportCustomAnimCurvesToFbx( CustomCurveMap, AnimSeq, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime );

	TArray<FCustomAttribute> CustomAttributes;

	// Add the animation data to the bone nodes
	for( int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex )
	{
		FbxNode* CurrentBoneNode = BoneNodes[ BoneIndex ];
		int32 BoneTreeIndex = Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex( SkelMesh, BoneIndex );
		int32 BoneTrackIndex = Skeleton->GetRawAnimationTrackIndex( BoneTreeIndex, AnimSeq );
		FName BoneName = Skeleton->GetReferenceSkeleton().GetBoneName( BoneTreeIndex );

		CustomAttributes.Reset();
		AnimSeq->GetCustomAttributesForBone( BoneName, CustomAttributes );

		TArray<TPair<int32, FbxAnimCurve*>> FloatCustomAttributeIndices;
		TArray<TPair<int32, FbxAnimCurve*>> IntCustomAttributeIndices;

		// Setup custom attribute properties and curves
		for( int32 AttributeIndex = 0; AttributeIndex < CustomAttributes.Num(); ++AttributeIndex )
		{
			const FCustomAttribute& Attribute = CustomAttributes[ AttributeIndex ];
			const FName& AttributeName = Attribute.Name;

			const EVariantTypes VariantType = static_cast<EVariantTypes>( Attribute.VariantType );

			if( VariantType == EVariantTypes::Int32 )
			{
				FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxIntDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

				FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
				AnimFbxCurve->KeyModifyBegin();
				IntCustomAttributeIndices.Emplace( AttributeIndex, AnimFbxCurve );
			}
			else if( VariantType == EVariantTypes::Float )
			{
				FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxFloatDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eAnimatable, true );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

				FbxAnimCurve* AnimFbxCurve = AnimCurveFbxProp.GetCurve( InAnimLayer, true );
				AnimFbxCurve->KeyModifyBegin();
				FloatCustomAttributeIndices.Emplace( AttributeIndex, AnimFbxCurve );
			}
			else if( VariantType == EVariantTypes::String )
			{
				FbxProperty AnimCurveFbxProp = FbxProperty::Create( CurrentBoneNode, FbxStringDT, TCHAR_TO_UTF8( *AttributeName.ToString() ) );
				AnimCurveFbxProp.ModifyFlag( FbxPropertyFlags::eUserDefined, true );

				// String attributes can't be keyed, simply set a normal value.
				FString AttributeValue;
				#if ENGINE_MAJOR_VERSION == 4
				FCustomAttributesRuntime::GetAttributeValue( Attribute, 0.f, AttributeValue );
				#endif
				FbxString FbxValueString( TCHAR_TO_UTF8( *AttributeValue ) );
				AnimCurveFbxProp.Set( FbxValueString );
			}
			else
			{
				ensureMsgf( false, TEXT( "Trying to export unsupported custom attribte (float, int32 and FString are currently supported)" ) );
			}
		}

		// Create the transform AnimCurves
		const uint32 NumberOfCurves = 9;
		FbxAnimCurve* Curves[ NumberOfCurves ];

		// Individual curves for translation, rotation and scaling
		Curves[ 0 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 1 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 2 ] = CurrentBoneNode->LclTranslation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		Curves[ 3 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 4 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 5 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		Curves[ 6 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 7 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 8 ] = CurrentBoneNode->LclScaling.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		if( BoneTrackIndex == INDEX_NONE )
		{
			// If this sequence does not have a track for the current bone, then skip it
			continue;
		}

		for( FbxAnimCurve* Curve : Curves )
		{
			Curve->KeyModifyBegin();
		}

		auto ExportLambda = [&]( float AnimTime, FbxTime ExportTime, bool bLastKey )
		{
			FTransform BoneAtom;
			AnimSeq->GetBoneTransform( BoneAtom, BoneTrackIndex, AnimTime, true );
			FbxAMatrix FbxMatrix = Converter.ConvertMatrix( BoneAtom.ToMatrixWithScale() );

			FbxVector4 Translation = FbxMatrix.GetT();
			FbxVector4 Rotation = FbxMatrix.GetR();
			FbxVector4 Scale = FbxMatrix.GetS();
			FbxVector4 Vectors[ 3 ] = { Translation, Rotation, Scale };

			// Loop over each curve and channel to set correct values
			for( uint32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex )
			{
				for( uint32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex )
				{
					uint32 OffsetCurveIndex = ( CurveIndex * 3 ) + ChannelIndex;

					int32 lKeyIndex = Curves[ OffsetCurveIndex ]->KeyAdd( ExportTime );
					Curves[ OffsetCurveIndex ]->KeySetValue( lKeyIndex, Vectors[ CurveIndex ][ ChannelIndex ] );
					Curves[ OffsetCurveIndex ]->KeySetInterpolation( lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic );

					if( bLastKey )
					{
						Curves[ OffsetCurveIndex ]->KeySetConstantMode( lKeyIndex, FbxAnimCurveDef::eConstantStandard );
					}
				}
			}

			#if ENGINE_MAJOR_VERSION == 4
			for( TPair<int32, FbxAnimCurve*>& CurrentAttributeCurve : FloatCustomAttributeIndices )
			{
				float AttributeValue = 0.f;
				FCustomAttributesRuntime::GetAttributeValue( CustomAttributes[ CurrentAttributeCurve.Key ], AnimTime, AttributeValue );
				int32 KeyIndex = CurrentAttributeCurve.Value->KeyAdd( ExportTime );
				CurrentAttributeCurve.Value->KeySetValue( KeyIndex, AttributeValue );
			}

			for( TPair<int32, FbxAnimCurve*>& CurrentAttributeCurve : IntCustomAttributeIndices )
			{
				int32 AttributeValue = 0;
				FCustomAttributesRuntime::GetAttributeValue( CustomAttributes[ CurrentAttributeCurve.Key ], AnimTime, AttributeValue );
				int32 KeyIndex = CurrentAttributeCurve.Value->KeyAdd( ExportTime );
				CurrentAttributeCurve.Value->KeySetValue( KeyIndex, static_cast<float>( AttributeValue ) );
			}
			#endif
		};

		IterateInsideAnimSequence( AnimSeq, AnimStartOffset, AnimEndOffset, AnimPlayRate, StartTime, ExportLambda );

		for( FbxAnimCurve* Curve : Curves )
		{
			Curve->KeyModifyEnd();
		}

		auto MarkCurveEnd = []( auto& CurvesArray )
		{
			for( auto& CurvePair : CurvesArray )
			{
				CurvePair.Value->KeyModifyEnd();
			}
		};

		MarkCurveEnd( FloatCustomAttributeIndices );
		MarkCurveEnd( IntCustomAttributeIndices );
	}
}
void CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* InAnimLayer )
{
	// Add the animation data to the bone nodes
	for( int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex )
	{
		FbxNode* CurrentBoneNode = BoneNodes[ BoneIndex ];

		// Fetch the AnimCurves
		FbxAnimCurve* Curves[ 3 ];
		Curves[ 0 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true );
		Curves[ 1 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true );
		Curves[ 2 ] = CurrentBoneNode->LclRotation.GetCurve( InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true );

		for( int32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex )
		{
			FbxAnimCurve* CurrentCurve = Curves[ CurveIndex ];
			CurrentCurve->KeyModifyBegin();

			float CurrentAngleOffset = 0.f;
			for( int32 KeyIndex = 1; KeyIndex < CurrentCurve->KeyGetCount(); ++KeyIndex )
			{
				float PreviousOutVal = CurrentCurve->KeyGetValue( KeyIndex - 1 );
				float CurrentOutVal = CurrentCurve->KeyGetValue( KeyIndex );

				float DeltaAngle = ( CurrentOutVal + CurrentAngleOffset ) - PreviousOutVal;

				if( DeltaAngle >= 180 )
				{
					CurrentAngleOffset -= 360;
				}
				else if( DeltaAngle <= -180 )
				{
					CurrentAngleOffset += 360;
				}

				CurrentOutVal += CurrentAngleOffset;

				CurrentCurve->KeySetValue( KeyIndex, CurrentOutVal );
			}

			CurrentCurve->KeyModifyEnd();
		}
	}
}
void BindMeshToSkeleton( const USkeletalMesh* SkelMesh, FbxNode* MeshRootNode, TArray<FbxNode*>& BoneNodes, int32 LODIndex )
{
	const FSkeletalMeshModel* SkelMeshResource = SkelMesh->GetImportedModel();
	if( !SkelMeshResource->LODModels.IsValidIndex( LODIndex ) )
	{
		//We cannot bind the LOD if its not valid
		return;
	}
	const FSkeletalMeshLODModel& SourceModel = SkelMeshResource->LODModels[ LODIndex ];
	const int32 VertexCount = SourceModel.NumVertices;

	FbxAMatrix MeshMatrix;

	FbxScene* lScene = MeshRootNode->GetScene();
	if( lScene )
	{
		MeshMatrix = MeshRootNode->EvaluateGlobalTransform();
	}

	FbxGeometry* MeshAttribute = (FbxGeometry*)MeshRootNode->GetNodeAttribute();
	FbxSkin* Skin = FbxSkin::Create( TheScene, "" );

	const int32 BoneCount = BoneNodes.Num();
	for( int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex )
	{
		FbxNode* BoneNode = BoneNodes[ BoneIndex ];

		// Create the deforming cluster
		FbxCluster* CurrentCluster = FbxCluster::Create( TheScene, "" );
		CurrentCluster->SetLink( BoneNode );
		CurrentCluster->SetLinkMode( FbxCluster::eTotalOne );

		// Add all the vertices that are weighted to the current skeletal bone to the cluster
		// NOTE: the bone influence indices contained in the vertex data are based on a per-chunk
		// list of verts.  The convert the chunk bone index to the mesh bone index, the chunk's
		// boneMap is needed
		int32 VertIndex = 0;
		const int32 SectionCount = SourceModel.Sections.Num();
		for( int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex )
		{
			const FSkelMeshSection& Section = SourceModel.Sections[ SectionIndex ];
			if( Section.HasClothingData() )
			{
				continue;
			}

			for( int32 SoftIndex = 0; SoftIndex < Section.SoftVertices.Num(); ++SoftIndex )
			{
				const FSoftSkinVertex& Vert = Section.SoftVertices[ SoftIndex ];

				for( int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex )
				{
					int32 InfluenceBone = Section.BoneMap[ Vert.InfluenceBones[ InfluenceIndex ] ];
					float InfluenceWeight = Vert.InfluenceWeights[ InfluenceIndex ] / 255.f;

					if( InfluenceBone == BoneIndex && InfluenceWeight > 0.f )
					{
						CurrentCluster->AddControlPointIndex( VertIndex, InfluenceWeight );
					}
				}

				++VertIndex;
			}
		}

		// Now we have the Patch and the skeleton correctly positioned,
		// set the Transform and TransformLink matrix accordingly.
		CurrentCluster->SetTransformMatrix( MeshMatrix );

		FbxAMatrix LinkMatrix;
		if( lScene )
		{
			LinkMatrix = BoneNode->EvaluateGlobalTransform();
		}

		CurrentCluster->SetTransformLinkMatrix( LinkMatrix );

		// Add the clusters to the mesh by creating a skin and adding those clusters to that skin.
		Skin->AddCluster( CurrentCluster );
	}

	// Add the skin to the mesh after the clusters have been added
	MeshAttribute->AddDeformer( Skin );
}
void AddNodeRecursively( FbxArray<FbxNode*>& pNodeArray, FbxNode* pNode )
{
	if( pNode )
	{
		AddNodeRecursively( pNodeArray, pNode->GetParent() );

		if( pNodeArray.Find( pNode ) == -1 )
		{
			// Node not in the list, add it
			pNodeArray.Add( pNode );
		}
	}
}
void CreateBindPose( FbxNode* MeshRootNode )
{
	if( !MeshRootNode )
	{
		return;
	}

	// In the bind pose, we must store all the link's global matrix at the time of the bind.
	// Plus, we must store all the parent(s) global matrix of a link, even if they are not
	// themselves deforming any model.

	// Create a bind pose with the link list

	FbxArray<FbxNode*> lClusteredFbxNodes;
	int                       i, j;

	if( MeshRootNode->GetNodeAttribute() )
	{
		int lSkinCount = 0;
		int lClusterCount = 0;
		switch( MeshRootNode->GetNodeAttribute()->GetAttributeType() )
		{
			case FbxNodeAttribute::eMesh:
			case FbxNodeAttribute::eNurbs:
			case FbxNodeAttribute::ePatch:

				lSkinCount = ( (FbxGeometry*)MeshRootNode->GetNodeAttribute() )->GetDeformerCount( FbxDeformer::eSkin );
				//Go through all the skins and count them
				//then go through each skin and get their cluster count
				for( i = 0; i < lSkinCount; ++i )
				{
					FbxSkin* lSkin = (FbxSkin*)( (FbxGeometry*)MeshRootNode->GetNodeAttribute() )->GetDeformer( i, FbxDeformer::eSkin );
					lClusterCount += lSkin->GetClusterCount();
				}
				break;
		}
		//if we found some clusters we must add the node
		if( lClusterCount )
		{
			//Again, go through all the skins get each cluster link and add them
			for( i = 0; i < lSkinCount; ++i )
			{
				FbxSkin* lSkin = (FbxSkin*)( (FbxGeometry*)MeshRootNode->GetNodeAttribute() )->GetDeformer( i, FbxDeformer::eSkin );
				lClusterCount = lSkin->GetClusterCount();
				for( j = 0; j < lClusterCount; ++j )
				{
					FbxNode* lClusterNode = lSkin->GetCluster( j )->GetLink();
					AddNodeRecursively( lClusteredFbxNodes, lClusterNode );
				}

			}

			// Add the patch to the pose
			lClusteredFbxNodes.Add( MeshRootNode );
		}
	}

	// Now create a bind pose with the link list
	if( lClusteredFbxNodes.GetCount() )
	{
		// A pose must be named. Arbitrarily use the name of the patch node.
		FbxPose* lPose = FbxPose::Create( TheScene, MeshRootNode->GetName() );

		// default pose type is rest pose, so we need to set the type as bind pose
		lPose->SetIsBindPose( true );

		for( i = 0; i < lClusteredFbxNodes.GetCount(); i++ )
		{
			FbxNode* lKFbxNode = lClusteredFbxNodes.GetAt( i );
			FbxMatrix lBindMatrix = lKFbxNode->EvaluateGlobalTransform();

			lPose->Add( lKFbxNode, lBindMatrix );
		}

		// Add the pose to the scene
		TheScene->AddPose( lPose );
	}
}
FbxSurfaceMaterial* ExportMaterial( UMaterialInterface* MaterialInterface );\
FbxNode* CreateMesh( FFbxExporter* Exporter, const USkeletalMesh* SkelMesh, const TCHAR* MeshName, int32 LODIndex, const UAnimSequence* AnimSeq /*= nullptr*/, const TArray<UMaterialInterface*>* OverrideMaterials /*= nullptr*/ )
{
	const FSkeletalMeshModel* SkelMeshResource = SkelMesh->GetImportedModel();
	if( !SkelMeshResource->LODModels.IsValidIndex( LODIndex ) )
	{
		//Return an empty node
		return FbxNode::Create( TheScene, TCHAR_TO_UTF8( MeshName ) );
	}

	const FSkeletalMeshLODModel& SourceModel = SkelMeshResource->LODModels[ LODIndex ];
	const int32 VertexCount = SourceModel.GetNumNonClothingVertices();

	// Verify the integrity of the mesh.
	if( VertexCount == 0 ) return NULL;

	// Copy all the vertex data from the various chunks to a single buffer.
	// Makes the rest of the code in this function cleaner and easier to maintain.  
	TArray<FSoftSkinVertex> Vertices;
	SourceModel.GetNonClothVertices( Vertices );
	if( Vertices.Num() != VertexCount ) return NULL;

	FbxMesh* Mesh = FbxMesh::Create( TheScene, TCHAR_TO_UTF8( MeshName ) );

	// Create and fill in the vertex position data source.
	Mesh->InitControlPoints( VertexCount );
	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	for( int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex )
	{
		FVector Position = (FVector)Vertices[ VertIndex ].Position;
		ControlPoints[ VertIndex ] = Converter.ConvertToFbxPos( Position );
	}

	// Create Layer 0 to hold the normals
	FbxLayer* LayerZero = Mesh->GetLayer( 0 );
	if( LayerZero == NULL )
	{
		Mesh->CreateLayer();
		LayerZero = Mesh->GetLayer( 0 );
	}

	// Create and fill in the per-face-vertex normal data source.
	// We extract the Z-tangent and drop the X/Y-tangents which are also stored in the render mesh.
	FbxLayerElementNormal* LayerElementNormal = FbxLayerElementNormal::Create( Mesh, "" );

	LayerElementNormal->SetMappingMode( FbxLayerElement::eByControlPoint );
	// Set the normal values for every control point.
	LayerElementNormal->SetReferenceMode( FbxLayerElement::eDirect );

	for( int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex )
	{
		FVector Normal;
		#if ENGINE_MAJOR_VERSION >= 5
			FVector4f Normal4 = Vertices[ VertIndex ].TangentZ;
			Normal = FVector( Normal4.X, Normal4.Y, Normal4.Z );
		#else
			Normal = Vertices[ VertIndex ].TangentZ;
		#endif
		FbxVector4 FbxNormal = Converter.ConvertToFbxPos( Normal );

		LayerElementNormal->GetDirectArray().Add( FbxNormal );
	}

	LayerZero->SetNormals( LayerElementNormal );


	// Create and fill in the per-face-vertex texture coordinate data source(s).
	// Create UV for Diffuse channel.
	const int32 TexCoordSourceCount = SourceModel.NumTexCoords;
	TCHAR UVChannelName[ 32 ];
	for( int32 TexCoordSourceIndex = 0; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex )
	{
		FbxLayer* Layer = Mesh->GetLayer( TexCoordSourceIndex );
		if( Layer == NULL )
		{
			Mesh->CreateLayer();
			Layer = Mesh->GetLayer( TexCoordSourceIndex );
		}

		if( TexCoordSourceIndex == 1 )
		{
			FCString::Sprintf( UVChannelName, TEXT( "LightMapUV" ) );
		}
		else
		{
			FCString::Sprintf( UVChannelName, TEXT( "DiffuseUV" ) );
		}

		FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create( Mesh, TCHAR_TO_UTF8( UVChannelName ) );
		UVDiffuseLayer->SetMappingMode( FbxLayerElement::eByControlPoint );
		UVDiffuseLayer->SetReferenceMode( FbxLayerElement::eDirect );

		// Create the texture coordinate data source.
		for( int32 TexCoordIndex = 0; TexCoordIndex < VertexCount; ++TexCoordIndex )
		{
			#if ENGINE_MAJOR_VERSION == 4
			const FVector2D& TexCoord = Vertices[ TexCoordIndex ].UVs[ TexCoordSourceIndex ];
			#else
			const FVector2f& TexCoord = Vertices[ TexCoordIndex ].UVs[ TexCoordSourceIndex ];
			#endif
			UVDiffuseLayer->GetDirectArray().Add( FbxVector2( TexCoord.X, -TexCoord.Y + 1.0 ) );
		}

		Layer->SetUVs( UVDiffuseLayer, FbxLayerElement::eTextureDiffuse );
	}

	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create( Mesh, "" );
	MatLayer->SetMappingMode( FbxLayerElement::eByPolygon );
	MatLayer->SetReferenceMode( FbxLayerElement::eIndexToDirect );
	LayerZero->SetMaterials( MatLayer );


	// Create the per-material polygons sets.
	int32 SectionCount = SourceModel.Sections.Num();
	int32 ClothSectionVertexRemoveOffset = 0;
	TArray<TPair<uint32, uint32>> VertexIndexOffsetPairArray{ TPair<uint32, uint32>( 0,0 ) };
	for( int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex )
	{
		const FSkelMeshSection& Section = SourceModel.Sections[ SectionIndex ];
		if( Section.HasClothingData() )
		{
			ClothSectionVertexRemoveOffset += Section.GetNumVertices();
			VertexIndexOffsetPairArray.Emplace( Section.BaseVertexIndex, ClothSectionVertexRemoveOffset );
			continue;
		}
		int32 MatIndex = Section.MaterialIndex;

		// Static meshes contain one triangle list per element.
		int32 TriangleCount = Section.NumTriangles;

		// Copy over the index buffer into the FBX polygons set.
		for( int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex )
		{
			Mesh->BeginPolygon( MatIndex );
			for( int32 PointIndex = 0; PointIndex < 3; PointIndex++ )
			{
				int32 VertexPositionIndex = SourceModel.IndexBuffer[ Section.BaseIndex + ( ( TriangleIndex * 3 ) + PointIndex ) ] - ClothSectionVertexRemoveOffset;
				check( VertexPositionIndex >= 0 );
				Mesh->AddPolygon( VertexPositionIndex );
			}
			Mesh->EndPolygon();
		}
	}

	if( Exporter->GetExportOptions()->VertexColor )
	{
		// Create and fill in the vertex color data source.
		FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create( Mesh, "" );
		VertexColor->SetMappingMode( FbxLayerElement::eByControlPoint );
		VertexColor->SetReferenceMode( FbxLayerElement::eDirect );
		FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
		LayerZero->SetVertexColors( VertexColor );

		for( int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex )
		{
			FLinearColor VertColor = Vertices[ VertIndex ].Color.ReinterpretAsLinear();
			VertexColorArray.Add( FbxColor( VertColor.R, VertColor.G, VertColor.B, VertColor.A ) );
		}
	}


	if( Exporter->GetExportOptions()->bExportMorphTargets && GetSkeleton(SkelMesh) ) //The skeleton can be null if this is a destructible mesh.
	{
		const FSmartNameMapping* SmartNameMapping = GetSkeleton( SkelMesh )->GetSmartNameContainer( USkeleton::AnimCurveMappingName );
		TMap<FName, FbxAnimCurve*> BlendShapeCurvesMap;

		if( GetMorphTargets( SkelMesh ).Num() )
		{
			// The original BlendShape Name was not saved during import, so we need to come up with a new one.
			const FString BlendShapeName( SkelMesh->GetName() + TEXT( "_blendShapes" ) );
			FbxBlendShape* BlendShape = FbxBlendShape::Create( Mesh, TCHAR_TO_UTF8( *BlendShapeName ) );
			bool bHasBadMorphTarget = false;

			for( UMorphTarget* MorphTarget : GetMorphTargets(SkelMesh) )
			{
				int32 DeformerIndex = Mesh->AddDeformer( BlendShape );
				FbxBlendShapeChannel* BlendShapeChannel = FbxBlendShapeChannel::Create( BlendShape, TCHAR_TO_UTF8( *MorphTarget->GetName() ) );

				if( BlendShape->AddBlendShapeChannel( BlendShapeChannel ) )
				{
					FbxShape* Shape = FbxShape::Create( BlendShapeChannel, TCHAR_TO_UTF8( *MorphTarget->GetName() ) );
					Shape->InitControlPoints( VertexCount );
					FbxVector4* ShapeControlPoints = Shape->GetControlPoints();

					// Replicate the base mesh in the shape control points to set up the data.
					for( int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex )
					{
						FVector Position = (FVector)Vertices[ VertIndex ].Position;
						ShapeControlPoints[ VertIndex ] = Converter.ConvertToFbxPos( Position );
					}

					int32 NumberOfDeltas = 0;
					const FMorphTargetDelta* MorphTargetDeltas = MorphTarget->GetMorphTargetDelta( LODIndex, NumberOfDeltas );
					for( int32 MorphTargetDeltaIndex = 0; MorphTargetDeltaIndex < NumberOfDeltas; ++MorphTargetDeltaIndex )
					{
						// Apply the morph target deltas to the control points.
						const FMorphTargetDelta& CurrentDelta = MorphTargetDeltas[ MorphTargetDeltaIndex ];
						uint32 RemappedSourceIndex = CurrentDelta.SourceIdx;

						if( VertexIndexOffsetPairArray.Num() > 1 )
						{
							//If the skeletal mesh contains clothing we need to remap the morph target index too.
							int32 UpperBoundIndex = Algo::UpperBoundBy( VertexIndexOffsetPairArray, RemappedSourceIndex,
																		[]( const auto& CurrentPair ) { return CurrentPair.Key; } ); //Value functor
							RemappedSourceIndex -= VertexIndexOffsetPairArray[ UpperBoundIndex - 1 ].Value;
						}

						if( RemappedSourceIndex < static_cast<uint32>( VertexCount ) )
						{
							ShapeControlPoints[ RemappedSourceIndex ] = Converter.ConvertToFbxPos( (FVector)Vertices[ RemappedSourceIndex ].Position + (FVector)CurrentDelta.PositionDelta );
						}
						else
						{
							bHasBadMorphTarget = true;
						}
					}

					BlendShapeChannel->AddTargetShape( Shape );
					FName MorphTargetName = MorphTarget->GetFName();
					if( AnimSeq && SmartNameMapping && SmartNameMapping->GetCurveMetaData( MorphTargetName ) && SmartNameMapping->GetCurveMetaData( MorphTargetName )->Type.bMorphtarget )
					{
						FbxAnimCurve* AnimCurve = Mesh->GetShapeChannel( DeformerIndex, BlendShape->GetBlendShapeChannelCount() - 1, AnimLayer, true );
						BlendShapeCurvesMap.Add( MorphTargetName, AnimCurve );
					}
				}
			}

			if( bHasBadMorphTarget )
			{
				UE_LOG( LogTemp, Warning, TEXT( "Encountered corrupted morphtarget(s) during export of SkeletalMesh %s, bad vertices were ignored." ), *SkelMesh->GetName() );
			}
		}

		if( AnimSeq && BlendShapeCurvesMap.Num() > 0 )
		{
			ExportCustomAnimCurvesToFbx( BlendShapeCurvesMap, AnimSeq,
										 0.f,		// AnimStartOffset
										 0.f,		// AnimEndOffset
										 1.f,		// AnimPlayRate
										 0.f,		// StartTime
										 100.f );		// ValueScale, for some reason we need to scale BlendShape curves by a factor of 100.
		}
	}

	FbxNode* MeshNode = FbxNode::Create( TheScene, TCHAR_TO_UTF8( MeshName ) );
	MeshNode->SetNodeAttribute( Mesh );



	// Add the materials for the mesh
	const TArray<FSkeletalMaterial>& SkelMeshMaterials = GetMaterials(SkelMesh);
	int32 MaterialCount = SkelMeshMaterials.Num();

	for( int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex )
	{
		UMaterialInterface* MatInterface = nullptr;

		if( OverrideMaterials && OverrideMaterials->IsValidIndex( MaterialIndex ) )
		{
			MatInterface = ( *OverrideMaterials )[ MaterialIndex ];
		}
		else
		{
			MatInterface = GetMaterials( SkelMesh )[ MaterialIndex ].MaterialInterface;
		}

		FbxSurfaceMaterial* FbxMaterial = NULL;
		if( LODIndex == 0 )
		{
			if( MatInterface && !FbxMaterials.Find( MatInterface ) )
			{
				FbxMaterial = ExportMaterial( MatInterface );
			}
		}
		else if( MatInterface )
		{
			if( FbxSurfaceMaterial** FbxMaterialPtr = FbxMaterials.Find( MatInterface ) )
			{
				FbxMaterial = *FbxMaterialPtr;
			}
		}

		if( !FbxMaterial )
		{
			// Note: The vertex data relies on there being a set number of Materials.  
			// If you try to add the same material again it will not be added, so create a 
			// default material with a unique name to ensure the proper number of materials

			TCHAR NewMaterialName[ MAX_SPRINTF ] = TEXT( "" );
			FCString::Sprintf( NewMaterialName, TEXT( "Fbx Default Material %i" ), MaterialIndex );

			FbxMaterial = FbxSurfaceLambert::Create( TheScene, TCHAR_TO_UTF8( NewMaterialName ) );
			( (FbxSurfaceLambert*)FbxMaterial )->Diffuse.Set( FbxDouble3( 0.72, 0.72, 0.72 ) );
		}

		MeshNode->AddMaterial( FbxMaterial );
	}

	int32 SavedMaterialCount = MeshNode->GetMaterialCount();
	check( SavedMaterialCount == MaterialCount );

	return MeshNode;
}
FbxNode* ExportAnimSequence( FFbxExporter* Exporter, const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, bool bExportSkelMesh, const TCHAR* MeshName, FbxNode* ActorRootNode, const TArray<UMaterialInterface*>* OverrideMaterials /*= nullptr*/ )
{
	if( TheScene == NULL || AnimSeq == NULL || SkelMesh == NULL )
	{
		return NULL;
	}


	FbxNode* RootNode = ( ActorRootNode ) ? ActorRootNode : TheScene->GetRootNode();

	//Create a temporary node attach to the scene root.
	//This will allow us to do the binding without the scene transform (non uniform scale is not supported when binding the skeleton)
	//We then detach from the temp node and attach to the parent and remove the temp node
	FString FbxNodeName = FGuid::NewGuid().ToString( EGuidFormats::Digits );
	FbxNode* TmpNodeNoTransform = FbxNode::Create( TheScene, TCHAR_TO_UTF8( *FbxNodeName ) );
	TheScene->GetRootNode()->AddChild( TmpNodeNoTransform );


	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton( SkelMesh, BoneNodes );
	TmpNodeNoTransform->AddChild( SkeletonRootNode );


	// Export the anim sequence
	{
		ExportAnimSequenceToFbx( Exporter, AnimSeq,
								 SkelMesh,
								 BoneNodes,
								 AnimLayer,
								 0.f,		// AnimStartOffset
								 0.f,		// AnimEndOffset
								 1.f,		// AnimPlayRate
								 0.f );		// StartTime

		CorrectAnimTrackInterpolation( BoneNodes, AnimLayer );
	}


	// Optionally export the mesh
	if( bExportSkelMesh )
	{
		FString MeshNodeName;

		if( MeshName )
		{
			MeshNodeName = MeshName;
		}
		else
		{
			SkelMesh->GetName( MeshNodeName );
		}

		FbxNode* MeshRootNode = nullptr;
		if( Exporter->GetExportOptions()->LevelOfDetail && SkelMesh->GetLODNum() > 1 )
		{
			FString LodGroup_MeshName = MeshNodeName + TEXT( "_LodGroup" );
			MeshRootNode = FbxNode::Create( TheScene, TCHAR_TO_UTF8( *LodGroup_MeshName ) );
			TmpNodeNoTransform->AddChild( MeshRootNode );
			LodGroup_MeshName = MeshNodeName + TEXT( "_LodGroupAttribute" );
			FbxLODGroup* FbxLodGroupAttribute = FbxLODGroup::Create( TheScene, TCHAR_TO_UTF8( *LodGroup_MeshName ) );
			MeshRootNode->AddNodeAttribute( FbxLodGroupAttribute );

			FbxLodGroupAttribute->ThresholdsUsedAsPercentage = true;
			//Export an Fbx Mesh Node for every LOD and child them to the fbx node (LOD Group)
			for( int CurrentLodIndex = 0; CurrentLodIndex < SkelMesh->GetLODNum(); ++CurrentLodIndex )
			{
				FString FbxLODNodeName = MeshNodeName + TEXT( "_LOD" ) + FString::FromInt( CurrentLodIndex );
				if( CurrentLodIndex + 1 < SkelMesh->GetLODNum() )
				{
					//Convert the screen size to a threshold, it is just to be sure that we set some threshold, there is no way to convert this precisely
					double LodScreenSize = (double)( 10.0f / SkelMesh->GetLODInfo( CurrentLodIndex )->ScreenSize.Default );
					FbxLodGroupAttribute->AddThreshold( LodScreenSize );
				}
				FbxNode* FbxActorLOD = CreateMesh( Exporter, SkelMesh, *FbxLODNodeName, CurrentLodIndex, AnimSeq, OverrideMaterials );
				if( FbxActorLOD )
				{
					MeshRootNode->AddChild( FbxActorLOD );
					if( SkeletonRootNode )
					{
						// Bind the mesh to the skeleton
						BindMeshToSkeleton( SkelMesh, FbxActorLOD, BoneNodes, CurrentLodIndex );
						// Add the bind pose
						CreateBindPose( FbxActorLOD );
					}
				}
			}
		}
		else
		{
			const int32 LodIndex = 0;
			MeshRootNode = CreateMesh( Exporter, SkelMesh, *MeshNodeName, LodIndex, AnimSeq, OverrideMaterials );
			if( MeshRootNode )
			{
				TmpNodeNoTransform->AddChild( MeshRootNode );
				if( SkeletonRootNode )
				{
					// Bind the mesh to the skeleton
					BindMeshToSkeleton( SkelMesh, MeshRootNode, BoneNodes, LodIndex );

					// Add the bind pose
					CreateBindPose( MeshRootNode );
				}
			}
		}

		if( MeshRootNode )
		{
			TmpNodeNoTransform->RemoveChild( MeshRootNode );
			RootNode->AddChild( MeshRootNode );
		}
	}

	if( SkeletonRootNode )
	{
		TmpNodeNoTransform->RemoveChild( SkeletonRootNode );
		RootNode->AddChild( SkeletonRootNode );
	}

	TheScene->GetRootNode()->RemoveChild( TmpNodeNoTransform );
	TheScene->RemoveNode( TmpNodeNoTransform );

	return SkeletonRootNode;
}
void ExportObjectMetadata( const UObject* ObjectToExport, FbxNode* Node )
{
	if( ObjectToExport && Node )
	{
		// Retrieve the metadata map without creating it
		const TMap<FName, FString>* MetadataMap = UMetaData::GetMapForObject( ObjectToExport );
		if( MetadataMap )
		{
			static const FString MetadataPrefix( FBX_METADATA_PREFIX );
			for( const auto& MetadataIt : *MetadataMap )
			{
				// Export object metadata tags that are prefixed as FBX custom user-defined properties
				// Remove the prefix since it's for Unreal use only (and '.' is considered an invalid character for user property names in DCC like Maya)
				FString TagAsString = MetadataIt.Key.ToString();
				if( TagAsString.RemoveFromStart( MetadataPrefix ) )
				{
					// Remaining tag follows the format NodeName.PropertyName, so replace '.' with '_'
					TagAsString.ReplaceInline( TEXT( "." ), TEXT( "_" ) );

					if( MetadataIt.Value == TEXT( "true" ) || MetadataIt.Value == TEXT( "false" ) )
					{
						FbxProperty Property = FbxProperty::Create( Node, FbxBoolDT, TCHAR_TO_UTF8( *TagAsString ) );
						FbxBool ValueBool = MetadataIt.Value == TEXT( "true" ) ? true : false;

						Property.Set( ValueBool );
						Property.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					}
					else
					{
						FbxProperty Property = FbxProperty::Create( Node, FbxStringDT, TCHAR_TO_UTF8( *TagAsString ) );
						FbxString ValueString( TCHAR_TO_UTF8( *MetadataIt.Value ) );

						Property.Set( ValueString );
						Property.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					}
				}
			}
		}
	}
}
void ExportObjectMetadataToBones( const UObject* ObjectToExport, const TArray<FbxNode*>& Nodes )
{
	if( !ObjectToExport || Nodes.Num() == 0 )
	{
		return;
	}

	// Retrieve the metadata map without creating it
	const TMap<FName, FString>* MetadataMap = UMetaData::GetMapForObject( ObjectToExport );
	if( MetadataMap )
	{
		// Map the nodes to their names for fast access
		TMap<FString, FbxNode*> NameToNode;
		for( FbxNode* Node : Nodes )
		{
			NameToNode.Add( FString( Node->GetName() ), Node );
		}

		static const FString MetadataPrefix( FBX_METADATA_PREFIX );
		for( const auto& MetadataIt : *MetadataMap )
		{
			// Export object metadata tags that are prefixed as FBX custom user-defined properties
			// Remove the prefix since it's for Unreal use only (and '.' is considered an invalid character for user property names in DCC like Maya)
			FString TagAsString = MetadataIt.Key.ToString();
			if( TagAsString.RemoveFromStart( MetadataPrefix ) )
			{
				// Extract the node name from the metadata tag
				FString NodeName;
				int32 CharPos = INDEX_NONE;
				if( TagAsString.FindChar( TEXT( '.' ), CharPos ) )
				{
					NodeName = TagAsString.Left( CharPos );

					// The remaining part is the actual metadata tag
					TagAsString.RightChopInline( CharPos + 1, false ); // exclude the period
				}

				// Try to attach the metadata to its associated node by name
				FbxNode** Node = NameToNode.Find( NodeName );
				if( Node )
				{
					if( MetadataIt.Value == TEXT( "true" ) || MetadataIt.Value == TEXT( "false" ) )
					{
						FbxProperty Property = FbxProperty::Create( *Node, FbxBoolDT, TCHAR_TO_UTF8( *TagAsString ) );
						FbxBool ValueBool = MetadataIt.Value == TEXT( "true" ) ? true : false;

						Property.Set( ValueBool );
						Property.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					}
					else
					{
						FbxProperty Property = FbxProperty::Create( *Node, FbxStringDT, TCHAR_TO_UTF8( *TagAsString ) );
						FbxString ValueString( TCHAR_TO_UTF8( *MetadataIt.Value ) );

						Property.Set( ValueString );
						Property.ModifyFlag( FbxPropertyFlags::eUserDefined, true );
					}
				}
			}
		}
	}
}
FbxNode* ExportSkeletalMeshToFbx( FFbxExporter* Exporter, const USkeletalMesh* SkeletalMesh, const UAnimSequence* AnimSeq, const TCHAR* MeshName, FbxNode* ActorRootNode, const TArray<UMaterialInterface*>* OverrideMaterials /*= nullptr*/ )
{
	if( AnimSeq )
	{
		return ExportAnimSequence( Exporter, AnimSeq, SkeletalMesh, Exporter->GetExportOptions()->bExportPreviewMesh, MeshName, ActorRootNode, OverrideMaterials );

	}
	else
	{
		//Create a temporary node attach to the scene root.
		//This will allow us to do the binding without the scene transform (non uniform scale is not supported when binding the skeleton)
		//We then detach from the temp node and attach to the parent and remove the temp node
		FString FbxNodeName = FGuid::NewGuid().ToString( EGuidFormats::Digits );
		FbxNode* TmpNodeNoTransform = FbxNode::Create( TheScene, TCHAR_TO_UTF8( *FbxNodeName ) );
		TheScene->GetRootNode()->AddChild( TmpNodeNoTransform );

		TArray<FbxNode*> BoneNodes;

		// Add the skeleton to the scene
		FbxNode* SkeletonRootNode = CreateSkeleton( SkeletalMesh, BoneNodes );
		if( SkeletonRootNode )
		{
			TmpNodeNoTransform->AddChild( SkeletonRootNode );
		}

		FbxNode* MeshRootNode = nullptr;
		if( Exporter->GetExportOptions()->LevelOfDetail && SkeletalMesh->GetLODNum() > 1 )
		{
			FString LodGroup_MeshName = FString( MeshName ) + TEXT( "_LodGroup" );
			MeshRootNode = FbxNode::Create( TheScene, TCHAR_TO_UTF8( *LodGroup_MeshName ) );
			TmpNodeNoTransform->AddChild( MeshRootNode );
			LodGroup_MeshName = FString( MeshName ) + TEXT( "_LodGroupAttribute" );
			FbxLODGroup* FbxLodGroupAttribute = FbxLODGroup::Create( TheScene, TCHAR_TO_UTF8( *LodGroup_MeshName ) );
			MeshRootNode->AddNodeAttribute( FbxLodGroupAttribute );

			FbxLodGroupAttribute->ThresholdsUsedAsPercentage = true;
			//Export an Fbx Mesh Node for every LOD and child them to the fbx node (LOD Group)
			for( int CurrentLodIndex = 0; CurrentLodIndex < SkeletalMesh->GetLODNum(); ++CurrentLodIndex )
			{
				FString FbxLODNodeName = FString( MeshName ) + TEXT( "_LOD" ) + FString::FromInt( CurrentLodIndex );
				if( CurrentLodIndex + 1 < SkeletalMesh->GetLODNum() )
				{
					//Convert the screen size to a threshold, it is just to be sure that we set some threshold, there is no way to convert this precisely
					double LodScreenSize = (double)( 10.0f / SkeletalMesh->GetLODInfo( CurrentLodIndex )->ScreenSize.Default );
					FbxLodGroupAttribute->AddThreshold( LodScreenSize );
				}
				const UAnimSequence* NullAnimSeq = nullptr;
				FbxNode* FbxActorLOD = CreateMesh( Exporter, SkeletalMesh, *FbxLODNodeName, CurrentLodIndex, NullAnimSeq, OverrideMaterials );
				if( FbxActorLOD )
				{
					MeshRootNode->AddChild( FbxActorLOD );
					if( SkeletonRootNode )
					{
						// Bind the mesh to the skeleton
						BindMeshToSkeleton( SkeletalMesh, FbxActorLOD, BoneNodes, CurrentLodIndex );
						// Add the bind pose
						CreateBindPose( FbxActorLOD );
					}
				}
			}
		}
		else
		{
			const int32 LODIndex = 0;
			const UAnimSequence* NullAnimSeq = nullptr;
			MeshRootNode = CreateMesh( Exporter, SkeletalMesh, MeshName, LODIndex, NullAnimSeq, OverrideMaterials );
			if( MeshRootNode )
			{
				TmpNodeNoTransform->AddChild( MeshRootNode );
				if( SkeletonRootNode )
				{
					// Bind the mesh to the skeleton
					BindMeshToSkeleton( SkeletalMesh, MeshRootNode, BoneNodes, 0 );

					// Add the bind pose
					CreateBindPose( MeshRootNode );
				}
			}
		}

		if( SkeletonRootNode )
		{
			TmpNodeNoTransform->RemoveChild( SkeletonRootNode );
			ActorRootNode->AddChild( SkeletonRootNode );
		}

		ExportObjectMetadataToBones( GetSkeleton(SkeletalMesh), BoneNodes );

		if( MeshRootNode )
		{
			TmpNodeNoTransform->RemoveChild( MeshRootNode );
			ActorRootNode->AddChild( MeshRootNode );
			ExportObjectMetadata( SkeletalMesh, MeshRootNode );
		}

		TheScene->GetRootNode()->RemoveChild( TmpNodeNoTransform );
		TheScene->RemoveNode( TmpNodeNoTransform );
		return SkeletonRootNode;
	}

	return NULL;
}
#if ENGINE_MAJOR_VERSION == 4
	TArray<UMaterialInterface*> GetArrayOfMaterials( TArray<UMaterialInterface*>& ArrayType )
#else
	TArray<UMaterialInterface*> GetArrayOfMaterials( TArray<TObjectPtr<class UMaterialInterface>>& NewArrayType )
#endif
{
	#if ENGINE_MAJOR_VERSION == 4
		return ArrayType;
	#else
		TArray<UMaterialInterface*> Array;
		for( int i = 0; i < NewArrayType.Num(); i++ )
		{
			Array.Add( NewArrayType[ i ] );
		}	
		return Array;
	#endif
}
void ExportSkeletalMeshComponent( FFbxExporter* Exporter, USkeletalMeshComponent* SkelMeshComp, const TCHAR* MeshName, FbxNode* ActorRootNode, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq )
{
	if( SkelMeshComp && SkelMeshComp->SkeletalMesh )
	{
		UAnimSequence* AnimSeq = ( bSaveAnimSeq && SkelMeshComp->GetAnimationMode() == EAnimationMode::AnimationSingleNode ) ?
			Cast<UAnimSequence>( SkelMeshComp->AnimationData.AnimToPlay ) : NULL;
		#if ENGINE_MAJOR_VERSION == 4
			FbxNode* SkeletonRootNode = ExportSkeletalMeshToFbx( Exporter, SkelMeshComp->SkeletalMesh, AnimSeq, MeshName, ActorRootNode, &SkelMeshComp->OverrideMaterials );
		#else
			TArray<UMaterialInterface*> Materials = GetArrayOfMaterials( SkelMeshComp->OverrideMaterials );
			FbxNode* SkeletonRootNode = ExportSkeletalMeshToFbx( Exporter, SkelMeshComp->SkeletalMesh, AnimSeq, MeshName, ActorRootNode, &Materials );
		#endif

		if( SkeletonRootNode )
		{
			FbxSkeletonRoots.Add( SkelMeshComp, SkeletonRootNode );
			NodeNameAdapter.AddFbxNode( SkelMeshComp, SkeletonRootNode );
		}
	}
}
FbxNode* ExportActor( FFbxExporter* Exporter, AActor* Actor, bool bExportComponents, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq = true )
{
	// Verify that this actor isn't already exported, create a structure for it
	// and buffer it.
	FbxNode* ActorNode = FindActor( Actor, &NodeNameAdapter );
	if( ActorNode == NULL )
	{
		FString FbxNodeName = NodeNameAdapter.GetActorNodeName( Actor );
		FbxNodeName = GetFbxObjectName( FbxNodeName, NodeNameAdapter );
		ActorNode = FbxNode::Create( TheScene, TCHAR_TO_UTF8( *FbxNodeName ) );

		AActor* ParentActor = Actor->GetAttachParentActor();
		// this doesn't work with skeletalmeshcomponent
		FbxNode* ParentNode = FindActor( ParentActor, &NodeNameAdapter );
		FVector ActorLocation, ActorRotation, ActorScale;

		TArray<AActor*> AttachedActors;
		Actor->GetAttachedActors( AttachedActors );
		const bool bHasAttachedActors = AttachedActors.Num() != 0;

		// For cameras and lights: always add a rotation to get the correct coordinate system.
		FTransform RotationDirectionConvert = FTransform::Identity;
		const bool bIsCameraActor = Actor->IsA( ACameraActor::StaticClass() );
		const bool bIsLightActor = Actor->IsA( ALight::StaticClass() );
		if( bIsCameraActor || bIsLightActor )
		{
			if( bIsCameraActor )
			{
				FRotator Rotator = FFbxDataConverter::GetCameraRotation().GetInverse();
				RotationDirectionConvert = FTransform( Rotator );
			}
			else if( bIsLightActor )
			{
				FRotator Rotator = FFbxDataConverter::GetLightRotation().GetInverse();
				RotationDirectionConvert = FTransform( Rotator );
			}
		}

		//If the parent is the root or is not export use the root node as the parent
		if( bKeepHierarchy && ParentNode )
		{
			// Set the default position of the actor on the transforms
			// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
			const FTransform RelativeTransform = RotationDirectionConvert * Actor->GetTransform().GetRelativeTransform( ParentActor->GetTransform() );
			ActorLocation = RelativeTransform.GetTranslation();
			ActorRotation = RelativeTransform.GetRotation().Euler();
			ActorScale = RelativeTransform.GetScale3D();
		}
		else
		{
			ParentNode = TheScene->GetRootNode();
			// Set the default position of the actor on the transforms
			// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
			if( ParentActor != NULL )
			{
				//In case the parent was not export, get the absolute transform
				const FTransform AbsoluteTransform = RotationDirectionConvert * Actor->GetTransform();
				ActorLocation = AbsoluteTransform.GetTranslation();
				ActorRotation = AbsoluteTransform.GetRotation().Euler();
				ActorScale = AbsoluteTransform.GetScale3D();
			}
			else
			{
				const FTransform ConvertedTransform = RotationDirectionConvert * Actor->GetTransform();
				ActorLocation = ConvertedTransform.GetTranslation();
				ActorRotation = ConvertedTransform.GetRotation().Euler();
				ActorScale = ConvertedTransform.GetScale3D();
			}
		}

		ParentNode->AddChild( ActorNode );
		FbxActors.Add( Actor, ActorNode );
		NodeNameAdapter.AddFbxNode( Actor, ActorNode );

		// Set the default position of the actor on the transforms
		// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		bool SetIdentity = true;
		if( SetIdentity )
		{
			ActorNode->LclRotation.Set( Converter.ConvertToFbxRot( FVector( 90, 0, 0 )));//used to work
			//ActorNode->LclRotation.Set( Converter.ConvertToFbxRot( FVector( 90, 180, 0 ) ) );//rotate for similar preview angle
		}
		else
		{
			ActorNode->LclTranslation.Set( Converter.ConvertToFbxPos( ActorLocation ) );
			ActorNode->LclRotation.Set( Converter.ConvertToFbxRot( ActorRotation ) );
			ActorNode->LclScaling.Set( Converter.ConvertToFbxScale( ActorScale ) );
		}

		if( bExportComponents )
		{
			TInlineComponentArray<USceneComponent*> ComponentsToExport;
			for( UActorComponent* ActorComp : Actor->GetComponents() )
			{
				USceneComponent* Component = Cast<USceneComponent>( ActorComp );

				if( Component == nullptr || Component->bHiddenInGame )
				{
					//Skip hidden component like camera mesh or other editor helper
					continue;
				}

				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( Component );
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>( Component );
				UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>( Component );

				if( StaticMeshComp && StaticMeshComp->GetStaticMesh() )
				{
					ComponentsToExport.Add( StaticMeshComp );
				}
				else if( SkelMeshComp && SkelMeshComp->SkeletalMesh )
				{
					ComponentsToExport.Add( SkelMeshComp );
				}
				else if( Component->IsA( UCameraComponent::StaticClass() ) )
				{
					ComponentsToExport.Add( Component );
				}
				else if( Component->IsA( ULightComponent::StaticClass() ) )
				{
					ComponentsToExport.Add( Component );
				}
				else if( ChildActorComp && ChildActorComp->GetChildActor() )
				{
					ComponentsToExport.Add( ChildActorComp );
				}
			}

			for( int32 CompIndex = 0; CompIndex < ComponentsToExport.Num(); ++CompIndex )
			{
				USceneComponent* Component = ComponentsToExport[ CompIndex ];

				RotationDirectionConvert = FTransform::Identity;
				// For cameras and lights: always add a rotation to get the correct coordinate system
				// Unless we are parented to an Actor of the same type, since the rotation direction was already added
				if( Component->IsA( UCameraComponent::StaticClass() ) || Component->IsA( ULightComponent::StaticClass() ) )
				{
					if( !bIsCameraActor && Component->IsA( UCameraComponent::StaticClass() ) )
					{
						FRotator Rotator = FFbxDataConverter::GetCameraRotation().GetInverse();
						RotationDirectionConvert = FTransform( Rotator );
					}
					else if( !bIsLightActor && Component->IsA( ULightComponent::StaticClass() ) )
					{
						FRotator Rotator = FFbxDataConverter::GetLightRotation().GetInverse();
						RotationDirectionConvert = FTransform( Rotator );
					}
				}

				FbxNode* ExportNode = ActorNode;
				if( ComponentsToExport.Num() > 1 )
				{
					// This actor has multiple components
					// create a child node under the actor for each component
					FbxNode* CompNode = FbxNode::Create( TheScene, TCHAR_TO_UTF8( *Component->GetName() ) );

					if( Component != Actor->GetRootComponent() )
					{
						// Transform is relative to the root component
						const FTransform RelativeTransform = RotationDirectionConvert * Component->GetComponentToWorld().GetRelativeTransform( Actor->GetTransform() );
						CompNode->LclTranslation.Set( Converter.ConvertToFbxPos( RelativeTransform.GetTranslation() ) );
						CompNode->LclRotation.Set( Converter.ConvertToFbxRot( RelativeTransform.GetRotation().Euler() ) );
						CompNode->LclScaling.Set( Converter.ConvertToFbxScale( RelativeTransform.GetScale3D() ) );
					}

					ExportNode = CompNode;
					ActorNode->AddChild( CompNode );
				}
				// If this actor has attached actors, don't allow its non root component components to contribute to the transform
				else if( Component != Actor->GetRootComponent() && !bHasAttachedActors )
				{
					// Merge the component relative transform in the ActorNode transform since this is the only component to export and its not the root
					const FTransform RelativeTransform = RotationDirectionConvert * Component->GetComponentToWorld().GetRelativeTransform( Actor->GetTransform() );

					FTransform ActorTransform( FRotator::MakeFromEuler( ActorRotation ).Quaternion(), ActorLocation, ActorScale );
					FTransform TotalTransform = RelativeTransform * ActorTransform;

					ActorNode->LclTranslation.Set( Converter.ConvertToFbxPos( TotalTransform.GetLocation() ) );
					ActorNode->LclRotation.Set( Converter.ConvertToFbxRot( TotalTransform.GetRotation().Euler() ) );
					ActorNode->LclScaling.Set( Converter.ConvertToFbxScale( TotalTransform.GetScale3D() ) );
				}

				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( Component );
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>( Component );
				UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>( Component );

				if( StaticMeshComp && StaticMeshComp->GetStaticMesh() )
				{
					if( USplineMeshComponent* SplineMeshComp = Cast<USplineMeshComponent>( StaticMeshComp ) )
					{
						//ExportSplineMeshToFbx( SplineMeshComp, *SplineMeshComp->GetName(), ExportNode );
					}
					else if( UInstancedStaticMeshComponent* InstancedMeshComp = Cast<UInstancedStaticMeshComponent>( StaticMeshComp ) )
					{
						//ExportInstancedMeshToFbx( InstancedMeshComp, *InstancedMeshComp->GetName(), ExportNode );
					}
					else
					{
						const int32 LODIndex = ( StaticMeshComp->ForcedLodModel > 0 ? StaticMeshComp->ForcedLodModel - 1 : /* auto-select*/ 0 );
						const int32 LightmapUVChannel = -1;
						const TArray<FStaticMaterial>* MaterialOrderOverride = nullptr;
						const TArray<UMaterialInterface*> Materials = StaticMeshComp->GetMaterials();
						const FColorVertexBuffer* ColorBuffer = nullptr;

						//Original name is a blueprint name
						//TODO : temporary fix, LODs are not exported through this !
						FString FbxLODNodeName = StaticMeshComp->GetStaticMesh()->GetName();
						FbxLODNodeName += TEXT( "_LOD0" );
						ExportNode->SetName( TCHAR_TO_UTF8( *FbxLODNodeName ) );
						ExportStaticMeshToFbx( Exporter, StaticMeshComp->GetStaticMesh(), LODIndex, *StaticMeshComp->GetName(), ExportNode, LightmapUVChannel, ColorBuffer, MaterialOrderOverride, &Materials );
					}
				}
				else if( SkelMeshComp && SkelMeshComp->SkeletalMesh )
				{
					ExportSkeletalMeshComponent( Exporter, SkelMeshComp, *SkelMeshComp->GetName(), ExportNode, NodeNameAdapter, bSaveAnimSeq );
				}
				// If this actor has attached actors, don't allow a camera component to determine the node attributes because that would alter the transform
				else if( Component->IsA( UCameraComponent::StaticClass() ) && !bHasAttachedActors )
				{
					//FbxCamera* Camera = FbxCamera::Create( Scene, TCHAR_TO_UTF8( *Component->GetName() ) );
					//FillFbxCameraAttribute( ActorNode, Camera, Cast<UCameraComponent>( Component ) );
					//ExportNode->SetNodeAttribute( Camera );
				}
				else if( Component->IsA( ULightComponent::StaticClass() ) && !bHasAttachedActors )
				{
					//FbxLight* Light = FbxLight::Create( Scene, TCHAR_TO_UTF8( *Component->GetName() ) );
					//FillFbxLightAttribute( Light, ActorNode, Cast<ULightComponent>( Component ) );
					//ExportNode->SetNodeAttribute( Light );
				}
				else if( ChildActorComp && ChildActorComp->GetChildActor() )
				{
					FbxNode* ChildActorNode = ExportActor( Exporter, ChildActorComp->GetChildActor(), true, NodeNameAdapter, bSaveAnimSeq );
					FbxActors.Add( ChildActorComp->GetChildActor(), ChildActorNode );
					NodeNameAdapter.AddFbxNode( ChildActorComp->GetChildActor(), ChildActorNode );
				}
			}
		}

	}

	return ActorNode;
}
void ExportStaticMesh( FFbxExporter* Exporter, AActor* Actor, UStaticMeshComponent* StaticMeshComponent, INodeNameAdapter& NodeNameAdapter )
{
	//if( Exporter->Scene == NULL || Actor == NULL || StaticMeshComponent == NULL )
	//{
	//	return;
	//}

	// Retrieve the static mesh rendering information at the correct LOD level.
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if( StaticMesh == NULL || !StaticMesh->HasValidRenderData() )
	{
		return;
	}
	FString FbxNodeName = NodeNameAdapter.GetActorNodeName( Actor );
	FString FbxMeshName = StaticMesh->GetName();// .Replace( TEXT( "-" ), TEXT( "_" ) );
	FColorVertexBuffer* ColorBuffer = NULL;

	if( Exporter->GetExportOptions()->LevelOfDetail && StaticMesh->GetNumLODs() > 1 )
	{
		//Create a fbx LOD Group node
		FbxNode* FbxActor = ExportActor( Exporter, Actor, false, NodeNameAdapter );
		FString FbxLODGroupName = NodeNameAdapter.GetActorNodeName( Actor );
		FbxLODGroupName += TEXT( "_LodGroup" );
		FbxLODGroupName = GetFbxObjectName( FbxLODGroupName, NodeNameAdapter );
		FbxLODGroup* FbxLodGroupAttribute = FbxLODGroup::Create( TheScene, TCHAR_TO_UTF8( *FbxLODGroupName ) );
		FbxActor->AddNodeAttribute( FbxLodGroupAttribute );
		FbxLodGroupAttribute->ThresholdsUsedAsPercentage = true;
		//Export an Fbx Mesh Node for every LOD and child them to the fbx node (LOD Group)
		for( int CurrentLodIndex = 0; CurrentLodIndex < StaticMesh->GetNumLODs(); ++CurrentLodIndex )
		{
			if( CurrentLodIndex < StaticMeshComponent->LODData.Num() )
			{
				ColorBuffer = StaticMeshComponent->LODData[ CurrentLodIndex ].OverrideVertexColors;
			}
			else
			{
				ColorBuffer = nullptr;
			}
			FString FbxLODNodeName = StaticMesh->GetName();// NodeNameAdapter.GetActorNodeName( Actor );
			FbxLODNodeName += TEXT( "_LOD" ) + FString::FromInt( CurrentLodIndex );
			FbxLODNodeName = GetFbxObjectName( FbxLODNodeName, NodeNameAdapter );
			FbxNode* FbxActorLOD = FbxNode::Create( TheScene, TCHAR_TO_UTF8( *FbxLODNodeName ) );
			FbxActor->AddChild( FbxActorLOD );
			if( CurrentLodIndex + 1 < StaticMesh->GetNumLODs() )
			{
				//Convert the screen size to a threshold, it is just to be sure that we set some threshold, there is no way to convert this precisely
				double LodScreenSize = (double)( 10.0f / GetRenderData( StaticMesh )->ScreenSize[ CurrentLodIndex ].Default );
				FbxLodGroupAttribute->AddThreshold( LodScreenSize );
			}

			const int32 LightmapUVChannel = -1;
			const TArray<FStaticMaterial>* MaterialOrderOverride = nullptr;
			const TArray<UMaterialInterface*> Materials = GetMaterialsToBeUsedInFBX( StaticMeshComponent );			
			ExportStaticMeshToFbx( Exporter, StaticMesh, CurrentLodIndex, *FbxMeshName, FbxActorLOD, LightmapUVChannel, ColorBuffer, MaterialOrderOverride,  &Materials );
		}
	}
	else
	{
		const int32 LODIndex = ( StaticMeshComponent->ForcedLodModel > 0 ? StaticMeshComponent->ForcedLodModel - 1 : /* auto-select*/ 0 );
		if( LODIndex != INDEX_NONE && LODIndex < StaticMeshComponent->LODData.Num() )
		{
			ColorBuffer = StaticMeshComponent->LODData[ LODIndex ].OverrideVertexColors;
		}
		FbxNode* FbxActor = ExportActor( Exporter, Actor, false, NodeNameAdapter );

		FString FbxLODNodeName = StaticMesh->GetName();// NodeNameAdapter.GetActorNodeName( Actor );
		FbxLODNodeName += TEXT( "_LOD0" );
		FbxActor->SetName( TCHAR_TO_UTF8( *FbxLODNodeName ) );

		const int32 LightmapUVChannel = -1;
		const TArray<FStaticMaterial>* MaterialOrderOverride = nullptr;
		const TArray<UMaterialInterface*> Materials = GetMaterialsToBeUsedInFBX( StaticMeshComponent );
		ExportStaticMeshToFbx( Exporter, StaticMesh, LODIndex, *FbxMeshName, FbxActor, LightmapUVChannel, ColorBuffer, MaterialOrderOverride, &Materials );
	}
}
void ExportLevelMesh( FFbxExporter* Exporter, ULevel* InLevel, bool bExportLevelGeometry, TArray<AActor*>& ActorToExport, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq )
{
	if( InLevel == NULL )
	{
		return;
	}

	if( bExportLevelGeometry )
	{		
		// Exports the level's scene geometry
		// the vertex number of Model must be more than 2 (at least a triangle panel)
		//if( InLevel->Model != NULL && InLevel->Model->VertexBuffer.Vertices.Num() > 2 && InLevel->Model->MaterialIndexBuffers.Num() > 0 )
		//{
		//	// create a FbxNode
		//	FbxNode* Node = FbxNode::Create( TheScene, "LevelMesh" );
		//
		//	// set the shading mode to view texture
		//	Node->SetShadingMode( FbxNode::eTextureShading );
		//	Node->LclScaling.Set( FbxVector4( 1.0, 1.0, 1.0 ) );
		//
		//	TheScene->GetRootNode()->AddChild( Node );
		//
		//	// Export the mesh for the world
		//	//Exporter->ExportModel( InLevel->Model, Node, "Level Mesh" );
		//}
	}

	//Sort the hierarchy to make sure parent come first
	//Exporter->SortActorsHierarchy( ActorToExport );
	int32 ActorCount = ActorToExport.Num();
	for( int32 ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex )
	{
		AActor* Actor = ActorToExport[ ActorIndex ];
		//We export only valid actor
		if( Actor == nullptr )
			continue;

		bool bIsBlueprintClass = false;
		if( UClass* ActorClass = Actor->GetClass() )
		{
			// Check if we export the actor as a blueprint
			bIsBlueprintClass = UBlueprint::GetBlueprintFromClass( ActorClass ) != nullptr;
		}

		//Blueprint can be any type of actor so it must be done first
		if( bIsBlueprintClass )
		{
			// Export blueprint actors and all their components.
			//ExportActor( Exporter, Actor, true, NodeNameAdapter, bSaveAnimSeq );

			TArray<UActorComponent*> comps;
			Actor->GetComponents( comps );
			
			for( int i = 0; i < comps.Num(); i++ )
			{
				UActorComponent* AC = comps[ i ];
				UStaticMeshComponent* StaticMeshComp = dynamic_cast<UStaticMeshComponent*>( AC );
				if( StaticMeshComp )
				{
					ExportStaticMesh( Exporter, Actor, StaticMeshComp, NodeNameAdapter );
				}
			}			
		}
		//else if( Actor->IsA( ALight::StaticClass() ) )
		//{
		//	Exporter->ExportLight( (ALight*)Actor, NodeNameAdapter );
		//}
		else if( Actor->IsA( AStaticMeshActor::StaticClass() ) )
		{
			ExportStaticMesh( Exporter, Actor, CastChecked<AStaticMeshActor>( Actor )->GetStaticMeshComponent(), NodeNameAdapter );
		}
		//else if( Actor->IsA( ALandscapeProxy::StaticClass() ) )
		//{
		//	Exporter->ExportLandscape( CastChecked<ALandscapeProxy>( Actor ), false, NodeNameAdapter );
		//}
		//else if( Actor->IsA( ABrush::StaticClass() ) )
		//{
		//	// All brushes should be included within the world geometry exported above.
		//	Exporter->ExportBrush( (ABrush*)Actor, NULL, 0, NodeNameAdapter );
		//}
		//else if( Actor->IsA( AEmitter::StaticClass() ) )
		//{
		//	Exporter->ExportActor( Actor, false, NodeNameAdapter, bSaveAnimSeq ); // Just export the placement of the particle emitter.
		//}
		//else if( Actor->IsA( ACameraActor::StaticClass() ) )
		//{
		//	Exporter->ExportCamera( CastChecked<ACameraActor>( Actor ), false, NodeNameAdapter );
		//}
		else
		{
			// Export any other type of actors and all their components.
			ExportActor( Exporter, Actor, true, NodeNameAdapter, bSaveAnimSeq );
		}
	}
}
void CreateDocument( FFbxExporter* Exporter )
{
	SdkManager = FbxManager::Create();

	// create an IOSettings object
	FbxIOSettings* ios = FbxIOSettings::Create( SdkManager, "IOSROOT");
	SdkManager->SetIOSettings( ios );

	DefaultCamera = NULL;

	TheScene = FbxScene::Create( SdkManager, "" );

	// create scene info
	FbxDocumentInfo* SceneInfo = FbxDocumentInfo::Create( SdkManager, "SceneInfo" );
	SceneInfo->mTitle = "Unreal FBX Exporter";
	SceneInfo->mSubject = "Export FBX meshes from Unreal";
	SceneInfo->Original_ApplicationVendor.Set( "Ciprian Stanciu" );
	SceneInfo->Original_ApplicationName.Set( "UnrealToUnity" );
	SceneInfo->Original_ApplicationVersion.Set( "1.17" );
	SceneInfo->LastSaved_ApplicationVendor.Set( "Ciprian Stanciu" );
	SceneInfo->LastSaved_ApplicationName.Set( "UnrealToUnity" );
	SceneInfo->LastSaved_ApplicationVersion.Set( "1.17" );

	TheScene->SetSceneInfo( SceneInfo );

	//FbxScene->GetGlobalSettings().SetOriginalUpAxis(KFbxAxisSystem::Max);
	FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd;
	if( Exporter->GetExportOptions()->bForceFrontXAxis )
		FrontVector = FbxAxisSystem::eParityEven;

	const FbxAxisSystem UnrealZUp( FbxAxisSystem::eZAxis, FrontVector, FbxAxisSystem::eRightHanded );
	TheScene->GetGlobalSettings().SetAxisSystem( UnrealZUp );
	TheScene->GetGlobalSettings().SetOriginalUpAxis( UnrealZUp );
	// Maya use cm by default
	TheScene->GetGlobalSettings().SetSystemUnit( FbxSystemUnit::cm );
	//FbxScene->GetGlobalSettings().SetOriginalSystemUnit( KFbxSystemUnit::m );

	//bSceneGlobalTimeLineSet = false;
	TheScene->GetGlobalSettings().SetTimeMode( FbxTime::eDefaultMode );

	// setup anim stack
	AnimStack = FbxAnimStack::Create( TheScene, "Unreal Take" );
	//KFbxSet<KTime>(AnimStack->LocalStart, KTIME_ONE_SECOND);
	AnimStack->Description.Set( "Animation Take for Unreal." );

	// this take contains one base layer. In fact having at least one layer is mandatory.
	AnimLayer = FbxAnimLayer::Create( TheScene, "Base Layer" );
	AnimStack->AddMember( AnimLayer );
}
void CloseDocument()
{
	FbxActors.Reset();
	FbxSkeletonRoots.Reset();
	FbxMaterials.Reset();
	FbxMeshes.Reset();
	FbxCollisionMeshes.Reset();
	FbxNodeNameToIndexMap.Reset();

	if( TheScene )
	{
		TheScene->Destroy();
		TheScene = NULL;
	}
	//bSceneGlobalTimeLineSet = false;
}
#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(SdkManager->GetIOSettings()))
#endif
void WriteToFile( FFbxExporter* UEExporter, const TCHAR* Filename )
{
	int32 Major, Minor, Revision;
	bool Status = true;

	int32 FileFormat = -1;
	bool bEmbedMedia = false;
	bool bASCII = UEExporter->GetExportOptions()->bASCII;

	// Create an exporter.
	FbxExporter* Exporter = FbxExporter::Create( SdkManager, "" );

	// set file format
	// Write in fall back format if pEmbedMedia is true
	if( bASCII )
	{
		FileFormat = SdkManager->GetIOPluginRegistry()->FindWriterIDByDescription( "FBX ascii (*.fbx)" );
	}
	else
	{
		FileFormat = SdkManager->GetIOPluginRegistry()->GetNativeWriterFormat();
	}

	// Set the export states. By default, the export states are always set to 
	// true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below 
	// shows how to change these states.

	IOS_REF.SetBoolProp( EXP_FBX_MATERIAL, true );
	IOS_REF.SetBoolProp( EXP_FBX_TEXTURE, true );
	IOS_REF.SetBoolProp( EXP_FBX_EMBEDDED, bEmbedMedia );
	IOS_REF.SetBoolProp( EXP_FBX_SHAPE, true );
	IOS_REF.SetBoolProp( EXP_FBX_GOBO, true );
	IOS_REF.SetBoolProp( EXP_FBX_ANIMATION, true );
	IOS_REF.SetBoolProp( EXP_FBX_GLOBAL_SETTINGS, true );
	IOS_REF.SetBoolProp( EXP_ASCIIFBX, bASCII );

	//Get the compatibility from the editor settings
	const char* CompatibilitySetting = FBX_2013_00_COMPATIBLE;
	const EFbxExportCompatibility FbxExportCompatibility = UEExporter->GetExportOptions()->FbxExportCompatibility;
	switch( FbxExportCompatibility )
	{
		case EFbxExportCompatibility::FBX_2011:
			CompatibilitySetting = FBX_2011_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2012:
			CompatibilitySetting = FBX_2012_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2013:
			CompatibilitySetting = FBX_2013_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2014:
			CompatibilitySetting = FBX_2014_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2016:
			CompatibilitySetting = FBX_2016_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2018:
			CompatibilitySetting = FBX_2018_00_COMPATIBLE;
			break;
		#if ENGINE_MINOR_VERSION >= 26
		case EFbxExportCompatibility::FBX_2019:
			CompatibilitySetting = FBX_2019_00_COMPATIBLE;
			break;
		case EFbxExportCompatibility::FBX_2020:
			CompatibilitySetting = FBX_2020_00_COMPATIBLE;
			break;
		#endif
		default:
			CompatibilitySetting = FBX_2013_00_COMPATIBLE;
			break;
	}

	// We export using FBX 2013 format because many users are still on that version and FBX 2014 files has compatibility issues with
	// normals when importing to an earlier version of the plugin
	if( !Exporter->SetFileExportVersion( CompatibilitySetting, FbxSceneRenamer::eNone ) )
	{
		UE_LOG( LogTemp, Warning, TEXT( "Call to KFbxExporter::SetFileExportVersion(FBX_2013_00_COMPATIBLE) to export 2013 fbx file format failed.\n" ) );
	}

	// Initialize the exporter by providing a filename.
	if( !Exporter->Initialize( TCHAR_TO_UTF8( Filename ), FileFormat, SdkManager->GetIOSettings() ) )
	{
		UE_LOG( LogTemp, Warning, TEXT( "Call to KFbxExporter::Initialize() failed.\n" ) );
		UE_LOG( LogTemp, Warning, TEXT( "Error returned: %s\n\n" ), Exporter->GetStatus().GetErrorString() );
		return;
	}

	FbxManager::GetFileFormatVersion( Major, Minor, Revision );
	UE_LOG( LogTemp, Warning, TEXT( "FBX version number for this version of the FBX SDK is %d.%d.%d\n\n" ), Major, Minor, Revision );

	// Export the scene.
	Status = Exporter->Export( TheScene );

	// Destroy the exporter.
	Exporter->Destroy();

	CloseDocument();

	return;
}
UFbxExportOption* GlobalFBXExportOptions = nullptr;
bool ExportCancel = false;
bool ShowFBXDialog = false;
void ExportFBXForActor( AActor *Actor, FString CurrentFilename )
{
	UWorld* World = Actor->GetWorld();

	UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
	
	if( !GlobalFBXExportOptions )
	{
		ExportCancel = false;
		//Show the fbx export dialog options
		bool ExportAll = false;
		if ( ShowFBXDialog )
			Exporter->FillExportOptions( false, true, CurrentFilename, ExportCancel, ExportAll );
		GlobalFBXExportOptions = Exporter->GetExportOptions();
	}
	else
		Exporter->SetExportOptionsOverride( GlobalFBXExportOptions );

	if( !ExportCancel )
	{
		CreateDocument( Exporter );

		GWarn->StatusUpdate( 0, 1, NSLOCTEXT( "UnrealEd", "ExportingToFBX", "Exporting To FBX" ) );
		{
			ULevel* Level = World->PersistentLevel;

			//if( bSelectedOnly )
			//{
			//	Exporter->ExportBSP( World->GetModel(), true );
			//}

			INodeNameAdapter NodeNameAdapter;
			bool bSaveAnimSeq = true;

			TArray<AActor*> ActorToExport;
			ActorToExport.Add( Actor );
			//Exporter->ExportLevelMesh( Level, bSelectedOnly, NodeNameAdapter );
			ExportLevelMesh( Exporter, Level, true, ActorToExport, NodeNameAdapter, bSaveAnimSeq );

			// Export streaming levels and actors
			//for( int32 CurLevelIndex = 0; CurLevelIndex < World->GetNumLevels(); ++CurLevelIndex )
			//{
			//	ULevel* CurLevel = World->GetLevel( CurLevelIndex );
			//	if( CurLevel != NULL && CurLevel != Level )
			//	{
			//		Exporter->ExportLevelMesh( CurLevel, bSelectedOnly, NodeNameAdapter );
			//	}
			//}
		}
		WriteToFile( Exporter , *CurrentFilename );
	}

	Exporter->SetExportOptionsOverride( nullptr );
}
bool ClearMetas = false;
void DoExportScene( UWorld* World, FString ExportFolder )
{
	UnityProjectFolder = ExportFolder;

#if WITH_EDITOR

	GlobalFeatureLevel = GMaxRHIFeatureLevel;

	ProcessingState = 0;
	AllMaterials.clear();
	AllTextures.clear();
	MeshList.clear();
	//MeshInstances.clear();

	FString AssetsFolder = GetUnityAssetsFolder();
	FString ShadersFolder = AssetsFolder + TEXT("Shaders/");
	FString ScriptsFolder = AssetsFolder + TEXT( "Scripts/");
	FString ProjectSettingsFolder = AssetsFolder + TEXT( "../ProjectSettings/");
	FString SettingsFolder = AssetsFolder + TEXT( "Settings/");
	FString PackagesFolder = AssetsFolder + TEXT( "../Packages/");
	FString HDRPDefaultResourcesFolder = AssetsFolder + TEXT( "../Assets/HDRPDefaultResources/");

	if( ClearMetas )
	{
		IFileManager& FileMan = IFileManager::Get();

		TArray<FString> DictionaryList;
		
		FileMan.FindFilesRecursive( DictionaryList, *AssetsFolder, TEXT( "*.meta" ), true, false );

		for( int i = 0; i < DictionaryList.Num(); i++ )
		{
			FString File = DictionaryList[ i ];
			bool Result = FPlatformFileManager::Get().GetPlatformFile().DeleteFile( *File );
			if( !Result )
			{
			}
		}

		bool RemoveLibrary = false;
		if( RemoveLibrary )
		{
			FString LibraryDir = GetUnityAssetsFolder();
			LibraryDir += TEXT( "../Library");
			bool DeletedLibrary = FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively( *LibraryDir );
		}

		FString MaterialsDir = GetUnityAssetsFolder();
		MaterialsDir += TEXT( "/Materials");
		bool DeletedMaterials = FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively( *MaterialsDir );

		FString MeshesDir = GetUnityAssetsFolder();
		MeshesDir += TEXT( "/Meshes");
		bool DeletedMeshes = FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively( *MeshesDir );
	}


	CurrentWorld = World;

	VerifyOrCreateDirectory( UnityProjectFolder );
	VerifyOrCreateDirectory( AssetsFolder );
	VerifyOrCreateDirectory( ShadersFolder );
	VerifyOrCreateDirectory( ProjectSettingsFolder );
	VerifyOrCreateDirectory( ScriptsFolder );

	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP )
	{
		CopyToOutput( TEXT( "URP/ProjectSettings.asset" ), TEXT( "ProjectSettings/ProjectSettings.asset"), false );
		CopyToOutput( TEXT( "URP/GraphicsSettings.asset" ), TEXT( "ProjectSettings/GraphicsSettings.asset"), false );
		CopyToOutput( TEXT( "URP/ProjectVersion.txt" ), TEXT( "ProjectSettings/ProjectVersion.txt"), false );
	}
	else if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
	{
		CopyToOutput( TEXT( "HDRP/ProjectSettings.asset" ), TEXT( "ProjectSettings/ProjectSettings.asset"), false );
		CopyToOutput( TEXT( "HDRP/GraphicsSettings.asset" ), TEXT( "ProjectSettings/GraphicsSettings.asset"), false );
		CopyToOutput( TEXT( "HDRP/ProjectVersion.txt" ), TEXT( "ProjectSettings/ProjectVersion.txt"), false );
	}
	else
	{
		CopyToOutput( TEXT( "ProjectSettings.asset" ), TEXT("ProjectSettings/ProjectSettings.asset" ), false );
		CopyToOutput( TEXT( "ProjectVersion.txt" ), TEXT("ProjectSettings/ProjectVersion.txt" ), false );
	}

	if( CVarUsePostImportTool.GetValueOnAnyThread() == 1 && CVarFBXPerActor.GetValueOnAnyThread() == 0 )
		CopyToOutput( TEXT("PostImportTool.cs" ), TEXT("Assets/Scripts/PostImportTool.cs" ), false );

	VerifyOrCreateDirectory( PackagesFolder );

	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_BUILTIN )
	{
		CopyToOutput( TEXT("manifest.json" ), TEXT("/Packages/manifest.json" ), false );
		CopyToOutput( TEXT("packages-lock.json" ), TEXT("/Packages/packages-lock.json" ), false );
	}
	else if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP )
	{
		VerifyOrCreateDirectory( SettingsFolder );

		CopyToOutput( TEXT("URP/ForwardRenderer.asset" ), TEXT("/Assets/Settings/ForwardRenderer.asset" ), false );
		CopyToOutput( TEXT("URP/UniversalRP-HighQuality.asset" ), TEXT("/Assets/Settings/UniversalRP-HighQuality.asset" ), false );
		CopyToOutput( TEXT("URP/ForwardRenderer.asset.meta" ), TEXT("/Assets/Settings/ForwardRenderer.asset.meta" ), false );
		CopyToOutput( TEXT("URP/UniversalRP-HighQuality.asset.meta" ), TEXT("/Assets/Settings/UniversalRP-HighQuality.asset.meta" ), false );
		CopyToOutput( TEXT("URP/manifest.json" ), TEXT("/Packages/manifest.json" ), false );
		CopyToOutput( TEXT("URP/packages-lock.json" ), TEXT("/Packages/packages-lock.json" ), false );
		//CopyToOutput( TEXT("URP/URPCommonPass.hlsl", TEXT("/Assets/Shaders/URPCommonPass.hlsl" );
	}
	else if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
	{
		VerifyOrCreateDirectory( HDRPDefaultResourcesFolder );

		CopyToOutput( TEXT("HDRP/DefaultHDRPAsset.asset" ), TEXT("/Assets/HDRPDefaultResources/DefaultHDRPAsset.asset" ), false );
		CopyToOutput( TEXT("HDRP/DefaultHDRPAsset.asset.meta" ), TEXT("/Assets/HDRPDefaultResources/DefaultHDRPAsset.asset.meta" ), false );
		CopyToOutput( TEXT("HDRP/manifest.json" ), TEXT("/Packages/manifest.json" ), false );
		CopyToOutput( TEXT("HDRP/packages-lock.json" ), TEXT("/Packages/packages-lock.json" ), false );
		//CopyToOutput( TEXT("URP/URPCommonPass.hlsl", TEXT("/Assets/Shaders/URPCommonPass.hlsl" );
	}

	FString SceneFile = GetUnityAssetsFolder();
	SceneFile += *World->GetName();
	SceneFile += TEXT(".unity" );

	UnityScene = AllocateScene();

	UnityScene->UsePrefabs = ( CVarUsePrefabs.GetValueOnAnyThread() == 1 );

	int32 AllActors = CurrentWorld->GetProgressDenominator();

	{
		FScopedSlowTask ActorProgress( AllActors );
		ActorProgress.MakeDialog();
		//SlowTask.EnterProgressFrame();

		for( TActorIterator<AActor> ActorItr( CurrentWorld ); ActorItr; ++ActorItr )
		{
			auto Current = ActorItr.GetProgressNumerator();
			ActorProgress.EnterProgressFrame( 1.0f, LOCTEXT( "ProcessingActors", "ProcessingActors" ) );

			//Ignore HLOD Actors
			ALODActor* LODActor = Cast< ALODActor >( *ActorItr );
			if( LODActor )
				continue;
			ProcessActor( *ActorItr, UnityScene );
		}

	}

	TArray<UTexture*> AllUsedTextures;
	int MaterialGUIDCollisions = 0;
	int MaterialShaderGUIDCollisions = 0;
	for( int i = 0; i < AllMaterials.size(); i++ )
	{
		auto Material = AllMaterials[ i ];
		CountTextures( Material, AllUsedTextures );

		for( int u = i + 1; u < AllMaterials.size(); u++ )
		{
			auto MaterialB = AllMaterials[ i ];
			if( Material->UnityMat->GUID.compare( MaterialB->UnityMat->GUID ) == 0 )
			{
				MaterialGUIDCollisions++;
			}
			if( Material->UnityMat->ShaderGUID.compare( MaterialB->UnityMat->ShaderGUID ) == 0 )
			{
				MaterialShaderGUIDCollisions++;
			}
		}
	}

	{
		int TotalTextureCount = AllUsedTextures.Num();
		FScopedSlowTask TexturesProgress( TotalTextureCount );
		TexturesProgress.MakeDialog();

		int PrevCount = 0;
		for( int i = 0; i < AllMaterials.size(); i++ )
		{			
			auto Material = AllMaterials[ i ];
			ExportMaterialTextures( Material, AllUsedTextures, TotalTextureCount );
			int CurrentCount = TotalTextureCount - AllUsedTextures.Num();
			GetMaterialShaderSource( Material, true );
			if( Material->MaterialInterface )
			{
				Material->BaseMaterial = Material->MaterialInterface->GetMaterial();
				Material->MaterialInstance = Cast<UMaterialInstance>( Material->MaterialInterface );
			}
			if( CurrentCount > PrevCount )
			{
				int Dif = CurrentCount - PrevCount;
				PrevCount = CurrentCount;
				TexturesProgress.EnterProgressFrame( Dif, LOCTEXT( "ExportingTextures", "Exporting Textures" ) );
			}
		}

		//TexturesProgress.Destroy();
	}

	bool Sync = false;// true;
	RTTI->QueCommand( Sync );

	while( ProcessingState < 1 )
	{
		FPlatformProcess::Sleep( 0.01f );
	}

	for( int i = 0; i < AllTextures.size(); i++ )
	{
		auto Binding = AllTextures[ i ];
		
		if( !Binding->UnityTex )
		{
			//UE_LOG( LogEngine, Error, TEXT( "Texture %d has no UnityTex !!" ), i );
			continue;
		}

		std::string TexMetaFile = Binding->UnityTex->File;
		TexMetaFile += ".meta";

		int sRGB = Binding->UnrealTexture->SRGB;
		UnityTextureShape Shape = UTS_2D;
		int TileX = 0, TileY = 0;
		if( Cast<UTextureCube>( Binding->UnrealTexture ) )
			Shape = UTS_Cube;
		if( Cast<UVolumeTexture>( Binding->UnrealTexture ) )
		{
			Shape = UTS_3D;
			UVolumeTexture* VolTex = Cast<UVolumeTexture>( Binding->UnrealTexture );
			UTexture2D* SourceTex = nullptr;
			#if ENGINE_MAJOR_VERSION == 4
				SourceTex = VolTex->Source2DTexture;
			#else
				SourceTex = VolTex->Source2DTexture.Get();
			#endif
			if( SourceTex )
			{
				TileX = SourceTex->Source.GetSizeX() / VolTex->Source2DTileSizeX;
				TileY = SourceTex->Source.GetSizeY() / VolTex->Source2DTileSizeY;
			}
		}
		
		std::string TexMetaContents = GenerateTextureMeta( Binding->UnityTex->GUID, Binding->IsNormalMap, sRGB, Shape, TileX, TileY );
		if ( ExportTextures )
			::SaveFile( TexMetaFile.c_str(), (uint32*)TexMetaContents.c_str(), TexMetaContents.length() );
	}

	for( int i = 0; i < AllMaterials.size(); i++ )
	{
		MaterialBinding* Mat = AllMaterials[ i ];

		FString MatFile = GetUnityAssetsFolder();

		if( CVarUseOriginalPaths.GetValueOnAnyThread() == 1 )
		{
			MatFile += GetAssetPathFolder( Mat->MaterialInterface );
		}
		else
		{
			MatFile += TEXT("/Materials/" );
		}
		CreateAllDirectories( MatFile );

		MatFile += ToWideString( Mat->UnityMat->Name ).c_str();
		MatFile += TEXT(".mat" );
		Mat->UnityMat->File = ToANSIString( *MatFile );

		std::string MatFileContents = Mat->UnityMat->GenerateMaterialFile();
		::SaveFile( ToANSIString( *MatFile ).c_str(), (uint32*)MatFileContents.c_str(), MatFileContents.length() );

		FString MatMetaFile = MatFile;
		MatMetaFile += TEXT(".meta" );
		
		Mat->UnityMat->GenerateGUID();
		std::string MaterialMetaContents = GenerateMaterialMeta( Mat->UnityMat->GUID );
		::SaveFile( ToANSIString( *MatMetaFile ).c_str(), (uint32*)MaterialMetaContents.c_str(), MaterialMetaContents.length() );
	}
	
	{
		FScopedSlowTask MeshesProgress( MeshList.size() );
		MeshesProgress.MakeDialog();

		for( int i = 0; i < MeshList.size(); i++ )
		{
			MeshesProgress.EnterProgressFrame( 1.0f, LOCTEXT( "ExportingMeshes", "Exporting Meshes" ) );
			MeshBinding* MB = MeshList[ i ];
			UnityMesh* M = MB->TheUnityMesh;

			FString MeshFile = M->File.c_str();

			UE_LOG( LogTemp, Log, TEXT( "Writing Mesh %s (%d/%d)" ), *MeshFile, i + 1, MeshList.size() );

			//if( CVarFBXPerActor.GetValueOnAnyThread() )
			{
				FString Filename = MeshFile;
				ExportFBXForActor( MB->OwnerActor, Filename );
			}
			//else
				//ExportOBJ( ToANSIString( *MeshFile ).c_str(), M );

			FString MeshMetaFile = MeshFile;
			MeshMetaFile += TEXT(".meta" );

			std::string MetaData = GenerateMeshMeta( M, ( CVarFBXPerActor.GetValueOnAnyThread() == 1 ), MB->TotalLods );
			int MetaDataLen = MetaData.length();
			::SaveFile( ToANSIString( *MeshMetaFile ).c_str(), (uint32*)MetaData.c_str(), MetaDataLen );
		}
	}

	bool WriteScene = true;

	if( WriteScene )
	{
		FString ProxySceneFile = GetResourceDir();
		ProxySceneFile += TEXT( "Proxy.unity" );

		if( UnityScene->UsePrefabs )
		{
			FString PrefabFolder = GetUnityAssetsFolder();
			PrefabFolder += TEXT("Prefabs/" );
			VerifyOrCreateDirectory( PrefabFolder );
			UnityScene->FixDuplicatePrefabNames();
			UnityScene->WritePrefabs( ToANSIString( *PrefabFolder ).c_str() );
		}

		UnityScene->WriteScene( ToANSIString(* SceneFile).c_str(), ToANSIString(*ProxySceneFile).c_str() );
	}
	else
	{
		//Export FBX
		FString FBXFile = GetUnityAssetsFolder();
		FBXFile += *( World->GetName() );
		FBXFile += TEXT(".fbx" );
		GUnrealEd->ExportMap( World, *FBXFile, false );

		FString FBXFileMeta = FBXFile;
		FBXFileMeta += TEXT(".meta" );

		std::string MetaData = GenerateExportedFBXMeta();
		int MetaDataLen = MetaData.length();
		::SaveFile( ToANSIString( *FBXFileMeta ).c_str(), (uint32*)MetaData.c_str(), MetaDataLen );
	}

	//Reset these to give a chance of changing them on subsequent exports
	GlobalFBXExportOptions = nullptr;
	#endif
}
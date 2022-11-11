
#include "Module.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "LevelEditor.h"
#include "ExportActor.h"

#include "AssetRegistryModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include <functional>

#include "Editor/MaterialEditor/Public/MaterialEditingLibrary.h"
#include "Engine/Classes/Materials/MaterialExpressionTextureCoordinate.h"
#include "Engine/Classes/Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Engine/Classes/Materials/MaterialExpressionRuntimeVirtualTextureSampleParameter.h"
#include "Engine/Classes/Materials/MaterialExpressionConstant4Vector.h"
#include "Engine/Classes/Materials/MaterialExpressionConstant3Vector.h"
#include "Engine/Classes/Materials/MaterialExpressionWorldPosition.h"
#include "Engine/Classes/Materials/MaterialExpressionComponentMask.h"
#include "Engine/Classes/Materials/MaterialExpressionVertexInterpolator.h"
#include "Engine/Classes/Materials/MaterialExpressionDepthFade.h"
#include "Engine/Classes/Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Engine/Classes/Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Engine/Classes/Materials/MaterialExpressionTextureSample.h"
#include "Engine/Classes/Materials/MaterialExpressionTextureObject.h"
#include "Runtime/Landscape/Classes/Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Runtime/Foliage/Public/InstancedFoliageActor.h"
#include "Engine/Classes/Engine/StaticMeshActor.h"
#include "Engine/Classes/Engine/DecalActor.h"
#include "Runtime/Engine/Classes/Components/DecalComponent.h"

#include "ExportLandscape.h"

#include "Runtime/StaticMeshDescription/Public/StaticMeshAttributes.h"
#include "Runtime/StaticMeshDescription/Public/StaticMeshOperations.h"
#include "Runtime/RawMesh/Public/RawMesh.h"
#include "Runtime/Engine/Public/SkeletalRenderPublic.h"
#include "Engine/Classes/Engine/RendererSettings.h"
#include "Editor/UnrealEd/Public/FileHelpers.h"
//Function params
#include "IStructureDetailsView.h"
#include "Widgets/Layout/SScrollBox.h"

TAutoConsoleVariable<int> CVarMaxFoliageActors(
	TEXT( "utu.MaxFoliageActors" ),
	100000,
	TEXT( "Max foliage instance threshold after which you get a warning and ability to skip foliage" ),
	ECVF_Cheat );

#define LOCTEXT_NAMESPACE "FUnrealToUnityModule"

void FUnrealToUnityModule::StartupModule()
{
	//return;
	FUnrealToUnity_Commands::Register();
	BindEditorCommands();
	
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable( new FExtender );
	ToolbarExtender->AddToolBarExtension(
	#if ENGINE_MAJOR_VERSION == 4
		"Game",
	#else
		"File",
	#endif
		EExtensionHook::After,
		NULL,
		FToolBarExtensionDelegate::CreateRaw( this, &FUnrealToUnityModule::AddCustomMenu )
	);

	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender( ToolbarExtender );
}

#define MapActionHelper(x) EditorCommands->MapAction( Commands.x, FExecuteAction::CreateRaw(this, &FUnrealToUnityModule::x), FCanExecuteAction(), FIsActionChecked())

void FUnrealToUnityModule::BindEditorCommands()
{
	if( !EditorCommands.IsValid() )
	{
		EditorCommands = MakeShareable( new FUICommandList() );
	}

	const FUnrealToUnity_Commands& Commands = FUnrealToUnity_Commands::Get();

	MapActionHelper( Export );
	MapActionHelper( PlaceAllAssetsInScene );
	MapActionHelper( ConvertLandscapesToStaticMeshes );	
	MapActionHelper( PrepareForExport );
	MapActionHelper( DetectMaterialsWithWPO );
}

bool PreparedForExport = false;
void FUnrealToUnityModule::DoExport( RenderPipeline Pipeline, FString StrOutputFolder )
{
	CVarRenderPipeline.AsVariable()->Set( (int)Pipeline );

	DoPrepareForExport();	
	
	AExportActor::DoExport( *StrOutputFolder );
	PreparedForExport = false;
}

class SFunctionParamDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SFunctionParamDialog) {}

	/** Text to display on the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonText)

	/** Tooltip text for the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonTooltipText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope)
	{
		bOKPressed = false;

		// Initialize details view
		FDetailsViewArgs DetailsViewArgs;
		{
			DetailsViewArgs.bAllowSearch = false;
			DetailsViewArgs.bHideSelectionTip = true;
			DetailsViewArgs.bLockable = false;
			DetailsViewArgs.bSearchInitialKeyFocus = true;
			DetailsViewArgs.bUpdatesFromSelection = false;
			DetailsViewArgs.bShowOptions = false;
			DetailsViewArgs.bShowModifiedPropertiesOption = false;
			DetailsViewArgs.bShowActorLabel = false;
			DetailsViewArgs.bForceHiddenPropertyVisibility = true;
			DetailsViewArgs.bShowScrollBar = false;
		}
	
		FStructureDetailsViewArgs StructureViewArgs;
		{
			StructureViewArgs.bShowObjects = true;
			StructureViewArgs.bShowAssets = true;
			StructureViewArgs.bShowClasses = true;
			StructureViewArgs.bShowInterfaces = true;
		}

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		TSharedRef<IStructureDetailsView> StructureDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, InStructOnScope);

		StructureDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& InPropertyAndParent)
		{
			return InPropertyAndParent.Property.HasAnyPropertyFlags(CPF_Parm);
		}));

		StructureDetailsView->GetDetailsView()->ForceRefresh();

		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					StructureDetailsView->GetWidget().ToSharedRef()
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(6, 2))
						.OnClicked_Lambda([this, InParentWindow, InArgs]()
						{
							if(InParentWindow.IsValid())
							{
								InParentWindow.Pin()->RequestDestroyWindow();
							}
							bOKPressed = true;
							return FReply::Handled(); 
						})
						.ToolTipText(InArgs._OkButtonTooltipText)
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text(InArgs._OkButtonText)
						]
					]
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(6, 2))
						.OnClicked_Lambda([InParentWindow]()
						{ 
							if(InParentWindow.IsValid())
							{
								InParentWindow.Pin()->RequestDestroyWindow();
							}
							return FReply::Handled(); 
						})
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text(LOCTEXT("Cancel", "Cancel"))
						]
					]
				]
			]
		];
	}

	bool bOKPressed;
};
FString GetDefaultOutputFolder()
{
#if PLATFORM_WINDOWS
	FString StrOutputFolder = TEXT( "C:\\UnrealToUnity" );
#else
	FString StrOutputFolder = TEXT( "/Users/Shared/UnrealToUnity" );
#endif

	return StrOutputFolder;
}
void UClassForFunctionParams::Parameters( RenderPipeline ScriptableRenderPipeline, bool UseStandardShaders, bool ShowFBXExportDialog,
										  bool ExportVertexShader, FString OutputDirectory )
{
}
ParamsStructure::ParamsStructure()
{
	ScriptableRenderPipeline = RenderPipeline::RP_URP;
	UseStandardShaders = false;
	ShowFBXExportDialog = false;
	ExportVertexShader = false;// true;
	
	OutputDirectory = GetDefaultOutputFolder();
}
extern TAutoConsoleVariable<int> CVarUseStandardMaterial;
extern bool ShowFBXDialog;
extern bool ExportVS;
ParamsStructure Settings;
void FUnrealToUnityModule::Export()
{
	UClass* Class = UClassForFunctionParams::StaticClass();
	UFunction* UFunc = nullptr;
	for( TFieldIterator<UFunction> FunctionIt( Class ); FunctionIt; ++FunctionIt )
	{
		if( ( *FunctionIt )->GetName().Compare( TEXT( "Parameters" ) ) == 0 )
		{
			UFunc = *FunctionIt;
			break;
		}
	}

	TSharedRef<FStructOnScope> FuncParams = MakeShared<FStructOnScope>( UFunc );

	uint8* DefaultParams = FuncParams->GetStructMemory();
	memcpy( DefaultParams, &Settings, sizeof( Settings ) );

	FText Title = FText::FromString( TEXT( "Unreal To Unity" ) );
	// pop up a dialog to input params to the function
	TSharedRef<SWindow> Window = SNew( SWindow )
		.Title( Title )
		.ClientSize( FVector2D( 400, 200 ) )
		.SupportsMinimize( false )
		.SupportsMaximize( false );

	TSharedPtr<SFunctionParamDialog> Dialog;
	Window->SetContent(
		SAssignNew( Dialog, SFunctionParamDialog, Window, FuncParams )
		.OkButtonText( LOCTEXT( "OKButton", "OK" ) )
		.OkButtonTooltipText( UFunc->GetToolTipText() ) );

	GEditor->EditorAddModalWindow( Window );

	if( Dialog->bOKPressed )
	{
		uint8* Params = FuncParams->GetStructMemory();
		memcpy( &Settings, Params, sizeof( Settings ) );

		CVarUseStandardMaterial.AsVariable()->Set( (int)Settings.UseStandardShaders );
		ShowFBXDialog = Settings.ShowFBXExportDialog;
		ExportVS = Settings.ExportVertexShader;
		if ( Settings.OutputDirectory[ Settings.OutputDirectory.Len() - 1] != '/' )
			Settings.OutputDirectory += TEXT( "/" );
		
		DoExport( Settings.ScriptableRenderPipeline, Settings.OutputDirectory );
	}
}
void FUnrealToUnityModule::PrepareForExport()
{
	DoPrepareForExport();
}

void FUnrealToUnityModule::DoPrepareForExport()
{
	if( !PreparedForExport )
	{
		RemoveVirtualTextures();
		DoRemoveNanite();
		RemoveRVTs();
		RemoveVertexInterpolators();
		RemoveUnsupportedMaterialNodes();
		MoveFoliageToActors();
		FixVolumeMaterials();
		ConvertISMToActors();
		DetachMultipleComponentActors();
		//ConvertLandscapesToStaticMeshes();
		ConvertSplineMeshesToStaticActors();
		ConvertSkeletalMeshesToStaticActors();

		PreparedForExport = true;
	}
}
extern TAutoConsoleVariable<int> CVarRenderPipeline;

void FUnrealToUnityModule::AddCustomMenu( FToolBarBuilder& ToolbarBuilder )
{
	ToolbarBuilder.BeginSection( "UnrealToUnity" );
#if ENGINE_MAJOR_VERSION == 5
	ToolbarBuilder.SetLabelVisibility( EVisibility::Visible );
#endif
	{
		ToolbarBuilder.AddComboButton(
			FUIAction()
			, FOnGetContent::CreateRaw( this, &FUnrealToUnityModule::CreateMenuContent )
			, LOCTEXT( "UnrealToUnityLabel", "UnrealToUnity" )
			, LOCTEXT( "UnrealToUnityTooltip", "UnrealToUnity" )
			, FSlateIcon( FEditorStyle::GetStyleSetName(), "LevelEditor.GameSettings" )
		);
	}
	ToolbarBuilder.EndSection();
}
TSharedRef<SWidget> FUnrealToUnityModule::CreateMenuContent()
{
	FMenuBuilder MenuBuilder( true, EditorCommands );
	
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().Export );	
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().PlaceAllAssetsInScene );
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().ConvertLandscapesToStaticMeshes );
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().PrepareForExport );
	MenuBuilder.AddMenuEntry( FUnrealToUnity_Commands::Get().DetectMaterialsWithWPO );

	return MenuBuilder.MakeWidget();
}

void FUnrealToUnity_Commands::RegisterCommands()
{
	UI_COMMAND( Export, "Export...", "Export current scene to a unity project", EUserInterfaceActionType::Button, FInputChord() );	
	UI_COMMAND( PlaceAllAssetsInScene, "PlaceAllAssetsInScene", "Place all assets in the project in a grid", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ConvertLandscapesToStaticMeshes, "ConvertLandscapesToStaticMeshes", "Converts Landscapes To StaticMeshes. Also modifies their materials to work with Static Meshes", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( PrepareForExport, "PrepareForExport", "Prepares materials for export. Wait for all shaders to recompile before using Export", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( DetectMaterialsWithWPO, "DetectMaterialsWithWPO", "DetectMaterialsWithWPO", EUserInterfaceActionType::Button, FInputChord() );
}

void FUnrealToUnityModule::ShutdownModule()
{
}

void GetOutputExpression( FExpressionInput* MaterialInput, UMaterialExpression* Source, int OutputIndex, TArray< FExpressionInput*>& Inputs )
{
	if( MaterialInput->Expression == Source && MaterialInput->OutputIndex == OutputIndex )
		Inputs.Add( MaterialInput );
}
void GetOutputExpression( UMaterial* Material, UMaterialExpression* Source, int OutputIndex, TArray< FExpressionInput*>& Inputs )
{
	TArray<UMaterialExpression*> AllExpressions;
	Material->GetAllReferencedExpressions( AllExpressions, nullptr );

	for( int i = 0; i < AllExpressions.Num(); i++ )
	{
		UMaterialExpression* Exp = AllExpressions[ i ];

		TArray<FExpressionInput*> ExpInputs = Exp->GetInputs();
		for( int u = 0; u < ExpInputs.Num(); u++ )
		{
			UMaterialExpression* A = ExpInputs[ u ]->Expression;
			if( A == Source && ExpInputs[ u ]->OutputIndex == OutputIndex )
			{
				Inputs.Add( ExpInputs[ u ] );
			}
		}
	}

	GetOutputExpression( &Material->BaseColor, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->Metallic, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->Specular, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->Roughness, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->Anisotropy, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->Normal, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->Tangent, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->EmissiveColor, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->Opacity, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->OpacityMask, Source, OutputIndex, Inputs );

	GetOutputExpression( &Material->WorldPositionOffset, Source, OutputIndex, Inputs );
	#if ENGINE_MAJOR_VERSION == 4
	GetOutputExpression( &Material->WorldDisplacement, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->TessellationMultiplier, Source, OutputIndex, Inputs );
	#endif
	GetOutputExpression( &Material->SubsurfaceColor, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->ClearCoat, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->ClearCoatRoughness, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->AmbientOcclusion, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->Refraction, Source, OutputIndex, Inputs );

	GetOutputExpression( &Material->MaterialAttributes, Source, OutputIndex, Inputs );
	

	for( int i = 0; i < 8; i++ )
		GetOutputExpression( &Material->CustomizedUVs[ i ], Source, OutputIndex, Inputs );

	GetOutputExpression( &Material->PixelDepthOffset, Source, OutputIndex, Inputs );
	GetOutputExpression( &Material->ShadingModelFromMaterialExpression, Source, OutputIndex, Inputs );
}
UMaterialExpression* CreateRVTReplacementExpression( FExpressionOutput& OE, UMaterial* Material, FVector2D& Location )
{
	UMaterialExpression* NewExpression = nullptr;

	if( OE.OutputName.ToString().Compare( "BaseColor" ) == 0 ||
		OE.OutputName.ToString().Compare( "Normal" ) == 0 )
	{
		//1, 1, 1, 0
		NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionConstant3Vector::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionConstant3Vector* NewExpC = Cast < UMaterialExpressionConstant3Vector>( NewExpression );

		if( OE.OutputName.ToString().Compare( "BaseColor" ) == 0 )
		{
			NewExpC->Constant.R = NewExpC->Constant.G = NewExpC->Constant.B = 1;
			NewExpC->Constant.A = 0;
		}
		else
		{
			NewExpC->Constant.R = 0;
			NewExpC->Constant.G = 0;
			NewExpC->Constant.B = 1;
			NewExpC->Constant.A = 0;
		}

		Location.Y += 140;
	}
	else if( OE.OutputName.ToString().Compare( "Specular" ) == 0 ||
			 OE.OutputName.ToString().Compare( "Roughness" ) == 0 ||			 
			 OE.OutputName.ToString().Compare( "Mask" ) == 0 )
	{
		NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionConstant::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionConstant* NewExpC = Cast < UMaterialExpressionConstant>( NewExpression );

		if( OE.OutputName.ToString().Compare( "Roughness" ) == 0 )
			NewExpC->R = 1;
		else
			NewExpC->R = 0;

		Location.Y += 60;
	}
	else if( OE.OutputName.ToString().Compare( "WorldHeight" ) == 0 )
	{
		UMaterialExpression* NewExp1 = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionWorldPosition::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionWorldPosition* WPExp = Cast < UMaterialExpressionWorldPosition>( NewExp1 );
		WPExp->Desc = TEXT( "Replacement for RVT->WorldHeight" );

		Location.X += 60;
		Location.Y += 60;

		UMaterialExpression* NewExp2 = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionComponentMask::StaticClass(), nullptr, Location.X, Location.Y );
		UMaterialExpressionComponentMask* CMExp = Cast < UMaterialExpressionComponentMask>( NewExp2 );

		CMExp->R = 0;
		CMExp->G = 0;
		CMExp->B = 1;//Z
		CMExp->A = 0;

		CMExp->Input.Expression = WPExp;

		Location.Y += 60;

		NewExpression = CMExp;
	}

	return NewExpression;
}
void IterateOverAllAssetsOfType( FName TypeName, std::function<bool( FAssetData& )> lambda, bool SaveAndUnload = true )
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );

	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssetsByClass( TypeName, Assets );

	int current = 0;
	int Offset = 0;
	int threshold = 300;
	TArray< UPackage*> Packages;
	TArray< UPackage*> SavePackages;
	bool bOutAnyPackagesUnloaded;
	FText OutErrorMessage;
	for( int i = 0; i < Assets.Num(); i++)
	{
		FAssetData& Asset = Assets[ i ];
		if( Offset <= current )
		{
			if( Asset.PackageName.ToString().Contains( "/Game/Developers/" ) ||
				!Asset.PackageName.ToString().StartsWith( "/Game" ) )
			{
				continue;
			}

			UObject* Object = Asset.GetAsset();
			UPackage* Package = Object->GetPackage();
			Packages.Add( Package );
			bool Modified = lambda( Asset );
			if( Modified )
			{
				SavePackages.Add( Package );
			}
			Package->GetMetaData()->ClearFlags( RF_Standalone );

			if( SaveAndUnload && Packages.Num() % threshold == 0 )
			{
				if( SavePackages.Num() > 0 )
				{
					UEditorLoadingAndSavingUtils::SavePackages( SavePackages, true );
					SavePackages.Reset();
				}
			
				UEditorLoadingAndSavingUtils::UnloadPackages( Packages, bOutAnyPackagesUnloaded, OutErrorMessage );
				Packages.Reset();

				FlushRenderingCommands();
			}
		}
		current++;
	}

	if( SaveAndUnload )
	{
		UEditorLoadingAndSavingUtils::SavePackages( SavePackages, true );
		SavePackages.Reset();

		UEditorLoadingAndSavingUtils::UnloadPackages( Packages, bOutAnyPackagesUnloaded, OutErrorMessage );

		FlushRenderingCommands();
	}
}
void IterateOverSelection( std::function<void( FAssetData& )> lambda )
{
	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>( "ContentBrowser" ).Get();

	TArray<FAssetData> SelectedAssets;
	ContentBrowser.GetSelectedAssets( SelectedAssets );

	int current = 0;
	for( FAssetData& Asset : SelectedAssets )
	{
		lambda( Asset );
		current++;
	}
}
EMaterialSamplerType GetNonVirtualSampler( EMaterialSamplerType Type)
{
	switch( Type )
	{
		default:
		case SAMPLERTYPE_VirtualColor: return SAMPLERTYPE_Color;
		case SAMPLERTYPE_VirtualGrayscale: return SAMPLERTYPE_Grayscale;
		case SAMPLERTYPE_VirtualAlpha: return SAMPLERTYPE_Alpha;
		case SAMPLERTYPE_VirtualNormal: return SAMPLERTYPE_Normal;
		case SAMPLERTYPE_VirtualMasks: return SAMPLERTYPE_Masks;

		case SAMPLERTYPE_VirtualLinearColor: return SAMPLERTYPE_LinearColor;
		case SAMPLERTYPE_VirtualLinearGrayscale: return SAMPLERTYPE_LinearGrayscale;
	}
}

class UTexture2DPublic : public UTexture2D
{
public:
	void CancelCachePlatformData()
	{
		#if ENGINE_MAJOR_VERSION >= 5
			while( !UTexture2D::TryCancelCachePlatformData() )
			{
			
				FPlatformProcess::Sleep( 0.01f );
			};
		#else
			UTexture2D::FinishCachePlatformData();
		#endif
	}
};

void FUnrealToUnityModule::RemoveVirtualTextures()
{
	if( !GetDefault<URendererSettings>()->bVirtualTextures )
		return;
	
	TArray<FString> Messages;
	auto lambdaTextures = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UTexture2D* Tex = Cast<UTexture2D>( Asset.GetAsset() );
		if( Tex )
		{
			if( Tex->VirtualTextureStreaming )
			{
				((UTexture2DPublic*)Tex)->CancelCachePlatformData();
				Tex->PreEditChange( nullptr );
				Tex->VirtualTextureStreaming = false;
				Modified = true;
			}
			
			if( Modified )
			{
				Tex->PostEditChange();
				Tex->Modify();
			}

			Tex->ClearFlags( RF_Standalone );
			Tex->AssetImportData = nullptr;
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Texture2D" ), lambdaTextures );
	//IterateOverSelection( lambdaTextures );

	auto lambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			for( int i = 0; i < Material->Expressions.Num(); i++ )
			{
				UMaterialExpression* Exp = Material->Expressions[ i ];
				UMaterialExpressionTextureBase* TexBase = Cast< UMaterialExpressionTextureBase>( Exp );
				if( TexBase && IsVirtualSamplerType( TexBase->SamplerType ) )
				{
					TexBase->SamplerType = GetNonVirtualSampler( TexBase->SamplerType );
					Exp->PostEditChange();
					Modified = true;
				}
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), lambda );
	//IterateOverSelection( lambda );

	auto MaterialFunctionLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>( Asset.GetAsset() );
		if( MaterialFunction )
		{
			TArray<UMaterialExpression*> TempExpArray = GetFunctionExpressions( MaterialFunction );
			const TArray<UMaterialExpression*>* OutExpressions = &TempExpArray;			
			for( int i = 0; i < OutExpressions->Num(); i++ )
			{
				UMaterialExpression* Exp = ( *OutExpressions )[ i ];
				UMaterialExpressionTextureBase* TexBase = Cast< UMaterialExpressionTextureBase>( Exp );
				if( TexBase && IsVirtualSamplerType( TexBase->SamplerType ) )
				{
					TexBase->SamplerType = GetNonVirtualSampler( TexBase->SamplerType );
					Exp->PostEditChange();
					Modified = true;
				}
				
			}

			if( Modified )
			{
				MaterialFunction->PostEditChange();
				MaterialFunction->MarkPackageDirty();

				UMaterialEditingLibrary::UpdateMaterialFunction( MaterialFunction, nullptr );				
			}						
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "MaterialFunction" ), MaterialFunctionLambda );
	//IterateOverSelection( MaterialFunctionLambda );
}

void FUnrealToUnityModule::DoRemoveNanite()
{
#if ENGINE_MAJOR_VERSION == 5
	auto lambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UStaticMesh* StaticMesh = Cast<UStaticMesh>( Asset.GetAsset() );
		if( StaticMesh )
		{
			if( StaticMesh->NaniteSettings.bEnabled )
			{
				StaticMesh->NaniteSettings.bEnabled = false;
				Modified = true;
			}
		
			if( Modified )
			{
				StaticMesh->PostEditChange();
				StaticMesh->MarkPackageDirty();
			}
			StaticMesh->ClearFlags( RF_Standalone );
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "StaticMesh" ), lambda );
	//IterateOverSelection( lambda );	
#endif
}
void FUnrealToUnityModule::RemoveRVTs()
{
	TArray<FString> Messages;
	auto lambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			TArray<UMaterialExpression*> OutExpressions;
			Material->GetAllReferencedExpressions( OutExpressions, nullptr );
			for( int i = 0; i < OutExpressions.Num(); i++ )
			{
				UMaterialExpression* Exp = OutExpressions[ i ];
				UMaterialExpressionRuntimeVirtualTextureSample* RVTExp = Cast< UMaterialExpressionRuntimeVirtualTextureSample>( Exp );
				UMaterialExpressionRuntimeVirtualTextureSampleParameter* RVTExp2 = Cast< UMaterialExpressionRuntimeVirtualTextureSampleParameter>( Exp );
				if( RVTExp || RVTExp2 )
				{
					TArray<FExpressionOutput>& Outputs = Exp->GetOutputs();

					FVector2D Location( Exp->MaterialExpressionEditorX + 250, Exp->MaterialExpressionEditorY );

					for( int o = 0; o < Outputs.Num(); o++ )
					{
						FExpressionOutput& OE = Outputs[ o ];
						TArray< FExpressionInput*> Inputs;
						GetOutputExpression( Material, Exp, o, Inputs );
						if( Inputs.Num() > 0 )
						{
							auto NewExp = CreateRVTReplacementExpression( OE, Material, Location );

							for( int j = 0; j < Inputs.Num(); j++ )
							{
								auto ExpInput = Inputs[ j ];
								ExpInput->Expression = NewExp;
							}
							Modified = true;
						}
					}
				}
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}	
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), lambda );
	//IterateOverSelection( lambda );	
}
void ConnectExpressionOutputToOtherExpression( UMaterial* BaseMaterial, UMaterialFunction* MaterialFunction, UMaterialExpression* TargetExpression,
											   UMaterialExpression* OtherExpression, int OutputIndex, int Mask, int MaskR, int MaskG, int MaskB, int MaskA  )
{
	TArray<UMaterialExpression*> Connections;
	TArray<FExpressionInput*> OutputConnections;
	GetExpressionConnections( TargetExpression, BaseMaterial, MaterialFunction, Connections, OutputConnections );
	for( int u = 0; u < Connections.Num(); u++ )
	{
		UMaterialExpression* Connection = Connections[ u ];
		auto Inputs = Connection->GetInputs();
		for( int m = 0; m < Inputs.Num(); m++ )
		{
			if( Inputs[ m ]->Expression == TargetExpression )
			{
				Inputs[ m ]->Expression = OtherExpression;
				Inputs[ m ]->OutputIndex = OutputIndex;
				Inputs[ m ]->Mask = Mask;
				Inputs[ m ]->MaskR = MaskR;
				Inputs[ m ]->MaskG = MaskG;
				Inputs[ m ]->MaskB = MaskB;
				Inputs[ m ]->MaskA = MaskA;
			}
		}
	}
	for( int i = 0; i < OutputConnections.Num(); i++ )
	{
		OutputConnections[ i ]->Expression = OtherExpression;
		OutputConnections[ i ]->OutputIndex = OutputIndex;
		OutputConnections[ i ]->Mask = Mask;
		OutputConnections[ i ]->MaskR = MaskR;
		OutputConnections[ i ]->MaskG = MaskG;
		OutputConnections[ i ]->MaskB = MaskB;
		OutputConnections[ i ]->MaskA = MaskA;
	}
}
const TArray<UMaterialExpression*> GetFunctionExpressions( UMaterialFunctionInterface* MaterialFunction );
void GetExpressionConnections( UMaterialExpression* Exp, UMaterial* Material, UMaterialFunction* MaterialFunction, TArray<UMaterialExpression*>& Connections,
							   TArray<FExpressionInput*>& OutputConnections )
{
	const TArray<UMaterialExpression*>* ExpressionsPointer = nullptr;
	TArray<UMaterialExpression*> ExpressionsArray;
	TArray<UMaterialExpression*> TempExpArray;
	if( Material )
	{
		Material->GetAllReferencedExpressions( ExpressionsArray, nullptr );
		ExpressionsPointer = &ExpressionsArray;

		TArray<FExpressionInput*> MaterialOutputs;

		MaterialOutputs.Add( &Material->BaseColor );
		MaterialOutputs.Add( &Material->Metallic );
		MaterialOutputs.Add( &Material->Specular );
		MaterialOutputs.Add( &Material->Roughness );
		MaterialOutputs.Add( &Material->Anisotropy );
		MaterialOutputs.Add( &Material->Normal );
		MaterialOutputs.Add( &Material->Tangent );
		MaterialOutputs.Add( &Material->EmissiveColor );
		MaterialOutputs.Add( &Material->Opacity );
		MaterialOutputs.Add( &Material->OpacityMask );

		MaterialOutputs.Add( &Material->WorldPositionOffset );
		#if ENGINE_MAJOR_VERSION == 4
		MaterialOutputs.Add( &Material->WorldDisplacement );
		MaterialOutputs.Add( &Material->TessellationMultiplier );
		#endif
		MaterialOutputs.Add( &Material->SubsurfaceColor );
		MaterialOutputs.Add( &Material->ClearCoat );
		MaterialOutputs.Add( &Material->ClearCoatRoughness );
		MaterialOutputs.Add( &Material->AmbientOcclusion );
		MaterialOutputs.Add( &Material->Refraction );

		for( int i = 0; i < 8; i++ )
			MaterialOutputs.Add( &Material->CustomizedUVs[ i ] );

		MaterialOutputs.Add( &Material->PixelDepthOffset );
		MaterialOutputs.Add( &Material->ShadingModelFromMaterialExpression );

		for( int i = 0; i < MaterialOutputs.Num(); i++ )
		{
			if( MaterialOutputs[ i ]->Expression == Exp )
			{
				OutputConnections.Add( MaterialOutputs[ i ] );
			}
		}
	}
	else if ( MaterialFunction )
	{
		TempExpArray = GetFunctionExpressions( MaterialFunction );
		ExpressionsPointer = &TempExpArray;
	}
	for( int i = 0; i < ExpressionsPointer->Num(); i++ )
	{
		UMaterialExpression* MatExp = ( *ExpressionsPointer)[ i ];
		if( MatExp)
		{
			auto Inputs = MatExp->GetInputs();
			for( int u = 0; u < Inputs.Num(); u++ )
			{
				if( Inputs[ u ]->Expression == Exp )
				{
					Connections.Add( MatExp );
					break;
				}				
			}
		}
	}

	
}
struct ExpressionReplacementResult
{
	UMaterialExpression* Expression = nullptr;
	int OutputIndex = 0;
};
void DisconnectNodes( std::function<bool ( UMaterialExpression* )> IsNodeEligible,
	std::function<ExpressionReplacementResult( UMaterialExpression* )> ReplaceLambda, int OutputIndex = 0 )
{
	TArray<FString> Messages;
	auto MaterialLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			TArray<UMaterialExpression*> OutExpressions;
			Material->GetAllReferencedExpressions( OutExpressions, nullptr );
			for( int i = 0; i < OutExpressions.Num(); i++ )
			{
				UMaterialExpression* TargetExp = OutExpressions[ i ];
				if( IsNodeEligible( TargetExp ))
				{
					TArray<UMaterialExpression*> Connections;
					TArray<FExpressionInput*> OutputConnections;
					GetExpressionConnections( TargetExp, Material, nullptr, Connections, OutputConnections );
					for( int c = 0; c < Connections.Num(); c++ )
					{
						auto Inputs = Connections[ c ]->GetInputs();
						for( int u = 0; u < Inputs.Num(); u++ )
						{
							if( Inputs[ u ]->Expression == TargetExp && Inputs[ u ]->OutputIndex == OutputIndex )
							{
								ExpressionReplacementResult Result = ReplaceLambda( TargetExp );
								Inputs[ u ]->Expression = Result.Expression;
								Inputs[ u ]->OutputIndex = Result.OutputIndex;
								Modified = true;
							}
						}
					}

					TArray< FExpressionInput*> Inputs;
					GetOutputExpression( Material, TargetExp, 0, Inputs );
					for( int u = 0; u < Inputs.Num(); u++ )
					{
						if( Inputs[ u ]->Expression == TargetExp )
						{
							ExpressionReplacementResult Result = ReplaceLambda( TargetExp );
							Inputs[ u ]->Expression = Result.Expression;
							Inputs[ u ]->OutputIndex = Result.OutputIndex;
							Modified = true;
						}
					}
				}
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), MaterialLambda );

	auto MaterialFunctionLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>( Asset.GetAsset() );
		if( MaterialFunction )
		{
			TArray<UMaterialExpression*> TempExpArray = GetFunctionExpressions( MaterialFunction );
			const TArray<UMaterialExpression*>* OutExpressions = &TempExpArray;
			for( int i = 0; i < OutExpressions->Num(); i++ )
			{
				UMaterialExpression* TargetExp = ( *OutExpressions )[ i ];
				if( IsNodeEligible( TargetExp ) )
				{
					TArray<UMaterialExpression*> Connections;
					TArray<FExpressionInput*> OutputConnections;
					GetExpressionConnections( TargetExp, nullptr, MaterialFunction, Connections, OutputConnections );
					for( int c = 0; c < Connections.Num(); c++ )
					{
						auto Inputs = Connections[ c ]->GetInputs();
						for( int u = 0; u < Inputs.Num(); u++ )
						{
							if( Inputs[ u ]->Expression == TargetExp )
							{
								ExpressionReplacementResult Result = ReplaceLambda( TargetExp );
								Inputs[ u ]->Expression = Result.Expression;
								Inputs[ u ]->OutputIndex = Result.OutputIndex;
							}
						}
					}

					Modified = true;
				}
			}

			if( Modified )
			{
				MaterialFunction->PostEditChange();
				MaterialFunction->MarkPackageDirty();

				UMaterialEditingLibrary::UpdateMaterialFunction( MaterialFunction, nullptr );
			}			
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "MaterialFunction" ), MaterialFunctionLambda );
}
void FUnrealToUnityModule::RemoveVertexInterpolators()
{
	auto IsEligibleVertexInterpolator = [&]( UMaterialExpression* Exp )
	{
		UMaterialExpressionVertexInterpolator* E = Cast<UMaterialExpressionVertexInterpolator>( Exp );
		if( E )
			return true;
		else
			return false;
	};
	auto ReplaceVertexInterpolator = [&]( UMaterialExpression* TargetExp )
	{
		ExpressionReplacementResult Result;
		UMaterialExpressionVertexInterpolator* ExpressionVertexInterpolator = Cast<UMaterialExpressionVertexInterpolator>( TargetExp );
		Result.Expression = ExpressionVertexInterpolator->Input.Expression;
		return Result;
	};

	DisconnectNodes( IsEligibleVertexInterpolator, ReplaceVertexInterpolator );
}
void FUnrealToUnityModule::RemoveUnsupportedMaterialNodes()
{
	//No longer needed, it looks ok without it ( StonePineForest fails to recompile material anyway )
	//RemoveDitherTemporalAA();
	RemoveParallaxMapping();
	RemoveDepthFade();
}
void FUnrealToUnityModule::RemoveDitherTemporalAA()
{
	auto IsEligibleDitherTemporalAA = [&]( UMaterialExpression* Exp ) -> bool
	{
		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( Exp );
		if( ExpMFC && ExpMFC->MaterialFunction )
		{
			if ( ExpMFC->MaterialFunction->GetName().Compare( TEXT("DitherTemporalAA")) == 0 )
				return true;
		}
		return false;
	};
	auto ReplaceDitherTemporalAA = [&]( UMaterialExpression* TargetExp ) -> ExpressionReplacementResult
	{
		ExpressionReplacementResult Result;

		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( TargetExp );
		FExpressionInput* ExpInput = ExpMFC->GetInput( 0 );
		if( ExpInput )
		{
			Result.Expression = ExpInput->Expression;
			Result.OutputIndex = ExpInput->OutputIndex;
		}
		else
		{
			auto NewConstantExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( TargetExp->Material, TargetExp->Function, UMaterialExpressionConstant::StaticClass(), nullptr, TargetExp->MaterialExpressionEditorX, TargetExp->MaterialExpressionEditorY + 100 );
			UMaterialExpressionConstant* NewExpC = Cast < UMaterialExpressionConstant>( NewConstantExp );

			NewExpC->R = 0.5;
			Result.Expression = NewExpC;
		}
		return Result;
	};
	
	DisconnectNodes( IsEligibleDitherTemporalAA, ReplaceDitherTemporalAA );
}

void FUnrealToUnityModule::RemoveDepthFade()
{
	//Can't do it, linker error, wtf ?

	//auto IsEligibleLambda = [&]( UMaterialExpression* Exp ) -> bool
	//{
	//	UMaterialExpressionDepthFade* ExpCasted = Cast<UMaterialExpressionDepthFade>( Exp );
	//	if( ExpCasted )
	//	{
	//		return true;
	//	}
	//	return false;
	//};
	//auto ReplaceLambda = [&]( UMaterialExpression* TargetExp ) -> UMaterialExpression*
	//{
	//	auto NewConstantExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( TargetExp->Material, nullptr, UMaterialExpressionConstant::StaticClass(), nullptr, TargetExp->MaterialExpressionEditorX + 200, TargetExp->MaterialExpressionEditorY );
	//	UMaterialExpressionConstant* NewExpC = Cast < UMaterialExpressionConstant>( NewConstantExp );
	//
	//	NewExpC->R = 1;
	//	return NewExpC;
	//};
	//
	//DisconnectNodes( IsEligibleLambda, ReplaceLambda );
}
void FUnrealToUnityModule::RemoveParallaxMapping()
{
	auto IsEligibleNode = [&]( UMaterialExpression* Exp ) -> bool
	{
		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( Exp );
		if( ExpMFC && ExpMFC->MaterialFunction )
		{
			if( ExpMFC->MaterialFunction->GetName().Compare( TEXT( "ParallaxOcclusionMapping" ) ) == 0 )
				return true;
		}
		return false;
	};
	auto ReplaceNode = [&]( UMaterialExpression* TargetExp )
	{
		ExpressionReplacementResult Result;
		UMaterialExpressionMaterialFunctionCall* ExpMFC = Cast<UMaterialExpressionMaterialFunctionCall>( TargetExp );
		FExpressionInput* ExpInput = ExpMFC->GetInput( 4 );
		if( ExpInput )
			Result.Expression = ExpInput->Expression;
		return Result;
	};

	DisconnectNodes( IsEligibleNode, ReplaceNode );
}
void FUnrealToUnityModule::FixVolumeMaterials()
{
	TArray<FString> Messages;
	auto MaterialLambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			if( Material->MaterialDomain == EMaterialDomain::MD_Volume )
			{
				if( Material->SubsurfaceColor.Expression )
				{
					Material->Opacity.Expression = Material->SubsurfaceColor.Expression;
					Material->SubsurfaceColor.Expression = nullptr;
				}
				Material->MaterialDomain = EMaterialDomain::MD_Surface;
				Modified = true;
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}			
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), MaterialLambda );
}

const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& GetFoliageInfos( AInstancedFoliageActor* IFA )
{
	#if ENGINE_MAJOR_VERSION == 4
		auto& Infos = IFA->FoliageInfos;
	#else
		auto& Infos = IFA->GetFoliageInfos();
	#endif

	return Infos;
}
int64 GetTotalFoliageInstances()
{
	int64 Total = 0;
	
	UWorld* World = GEditor->GetEditorWorldContext().World();

	for( TActorIterator<AActor> ActorIterator( World, AActor::StaticClass(), EActorIteratorFlags::OnlyActiveLevels ); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>( Actor );

		if( IFA )
		{
			auto& Infos = GetFoliageInfos( IFA );
			for( auto& Pair : Infos )
			{
				const UFoliageType* FoliageType = Pair.Key;
				const UFoliageType_InstancedStaticMesh* FT_StaticMesh = Cast<UFoliageType_InstancedStaticMesh>( FoliageType );
				if( !FT_StaticMesh )
				{
					continue;
				}
				UStaticMesh* SM = FT_StaticMesh->GetStaticMesh();
				if( !SM )
				{
					continue;
				}

				const FFoliageInfo& Info = *Pair.Value;
				
				Total += Info.Instances.Num();
			}
		}
	}

	return Total;
}
FVector RemoveNans( FVector Value );
bool HasNans( FVector Value );
void FUnrealToUnityModule::MoveFoliageToActors()
{
	int64 Total = GetTotalFoliageInstances();
	UE_LOG( LogTemp, Warning, TEXT( "GetTotalFoliageInstances = %lld" ), Total );
	int Threshold = CVarMaxFoliageActors.GetValueOnAnyThread();
	if( Total > Threshold )
	{
		FString Text = FString::Printf( TEXT( "Foliage contains %d Instances. The conversion to static actors may take from 1 minute to hours.\n"
											  "Do you still want to export all foliage?" ), Total );
		FText Txt = FText::FromString( Text );

		EAppReturnType::Type Result = FMessageDialog::Open( EAppMsgType::YesNo, Txt );
		if( Result == EAppReturnType::Type::No )
			return;
	}
	TArray<FString> Messages;

	int UniqueIndex = 0;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	for( TActorIterator<AActor> ActorIterator( World, AActor::StaticClass(), EActorIteratorFlags::OnlyActiveLevels ); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>( Actor );

		if( IFA )
		{
			auto& Infos = GetFoliageInfos( IFA );
			for( auto& Pair : Infos )
			{
				const UFoliageType* FoliageType = Pair.Key;
				const UFoliageType_InstancedStaticMesh* FT_StaticMesh = Cast<UFoliageType_InstancedStaticMesh>( FoliageType );
				if( !FT_StaticMesh )
				{
					continue;
				}
				UStaticMesh* SM = FT_StaticMesh->GetStaticMesh();
				if( !SM )
				{
					continue;
				}

				const FFoliageInfo& Info = *Pair.Value;
				int Duplicates = 0;
				TArray<int32> InstancesIndices;
				for( int i = 0; i < Info.Instances.Num(); i++ )
				{
					const FFoliageInstance& Instance = Info.Instances[ i ];

					FTransform Transform = Instance.GetInstanceWorldTransform();
					FVector Location = Transform.GetLocation();
					if( HasNans( Location ) )
						continue;

					//Location = RemoveNans( Location );
					Transform.SetTranslation( Location );

					AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( SM );

					UniqueIndex++;
					FString NewName = FString::Printf( TEXT( "%s_FromFoliage_%d" ), *SM->GetName(), UniqueIndex );
					NewSMActor->Rename( *NewName );
					NewSMActor->SetActorLabel( NewName );
					InstancesIndices.Add( i );
				}

				#if ENGINE_MAJOR_VERSION == 4
					( (FFoliageInfo&)Info ).RemoveInstances( IFA, InstancesIndices, true );
				#else
					((FFoliageInfo&)Info).RemoveInstances( InstancesIndices, true );
				#endif
			}
		}
	}
}
void FUnrealToUnityModule::ConvertISMToActors()
{
	TArray<FString> Messages;

	int UniqueIndex = 0;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	for( TActorIterator<AActor> ActorIterator( World); ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>( Actor );
		if( IFA )
			continue;
		if( Actor->IsHiddenEd() )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );
		
		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>( AC );//UHierarchicalInstancedStaticMeshComponent
			if( ISM )
			{
				for( int u = 0; u < ISM->GetInstanceCount(); u++ )
				{
					FTransform Transform;
					ISM->GetInstanceTransform( u, Transform, true );

					FActorSpawnParameters Params;
					FString NewActorName = FString::Printf( TEXT( "%s_ISM%d_Instance%d" ), *Actor->GetName(), i, u );
					Params.Name = FName(NewActorName);
					AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform, Params );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( ISM->GetStaticMesh() );
				}
				for( int u = 0; u < ISM->GetInstanceCount(); u++ )
				{
					ISM->RemoveInstance( u );
					u--;
				}
				//Actor->RemoveOwnedComponent( ISM );
			}
		}
	}
}
void FUnrealToUnityModule::PlaceAllAssetsInScene()
{
	TArray<UStaticMesh*> StaticMeshes;
	TArray<FString> Messages;
	float MaxSphere = 0.0f;
	UWorld* World = GEditor->GetEditorWorldContext().World();

	auto MaterialLambda = [&]( FAssetData& Asset )
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>( Asset.GetAsset() );
		if( StaticMesh )
		{
			FBoxSphereBounds Bounds = StaticMesh->GetBounds();
			MaxSphere = FMath::Max( MaxSphere, Bounds.SphereRadius );
			StaticMeshes.Add( StaticMesh );
		}
	};

	//IterateOverAllAssetsOfType( FName( "StaticMesh" ), MaterialLambda );
	IterateOverSelection( MaterialLambda );

	int GridSize = (int)FMath::Sqrt( StaticMeshes.Num() ) + 1;
	int i = 0;
	for( int y = 0; y < GridSize; y++ )
	{
		for( int x = 0; x < GridSize; x++ )
		{
			if( i < StaticMeshes.Num() )
			{
				FVector Position( x * MaxSphere, y * MaxSphere, 0 );

				FTransform Transform;
				Transform.SetLocation( Position );

				AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform );
				AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
				NewSMActor->GetStaticMeshComponent()->SetStaticMesh( StaticMeshes[i] );
				i++;
			}
		}
	}
}
void FUnrealToUnityModule::FixLandscapeMaterials()
{
	TArray<FString> Messages;
	auto lambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			TArray<UMaterialExpression*> AllExpressions;
			Material->GetAllReferencedExpressions( AllExpressions, nullptr );
			for( int e = 0; e < AllExpressions.Num(); e++ )
			{
				UMaterialExpression* BaseExp = AllExpressions[ e ];
				UMaterialExpressionLandscapeLayerBlend* LayerBlendExp = Cast< UMaterialExpressionLandscapeLayerBlend>( BaseExp );
				if( LayerBlendExp )
				{
					bool HasMaterialAttributes = false;
					for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
					{
						auto Layer = LayerBlendExp->Layers[ i ];
						if( Layer.LayerInput.Expression &&
							Layer.LayerInput.Expression->IsResultMaterialAttributes( Layer.LayerInput.OutputIndex ) )
						{
							HasMaterialAttributes = true;
						}
					}
					if( HasMaterialAttributes )
					{
						TArray< UMaterialExpressionBreakMaterialAttributes*> BreakNodes;
						for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
						{
							auto Layer = LayerBlendExp->Layers[ i ];
							auto SourceExp = Layer.LayerInput.Expression;
							if( SourceExp )
							{
								FVector2D Location( SourceExp->MaterialExpressionEditorX + 200, SourceExp->MaterialExpressionEditorY );
								auto NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionBreakMaterialAttributes::StaticClass(), nullptr, Location.X, Location.Y );
								UMaterialExpressionBreakMaterialAttributes* NewExpC = Cast < UMaterialExpressionBreakMaterialAttributes>( NewExpression );
								*NewExpC->GetInput( 0 ) = Layer.LayerInput;
								BreakNodes.Add( NewExpC );
							}
						}

						int InputIndices[ 3 ] = { 0, 3, 8 };
						TArray< UMaterialExpressionLandscapeLayerBlend*> NewLayerBlendNodes;
						for( int u = 0; u < 3; u++ )
						{
							FVector2D Location( LayerBlendExp->MaterialExpressionEditorX - 200, LayerBlendExp->MaterialExpressionEditorY + InputIndices[ u ] * 50 );
							auto NewLayerBlendExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionLandscapeLayerBlend::StaticClass(), nullptr, Location.X, Location.Y );
							UMaterialExpressionLandscapeLayerBlend* NewLayerBlendExpC = Cast < UMaterialExpressionLandscapeLayerBlend>( NewLayerBlendExp );

							for( int i = 0; i < LayerBlendExp->Layers.Num(); i++ )
							{
								NewLayerBlendExpC->Layers.Add( LayerBlendExp->Layers[ i ] );
								if ( BreakNodes.Num() > i )
									NewLayerBlendExpC->Layers[ i ].LayerInput.Expression = BreakNodes[ i ];
								NewLayerBlendExpC->Layers[ i ].LayerInput.OutputIndex = InputIndices[ u ];
							}

							NewLayerBlendNodes.Add( NewLayerBlendExpC );
						}

						FVector2D Location( LayerBlendExp->MaterialExpressionEditorX + 200, LayerBlendExp->MaterialExpressionEditorY );
						
						auto NewMakeAttribsExp = UMaterialEditingLibrary::CreateMaterialExpressionEx( Material, nullptr, UMaterialExpressionMakeMaterialAttributes::StaticClass(), nullptr, Location.X, Location.Y );
						UMaterialExpressionMakeMaterialAttributes* NewMakeAttribsExpC = Cast < UMaterialExpressionMakeMaterialAttributes>( NewMakeAttribsExp );

						for( int u = 0; u < 3; u++ )
						{
							NewMakeAttribsExp->GetInput( InputIndices[ u ] )->Expression = NewLayerBlendNodes[u];
						}

						TArray<FExpressionInput*> ConnectedExpressions;
						GetOutputExpression( Material, LayerBlendExp, 0, ConnectedExpressions );
						for( int i = 0; i < ConnectedExpressions.Num(); i++ )
						{
							ConnectedExpressions[ i ]->Expression = NewMakeAttribsExpC;
							ConnectedExpressions[ i ]->OutputIndex = 0;
						}
					}
				}
			}

			if( Modified )
			{
				Material->PostEditChange();
				Material->MarkPackageDirty();

				UMaterialEditingLibrary::RecompileMaterial( Material );
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), lambda );
	//IterateOverSelection( lambda );

}
void FUnrealToUnityModule::ConvertLandscapesToStaticMeshes()
{
	FixLandscapeMaterials();

	UWorld* World = GEditor->GetEditorWorldContext().World();
	
	LandscapeTools.CreateAssets( World );
}
void FUnrealToUnityModule::DetachMultipleComponentActors()
{
	//Append a unique ID in case there's 2 actors with the same name
	int UniqueID = 0;
	int UniqueDecalID = 0;
	UWorld* world = GEditor->GetEditorWorldContext().World();
	for( TActorIterator<AActor> iterator( world ); iterator; ++iterator )
	{
		AActor* Actor = *iterator;
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>( Actor );
		if( Actor->IsHiddenEd() || IFA )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );
		int DetachableComponents = 0;
		int LocalDecalID = 0;
		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( AC );
			USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( AC );
			UDecalComponent* DecalComp = Cast<UDecalComponent>( AC );
			if( (StaticMeshComp && !SplineMeshComponent) || DecalComp )
			{
				DetachableComponents++;
			}
		}
		if( DetachableComponents > 1 )
		{			
			for( int i = 0; i < comps.Num(); i++ )
			{
				UActorComponent* AC = comps[ i ];
				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( AC );
				USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( AC );
				UDecalComponent* DecalComp = Cast<UDecalComponent>( AC );
				if( StaticMeshComp && StaticMeshComp->GetStaticMesh() && !SplineMeshComponent && StaticMeshComp->IsVisible())
				{
					FTransform Transform = StaticMeshComp->GetComponentToWorld();
					FActorSpawnParameters Params;
					FString NewActorName = FString::Printf( TEXT( "%s_%s_%d_%d" ), *Actor->GetName(), *StaticMeshComp->GetStaticMesh()->GetName(), i, UniqueID++ );
					Params.Name = FName( NewActorName );
					AActor* NewActor = world->SpawnActor( AStaticMeshActor::StaticClass(), &Transform, Params );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->SetActorLabel( NewActorName );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( StaticMeshComp->GetStaticMesh() );

					StaticMeshComp->DestroyComponent();
				}
				if( DecalComp && DecalComp->IsVisible() )
				{
					FDetachmentTransformRules DetachRules( EDetachmentRule::KeepWorld, true );
					FAttachmentTransformRules AttachRules( EAttachmentRule::KeepWorld, false );

					//FTransform TransformBefore = DecalComp->GetComponentToWorld();
					//DecalComp->DetachFromComponent( DetachRules );
					FTransform Transform = DecalComp->GetComponentTransform();// *Actor->GetTransform();
					FActorSpawnParameters Params;
					FString NewActorName = FString::Printf( TEXT( "%s_Decal_%d_%d" ), *Actor->GetName(), LocalDecalID++, UniqueDecalID++ );
					Params.Name = FName( NewActorName );
					//Params.Template = DecalComp->GetOwner();
					
					AActor* NewActor = world->SpawnActor( ADecalActor::StaticClass(), &FTransform::Identity, Params);
					ADecalActor* NewDecalActor = Cast<ADecalActor>( NewActor );
					NewActor->SetActorLabel( NewActorName );

					NewDecalActor->GetDecal()->SetRelativeTransform( Transform );
					NewDecalActor->GetDecal()->UpdateComponentToWorld();

					NewDecalActor->GetDecal()->DecalSize = DecalComp->DecalSize;
					NewDecalActor->GetDecal()->SetDecalMaterial( DecalComp->GetDecalMaterial() );
					//DecalComp->UnregisterComponent();
					//AActor* OldOwner = DecalComp->GetOwner();
					//DecalComp->Rename( nullptr, NewActor );
					//AActor *NewOwner = DecalComp->GetOwner();
					
					//DecalComp->AttachToComponent( NewActor->GetDefaultAttachComponent(), AttachRules );

					DecalComp->DestroyComponent();
				}
			}
			//Actor->Destroy();
		}
	}
}
void FUnrealToUnityModule::ConvertSplineMeshesToStaticActors()
{
	UWorld* world = GEditor->GetEditorWorldContext().World();
	for( TActorIterator<AActor> iterator( world ); iterator; ++iterator )
	{
		AActor* Actor = *iterator;
		if( Actor->IsHiddenEd() )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );
		int SplineComponents = 0;
		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			USplineMeshComponent* SplineMeshComponent = dynamic_cast<USplineMeshComponent*>( AC );
			if( SplineMeshComponent )
			{
				SplineComponents++;
			}
		}
		if( SplineComponents > 0 )
		{
			ConvertSplineMeshesToStaticActor( Actor );
		}
	}
}
void ExportStaticMeshLOD( const FStaticMeshLODResources& StaticMeshLOD, FMeshDescription& OutRawMesh, const TArray<FStaticMaterial>& Materials )
{
	const int32 NumWedges = StaticMeshLOD.IndexBuffer.GetNumIndices();
	const int32 NumVertexPositions = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	const int32 NumFaces = NumWedges / 3;

	OutRawMesh.Empty();

	if( NumVertexPositions <= 0 || StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() <= 0 )
	{
		return;
	}

	TEdgeAttributesRef<bool> EdgeHardnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );
	TEdgeAttributesRef<float> EdgeCreaseSharpnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<float>( MeshAttribute::Edge::CreaseSharpness );
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = OutRawMesh.PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
	#if ENGINE_MAJOR_VERSION == 4
	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );
	#else
	FStaticMeshAttributes Attributes( OutRawMesh );
	Attributes.Register();
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	#endif
	

	OutRawMesh.ReserveNewVertices( NumVertexPositions );
	OutRawMesh.ReserveNewVertexInstances( NumWedges );
	OutRawMesh.ReserveNewPolygons( NumFaces );
	OutRawMesh.ReserveNewEdges( NumWedges );

	const int32 NumTexCoords = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	VertexInstanceUVs.SetNumIndices( NumTexCoords );

	for( int32 SectionIndex = 0; SectionIndex < StaticMeshLOD.Sections.Num(); ++SectionIndex )
	{
		const FStaticMeshSection& Section = StaticMeshLOD.Sections[ SectionIndex ];
		FPolygonGroupID CurrentPolygonGroupID = OutRawMesh.CreatePolygonGroup();
		check( CurrentPolygonGroupID.GetValue() == SectionIndex );
		if( Materials.IsValidIndex( Section.MaterialIndex ) )
		{
			PolygonGroupImportedMaterialSlotNames[ CurrentPolygonGroupID ] = Materials[ Section.MaterialIndex ].ImportedMaterialSlotName;
		}
		else
		{
			PolygonGroupImportedMaterialSlotNames[ CurrentPolygonGroupID ] = FName( *( TEXT( "MeshMergeMaterial_" ) + FString::FromInt( SectionIndex ) ) );
		}
	}

	//Create the vertex
	for( int32 VertexIndex = 0; VertexIndex < NumVertexPositions; ++VertexIndex )
	{
		FVertexID VertexID = OutRawMesh.CreateVertex();
		VertexPositions[ VertexID ] = StaticMeshLOD.VertexBuffers.PositionVertexBuffer.VertexPosition( VertexIndex );
	}

	//Create the vertex instances
	for( int32 TriangleIndex = 0; TriangleIndex < NumFaces; ++TriangleIndex )
	{
		FPolygonGroupID CurrentPolygonGroupID = FPolygonGroupID::Invalid;
		for( int32 SectionIndex = 0; SectionIndex < StaticMeshLOD.Sections.Num(); ++SectionIndex )
		{
			const FStaticMeshSection& Section = StaticMeshLOD.Sections[ SectionIndex ];
			uint32 BeginTriangle = Section.FirstIndex / 3;
			uint32 EndTriangle = BeginTriangle + Section.NumTriangles;
			if( (uint32)TriangleIndex >= BeginTriangle && (uint32)TriangleIndex < EndTriangle )
			{
				CurrentPolygonGroupID = FPolygonGroupID( SectionIndex );
				break;
			}
		}
		check( CurrentPolygonGroupID != FPolygonGroupID::Invalid );

		FVertexID VertexIDs[ 3 ];
		TArray<FVertexInstanceID> VertexInstanceIDs;
		VertexInstanceIDs.SetNum( 3 );

		for( int32 Corner = 0; Corner < 3; ++Corner )
		{
			int32 WedgeIndex = StaticMeshLOD.IndexBuffer.GetIndex( TriangleIndex * 3 + Corner );
			FVertexID VertexID( WedgeIndex );
			FVertexInstanceID VertexInstanceID = OutRawMesh.CreateVertexInstance( VertexID );
			VertexIDs[ Corner ] = VertexID;
			VertexInstanceIDs[ Corner ] = VertexInstanceID;

			//NTBs
			#if ENGINE_MAJOR_VERSION == 4
				FVector TangentX = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX( WedgeIndex );
				FVector TangentY = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY( WedgeIndex );
				FVector TangentZ = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( WedgeIndex );
				VertexInstanceBinormalSigns[ VertexInstanceID ] = GetBasisDeterminantSign( TangentX, TangentY, TangentZ );
			#else
				FVector4f TangentX = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX( WedgeIndex );
				FVector4f TangentY = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY( WedgeIndex );
				FVector4f TangentZ = StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( WedgeIndex );
				VertexInstanceBinormalSigns[ VertexInstanceID ] = GetBasisDeterminantSign( FVector( TangentX.X, TangentX.Y, TangentX.Z ),
																						   FVector( TangentY.X, TangentY.Y, TangentY.Z ),
																						   FVector( TangentZ.X, TangentZ.Y, TangentZ.Z ) );
			#endif
			VertexInstanceTangents[ VertexInstanceID ] = TangentX;
			
			VertexInstanceNormals[ VertexInstanceID ] = TangentZ;

			// Vertex colors
			if( StaticMeshLOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0 )
			{
				VertexInstanceColors[ VertexInstanceID ] = FLinearColor( StaticMeshLOD.VertexBuffers.ColorVertexBuffer.VertexColor( WedgeIndex ) );
			}
			else
			{
				VertexInstanceColors[ VertexInstanceID ] = FLinearColor::White;
			}

			//Tex coord
			for( int32 TexCoodIdx = 0; TexCoodIdx < NumTexCoords; ++TexCoodIdx )
			{
				VertexInstanceUVs.Set( VertexInstanceID, TexCoodIdx, StaticMeshLOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( WedgeIndex, TexCoodIdx ) );
			}
		}
		//Create a polygon from this triangle
		const FPolygonID NewPolygonID = OutRawMesh.CreatePolygon( CurrentPolygonGroupID, VertexInstanceIDs );
	}
}
#if ENGINE_MAJOR_VERSION == 5
FVector3f ToFVector3f( FVector3d VD )
{
	return FVector3f( VD.X, VD.Y, VD.Z );
}
FVector3d ToFVector3d( FVector3f V )
{
	return FVector3d( V.X, V.Y, V.Z );
}
#else
FVector ToFVector3f( FVector VD )
{
	return FVector( VD.X, VD.Y, VD.Z );
}
FVector ToFVector3d( FVector V )
{
	return FVector( V.X, V.Y, V.Z );
}
#endif

void FMeshMergeHelpers_PropagateSplineDeformationToRawMesh( const USplineMeshComponent* InSplineMeshComponent, FMeshDescription& OutRawMesh )
{
#if ENGINE_MAJOR_VERSION == 4
	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );
#else
	FStaticMeshAttributes Attributes( OutRawMesh );
	
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
#endif

	// Apply spline deformation for each vertex's tangents
	int32 WedgeIndex = 0;
	for( const FPolygonID PolygonID : OutRawMesh.Polygons().GetElementIDs() )
	{
		for( const FTriangleID& TriangleID : OutRawMesh.GetPolygonTriangleIDs( PolygonID ) )
		{
			for( int32 Corner = 0; Corner < 3; ++Corner, ++WedgeIndex )
			{
				const FVertexInstanceID VertexInstanceID = OutRawMesh.GetTriangleVertexInstance( TriangleID, Corner );
				const FVertexID VertexID = OutRawMesh.GetVertexInstanceVertex( VertexInstanceID );
				const float& AxisValue = USplineMeshComponent::GetAxisValue( VertexPositions[ VertexID ], InSplineMeshComponent->ForwardAxis );
				FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform( AxisValue );
				FVector TangentY = FVector::CrossProduct( (FVector)VertexInstanceNormals[ VertexInstanceID ], (FVector)VertexInstanceTangents[ VertexInstanceID ] ).GetSafeNormal() * VertexInstanceBinormalSigns[ VertexInstanceID ];
				VertexInstanceTangents[ VertexInstanceID ] = ToFVector3f( SliceTransform.TransformVector( ToFVector3d(VertexInstanceTangents[ VertexInstanceID ] ) ));
				TangentY = SliceTransform.TransformVector( TangentY );
				VertexInstanceNormals[ VertexInstanceID ] = ToFVector3f( SliceTransform.TransformVector( ToFVector3d(VertexInstanceNormals[ VertexInstanceID ] ) ));
				VertexInstanceBinormalSigns[ VertexInstanceID ] = GetBasisDeterminantSign( (FVector)VertexInstanceTangents[ VertexInstanceID ], TangentY, (FVector)VertexInstanceNormals[ VertexInstanceID ] );
			}
		}
	}

	// Apply spline deformation for each vertex position
	for( const FVertexID VertexID : OutRawMesh.Vertices().GetElementIDs() )
	{
		auto AxisValue = USplineMeshComponent::GetAxisValue( VertexPositions[ VertexID ], InSplineMeshComponent->ForwardAxis );		
		
		FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform( AxisValue );
		
		AxisValue = 0.0f;
		VertexPositions[ VertexID ] = ToFVector3f( SliceTransform.TransformPosition( ToFVector3d( VertexPositions[ VertexID ] ) ) );
	}
}
const TArray<FStaticMaterial>& GetStaticMaterials( const UStaticMesh* StaticMesh )
{
	#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION == 5
		const TArray<FStaticMaterial>& Materials = StaticMesh->GetStaticMaterials();
	#else
		const TArray<FStaticMaterial>& Materials = StaticMesh->StaticMaterials;
	#endif

	return Materials;
}
UStaticMesh* CreateStaticMeshFromMeshDescription( FString UserPackageName, FMeshDescription* MeshDescription, UStaticMesh* OriginalMesh )
{
	FString NewNameSuggestion = FString( TEXT( "ProcMesh" ) );
	FString PackageName = FString( TEXT( "/Game/Meshes/" ) ) + NewNameSuggestion;
	FString Name;

	//FAssetToolsModule* 
		AssetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
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
			SrcModel.BuildSettings.bRecomputeNormals = false;
			SrcModel.BuildSettings.bRecomputeTangents = false;
			SrcModel.BuildSettings.bRemoveDegenerates = false;
			SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
			SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
			SrcModel.BuildSettings.bGenerateLightmapUVs = true;
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

			const TArray<FStaticMeshSourceModel>& SourceModels = OriginalMesh->GetSourceModels();
			for( int i = 1; i < SourceModels.Num(); i++ )
			{
				FStaticMeshSourceModel& SrcModelLOD = StaticMesh->AddSourceModel();
				SrcModelLOD.BuildSettings = OriginalMesh->GetSourceModel( i ).BuildSettings;
				SrcModelLOD.ReductionSettings = OriginalMesh->GetSourceModel( i ).ReductionSettings;
			}

			
			TArray<FStaticMaterial> LocalMaterials = GetStaticMaterials( OriginalMesh );

			#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION == 5
				StaticMesh->SetStaticMaterials( LocalMaterials );
			#else
				StaticMesh->StaticMaterials = LocalMaterials;
			#endif

			StaticMesh->GetSectionInfoMap().CopyFrom( OriginalMesh->GetSectionInfoMap() );

			//Set the Imported version before calling the build
			StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

			// Build mesh from source
			StaticMesh->Build( false );
		#if ENGINE_MINOR_VERSION >= 27 || ENGINE_MAJOR_VERSION == 5
			UBodySetup* BodySetup = StaticMesh->GetBodySetup();
		#else
			UBodySetup* BodySetup = StaticMesh->BodySetup;
		#endif
			if( BodySetup )
				BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
			StaticMesh->PostEditChange();

			Package->Modify();			

			return StaticMesh;
		}
	}

	return nullptr;
}
bool FMeshMergeHelpers_PropagatePaintedColorsToRawMesh( const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FMeshDescription& RawMesh )
{
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

	if( StaticMesh->IsSourceModelValid( LODIndex ) &&
		StaticMeshComponent->LODData.IsValidIndex( LODIndex ) &&
		StaticMeshComponent->LODData[ LODIndex ].OverrideVertexColors != nullptr )
	{
		FColorVertexBuffer& ColorVertexBuffer = *StaticMeshComponent->LODData[ LODIndex ].OverrideVertexColors;
		const FStaticMeshLODResources& RenderModel = GetRenderData( StaticMesh )->LODResources[ LODIndex ];

		if( ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices() )
		{
			const int32 NumWedges = RawMesh.VertexInstances().Num();
			const int32 NumRenderWedges = RenderModel.IndexBuffer.GetNumIndices();
			const bool bUseRenderWedges = NumWedges == NumRenderWedges;

		#if ENGINE_MAJOR_VERSION == 4
			TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = RawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );
		#else
			TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = FStaticMeshAttributes( RawMesh ).GetVertexInstanceColors();
		#endif

			if( bUseRenderWedges )
			{
				//Create a map index
				TMap<int32, FVertexInstanceID> IndexToVertexInstanceID;
				IndexToVertexInstanceID.Reserve( NumWedges );
				int32 CurrentWedgeIndex = 0;
				for( const FPolygonID PolygonID : RawMesh.Polygons().GetElementIDs() )
				{
					const TArray<FTriangleID>& TriangleIDs = RawMesh.GetPolygonTriangleIDs( PolygonID );
					for( const FTriangleID& TriangleID : TriangleIDs )
					{
						for( int32 Corner = 0; Corner < 3; ++Corner, ++CurrentWedgeIndex )
						{
							IndexToVertexInstanceID.Add( CurrentWedgeIndex, RawMesh.GetTriangleVertexInstance( TriangleID, Corner ) );
						}
					}
				}

				const FIndexArrayView ArrayView = RenderModel.IndexBuffer.GetArrayView();
				for( int32 WedgeIndex = 0; WedgeIndex < NumRenderWedges; WedgeIndex++ )
				{
					const int32 Index = ArrayView[ WedgeIndex ];
					FColor WedgeColor = FColor::White;
					if( Index != INDEX_NONE )
					{
						WedgeColor = ColorVertexBuffer.VertexColor( Index );
					}
					VertexInstanceColors[ IndexToVertexInstanceID[ WedgeIndex ] ] = FLinearColor( WedgeColor );
				}

				return true;
			}
			// No wedge map (this can happen when we poly reduce the LOD for example)
			// Use index buffer directly. Not sure this will happen with FMeshDescription
			else
			{
				if( RawMesh.Vertices().Num() == ColorVertexBuffer.GetNumVertices() )
				{
					//Create a map index
					TMap<FVertexID, int32> VertexIDToVertexIndex;
					VertexIDToVertexIndex.Reserve( RawMesh.Vertices().Num() );
					int32 CurrentVertexIndex = 0;
					for( const FVertexID VertexID : RawMesh.Vertices().GetElementIDs() )
					{
						VertexIDToVertexIndex.Add( VertexID, CurrentVertexIndex++ );
					}

					for( const FVertexID VertexID : RawMesh.Vertices().GetElementIDs() )
					{
						FColor WedgeColor = FColor::White;
						uint32 VertIndex = VertexIDToVertexIndex[ VertexID ];

						if( VertIndex < ColorVertexBuffer.GetNumVertices() )
						{
							WedgeColor = ColorVertexBuffer.VertexColor( VertIndex );
						}
						const TArray<FVertexInstanceID>& VertexInstances = RawMesh.GetVertexVertexInstances( VertexID );
						for( const FVertexInstanceID& VertexInstanceID : VertexInstances )
						{
							VertexInstanceColors[ VertexInstanceID ] = FLinearColor( WedgeColor );
						}
					}
					return true;
				}
			}
		}
	}

	return false;
}
const FStaticMeshRenderData* GetRenderData( const UStaticMesh* StaticMesh );
void FMeshMergeHelpers_TransformRawMeshVertexData( const FTransform& InTransform, FMeshDescription& OutRawMesh )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FMeshMergeHelpers::TransformRawMeshVertexData )

	
	TEdgeAttributesRef<bool> EdgeHardnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );
	TEdgeAttributesRef<float> EdgeCreaseSharpnesses = OutRawMesh.EdgeAttributes().GetAttributesRef<float>( MeshAttribute::Edge::CreaseSharpness );
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = OutRawMesh.PolygonGroupAttributes().GetAttributesRef<FName>( MeshAttribute::PolygonGroup::ImportedMaterialSlotName );
#if ENGINE_MAJOR_VERSION == 4
	TVertexAttributesRef<FVector> VertexPositions = OutRawMesh.VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = OutRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );
#else
	FStaticMeshAttributes Attributes( OutRawMesh );
	Attributes.Register();
	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
#endif

	for( const FVertexID VertexID : OutRawMesh.Vertices().GetElementIDs() )
	{
		VertexPositions[ VertexID ] = ToFVector3f( InTransform.TransformPosition( ToFVector3d( VertexPositions[ VertexID ] ) ));
	}

	FMatrix Matrix = InTransform.ToMatrixWithScale();
	FMatrix AdjointT = Matrix.TransposeAdjoint();
	AdjointT.RemoveScaling();

	const float MulBy = Matrix.Determinant() < 0.f ? -1.f : 1.f;
	auto TransformNormal =
		[&AdjointT, MulBy]( FVector& Normal )
	{
		Normal = AdjointT.TransformVector( Normal ) * MulBy;
	};

	for( const FVertexInstanceID VertexInstanceID : OutRawMesh.VertexInstances().GetElementIDs() )
	{
	#if ENGINE_MAJOR_VERSION == 4
		FVector TangentY = FVector::CrossProduct( VertexInstanceNormals[ VertexInstanceID ], VertexInstanceTangents[ VertexInstanceID ] ).GetSafeNormal() * VertexInstanceBinormalSigns[ VertexInstanceID ];
		TransformNormal( VertexInstanceTangents[ VertexInstanceID ] );
		TransformNormal( TangentY );
		TransformNormal( VertexInstanceNormals[ VertexInstanceID ] );
		VertexInstanceBinormalSigns[ VertexInstanceID ] = GetBasisDeterminantSign( VertexInstanceTangents[ VertexInstanceID ], TangentY, VertexInstanceNormals[ VertexInstanceID ] );
	#else
		FVector3f Tangent = VertexInstanceTangents[ VertexInstanceID ];
		FVector3f Normal = VertexInstanceNormals[ VertexInstanceID ];
		float BinormalSign = VertexInstanceBinormalSigns[ VertexInstanceID ];

		VertexInstanceTangents[ VertexInstanceID ] = (FVector3f)FVector( AdjointT.TransformVector( (FVector)Tangent ) * MulBy );
		VertexInstanceBinormalSigns[ VertexInstanceID ] = BinormalSign * MulBy;
		VertexInstanceNormals[ VertexInstanceID ] = (FVector3f)FVector( AdjointT.TransformVector( (FVector)Normal ) * MulBy );
	#endif
	}

	const bool bIsMirrored = InTransform.GetDeterminant() < 0.f;
	if( bIsMirrored )
	{
		//Reverse the vertex instance
		OutRawMesh.ReverseAllPolygonFacing();
	}
}
FMeshDescription* FMeshMergeHelpers_RetrieveMesh( FMeshDescription* RawMesh, const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, bool bPropagateVertexColours )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FMeshMergeHelpers::RetrieveMesh )

	const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if( !StaticMesh )
		return nullptr;
	const FStaticMeshSourceModel& StaticMeshModel = StaticMesh->GetSourceModel( LODIndex );

	const bool bIsSplineMeshComponent = StaticMeshComponent->IsA<USplineMeshComponent>();

	// Imported meshes will have a valid mesh description
	const bool bImportedMesh = StaticMesh->IsMeshDescriptionValid( LODIndex );

	// Export the raw mesh data using static mesh render data
	ExportStaticMeshLOD( GetRenderData(StaticMesh)->LODResources[ LODIndex ], *RawMesh, GetStaticMaterials( StaticMesh ) );

	// Make sure the raw mesh is not irreparably malformed.
	if( RawMesh->VertexInstances().Num() <= 0 )
	{
		return nullptr;
	}

	// Use build settings from base mesh for LOD entries that was generated inside Editor.
	const FMeshBuildSettings& BuildSettings = bImportedMesh ? StaticMeshModel.BuildSettings : StaticMesh->GetSourceModel( 0 ).BuildSettings;

	// Transform raw mesh to world space
	FTransform ComponentToWorldTransform = StaticMeshComponent->GetComponentTransform();

	// Handle spline mesh deformation
	if( bIsSplineMeshComponent )
	{
		const USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>( StaticMeshComponent );
		// Deform raw mesh data according to the Spline Mesh Component's data
		FMeshMergeHelpers_PropagateSplineDeformationToRawMesh( SplineMeshComponent, *RawMesh );
	}

	if( bPropagateVertexColours )
	{
		FMeshMergeHelpers_PropagatePaintedColorsToRawMesh( StaticMeshComponent, LODIndex, *RawMesh );
	}

	// Transform raw mesh vertex data by the Static Mesh Component's component to world transformation	
	FMeshMergeHelpers_TransformRawMeshVertexData( ComponentToWorldTransform, *RawMesh );

	return RawMesh;	
}
void FUnrealToUnityModule::ConvertSplineMeshesToStaticActor( AActor* Actor )
{
	TArray<USplineMeshComponent*> ComponentsToMerge;
	TArray<UActorComponent*> comps;
	Actor->GetComponents( comps );
	int SplineComponents = 0;
	for( int i = 0; i < comps.Num(); i++ )
	{
		UActorComponent* AC = comps[ i ];
		USplineMeshComponent* SplineMeshComponent = dynamic_cast<USplineMeshComponent*>( AC );
		if( SplineMeshComponent )
		{
			ComponentsToMerge.Add( SplineMeshComponent );
		}
	}

	if( ComponentsToMerge.Num() > 0 && ComponentsToMerge[ 0 ]->GetStaticMesh() )
	{
		TArray< FMeshDescription*> RawMeshArray;
		FMeshDescription* MergedRawMesh = new FMeshDescription();
		FStaticMeshAttributes( *MergedRawMesh ).Register();
		for( int i = 0; i < ComponentsToMerge.Num(); i++ )
		{
			FMeshDescription* RawMesh = new FMeshDescription();
			FStaticMeshAttributes( *RawMesh ).Register();
			RawMeshArray.Add( RawMesh );

			FMeshMergeHelpers_RetrieveMesh( RawMesh, ComponentsToMerge[ i ], 0, true );
			FStaticMeshOperations::FAppendSettings AppendSettings;
			FStaticMeshOperations::AppendMeshDescription( *RawMesh, *MergedRawMesh, AppendSettings );
		}

		static int ID = 0;		
		FString MeshName = ComponentsToMerge[ 0 ]->GetStaticMesh()->GetName();
		FString PackageName = FString::Printf( TEXT( "/Game/%s_Splines_%d" ), *MeshName, ID );

		UStaticMesh* NewStaticMesh = CreateStaticMeshFromMeshDescription( PackageName, MergedRawMesh, ComponentsToMerge[ 0 ]->GetStaticMesh() );

		FTransform ActorTransform;// = Actor->GetActorTransform();
		UWorld* World = GEditor->GetEditorWorldContext().World();
		AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &ActorTransform );
		AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
		NewSMActor->GetStaticMeshComponent()->SetStaticMesh( NewStaticMesh );

		FString NewName = FString::Printf( TEXT( "%s_Splines_%d" ), *MeshName, ID );
		NewSMActor->Rename( *NewName );
		NewSMActor->SetActorLabel( NewName );
		//Destroy original so if I export twice I don't get duplicates
		Actor->Destroy();
		ID++;
	}
}
static bool IsValidSkinnedMeshComponent( USkinnedMeshComponent* InComponent )
{
	return InComponent && InComponent->MeshObject && InComponent->IsVisible();
}
static bool IsValidStaticMeshComponent( UStaticMeshComponent* InComponent )
{
	return InComponent && InComponent->GetStaticMesh() && GetRenderData( InComponent->GetStaticMesh()) && InComponent->IsVisible();
}

/** Helper struct for tracking validity of optional buffers */
struct FRawMeshTracker
{
	FRawMeshTracker()
		: bValidColors( false )
	{
		FMemory::Memset( bValidTexCoords, 0 );
	}

	bool bValidTexCoords[ MAX_MESH_TEXTURE_COORDS ];
	bool bValidColors;
};
static void AddOrDuplicateMaterial( UMaterialInterface* InMaterialInterface, const FString& InPackageName, TArray<UMaterialInterface*>& OutMaterials )
{
	if( InMaterialInterface && !InMaterialInterface->GetOuter()->IsA<UPackage>() )
	{
		// Convert runtime material instances to new concrete material instances
		// Create new package
		FString OriginalMaterialName = InMaterialInterface->GetName();
		FString MaterialPath = FPackageName::GetLongPackagePath( InPackageName ) / OriginalMaterialName;
		FString MaterialName;
		//FAssetToolsModule& 
			AssetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
		AssetToolsModule->Get().CreateUniqueAssetName( MaterialPath, TEXT( "" ), MaterialPath, MaterialName );
		UPackage* MaterialPackage = CreatePackage( *MaterialPath );

		// Duplicate the object into the new package
		UMaterialInterface* NewMaterialInterface = DuplicateObject<UMaterialInterface>( InMaterialInterface, MaterialPackage, *MaterialName );
		NewMaterialInterface->SetFlags( RF_Public | RF_Standalone );

		if( UMaterialInstanceDynamic* MaterialInstanceDynamic = Cast<UMaterialInstanceDynamic>( NewMaterialInterface ) )
		{
			UMaterialInstanceDynamic* OldMaterialInstanceDynamic = CastChecked<UMaterialInstanceDynamic>( InMaterialInterface );
			MaterialInstanceDynamic->K2_CopyMaterialInstanceParameters( OldMaterialInstanceDynamic );
		}

		NewMaterialInterface->MarkPackageDirty();

		FAssetRegistryModule::AssetCreated( NewMaterialInterface );

		InMaterialInterface = NewMaterialInterface;
	}

	OutMaterials.Add( InMaterialInterface );
}
template <typename ComponentType>
static void ProcessMaterials( ComponentType* InComponent, const FString& InPackageName, TArray<UMaterialInterface*>& OutMaterials )
{
	const int32 NumMaterials = InComponent->GetNumMaterials();
	for( int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++ )
	{
		UMaterialInterface* MaterialInterface = InComponent->GetMaterial( MaterialIndex );
		AddOrDuplicateMaterial( MaterialInterface, InPackageName, OutMaterials );
	}
}
const TArray<FSkeletalMaterial>& GetMaterials( const USkeletalMesh* SkeletalMesh );
static void SkinnedMeshToRawMeshes( USkinnedMeshComponent* InSkinnedMeshComponent, int32 InOverallMaxLODs, const FMatrix& InComponentToWorld, const FString& InPackageName, TArray<FRawMeshTracker>& OutRawMeshTrackers, TArray<FRawMesh>& OutRawMeshes, TArray<UMaterialInterface*>& OutMaterials )
{
	const int32 BaseMaterialIndex = OutMaterials.Num();

	// Export all LODs to raw meshes
	const int32 NumLODs = InSkinnedMeshComponent->GetNumLODs();

	for( int32 OverallLODIndex = 0; OverallLODIndex < InOverallMaxLODs; OverallLODIndex++ )
	{
		int32 LODIndexRead = FMath::Min( OverallLODIndex, NumLODs - 1 );

		FRawMesh& RawMesh = OutRawMeshes[ OverallLODIndex ];
		FRawMeshTracker& RawMeshTracker = OutRawMeshTrackers[ OverallLODIndex ];
		const int32 BaseVertexIndex = RawMesh.VertexPositions.Num();

		FSkeletalMeshLODInfo& SrcLODInfo = *( InSkinnedMeshComponent->SkeletalMesh->GetLODInfo( LODIndexRead ) );

		// Get the CPU skinned verts for this LOD
		TArray<FFinalSkinVertex> FinalVertices;
		InSkinnedMeshComponent->GetCPUSkinnedVertices( FinalVertices, LODIndexRead );

		FSkeletalMeshRenderData& SkeletalMeshRenderData = InSkinnedMeshComponent->MeshObject->GetSkeletalMeshRenderData();
		FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData.LODRenderData[ LODIndexRead ];

		// Copy skinned vertex positions
		for( int32 VertIndex = 0; VertIndex < FinalVertices.Num(); ++VertIndex )
		{
			auto NewPos = InComponentToWorld.TransformPosition( ToFVector3d( FinalVertices[ VertIndex ].Position ) );
			RawMesh.VertexPositions.Add( ToFVector3f( NewPos) );												
		}

		const uint32 NumTexCoords = FMath::Min( LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(), (uint32)MAX_MESH_TEXTURE_COORDS );
		const int32 NumSections = LODData.RenderSections.Num();
		FRawStaticIndexBuffer16or32Interface& IndexBuffer = *LODData.MultiSizeIndexContainer.GetIndexBuffer();

		for( int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++ )
		{
			const FSkelMeshRenderSection& SkelMeshSection = LODData.RenderSections[ SectionIndex ];
			if( InSkinnedMeshComponent->IsMaterialSectionShown( SkelMeshSection.MaterialIndex, LODIndexRead ) )
			{
				// Build 'wedge' info
				const int32 NumWedges = SkelMeshSection.NumTriangles * 3;
				for( int32 WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++ )
				{
					const int32 VertexIndexForWedge = IndexBuffer.Get( SkelMeshSection.BaseIndex + WedgeIndex );
					//Fix for rare crash
					if( VertexIndexForWedge >= FinalVertices.Num() )
						continue;

					RawMesh.WedgeIndices.Add( BaseVertexIndex + VertexIndexForWedge );

					const FFinalSkinVertex& SkinnedVertex = FinalVertices[ VertexIndexForWedge ];
					const FVector TangentX = InComponentToWorld.TransformVector( SkinnedVertex.TangentX.ToFVector() );
					const FVector TangentZ = InComponentToWorld.TransformVector( SkinnedVertex.TangentZ.ToFVector() );
					const FVector4 UnpackedTangentZ = SkinnedVertex.TangentZ.ToFVector4();
					const FVector TangentY = ( TangentZ ^ TangentX ).GetSafeNormal() * UnpackedTangentZ.W;

					RawMesh.WedgeTangentX.Add( ToFVector3f( TangentX ) );
					RawMesh.WedgeTangentY.Add( ToFVector3f( TangentY ) );
					RawMesh.WedgeTangentZ.Add( ToFVector3f( TangentZ ) );

					for( uint32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; TexCoordIndex++ )
					{
						if( TexCoordIndex >= NumTexCoords )
						{
							RawMesh.WedgeTexCoords[ TexCoordIndex ].AddDefaulted();
						}
						else
						{
							RawMesh.WedgeTexCoords[ TexCoordIndex ].Add( LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV( VertexIndexForWedge, TexCoordIndex ) );
							RawMeshTracker.bValidTexCoords[ TexCoordIndex ] = true;
						}
					}

					if( LODData.StaticVertexBuffers.ColorVertexBuffer.IsInitialized() )
					{
						RawMesh.WedgeColors.Add( LODData.StaticVertexBuffers.ColorVertexBuffer.VertexColor( VertexIndexForWedge ) );
						RawMeshTracker.bValidColors = true;
					}
					else
					{
						RawMesh.WedgeColors.Add( FColor::White );
					}
				}

				int32 MaterialIndex = SkelMeshSection.MaterialIndex;
				// use the remapping of material indices if there is a valid value
				if( SrcLODInfo.LODMaterialMap.IsValidIndex( SectionIndex ) && SrcLODInfo.LODMaterialMap[ SectionIndex ] != INDEX_NONE )
				{
					MaterialIndex = FMath::Clamp<int32>( SrcLODInfo.LODMaterialMap[ SectionIndex ], 0, GetMaterials( InSkinnedMeshComponent->SkeletalMesh).Num() );
				}

				// copy face info
				for( uint32 TriIndex = 0; TriIndex < SkelMeshSection.NumTriangles; TriIndex++ )
				{
					RawMesh.FaceMaterialIndices.Add( BaseMaterialIndex + MaterialIndex );
					RawMesh.FaceSmoothingMasks.Add( 0 ); // Assume this is ignored as bRecomputeNormals is false
				}
			}
		}
	}

	ProcessMaterials<USkinnedMeshComponent>( InSkinnedMeshComponent, InPackageName, OutMaterials );
}
UStaticMesh* FMeshUtilities_ConvertMeshesToStaticMesh( const TArray<UMeshComponent*>& InMeshComponents, const FTransform& InRootTransform, const FString& InPackageName )
{
	UStaticMesh* StaticMesh = nullptr;

	// Build a package name to use
	FString MeshName;
	FString PackageName;
	if( InPackageName.IsEmpty() )
	{
		FString NewNameSuggestion = FString( TEXT( "StaticMesh" ) );
		FString PackageNameSuggestion = FString( TEXT( "/Game/Meshes/" ) ) + NewNameSuggestion;
		FString Name;
		//FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
						   AssetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
		AssetToolsModule->Get().CreateUniqueAssetName( PackageNameSuggestion, TEXT( "" ), PackageNameSuggestion, Name );

		//TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
		//	SNew( SDlgPickAssetPath )
		//	.Title( LOCTEXT( "ConvertToStaticMeshPickName", "Choose New StaticMesh Location" ) )
		//	.DefaultAssetPath( FText::FromString( PackageNameSuggestion ) );

		UMeshComponent* MC = InMeshComponents[ 0 ];
		USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>( MC );
		FString FullAssetPath = FString::Printf( TEXT( "/Game/Meshes/%s_AsStaticMesh" ), *SMC->SkeletalMesh->GetName() );
		//if( PickAssetPathWidget->ShowModal() == EAppReturnType::Ok )
		{
			// Get the full name of where we want to create the mesh asset.
			PackageName = FullAssetPath;// PickAssetPathWidget->GetFullAssetPath().ToString();
			MeshName = FPackageName::GetLongPackageAssetName( PackageName );

			// Check if the user inputed a valid asset name, if they did not, give it the generated default name
			if( MeshName.IsEmpty() )
			{
				// Use the defaults that were already generated.
				PackageName = PackageNameSuggestion;
				MeshName = *Name;
			}
		}
	}
	else
	{
		PackageName = InPackageName;
		MeshName = *FPackageName::GetLongPackageAssetName( PackageName );
	}

	if( !PackageName.IsEmpty() && !MeshName.IsEmpty() )
	{
		TArray<FRawMesh> RawMeshes;
		TArray<UMaterialInterface*> Materials;

		TArray<FRawMeshTracker> RawMeshTrackers;

		FMatrix WorldToRoot = InRootTransform.ToMatrixWithScale().Inverse();

		// first do a pass to determine the max LOD level we will be combining meshes into
		int32 OverallMaxLODs = 0;
		for( UMeshComponent* MeshComponent : InMeshComponents )
		{
			USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>( MeshComponent );
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( MeshComponent );

			if( IsValidSkinnedMeshComponent( SkinnedMeshComponent ) )
			{
				OverallMaxLODs = FMath::Max( SkinnedMeshComponent->MeshObject->GetSkeletalMeshRenderData().LODRenderData.Num(), OverallMaxLODs );
			}
			else if( IsValidStaticMeshComponent( StaticMeshComponent ) )
			{
				OverallMaxLODs = FMath::Max( GetRenderData( StaticMeshComponent->GetStaticMesh() )->LODResources.Num(), OverallMaxLODs );
			}
		}

		// Resize raw meshes to accommodate the number of LODs we will need
		RawMeshes.SetNum( OverallMaxLODs );
		RawMeshTrackers.SetNum( OverallMaxLODs );

		// Export all visible components
		for( UMeshComponent* MeshComponent : InMeshComponents )
		{
			FMatrix ComponentToWorld = MeshComponent->GetComponentTransform().ToMatrixWithScale() * WorldToRoot;

			USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>( MeshComponent );
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( MeshComponent );

			if( IsValidSkinnedMeshComponent( SkinnedMeshComponent ) )
			{
				SkinnedMeshToRawMeshes( SkinnedMeshComponent, OverallMaxLODs, ComponentToWorld, PackageName, RawMeshTrackers, RawMeshes, Materials );
			}
			else if( IsValidStaticMeshComponent( StaticMeshComponent ) )
			{
				//StaticMeshToRawMeshes( StaticMeshComponent, OverallMaxLODs, ComponentToWorld, PackageName, RawMeshTrackers, RawMeshes, Materials );
			}
		}

		uint32 MaxInUseTextureCoordinate = 0;

		// scrub invalid vert color & tex coord data
		check( RawMeshes.Num() == RawMeshTrackers.Num() );
		for( int32 RawMeshIndex = 0; RawMeshIndex < RawMeshes.Num(); RawMeshIndex++ )
		{
			if( !RawMeshTrackers[ RawMeshIndex ].bValidColors )
			{
				RawMeshes[ RawMeshIndex ].WedgeColors.Empty();
			}

			for( uint32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; TexCoordIndex++ )
			{
				if( !RawMeshTrackers[ RawMeshIndex ].bValidTexCoords[ TexCoordIndex ] )
				{
					RawMeshes[ RawMeshIndex ].WedgeTexCoords[ TexCoordIndex ].Empty();
				}
				else
				{
					// Store first texture coordinate index not in use
					MaxInUseTextureCoordinate = FMath::Max( MaxInUseTextureCoordinate, TexCoordIndex );
				}
			}
		}

		// Check if we got some valid data.
		bool bValidData = false;
		for( FRawMesh& RawMesh : RawMeshes )
		{
			if( RawMesh.IsValidOrFixable() )
			{
				bValidData = true;
				break;
			}
		}

		if( bValidData )
		{
			// Then find/create it.
			UPackage* Package = CreatePackage( *PackageName );
			check( Package );

			// Create StaticMesh object
			StaticMesh = NewObject<UStaticMesh>( Package, *MeshName, RF_Public | RF_Standalone );
			StaticMesh->InitResources();

			StaticMesh->SetLightingGuid();

			// Determine which texture coordinate map should be used for storing/generating the lightmap UVs
			const uint32 LightMapIndex = FMath::Min( MaxInUseTextureCoordinate + 1, (uint32)MAX_MESH_TEXTURE_COORDS - 1 );

			// Add source to new StaticMesh
			for( FRawMesh& RawMesh : RawMeshes )
			{
				if( RawMesh.IsValidOrFixable() )
				{
					FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
					SrcModel.BuildSettings.bRecomputeNormals = false;
					SrcModel.BuildSettings.bRecomputeTangents = false;
					SrcModel.BuildSettings.bRemoveDegenerates = true;
					SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
					SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
					SrcModel.BuildSettings.bGenerateLightmapUVs = true;
					SrcModel.BuildSettings.SrcLightmapIndex = 0;
					SrcModel.BuildSettings.DstLightmapIndex = LightMapIndex;
					SrcModel.SaveRawMesh( RawMesh );
				}
			}

			// Copy materials to new mesh 
			for( UMaterialInterface* Material : Materials )
			{
				const TArray<FStaticMaterial>& StaticMaterials = GetStaticMaterials( StaticMesh );
				TArray<FStaticMaterial>& StaticMaterialsMutable = ( TArray<FStaticMaterial>&)StaticMaterials;
				StaticMaterialsMutable.Add( FStaticMaterial( Material ) );
			}

			//Set the Imported version before calling the build
			StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		#if ENGINE_MINOR_VERSION >= 27
			// Set light map coordinate index to match DstLightmapIndex
			StaticMesh->SetLightMapCoordinateIndex( LightMapIndex );
		#endif
			// setup section info map
			for( int32 RawMeshLODIndex = 0; RawMeshLODIndex < RawMeshes.Num(); RawMeshLODIndex++ )
			{
				const FRawMesh& RawMesh = RawMeshes[ RawMeshLODIndex ];
				TArray<int32> UniqueMaterialIndices;
				for( int32 MaterialIndex : RawMesh.FaceMaterialIndices )
				{
					UniqueMaterialIndices.AddUnique( MaterialIndex );
				}

				int32 SectionIndex = 0;
				for( int32 UniqueMaterialIndex : UniqueMaterialIndices )
				{
					StaticMesh->GetSectionInfoMap().Set( RawMeshLODIndex, SectionIndex, FMeshSectionInfo( UniqueMaterialIndex ) );
					SectionIndex++;
				}
			}
			StaticMesh->GetOriginalSectionInfoMap().CopyFrom( StaticMesh->GetSectionInfoMap() );

			// Build mesh from source
			StaticMesh->Build( false );
			StaticMesh->PostEditChange();

			StaticMesh->MarkPackageDirty();

			// Notify asset registry of new asset
			FAssetRegistryModule::AssetCreated( StaticMesh );

			// Display notification so users can quickly access the mesh
			//if( GIsEditor )
			//{
			//	FNotificationInfo Info( FText::Format( LOCTEXT( "SkeletalMeshConverted", "Successfully Converted Mesh" ), FText::FromString( StaticMesh->GetName() ) ) );
			//	Info.ExpireDuration = 8.0f;
			//	Info.bUseLargeFont = false;
			//	Info.Hyperlink = FSimpleDelegate::CreateLambda( [=]() { GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets( TArray<UObject*>( { StaticMesh } ) ); } );
			//	Info.HyperlinkText = FText::Format( LOCTEXT( "OpenNewAnimationHyperlink", "Open {0}" ), FText::FromString( StaticMesh->GetName() ) );
			//	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( Info );
			//	if( Notification.IsValid() )
			//	{
			//		Notification->SetCompletionState( SNotificationItem::CS_Success );
			//	}
			//}
		}
	}

	return StaticMesh;
}
void FUnrealToUnityModule::ConvertSkeletalMeshesToStaticActors()
{
	TArray<USkeletalMesh*> SkeletalMeshSources;
	TArray<UStaticMesh*> GeneratedMeshes;
	static int GeneratedSkeletalID = 0;
	UWorld* world = GEditor->GetEditorWorldContext().World();
	for( TActorIterator<AActor> iterator( world ); iterator; ++iterator )
	{
		AActor* Actor = *iterator;
		if( Actor->IsHiddenEd() )
			continue;
		TArray<UActorComponent*> comps;
		Actor->GetComponents( comps );		
		for( int i = 0; i < comps.Num(); i++ )
		{
			UActorComponent* AC = comps[ i ];
			USkeletalMeshComponent* SkeletalMeshComponent = dynamic_cast<USkeletalMeshComponent*>( AC );
			if( SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh )
			{
				UStaticMesh* GeneratedStaticMesh = nullptr;
				int Index = SkeletalMeshSources.Find( SkeletalMeshComponent->SkeletalMesh );
				FTransform ComponentToWorld = SkeletalMeshComponent->GetComponentToWorld();
				if( Index == INDEX_NONE )
				{
					TArray<UMeshComponent*> InMeshComponents;
					InMeshComponents.Add( SkeletalMeshComponent );
					FString InPackageName;
					FTransform DefaultTransform;					
					SkeletalMeshComponent->SetComponentToWorld( DefaultTransform );
					GeneratedStaticMesh = FMeshUtilities_ConvertMeshesToStaticMesh( InMeshComponents, DefaultTransform, InPackageName );

					GeneratedMeshes.Add( GeneratedStaticMesh );
					SkeletalMeshSources.Add( SkeletalMeshComponent->SkeletalMesh );
				}
				else
					GeneratedStaticMesh = GeneratedMeshes[ Index ];

				if( GeneratedStaticMesh )
				{
					UWorld* World = GEditor->GetEditorWorldContext().World();
					AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &ComponentToWorld );
					AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
					NewSMActor->GetStaticMeshComponent()->SetStaticMesh( GeneratedStaticMesh );

					FString NewName = FString::Printf( TEXT( "%s_%d" ), *GeneratedStaticMesh->GetName(), GeneratedSkeletalID++ );
					NewSMActor->Rename( *NewName );
					NewSMActor->SetActorLabel( NewName );

					SkeletalMeshComponent->DestroyComponent();
				}
				else
				{
					UE_LOG( LogTemp, Warning, TEXT( "ERROR! FMeshUtilities_ConvertMeshesToStaticMesh returned nullptr!" ) );
				}
			}
		}
	}
}
void FUnrealToUnityModule::DetectMaterialsWithWPO()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	FVector LastPos(0,0,0);
	int NumMats = 0;
	auto lambda = [&]( FAssetData& Asset )
	{
		bool Modified = false;
		UMaterial* Material = Cast<UMaterial>( Asset.GetAsset() );
		if( Material )
		{
			if( Material->WorldPositionOffset.Expression )
			{
				auto lambda2 = [&]( FAssetData& Asset )
				{
					bool Modified = false;
					UStaticMesh* StaticMesh = Cast<UStaticMesh>( Asset.GetAsset() );
					if( StaticMesh )
					{
						auto Mats = GetStaticMaterials( StaticMesh );
						for( int i = 0; i < Mats.Num(); i++ )
						if ( Mats[ i ].MaterialInterface )
						{
							UMaterial* BaseMat = Mats[ i ].MaterialInterface->GetBaseMaterial();
							if ( BaseMat == Material )
							{
								FTransform Transform;
								Transform.SetTranslation( LastPos );
								LastPos.X += StaticMesh->GetBounds().BoxExtent.X;

								AActor* NewActor = World->SpawnActor( AStaticMeshActor::StaticClass(), &Transform );
								AStaticMeshActor* NewSMActor = Cast<AStaticMeshActor>( NewActor );
								NewSMActor->GetStaticMeshComponent()->SetStaticMesh( StaticMesh );
								return Modified;
							}
						}
					}

					return Modified;
				};

				IterateOverAllAssetsOfType( FName( "StaticMesh" ), lambda2, false );
				
				NumMats++;
			}
		}

		return Modified;
	};

	IterateOverAllAssetsOfType( FName( "Material" ), lambda, false );

	UE_LOG( LogTemp, Warning, TEXT( "ERROR! MaterialsWithWPO=%d" ), NumMats );
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE( FUnrealToUnityModule, UnrealToUnity )

#include "UnityScene.h"
#include "Module.h"

#if PLATFORM_WINDOWS
#else
//#define snprintf sprintf
#endif
#if WITH_EDITOR

std::string NullGUID = "0000000000000000f000000000000000";

UnityComponent::UnityComponent()
{
	ID = GetUniqueID();
}
std::string UnityComponent::GetSceneData()
{
	return "";
}
uint64 UnityComponent::GetUniqueID()
{
	uint64 ID = 1030971880 + ( Counter++);
	return ID;
}

uint64 UnityComponent::Counter = 0;

TransformComponent::TransformComponent()
{
	Type = CT_TRANSFORM;

	Reset();
}
void TransformComponent::Reset()
{
	memset( LocalPosition, 0, sizeof( LocalPosition ) );
	memset( LocalRotation, 0, sizeof( LocalRotation ) );
	memset( LocalScale, 0, sizeof( LocalScale ) );

	LocalRotation[ 3 ] = 1.0f;
	for( int i = 0; i < 3; i++ )
		LocalScale[ i ] = 1.0f;
}
void TransformComponent::Set( FVector Position, FQuat Quat, FVector Scale )
{
	LocalPosition[ 0 ] = Position.X;
	LocalPosition[ 1 ] = Position.Y;
	LocalPosition[ 2 ] = Position.Z;
	LocalRotation[ 0 ] = Quat.X;
	LocalRotation[ 1 ] = Quat.Y;
	LocalRotation[ 2 ] = Quat.Z;
	LocalRotation[ 3 ] = Quat.W;
	LocalScale[ 0 ] = Scale.X;
	LocalScale[ 1 ] = Scale.Y;
	LocalScale[ 2 ] = Scale.Z;
}
std::string TransformComponent::GetChildrenText()
{	
	if( Owner->Children.size() == 0 )
		return "[]\n";
	else
	{
		std::string Ret = "\n";
		for( int i = 0; i < Owner->Children.size(); i++ )
		{
			auto Child = Owner->Children[ i ];
			TransformComponent* TC = ( TransformComponent*)Child->GetComponent( ComponentType::CT_TRANSFORM );
			char Line[ 256 ];
			//uint64_t ID = 0;
			snprintf( Line, sizeof( Line ), "  - {fileID: %lld}\n", TC->ID );
			Ret += Line;
		}

		return Ret;
	}
}
std::string TransformComponent::GetSceneData()
{
	uint64 ParentID = 0;
	if( Owner->Parent )
	{
		TransformComponent * TC = ( TransformComponent*) Owner->Parent->GetComponent( CT_TRANSFORM );
		
		ParentID = TC->ID;
	}
	std::string ChildrenText = GetChildrenText();
	int Size = 1024 + ChildrenText.length();
	char* Text = new char[ Size ];
	snprintf( Text, Size,
			   "--- !u!4 &%lld\n"
			   "Transform:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_LocalRotation: {x: %f, y: %f, z: %f, w: %f}\n"
			   "  m_LocalPosition: {x: %f, y: %f, z: %f}\n"
			   "  m_LocalScale: {x: %f, y: %f, z: %f}\n"
			   "  m_Children: %s"
			   "  m_Father: {fileID: %lld}\n"
			   "  m_RootOrder: 0\n"
			   "  m_LocalEulerAnglesHint: {x: 0, y: 0, z: 0}\n"
			   ,
			   this->ID,
			   Owner->ID,//Gameobject
			   LocalRotation[ 0 ], LocalRotation[ 1 ], LocalRotation[ 2 ], LocalRotation[ 3 ],//Rotation
			   LocalPosition[ 0 ], LocalPosition[ 1 ], LocalPosition[ 2 ],//Position
			   LocalScale[ 0 ], LocalScale[ 1 ], LocalScale[ 2 ],//Scale
			   ChildrenText.c_str(),
			   ParentID//father
			   );
	
	std::string RetStr = Text;

	return RetStr;
}


std::string MeshRenderer::GetSceneData()
{
	std::string MaterialText;
	char Line[ 512 ];

	if( Materials.size() > 0 )
	{
		//MeshFilter* MF = (MeshFilter*)Owner->GetComponent( ComponentType::CT_MESHFILTER );
		
		for( int i = 0; i < Materials.size(); i++ )
		{
			int fileIDBase = 2100000;
			UnityMaterial* Mat = Materials[ i ];
			//Don't reindex materials since I already did that based on LOD level
			std::string MatGUID = "0000000000000000f000000000000000";
			if( Mat )
				MatGUID = Mat->GUID;
			snprintf( Line, sizeof(Line), "  - {fileID: %d, guid: %s, type: 2}\n", fileIDBase, MatGUID.c_str() );
			MaterialText += Line;
		}
	}
	else
	{
		snprintf( Line, sizeof(Line), "  - {fileID: 10303, guid: 0000000000000000f000000000000000, type: 0}\n" );
		MaterialText += Line;
	}

	int TextLen = 2048 + MaterialText.length();
	char* Text = new char[ TextLen ];

	snprintf( Text, TextLen,
	"--- !u!23 &%lld\n"
	"MeshRenderer:\n"
	"  m_ObjectHideFlags: 0\n"
	"  m_CorrespondingSourceObject: {fileID: 0}\n"
	"  m_PrefabInstance: {fileID: 0}\n"
	"  m_PrefabAsset: {fileID: 0}\n"
	"  m_GameObject: {fileID: %lld}\n"
	"  m_Enabled: 1\n"
	"  m_CastShadows: %d\n"
	"  m_ReceiveShadows: 1\n"
	"  m_DynamicOccludee: 1\n"
	"  m_MotionVectors: 1\n"
	"  m_LightProbeUsage: 1\n"
	"  m_ReflectionProbeUsage: 1\n"
	"  m_RayTracingMode: 2\n"
	"  m_RayTraceProcedural: 0\n"
	"  m_RenderingLayerMask: 1\n"
	"  m_RendererPriority: 0\n"
	"  m_Materials:\n"
	"%s"
	"  m_StaticBatchInfo:\n"
	"    firstSubMesh: 0\n"
	"    subMeshCount: 0\n"
	"  m_StaticBatchRoot: {fileID: 0}\n"
	"  m_ProbeAnchor: {fileID: 0}\n"
	"  m_LightProbeVolumeOverride: {fileID: 0}\n"
	"  m_ScaleInLightmap: 1\n"
	"  m_ReceiveGI: 1\n"
	"  m_PreserveUVs: 1\n"
	"  m_IgnoreNormalsForChartDetection: 0\n"
	"  m_ImportantGI: 0\n"
	"  m_StitchLightmapSeams: 1\n"
	"  m_SelectedEditorRenderState: 3\n"
	"  m_MinimumChartSize: 4\n"
	"  m_AutoUVMaxDistance: 0.5\n"
	"  m_AutoUVMaxAngle: 89\n"
	"  m_LightmapParameters: {fileID: 0}\n"
	"  m_SortingLayerID: 0\n"
	"  m_SortingLayer: 0\n"
	"  m_SortingOrder: 0\n"
	"  m_AdditionalVertexStreams: {fileID: 0}\n"
	, ID, Owner->ID, CastShadows, MaterialText.c_str() );

	std::string RetStr = Text;

	delete[] Text;

	return RetStr;
}
std::string SkinnedMeshRenderer::GetSceneData()
{
	std::string MaterialText;
	char Line[ 512 ];

	if( Materials.size() > 0 )
	{
		MeshFilter* MF = (MeshFilter*)Owner->GetComponent( ComponentType::CT_MESHFILTER );

		for( int i = 0; i < Materials.size(); i++ )
		{
			int fileIDBase = 2100000;
			UnityMaterial* Mat = Materials[ i ];
			//Don't reindex materials since I already did that based on LOD level
			std::string MatGUID = "0000000000000000f000000000000000";
			if( Mat )
				MatGUID = Mat->GUID;
			snprintf( Line, sizeof(Line), "  - {fileID: %d, guid: %s, type: 2}\n", fileIDBase, MatGUID.c_str());
			MaterialText += Line;
		}
	}
	else
	{
		snprintf( Line, sizeof( Line ), "  - {fileID: 10303, guid: 0000000000000000f000000000000000, type: 0}\n" );
		MaterialText += Line;
	}

	int TextLen = 2048 + MaterialText.length();
	char* Text = new char[ TextLen ];

	snprintf( Text, TextLen,
			   "--- !u!137 &%lld\n"
			   "SkinnedMeshRenderer:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Enabled: 1\n"
			   "  m_CastShadows: %d\n"
			   "  m_ReceiveShadows: 1\n"
			   "  m_DynamicOccludee: 1\n"
			   "  m_StaticShadowCaster: 0\n"
			   "  m_MotionVectors: 1\n"
			   "  m_LightProbeUsage: 1\n"
			   "  m_ReflectionProbeUsage: 1\n"
			   "  m_RayTracingMode: 3\n"
			   "  m_RayTraceProcedural: 0\n"
			   "  m_RenderingLayerMask: 1\n"
			   "  m_RendererPriority: 0\n"
			   "  m_Materials:\n"
			   "%s"
			   "  m_StaticBatchInfo:\n"
			   "    firstSubMesh: 0\n"
			   "    subMeshCount: 0\n"
			   "  m_StaticBatchRoot: {fileID: 0}\n"
			   "  m_ProbeAnchor: {fileID: 0}\n"
			   "  m_LightProbeVolumeOverride: {fileID: 0}\n"
			   "  m_ScaleInLightmap: 1\n"
			   "  m_ReceiveGI: 1\n"
			   "  m_PreserveUVs: 1\n"
			   "  m_IgnoreNormalsForChartDetection: 0\n"
			   "  m_ImportantGI: 0\n"
			   "  m_StitchLightmapSeams: 1\n"
			   "  m_SelectedEditorRenderState: 3\n"
			   "  m_MinimumChartSize: 4\n"
			   "  m_AutoUVMaxDistance: 0.5\n"
			   "  m_AutoUVMaxAngle: 89\n"
			   "  m_LightmapParameters: {fileID: 0}\n"
			   "  m_SortingLayerID: 0\n"
			   "  m_SortingLayer: 0\n"
			   "  m_SortingOrder: 0\n"
			   "  serializedVersion: 2\n"
			   "  m_Quality: 0\n"
			   "  m_UpdateWhenOffscreen: 0\n"
			   "  m_SkinnedMotionVectors: 1\n"
			   "  m_Mesh: {fileID: 0}\n"
			   "  m_Bones: []\n"
			   "  m_BlendShapeWeights: []\n"
			   "  m_RootBone: {fileID: 0}\n"
			   "  m_AABB:\n"
			   "    m_Center: {x: 0, y: 0, z: 0}\n"
			   "    m_Extent: {x: 0, y: 0, z: 0}\n"
			   "  m_DirtyAABB: 0\n"
			   , ID, Owner->ID, CastShadows, MaterialText.c_str() );

	std::string RetStr = Text;

	delete[] Text;

	return RetStr;
}
void Renderer::AddMaterial( UnityMaterial* NewMat, bool HasDuplicateMaterials )
{
	//Only needs this when base mesh has duplicate materials that will get merged by unity
	//if( HasDuplicateMaterials )
	//{
	//	for( int i = 0; i < Materials.size(); i++ )
	//	{
	//		if( Materials[ i ] == NewMat )
	//		{
	//			//If it's a duplicate, don't add it
	//			return;
	//		}
	//	}
	//}
	Materials.push_back( NewMat );
}
std::string MeshFilter::GetSceneData()
{
	//uint64_t SubMeshID = 4300000 + LOD * 2;

	char Text[ 1024 ];
	snprintf( Text, sizeof( Text ),
		"--- !u!33 &%lld\n"
		"MeshFilter:\n"
		"  m_ObjectHideFlags: 0\n"
		"  m_CorrespondingSourceObject: {fileID: 0}\n"
		"  m_PrefabInstance: {fileID: 0}\n"
		"  m_PrefabAsset: {fileID: 0}\n"
		"  m_GameObject: {fileID: %lld}\n"
		"  m_Mesh: {fileID: %lld, guid: %s, type: 3}\n"
		, this->ID, Owner->ID, SubMeshID, Mesh->GUID.c_str() );

	std::string RetStr = Text;

	return RetStr;
}
std::string LodGroup::GetLODsString()
{
	std::string RetStr;
	for( int i = 0; i < Lods.size(); i++ )
	{
		LOD* lod = Lods[ i ];

		std::string RenderersText;
		for( int l = 0; l < lod->renderers.size(); l++ )
		{
			char Local[ 1024 ];
			snprintf( Local, sizeof( Local ), "    - renderer: {fileID: %lld}\n", lod->renderers[l]->ID );
			RenderersText += Local;
		}

		char Text[ 1024 ];
		snprintf( Text, sizeof( Text ),
				   "  - screenRelativeHeight: %0.2f\n"
				   "    fadeTransitionWidth: %0.1f\n"
				   "    renderers:\n", lod->screenRelativeTransitionHeight, lod->fadeTransitionWidth );
		RetStr += Text;
		RetStr += RenderersText;
	}

	return RetStr;
}
std::string LodGroup::GetSceneData()
{
	std::string LodsString = LodGroup::GetLODsString();	

	int TextSize = 1024 + LodsString.length();
	char* Text = new char[ TextSize ];
	snprintf( Text, TextSize,
			   "--- !u!205 &%lld\n"
			   "LODGroup:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  serializedVersion: 2\n"
			   "  m_LocalReferencePoint: {x: %f, y: %f, z: %f}\n"
			   "  m_Size: %f\n"
			   "  m_FadeMode: 0\n"
			   "  m_AnimateCrossFading: 0\n"
			   "  m_LastLODIsBillboard: 0\n"
			   "  m_LODs:\n"
			   "%s\n"
			   "  m_Enabled: 1\n"
			   , this->ID, Owner->ID, LocalReferencePoint.X, LocalReferencePoint.Y, LocalReferencePoint.Z, Size, LodsString.c_str() );

	std::string RetStr = Text;

	delete[] Text;

	return RetStr;
}

std::string MeshCollider::GetSceneData()
{
	std::string MeshGUID = "";
	if( Mesh )
		MeshGUID = Mesh->GUID;
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!64 &%lld\n"
			   "MeshCollider:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Material: {fileID: 0}\n"
			   "  m_IsTrigger: 0\n"
			   "    m_Enabled: 1\n"
			   "    serializedVersion: 3\n"
			   "    m_Convex: %d\n"
			   "    m_CookingOptions: 14\n"
			   "    m_Mesh : {fileID: %lld, guid : %s, type : 3}\n",
			   this->ID, Owner->ID, IsConvex ? 1 : 0, SubMeshID, MeshGUID.c_str() );

	return Text;
}
std::string BoxCollider::GetSceneData()
{
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!65 &%lld\n"
			   "BoxCollider:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Material: {fileID: 0}\n"
			   "  m_IsTrigger: 0\n"
			   "  m_Enabled: 1\n"
			   "  serializedVersion: 2\n"
			   "  m_Size : {x: %f, y : %f, z : %f}\n"
			   "  m_Center : {x: %f, y : %f, z : %f}\n",
			   this->ID, Owner->ID, Size.X, Size.Y, Size.Z, Center.X, Center.Y, Center.Z );

	return Text;
}
std::string SphereCollider::GetSceneData()
{
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!135 &%lld\n"
			   "SphereCollider:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Material: {fileID: 0}\n"
			   "  m_IsTrigger: 0\n"
			   "  m_Enabled: 1\n"
			   "  serializedVersion: 2\n"
			   "  m_Radius : %f\n"
			   "  m_Center : {x: %f, y : %f, z : %f}\n",
			   this->ID, Owner->ID, Radius, Center.X, Center.Y, Center.Z );

	return Text;
}
std::string CapsuleCollider::GetSceneData()
{
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!136 &%lld\n"
			   "CapsuleCollider:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Material: {fileID: 0}\n"
			   "  m_IsTrigger: 0\n"
			   "  m_Enabled: 1\n"
			   "  m_Radius : %f\n"
			   "  m_Height : %f\n"
			   "  m_Direction : %d\n"
			   "  m_Center : {x: %f, y : %f, z : %f}\n",
			   this->ID, Owner->ID, Radius, Height, Direction, Center.X, Center.Y, Center.Z );

	return Text;
}
float ToFloat( uint8 v )
{
	float r = float(v) / 255.0f;
	return r;
}
std::string LightComponent::GetSceneData()
{
	char Text[ 2048 ];
	int EnabledInt = (Enabled == true  ? 1: 0);
	int ShadowsType = ( Shadows == true ? 2 : 0 );
	snprintf( Text, sizeof( Text ),
			   "--- !u!108 &%lld\n"
			   "Light:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Enabled: %d\n"
			   "  serializedVersion: 8\n"
			   "  m_Type: %d\n"
			   "  m_Color: {r: %f, g: %f, b: %f, a: %f}\n"
			   "  m_Intensity: %f\n"
			   "  m_Range: %f\n"
			   "  m_SpotAngle: %f\n"
			   "  m_CookieSize: 10\n"
			   "  m_Shadows:\n"
			   "    m_Type: %d\n"
			   "    m_Resolution: -1\n"
			   "    m_CustomResolution: -1\n"
			   "    m_Strength: 1\n"
			   "    m_Bias: 0.05\n"
			   "    m_NormalBias: 0.4\n"
			   "    m_NearPlane: 0.2\n"
			   "  m_Cookie: {fileID: 0}\n"
			   "  m_DrawHalo: 0\n"
			   "  m_Flare: {fileID: 0}\n"
			   "  m_RenderMode: 0\n"
			   "  m_CullingMask:\n"
			   "    serializedVersion: 2\n"
			   "    m_Bits: 4294967295\n"
			   "  m_Lightmapping: 4\n"
			   "  m_LightShadowCasterMode: 0\n"
			   "  m_AreaSize: {x: %f, y: %f}\n"
			   "  m_BounceIntensity: 1\n"
			   "  m_ColorTemperature: 6570\n"
			   "  m_UseColorTemperature: 0\n"
			   "  m_ShadowRadius: 0\n"
			   "  m_ShadowAngle: 0\n",
			   this->ID, Owner->ID, EnabledInt, Type,
			   ToFloat(Color.R), ToFloat( Color.G), ToFloat( Color.B ), ToFloat( Color.A ),
			   Intensity, Range, SpotAngle, ShadowsType,
			   Width, Height );

	std::string RetStr = Text;

	return RetStr;
}
std::string ReflectionProbeComponent::GetSceneData()
{
	char Text[ 2048 ];
	snprintf( Text, sizeof( Text ),
			   "--- !u!215 &%lld\n"
			   "ReflectionProbe:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Enabled: 1\n"
			   "  serializedVersion: 2\n"
			   "  m_Type: 0\n"
			   "  m_Mode: 1\n"
			   "  m_RefreshMode: 0\n"
			   "  m_TimeSlicingMode: 0\n"
			   "  m_Resolution: 512\n"
			   "  m_UpdateFrequency: 0\n"
			   "  m_BoxSize: {x: %f, y: %f, z: %f}\n"
			   "  m_BoxOffset: {x: 0, y: 0, z: 0}\n"
			   "  m_NearClip: 0.3\n"
			   "  m_FarClip: 1000\n"
			   "  m_ShadowDistance: 100\n"
			   "  m_ClearFlags: 1\n"
			   "  m_BackGroundColor: {r: 0.19215687, g: 0.3019608, b: 0.4745098, a: 0}\n"
			   "  m_CullingMask:\n"
			   "    serializedVersion: 2\n"
			   "    m_Bits: 4294967295\n"
			   "  m_IntensityMultiplier: %f\n"
			   "  m_BlendDistance: 1\n"
			   "  m_HDR: 1\n"
			   "  m_BoxProjection: 0\n"
			   "  m_RenderDynamicObjects: 0\n"
			   "  m_UseOcclusionCulling: 1\n"
			   "  m_Importance: 1\n"
			   "  m_CustomBakedTexture: {fileID: 0}\n",
			   this->ID, Owner->ID, Size.X, Size.Y, Size.Z, Intensity );

	return Text;
}
std::string DecalComponent::GetSceneData()
{
	std::string MatGUID = "0000000000000000f000000000000000";
	if( Material )
		MatGUID = Material->GUID;

	char Text[ 2048 ];
	const char* ScriptGUID = "0777d029ed3dffa4692f417d4aba19ca";//URP
	if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
	{
		ScriptGUID = "f19d9143a39eb3b46bc4563e9889cfbd";
	}

	snprintf( Text, sizeof( Text ),
			   "--- !u!114 &%lld\n"
			   "MonoBehaviour:\n"
			   "  m_ObjectHideFlags: 0\n"
			   "  m_CorrespondingSourceObject: {fileID: 0}\n"
			   "  m_PrefabInstance: {fileID: 0}\n"
			   "  m_PrefabAsset: {fileID: 0}\n"
			   "  m_GameObject: {fileID: %lld}\n"
			   "  m_Enabled: 1\n"
			   "  m_EditorHideFlags: 0\n"
			   "  m_Script: {fileID: 11500000, guid: %s, type: 3}\n"
			   "  m_Name: \n"
			   "  m_EditorClassIdentifier: \n"
			   "  m_Material: {fileID: 2100000, guid: %s, type: 2}\n"
			   "  m_DrawDistance: 1000\n"
			   "  m_FadeScale: 0.9\n"
			   "  m_StartAngleFade: 180\n"
			   "  m_EndAngleFade: 180\n"
			   "  m_UVScale: {x: 1, y: 1}\n"
			   "  m_UVBias: {x: 0, y: 0}\n"
			   "  m_ScaleMode: 1\n"
			   "  m_Offset: {x: 0, y: 0, z: 0.0}\n"
			   "  m_Size: {x: %f, y: %f, z: %f}\n"
			   "  m_FadeFactor: 1\n",
			   this->ID, Owner->ID, ScriptGUID, MatGUID.c_str(), Size.X, Size.Y, Size.Z );

	return Text;
}
void UnityTexture::GenerateGUID()
{
	std::size_t hash = std::hash<std::string>{}( File );
	GUID = GetGUIDFromSizeT( hash );
}
void UnityMesh::GenerateGUID()
{
	std::size_t hash = std::hash<std::string>{}( File );
	GUID = GetGUIDFromSizeT( hash );
}
void UnityMaterial::GenerateShaderGUID()
{
	std::size_t hash = std::hash<std::string>{}( ShaderFileName );
	ShaderGUID = GetGUIDFromSizeT( hash );
}
void UnityMaterial::GenerateGUID()
{
	std::string Input = File + ShaderGUID;
	std::size_t hash = std::hash<std::string>{}( Input );
	GUID = GetGUIDFromSizeT( hash );
}
std::string GenerateGUIDFromString( std::string str )
{
	std::size_t hash = std::hash<std::string>{}( str );
	std::string Ret = GetGUIDFromSizeT( hash );
	return Ret;
}
GameObject::GameObject( )
{
	ID = UnityComponent::GetUniqueID();

	TransformComponent *Trans = new TransformComponent;
	Trans->Owner = this;
	Components.push_back( Trans );
}
UnityComponent* GameObject::AddComponent( ComponentType Type )
{
	if( Type == CT_MESHFILTER )
	{
		MeshFilter *MF = new MeshFilter;
		MF->Owner = this;
		Components.push_back( MF );
		return MF;
	}
	else if( Type == CT_MESHRENDERER )
	{
		MeshRenderer *MR = new MeshRenderer;
		MR->Owner = this;
		Components.push_back( MR );
		return MR;
	}
	else if( Type == CT_SKINNEDMESHRENDERER )
	{
		SkinnedMeshRenderer* SMR = new SkinnedMeshRenderer;
		SMR->Owner = this;
		Components.push_back( SMR );
		return SMR;
	}	
	else if( Type == CT_LIGHT )
	{
		LightComponent* NewLight = new LightComponent;
		NewLight->Owner = this;
		Components.push_back( NewLight );
		return NewLight;
	}
	else if( Type == CT_LODGROUP )
	{
		LodGroup* NewLodGroup = new LodGroup;
		NewLodGroup->Owner = this;
		Components.push_back( NewLodGroup );
		return NewLodGroup;
	}
	else if( Type == CT_MESHCOLLIDER )
	{
		MeshCollider* NewMeshCollider = new MeshCollider;
		NewMeshCollider->Owner = this;
		Components.push_back( NewMeshCollider );
		return NewMeshCollider;
	}
	else if( Type == CT_BOXCOLLIDER )
	{
		BoxCollider* NewBoxCollider = new BoxCollider;
		NewBoxCollider->Owner = this;
		Components.push_back( NewBoxCollider );
		return NewBoxCollider;
	}
	else if( Type == CT_SPHERECOLLIDER )
	{
		SphereCollider* NewSphereCollider = new SphereCollider;
		NewSphereCollider->Owner = this;
		Components.push_back( NewSphereCollider );
		return NewSphereCollider;
	}
	else if( Type == CT_CAPSULECOLLIDER )
	{
		CapsuleCollider* NewCapsuleCollider = new CapsuleCollider;
		NewCapsuleCollider->Owner = this;
		Components.push_back( NewCapsuleCollider );
		return NewCapsuleCollider;
	}
	else if( Type == CT_REFLECTIONPROBE )
	{
		ReflectionProbeComponent* NewReflectionProbeComponent = new ReflectionProbeComponent;
		NewReflectionProbeComponent->Owner = this;
		Components.push_back( NewReflectionProbeComponent );
		return NewReflectionProbeComponent;
	}
	else if( Type == CT_DECAL )
	{
		DecalComponent* NewDecalComponent = new DecalComponent;
		NewDecalComponent->Owner = this;
		Components.push_back( NewDecalComponent );
		return NewDecalComponent;
	}

	return nullptr;
}
void GameObject::AddChild( GameObject* GO )
{
	Children.push_back( GO );
	GO->Parent = this;
}
UnityComponent*GameObject::GetComponent( ComponentType Type )
{
	for ( int i = 0; i < Components.size(); i++ )
	{
		if ( Components[ i ]->Type == Type )
			return Components[ i ];
	}

	return nullptr;
}
UnityComponent* GameObject::GetComponentInChildren( ComponentType Type )
{
	for( int i = 0; i < Components.size(); i++ )
	{
		if( Components[ i ]->Type == Type )
			return Components[ i ];
	}

	for( int i = 0; i < Children.size(); i++ )
	{
		UnityComponent* Comp = Children[ i ]->GetComponentInChildren( Type );
		if( Comp )
			return Comp;
	}

	return nullptr;
}
void GameObject::GetAllComponentsInChildren( std::vector< UnityComponent* >& AllComponents )
{
	for( int i = 0; i < Components.size(); i++ )
	{
		AllComponents.push_back( Components[ i ] );
	}

	for( int i = 0; i < Children.size(); i++ )
	{
		Children[ i ]->GetAllComponentsInChildren( AllComponents );
	}
}
std::string GameObject::ToSceneData( bool WritePrefab )
{
	std::string RetStr;

	std::string MainStr = GetGameObjectString( WritePrefab );
	RetStr += MainStr;

	for( int i = 0; i < Components.size(); i++ )
	{
		auto Comp = Components[ i ];
		if( WritePrefab && Comp->Type == ComponentType::CT_TRANSFORM )
		{
			TransformComponent* Trans = (TransformComponent*)Comp;
			TransformComponent OriginalTransform;
			OriginalTransform = *Trans;
			Trans->Reset();
			RetStr += Comp->GetSceneData();
			*Trans = OriginalTransform;
		}
		else
			RetStr += Comp->GetSceneData();
	}

	for( int i = 0; i < Children.size(); i++ )
	{
		auto Child = Children[ i ];
		std::string ChildStr = Child->ToSceneData();
		RetStr += ChildStr;
	}

	return RetStr;
}
std::string GameObject::GeneratePrefabInstance()
{
	if( !PrefabTemplate )
		return "";
	
	TransformComponent* TemplateTransform = ( TransformComponent*)PrefabTemplate->GetComponent( CT_TRANSFORM );
	TransformComponent* InstanceTransform = ( TransformComponent*)GetComponent( CT_TRANSFORM );
	uint64 PrefabTransformID = 0;
	if( TemplateTransform )
		PrefabTransformID = TemplateTransform->ID;

	std::string PrefabID = PrefabTemplate->PrefabGUID;
	char Text[ 4096 ];
	snprintf( Text, sizeof( Text ),
		"--- !u!1001 &%lld\n"
		"PrefabInstance:\n"
		"  m_ObjectHideFlags: 0\n"
		"  serializedVersion: 2\n"
		"  m_Modification:\n"
		"    m_TransformParent: {fileID: 0}\n"
		"    m_Modifications:\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_Name\n"
		"      value: %s\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalPosition.x\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalPosition.y\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalPosition.z\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalRotation.x\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalRotation.y\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalRotation.z\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalRotation.w\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		//"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		//"      propertyPath: m_LocalEulerAnglesHint.x\n"
		//"      value: %f\n"
		//"      objectReference: {fileID: 0}\n"
		//"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		//"      propertyPath: m_LocalEulerAnglesHint.y\n"
		//"      value:%f\n"
		//"      objectReference: {fileID: 0}\n"
		//"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		//"      propertyPath: m_LocalEulerAnglesHint.z\n"
		//"      value: %f\n"
		//"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalScale.x\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalScale.y\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    - target: {fileID: %lld, guid: %s, type: 3}\n"
		"      propertyPath: m_LocalScale.z\n"
		"      value: %f\n"
		"      objectReference: {fileID: 0}\n"
		"    m_RemovedComponents: []\n"
		"  m_SourcePrefab: {fileID: 100100000, guid : %s, type : 3}\n", ID, 
			   PrefabTemplate->ID, PrefabID.c_str(), this->Name.c_str(),

			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalPosition[ 0 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalPosition[ 1 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalPosition[ 2 ],

			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalRotation[ 0 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalRotation[ 1 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalRotation[ 2 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalRotation[ 3 ],

			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalScale[ 0 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalScale[ 1 ],
			   PrefabTransformID, PrefabID.c_str(), InstanceTransform->LocalScale[ 2 ],

			   PrefabID.c_str()
	);

	return Text;
}
std::string GameObject::GetGameObjectString( bool WritePrefab )
{
	int serializedVersion = 6;
	
	char Text[ 1024 ];
	snprintf( Text, sizeof( Text ),
		"--- !u!1 &%lld\n"
		"GameObject:\n"
		"  m_ObjectHideFlags: 0\n"
		"  m_CorrespondingSourceObject: {fileID: 0}\n"
		"  m_PrefabInstance: {fileID: 0}\n"
		"  m_PrefabAsset: {fileID: 0}\n"
		"  serializedVersion: %d\n"
		"  m_Component:\n",
			 this->ID, serializedVersion );

	std::string Str = Text;

	for ( int i = 0; i < Components.size(); i++ )
	{
		auto Comp = Components[ i ];
		char Line[ 128 ];
		if ( serializedVersion >= 5 )
		{
			snprintf( Line, sizeof( Line ), "  - component: {fileID: %lld}\n", Comp->ID );
		}		
		
		Str += Line;
	}
	
	const char* TargetName = Name.c_str();
	if( WritePrefab )
		TargetName = PrefabName.c_str();
	snprintf( Text, sizeof( Text ),
	"  m_Layer: 0\n"
	"  m_Name: %s\n"
	"  m_TagString: Untagged\n"
	"  m_Icon: {fileID: 0}\n"
	"  m_NavMeshLayer: 0\n"
	"  m_StaticEditorFlags: 0\n"
	"  m_IsActive: 1\n",
			   TargetName );

	Str += Text;

	return Str;
}
void Scene::Add( GameObject* GO )
{
	GameObjects.push_back( GO );
	GO->OwnerScene = this;
}
void Scene::WriteScene( const char* File, const char* ProxyScene )
{
	FILE *f = nullptr;
	f = fopen( File, "w" );
	if( !f )
	{
		return;
	}

	std::string SceneProxy;
	std::string Str = SceneProxy;
	
	for( int i = 0; i < GameObjects.size(); i++ )
	{
		GameObject *GO = GameObjects[ i ];
		if( !GO->Parent )
		{
			if( UsePrefabs && GO->GetComponentInChildren( CT_MESHRENDERER ) && GO->PrefabTemplate)
			{
				std::string TextData = GO->GeneratePrefabInstance();
				Str += TextData;
			}
			else
			{
				std::string TextData = GO->ToSceneData();
				Str += TextData;
			}
		}
	}

	unsigned char *ProxySceneData = nullptr;
	int Size = LoadFile( ProxyScene, &ProxySceneData );
	if( !ProxySceneData || Size <= 0 )
	{
		UE_LOG( LogTemp, Error, TEXT( "LoadFile( %s ) failed" ), ProxyScene );
		return;
	}

	std::string FinalSceneData = (char*)ProxySceneData;
	
	FinalSceneData += Str;

	fprintf( f, "%s", FinalSceneData.c_str() );
	fclose( f );
}
void Scene::WritePrefabs( const char* PrefabFolder )
{
	for( int i = 0; i < Prefabs.size(); i++ )
	{
		GameObject* GO = Prefabs[ i ];
		
		const char PrefabHeader[] = {
		"%YAML 1.1\n"
		"%TAG !u! tag:unity3d.com,2011:\n"
		};
		std::string TextData = PrefabHeader;
		TextData += GO->ToSceneData( true );

		std::string File = PrefabFolder;
		std::string NameWide = GO->PrefabName;
		File += NameWide;
		File += ".prefab";

		FILE* f = nullptr;
		f = fopen( File.c_str(), "w" );
		if( f )
		{
			fprintf( f, "%s", TextData.c_str() );
			fclose( f );
		}
		GO->PrefabGUID = GenerateGUIDFromString( File );
		std::string PrefabMetaContent = GeneratePrefabMeta( GO->PrefabGUID );

		std::string MetaFile = PrefabFolder;
		MetaFile += NameWide;
		MetaFile += ".prefab.meta";
		f = fopen( MetaFile.c_str(), "w" );
		if( f )
		{
			fprintf( f, "%s", PrefabMetaContent.c_str() );
			fclose( f );
		}
	}
}
std::string Scene::GetPrefabName( GameObject* GO )
{
	std::vector< UnityComponent* > AllComponents;
	GO->GetAllComponentsInChildren( AllComponents);

	for( int c = 0; c < AllComponents.size(); c++ )
	{
		if( AllComponents[ c ]->Type == CT_MESHFILTER )
		{
			MeshFilter* MF = (MeshFilter*)AllComponents[ c ];
			return MF->Mesh->Name;
		}
	}

	return "";
}
void Scene::FixDuplicatePrefabNames()
{
	if( Prefabs.size() == 0 )
		return;
	for( int i = 0; i < Prefabs.size() - 1; i++ )
	{
		auto PrefabA = Prefabs[ i ];
		int Instances = 1;
		for( int u = i + 1; u < Prefabs.size(); u++ )
		{
			auto PrefabB = Prefabs[ u ];
			if( PrefabA->PrefabName.compare( PrefabB->PrefabName.c_str() ) == 0 )
			{
				char Text[ 32 ];
				snprintf( Text, sizeof( Text ), "_%d", Instances );
				PrefabB->PrefabName += Text;
				Instances++;
			}
		}
	}
}
void Scene::EvaluatePrefab( GameObject* GO )
{
	bool IsPrefabInstance = false;
	for( int i = 0; i < Prefabs.size(); i++ )
	{
		std::vector< UnityComponent* > AllComponentsA;
		std::vector< UnityComponent* > AllComponentsB;
		auto Prefab = Prefabs[ i ];
		Prefab->GetAllComponentsInChildren( AllComponentsA );
		GO->GetAllComponentsInChildren( AllComponentsB );

		if( AllComponentsA.size() != AllComponentsB.size() )
			continue;

		bool Identical = true;
		for( int c = 0; c < AllComponentsA.size(); c++ )
		{
			if( AllComponentsA[ c ]->Type != AllComponentsB[ c ]->Type )
			{
				Identical = false;
				break;
			}
			if( AllComponentsA[ c ]->Type == CT_MESHFILTER )
			{
				MeshFilter* MF_A = (MeshFilter*)AllComponentsA[ c ];
				MeshFilter* MF_B = (MeshFilter*)AllComponentsB[ c ];
				if( MF_A->Mesh != MF_B->Mesh )
				{
					Identical = false;
					break;
				}
			}
			if( AllComponentsA[ c ]->Type == CT_MESHRENDERER )
			{
				MeshRenderer* MR_A = (MeshRenderer*)AllComponentsA[ c ];
				MeshRenderer* MR_B = (MeshRenderer*)AllComponentsB[ c ];
				if( MR_A->Materials.size() != MR_B->Materials.size() )
				{
					Identical = false;
					break;
				}
				for( int m = 0; m < MR_A->Materials.size(); m++ )
				{
					if( MR_A->Materials[m] != MR_B->Materials[m] )
					{
						Identical = false;
						break;
					}
				}
			}
		}

		if( !Identical )
			continue;

		IsPrefabInstance = true;
		GO->PrefabTemplate = Prefab;
	}

	if( !IsPrefabInstance )
	{
		if( GO->CanBePrefab )
		{
			//Give the prefab the name of the mesh
			GO->PrefabName = GetPrefabName( GO );
			GO->PrefabTemplate = GO;
			Prefabs.push_back( GO );
		}
	}
}

std::string GetGUIDFromSizeT( size_t Input )
{
	//char Base[] = "c5890e70e509bae42997d0b1b0696b6a";
	  char Base[] = "abc00000000000000000000000000000";
	//7338d2bd21532424daf2ad06ccf3a92c
	char Num[ 32 ];
	#if PLATFORM_WINDOWS
		uint64 Input64 = Input;
	#else
		unsigned long Input64 = Input;
	#endif
	snprintf( Num, sizeof( Num ), "%zu", Input64 );
	int NumLen = strlen( Num );
	std::string GUID = Base;
	for( int i = 32 - NumLen,u = 0; i < 32; i++, u++ )
	{
		GUID[ i ] = Num[ u ];
	}

	return GUID;
}

enum FileIDType
{
	FIT_MATERIALS,
	FIT_MESHES,
};
class FileID
{
public:
	FileID( int pBaseID, const char* str, int pIndex = 0)
	{
		BaseID = pBaseID;
		//Type = pType;
		Identifier = str;
		Index = pIndex;
	}
	int BaseID;
	//FileIDType Type;
	int Index;
	std::string Identifier;

	static int GetBase( FileIDType Type )
	{
		switch( Type )
		{
			case FIT_MATERIALS: return 2100000;
			case FIT_MESHES	  : return 4300000;
			default:return Type;
		}

		return Type;
	}
	std::string GetMetaLine()
	{
		//int Base = GetBase( Type );
		int ID = BaseID + Index * 2;
		char Line[ 1024 ];
		snprintf( Line, sizeof( Line ), "    %d: %s\n", ID, Identifier.c_str() );

		return Line;
	}
};
void AddLodObjects( int ID, int NumLods, const char* MeshName, std::string & str )
{
	char Text[ 1024 ] = "";
	for( int i = 0; i < NumLods; i++ )
	{
		snprintf( Text, sizeof( Text ), "    %d: %s_LOD%i\n", ID, MeshName, i );
		ID += 2;
		str += Text;
	}
	/*for( int i = 0; i < NumLods; i++ )
	{
		snprintf( Text, "    %d: %s_LOD%i\n", ID, MeshName, i );
		ID += 2;
		str += Text;
	}*/
}
std::string GenerateMaterialIDs( UnityMesh* M )
{
	char Text[ 1024 ] = "";
	std::string MaterialsText;
	for( int i = 0; i < M->Materials.size(); i++ )
	{
		int ID = 2100000 + i * 2;
		snprintf( Text, sizeof( Text ), "    %d: %s\n", ID, M->Materials[ i ]->Name.c_str() );
		MaterialsText += Text;
	}

	return MaterialsText;
}
std::string GenerateMeshMetaFileIDs( UnityMesh* M, bool IsFBX, int TotalLods )
{
	const char* MeshName = M->Name.c_str();
	if( IsFBX )
	{
		//M->Name.replace( "")
		std::string Ret;
		char Text[ 4096 ] = "";
		std::string MaterialsText = GenerateMaterialIDs( M );
		if( TotalLods > 1 )
		{
			snprintf( Text, sizeof( Text ), "    100000: //RootNode\n" );
			Ret += Text;

			AddLodObjects( 100002, TotalLods, MeshName, Ret );
			snprintf( Text, sizeof( Text ), "    400000: //RootNode\n" );
			Ret += Text;
			AddLodObjects( 400002, TotalLods, MeshName, Ret );
			Ret += MaterialsText;
			AddLodObjects( 2300000, TotalLods, MeshName, Ret );
			AddLodObjects( 3300000, TotalLods, MeshName, Ret );
			AddLodObjects( 4300000, TotalLods, MeshName, Ret );
			if( M->HasConvexHulls )
			{
				snprintf( Text, sizeof( Text ), "    %d: %s_ConvexHulls\n", M->ConvexHullMeshID, MeshName );
				Ret += Text;
			}
			snprintf( Text, sizeof( Text ), "    20500000: //RootNode\n" );
			Ret += Text;
		}
		else
		{
			snprintf( Text, sizeof( Text ), 
							 "    100000: //RootNode\n"
							 "    400000: //RootNode\n"
							 "%s"
							 "    2300000: //RootNode\n"
							 "    3300000: //RootNode\n"
							 "    4300000: %s\n"
							 "    4300002: %s_LOD0\n"
					   , MaterialsText.c_str(), MeshName, MeshName
			);
			Ret += Text;
			if( M->HasConvexHulls )
			{
				snprintf( Text, sizeof( Text ), "    %d: %s_ConvexHulls\n", M->ConvexHullMeshID, MeshName );
				Ret += Text;
			}
		}
		return Ret;
	}
	else
	{
		int SubMeshes = M->Sections.size();

		std::string Ret;	
		if( SubMeshes <= 1 )
		{
			char Text[ 4096 ] = "";
			snprintf( Text, sizeof( Text ),
				"    100000: %s\n"
				"    100002: //RootNode\n"
				"    400000: %s\n"
				"    400002: //RootNode\n"
				"    2300000: %s\n"
				"    3300000: %s\n"
				"    4300000: %s\n",
						   MeshName, MeshName, MeshName, MeshName, MeshName  );
			return  Text;
		}
		int TextLength = 1024 + SubMeshes * 128;
		char *Text = new char[ TextLength ];

		snprintf( Text, TextLength,
			"    100000: %s\n"
			"    100002: //RootNode\n",
			MeshName );
		Ret += Text;

		std::vector<FileID> FileIDArray;

		//100000: //RootNode
		//400000 : //RootNode
		//2100000 : MI_BS_Screens
		//2100002 : MI_BS_TV_Screen_01
		//2300000 : //RootNode
		//3300000 : //RootNode
		//4300000 : SM_BS_Screen_01_2

		FileIDArray.push_back( FileID( 100000, "//RootNode", 0 ) );
		FileIDArray.push_back( FileID( 400000, "//RootNode", 0 ) );

		for( int i = 0; i < M->Materials.size(); i++ )
		{
			if ( M->Materials[i] )
				FileIDArray.push_back( FileID( 2100000, M->Materials[i]->Name.c_str(), i ) );
		}

		FileIDArray.push_back( FileID( 2300000, "//RootNode", 0 ) );
		FileIDArray.push_back( FileID( 3300000, "//RootNode", 0 ) );

		FileIDArray.push_back( FileID( 4300000, M->Name.c_str(), 0 ) );
	
		for( int i = 0; i < FileIDArray.size(); i++ )
		{
			std::string Line = FileIDArray[ i ].GetMetaLine();
			Ret += Line;
		}

		return Ret;
	}
}

std::string GenerateMeshMeta( UnityMesh* Mesh, bool IsFBX, int TotalLods )
{
	const char* MeshName = Mesh->Name.c_str();
	int SubMeshes = Mesh->Sections.size();
	
	Mesh->GenerateGUID();

	std::string MetaFileIDs = GenerateMeshMetaFileIDs( Mesh, IsFBX, TotalLods );
	
	int TextLength = 4096 + MetaFileIDs.length();
	char *Text = new char[ TextLength ];
	snprintf( Text, TextLength,
"fileFormatVersion: 2\n"
"guid: %s\n"
"ModelImporter:\n"
"  serializedVersion: 23\n"
"  fileIDToRecycleName:\n"
"%s"//MetaFileIDs
"  externalObjects: {}\n"
"  materials:\n"
"    importMaterials: 1\n"
"    materialName: 1\n"
"    materialSearch: 1\n"
"    materialLocation: 0\n"
"  animations:\n"
"    legacyGenerateAnimations: 4\n"
"    bakeSimulation: 0\n"
"    resampleCurves: 1\n"
"    optimizeGameObjects: 0\n"
"    motionNodeName: \n"
"    rigImportErrors: \n"
"    rigImportWarnings: \n"
"    animationImportErrors: \n"
"    animationImportWarnings: \n"
"    animationRetargetingWarnings: \n"
"    animationDoRetargetingWarnings: 0\n"
"    animationCompression: 1\n"
"    animationRotationError: 0.5\n"
"    animationPositionError: 0.5\n"
"    animationScaleError: 0.5\n"
"    animationWrapMode: 0\n"
"    extraExposedTransformPaths: []\n"
"    clipAnimations: []\n"
"    isReadable: 1\n"
"  meshes:\n"
"    lODScreenPercentages: []\n"
"    globalScale: 1\n"
"    meshCompression: 0\n"
"    addColliders: 0\n"
"    importBlendShapes: 1\n"
"    swapUVChannels: 0\n"
"    generateSecondaryUV: 0\n"
"    useFileUnits: 1\n"
"    optimizeMeshForGPU: 0\n"
"    keepQuads: 0\n"
"    weldVertices: 0\n"
"    preserveHierarchy: 0\n"
"    indexFormat : 2\n"
"    secondaryUVAngleDistortion: 8\n"
"    secondaryUVAreaDistortion: 15.000001\n"
"    secondaryUVHardAngle: 88\n"
"    secondaryUVPackMargin: 4\n"
"    useFileScale: 1\n"
"  tangentSpace:\n"
"    normalSmoothAngle: 60\n"
"    normalImportMode: 0\n"
"    tangentImportMode: 0\n"
"  importAnimation: 1\n"
"  copyAvatar: 0\n"
"  humanDescription:\n"
"    serializedVersion: 2\n"
"    human: []\n"
"    skeleton: []\n"
"    armTwist: 0.5\n"
"    foreArmTwist: 0.5\n"
"    upperLegTwist: 0.5\n"
"    legTwist: 0.5\n"
"    armStretch: 0.05\n"
"    legStretch: 0.05\n"
"    feetSpacing: 0\n"
"    rootMotionBoneName: \n"
"    rootMotionBoneRotation: {x: 0, y: 0, z: 0, w: 1}\n"
"    hasTranslationDoF: 0\n"
"    hasExtraRoot: 0\n"
"    skeletonHasParents: 1\n"
"  lastHumanDescriptionAvatarSource: {instanceID: 0}\n"
"  animationType: 2\n"
"  humanoidOversampling: 1\n"
"  additionalBone: 0\n"
"  userData: \n"
"  assetBundleName: \n"
"  assetBundleVariant:\n"

	, Mesh->GUID.c_str(), MetaFileIDs.c_str() );

	std::string Ret;
	Ret = Text;
	delete[] Text;
	return Ret;
}

extern TAutoConsoleVariable<int> CVarUseStandardMaterial;
extern TAutoConsoleVariable<int> CVarRenderPipeline;
std::string  UnityMaterial::GenerateMaterialFile( )
{
	std::string Ret;
	if( CVarUseStandardMaterial.GetValueOnAnyThread() == 1)
	{
		char Text[ 2048 ] = "";
		const char* StandardShaderGUID = "0000000000000000f000000000000000";
		int StandardShaderFileID = 46;
		const char* URPLitShaderGUID	= "933532a4fcc9baf4fa0491de14d08ed7";
		int URPFileID = 4800000;
		const char* HDRPLitShaderGUID	= "6e4ae4064600d784cac1e41a9e6f2e59";
		int HDRPFileID = 4800000;
		const char* UsedShaderGUID = StandardShaderGUID;
		int UsedFileID = StandardShaderFileID;
		int type = 0;
		if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_URP )
		{
			UsedShaderGUID = URPLitShaderGUID;
			UsedFileID = URPFileID;
			type = 3;
		}
		if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
		{
			UsedShaderGUID = HDRPLitShaderGUID;
			UsedFileID = HDRPFileID;
			type = 3;
		}
		snprintf( Text, sizeof( Text ),
				   "%%YAML 1.1\n"
				   "%%TAG !u! tag:unity3d.com,2011:\n"
				   "--- !u!114 &-6605459782340829755\n"
				   "MonoBehaviour:\n"
				   "  m_ObjectHideFlags: 11\n"
				   "  m_CorrespondingSourceObject: {fileID: 0}\n"
				   "  m_PrefabInstance: {fileID: 0}\n"
				   "  m_PrefabAsset: {fileID: 0}\n"
				   "  m_GameObject: {fileID: 0}\n"
				   "  m_Enabled: 1\n"
				   "  m_EditorHideFlags: 0\n"
				   "  m_Script: {fileID: 11500000, guid: d0353a89b1f911e48b9e16bdc9f2e058, type: 3}\n"
				   "  m_Name: \n"
				   "  m_EditorClassIdentifier: \n"
				   "  version: 5\n"
				   "--- !u!21 &2100000\n"
				   "Material:\n"
				   "  serializedVersion: 8\n"
				   "  m_ObjectHideFlags: 0\n"
				   "  m_CorrespondingSourceObject: {fileID: 0}\n"
				   "  m_PrefabInstance: {fileID: 0}\n"
				   "  m_PrefabAsset: {fileID: 0}\n"
				   "  m_Name: %s\n"
				   "  m_Shader: {fileID: %d, guid: %s, type: %d}\n"
				   "  m_ValidKeywords:\n"
				   "  - _NORMALMAP\n"
				   "  m_InvalidKeywords: []\n"
				   "  m_LightmapFlags: 4\n"
				   "  m_EnableInstancingVariants: 1\n"
				   "  m_DoubleSidedGI: 0\n"
				   "  m_CustomRenderQueue: -1\n"
				   "  stringTagMap:\n"
				   "    RenderType: Opaque\n"
				   "  disabledShaderPasses: []\n"
				   "  m_SavedProperties:\n"
				   "    serializedVersion: 3\n", Name.c_str(), UsedFileID, UsedShaderGUID, type );
		Ret += Text;

		if( Textures.size() > 0 )
		{
			Ret += "    m_TexEnvs:\n";
			const int MaxSemantics = 11;
			const char* SemanticNames[ MaxSemantics ] = {
				"_BaseMap",
				"_BumpMap",
				"_DetailAlbedoMap",
				"_DetailMask",
				"_DetailNormalMap",
				"_EmissionMap",
				"_MainTex",
				"_MetallicGlossMap",
				"_OcclusionMap",
				"_ParallaxMap",
				"_SpecGlossMap"
			};
			if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_BUILTIN )
			{
				SemanticNames[ 0 ] = "_MainTex";
				SemanticNames[ 6 ] = nullptr;
			}
			if( CVarRenderPipeline.GetValueOnAnyThread() == (int)RenderPipeline::RP_HDRP )
			{
				SemanticNames[ 0 ] = "_BaseColorMap";
				SemanticNames[ 1 ] = "_NormalMap";
				SemanticNames[ 2 ] = "_DetailMap";
				SemanticNames[ 5 ] = "_EmissiveColorMap";
				//_MaskMap?
			}
			
			for( int i = 0; i < MaxSemantics; i++ )
			{
				if( !SemanticNames[ i ] )
					continue;
				UnityTexture* Tex = GetTexture( (TextureSemantic)(i + 1) );
				std::string TexGUID = NullGUID;
				if( Tex )
					TexGUID = Tex->GUID;

				snprintf( Text, sizeof( Text ), "    - %s:\n", SemanticNames [i] );
				Ret += Text;

				if( Tex )
				{
					snprintf( Text, sizeof( Text ), "        m_Texture: {fileID: 2800000, guid: %s, type: 3}\n",
							   TexGUID.c_str() );
				}
				else
					snprintf( Text, sizeof( Text ), "        m_Texture: {fileID: 0}\n" );

				Ret += Text;
				snprintf( Text, sizeof( Text ),
						   "        m_Scale: {x: 1, y: 1}\n"
						   "        m_Offset: {x: 0, y: 0}\n" );

				Ret += Text;
			}
		}

		snprintf( Text, sizeof( Text ),
				   "    m_Ints: []\n"
				   "    m_Floats:\n"
				   "    - _DstBlend: 0\n"
				   "    - _SrcBlend: 1\n"
				   "    - _UVSec: 0\n"
				   "    - _ZWrite: 1\n"
				   "    m_Colors:\n"
				   "    - _BaseColor: {r: 1, g: 1, b: 1, a: 1}\n"
				   "    - _Color: {r: 1, g: 1, b: 1, a: 1}\n"
				   "    - _EmissionColor: {r: 0, g: 0, b: 0, a: 1}\n"
				   "    - _SpecColor: {r: 0.5, g : 0.5, b : 0.5, a : 0.5}\n"
				   "  m_BuildTextureStacks: []\n"
		);

		Ret += Text;
	}
	else
	{
		int serializedVersion = 3;
		char Text[ 2048 ] = "";
		snprintf( Text, sizeof( Text ),
				   "%%YAML 1.1\n"
				   "%%TAG !u! tag:unity3d.com,2011:\n"
				   "--- !u!21 &2100000\n"
				   "Material:\n"
				   "  serializedVersion: 6\n"
				   "  m_ObjectHideFlags: 0\n"
				   "  m_CorrespondingSourceObject: {fileID: 0}\n"
				   "  m_PrefabInstance: {fileID: 0}\n"
				   "  m_PrefabAsset: {fileID: 0}\n"
				   "  m_Name: %s\n"
				   "  m_Shader: {fileID: 4800000, guid: %s, type: 3}\n"
				   "  m_ShaderKeywords: \n"
				   "  m_LightmapFlags: 4\n"
				   "  m_EnableInstancingVariants: 1\n"
				   "  m_DoubleSidedGI: 0\n"
				   "  m_CustomRenderQueue: -1\n"
				   "  stringTagMap: {}\n"
				   "  disabledShaderPasses: []\n"
				   "  m_SavedProperties:\n"
				   "    serializedVersion: %d\n", Name.c_str(), ShaderGUID.c_str(), serializedVersion );
		Ret += Text;

		if( Textures.size() > 0 )
		{
			Ret += "    m_TexEnvs:\n";
		}
		for( int i = 0; i < Textures.size(); i++ )
		{
			auto Tex = Textures[ i ];
			std::string TexGUID = NullGUID;
			if( Tex )
				TexGUID = Tex->GUID;

			if( serializedVersion == 3 )//Unity 2017.1
			{
				const char* Tex2D = "Material_Texture2D_";
				int Tex2DFileID = 2800000;
				const char* TexCube = "Material_TextureCube_";
				int TexCubeFileID = 8900000;
				const char* Tex3D = "Material_VolumeTexture_";
				int Tex3DFileID = 11700000;
				const char* Prefix = Tex2D;
				int fileID = Tex2DFileID;
				int TexIndex = i;
				if( Tex->Shape == UTS_Cube )
				{
					Prefix = TexCube;
					fileID = TexCubeFileID;
					TexIndex = Tex->SpecificIndex;
				}
				if( Tex->Shape == UTS_3D )
				{
					Prefix = Tex3D;
					fileID = Tex3DFileID;
					TexIndex = Tex->SpecificIndex;
				}
				snprintf( Text, sizeof( Text ),
						   "    - %s%d:\n"
						   "        m_Texture: {fileID: %d, guid: %s, type: 3}\n"
						   "        m_Scale: {x: 1, y: 1}\n"
						   "        m_Offset: {x: 0, y: 0}\n",
						   Prefix,
						   TexIndex,
						   fileID,
						   TexGUID.c_str() );
			}

			Ret += Text;
		}

		snprintf( Text, sizeof( Text ),
				   "    m_Floats:\n"
				   "    - _DstBlend: 0\n"
				   "    - _SrcBlend: 1\n"
				   "    - _UVSec: 0\n"
				   "    - _ZWrite: 1\n"
				   "    m_Colors:\n"
				   "    - _Color: {r: 0.8, g: 0.8, b: 0.8, a: 1}\n"
				   "    - _EmissionColor: {r: 0, g: 0, b: 0, a: 1}\n" );

		Ret += Text;
	}
	
		return Ret;
}
UnityTexture* UnityMaterial::GetTexture( TextureSemantic Semantic )
{
	for( int i = 0; i < Textures.size(); i++ )
	{
		if( Textures[ i ]->Semantic == Semantic )
		{
			return Textures[ i ];
		}
	}

	return nullptr;
}
std::string  GenerateMaterialMeta( std::string GUID )
{

	char Text[ 2048 ] = "";
	snprintf( Text, sizeof( Text ),
"fileFormatVersion: 2\n"
"guid: %s\n"
"NativeFormatImporter:\n"
"  mainObjectFileID: 2100000\n"
"  userData: \n"
"  assetBundleName: \n"
"  assetBundleVariant: \n",
	GUID.c_str() );

	return Text;
}

std::string  GenerateShaderMeta( std::string GUID )
{
	char Text[ 2048 ] = "";
	snprintf( Text, sizeof( Text ),
"fileFormatVersion: 2\n"
"guid: %s\n"
"ShaderImporter:\n"
"  defaultTextures: []\n"
"  userData: \n"
"  assetBundleName: \n"
"  assetBundleVariant: \n",
GUID.c_str() );

	return Text;
}

std::string  GenerateTextureMeta( std::string GUID, bool IsNormalMap, int sRGB, UnityTextureShape Shape, int TileX, int TileY )
{
	char Text[ 2048 ] = "";

	//int sRGB = 1;
	int texturetype = 0;
	
	if( IsNormalMap )
	{
		//sRGB = 0;
		texturetype = 1;
	}

	int textureShape = 1;
	if( Shape == UTS_Cube )
		textureShape = 2;
	else if( Shape == UTS_2DArray )
		textureShape = 3;//??
	else if ( Shape == UTS_3D )
		textureShape = 8;

	int serializedVersion = 4;
	if ( serializedVersion == 4 )
	{
	snprintf( Text, sizeof( Text ),
	"fileFormatVersion: 2\n"
	"guid: %s\n"	
	"TextureImporter:\n"
	"  fileIDToRecycleName: {}\n"
	"  serializedVersion: 4\n"
	"  mipmaps:\n"
	"    mipMapMode: 0\n"
	"    enableMipMap: 1\n"
	"    sRGBTexture: %d\n"
	"    linearTexture: 0\n"
	"    fadeOut: 0\n"
	"    borderMipMap: 0\n"
	"    mipMapsPreserveCoverage: 0\n"
	"    alphaTestReferenceValue: 0.5\n"
	"    mipMapFadeDistanceStart: 1\n"
	"    mipMapFadeDistanceEnd: 3\n"
	"  bumpmap:\n"
	"    convertToNormalMap: 0\n"
	"    externalNormalMap: 0\n"
	"    heightScale: 0.25\n"
	"    normalMapFilter: 0\n"
	"  isReadable: 0\n"
	"  grayScaleToAlpha: 0\n"
	"  generateCubemap: 6\n"
	"  cubemapConvolution: 0\n"
	"  seamlessCubemap: 0\n"
	"  textureFormat: 1\n"
	"  maxTextureSize: 8192\n"
	"  textureSettings:\n"
	"    serializedVersion: 2\n"
	"    filterMode: -1\n"
	"    aniso: -1\n"
	"    mipBias: -1\n"
	"    wrapU: -1\n"
	"    wrapV: -1\n"
	"    wrapW: -1\n"
	"  nPOTScale: 1\n"
	"  lightmap: 0\n"
	"  compressionQuality: 50\n"
	"  spriteMode: 0\n"
	"  spriteExtrude: 1\n"
	"  spriteMeshType: 1\n"
	"  alignment: 0\n"
	"  spritePivot: {x: 0.5, y: 0.5}\n"
	"  spriteBorder: {x: 0, y: 0, z: 0, w: 0}\n"
	"  spritePixelsToUnits: 100\n"
	"  alphaUsage: 1\n"
	"  alphaIsTransparency: 0\n"
	"  spriteTessellationDetail: -1\n"
	"  textureType: %d\n"
	"  textureShape: %d\n"
	"  singleChannelComponent: 0\n"
	"  flipbookRows : %d\n"
	"  flipbookColumns : %d\n"
	"  maxTextureSizeSet: 0\n"
	"  compressionQualitySet: 0\n"
	"  textureFormatSet: 0\n"
	"  platformSettings:\n"
	"  - buildTarget: DefaultTexturePlatform\n"
	"    maxTextureSize: 8192\n"
	"    textureFormat: -1\n"
	"    textureCompression: 1\n"
	"    compressionQuality: 50\n"
	"    crunchedCompression: 0\n"
	"    allowsAlphaSplitting: 0\n"
	"    overridden: 0\n"
	"  spriteSheet:\n"
	"    serializedVersion: 2\n"
	"    sprites: []\n"
	"    outline: []\n"
	"    physicsShape: []\n"
	"  spritePackingTag: \n"
	"  userData: \n"
	"  assetBundleName: \n"
	"  assetBundleVariant: \n",
				   GUID.c_str(), sRGB, texturetype, textureShape, TileX, TileY );
	}
	return Text;
}
std::string  GenerateExportedFBXMeta( )
{	
	char Text[ 2048 ] = "";
	std::string GUID = GetGUIDFromSizeT(1);
	snprintf( Text, sizeof( Text ),
				"fileFormatVersion: 2\n"
				"guid: %s\n"				
				"ModelImporter:\n"
				"  serializedVersion: 21\n"
				"  materials:\n"
				"    importMaterials: 1\n"
				"    materialName: 1\n"
				"    materialSearch: 1\n", GUID.c_str() );

	return Text;
}
std::string  GeneratePrefabMeta( std::string GUID )
{
	char Text[ 2048 ] = "";
	snprintf( Text, sizeof( Text ),
			   "fileFormatVersion: 2\n"
			   "guid: %s\n"
			   "PrefabImporter:\n"
			   "  externalObjects: {}\n"
			   "  userData:\n"
			   "  assetBundleName: \n"
			   "  assetBundleVariant: \n",
			   GUID.c_str()
	);
	return Text;	
}
#endif
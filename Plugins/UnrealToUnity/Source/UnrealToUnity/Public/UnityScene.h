#pragma once

#include <string>
#include <vector>

enum ComponentType
{
	CT_UNKNOWN = 0,
	CT_TRANSFORM,
	CT_MESHRENDERER,
	CT_SKINNEDMESHRENDERER,
	CT_MESHFILTER,	
	CT_LIGHT,
	CT_LODGROUP,
	CT_MESHCOLLIDER,
	CT_BOXCOLLIDER,
	CT_SPHERECOLLIDER,
	CT_CAPSULECOLLIDER,
	CT_REFLECTIONPROBE,
	CT_DECAL
};
class UnityComponent
{
public :
	UnityComponent();
	virtual ~UnityComponent() {};
	uint64 ID;
	ComponentType Type;
	std::string Flag;
	bool Enabled;
	std::string Name;
	class GameObject *Owner = nullptr;

	static uint64 Counter;
	virtual std::string GetSceneData();
	static uint64 GetUniqueID();
};

class TransformComponent : public UnityComponent
{
public:
	TransformComponent();
	void Reset();
	void Set( FVector Position, FQuat Quat, FVector Scale );

	float LocalPosition[ 3 ];
	float LocalRotation[ 4 ];//Quat ?
	float LocalScale[ 3 ];

	std::string GetChildrenText();
	virtual std::string GetSceneData();
};
class Renderer : public UnityComponent
{
public:
	void AddMaterial( UnityMaterial* NewMat, bool HasDuplicateMaterials );
	std::vector<class UnityMaterial*> Materials;
	bool CastShadows = true;
};
class MeshRenderer : public Renderer
{
public:
	MeshRenderer()
	{
		Type = CT_MESHRENDERER;
	}
	
	virtual std::string GetSceneData();
};

enum TextureSemantic
{
	TS_Unknown,
	TS_BaseMap,
	TS_BumpMap,
	TS_DetailAlbedoMap,
	TS_DetailMask,	
	TS_DetailNormalMap,	
	TS_EmissionMap,	
	TS_MainTex,
	TS_MetallicGlossMap,
	TS_OcclusionMap,
	TS_ParallaxMap,
	TS_SpecGlossMap,

	TS_MaxSemantics
};
enum UnityTextureShape
{
	UTS_2D,
	UTS_Cube,
	UTS_2DArray,
	UTS_3D,
};
class UnityTexture
{
public:
	void GenerateGUID();

	std::string File;
	std::string GUID;
	UnityTextureShape Shape = UTS_2D;
	int SpecificIndex = -1;
	TextureSemantic Semantic = TS_Unknown;
};

class UnityMaterial
{
public:
	void GenerateGUID();
	void GenerateShaderGUID();

	std::string Name;
	std::string File;
	std::string GUID;
	std::string ShaderGUID;
	std::string ShaderFileName;
	std::string ShaderContents;	

	std::vector< UnityTexture* > Textures;	
	std::string GenerateMaterialFile();
	UnityTexture* GetTexture( TextureSemantic Semantic );
};

struct MeshSection
{
	//uint32 *Indices = nullptr;
	uint32 IndexOffset = 0;
	uint32 NumIndices = 0;
	uint32 MaterialIndex = 0;
};

class UnityMesh
{
public:
	void GenerateGUID();

	std::string Name;
	std::string File;
	std::string GUID;

	FVector*		Vertices = nullptr;
	FVector*		Normals = nullptr;
	FVector4*	Tangents = nullptr;
	FVector2D*	Texcoords = nullptr;
	FVector2D*	Texcoords1 = nullptr;
	FVector2D*	Texcoords2 = nullptr;
	FVector2D*	Texcoords3 = nullptr;
	FColor*		Colors = nullptr;
	uint32* AllIndices = nullptr;
	std::vector<MeshSection*> Sections;

	int NumVertices;
	int NumIndices;

	std::vector<UnityMaterial*> Materials;
	bool HasConvexHulls = false;
	int ConvexHullMeshID = 0;
};

class MeshFilter : public UnityComponent
{
public:
	MeshFilter()
	{
		Type = CT_MESHFILTER;
	}
	UnityMesh *Mesh = nullptr;
	
	uint64_t SubMeshID = 4300000;
	virtual std::string GetSceneData();
};
class SkinnedMeshRenderer : public Renderer
{
public:
	SkinnedMeshRenderer()
	{
		Type = CT_SKINNEDMESHRENDERER;
	}

	virtual std::string GetSceneData();
	UnityMesh* Mesh = nullptr;

	uint64_t SubMeshID = 4300000;
};

class LOD
{
public:
	float screenRelativeTransitionHeight = 0;
	float fadeTransitionWidth = 0;

	std::vector<Renderer*> renderers;
};
class LodGroup : public UnityComponent
{
public:
	LodGroup()
	{
		Type = CT_LODGROUP;
		LocalReferencePoint = FVector( 0, 0, 0 );
	}
	
	virtual std::string GetSceneData();
	std::string GetLODsString();

	std::vector<LOD*> Lods;
	float Size = 1.0f;
	FVector LocalReferencePoint;
};
class MeshCollider : public UnityComponent
{
public:
	MeshCollider()
	{
		Type = CT_MESHCOLLIDER;
	}

	UnityMesh* Mesh = nullptr;
	bool IsConvex = false;
	uint64_t SubMeshID = 0;
	virtual std::string GetSceneData();
};
class BoxCollider : public UnityComponent
{
public:
	BoxCollider()
	{
		Type = CT_BOXCOLLIDER;
	}

	FVector Center;
	FVector Size;

	virtual std::string GetSceneData();
};
class SphereCollider : public UnityComponent
{
public:
	SphereCollider()
	{
		Type = CT_SPHERECOLLIDER;
	}

	FVector Center;
	float Radius;

	virtual std::string GetSceneData();
};
class CapsuleCollider : public UnityComponent
{
public:
	CapsuleCollider()
	{
		Type = CT_CAPSULECOLLIDER;
	}

	FVector Center;
	float Radius;
	float Height;
	int Direction = 1;

	virtual std::string GetSceneData();
};

enum UnityLightType
{
	LT_SPOT,
	LT_DIRECTIONAL,
	LT_POINT,
	LT_AREA
};

class LightComponent : public UnityComponent
{
public:
	LightComponent()
	{
		UnityComponent::Type = CT_LIGHT;
	}
	std::string GetSceneData();
	
	UnityLightType Type;
	float Range;
	FColor Color;
	float Intensity;
	float SpotAngle = 30.0f;
	bool Shadows = true;
	float Width = 1;
	float Height = 1;
};

enum UnityReflectionProbeType
{
	RPB_REALTIME,
	RPB_CUSTOM,
};

class ReflectionProbeComponent : public UnityComponent
{
public:
	ReflectionProbeComponent()
	{
		UnityComponent::Type = CT_REFLECTIONPROBE;
	}
	std::string GetSceneData();
	UnityReflectionProbeType Type = RPB_REALTIME;
	FVector Size;
	float Intensity = 1.0f;
};
class DecalComponent : public UnityComponent
{
public:
	DecalComponent()
	{
		UnityComponent::Type = CT_DECAL;
	}
	std::string GetSceneData();
	UnityMaterial* Material = nullptr;
	FVector Size;
};
class Scene;
class GameObject
{
public:
	GameObject( );

	UnityComponent* GetComponent( ComponentType Type );
	UnityComponent* GetComponentInChildren( ComponentType Type );
	void		GetAllComponentsInChildren( std::vector< UnityComponent* > & AllComponents );
	UnityComponent* AddComponent( ComponentType Type );
	void AddChild( GameObject* GO );

	std::string GetGameObjectString( bool WritePrefab = false );
	std::string ToSceneData( bool WritePrefab = false );
	std::string GeneratePrefabInstance();

	std::vector< UnityComponent* > Components;
	std::vector< GameObject* > Children;
	GameObject *Parent = nullptr;
	std::string Name;
	uint64 ID;
	Scene* OwnerScene = nullptr;

	bool CanBePrefab = false;
	std::string PrefabGUID;
	std::string PrefabName;
	GameObject* PrefabTemplate = nullptr;
};

class Scene
{
public:
	std::vector< GameObject* > GameObjects;
	std::vector< UnityMaterial* > Materials;
	std::vector< GameObject* > Prefabs;

	void Add( GameObject* GO );

	void WriteScene( const char* File, const char* ProxyScene );
	void WritePrefabs( const char* PrefabFolder );
	void EvaluatePrefab( GameObject* GO );
	void FixDuplicatePrefabNames();
	std::string GetPrefabName( GameObject* GO );
	bool UsePrefabs = true;
};

std::string GenerateMeshMeta( UnityMesh* M, bool IsFBX, int TotalLods );
int64_t LoadFile( const char *File, uint8** data );
std::string ToANSIString( std::wstring w );
std::string GenerateMaterialMeta( std::string GUID );
std::string GenerateShaderMeta( std::string GUID );
std::string GenerateTextureMeta( std::string GUID, bool IsNormalMap, int sRGB, UnityTextureShape Shape, int TileX, int TileY );
std::string GetGUIDFromSizeT( size_t Input );
std::string GenerateExportedFBXMeta();
std::string  GeneratePrefabMeta( std::string GUID );
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"

void DoExportScene( UWorld *World, FString ExportFolder );
int64_t LoadFile( const char *File, uint8** data );
bool SaveFile( const char *File, void *data, int64_t size );
bool VerifyOrCreateDirectory( const wchar_t * TestDir );
bool VerifyOrCreateDirectory( const FString& TestDir );
void ExportFBXForActor( AActor* Actor );

struct MaterialBinding
{
	MaterialBinding();
	FString GenerateName();

	class UMaterialInterface *MaterialInterface = nullptr;
	class UnityMaterial *UnityMat = nullptr;
	FString ShaderSource;

	//IntermediaryData
	UMaterial*			BaseMaterial = nullptr;
	UMaterialInstance*	MaterialInstance = nullptr;
	FMaterialResource*	MaterialResource = nullptr;
	FMaterialRenderProxy*	RenderProxy = nullptr;
	int ID = 0;
	bool IsInstance = false;
	TArray<FString> TextureParamNames;
};

#if ENGINE_MINOR_VERSION >= 26

struct FPreshaderDataContext
{
	explicit FPreshaderDataContext( const FMaterialPreshaderData& InData )
		: Ptr( InData.Data.GetData() )
		, EndPtr( Ptr + InData.Data.Num() )
		, Names( InData.Names.GetData() )
		, NumNames( InData.Names.Num() )
	{
	}

	explicit FPreshaderDataContext( const FPreshaderDataContext& InContext, const FMaterialUniformPreshaderHeader& InHeader )
		: Ptr( InContext.Ptr + InHeader.OpcodeOffset )
		, EndPtr( Ptr + InHeader.OpcodeSize )
		, Names( InContext.Names )
		, NumNames( InContext.NumNames )
	{
	}

	const uint8* RESTRICT Ptr;
	const uint8* RESTRICT EndPtr;
	const FScriptName* RESTRICT Names;
	int32 NumNames;
};

#endif
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <string>

#include "Module.h"

#include "ExportActor.generated.h"

UCLASS()
class AExportActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AExportActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	

	UPROPERTY( EditAnywhere )
	RenderPipeline Pipeline = RenderPipeline::RP_BUILTIN;

	UPROPERTY( EditAnywhere )
	FString OutputFolder = TEXT( "C:\\UnrealToUnity\\" );

	UFUNCTION( BlueprintCallable )
	void EventExport();

	static void DoExport( FString StrOutputFolder );
	static void RotateActorsForExport( bool Forward );
	static void DetachAllActors();

	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

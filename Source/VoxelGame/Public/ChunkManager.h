// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <Dynamic-Grids/DynamicGrid.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ChunkManager.generated.h"

class AMainCharacter;

class VOXELGAME_API Chunk
{
public:
	typedef TSharedPtr<Chunk> ptr;

	static constexpr int CX = 16;
	static constexpr int CY = 16;
	static constexpr int CZ = 16;

	TMap<serenity::Direction, Chunk::ptr> neighbours;

	struct BlockInfo
	{
		unsigned int id = 0;
		//TODO: meta data
	};

	FIntVector pos;

	int numElements = 0;
	BlockInfo blocks[CX * CY * CZ];

	UProceduralMeshComponent* Mesh = nullptr;

public:
	Chunk(int x, int y, int z);
	Chunk(FIntVector&& position);
	virtual ~Chunk();

	const BlockInfo* GetBlock(int lx, int ly, int lz);
	const BlockInfo* GetBlock(FIntVector&& position);
	bool SetBlock(int lx, int ly, int lz, BlockInfo newBlock);
	bool SetBlock(FIntVector&& position, BlockInfo newBlock);

	static int GetIndex(int x, int y, int z);
	static int GetIndex(FIntVector&& position);

private:
	BlockInfo* FindBlock(int lx, int ly, int lz);
	BlockInfo* FindBlock(FIntVector&& position);
};

class VOXELGAME_API Pillar
{
public:
	typedef TSharedPtr<Pillar> ptr;
	static constexpr int HEIGHT = 16;

	FIntPoint pos;

	TArray<Chunk::ptr> chunks;

public:
	Pillar(int x, int y);
	Pillar(FIntPoint position);
	~Pillar();

	// Pillar local coords x:[0;CX], y:[0;CY], z:[0;CZ * HEIGHT]
	bool SetBlock(int lx, int ly, int lz, Chunk::BlockInfo block);
	bool SetBlock(FIntVector&& position, Chunk::BlockInfo block);
};

UCLASS()
class VOXELGAME_API AChunkManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AChunkManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Voxel Params")
	UMaterial* BlockMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Voxel Params")
	TMap<FString, UStaticMesh*> BlockSides;

	UPROPERTY(EditAnywhere, Category = "Voxel Params")
	TMap<FString, UStaticMesh*> FoliageSides;

	AMainCharacter* PlayerCharacter = nullptr;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	serenity::Grid::ptr Grid = nullptr;
	void HandleDelivered(serenity::Delivered& d);
private:

	struct VertexData {
		TArray<FVector2D> uvs;
		TArray<FVector2D> meta; // for texture array

		TArray<int32> triangles;

		TArray<FVector> vertices;
		TArray<FVector> normals;

		void Clear();
	}Buffer;

	TMap<serenity::Direction, VertexData> BlockVertexData;
	TMap<serenity::Direction, VertexData> FoliageVertexData;

	
	serenity::GridManager::ptr GridManager = nullptr;

	void GeneratePillar(serenity::Cell::ptr cell);
	void UpdatePillarGeometry(serenity::Cell::ptr cell);
	void UpdatePillarNeighbours(serenity::Cell::ptr cell);

	void UpdateChunkGeometry(Chunk::ptr chunk);

	bool LoadLandscapeResources();
	VertexData GetVertexDataFromMesh(UStaticMesh* Mesh);
};
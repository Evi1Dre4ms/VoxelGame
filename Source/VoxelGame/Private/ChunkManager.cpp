// Fill out your copyright notice in the Description page of Project Settings.
#pragma optimize("", off)

#include "ChunkManager.h"
#include <DynamicGrid/DynamicGrid.cpp>
#include <FastNoiseLite.h>
#include <Runtime/Engine/Classes/Kismet/GameplayStatics.h>
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"

#include "MainCharacter.h"

// Sets default values
AChunkManager::AChunkManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	BlockSides.Emplace("FRONT", nullptr);
	BlockSides.Emplace("BACK", nullptr);
	BlockSides.Emplace("RIGHT", nullptr);
	BlockSides.Emplace("LEFT", nullptr);
	BlockSides.Emplace("TOP", nullptr);
	BlockSides.Emplace("BOTTOM", nullptr);
	
	FoliageSides.Emplace("FRONT", nullptr);
	FoliageSides.Emplace("BACK", nullptr);
}

// Called when the game starts or when spawned
void AChunkManager::BeginPlay()
{
	Super::BeginPlay();


	// Always on zero coords
	this->ApplyWorldOffset(FVector(0.0, 0.0, 0.0), false);

	if (LoadLandscapeResources())
	{
		PlayerCharacter = (AMainCharacter*)UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
		if (nullptr == PlayerCharacter)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot find player character."));
			return;
		}

		auto pos = PlayerCharacter->GetActorLocation() / 100.0 / Chunk::CX;
		auto startPoint = FIntPoint(FMath::Floor(pos.X), FMath::Floor(pos.Y));

		// Create GridManager
		GridManager = MakeShareable(new serenity::GridManager());
		Grid = GridManager->CreateGrid();

		serenity::Delivered d = Grid->Init(startPoint, 6);
		HandleDelivered(d);
	}
}

// Called every frame
void AChunkManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PlayerCharacter != nullptr)
	{
		auto pos = PlayerCharacter->GetActorLocation() / 100.0 / Chunk::CX;
		auto newPoint = FIntPoint(FMath::Floor(pos.X), FMath::Floor(pos.Y));

		auto gridPos = Grid->GetRoot()->GetIndex();

		if (gridPos != newPoint)
		{
			serenity::Delivered d = Grid->MoveTo(newPoint);
			HandleDelivered(d);
		}
	}
	
}

void AChunkManager::HandleDelivered(serenity::Delivered& d)
{
	namespace st = serenity;

	// Create and generate pillars
	for (auto cc : d.created)
	{
		if (!st::IsValid(cc)) continue;

		// Create pillar
		FIntPoint position = cc->GetIndex();
		Pillar* p = new Pillar(position);
		cc->GetData() = p;

		GeneratePillar(cc);
	}
	
	for (auto cc : d.created)
		UpdatePillarNeighbours(cc);

	// Handle new cells and create procedutal mesh for every chunk
	for (auto cc : d.created)
	{
		if (!st::IsValid(cc)) continue;

		if (Pillar* p = static_cast<Pillar*>(cc->GetData()))
		{
			for (int z = 0; z < Pillar::HEIGHT; z++)
			{
				auto& ch = p->chunks[z];
				FIntVector index = FIntVector(p->pos.X, p->pos.Y, z);

				// Update procedural mesh
				if (!ch->Mesh) {
					ch->Mesh = NewObject<UProceduralMeshComponent>(GetRootComponent(), UProceduralMeshComponent::StaticClass(), FName("Chunk_" + FString::FromInt(index.X) + "_" + FString::FromInt(index.Y) + "_" + FString::FromInt(index.Z)));
					if (!ch->Mesh)
					{
						UE_LOG(LogTemp, Warning, TEXT("Unable to create procedural mesh."));
						return;
					}

					ch->Mesh->bUseAsyncCooking = true;
					ch->Mesh->SetCastShadow(true);

					ch->Mesh->SetMaterial(0, BlockMaterial ? BlockMaterial : nullptr);

					ch->Mesh->SetupAttachment(GetRootComponent());

					ch->Mesh->SetWorldLocation((FVector)ch->pos * FVector(Chunk::CX, Chunk::CY, Chunk::CZ) * 100.0);
				}
			}

			UpdatePillarGeometry(cc);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Pillar was broken."));
			return;
		}
	}

	// Delete chunks, free memory
	for (auto data : d.deleted)
	{
		if (data == nullptr) continue;

		if (Pillar* p = static_cast<Pillar*>(data))
		{
			// Delete all procedural meshes
			for (int z = 0; z < Pillar::HEIGHT; z++)
			{
				auto Mesh = p->chunks[z]->Mesh;
				if (Mesh)
					Mesh->DestroyComponent();
			}
		}
	}
}

void AChunkManager::GeneratePillar(serenity::Cell::ptr cell)
{
	if (!serenity::IsValid(cell))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cell was broken."));
		return;
	}

	Pillar* p = static_cast<Pillar*>(cell->GetData());
	if (nullptr == p)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pillar was broken."));
		return;
	}

	FastNoiseLite noise;
	int seed = 1337;
	noise.SetSeed(seed);

	noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	noise.SetFrequency(25.0f);

	for (int x = 0; x < Chunk::CX; x++) {
		int cx = x + p->pos.X * Chunk::CX;

		for (int y = 0; y < Chunk::CY; y++) {
			int cy = y + p->pos.Y * Chunk::CY;

			for (int z = 0; z < Chunk::CZ * Pillar::HEIGHT; z++) {
				
				float n = noise.GetNoise(cx / 1024.f, cy / 1024.f, z / 1024.f) * 0.7f + 10.0f;

				int h = (int)(0.8 * n);
				
				if (z > h)
					break;

				if (z >= h - 1) {
					p->SetBlock(x, y, z, Chunk::BlockInfo{ 2 });
					p->SetBlock(x, y, z + 1, Chunk::BlockInfo{ 2 });
					p->SetBlock(x, y, z + 2, Chunk::BlockInfo{ 3 });
				}

				p->SetBlock(x, y, z, Chunk::BlockInfo{ 1 });
			}
		}
	}
}

void AChunkManager::UpdatePillarNeighbours(serenity::Cell::ptr cell)
{
	if (!serenity::IsValid(cell))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cell was broken."));
		return;
	}

	Pillar* p = static_cast<Pillar*>(cell->GetData());
	if (nullptr == p)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pillar was broken."));
		return;
	}

	TMap<Direction, const Pillar*> neighbourPillars = {
		{ Direction::LEFT,  cell->GetN(Direction::LEFT).IsValid()  ? static_cast<Pillar*>(cell->GetN(Direction::LEFT)->GetData())  : nullptr },
		{ Direction::BACK,  cell->GetN(Direction::BACK).IsValid()  ? static_cast<Pillar*>(cell->GetN(Direction::BACK)->GetData())  : nullptr },
		{ Direction::RIGHT, cell->GetN(Direction::RIGHT).IsValid() ? static_cast<Pillar*>(cell->GetN(Direction::RIGHT)->GetData()) : nullptr },
		{ Direction::FRONT, cell->GetN(Direction::FRONT).IsValid() ? static_cast<Pillar*>(cell->GetN(Direction::FRONT)->GetData()) : nullptr }
	};

	// Set neighbour chunks by Z axis
	for (int z = 0; z < Pillar::HEIGHT; z++)
	{
		if (z > 0)
			p->chunks[z]->neighbours[Direction::BOTTOM] = p->chunks[z - 1];
		
		if (z < Pillar::HEIGHT - 1)
			p->chunks[z]->neighbours[Direction::TOP]	= p->chunks[z + 1];
	}

	// Cycle: set neigbour chunks from another pillars
	for (const auto& [direction, neighbourPillar] : neighbourPillars)
	{
		// Skip non-existent pillar
		if (nullptr == neighbourPillar)
			continue;

		for (int z = 0; z < Pillar::HEIGHT; z++)
		{
			auto currentChunk   = p->chunks[z];
			auto neighbourChunk = neighbourPillar->chunks[z];

			currentChunk->neighbours[direction] = neighbourChunk;
		}
	}
}

void AChunkManager::UpdatePillarGeometry(serenity::Cell::ptr cell)
{
	if (!serenity::IsValid(cell))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cell was broken."));
		return;
	}

	Pillar* p = static_cast<Pillar*>(cell->GetData());
	if (nullptr == p)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pillar was broken."));
		return;
	}

	for (int z = 0; z < Pillar::HEIGHT; z++)
	{
		auto chunk = p->chunks[z];

		UpdateChunkGeometry(chunk);
	}
}

void AChunkManager::UpdateChunkGeometry(Chunk::ptr chunk)
{
	Buffer.Clear();

	for (const auto& [direction, vData] : BlockVertexData)
	{
		auto data = BlockVertexData.Find(direction);
		if (nullptr == data)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot find vertex data by given direction."));
			return;
		}

		auto numVertices = data->vertices.Num();

		//TODO: оптимизация: заменить цикл на одинарный
		for (int x = 0; x < Chunk::CX; x++)
			for (int y = 0; y < Chunk::CY; y++)
				for (int z = 0; z < Chunk::CZ; z++)
				{
					FVector shift = FVector(x, y, z) * 100;

					const Chunk::BlockInfo* block = chunk->GetBlock(x, y, z);

					// Skip empty blocks
					if (block && block->id == 0)
						continue;

					const Chunk::BlockInfo* adjustedBlock = nullptr;
					switch (direction) {
					case Direction::TOP:
						adjustedBlock = chunk->GetBlock(x, y, z + 1);
						break;
					case Direction::BOTTOM:
						adjustedBlock = chunk->GetBlock(x, y, z - 1);
						break;
					case Direction::FRONT:
						adjustedBlock = chunk->GetBlock(x + 1, y, z);
						break;
					case Direction::BACK:
						adjustedBlock = chunk->GetBlock(x - 1, y, z);
						break;
					case Direction::RIGHT:
						adjustedBlock = chunk->GetBlock(x, y + 1, z);
						break;
					case Direction::LEFT:
						adjustedBlock = chunk->GetBlock(x, y - 1, z);
						break;
					}

					if (nullptr == adjustedBlock)
						continue;
					else if (adjustedBlock->id != 0)
						continue;

					// PARSE VERTICES:
					// UVs
					for (uint32 i = 0; i < (uint32)numVertices; i++)
						Buffer.uvs.Add(data->uvs[i]);

					// Indices
					int32 numElements = Buffer.vertices.Num();
					for (int i = 0; i < data->triangles.Num(); i++)
						Buffer.triangles.Add(data->triangles[i] + numElements);

					// Vertices
					for (uint32 i = 0; i < (uint32)numVertices; i++)
						Buffer.vertices.Add(data->vertices[i] + shift);

					// Meta
					for (uint32 i = 0; i < (uint32)numVertices; i++)
						Buffer.meta.Add(FVector2D(block->id - 1, 0.0));
				}
	}

	bool bIsRegistered = chunk->Mesh->IsRegistered();

	// Return if there is no polygons
	if (Buffer.vertices.Num() == 0)
	{
		// Remove from drawing
		if (bIsRegistered)
			chunk->Mesh->UnregisterComponent();

		return;
	}

	// Upload vertices to GPU
	static const bool isUseCollision = true;
	chunk->Mesh->CreateMeshSection(0, Buffer.vertices, Buffer.triangles, TArray<FVector>(), Buffer.uvs, Buffer.meta, TArray<FVector2D>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), isUseCollision);

	// If was empty and not registered -> allow render
	if (!bIsRegistered)
		chunk->Mesh->RegisterComponent();
}

bool AChunkManager::LoadLandscapeResources()
{
	// Store block vertex data
	for (const auto& [direction, mesh] : BlockSides)
	{
		if (!mesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot get UStaticMesh by key: %s"), *direction);
			return false;
		}

		BlockVertexData.Emplace(serenity::DirectionToEnum[direction], GetVertexDataFromMesh(mesh));
	}

	// Store foliage vertex data
	for (const auto& [direction, mesh] : FoliageSides)
	{
		if (!mesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot get UStaticMesh by key: %s"), *direction);
			return false;
		}

		FoliageVertexData.Emplace(serenity::DirectionToEnum[direction], GetVertexDataFromMesh(mesh));
	}

	return true;
}

AChunkManager::VertexData AChunkManager::GetVertexDataFromMesh(UStaticMesh* Mesh)
{
	auto RenderData = Mesh->GetRenderData();
	auto nIdx = RenderData->GetFirstValidLODIdx(0);

	const auto& resource = RenderData->LODResources[nIdx];
	const auto& StaticVertexBuffer = resource.VertexBuffers.StaticMeshVertexBuffer;

	auto* VertexBuffer  = &RenderData->LODResources[nIdx].VertexBuffers.PositionVertexBuffer;
	auto* IndicesBuffer = &RenderData->LODResources[nIdx].IndexBuffer;

	check(nullptr != VertexBuffer);
	check(nullptr != IndicesBuffer);

	auto numUVs		  = StaticVertexBuffer.GetNumTexCoords();
	auto numIndices   = IndicesBuffer->GetNumIndices();
	auto numVertices  = VertexBuffer->GetNumVertices();
	auto numTexcoords = RenderData->LODResources[nIdx].GetNumTexCoords();

	VertexData data;

	// Vertices
	for (uint32 i = 0; i < numVertices; i++)
		data.vertices.Add(VertexBuffer->VertexPosition(i));

	// UVs
	for (uint32 i = 0; i < numVertices; i++)
		data.uvs.Add(StaticVertexBuffer.GetVertexUV(i, 0));

	// Indices
	for (int32 i = 0; i < numIndices; i++)
		data.triangles.Add(IndicesBuffer->GetIndex(i));

	return data; //TODO: optimize return
}

void AChunkManager::VertexData::Clear()
{
	uvs.Empty();
	meta.Empty();
	triangles.Empty();
	vertices.Empty();
	normals.Empty();
}

// Pillar

Pillar::Pillar(int x, int y)
{
	chunks.SetNum(HEIGHT);
	pos = FIntPoint(x, y);

	// Update procedural mesh
	for (int z = 0; z < Pillar::HEIGHT; z++)
		chunks[z] = MakeShareable(new Chunk(x, y, z));
}

Pillar::Pillar(FIntPoint position) : Pillar(position.X, position.Y) {}
Pillar::~Pillar() {}

bool Pillar::SetBlock(int lx, int ly, int lz, Chunk::BlockInfo block)
{
	int localZ = FMath::Floor(lz / (float)Pillar::HEIGHT);
	int blockZ = lz - localZ * Pillar::HEIGHT;

	return chunks[localZ]->SetBlock(lx, ly, blockZ, block);
}

// Chunk

Chunk::Chunk(int x, int y, int z)
{
	pos = FIntVector(x, y, z);

	neighbours = {
		{ Direction::FRONT,  nullptr },
		{ Direction::BACK,	 nullptr },
		{ Direction::RIGHT,  nullptr },
		{ Direction::LEFT,	 nullptr },
		{ Direction::TOP,	 nullptr },
		{ Direction::BOTTOM, nullptr }
	};
}

Chunk::Chunk(FIntVector&& pos) : Chunk(pos.X, pos.Y, pos.Z) {}
Chunk::~Chunk() {}

const Chunk::BlockInfo* Chunk::GetBlock(int lx, int ly, int lz)
{
	return FindBlock(lx, ly, lz);
}

bool Chunk::SetBlock(int lx, int ly, int lz, BlockInfo newBlock)
{
	if (auto oldBlock = FindBlock(lx, ly, lz))
	{
		if (oldBlock->id == 0 && newBlock.id != 0)
			++numElements;
		else if (newBlock.id == 0 && oldBlock->id != 0)
			--numElements;

		*oldBlock = newBlock;

		if (numElements < 0)
			numElements = 0;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Block pos out of range."));
		return false;
	}

	return true;
}

Chunk::BlockInfo* Chunk::FindBlock(int lx, int ly, int lz)
{
	if (lx < 0)
		return (neighbours[Direction::BACK] ? neighbours[Direction::BACK]->FindBlock(lx + CX, ly, lz) : nullptr);
	else if (lx >= CX)
		return (neighbours[Direction::FRONT] ? neighbours[Direction::FRONT]->FindBlock(lx - CX, ly, lz) : nullptr);
	else if (ly < 0)
		return (neighbours[Direction::LEFT] ? neighbours[Direction::LEFT]->FindBlock(lx, ly + CY, lz) : nullptr);
	else if (ly >= CY)
		return (neighbours[Direction::RIGHT] ? neighbours[Direction::RIGHT]->FindBlock(lx, ly - CY, lz) : nullptr);
	else if (lz < 0)
		return (neighbours[Direction::BOTTOM] ? neighbours[Direction::BOTTOM]->FindBlock(lx, ly, lz + CZ) : nullptr);
	else if (lz >= CZ)
		return (neighbours[Direction::TOP] ? neighbours[Direction::TOP]->FindBlock(lx, ly, lz - CZ) : nullptr);

	return &blocks[GetIndex(lx, ly, lz)];
}

int Chunk::GetIndex(int x, int y, int z)
{
	return (x * Chunk::CX + y) * Chunk::CY + z;
}

// ---------------------------------------------------------------

Chunk::BlockInfo* Chunk::FindBlock(FIntVector&& position)
{
	return FindBlock(position.X, position.Y, position.Z);
}

const Chunk::BlockInfo* Chunk::GetBlock(FIntVector&& position)
{
	return GetBlock(position.X, position.Y, position.Z);
}

bool Chunk::SetBlock(FIntVector&& position, BlockInfo newBlock)
{
	return SetBlock(position.X, position.Y, position.Z, newBlock);
}

bool Pillar::SetBlock(FIntVector&& position, Chunk::BlockInfo block)
{
	return SetBlock(position.X, position.Y, position.Z, block);
}

int Chunk::GetIndex(FIntVector&& pos)
{
	return GetIndex(pos.X, pos.Y, pos.Z);
}

// ---------------------------------------------------------------
#pragma once

#include <stdint.h>

struct Position
{
	float x;
	float y;
	float z;
};

struct TexCoord
{
	float u;
	float v;
};

struct Normal
{
	float x;
	float y;
	float z;
};

struct Face
{
	uint32_t posIdx;
	uint32_t texIdx;
	uint32_t normIdx;
};

struct Mesh
{
	char* Name;
	struct Position* Positions;
	uint32_t NumPositions;
	struct TexCoord* TexCoords;
	uint32_t NumTexCoords;
	struct Normal* Normals;
	uint32_t NumNormals;
	struct Face* Faces;
	uint32_t NumFaces;
};

struct Model
{
	struct Mesh* Meshes;
	uint32_t NumMeshes;
	char* Directory;
};

struct Model* OLLoad(const char* filename);
void OLDumpModelToFile(const struct Model* model, const char* filename);

struct Mesh* MeshNew(void);
void MeshFree(struct Mesh* mesh);
void MeshDeinit(struct Mesh* mesh);

struct Model* ModelNew(void);
void ModelFree(struct Model* model);
void ModelDeinit(struct Model* model);


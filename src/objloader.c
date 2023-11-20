#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

#include "objloader.h"

void OLLogInfo(const char* fmt, ...)
{
#if OBJLOADER_VERBOSE
	static char out[512];
	va_list args;
	va_start(args, fmt);
	vsprintf(out, fmt, args);
	va_end(args);
	fprintf(stdout, "%s\n", out);
#endif
}

void OLLogError(const char* fmt, ...)
{
	static char out[512];
	va_list args;
	va_start(args, fmt);
	vsprintf(out, fmt, args);
	va_end(args);
	fprintf(stderr, "ERROR: %s\n", out);
}

uint32_t OLReadLine(FILE* f, char* out, uint32_t sz)
{
	uint32_t len = 0;
	char ch = 0;
	while(sz--)
	{
		if ((ch = fgetc(f)) != EOF)
		{
			if (ch == '\n')
			{
				break;
			}
			out[len++] = ch;
		}
	}
	out[len] = 0;
	return len;
}

struct MeshInfo
{
	uint32_t NumPositions;
	uint32_t NumNormals;
	uint32_t NumTexCoords;
	uint32_t NumFaces;
};

static uint32_t OLGetNumMeshes(FILE* objfile)
{
	uint32_t numMeshes = 0;
	char line[128];
	while(OLReadLine(objfile, line, 128))
	{
		if (line[0] == 'o')
		{
			++numMeshes;
		}
	}
	rewind(objfile);
	return numMeshes;
}

static uint32_t OLNumFacesInLine(const char* line)
{
	uint32_t numSlashes = 0;
	while (*line++)
	{
		if (*line == '/')
		{
			++numSlashes;
		}
	}
	assert(numSlashes % 2 == 0 && "Mesh is not triangulated");
	return numSlashes / 2;
}

static void OLGetMeshInfos(struct MeshInfo* infos, uint32_t numMeshes, FILE* objfile)
{
	char line[128];
	int32_t meshIdx = -1;
	while(OLReadLine(objfile, line, 128))
	{
		if (strstr(line, "o "))
		{
			++meshIdx;
		}
		else if (strstr(line, "v "))
		{
			infos[meshIdx].NumPositions++;
		}
		else if (strstr(line, "vt"))
		{
			infos[meshIdx].NumTexCoords++;
		}
		else if (strstr(line, "vn"))
		{
			infos[meshIdx].NumNormals++;
		}
		else if (strstr(line, "f "))
		{
			infos[meshIdx].NumFaces += OLNumFacesInLine(line); // we expect 3 faces in row
		}
	}
	assert(meshIdx + 1 == numMeshes);
	rewind(objfile);
}

static void OLParseMeshes(struct Mesh* meshes, uint32_t numMeshes, struct MeshInfo* infos, FILE* objfile)
{
	char line[128];
	char prefix[4];
	float vec[3];
	uint32_t idx[9];
	int32_t result = 0;
	int32_t meshIdx = -1;
	struct Mesh* mesh = NULL;
	struct MeshInfo* info = NULL;
	struct Face face = { 0 };
	
	while(OLReadLine(objfile, line, 128))
	{
		memset(prefix, 0, sizeof(prefix));
		strncpy(prefix, line, 2);
		if (strcmp(prefix, "o ") == 0)
		{
			++meshIdx;
			mesh = meshes + meshIdx;
			info = infos + meshIdx;
			mesh->Name = strdup(line + 2); 
		}
		else if (strcmp(prefix, "v ") == 0)
		{
			result = sscanf(line, "%s%f%f%f", prefix,  vec, vec + 1, vec + 2);
			assert(result == 4);
			mesh->Positions[mesh->NumPositions].x = vec[0];
			mesh->Positions[mesh->NumPositions].y = vec[1];
			mesh->Positions[mesh->NumPositions].z = vec[2];
			++mesh->NumPositions;
			assert(mesh->NumPositions <= info->NumPositions);
			OLLogInfo("Position { %f %f %f }", vec[0], vec[1], vec[2]); 	
		}
		else if (strcmp(prefix, "vt") == 0)
		{
			result = sscanf(line, "%s%f%f", prefix, vec, vec + 1);
			assert(result == 3);
			mesh->TexCoords[mesh->NumTexCoords].u = vec[0];
			mesh->TexCoords[mesh->NumTexCoords].v = vec[1];
			++mesh->NumTexCoords;
			assert(mesh->NumTexCoords <= info->NumTexCoords);
			OLLogInfo("TexCoord { pref: %s %f %f }", prefix, vec[0], vec[1]);
		}
		else if (strcmp(prefix, "vn") == 0)
		{
			result = sscanf(line, "%s%f%f%f", prefix, vec, vec + 1, vec + 2);
			assert(result == 4);
			mesh->Normals[mesh->NumNormals].x = vec[0];
			mesh->Normals[mesh->NumNormals].y = vec[1];
			mesh->Normals[mesh->NumNormals].z = vec[2];
			++mesh->NumNormals;
			assert(mesh->NumNormals <= info->NumNormals);
			OLLogInfo("Normal { %f %f %f }", vec[0], vec[1], vec[2]);
		}
		else if (strcmp(prefix, "f ") == 0)
		{
			result = sscanf(line, "%s%d/%d/%d%d/%d/%d%d/%d/%d",
					prefix, idx, idx+1, idx+2, idx+3, idx+4, idx+5, idx+6, idx+7, idx+8);
			assert(result == 10);
			OLLogInfo("Face { %d/%d/%d %d/%d/%d %d/%d/%d }",
					idx[0], idx[1], idx[2], idx[3], idx[4], idx[5], idx[6], idx[7], idx[8]);
			for (uint32_t i = 0; i < 9; i += 3)
			{
				face.posIdx = idx[i] - 1;
				face.texIdx = idx[i + 1] - 1;
				face.normIdx = idx[i + 2] - 1;
				mesh->Faces[mesh->NumFaces++] = face;
				assert(mesh->NumFaces <= info->NumFaces);
			}
		}
		else 
		{
			OLLogInfo("Skip %s, because unknown prefix: %s\n",line, prefix);
		}
	}
}

static uint32_t OLValidateMeshes(const struct Mesh* meshes, const struct MeshInfo* infos, uint32_t numMeshes)
{
	uint32_t areMeshesValid = 1;
	for (uint32_t i = 0; i < numMeshes; ++i)
	{
		const struct Mesh* m = meshes + i;
		const struct MeshInfo* mi = infos + i;
		areMeshesValid = areMeshesValid && m->NumFaces == mi->NumFaces;
		assert(m->NumFaces == mi->NumFaces);
		areMeshesValid = areMeshesValid && m->NumNormals == mi->NumNormals;
		assert(m->NumNormals == mi->NumNormals);
		areMeshesValid = areMeshesValid && m->NumTexCoords == mi->NumTexCoords;
		assert(m->NumTexCoords == mi->NumTexCoords);
		areMeshesValid = areMeshesValid && m->NumPositions == mi->NumPositions;
		assert(m->NumPositions == mi->NumPositions);
	}
	return areMeshesValid;
}

static char* OLGetCwd(const char* filename)
{
	char* lastSlash = NULL;
	uint32_t len = 0;
	lastSlash = strrchr(filename, '/');
	lastSlash = lastSlash ? lastSlash : strrchr(filename, '\\');
	if (lastSlash)
	{
		len = lastSlash - filename; 
	}
	char* dir = NULL;
	if (len > 0)
	{
		dir = malloc(len + 1);
		strncpy(dir, filename, len);
		return dir;
	}
	else
	{
		dir = strdup(filename);
	}
	return dir;	
}

void OLDumpModelToFile(const struct Model* model, const char* filename)
{
	FILE* f = fopen(filename, "w");
	if (!f)
	{
		OLLogError("Failed to create file %s", filename);
	}
	
	fprintf(f, "NumMeshes: %d\nDirectory: %s\n", model->NumMeshes, model->Directory);
	for (uint32_t i = 0; i < model->NumMeshes; ++i)
	{
		struct Mesh* m = model->Meshes + i;
		fprintf(f, "MeshName: %s\n", m->Name);

		fprintf(f, "NumPositions: %d\n", m->NumPositions);
		for (uint32_t j = 0; j < m->NumPositions; ++j)
		{
			fprintf(f, "Position { %f %f %f }\n", 
					m->Positions[j].x, m->Positions[j].y, m->Positions[j].z);
		}

		fprintf(f, "NumTexCoords: %d\n", m->NumTexCoords);
		for (uint32_t j = 0; j < m->NumTexCoords; ++j)
		{
			fprintf(f, "TexCoord { %f %f }\n", 
					m->TexCoords[j].u, m->TexCoords[j].v);
		}

		fprintf(f, "NumNormals: %d\n", m->NumNormals);
		for (uint32_t j = 0; j < m->NumNormals; ++j)
		{
			fprintf(f, "Normal { %f %f %f }\n", 
					m->Normals[j].x, m->Normals[j].x, m->Normals[j].y);
		}

		fprintf(f, "NumFaces: %llu\n", m->NumFaces);
		for (uint32_t j = 0; j < m->NumFaces; ++j)
		{
			fprintf(f, "Face { %d %d %d }\n", 
					m->Faces[j].posIdx, m->Faces[j].texIdx, m->Faces[j].normIdx);
		}
	}
	fclose(f);
}

struct Model* OLLoad(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (!f)
	{
		OLLogError("Failed to open %s", filename);
		return NULL;
	}

	const uint32_t numMeshes = OLGetNumMeshes(f);
	if (numMeshes == 0)
	{
		OLLogError("No meshes found in %s", filename);
		return NULL;
	}

	size_t bytes = sizeof(struct Mesh) * numMeshes;
	struct Mesh* meshes = malloc(bytes);
	memset(meshes, 0, bytes);

	bytes = sizeof(struct MeshInfo) * numMeshes;
	struct MeshInfo* infos = malloc(bytes);
	memset(infos, 0, bytes);

	OLGetMeshInfos(infos, numMeshes, f);
	//assert(numMeshes == 1);
	for (uint32_t i = 0; i < numMeshes; ++i)
	{
		meshes[i].Faces = malloc(sizeof(struct Face) * infos[i].NumFaces);
		meshes[i].Normals = malloc(sizeof(struct Normal) * infos[i].NumNormals);
		meshes[i].Positions = malloc(sizeof(struct Position) * infos[i].NumPositions);
		meshes[i].TexCoords = malloc(sizeof(struct TexCoord) * infos[i].NumTexCoords);
	}
	OLParseMeshes(meshes, numMeshes, infos, f);
	fclose(f);

	assert(OLValidateMeshes(meshes, infos, numMeshes));
	free(infos);

	struct Model* model = malloc(sizeof(struct Model));
	model->Meshes = meshes;
	model->NumMeshes = numMeshes;
	model->Directory = OLGetCwd(filename);
	//OLDumpModelToFile(model, "model.txt");

	return model;
}

struct Mesh* MeshNew(void)
{
	const size_t bytes = sizeof(struct Mesh);
	struct Mesh* mesh = malloc(bytes);
	memset(mesh, 0, bytes);
	return mesh;
}

void MeshFree(struct Mesh* mesh)
{
	MeshDeinit(mesh);
	free(mesh);
}

void MeshDeinit(struct Mesh* mesh)
{
	free(mesh->Name);
	free(mesh->Normals);
	free(mesh->Positions);
	free(mesh->TexCoords);
	free(mesh->Faces);
}

struct Model* ModelNew(void)
{
	const size_t bytes = sizeof(struct Model);
	struct Model* model = malloc(bytes);
	memset(model, 0, bytes);
	return model;
}

void ModelFree(struct Model* model)
{
	ModelDeinit(model);
	free(model);
}

void ModelDeinit(struct Model* model)
{
	free(model->Directory);
	for (uint32_t i = 0; i < model->NumMeshes; ++i)
	{
		MeshDeinit(model->Meshes + i);
	}
	free(model->Meshes);
}


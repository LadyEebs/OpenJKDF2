#ifndef _RDMODEL3_H
#define _RDMODEL3_H

#include "types.h"
#include "globals.h"

#include "Primitives/rdVector.h"
#include "Raster/rdFace.h"
#include "Engine/rdMaterial.h"
#include "Primitives/rdMatrix.h"

#define rdModel3_RegisterLoader_ADDR (0x00443DA0)
#define rdModel3_RegisterUnloader_ADDR (0x00443DB0)
#define rdModel3_ClearFrameCounters_ADDR (0x00443DC0)
#define rdModel3_NewEntry_ADDR (0x00443DD0)
#define rdModel3_New_ADDR (0x00443E00)
#define rdModel3_Load_ADDR (0x00443E80)
#define rdModel3_LoadPostProcess_ADDR (0x00444B60)
#define rdModel3_WriteText_ADDR (0x00444B90)
#define rdModel3_Free_ADDR (0x004453C0)
#define rdModel3_FreeEntry_ADDR (0x004453F0)
#define rdModel3_FreeEntryGeometryOnly_ADDR (0x00445560)
#define rdModel3_Validate_ADDR (0x004456B0)
#define rdModel3_CalcBoundingBoxes_ADDR (0x00445750)
#define rdModel3_BuildExpandedRadius_ADDR (0x00445810)
#define rdModel3_CalcFaceNormals_ADDR (0x00445970)
#define rdModel3_CalcVertexNormals_ADDR (0x00445AD0)
#define rdModel3_FindNamedNode_ADDR (0x00445D30)
#define rdModel3_GetMeshMatrix_ADDR (0x00445D80)
#define rdModel3_ReplaceMesh_ADDR (0x00445DD0)
#define rdModel3_Draw_ADDR (0x00445E10)
#define rdModel3_DrawHNode_ADDR (0x00446090)
#define rdModel3_DrawMesh_ADDR (0x00446110)
#define rdModel3_DrawFace_ADDR (0x00446580)

typedef struct rdThing rdThing;
typedef struct rdHierarchyNode rdHierarchyNode;
typedef struct rdMesh rdMesh;
typedef struct rdMeshinfo rdMeshinfo;

#ifndef JKM_TYPES
typedef struct rdHierarchyNode
{
    char name[32];
    uint32_t field_20;
    uint32_t field_24;
    uint32_t field_28;
    uint32_t field_2C;
    uint32_t field_30;
    uint32_t field_34;
    uint32_t field_38;
    uint32_t field_3C;
    uint32_t flags;
    uint32_t idx;
    int type;
    uint32_t meshIdx;
#ifdef JKM_BONES
    uint32_t numParents;
#endif
    rdHierarchyNode* parent;
    uint32_t numChildren;
    rdHierarchyNode* child;
    rdHierarchyNode* nextSibling;
    rdVector3 pivot;
    rdVector3 pos;
    rdVector3 rot;
    rdMatrix34 posRotMatrix;
} rdHierarchyNode;
#else
typedef struct rdHierarchyNode
{
    uint32_t flags;
    uint32_t idx;
    int type;
    uint32_t meshIdx;
    uint32_t numParents;
    rdHierarchyNode* parent;
    uint32_t numChildren;
    rdHierarchyNode* child;
    rdHierarchyNode* nextSibling;
    rdVector3 pivot;
    rdVector3 pos;
    rdVector3 rot;
    rdMatrix34 posRotMatrix;
} rdHierarchyNode;
#endif

typedef struct rdGeoset
{
    uint32_t numMeshes;
    rdMesh* meshes;    
} rdGeoset;

typedef struct rdModel3
{
    char filename[32];
    int id;
    rdGeoset geosets[4];
    uint32_t numGeosets;
    rdMaterial** materials;
    uint32_t numMaterials;
    uint32_t geosetSelect;
    uint32_t numHierarchyNodes;
    rdHierarchyNode* hierarchyNodes;
    float radius;
    uint32_t field_60;
    float field_64;
    uint32_t field_68;
    uint32_t field_6C;
    uint32_t field_70;
    uint32_t field_74;
    rdVector3 insertOffset;
#ifdef PUPPET_PHYSICS
	rdMatrix34* paBasePoseMatrices;
#endif
} rdModel3;

#ifndef JKM_TYPES
typedef struct rdMesh
{
    char name[32];
    int mesh_num;
    int geometryMode;
    int lightingMode;
    int textureMode;
    rdVector3* vertices;
    rdVector2* vertexUVs;
    float* vertices_i;
    float* vertices_unk;
#ifdef JKM_LIGHTING
    float* paRedIntensities;
    float* paGreenIntensities;
    float* paBlueIntensities;
#ifdef RGB_THING_LIGHTS
	float* vertices_r;
	float* vertices_g;
	float* vertices_b;
#endif
#endif
    rdFace* faces;
    rdVector3* vertexNormals;
    int numVertices;
    int numUVs;
    int numFaces;
#ifdef JKM_LIGHTING
    float extraLight;
#endif
    float radius;
    int field_58;
    int field_5C;
    int field_60;
    float field_64;
    int field_68;
    int field_6C;
#ifdef PUPPET_PHYSICS
	float minRadius;
	float maxRadius;
	rdVector3 center;
#endif
} rdMesh;
#else

typedef struct rdMesh
{
    //char name[32];
    int mesh_num;
    int geometryMode;
    int lightingMode;
    int textureMode;
    rdVector3* vertices;
    rdVector2* vertexUVs;
    float* vertices_i;
    float* vertices_unk;
    float* paRedIntensities;
    float* paGreenIntensities;
    float* paBlueIntensities;
    int unk4;
    rdFace* faces;
    rdVector3* vertexNormals;
    int numVertices;
    int numUVs;
    int numFaces;
    float extraLight;
    float radius;
    int field_58;
    int field_5C;
    int field_60;
    float field_64;
    int field_68;
    int field_6C;
#ifdef PUPPET_PHYSICS
	float minRadius;
	float maxRadius;
#endif
} rdMesh;
#endif

model3Loader_t rdModel3_RegisterLoader(model3Loader_t loader);
model3Unloader_t rdModel3_RegisterUnloader(model3Unloader_t unloader);
void rdModel3_ClearFrameCounters();
int rdModel3_NewEntry(rdModel3 *model);
rdModel3* rdModel3_New(char *path);
int rdModel3_Load(char *model_fpath, rdModel3 *model);
void rdModel3_LoadPostProcess(rdModel3 *model);
int rdModel3_WriteText(char *fout, rdModel3 *model, char *createdfrom);
void rdModel3_Free(rdModel3 *model);
void rdModel3_FreeEntry(rdModel3 *model);
void rdModel3_FreeEntryGeometryOnly(rdModel3 *model);
rdModel3* rdModel3_Validate(rdModel3 *model);
void rdModel3_CalcBoundingBoxes(rdModel3 *model);
void rdModel3_BuildExpandedRadius(rdModel3 *model, rdHierarchyNode *node, const rdMatrix34 *matrix);
void rdModel3_CalcFaceNormals(rdModel3 *model);
void rdModel3_CalcVertexNormals(rdModel3 *model);
void rdModel3_CalcNumParents(rdModel3* pModel); // MOTS added
rdHierarchyNode* rdModel3_FindNamedNode(char *name, rdModel3 *model);
int rdModel3_GetMeshMatrix(rdThing *thing, rdMatrix34 *matrix, uint32_t nodeIdx, rdMatrix34 *out);
int rdModel3_ReplaceMesh(rdModel3 *model, int geosetIdx, int meshIdx, rdMesh *in);
int rdModel3_Draw(rdThing *thing, rdMatrix34 *matrix_4_3);
void rdModel3_DrawHNode(rdHierarchyNode *pNode);
void rdModel3_DrawMesh(rdMesh *meshIn, rdMatrix34 *mat);
int rdModel3_DrawFace(rdFace *face, int lightFlags);

#ifdef RENDER_DROID2
void rdModel3_DrawOccluders(rdThing* pThing, rdMatrix34* pMat);
#endif

//static int (__cdecl *rdModel3_CalcVertexNormals)(rdModel3 *model) = (void*)rdModel3_CalcVertexNormals_ADDR;

#endif // _RDMODEL3_H

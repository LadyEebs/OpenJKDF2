#include "sithModel.h"

#include "Primitives/rdModel3.h"
#include "Engine/rdroid.h"
#include "World/sithWorld.h"
#include "General/stdConffile.h"
#include "stdPlatform.h"
#include "jk.h"

static stdHashTable* sithModel_hashtable;

int sithModel_Startup()
{
    sithModel_hashtable = stdHashTable_New(256);
    return sithModel_hashtable != 0;
}

void sithModel_Shutdown()
{
    if ( sithModel_hashtable )
    {
        stdHashTable_Free(sithModel_hashtable);
        sithModel_hashtable = 0;
    }
}

int sithModel_Load(sithWorld *world, int a2)
{
    int numModels;
    flex_t loadStep;
    flex_t loadProgress;

    if ( a2 )
        return 0;
    stdConffile_ReadArgs();
    if ( _memcmp(stdConffile_entry.args[0].value, "world", 6u) || _memcmp(stdConffile_entry.args[1].value, "models", 7u) )
        return 0;
    world->numModels = _atoi(stdConffile_entry.args[2].value);
    if ( !world->numModels )
        return 1;

    world->models = (rdModel3 *)pSithHS->alloc(sizeof(rdModel3) * world->numModels);
    if ( !world->models )
    {
        stdPrintf(pSithHS->errorPrint, ".\\World\\sithModel.c", 164, "Memory error while reading models, line %d.\n", stdConffile_linenum, 0, 0, 0);
        return 0;
    }
    world->numModelsLoaded = 0;
    _memset(world->models, 0, sizeof(rdModel3) * world->numModels);

    sithWorld_UpdateLoadPercent(60.0);
    loadStep = 10.0 / (flex_d_t)world->numModels;
    loadProgress = 60.0;
    while ( stdConffile_ReadArgs() )
    {
        if ( !_memcmp(stdConffile_entry.args[0].value, "end", 4u) )
            break;
        sithModel_LoadEntry(stdConffile_entry.args[1].value, 0);
        loadProgress = loadProgress + loadStep;
        sithWorld_UpdateLoadPercent(loadProgress);
    }
    sithWorld_UpdateLoadPercent(70.0);

    return 1;
}

void sithModel_Free(sithWorld *world)
{
    if (!world->numModels )
        return;

    for (int i = 0; i < world->numModelsLoaded; i++)
    {
        stdHashTable_FreeKey(sithModel_hashtable, world->models[i].filename);
        rdModel3_FreeEntryGeometryOnly(&world->models[i]);
    }
    pSithHS->free(world->models);
    world->models = 0;
    world->numModelsLoaded = 0;
    world->numModels = 0;
}

rdModel3* sithModel_LoadEntry(const char *model_3do_fname, int unk)
{
    rdModel3 *model;
    char model_fpath[128];

    model = (rdModel3 *)stdHashTable_GetKeyVal(sithModel_hashtable, model_3do_fname);
    if ( model )
        return model;

    if ( sithWorld_pLoading->numModelsLoaded >= sithWorld_pLoading->numModels )
        return 0;
    model = &sithWorld_pLoading->models[sithWorld_pLoading->numModelsLoaded];

    _sprintf(model_fpath, "%s%c%s", "3do", '\\', model_3do_fname);
    if ( !rdModel3_Load(model_fpath, model) )
    {
        if ( !unk )
            return sithModel_LoadEntry("dflt.3do", 1);
        return 0;
    }
    
    model->id = sithWorld_pLoading->numModelsLoaded;
    if (sithWorld_pLoading->level_type_maybe & 1)
        model->id |= 0x8000;
    
    stdHashTable_SetKeyVal(sithModel_hashtable, model->filename, model);
    sithWorld_pLoading->numModelsLoaded += 1;

    return model;
}

uint32_t sithModel_GetMemorySize(rdModel3 *model)
{
    unsigned int result; // eax
    rdGeoset *v2; // ebx
    int v3; // edi
    rdMesh* v4; // edx
    int v5; // esi
    rdFace* v6; // ecx
    int modela; // [esp+8h] [ebp+4h]

    result = (sizeof(void*) * model->numMaterials) + (sizeof(rdHierarchyNode) * model->numHierarchyNodes) + sizeof(rdModel3);
    if ( model->numGeosets )
    {
        v2 = model->geosets;
        modela = model->numGeosets;
        do
        {
            result += 8;
            if ( v2->numMeshes )
            {
                v3 = v2->numMeshes;
                v4 = v2->meshes;
                do
                {
                    v5 = v4->numFaces;
                    result += (sizeof(rdVector2) * v4->numUVs) + ((sizeof(rdVector3) + sizeof(rdVector3) + sizeof(rdVector2)) * v4->numVertices) + (sizeof(rdFace) * v4->numFaces) + sizeof(rdMesh);
                    if ( v4->numFaces )
                    {
                        v6 = v4->faces;
                        do
                        {
                            result += (sizeof(int) * 2) * v6->numVertices;
                            v6++;
                            --v5;
                        }
                        while ( v5 );
                    }
                    ++v4;
                    --v3;
                }
                while ( v3 );
            }
            ++v2;
            --modela;
        }
        while ( modela );
    }
    return result;
}

int sithModel_New(sithWorld *world, int num)
{
    world->models = (rdModel3 *)pSithHS->alloc(sizeof(rdModel3) * num);
    if ( !world->models )
        return 0;

    world->numModels = num;
    world->numModelsLoaded = 0;
    _memset(world->models, 0, sizeof(rdModel3) * num);

    return 1;
}

rdModel3* sithModel_GetByIdx(int idx)
{
    sithWorld *world;
    rdModel3 *result;

    world = sithWorld_pCurrentWorld;
    if ( (idx & 0x8000) != 0 )
    {
        world = sithWorld_pStatic;
        idx &= 0x7FFF;
    }
    if ( world && idx >= 0 && idx < world->numModelsLoaded )
        return &world->models[idx];

    return NULL;
}

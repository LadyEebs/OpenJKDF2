#include "sithShader.h"

#ifdef RENDER_DROID2

#include "General/stdHashTable.h"
#include "General/stdString.h"
#include "General/stdFnames.h"

#define SITHSHADER_MAX_SHADERS 32

stdHashTable* sithShader_hashmap = NULL;

int sithShader_Startup()
{
	sithShader_hashmap = stdHashTable_New(128);
	if (sithShader_hashmap)
		return 1;
	stdPrintf(pSithHS->errorPrint, ".\\World\\sithShader.c", 63, "Failed to allocate memory for shaders.\n", 0, 0, 0, 0);
    return 1;
}

void sithShader_Shutdown()
{
	if (sithShader_hashmap)
	{
		stdHashTable_Free(sithShader_hashmap);
		sithShader_hashmap = 0;
	}
}

rdShader* sithShader_LoadEntry(char* fname)
{
	sithWorld* world = sithWorld_pLoading;
	if (!world->shaders)
	{
		world->shaders = (rdShader*)pSithHS->alloc(SITHSHADER_MAX_SHADERS * sizeof(rdShader));
		if (world->shaders)
		{
			world->numShaders = SITHSHADER_MAX_SHADERS;
			world->numShadersLoaded = 0;
			_memset(world->shaders, 0, SITHSHADER_MAX_SHADERS * sizeof(rdShader));
		}
	}

	rdShader* result = (rdShader*)stdHashTable_GetKeyVal(sithShader_hashmap, fname);
	if (!result)
	{
		if (world->numShadersLoaded < world->numParticles)
		{
			rdShader* shader = &world->shaders[world->numShadersLoaded];
			
			char path[128]; // [esp+Ch] [ebp-80h] BYREF
			_sprintf(path, "%s%c%s", "misc\\shader", '\\', fname);
			if (rdShader_LoadEntry(path, shader))
			{
				stdHashTable_SetKeyVal(sithShader_hashmap, shader->name, shader);
				++world->numParticlesLoaded;
				result = shader;
			}
		}
		else
		{
			result = 0;
		}
	}
	return result;
}

void sithShader_Free(sithWorld* world)
{
	if (!world->numShadersLoaded) return;

	for (int i = 0; i < world->numShadersLoaded; i++)
	{
		stdHashTable_FreeKey(sithShader_hashmap, world->shaders[i].name);
		rdShader_FreeEntry(&world->shaders[i]);
	}

	pSithHS->free(world->shaders);
	world->shaders = 0;
	world->numShaders = 0;
	world->numShadersLoaded = 0;
}

#endif

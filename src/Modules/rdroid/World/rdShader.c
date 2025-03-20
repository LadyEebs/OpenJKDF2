#include "rdShader.h"

#ifdef RENDER_DROID2

#include "General/stdMath.h"
#include "General/stdString.h"
#include "Engine/rdroid.h"

#include "Modules/std/std3D.h"

rdShader* rdShader_New(char* fpath)
{
	rdShader* shader;
	shader = (rdShader*)rdroid_pHS->alloc(sizeof(rdShader));
    if ( shader )
    {
		rdShader_NewEntry(shader, fpath);
    }
    
    return shader;
}

int rdShader_NewEntry(rdShader* shader, char* path)
{
    if (path)
        stdString_SafeStrCopy(shader->name, path, 0x20);
	shader->id = -1;
	shader->shaderid = std3D_GenShader();
	return 0;
}

void rdShader_Free(rdShader* shader)
{
    if (shader)
    {
		rdShader_FreeEntry(shader);
        rdroid_pHS->free(shader);
    }
}

void rdShader_FreeEntry(rdShader* shader)
{
	std3D_DeleteShader(shader->shaderid);
}

int rdShader_LoadEntry(char* fpath, rdShader* shader)
{
	stdString_SafeStrCopy(shader->name, stdFileFromPath(fpath), 0x20);

	// todo: move parsing/file loading here and parse raw strings in std3D

	return 0;
}

#endif
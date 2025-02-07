#ifndef _SITHSHADER_H
#define _SITHSHADER_H

#include "types.h"
#include "globals.h"

#ifdef RENDER_DROID2

#include "Modules/rdroid/World/rdShader.h"

int sithShader_Startup();
void sithShader_Shutdown();
rdShader* sithShader_LoadEntry(char* fname);
void sithShader_Free(sithWorld* world);

#endif

#endif // _SITHSHADER_H

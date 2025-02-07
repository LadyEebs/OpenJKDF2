#ifndef _RDSHADER_H
#define _RDSHADER_H

#include "types.h"
#include "globals.h"

#ifdef RENDER_DROID2

typedef struct rdShader
{
    char name[32];
	int  shaderid;
	int  id;
} rdShader;

rdShader* rdShader_New(char *fpath);
int rdShader_NewEntry(rdShader* shader, char* path);
void rdShader_Free(rdShader* shader);
void rdShader_FreeEntry(rdShader* shader);
int rdShader_LoadEntry(char* fpath, rdShader* shader);

#endif

#endif // _RDSHADER_H

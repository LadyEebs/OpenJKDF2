#ifndef _SITHLIGHT_H
#define _SITHLIGHT_H

#include "types.h"
#include "globals.h"

int sithLight_Load(sithWorld *world, int a2);
void sithLight_Free(sithWorld *world);
int sithLight_New(sithWorld *world, int num);

#endif // _SITHLIGHT_H

#pragma once

#include "types.h"
#include "globals.h"

#include "Modules/rdroid/types.h"

void rdCluster_Clear();

int rdCluster_AddLight(rdLight* light, rdVector3* position, rdVector3* direction, float intensity);
int rdCluster_AddOccluder(rdVector3* position, float radius, rdVector3* verts);
int rdCluster_AddDecal(stdVBuffer* vbuf, rdDDrawSurface* texture, rdVector3* verts,
	rdMatrix44* decalMatrix, rdVector3* color, uint32_t flags, float angleFade);

void rdCluster_Build(rdMatrix44* pProjection, uint32_t width, uint32_t height);

void rdCluster_ClearLights();
void rdCluster_ClearOccluders();
void rdCluster_ClearDecals();
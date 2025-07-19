#ifndef _RDRASTER_H
#define _RDRASTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "globals.h"

#define rdRaster_Startup_ADDR (0x0044BB40)

void rdRaster_Startup();

//static int (*rdRaster_Startup)(void) = (void*)rdRaster_Startup_ADDR;
#ifdef TILE_SW_RASTER

void rdRaster_DrawToTile(rdProcEntry* entry, rdTexinfo* texinfo, int tileX, int tileY);
void rdCache_DrawFaceTiled(rdProcEntry* face, int tileX, int tileY);

void rdRaster_ClearBins();
void rdRaster_BinFaceCoarse(rdProcEntry* face);
void rdRaster_BinFaces();
#endif

#ifdef __cplusplus
}
#endif

#endif // _RDRASTER_H

#include "sithSector.h"

#include "General/stdMath.h"
#include "Primitives/rdMath.h"
#include "Raster/rdFace.h"
#include "World/sithThing.h"
#include "World/jkPlayer.h"
#include "World/sithWorld.h"
#include "Engine/sithCollision.h"
#include "Engine/sithIntersect.h"
#include "jk.h"
#include "Gameplay/sithEvent.h"
#include "Engine/rdColormap.h"
#include "Engine/sithCamera.h"
#include "Devices/sithSound.h"
#include "Devices/sithSoundMixer.h"
#include "Engine/sithRender.h"
#include "Raster/rdCache.h"
#include "Engine/sithPuppet.h"
#include "Engine/sithKeyFrame.h"
#include "World/sithMaterial.h"
#include "World/sithSurface.h"
#include "AI/sithAI.h"
#include "AI/sithAIClass.h"
#include "Dss/sithDSS.h"

// MOTS altered
int sithSector_Load(sithWorld *world, int tmp)
{
    unsigned int alloc_size; // ebx
    sithSector *v5; // eax
    sithSector *v6; // eax
    unsigned int v7; // ecx
    sithSector *sectors; // esi
    int *sector_vertices; // eax
    int v13; // edi
    unsigned int v15; // eax
    void *v16; // ecx
    int junk; // [esp+10h] [ebp-3Ch] BYREF
    unsigned int num_vertices; // [esp+14h] [ebp-38h] BYREF
    unsigned int amount_2; // [esp+18h] [ebp-34h] BYREF
    unsigned int sectors_amt; // [esp+1Ch] [ebp-30h] BYREF
    int v21; // [esp+20h] [ebp-2Ch]
    int vtx_idx; // [esp+24h] [ebp-28h] BYREF
    int amount_1; // [esp+28h] [ebp-24h] BYREF
    char sound_fname[32]; // [esp+2Ch] [ebp-20h] BYREF

    if ( tmp )
        return 0;
    if ( !stdConffile_ReadLine() || _sscanf(stdConffile_aLine, " world sectors %d", &sectors_amt) != 1 )
        return 0;
    alloc_size = sizeof(sithSector) * sectors_amt;
    v5 = (sithSector *)pSithHS->alloc(sizeof(sithSector) * sectors_amt);
    world->sectors = v5;
    if ( v5 )
    {
        _memset(v5, 0, alloc_size);
        v6 = world->sectors;
        v7 = 0;
        for ( world->numSectors = sectors_amt; v7 < sectors_amt; ++v7 )
        {
            v6->id = v7;
            v6->numVertices = 0;
            v6->verticeIdxs = 0;
            v6->numSurfaces = 0;
            v6->surfaces = 0;
            v6->thingsList = 0;
	#ifdef RENDER_DROID2
			v6->lightBuckets = 0;
	#endif
            ++v6;
        }
    }
    sectors = world->sectors;
    if ( !sectors )
        return 0;
    v21 = 0;
    if ( sectors_amt )
    {
        while ( stdConffile_ReadLine() )
        {
            if ( _sscanf(stdConffile_aLine, " sector %d", &junk) != 1 )
                break;
            if ( !stdConffile_ReadLine() )
                break;
            if ( _sscanf(stdConffile_aLine, " flags %x", &sectors->flags) != 1 )
                break;
            if ( !stdConffile_ReadLine() )
                break;
            if ( _sscanf(stdConffile_aLine, " ambient light %f", &sectors->ambientLight) != 1 )
                break;
            if ( !stdConffile_ReadLine() )
                break;
            if ( _sscanf(stdConffile_aLine, " extra light %f", &sectors->extraLight) != 1 )
                break;
            if ( !stdConffile_ReadLine() )
                break;
            if ( _sscanf(stdConffile_aLine, " colormap %d", &tmp) != 1 )
                break;
            sectors->colormap = &world->colormaps[tmp];
            if ( !stdConffile_ReadLine()
              || _sscanf(stdConffile_aLine, " tint %f %f %f", &sectors->tint, &sectors->tint.y, &sectors->tint.z) == 3 && !stdConffile_ReadLine() )
            {
                break;
            }
            if ( _sscanf(
                     stdConffile_aLine,
                     " boundbox %f %f %f %f %f %f ",
                     &sectors->boundingbox_onecorner,
                     &sectors->boundingbox_onecorner.y,
                     &sectors->boundingbox_onecorner.z,
                     &sectors->boundingbox_othercorner,
                     &sectors->boundingbox_othercorner.y,
                     &sectors->boundingbox_othercorner.z) != 6 )
                break;
            if ( !stdConffile_ReadLine() )
                break;
            if ( _sscanf(
                     stdConffile_aLine,
                     " collidebox %f %f %f %f %f %f ",
                     &sectors->collidebox_onecorner,
                     &sectors->collidebox_onecorner.y,
                     &sectors->collidebox_onecorner.z,
                     &sectors->collidebox_othercorner,
                     &sectors->collidebox_othercorner.y,
                     &sectors->collidebox_othercorner.z) == 6 )
            {
                sectors->flags |= SITH_SECTOR_HAS_COLLIDE_BOX;
                if ( !stdConffile_ReadLine() )
                    break;
            }
            if ( _sscanf(stdConffile_aLine, "sound %s %f", sound_fname, &sectors->sectorSoundVol) == 2 )
            {
                sectors->sectorSound = sithSound_LoadEntry(sound_fname, 0);
                if ( !stdConffile_ReadLine() )
                    break;
            }
            if ( _sscanf(stdConffile_aLine, " center %f %f %f", &sectors->center, &sectors->center.y, &sectors->center.z) != 3 )
                break;
            if ( !stdConffile_ReadLine() )
                break;
            if ( _sscanf(stdConffile_aLine, " radius %f", &sectors->radius) != 1 )
                break;
            if ( !stdConffile_ReadLine() )
                break;
            if ( _sscanf(stdConffile_aLine, " vertices %d", &num_vertices) != 1 )
                break;
            sector_vertices = (int *)pSithHS->alloc(4 * num_vertices);
            sectors->verticeIdxs = sector_vertices;
            if ( !sector_vertices )
                break;

            for (v13 = 0; v13 < num_vertices; v13++)
            {
                if (!stdConffile_ReadLine())
                    return 0;
                if (_sscanf(stdConffile_aLine, " %d: %d", &junk, &vtx_idx) != 2)
                    return 0;
                sectors->verticeIdxs[v13] = vtx_idx;
            }

            sectors->numVertices = num_vertices;
            if ( !stdConffile_ReadLine() || _sscanf(stdConffile_aLine, " surfaces %d %d", &amount_1, &amount_2) != 2 )
                return 0;
            sectors->numSurfaces = amount_2;

            sectors->surfaces = &world->surfaces[amount_1];
            for (v15 = 0; v15 < amount_2; v15++)
            {
                sectors->surfaces[v15].parent_sector = sectors;
            }

#ifdef RENDER_DROID2
			if (sectors->flags & SITH_SECTOR_BACKDROP)
			{
				sectors->nextBackdropSector = world->backdropSector;
				world->backdropSector = sectors;
			}
#endif

            ++sectors;
            if ( ++v21 >= sectors_amt )
                return 1;
        }
        return 0;
    }
    return 1;
}

int sithSector_GetIdxFromPtr(sithSector *sector)
{
    return sector && sector->id == sector - sithWorld_pCurrentWorld->sectors && sector->id < (unsigned int)sithWorld_pCurrentWorld->numSectors;
}

void sithSector_SetAdjoins(sithSector *sector)
{
    sithAdjoin *i; // esi

    for ( i = sector->adjoins; i; i = i->next )
        sithSurface_SetAdjoins(i);
    sector->flags &= ~SITH_SECTOR_ADJOINS_SET;
}

void sithSector_UnsetAdjoins(sithSector *sector)
{
    sithAdjoin *i; // esi

    for ( i = sector->adjoins; i; i = i->next )
        sithSurface_UnsetAdjoins(i);
    sector->flags |= SITH_SECTOR_ADJOINS_SET;
}

int sithSector_GetThingsCount(sithSector *sector)
{
    int result; // eax
    sithThing *i; // ecx

    result = 0;
    for ( i = sector->thingsList; i; ++result )
        i = i->nextThing;
    return result;
}

void sithSector_Free(sithWorld *world)
{
    for (uint32_t i = 0; i < world->numSectors; i++)
    {
        if ( world->sectors[i].verticeIdxs )
            pSithHS->free(world->sectors[i].verticeIdxs);
#ifdef RENDER_DROID2
		if ( world->sectors[i].lightBuckets )
			pSithHS->free(world->sectors[i].lightBuckets);
#endif
	}
    pSithHS->free(world->sectors);
    world->sectors = 0;
    world->numSectors = 0;
}

int sithSector_GetNumPlayers(sithSector *sector)
{
    int result; // eax
    sithThing *i; // ecx

    result = 0;
    for ( i = sector->thingsList; i; i = i->nextThing )
    {
        if ( i->type == SITH_THING_PLAYER )
            ++result;
    }
    return result;
}

sithSector* sithSector_GetPtrFromIdx(int idx)
{
    sithSector *result; // eax

    if ( sithWorld_pCurrentWorld && idx >= 0 && idx < sithWorld_pCurrentWorld->numSectors )
        result = &sithWorld_pCurrentWorld->sectors[idx];
    else
        result = 0;
    return result;
}

void sithSector_SyncSector(sithSector *pSector, int a2)
{
    uint32_t v3; // edx
    uint32_t v4; // eax
    sithSector **v5; // ecx

    if ( a2 )
    {
        pSector->flags |= SITH_SECTOR_SYNC;
    }

    if (!sithComm_multiplayerFlags || sithSector_numSync >= 0x10)
        return;

    for (v4 = 0; v4 < sithSector_numSync; v4++ )
    {
        if ( sithSector_aSyncIdk[v4] == pSector )
        {
            sithSector_aSyncIdk2[v4] |= a2;
            break;
        }
    }

    if (v4 == sithSector_numSync)
    {
        sithSector_aSyncIdk[sithSector_numSync] = pSector;
        sithSector_aSyncIdk2[sithSector_numSync++] = a2;
    }
}

void sithSector_Sync()
{
    uint32_t i; // esi

    for ( i = 0; i < sithSector_numSync; ++i )
    {
        if ( (sithSector_aSyncIdk2[i] & 1) != 0 )
            sithDSS_SendSectorStatus(sithSector_aSyncIdk[i], INVALID_DPID, 255);
        else
            sithDSS_SendSectorFlags(sithSector_aSyncIdk[i], INVALID_DPID, 255);
    }
    sithSector_numSync = 0;
}

sithSector* sithSector_sub_4F8D00(sithWorld *pWorld, rdVector3 *pos)
{
    int v2; // ebx
    unsigned int v3; // ebp
    sithSector *v4; // esi
    int v7; // eax

    v2 = 0;
    v3 = pWorld->numSectors;
    v4 = pWorld->sectors;
    if ( !v3 )
        return 0;
    while ( 1 )
    {
        if ( pos->x >= (double)v4->boundingbox_onecorner.x
          && pos->x <= (double)v4->boundingbox_othercorner.x
          && v4->boundingbox_onecorner.y <= (double)pos->y
          && v4->boundingbox_othercorner.y >= (double)pos->y )
        {
            v7 = v4->boundingbox_onecorner.z <= (double)pos->z && v4->boundingbox_othercorner.z >= (double)pos->z;
            if ( v7 && sithIntersect_IsSphereInSector(pos, 0.0, v4) )
                break;
        }
        ++v4;
        if ( ++v2 >= v3 )
            return 0;
    }
    return v4;
}


#ifdef RGB_AMBIENT
void sithWorld_ComputeSectorRGBAmbient(sithSector* sector)
{
	// JED doesn't export colored ambient, so we need to generate it on load
	rdVector_Zero3(&sector->ambientRGB); // rgb flat ambient (replaces ambient light intensity)
	rdAmbient_Zero(&sector->ambientSH); // rgb directional ambient

	//rdVector_Set3(&sector->ambientRGB, sector->ambientLight, sector->ambientLight, sector->ambientLight);
	//continue;

	double sflight = 0.0;
	float total = 0.00001f;
	for (int j = 0; j < sector->numSurfaces; j++)
	{
		sithSurface* surface = &sector->surfaces[j];

		// ignore invisible surfaces
		if (surface->surfaceInfo.face.geometryMode == RD_GEOMODE_NOTRENDERED)
			continue;

		// ignore adjoins
		if (surface->adjoin && !(surface->adjoin->flags & SITHSURF_ADJOIN_VISIBLE))
			continue;

		// ignore surfaces with no material
		if (!surface->surfaceInfo.face.material)
			continue;


		float el = stdMath_Clamp(surface->surfaceInfo.face.extraLight + sector->extraLight, 0.0f, 1.0f);
		sflight += el;

		rdVector3 negNormal;
		rdVector_Neg3(&negNormal, &surface->surfaceInfo.face.normal);

		float minlight = el;
		if ((surface->surfaceInfo.face.geometryMode != RD_GEOMODE_NOTRENDERED) && surface->surfaceInfo.face.lightingMode == RD_LIGHTMODE_FULLYLIT)
			minlight = 1.0f;

		int emissiveLightLevel = 0;
		if ((surface->surfaceFlags & SITH_SURFACE_HORIZON_SKY) || (surface->surfaceFlags & SITH_SURFACE_CEILING_SKY))
			emissiveLightLevel = -1;


		rdVector3 emissive;
		if((surface->surfaceInfo.face.geometryMode != RD_GEOMODE_NOTRENDERED)
			&& rdMaterial_GetFillColor(&emissive, surface->surfaceInfo.face.material, surface->parent_sector->colormap, 0, emissiveLightLevel)
		)
		{
			total += 1.0f;
			rdAmbient_Acc(&sector->ambientSH, &emissive, &negNormal);// &surface->surfaceInfo.face.normal);
		}

		total += surface->surfaceInfo.face.numVertices;
		for (int k = 0; k < surface->surfaceInfo.face.numVertices; ++k)
		{
			rdVector3 col;
			col.x = stdMath_Clamp(surface->surfaceInfo.intensities[k + surface->surfaceInfo.face.numVertices * 1], minlight, 1.0f);
			col.y = stdMath_Clamp(surface->surfaceInfo.intensities[k + surface->surfaceInfo.face.numVertices * 2], minlight, 1.0f);
			col.z = stdMath_Clamp(surface->surfaceInfo.intensities[k + surface->surfaceInfo.face.numVertices * 3], minlight, 1.0f);
			rdVector_Add3Acc(&sector->ambientRGB, &col);
			
			// we get more directionality by using the vertex to sector center instead of surface normal
			rdVector3* vertex = &sithWorld_pCurrentWorld->vertices[surface->surfaceInfo.face.vertexPosIdx[k]];
			rdVector3 dirToCenter;
			rdVector_Sub3(&dirToCenter, vertex, &sector->center);
			rdVector_Normalize3Acc(&dirToCenter);
			
			rdAmbient_Acc(&sector->ambientSH, &col, &dirToCenter);
		}
	}

	//if(sector->lightBuckets)
	//{
	//	for (int bucket = 0; bucket < sithWorld_pCurrentWorld->numLightBuckets; ++bucket)
	//	{
	//		uint64_t lightOffset = bucket * 64;
	//		uint64_t bucketBits = sector->lightBuckets[bucket];
	//		while (bucketBits != 0)
	//		{
	//			int bitIndex = stdMath_FindLSB64(bucketBits);
	//			bucketBits ^= 1ull << bitIndex;
	//
	//			int lightIndex = bitIndex + lightOffset;
	//			total += 1.0f;

	//			rdVector3 col;
	//			col.x = col.y = col.z = sithWorld_pCurrentWorld->lights[lightIndex].rdlight.intensity;
	//		
	//			rdVector3 lightDir;
	//			rdVector_Sub3(&lightDir, &sithWorld_pCurrentWorld->lights[lightIndex].pos , &sector->center);
	//			rdVector_Normalize3Acc(&lightDir);
	//		
	//			rdAmbient_Acc(&sector->ambientSH, &col, &lightDir);
	//		}
	//	}
	//}

	sector->ambientSH.center.x = sector->center.x;
	sector->ambientSH.center.y = sector->center.y;
	sector->ambientSH.center.z = sector->center.z;
	sector->ambientSH.center.w = sector->radius;//rdVector_Dist3(&sector->boundingbox_onecorner, &sector->boundingbox_othercorner) * 0.5f;

	//sflight /= (float)pWorld->sectors[i].numSurfaces;
	rdVector_InvScale3Acc(&sector->ambientRGB, total);
#ifdef RENDER_DROID2
	rdAmbient_Scale(&sector->ambientSH, 4.0f / total); // integration over sphere
#else
	rdAmbient_Scale(&sector->ambientSH, 4.0f * M_PI / total); // integration over sphere
	rdAmbient_UpdateDominantDirection(&sector->ambientSH);
#endif

	// normalize the color values to the avg intensity and apply the sector ambient light scalar
	//rdVector3 lum = {0.33, 0.55, 0.11};
	//float avg = fmin(1.0f, rdVector_Dot3(&sector->ambientRGB, &lum) + 1e-5f);
	//rdVector_Scale3Acc(&sector->ambientRGB, fmin(1.0f, sector->ambientLight) / avg);
	//rdAmbient_Scale(&sector->ambientSH, fmin(1.0f, sector->ambientLight) / avg);
}
#endif


#ifdef RENDER_DROID2
void sithSector_AddLight(sithSector* pSector, sithLight* pLight)
{
	if (!pSector->lightBuckets)
	{
		pSector->lightBuckets = (uint64_t*)pSithHS->alloc(sizeof(uint64_t) * sithWorld_pCurrentWorld->numLightBuckets);
		_memset(pSector->lightBuckets, 0, sizeof(uint64_t) * sithWorld_pCurrentWorld->numLightBuckets);
	}

	if (pSector->lightBuckets)
	{
		uint64_t bucketIndex = pLight->id / 64;
		uint64_t bucketPlace = pLight->id % 64;
		pSector->lightBuckets[bucketIndex] |= (1ull << bucketPlace);
	}
}
#endif
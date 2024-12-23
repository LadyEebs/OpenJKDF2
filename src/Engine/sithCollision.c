#include "sithCollision.h"

#include "World/sithThing.h"
#include "World/sithWeapon.h"
#include "World/sithItem.h"
#include "World/sithActor.h"
#include "World/sithSector.h"
#include "Engine/sithIntersect.h"
#include "World/sithWorld.h"
#include "World/jkPlayer.h"
#include "World/sithSurface.h"
#include "World/sithSoundClass.h"
#include "Gameplay/sithTime.h"
#include "Engine/sithPhysics.h"
#include "General/stdMath.h"
#include "Primitives/rdMath.h"
#include "jk.h"
#ifdef RAGDOLLS
#include "Primitives/rdRagdoll.h"
#endif

#ifdef PUPPET_PHYSICS
#include "Modules/sith/Engine/sithConstraint.h"
#endif

static int sithCollision_initted = 0;

int sithCollision_bDebugCollide = 0;

int sithCollision_Startup()
{
    if ( sithCollision_initted )
        return 0;

    _memset(sithCollision_collisionHandlers, 0, 144 * sizeof(sithCollisionEntry)); // sizeof(sithCollision_collisionHandlers)
    _memset(sithCollision_funcList, 0, 12 * sizeof(int)); // sizeof(sithCollision_funcList)
    sithCollision_RegisterCollisionHandler(SITH_THING_ACTOR, SITH_THING_ACTOR, sithActor_ActorActorCollide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_ACTOR, SITH_THING_PLAYER, sithActor_ActorActorCollide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_ACTOR, SITH_THING_COG, sithActor_ActorActorCollide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_PLAYER, SITH_THING_PLAYER, sithCollision_DebrisDebrisCollide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_PLAYER, SITH_THING_COG, sithCollision_DebrisDebrisCollide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_DEBRIS, SITH_THING_ACTOR, sithCollision_DebrisPlayerCollide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_DEBRIS, SITH_THING_PLAYER, sithCollision_DebrisPlayerCollide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_DEBRIS, SITH_THING_DEBRIS, sithCollision_DebrisDebrisCollide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_WEAPON, SITH_THING_ACTOR, sithWeapon_Collide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_WEAPON, SITH_THING_PLAYER, sithWeapon_Collide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_WEAPON, SITH_THING_DEBRIS, sithWeapon_Collide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_WEAPON, SITH_THING_COG, sithWeapon_Collide, 0);
    sithCollision_RegisterCollisionHandler(SITH_THING_ITEM, SITH_THING_PLAYER, sithItem_Collide, 0);

    sithCollision_RegisterHitHandler(SITH_THING_ACTOR, (void*)sithActor_sub_4ED1D0);
    sithCollision_RegisterHitHandler(SITH_THING_WEAPON, sithWeapon_HitDebug);

#ifdef PUPPET_PHYSICS
	sithCollision_RegisterCollisionHandler(SITH_THING_CORPSE, SITH_THING_PLAYER, sithCollision_DebrisDebrisCollide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_CORPSE, SITH_THING_CORPSE, sithCollision_DebrisDebrisCollide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_CORPSE, SITH_THING_ACTOR, sithCollision_DebrisDebrisCollide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_CORPSE, SITH_THING_WEAPON, sithCollision_DebrisDebrisCollide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_PLAYER, SITH_THING_CORPSE, sithCollision_DebrisDebrisCollide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_ACTOR, SITH_THING_CORPSE, sithCollision_DebrisDebrisCollide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_WEAPON, SITH_THING_CORPSE, sithWeapon_Collide, 0);
#endif

#ifdef RAGDOLLS
	sithCollision_RegisterCollisionHandler(SITH_THING_CORPSE, SITH_THING_PLAYER, sithCorpse_Collide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_CORPSE, SITH_THING_ACTOR, sithCorpse_Collide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_CORPSE, SITH_THING_WEAPON, sithCorpse_Collide, 0);

	sithCollision_RegisterCollisionHandler(SITH_THING_PLAYER, SITH_THING_CORPSE, sithCorpse_Collide, 0);
	sithCollision_RegisterCollisionHandler(SITH_THING_ACTOR, SITH_THING_CORPSE, sithCorpse_Collide, 0);

	// actual weapon collision looks better but it's actually kinda annoying during gameplay
	// because the projectiles collide with the things bounding sphere, often in mid-air while trying
	// to shoot at other targets behind the body
	sithCollision_RegisterCollisionHandler(SITH_THING_WEAPON, SITH_THING_CORPSE, sithWeapon_Collide, 0);
	//sithCollision_RegisterCollisionHandler(SITH_THING_WEAPON, SITH_THING_CORPSE, sithCorpse_Collide, 0);
#endif

    sithCollision_initted = 1;
    return 1;
}

int sithCollision_Shutdown()
{
    int result; // eax

    result = sithCollision_initted;
    if ( sithCollision_initted )
        sithCollision_initted = 0;
    return result;
}

void sithCollision_RegisterCollisionHandler(int type1, int type2, void* pProcessFunc, void* a4)
{
    int idx = type2 + 12 * type1;
    sithCollision_collisionHandlers[idx].handler = pProcessFunc;
    sithCollision_collisionHandlers[idx].search_handler = a4;
    sithCollision_collisionHandlers[idx].inverse = 0;
    if ( type1 != type2 )
    {
        idx = type1 + 12 * type2;
        sithCollision_collisionHandlers[idx].handler = pProcessFunc;
        sithCollision_collisionHandlers[idx].search_handler = a4;
        sithCollision_collisionHandlers[idx].inverse = 1;
    }
}

void sithCollision_RegisterHitHandler(int type, void* a2)
{
    sithCollision_funcList[type] = a2;
}

sithCollisionSearchEntry* sithCollision_NextSearchResult()
{
    sithCollisionSearchEntry* retVal = NULL;
    float maxDist = 3.4e38;
    
    for (int i = 0; i < sithCollision_searchNumResults[sithCollision_searchStackIdx]; i++)
    {
        sithCollisionSearchEntry* iter = &sithCollision_searchStack[sithCollision_searchStackIdx].collisions[i];
        if ( !iter->hasBeenEnumerated )
        {
            if ( maxDist <= iter->distance )
            {
                if ( maxDist == iter->distance && retVal->hitType & (SITHCOLLISION_THINGTOUCH | SITHCOLLISION_THINGCROSS) && iter->hitType & SITHCOLLISION_THINGADJOINCROSS ) // TODO enums
                    retVal = iter;
            }
            else
            {
                maxDist = iter->distance;
                retVal = iter;
            }
        }
    }

    if ( retVal )
    {
        retVal->hasBeenEnumerated = 1;
        return retVal;
    }
    else
    {
        sithCollision_searchNumResults[sithCollision_searchStackIdx] = 0;
        sithCollision_stackIdk[sithCollision_searchStackIdx] = 0;
        return NULL;
    }
}

float sithCollision_SearchRadiusForThings(sithSector *pStartSector, sithThing *pThing, const rdVector3 *pStartPos, const rdVector3 *pMoveNorm, float moveDist, float radius, int flags)
{
    sithCollisionSearchEntry *i; // ebp
    sithSector *pSurfAdjSector; // esi
    unsigned int num; // eax
    unsigned int chk; // edi
    unsigned int v17; // edx
    unsigned int v18; // ebp
    sithSector *j; // eax
    sithAdjoin *pAdjoin; // ebx
    sithSector *pAdjoinSector; // esi
    sithSector *v24; // edx
    unsigned int v26; // [esp+10h] [ebp-8h]
    float curMoveDist; // [esp+2Ch] [ebp+14h]


    sithCollision_searchStackIdx++;
    sithCollision_searchNumResults[sithCollision_searchStackIdx] = 0;
    sithCollision_stackIdk[sithCollision_searchStackIdx] = 1;
    curMoveDist = moveDist;
    sithCollision_stackSectors[sithCollision_searchStackIdx].sectors[0] = pStartSector;

    if (!pStartSector) {
        jk_printf("OpenJKDF2 WARN: sithCollision_SearchRadiusForThings received NULL pStartSector!\n");
        return 0.0f;
    }

    if ( (flags & SITH_RAYCAST_IGNORE_THINGS) == 0 )
        curMoveDist = sithCollision_UpdateSectorThingCollision(pStartSector, pThing, pStartPos, pMoveNorm, moveDist, radius, flags);
    sithCollision_BuildCollisionList(pStartSector, pStartPos, pMoveNorm, curMoveDist, radius, flags);

    v26 = 0;
    for ( i = sithCollision_searchStack[sithCollision_searchStackIdx].collisions; v26 < sithCollision_searchNumResults[sithCollision_searchStackIdx]; ++v26 )
    {
        if ( i->hitType == SITHCOLLISION_ADJOINTOUCH )
        {
            if ( (flags & RAYCAST_400) != 0 || i->distance <= (double)curMoveDist )
            {
                pSurfAdjSector = i->surface->adjoin->sector;
                num = sithCollision_stackIdk[sithCollision_searchStackIdx];
                for (chk = 0; chk < num; chk++)
                {
                    if ( sithCollision_stackSectors[sithCollision_searchStackIdx].sectors[chk] == pSurfAdjSector )
                        break;
                }
                
                if ( chk >= num && num != 64)
                {
                    sithCollision_stackIdk[sithCollision_searchStackIdx] = num + 1;
                    sithCollision_stackSectors[sithCollision_searchStackIdx].sectors[num] = pSurfAdjSector;
                    if ( (flags & SITH_RAYCAST_IGNORE_THINGS) == 0 )
                        curMoveDist = sithCollision_UpdateSectorThingCollision(pSurfAdjSector, pThing, pStartPos, pMoveNorm, curMoveDist, radius, flags);
					sithCollision_BuildCollisionList(pSurfAdjSector, pStartPos, pMoveNorm, curMoveDist, radius, flags);
                }
            }
            i->hasBeenEnumerated = 1;
        }
        ++i;
    }
    if ( curMoveDist != 0.0 && (flags & RAYCAST_800) != 0 )
    {
        v17 = sithCollision_stackIdk[sithCollision_searchStackIdx];
        for (v18 = 0; v18 < v17; v18++)
        {
            j = sithCollision_stackSectors[sithCollision_searchStackIdx].sectors[v18];
            for (pAdjoin = j->adjoins; pAdjoin != NULL; pAdjoin = pAdjoin->next)
            {
                if (!(pAdjoin->flags & SITHSURF_ADJOIN_ALLOW_MOVEMENT)) continue;

                pAdjoinSector = pAdjoin->sector;
                if (!pAdjoinSector->thingsList) continue;
                
                num = sithCollision_stackIdk[sithCollision_searchStackIdx];
                for (chk = 0; chk < num; chk++)
                {
                    v24 = sithCollision_stackSectors[sithCollision_searchStackIdx].sectors[chk];
                    if ( v24 == pAdjoinSector )
                        break;
                }

                if (chk >= num && num != 64)
                {
                    sithCollision_stackIdk[sithCollision_searchStackIdx] = num + 1;
                    sithCollision_stackSectors[sithCollision_searchStackIdx].sectors[num] = pAdjoinSector;
                    curMoveDist = sithCollision_UpdateSectorThingCollision(pAdjoinSector, pThing, pStartPos, pMoveNorm, curMoveDist, radius, flags);
                }
            }
        }
    }
    return curMoveDist;
}

void sithCollision_SearchClose()
{
    --sithCollision_searchStackIdx;
}

float sithCollision_UpdateSectorThingCollision(sithSector *pSector, sithThing *sender, const rdVector3 *a2, const rdVector3 *a3, float a4, float range, int flags)
{
    sithThing *v7; // esi
    sithThing *v8; // ebp
    int v9; // ebx
    int v10; // eax
    sithThing *v13; // ecx
    sithThing *v14; // eax
    sithThing *v15; // ecx
    sithThing *v16; // eax
    int v19; // eax
    rdFace *v21; // ebx
    int v22; // edx
    float v23; // st7
    sithCollisionSearchEntry *v24; // ecx
    rdMesh *senderMesh; // edx
    sithCollision_searchHandler_t handler;
    int v27; // eax
    rdFace *a10; // [esp+4h] [ebp-18h] BYREF
    rdVector3 a11; // [esp+10h] [ebp-Ch] BYREF

    senderMesh = 0;
    a10 = 0;
    v7 = pSector->thingsList;
    if ( v7 )
    {
        v8 = sender;
        v10 = flags & RAYCAST_8;
        while (1)
        {
            if ( (!v10 || (v7->thingflags & SITH_TF_80))
              && ((flags & SITH_RAYCAST_IGNORE_FLOOR) == 0 || (v7->thingflags & SITH_TF_STANDABLE) != 0)
              && v7->collide
              && (v7->thingflags & (SITH_TF_DISABLED|SITH_TF_WILLBEREMOVED)) == 0
              && ((flags & SITH_RAYCAST_ONLY_COG_THINGS) == 0 || v7->type == SITH_THING_COG)
			#if defined(RAGDOLLS) || defined(PUPPET_PHYSICS)
			  && ((flags & SITH_RAYCAST_IGNORE_CORPSES) == 0 || v7->type != SITH_THING_CORPSE)
			  && (!v7->constraintParent || v7->constraintParent != v8)
			#endif
			)
            {
                if ( !v8 )
                    goto LABEL_41;
                if ( v8 != v7 )
                {
			#ifdef PUPPET_PHYSICS
					//int jointIdx = (v8->thingIdx >> 16);
					//if(jointIdx < 0 || v8->prev_thing != v7)
			#endif
                    if ( sithCollision_collisionHandlers[12 * v8->type + v7->type].handler )
                    {
					#ifdef RAGDOLLS
						if (((v8->thingflags & SITH_TF_DEAD) == 0 || v8->rdthing.pRagdoll)
							&& ((v7->thingflags & SITH_TF_DEAD) == 0 || v7->rdthing.pRagdoll)
					#else
                        if ( (v8->thingflags & SITH_TF_DEAD) == 0
                          && (v7->thingflags & SITH_TF_DEAD) == 0
					#endif
                          && (v8->type != SITH_THING_WEAPON
                           || (v8->actorParams.typeflags & SITH_AF_CAN_ROTATE_HEAD) == 0
                           || ((v13 = v8->prev_thing) == 0 || (v14 = v7->prev_thing) == 0 || v13 != v14 || v8->child_signature != v7->child_signature)
                           && (v13 != v7 || v8->child_signature != v7->signature))
                          && (v7->type != SITH_THING_WEAPON
                           || (v7->actorParams.typeflags & SITH_AF_CAN_ROTATE_HEAD) == 0
                           || ((v15 = v7->prev_thing) == 0 || (v16 = v8->prev_thing) == 0 || v15 != v16 || v7->child_signature != v8->child_signature)
                           && (v15 != v8 || v7->child_signature != v8->signature)) 
#ifdef RAGDOLLS
							// don't collide ragdolls with parent object
							&& (!v8->rdthing.pRagdoll || v8->prev_thing == 0 || v7->prev_thing == 0 || v8->prev_thing != v7->prev_thing || v8->child_signature != v7->child_signature)
							&& (!v7->rdthing.pRagdoll || v7->prev_thing == 0 || v8->prev_thing == 0 || v7->prev_thing != v8->prev_thing || v7->child_signature != v8->child_signature)
#endif
#if defined(RAGDOLLS) || defined(PUPPET_PHYSICS)
							&& ((flags & SITH_RAYCAST_IGNORE_CORPSES) == 0 || v8->type != SITH_THING_CORPSE)
							&& (!v8->constraintParent || v8->constraintParent != v7)
#endif
						)
                        {
                            if ( (v8->attach_flags & (SITH_ATTACH_THINGSURFACE | SITH_ATTACH_THING)) == 0 || v8->attachedThing != v7 || (v8->attach_flags & SITH_ATTACH_NO_MOVE) == 0 && (flags & RAYCAST_40) == 0 )
                            {
                                if ( (v7->attach_flags & (SITH_ATTACH_THINGSURFACE | SITH_ATTACH_THING)) == 0 || v7->attachedThing != v8 || (v7->attach_flags & SITH_ATTACH_NO_MOVE) == 0 && (flags & RAYCAST_40) == 0 )
                                {
LABEL_41:
                                    v19 = sithIntersect_CollideThings(v8, a2, a3, a4, range, v7, flags, &v23, &senderMesh, &a10, &a11);
                                    if ( v19 )
                                    {
                                        v21 = a10;
                                        v22 = sithCollision_searchNumResults[sithCollision_searchStackIdx];
                                        if ( v22 != 128 )
                                        {
                                            v19 |= SITHCOLLISION_THING;
                                            sithCollision_searchNumResults[sithCollision_searchStackIdx] = v22 + 1;
                                            v24 = &sithCollision_searchStack[sithCollision_searchStackIdx].collisions[v22];
                                            v24->surface = 0;
                                            v24->hasBeenEnumerated = 0;
                                            v24->hitType = v19;
                                            v24->distance = v23;
                                            v24->receiver = v7;
                                            v24->sender = senderMesh;
                                            v24->face = v21;
                                            rdVector_Copy3(&v24->hitNorm, &a11);
                                        }
                                        if ( v8 )
                                        {
                                            handler = sithCollision_collisionHandlers[12 * v8->type + v7->type].search_handler;
                                            if ( handler )
                                                v27 = handler(v8, v7);
                                            else
                                                v27 = 0;
                                            if ( v27 )
                                                a4 = v23;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // Added: Prevent deadlocks in some conditions
            if (v7->nextThing == v7) break;
            v7 = v7->nextThing;
            if ( !v7 )
                break;
        }
    }
    return a4;
}

void sithCollision_BuildCollisionList(sithSector *sector, const rdVector3 *vec1, const rdVector3 *vec2, float a4, float a5, int unk3Flags)
{
    sithSurface *v12; // esi
    sithAdjoin *v15; // eax
    unsigned int v17; // ecx
    unsigned int v18; // edi
    sithSector **v19; // eax
    int v20; // ecx
    double v21; // st7
    sithCollisionSearchEntry *v23; // eax
    int v24; // ecx
    unsigned int v25; // edi
    unsigned int v26; // edx
    sithSector **v27; // eax
    int v28; // edx
    double v29; // st7
    sithCollisionSearchEntry *v31; // eax
    int v32; // edx
    double v33; // st7
    sithCollisionSearchEntry *v34; // eax
    rdVector3 *v35; // ecx
    int v36; // ecx
    double v37; // st7
    int v38; // edx
    sithCollisionSearchEntry *v40; // eax
    int v42; // [esp+0h] [ebp-40h] BYREF
    float a7; // [esp+10h] [ebp-30h] BYREF
    int v47; // [esp+20h] [ebp-20h]
    float v48; // [esp+24h] [ebp-1Ch] BYREF
    rdVector3 pushVel; // [esp+34h] [ebp-Ch] BYREF
    rdVector3 tmp;
    
    // Added: nullptr check
    //if (!sector) return;

    rdVector_Copy3(&tmp, vec1);
    rdVector_MultAcc3(&tmp, vec2, a4);

    if(sithIntersect_IsSphereInSectorBox(&tmp, a5, sector))
    {
        return;
    }

    for (v47 = 0; v47 < sector->numSurfaces; v47++)
    {
        v12 = &sector->surfaces[v47];
        v15 = v12->adjoin;
        if ( (v12->surfaceFlags & SITH_SURFACE_HAS_COLLISION) == 0 && !v15 )
            continue;

        if ( !v15 )
        {
LABEL_46:
            if ( (unk3Flags & RAYCAST_4) == 0 && ((unk3Flags & SITH_RAYCAST_IGNORE_FLOOR) == 0 || (v12->surfaceFlags & SITH_SURFACE_FLOOR) != 0) )
            {
                v35 = sithWorld_pCurrentWorld->vertices;
                
                if ( rdMath_DistancePointToPlane(&tmp, &v12->surfaceInfo.face.normal, &v35[*v12->surfaceInfo.face.vertexPosIdx]) <= a5 )
                {
                    v36 = sithIntersect_sub_508D20(vec1, vec2, a4, a5, &v12->surfaceInfo.face, v35, &a7, &pushVel, unk3Flags);
                    if ( v36 )
                    {
                        if ( (unk3Flags & RAYCAST_400) != 0 || rdVector_Dot3(vec2, &pushVel) < 0.0 )
                        {
                            v37 = a7;
                            v38 = sithCollision_searchNumResults[sithCollision_searchStackIdx];
                            if ( v38 != 128 )
                            {
                                sithCollision_searchNumResults[sithCollision_searchStackIdx] = v38 + 1;
                                v40 = &sithCollision_searchStack[sithCollision_searchStackIdx].collisions[v38];
                                v40->receiver = 0;
                                v40->hasBeenEnumerated = 0;
                                v40->hitType = v36 | SITHCOLLISION_WORLD;
                                v40->distance = v37;
                                v40->surface = v12;
                                if ( &v42 != (int *)-52 )
                                    v40->hitNorm = pushVel;
                            }
                        }
                    }
                }
            }
            continue;
        }

        if ( (unk3Flags & RAYCAST_4) == 0 )
        {
            if ( (unk3Flags & (RAYCAST_1000 | RAYCAST_100)) != 0 && (v15->flags & SITHSURF_ADJOIN_VISIBLE) == 0 )
                goto LABEL_46;
            if ( (unk3Flags & RAYCAST_200) != 0 )
            {
                if ( (unk3Flags & RAYCAST_100) != 0 )
                    goto LABEL_22;
                if ( (v15->flags & SITHSURF_ADJOIN_ALLOW_AI_ONLY) != 0 )
                    goto LABEL_46;
            }
            if ( (unk3Flags & RAYCAST_100) == 0 && (v15->flags & SITHSURF_ADJOIN_ALLOW_MOVEMENT) == 0 )
                goto LABEL_46;
        }
LABEL_22:
        // Standing?
        if ( sithIntersect_sub_5090B0(vec1, vec2, a4, a5, &v12->surfaceInfo, sithWorld_pCurrentWorld->vertices, &a7, unk3Flags) )
        {
            if ( !(unk3Flags & RAYCAST_4) || (unk3Flags & SITH_RAYCAST_IGNORE_THINGS) == 0 )
            {
                v17 = 0;
                v18 = sithCollision_stackIdk[sithCollision_searchStackIdx];
                if ( v18 )
                {
                    v19 = sithCollision_stackSectors[sithCollision_searchStackIdx].sectors;
                    while ( *v19 != v15->sector )
                    {
                        ++v17;
                        ++v19;
                        if ( v17 >= v18 )
                        {
                            goto LABEL_30;
                        }
                    }
                }
                else
                {
LABEL_30:
                    v20 = sithCollision_searchNumResults[sithCollision_searchStackIdx];
                    v21 = a7;
                    if ( v20 != 128 )
                    {
                        sithCollision_searchNumResults[sithCollision_searchStackIdx] = v20 + 1;
                        v23 = &sithCollision_searchStack[sithCollision_searchStackIdx].collisions[v20];
                        v23->receiver = 0;
                        v23->hasBeenEnumerated = 0;
                        v23->hitType = SITHCOLLISION_ADJOINTOUCH;
                        v23->distance = v21;
                        v23->surface = v12;
                    }
                }
            }

            // Falling?
            if ( (unk3Flags & SITH_RAYCAST_IGNORE_ADJOINS) == 0 && sithIntersect_sub_5090B0(vec1, vec2, a4, 0.0, &v12->surfaceInfo, sithWorld_pCurrentWorld->vertices, &v48, unk3Flags) )
            {
                v24 = sithCollision_searchStackIdx;
                if ( (unk3Flags & RAYCAST_4) && (unk3Flags & SITH_RAYCAST_IGNORE_THINGS) != 0 )
                {
                    v25 = sithCollision_stackIdk[sithCollision_searchStackIdx];
                    v26 = 0;
                    if ( v25 )
                    {
                        v27 = sithCollision_stackSectors[sithCollision_searchStackIdx].sectors;
                        while ( *v27 != v15->sector )
                        {
                            ++v26;
                            ++v27;
                            if ( v26 >= v25 )
                                goto LABEL_42;
                        }
                    }
                    else
                    {
LABEL_42:
                        v28 = sithCollision_searchNumResults[sithCollision_searchStackIdx];
                        v29 = a7;
                        if ( v28 != 128 )
                        {
                            sithCollision_searchNumResults[sithCollision_searchStackIdx] = v28 + 1;
                            v31 = &sithCollision_searchStack[sithCollision_searchStackIdx].collisions[v28];
                            v31->receiver = 0;
                            v31->hasBeenEnumerated = 0;
                            v31->hitType = SITHCOLLISION_ADJOINTOUCH;
                            v31->distance = v29;
                            v31->surface = v12;
                        }
                    }
                }
                v32 = sithCollision_searchNumResults[v24];
                v33 = v48;
                if ( v32 != 128 )
                {
                    sithCollision_searchNumResults[v24] = v32 + 1;
                    v34 = &sithCollision_searchStack[sithCollision_searchStackIdx].collisions[v32];
                    v34->receiver = 0;
                    v34->hasBeenEnumerated = 0;
                    v34->hitType = SITHCOLLISION_ADJOINCROSS;
                    v34->distance = v33;
                    v34->surface = v12;
                }
            }
        }
    }
}

sithSector* sithCollision_GetSectorLookAt(sithSector *pStartSector, const rdVector3 *pStartPos, rdVector3 *pEndPos, float a5)
{
    double v4; // st6
    sithSector *result; // eax
    int v7; // edi
    sithCollisionSearchResult *v8; // ebx
    sithCollisionSearchEntry *v9; // edx
    double v10; // st7
    sithCollisionSearchEntry *v11; // ecx
    int v12; // esi
    rdVector3 a1; // [esp+8h] [ebp-Ch] BYREF
    float a3a; // [esp+1Ch] [ebp+8h]

    if ( sithIntersect_IsSphereInSector(pEndPos, 0.0, pStartSector) )
        return pStartSector;
    rdVector_Sub3(&a1, pEndPos, pStartPos);
    a3a = rdVector_Normalize3Acc(&a1);
    sithCollision_SearchRadiusForThings(pStartSector, 0, pStartPos, &a1, a3a, a5, SITH_RAYCAST_IGNORE_THINGS);
    v7 = sithCollision_searchStackIdx;
    v8 = &sithCollision_searchStack[sithCollision_searchStackIdx];
    while ( 1 )
    {
        v9 = 0;
        v10 = 3.4e38;
        v11 = (sithCollisionSearchEntry *)v8;
        if ( sithCollision_searchNumResults[v7] )
        {
            v12 = sithCollision_searchNumResults[v7];
            do
            {
                if ( !v11->hasBeenEnumerated )
                {
                    if ( v10 <= v11->distance )
                    {
                        if ( v10 == v11->distance && (v9->hitType & (SITHCOLLISION_THINGTOUCH | SITHCOLLISION_THINGCROSS)) != 0 && (v11->hitType & 4) != 0 )
                            v9 = v11;
                    }
                    else
                    {
                        v10 = v11->distance;
                        v9 = v11;
                    }
                }
                ++v11;
                --v12;
            }
            while ( v12 );
        }
        if ( v9 )
        {
            v9->hasBeenEnumerated = 1;
        }
        else
        {
            sithCollision_searchNumResults[v7] = 0;
            sithCollision_stackIdk[v7] = 0;
        }
        if ( !v9 )
            break;
        if ( (v9->hitType & SITHCOLLISION_ADJOINCROSS) == 0 )
        {
            rdVector_Copy3(pEndPos, pStartPos);
            rdVector_MultAcc3(pEndPos, &a1, v9->distance);
            break;
        }
        pStartSector = v9->surface->adjoin->sector;
    }
    result = pStartSector;
    sithCollision_searchStackIdx = v7 - 1;
    return result;
}

void sithCollision_FallHurt(sithThing *thing, float vel)
{
    double v2; // st7

    v2 = (vel - 2.5) * (vel - 2.5) * 45.0;
    if ( v2 > 1.0 )
    {
        sithSoundClass_PlayModeRandom(thing, SITH_SC_HITDAMAGED);
        sithThing_Damage(thing, thing, v2, SITH_DAMAGE_FALL, -1);
    }
}



void sithCollision_sub_4E7670(sithThing *thing, rdMatrix34 *orient)
{
    sithThing *i; // esi
    rdVector3 a1a; // [esp+18h] [ebp-Ch] BYREF
    rdVector3 tmp;

    rdMatrix_PreMultiply34(&thing->lookOrientation, orient);
    for ( i = thing->attachedParentMaybe; i; i = i->childThing )
    {
        rdVector_Sub3(&tmp, &i->position, &thing->position);
        rdVector_Copy3(&i->lookOrientation.scale, &tmp);
        sithCollision_sub_4E7670(i, orient);
        if ( (i->attach_flags & SITH_ATTACH_NO_MOVE) == 0 )
        {
            rdVector_Sub3(&a1a, &i->lookOrientation.scale, &tmp);
            if ( !rdVector_IsZero3(&a1a) )
            {
                sithCollision_UpdateThingCollision(i, &a1a, rdVector_Normalize3Acc(&a1a), 0);
            }
        }
        rdVector_Zero3(&i->lookOrientation.scale);
    }

#ifdef PUPPET_PHYSICS
	//if (i = thing->constraintThing)
	//{
	//	rdVector_Sub3(&tmp, &i->position, &thing->position);
	//	rdVector_Copy3(&i->lookOrientation.scale, &tmp);
	//
	//	//sithCollision_ConeConstrain(i, thing);
	//	sithCollision_sub_4E7670(i, orient);
	//
	//	rdVector_Sub3(&a1a, &i->lookOrientation.scale, &tmp);
	//	if (!rdVector_IsZero3(&a1a))
	//	{
	//		sithCollision_UpdateThingCollision(i, &a1a, rdVector_Normalize3Acc(&a1a), 0);
	//	}
	//	rdVector_Zero3(&i->lookOrientation.scale);
	//}
#endif
}
#ifdef RIGID_BODY

void sithCollision_ConstrainThings(sithThing* thing1, sithThing* thing2, float distance)
{
	rdVector3 delta;
	rdVector_Sub3(&delta, &thing1->position, &thing2->position);
	rdVector_ClipPrecision3(&delta);

	float deltaLen = rdVector_Normalize3Acc(&delta);
	float diff = (deltaLen - distance);
	if(diff < 0.0)
	{
		rdVector_Neg3Acc(&delta);
		diff = -diff;
	}

	if (fabs(diff) > 0.00001f)
	{
		sithCollision_UpdateThingCollision(thing2, &delta, diff, 0);
	}

	// compute the difference in mass, used to adjust the positions
	//float invMassA = 1.0f / (thing1->physicsParams.mass);
	//float invMassB = 1.0f / (thing2->physicsParams.mass);
	//float diff = (deltaLen - distance) / (invMassA + invMassB);
	//if (fabs(diff) > 0.00001f)
	//{
	//	sithCollision_UpdateThingCollision(thing2, &delta, diff * invMassB, 0); 
	//
	//	rdVector_Neg3Acc(&delta);
	//	sithCollision_UpdateThingCollision(thing1, &delta, diff * invMassA, SITH_RAYCAST_NO_CONSTRAINT_UPDATE);
	//}
}

void compute_angular_velocity_from_rotation(rdVector3* angVel, const rdMatrix34* R_rel);
void apply_friction_to_rotational_impulse(const rdVector3* omega_rel, float friction_coefficient,
										  rdVector3* impulseA, rdVector3* impulseB);

void rdMatrix_TransformEulerPYRToWorld(rdVector3* result, const rdVector3* eulerPYR, const rdMatrix34* orientation)
{
	// Local angular velocity in axis terms
	rdVector3 pitchAxis = { 1, 0, 0 };
	rdVector3 yawAxis = { 0, 0, 1 };
	rdVector3 rollAxis = { 0, 1, 0 };

	// Scale by respective angular velocities
	rdVector_Scale3(&pitchAxis, &pitchAxis, eulerPYR->x * (M_PI / 180.0f)); // Convert to radians
	rdVector_Scale3(&yawAxis, &yawAxis, eulerPYR->y * (M_PI / 180.0f));     // Convert to radians
	rdVector_Scale3(&rollAxis, &rollAxis, eulerPYR->z * (M_PI / 180.0f));   // Convert to radians

	// Transform each axis to world space
	rdVector3 pitchWorld, yawWorld, rollWorld;
	rdMatrix_TransformVector34(&pitchWorld, &pitchAxis, orientation);
	rdMatrix_TransformVector34(&yawWorld, &yawAxis, orientation);
	rdMatrix_TransformVector34(&rollWorld, &rollAxis, orientation);

	// Combine them into a single world angular velocity vector
	rdVector_Add3(result, &pitchWorld, &rollWorld);
	rdVector_Add3Acc(result, &yawWorld);
}

void ssithCollision_ApplyConstraint(sithThing* pParent, sithThing* pChild, sithConstraintResult* result, float deltaSeconds)
{
	result->C = stdMath_ClipPrecision(result->C);
	if (stdMath_Fabs(result->C) > 0.0)
	{
		float invMassA = 1.0f / pParent->physicsParams.mass;
		float invMassB = 1.0f / pChild->physicsParams.mass;

		float invInertiaTensorA = 1.0f / pParent->physicsParams.inertia;
		float invInertiaTensorB = 1.0f / pChild->physicsParams.inertia;

		//rdVector3 JrArad, JrBrad;
		//rdVector_Scale3(&JrArad, &result->JrA, M_PI / 180.0f);
		//rdVector_Scale3(&JrBrad, &result->JrB, M_PI / 180.0f);

		float JWJT = invMassA + invMassB +
			invInertiaTensorA * rdVector_Dot3(&result->JrA, &result->JrA) +
			invInertiaTensorB * rdVector_Dot3(&result->JrB, &result->JrB);

		float Me = 1.0f / JWJT;// sithConstraint_ComputeEffectiveMass(&result->JrA, &result->JrB, pParent, pChild);

		float lambda = result->C * Me;
		lambda = stdMath_ClipPrecision(lambda);

		float damping = (1.0f - 0.f);

		// calculate linear impulses
		rdVector3 impulseLinearA, impulseLinearB;
		rdVector_Scale3(&impulseLinearA, &result->JvA, lambda * invMassA * damping);
		rdVector_Scale3(&impulseLinearB, &result->JvB, lambda * invMassB * damping);

		// calculate angular impulses
		rdVector3 impulseAngularA, impulseAngularB;
		rdVector_Scale3(&impulseAngularA, &result->JrA, lambda * invInertiaTensorA * damping);
		rdVector_Scale3(&impulseAngularB, &result->JrB, lambda * invInertiaTensorB * damping);

		//if(jkPlayer_puppetFriction > 0.0)
		//{
		//	sithPhysics_ApplyDrag(&impulseAngularA, jkPlayer_puppetFriction, 0.0f, deltaSeconds);
		//	sithPhysics_ApplyDrag(&impulseAngularB, jkPlayer_puppetFriction, 0.0f, deltaSeconds);
		//	sithPhysics_ApplyDrag(&impulseLinearA, jkPlayer_puppetFriction, 0.0f, deltaSeconds);
		//	sithPhysics_ApplyDrag(&impulseLinearB, jkPlayer_puppetFriction, 0.0f, deltaSeconds);
		//}

		// friction
		if(jkPlayer_puppetFriction > 0.0)
		{
			rdMatrix34 I;
			rdMatrix_InvertOrtho34(&I, &pParent->lookOrientation);
			
			rdMatrix34 R_rel;
			rdMatrix_Multiply34(&R_rel, &I, &pChild->lookOrientation);
			
			//// Subtract identity matrix from R_rel
			//R_rel.rvec.x -= 1.0f;
			//R_rel.lvec.y -= 1.0f;
			//R_rel.uvec.z -= 1.0f;
			//
			//rdVector3 omega_rel;
			//omega_rel.x = R_rel.uvec.y - R_rel.lvec.z;
			//omega_rel.y = R_rel.rvec.z - R_rel.uvec.x;
			//omega_rel.z = R_rel.lvec.x - R_rel.rvec.y;
			//
			//// Scale by 1/deltaTime to get angular velocity
			//omega_rel.x /= 2.0f * deltaSeconds;
			//omega_rel.y /= 2.0f * deltaSeconds;
			//omega_rel.z /= 2.0f * deltaSeconds;

			rdVector3 omega_rel;
			compute_angular_velocity_from_rotation(&omega_rel, &R_rel);

			//rdVector3 angVelWS_A, angVelWS_B;
			//rdMatrix_TransformEulerPYRToWorld(&angVelWS_A, &pParent->physicsParams.angVel, &pParent->lookOrientation);
			//rdMatrix_TransformEulerPYRToWorld(&angVelWS_B, &pChild->physicsParams.angVel, &pChild->lookOrientation);
			//
			//rdVector3 omega_rel;
			//rdVector_Sub3(&omega_rel, &angVelWS_B, &angVelWS_A);

			apply_friction_to_rotational_impulse(&omega_rel, jkPlayer_puppetFriction, &impulseAngularA, &impulseAngularB);
		}

		rdVector_Scale3Acc(&impulseLinearA, sithTime_deltaSeconds);
		rdVector_Scale3Acc(&impulseLinearB, sithTime_deltaSeconds);
		rdVector_Scale3Acc(&impulseAngularA, sithTime_deltaSeconds);
		rdVector_Scale3Acc(&impulseAngularB, sithTime_deltaSeconds);


		// apply the impulses
		{
			if (!rdVector_IsZero3(&impulseLinearA))
			{
				//rdVector_Add3Acc(&pParent->physicsParams.vel, &impulseLinearA);
				float impulseDist = rdVector_Normalize3Acc(&impulseLinearA);
				if(impulseDist > 0.0)
				{
					// ugh
					sithCollision_UpdateThingCollision(pParent, &impulseLinearA, impulseDist, 0x10000000);
				}
			}

			if (!rdVector_IsZero3(&impulseAngularA))
			{
				rdVector3 pitchAxisA = { 1, 0, 0 };  // Local X-axis
				rdVector3 yawAxisA = { 0, 0, 1 };  // Local Z-axis
				rdVector3 rollAxisA = { 0, 1, 0 };  // Local Y-axis

				rdMatrix34 invOrient;
				rdMatrix_InvertOrtho34(&invOrient, &pParent->lookOrientation);

				rdVector3 localUnitConstraint;
				rdMatrix_TransformVector34(&localUnitConstraint, &impulseAngularA, &invOrient);

				float angle = (180.0f / M_PI)* rdVector_Normalize3Acc(&localUnitConstraint);
				rdMatrix34 a;
				rdMatrix_BuildFromVectorAngle34(&a, &localUnitConstraint, angle);

				// Compute angular contributions in PYR space for child
				//rdVector3 angVel;
				//angVel.x = rdVector_Dot3(&localUnitConstraint, &pitchAxisA) * (180.0f / M_PI);  // Pitch
				//angVel.y = rdVector_Dot3(&localUnitConstraint, &yawAxisA) * (180.0f / M_PI);   // Yaw
				//angVel.z = rdVector_Dot3(&localUnitConstraint, &rollAxisA) * (180.0f / M_PI);   // Roll
				////rdVector_Add3Acc(&pParent->physicsParams.angVel, &angVel);
				//
				//rdMatrix34 a;
				//rdMatrix_BuildRotate34(&a, &angVel);
				sithCollision_sub_4E7670(pParent, &a);
				rdMatrix_Normalize34(&pParent->lookOrientation);
			}
		}
		{
			//sithCollision_UpdateThingCollision(pChild, &impulseLinearB, sithTime_deltaSeconds * rdVector_Normalize3Acc(&impulseLinearB), 0x10000000);

			//rdVector_Add3Acc(&pChild->physicsParams.angVel, &impulseAngularB);

			if (!rdVector_IsZero3(&impulseLinearB))
			{
				//rdVector_Add3Acc(&pChild->physicsParams.vel, &impulseLinearB);
				float impulseDist = rdVector_Normalize3Acc(&impulseLinearB);
				if (impulseDist > 0.0)
				{
					// ugh
					sithCollision_UpdateThingCollision(pChild, &impulseLinearB, impulseDist, 0x10000000);
				}
			}

			if (!rdVector_IsZero3(&impulseAngularB))
			{
				rdVector3 pitchAxisA = { 1, 0, 0 };  // Local X-axis
				rdVector3 yawAxisA = { 0, 0, 1 };  // Local Z-axis
				rdVector3 rollAxisA = { 0, 1, 0 };  // Local Y-axis

				rdMatrix34 invOrient;
				rdMatrix_InvertOrtho34(&invOrient, &pChild->lookOrientation);

				rdVector3 localUnitConstraint;
				rdMatrix_TransformVector34(&localUnitConstraint, &impulseAngularB, &invOrient);

				float angle = (180.0f / M_PI) * rdVector_Normalize3Acc(&localUnitConstraint);
				rdMatrix34 a;
				rdMatrix_BuildFromVectorAngle34(&a, &localUnitConstraint, angle);

				// Compute angular contributions in PYR space for child
				//rdVector3 angVel;
				//angVel.x = rdVector_Dot3(&localUnitConstraint, &pitchAxisA) * (180.0f / M_PI);  // Pitch
				//angVel.y = rdVector_Dot3(&localUnitConstraint, &yawAxisA) * (180.0f / M_PI);   // Yaw
				//angVel.z = rdVector_Dot3(&localUnitConstraint, &rollAxisA) * (180.0f / M_PI);   // Roll
				////rdVector_Add3Acc(&pChild->physicsParams.angVel, &angVel);
				//
				//rdMatrix34 a;
				//rdMatrix_BuildRotate34(&a, &angVel);
				sithCollision_sub_4E7670(pChild, &a);
				rdMatrix_Normalize34(&pChild->lookOrientation);
			}
		}
	}
}

void sithCollision_SolveDistanceConstraint(
	sithThing* pParent,
	sithThing* pChild,
	const rdVector3* pParentAnchor,
	const rdVector3* pChildAnchor,
	float constraintDistance
)
{
	// anchor A offset in world space
	rdVector3 offsetA;
	rdMatrix_TransformVector34(&offsetA, pParentAnchor, &pParent->lookOrientation);

	// anchor B offset in world space
	rdVector3 offsetB;
	rdMatrix_TransformVector34(&offsetB, pChildAnchor, &pChild->lookOrientation);

	// anchor positions in world space
	rdVector3 anchorA, anchorB;
	rdVector_Add3(&anchorA, &offsetA, &pParent->position);
	rdVector_Add3(&anchorB, &offsetB, &pChild->position);

	// vector between the anchors and the distance
	rdVector3 constraint, unitConstraint;
	rdVector_Sub3(&constraint, &anchorB, &anchorA);
	float len = rdVector_Normalize3(&unitConstraint, &constraint);

	sithConstraintResult res;
	res.C = len - constraintDistance;
	res.C = -(jkPlayer_puppetPosBias / sithTime_deltaSeconds) * res.C;

	res.JvA.x = -unitConstraint.x;
	res.JvA.y = -unitConstraint.y;
	res.JvA.z = -unitConstraint.z;

	res.JvB.x = unitConstraint.x;
	res.JvB.y = unitConstraint.y;
	res.JvB.z = unitConstraint.z;

	rdVector_Cross3(&res.JrB, &offsetB, &unitConstraint);
	rdVector_Neg3Acc(&unitConstraint);
	rdVector_Cross3(&res.JrA, &offsetA, &unitConstraint);

	ssithCollision_ApplyConstraint(pParent, pChild, &res, sithTime_deltaSeconds);
}

void sithCollision_SolveConeConstraint(sithThing* pParent,
									   sithThing* pChild,
									   const rdVector3* pConeAxis,
									   const rdVector3* pConeAnchor,
									   const rdVector3* pJointAxis,
									   float coneAngle,
									   float coneAngleCos
)
{
	rdVector3 coneAxis;
	rdMatrix_TransformVector34(&coneAxis, pConeAxis, &pParent->lookOrientation);
	rdVector_Normalize3Acc(&coneAxis);

	// cone anchor to world space
	rdVector3 coneAnchor;
	rdMatrix_TransformVector34(&coneAnchor, pConeAnchor, &pParent->lookOrientation);
	rdVector_Add3Acc(&coneAnchor, &pParent->position);

	// joint axis to world space
	rdVector3 thingAxis;
	rdMatrix_TransformVector34(&thingAxis, pJointAxis, &pChild->lookOrientation);


#if 1
	sithConstraintResult res;
	memset(&res, 0, sizeof(res));

	sithThing* bodyA = pParent;
	sithThing* bodyB = pChild;

	rdVector3 relativeAxis;
	//rdVector_Cross3(&relativeAxis, &coneAxis, &thingAxis);
	rdVector_Cross3(&relativeAxis, &thingAxis, &coneAxis);
	rdVector_Normalize3Acc(&relativeAxis);

	float dotProduct = rdVector_Dot3(&coneAxis, &thingAxis);
	dotProduct = stdMath_Clamp(dotProduct, -1.0f, 1.0f); // just in case cuz we're catching problems with NaN
	if (dotProduct >= coneAngleCos)
		return;

	float angle = acosf(dotProduct);

	rdVector3 correctionAxis;
	rdVector_Cross3(&correctionAxis, &coneAxis, &relativeAxis);
	rdVector_Normalize3Acc(&correctionAxis);

	rdVector3 JwA = relativeAxis;
	rdVector3 JwB = relativeAxis;
	rdVector_Neg3Acc(&JwA);

	res.JvA = rdroid_zeroVector3;
	res.JvB = rdroid_zeroVector3;
	res.JrA = JwA;
	res.JrB = JwB;
	res.C = coneAngleCos - dotProduct;//(angle - pConstraint->coneParams.coneAngle * (M_PI / 180.0f));

	res.C = (jkPlayer_puppetAngBias / sithTime_deltaSeconds) * res.C;

#else
	// cone axis to world space
	// calculate the angle and test for violation
	float angle = rdVector_Dot3(&coneAxis, &thingAxis);
	angle = stdMath_ClipPrecision(angle);
	if (angle > coneAngleCos)
		return;

	rdVector3 normal;
	rdVector_Cross3(&normal, &thingAxis, &coneAxis);
	rdVector_Normalize3Acc(&normal);

	rdMatrix34 qm;
	rdMatrix_BuildFromVectorAngle34(&qm, &normal, coneAngle);

	rdVector3 coneVector;
	rdMatrix_TransformVector34(&coneVector, &coneAxis, &qm);
	rdVector_Normalize3Acc(&coneVector);

	rdVector3 hitNormal;
	rdVector_Cross3(&hitNormal, &coneVector, &coneAxis);
	rdVector_Cross3(&normal, &hitNormal, &coneVector);
	rdVector_Normalize3Acc(&normal);

	rdVector3 p1 = coneAnchor;
	//rdVector_Sub3(&p1, &coneAnchor, &pConstraint->constrainedThing->position);
	//rdVector_MultAcc3(&p1, &coneVector, pConstraint->constrainedThing->moveSize);
	rdVector_Add3Acc(&p1, &coneVector);
	rdVector_Sub3Acc(&p1, &pChild->position);

//	rdVector_Neg3Acc(&normal);

	rdVector_Copy3(&res.JvB, &normal);
	rdVector_Cross3(&res.JrB, &p1, &normal);

	res.C = rdVector_Dot3(&normal, &thingAxis);// / (180.0f / M_PI);
	res.C = (jkPlayer_puppetAngBias / sithTime_deltaSeconds) * res.C;
	res.C = stdMath_ClipPrecision(res.C);

	rdVector_Neg3Acc(&normal);

	rdVector3 p2 = coneAnchor;
	//rdVector_Sub3(&p2, &coneAnchor, &pConstraint->targetThing->position);
	//rdVector_MultAcc3(&p2, &coneVector, pConstraint->targetThing->moveSize);
	rdVector_Add3Acc(&p2, &coneVector);
	rdVector_Sub3Acc(&p2, &pParent->position);

	rdVector_Copy3(&res.JvA, &normal);
	rdVector_Cross3(&res.JrA, &p2, &normal);
#endif

	ssithCollision_ApplyConstraint(pParent, pChild, &res, sithTime_deltaSeconds);
}


void sithCollision_ConstraintLoopTest(sithThing* pThing, const rdVector3* dir, float dist, int flags)
{
	if (pThing->moveType != SITH_MT_PHYSICS)
		return;

	// todo: we need iterations or this doesn't work reliably
	if (pThing->constraints)
	{
		sithConstraint* constraint = pThing->constraints;
		while (constraint)
		{
			switch (constraint->type)
			{
			case SITH_CONSTRAINT_DISTANCE:
				sithCollision_SolveDistanceConstraint(
					pThing,
					constraint->thing,
					&constraint->distanceParams.targetAnchor,
					&constraint->distanceParams.constraintAnchor,
					constraint->distanceParams.constraintDistance);
				break;
			case SITH_CONSTRAINT_CONE:
				sithCollision_SolveConeConstraint(pThing,
												  constraint->thing,
												  &constraint->coneParams.coneAxis,
												  &constraint->coneParams.coneAnchor,
												  &constraint->coneParams.jointAxis,
												  constraint->coneParams.coneAngle,
												  constraint->coneParams.coneAngleCos);
				break;
			default:
				break;
			}

			// this kinda fucks up gravity...
			//rdVector_Scale3Acc(&constraint->thing->physicsParams.vel, 0.99f);
			//rdVector_Scale3Acc(&pThing->physicsParams.vel, 0.99f);
			rdVector_Scale3Acc(&constraint->thing->physicsParams.angVel, 0.99f);
			rdVector_Scale3Acc(&pThing->physicsParams.angVel, 0.99f);

			constraint = constraint->next;
		}
	}
}
#endif

float sithCollision_UpdateThingCollision(sithThing *pThing, rdVector3 *a2, float a6, int flags)
{
    sithThing *v5; // ebp
    sithThing *v10; // esi
    double v11; // st7
    double v12; // st7
    //char v15; // c0
    int v16; // edi
    float v17; // edx
    //int v18; // edx
    sithCollisionSearchEntry *v19; // esi
    //double v20; // st7
    //sithCollisionSearchEntry *v21; // ecx
    int v22; // ebx
    double v23; // st6
    double v24; // st7
    double v25; // st7
    double v30; // st5
    sithThing *v34; // ecx
    int v35; // eax
    int v36; // eax
    sithSurface *v37; // eax
    double v44; // st7
    //char v46; // c3
    //char v49; // c0
    //char v52; // c0
    sithThing *i; // esi
    int v61; // eax
    sithSurface *amount; // [esp+0h] [ebp-54h]
    float v64; // [esp+18h] [ebp-3Ch]
    float v65; // [esp+1Ch] [ebp-38h]
    unsigned int v66; // [esp+20h] [ebp-34h]
    rdVector3 direction; // [esp+24h] [ebp-30h] BYREF
    rdVector3 posCopy;
    rdVector3 out; // [esp+3Ch] [ebp-18h] BYREF
    rdVector3 v72; // [esp+48h] [ebp-Ch] BYREF
    sithSector* sectTmp;

    v64 = 0.0;
    v65 = 0.0;
    v66 = 0;
    if ( a6 <= 0.0 )
        return 0.0;
    v5 = pThing;
    if (pThing->collide == SITH_COLLIDE_NONE)
    {
        flags |= SITH_RAYCAST_IGNORE_THINGS | RAYCAST_4;
    }
    if ( pThing->moveType == SITH_MT_PATH )
    {
        flags |= RAYCAST_4;
    }
    if ( pThing->type == SITH_THING_PLAYER )
    {
        flags |= RAYCAST_200;
    }
    if ( (flags & SITH_RAYCAST_IGNORE_THINGS) == 0 )
    {
        flags |= RAYCAST_800;
    }

#ifdef PUPPET_PHYSICS
#ifdef RIGID_BODY
	//if (pThing->type == SITH_THING_CORPSE)
		//flags |= SITH_RAYCAST_IGNORE_CORPSES; // disable collisions between corpse bodies for now
#endif

	//sithConstraint* constraint = pThing->constraints;
	//while (constraint)
	//{
	//	//sithCollision_UpdateThingCollision(constraint->thing, a2, a6, 0x30000000);
	//	constraint = constraint->next;
	//}
#endif
	direction = *a2;

#if 0//def PUPPET_PHYSICS
	static int iter = 0;
	if (iter < 5 && !(flags & SITH_RAYCAST_NO_CONSTRAINT_UPDATE))
	{
		for (int k = 0; k < v5->numConstraints; ++k)
		{
			sithThingConstraint* constraint = &v5->constraints[k];
		//	sithCollision_ConstrainThings(v5, constraint->thing, constraint->distance);

			rdVector3 delta;
			rdVector_Sub3(&delta, &v5->position, &constraint->thing->position);
			rdVector_ClipPrecision3(&delta);

			float deltaLen = rdVector_Normalize3Acc(&delta);
			float diff = (deltaLen - constraint->distance);
			if (diff < 0)
			{
				rdVector_Neg3Acc(&delta);
				diff = -diff;
			};

			if (fabs(diff) > 0.00001f)
			{
				++iter;
				sithCollision_UpdateThingCollision(constraint->thing, &delta, diff, SITH_RAYCAST_NO_CONSTRAINT_UPDATE);
				--iter;
			}

			// compute the difference in mass, used to adjust the positions
			//float invMassA = 1.0f / (v5->physicsParams.mass);
			//float invMassB = 1.0f / (constraint->thing->physicsParams.mass);
			//float diff = (deltaLen - constraint->distance) / (invMassA + invMassB);
			//if (fabs(diff) > 0.00001f)
			//{
			//	iter;
			//	rdVector3 d;
			//	rdVector_Scale3(&d, &delta, -diff * invMassA);
			//	
			//	rdVector_Sub3Acc(&direction, &d);
			//
			//	++iter;
			//	sithCollision_UpdateThingCollision(constraint->thing, &delta, diff * invMassB, 0);
			//	--iter;
			//}



			//if (constraint->useAxis)
			//{
			//	rdVector3 baseDir;
			//	rdMatrix_TransformVector34(&baseDir, &constraint->axis1, &constraint->thing->lookOrientation);
			//
			//	float dot = rdVector_Dot3(&direction, &baseDir);
			//	float angle = 90.0f - stdMath_ArcSin3(dot);
			//	if (angle > 10.0f)
			//	{
			//		rdVector3 rotationAxis;
			//		rdVector_Cross3(&rotationAxis, &direction, &baseDir);
			//		rdVector_Normalize3Acc(&rotationAxis);
			//	
			//		float correctionAngle = angle - 10.0f;
			//
			//		rdQuat quat;
			//		rdQuat_BuildFrom34(&quat, &constraint->thing->lookOrientation);
			//
			//		rdQuat correctionQuat;
			//		rdQuat_BuildFromAxisAngle(&correctionQuat, &rotationAxis, -correctionAngle);
			//		rdQuat_NormalizeAcc(&correctionQuat); 
			//	
			//		rdQuat newRotation;
			//		rdQuat_Mul(&newRotation, &correctionQuat, &quat);
			//	
			//	
			//		rdQuat_ToMatrix(&constraint->thing->lookOrientation, &newRotation);
			//
			//		//rdMatrix34 correctionMatrix;
			//		//rdVector_Cross3(&correctionMatrix.rvec, &direction, &rotationAxis);
			//		//rdVector_Normalize3Acc(&correctionMatrix.rvec);
			//		//
			//		//rdVector3 newUp;
			//		//rdVector_Cross3(&newUp, &correctionMatrix.rvec, &direction);
			//		//
			//		//rdMatrix_PostMultiply34(&v5->lookOrientation, &correctionMatrix);
			//	}
			//}

			//if(constraint->useAxis)
			//{
			//	rdVector3 worldHingeAxisA, worldHingeAxisB;
			//	rdMatrix_TransformVector34(&worldHingeAxisA, &constraint->axis1, &constraint->thing->lookOrientation);
			//	rdMatrix_TransformVector34(&worldHingeAxisB, &constraint->axis2, &v5->lookOrientation);
			//
			//	rdVector3 axisCorrection;
			//	rdVector_Cross3(&axisCorrection, &worldHingeAxisB, &worldHingeAxisA);
			//	float axisCorrectionLength = rdVector_Len3(&axisCorrection);
			//	if (axisCorrectionLength > 0.0f)
			//	{
			//		rdVector_Normalize3Acc(&axisCorrection);
			//		rdVector3 angularCorrection;
			//		rdVector_Scale3(&angularCorrection, &axisCorrection, axisCorrectionLength * 0.5f);
			//
			//		rdVector3 tmp1;
			//		rdVector_Scale3(&tmp1, &angularCorrection, 1.0f / constraint->thing->physicsParams.mass);
			//		rdVector_Sub3Acc(&constraint->thing->physicsParams.angVel, &tmp1);
			//
			//		rdVector3 tmp2;
			//		rdVector_Scale3(&tmp2, &angularCorrection, 1.0f / v5->physicsParams.mass);
			//		rdVector_Add3Acc(&v5->physicsParams.angVel, &tmp2);
			//
			//	//	rdMatrix34 m1;
			//	//	rdMatrix_BuildFromAxisAngle34(&m1, &axisCorrection, axisCorrectionLength * 0.5f * 180.0 / 3.141592);
			//	//	rdMatrix_PreMultiply34(&constraint->thing->lookOrientation, &m1);
			//	//
			//	//
			//	//	rdMatrix34 m2;
			//	//	rdMatrix_BuildFromAxisAngle34(&m2, &axisCorrection, -axisCorrectionLength * 0.5f * 180.0 / 3.141592);
			//	//	rdMatrix_PreMultiply34(&v5->lookOrientation, &m2);
			//
			//	}
			//}
		}
	}
#endif

    v10 = pThing->attachedParentMaybe;
    for ( ; v10; v10 = v10->childThing )
    {
        if (v10->attach_flags & SITH_ATTACH_NO_MOVE)
            continue;

        v11 = sithCollision_UpdateThingCollision(v10, &direction, a6, 64);
        if ( v11 >= a6 ) continue;
        
        if ( (v10->attach_flags & SITH_ATTACH_THINGSURFACE) != 0 )
        {
            rdMatrix_TransformVector34(&out, &v10->attachedSufaceInfo->face.normal, &v5->lookOrientation);
            v12 = stdMath_ClipPrecision(rdVector_Dot3(&direction, &out));
            if ( v12 <= 0.0 ) {
                continue;
            }
        }

        if ( (v5->thingflags & SITH_TF_NOIMPACTDAMAGE) == 0 )
        {
            sithThing_Damage(v10, v5, (a6 - v11) * 100.0, SITH_DAMAGE_IMPACT, -1);
        }
        a6 = v11;
    }
    sithCollision_dword_8B4BE4 = 0;
    sectTmp = v5->sector;
    if ( a6 == 0.0 )
    {
LABEL_78:
        if ( v66 < 4 )
            goto LABEL_81;
    }
    else
    {
        while ( v66 < 4 )
        {
            v16 = 0;
            rdVector_Copy3(&posCopy, &v5->position);
            out = direction;
            v17 = v5->moveSize;
            sectTmp = v5->sector;



#ifdef PUPPET_PHYSICS
			if (!(flags & 0x20000) && v5->constraints)
			{
				//sithConstraint_SolveConstraints(v5, sithTime_deltaSeconds);
				//for (int k = 0; k < 5; ++k)
				//{
				//	sithConstraint* constraint = v5->constraints;
				//	while (constraint)
				//	{
				//		switch(constraint->type)
				//		{
				//		case SITH_CONSTRAINT_DISTANCE:
				//			sithCollision_ApplyDistanceConstraint(constraint, v5, sithTime_deltaSeconds);
				//			break;
				//		case SITH_CONSTRAINT_ANGLES:
				//			sithConstraint_SolveAngleConstraint(constraint, v5, sithTime_deltaSeconds);
				//			break;
				//		case SITH_CONSTRAINT_LOOK:
				//			sithCollision_ApplyLookConstraint(constraint, v5);
				//			break;				
				//		default:
				//			break;
				//		}
				//		constraint = constraint->next;
				//	}
				//}
			}
#endif

            sithCollision_bDebugCollide = 0; // Added
            if (pThing == sithPlayer_pLocalPlayerThing) {
                sithCollision_bDebugCollide = 0;
            }
            sithCollision_SearchRadiusForThings(sectTmp, v5, &v5->position, &direction, a6, v17, flags);
            sithCollision_bDebugCollide = 0; // Added
            v36 = 0; // Added
            while ( 1 )
            {
                v19 = sithCollision_NextSearchResult();
                if ( !v19 ) {
                    break;
                }

                if ( v19->distance != 0.0 )
                {
                    rdVector_Copy3(&v5->position, &posCopy);
                    rdVector_MultAcc3(&v5->position, &direction, v19->distance);
                }
                if ( v19->distance >= (double)a6 )
                {
                    rdVector_Zero3(&v5->field_268);
                }
                else
                {
                    v25 = a6 - v19->distance;
                    rdVector_Scale3(&v5->field_268, &direction, v25);
                    if ( v5->moveType == SITH_MT_PHYSICS
                      && (v5->physicsParams.physflags & SITH_PF_SURFACEBOUNCE) != 0
                      && (!rdVector_IsZero3(&v5->physicsParams.addedVelocity)) )
                    {
                        v30 = 1.0 - v19->distance / a6;
                        v65 = v30;
                        rdVector_MultAcc3(&v5->physicsParams.vel, &v5->physicsParams.addedVelocity, -v30);
                    }
                }
                if ( (v19->hitType & SITHCOLLISION_THING) != 0 )
                {
				//	printf("thing kinda hitting thing\n");

                    // Added: noclip
                    if (!(g_debugmodeFlags & DEBUGFLAG_NOCLIP) || pThing != sithPlayer_pLocalPlayerThing)
                    {
                        if (!(g_debugmodeFlags & DEBUGFLAG_NOCLIP) || ((g_debugmodeFlags & DEBUGFLAG_NOCLIP) && v19->receiver != sithPlayer_pLocalPlayerThing))
                        {
                            v34 = v19->receiver;
                            v35 = v34->type + 12 * v5->type;
                            if ( sithCollision_collisionHandlers[v35].inverse )
                                v36 = sithCollision_collisionHandlers[v35].handler(v34, v5, v19, 1);
                            else
                                v36 = sithCollision_collisionHandlers[v35].handler(
                                          v5,
                                          v34,
                                          v19,
                                          0);
                        }
                    }
                    else {
                        v36 = 0; // Added: noclip
                    }
                }
                else if ( (v19->hitType & SITHCOLLISION_ADJOINCROSS) != 0 )
                {
				//	printf("thing crossing adjoin %d\n", v19->surface->adjoin->surface->index);

                    v37 = v19->surface;
                    rdVector_Copy3(&v72, &v5->position);
                    if ( (v37->surfaceFlags & SITH_SURFACE_COG_LINKED) != 0 )
                        sithCog_SendMessageFromSurface(v37, v5, 8);
                    sithThing_MoveToSector(v5, v19->surface->adjoin->sector, 0);
                 //   v36 = _memcmp(&v72, &v5->position, sizeof(rdVector3)) != 0;
                }
                else
                {
				//	printf("thing kinda hitting random shit\n");

                    // Added: noclip
                    if (!(g_debugmodeFlags & DEBUGFLAG_NOCLIP) || pThing != sithPlayer_pLocalPlayerThing)
                    {
                        amount = v19->surface;
                        if ( sithCollision_funcList[v5->type] )
                            v36 = sithCollision_funcList[v5->type](v5, amount, v19);
                        else
                            v36 = sithCollision_DefaultHitHandler(v5, amount, v19);
                    }
                    else {
                        v36 = 0; // Added: noclip
                    }
                }
                v16 = v36;
                if ( v65 != 0.0 && v5->moveType == SITH_MT_PHYSICS) // Added: physics check
                {
                    rdVector_Scale3(&v5->field_268, &v5->physicsParams.vel, v65 * sithTime_deltaSeconds);
                    v65 = 0.0;
                }
                if ( v36 )
                {
                    break;
                }
            }
            sithCollision_SearchClose();

            // Added: noclip
            if ((g_debugmodeFlags & DEBUGFLAG_NOCLIP) && pThing == sithPlayer_pLocalPlayerThing) {
                v16 = 0;
            }

            if ( v16 )
            {
                v64 = v19->distance + v64;
                a6 = 0.0;
                if (!rdVector_IsZero3(&v5->field_268))
                    a6 = stdMath_ClipPrecision(rdVector_Normalize3(&direction, &v5->field_268));
                ++v66;
            }
            else
            {
                v44 = v64 + a6;
                rdVector_Copy3(&v5->position, &posCopy);
                rdVector_MultAcc3(&v5->position, &direction, a6);

#ifdef PUPPET_PHYSICS
				//if (v5->constraintParent && !(flags & 0x10000000))
				//	sithCollision_ConstraintLoopTest(v5->constraintParent, &direction, a6, flags);
				//sithCollision_ConstraintLoopTest(v5, &direction, a6, flags);
#endif

                rdVector_Zero3(&v5->field_268);
                a6 = 0.0;
                v64 = v44;
            }
            if ( (v5->thingflags & 2) != 0 )
                return v64;
            if ( a6 == 0.0 )
                goto LABEL_78;
        }
    }

    // Added: noclip
    if (!(g_debugmodeFlags & DEBUGFLAG_NOCLIP) || pThing != sithPlayer_pLocalPlayerThing)
    {
        if ( v5->moveType == SITH_MT_PHYSICS )
            sithPhysics_ThingStop(v5);
    }
LABEL_81:
    
    v64 = stdMath_ClipPrecision(v64);
    if ( v5->collide && v5->moveType == SITH_MT_PHYSICS && !sithIntersect_IsSphereInSector(&v5->position, 0.0, v5->sector) )
    {
        // Added: noclip
        if (!(g_debugmodeFlags & DEBUGFLAG_NOCLIP) || pThing != sithPlayer_pLocalPlayerThing)
        {
#ifdef PUPPET_PHYSICS
			if (v5->type != SITH_THING_CORPSE)
			{
				// don't correct for limbs, as they'll be constrained to another thing
				rdVector_Copy3(&v5->position, &posCopy);
				rdVector_Copy3(&direction, &out);
			}
#else
            rdVector_Copy3(&v5->position, &posCopy);
            rdVector_Copy3(&direction, &out);
#endif
            sithThing_MoveToSector(v5, sectTmp, 0);
            if ( v5->lifeLeftMs )
                sithThing_Destroy(v5);
        }
        else {
            for (int i = 0; i < sithWorld_pCurrentWorld->numSectors; i++)
            {
                int found = 0;
                if (sithIntersect_IsSphereInSector(&v5->position, 0.0, &sithWorld_pCurrentWorld->sectors[i]))
                {
                    found = 1;
                    sithPlayer_bNoClippingRend = 0;
                    sithThing_MoveToSector(v5, &sithWorld_pCurrentWorld->sectors[i], 0);
                    break;
                }

                if (!found)
                {
                    sithPlayer_bNoClippingRend = 1;
                }
            }
        }
    }

    for ( i = v5->attachedParentMaybe; i; i = i->childThing )
    {
        if (!(i->attach_flags & SITH_ATTACH_NO_MOVE)) continue;

		//// calculate the distance between the 2 joints
		//rdVector3 delta;
		//rdVector_Sub3(&delta, &v5->position, &i->position);
		//float deltaLen = rdVector_Len3(&delta);
		//
		//// compute the difference in mass, used to adjust the positions
		//float invMassA = 1.0f / (v5->physicsParams.mass);
		//float invMassB = 1.0f / (i->physicsParams.mass);
		//float diff = (deltaLen - i->attach_distance) / (deltaLen * (invMassA + invMassB));
		//
		////if (!(pBodyPartA->flags & JOINTFLAGS_PINNED))
		//{
		//	rdVector3 deltaA;
		//	rdVector_Scale3(&deltaA, &delta, (double)diff * invMassA);
		//
		//	//rdVector_Add3Acc(&v5->position, &deltaA);
		//}
		//
		////if (!(pBodyPartB->flags & JOINTFLAGS_PINNED))
		//{
		//	rdVector3 deltaB;
		//	rdVector_Scale3(&deltaB, &delta, (double)diff * invMassB);
		//
		//	rdVector_Sub3Acc(&i->position, &deltaB);
		//}

		rdMatrix_TransformVector34(&i->position, &i->field_4C, &v5->lookOrientation);
		rdVector_Add3Acc(&i->position, &v5->position);


        if ( i->sector != v5->sector )
            sithThing_MoveToSector(i, v5->sector, 0);
    }


#ifdef PUPPET_PHYSICS
#ifdef RIGID_BODY
	if (v5->constraintParent && !(flags & 0x10000000))
		sithCollision_ConstraintLoopTest(v5->constraintParent, &direction, a6, flags);
	if(!(flags & 0x10000000))
		sithCollision_ConstraintLoopTest(v5, &direction, a6, flags);
#endif
#endif

#if 0//def PUPPET_PHYSICS
	if(v5->constraintThing)
	{
		sithCollision_ConeConstrain(v5->constraintThing, v5);

		rdVector3 dir;
		rdVector_Sub3(&dir, &v5->position, &posCopy);
		sithCollision_UpdateThingCollision(v5->constraintThing, &dir, rdVector_Normalize3Acc(&dir), 0);

		// the base pose local up vector is the cone axis
		// transform it to world space relative to the parent
		//rdVector3* refAxis = &v5->lookOrientation.uvec;//&(&pReferenceThing->lookOrientation.rvec)[axis];
		//
		//rdVector3 coneAxis;
		//rdVector_Copy3(&coneAxis, refAxis);
		//
		//rdVector3 childForward;
		//rdVector_Copy3(&childForward, &v5->constraintThing->lookOrientation.uvec);
		//
		//rdVector_Normalize3Acc(&coneAxis);
		//rdVector_Normalize3Acc(&childForward);
		//
		//float dotProd = rdVector_Dot3(&coneAxis, &childForward);
		//float currentAngle = 90.0f - stdMath_ArcSin3(dotProd);
		//float maxConeAngle = 5.0f;
		//if (currentAngle > maxConeAngle)
		//{
		//	float correctionAngle = currentAngle - maxConeAngle;
		//
		//	rdVector3 correctionAxis;
		//	rdVector_Cross3(&correctionAxis, &childForward, &coneAxis);
		//	rdVector_Normalize3Acc(&correctionAxis);
		//
		//	rdMatrix34 rotMat;
		//	rdMatrix_BuildFromAxisAngle34(&rotMat, &correctionAxis, -correctionAngle);
		//
		//	rdMatrix_PostMultiply34(&v5->constraintThing->lookOrientation, &rotMat);
		//}
	}
#endif

#if 0//def PUPPET_PHYSICS
	static int iter = 0;
	if (iter < 2 && !(flags & SITH_RAYCAST_NO_CONSTRAINT_UPDATE))
	{
		for (int k = 0; k < v5->numConstraints; ++k)
		{
			sithThingConstraint* constraint = &v5->constraints[k];

			rdVector3 delta;
			rdVector_Sub3(&delta, &v5->position, &constraint->thing->position);
			rdVector_ClipPrecision3(&delta);

			float deltaLen = rdVector_Normalize3Acc(&delta);
			float diff = (deltaLen - constraint->distance);
			if(diff <0)
			{
			rdVector_Neg3Acc(&delta);
			diff = -diff;
			};
			if (fabs(diff) > 0.00001f)
			{
				++iter;
				sithCollision_UpdateThingCollision(constraint->thing, &delta, diff, 0);
				--iter;
			}
		}
	}
#endif

    if ( v5->moveType == SITH_MT_PHYSICS )
    {
        if ( v64 == 0.0 )
            return 0.0;
        if (!(flags & RAYCAST_40))
        {
            if ( (v5->attach_flags) != 0 && !(v5->attach_flags & SITH_ATTACH_NO_MOVE)
		#ifdef PUPPET_PHYSICS
				&& !(v5->attach_flags& SITH_ATTACH_CONSTRAIN)
		#endif
              || (v5->physicsParams.physflags & SITH_PF_FLOORSTICK) != 0
              && (v5->physicsParams.vel.z < -2.0 || v5->physicsParams.vel.z <= 0.2) )
            {
                sithPhysics_FindFloor(v5, 0);
            }
        }
    }

#if 0//def PUPPET_PHYSICS
	if(!(flags & SITH_RAYCAST_NO_CONSTRAINT_UPDATE) && v5->moveType == SITH_MT_PHYSICS)// && v5->numConstraints > 0)
	{
		static int iter = 0;
		if(iter > 5)
			return v64;

		for (int k = 0; k < v5->numBallSocketJoints; ++k)
		{
			sithThingBallSocketJoint* pJoint = &v5->ballSocketJoints[k];

			rdVector3 worldAnchor;
			rdMatrix_TransformPoint34(&worldAnchor, &pJoint->anchor, &v5->lookOrientation);

			rdVector3 distanceA;
			rdVector_Sub3(&distanceA, &worldAnchor, &v5->position);
			float lengthA = rdVector_Len3(&distanceA);
			
			rdVector3 distanceB;
			rdVector_Sub3(&distanceB, &worldAnchor, &pJoint->thing->position);
			float lengthB = rdVector_Len3(&distanceB);
			
			if (lengthA > 0.0f || lengthB > 0.0f)
			{
				float invMassA = (1.0f / v5->physicsParams.mass);
				float invMassB = (1.0f / pJoint->thing->physicsParams.mass);

				float totalInvMass = invMassA + invMassB;
				if (totalInvMass != 0)
				{
					float factorA = invMassA / totalInvMass;
					float factorB = invMassB / totalInvMass;
					
					++iter;
					//sithCollision_UpdateThingCollision(v5, &delta, diff, 0);
					sithCollision_UpdateThingCollision(pJoint->thing, &distanceB, factorB, 0);
					--iter;

					//rdVector3 correctionA, correctionB;
					//rdVector_Scale3(&correctionA, &distanceA, factorA);
					//rdVector_Sub3Acc(&v5->position, &correctionA);
					//
					//rdVector_Scale3(&correctionB, &distanceB, factorB);
					//rdVector_Sub3Acc(&pJoint->thing->position, &correctionB);
				}
			}
		}

		/*for (int k = 0; k < v5->numConstraints; ++k)
		{
			sithThingConstraint* constraint = &v5->constraints[k];
	
			rdVector3 delta;
			rdVector_Sub3(&delta, &constraint->thing->position, &v5->position);
			rdVector_ClipPrecision3(&delta);

			float deltaLen = rdVector_Normalize3Acc(&delta);

			if (constraint->minDistance >= 0 && deltaLen > constraint->minDistance)
				continue;

			//float diff = (deltaLen - constraint->distance);
			//if (fabs(diff) > 0.001f)
			//{
			//	++iter;
			//	sithCollision_UpdateThingCollision(v5, &delta, diff, 0);
			//	--iter;
			//}

			// compute the difference in mass, used to adjust the positions
			float invMassA = 0.0001f;// 1.0f / (constraint->thing->physicsParams.mass);
			float invMassB = 1.0- 0.0001f;//1.0f / (v5->physicsParams.mass);
			float diff = (deltaLen - constraint->distance) / (invMassA + invMassB);
			if(fabs(diff) > 0.001f)
			{
				++iter;
				sithCollision_UpdateThingCollision(constraint->thing, &delta,  -diff * invMassA, 0);
				sithCollision_UpdateThingCollision(v5, &delta, diff * invMassB, 0);
				--iter;
			}
		}*/
	}
#endif

    return v64;
}

int sithCollision_DefaultHitHandler(sithThing *thing, sithSurface *surface, sithCollisionSearchEntry *a3)
{
    sithThing *v3; // esi
    float a1a; // [esp+Ch] [ebp+4h]

    v3 = thing;
    if ( thing->moveType != SITH_MT_PHYSICS )
        return 0;
    a1a = -rdVector_Dot3(&a3->hitNorm, &thing->physicsParams.vel);

#ifdef PUPPET_PHYSICS
	// todo: should this be here? linear effects for surface collision aren't...
	if (a1a >= 0.0f && thing->physicsParams.physflags & SITH_PF_ANGIMPULSE && thing->physicsParams.mass != 0.0)
	{
		rdVector3 contact = thing->position;
		rdVector_MultAcc3(&contact, &a3->hitNorm, -thing->moveSize);
		
		float restitution = 0.8f;
		rdVector3 force;
		rdVector_Scale3(&force, &a3->hitNorm, -(1.0f + restitution) * a1a * thing->physicsParams.mass);

		sithPhysics_ThingApplyRotForce(thing, &contact, &force, 0.0f);
	}
#endif

    if ( !sithCollision_CollideHurt(thing, &a3->hitNorm, a3->distance, surface->surfaceFlags & SITH_SURFACE_80) )
        return 0;

    if ( (surface->surfaceFlags & SITH_SURFACE_COG_LINKED) != 0 && (v3->thingflags & SITH_TF_INVULN) == 0 && surface->surfaceInfo.lastTouchedMs + 500 <= sithTime_curMsAbsolute )
    {
        surface->surfaceInfo.lastTouchedMs = sithTime_curMsAbsolute;
        sithCog_SendMessageFromSurface(surface, v3, SITH_MESSAGE_TOUCHED);
    }

    if ( a1a > 0.15000001 )
    {
        if ( a1a > 1.0 )
            a1a = 1.0;
        if ( (surface->surfaceFlags & SITH_SURFACE_METAL) != 0 )
        {
            sithSoundClass_PlayThingSoundclass(v3, SITH_SC_HITMETAL, a1a);
            return 1;
        }
        sithSoundClass_PlayThingSoundclass(v3, SITH_SC_HITHARD, a1a);
    }
    return 1;
}

int sithCollision_DebrisDebrisCollide(sithThing *thing1, sithThing *thing2, sithCollisionSearchEntry *a3, int isInverse)
{
    sithThing *v4; // esi
    sithThing *v5; // edi
    double v6; // st6
    //char v9; // c0
    double v11; // st7
    //char v14; // c0
    double v15; // st7
    float a3a; // [esp+0h] [ebp-38h]
    rdVector3 a2; // [esp+14h] [ebp-24h] BYREF
    rdVector3 forceVec; // [esp+20h] [ebp-18h] BYREF
    rdVector3 v19; // [esp+2Ch] [ebp-Ch] BYREF
    float senderb; // [esp+3Ch] [ebp+4h]
    float sender; // [esp+3Ch] [ebp+4h]
    float sendera; // [esp+3Ch] [ebp+4h]
    float a1a; // [esp+40h] [ebp+8h]

    if ( isInverse )
    {
        v4 = thing2;
        v5 = thing1;
    }
    else
    {
        v4 = thing1;
        v5 = thing2;
    }
    a2 = a3->hitNorm;

    if ( (v4->thingflags & SITH_TF_CAPTURED) != 0 && (v4->thingflags & SITH_TF_INVULN) == 0 )
        sithCog_SendMessageFromThing(v4, v5, SITH_MESSAGE_TOUCHED);
    if ( (v5->thingflags & SITH_TF_CAPTURED) != 0 && (v4->thingflags & SITH_TF_INVULN) == 0 )
        sithCog_SendMessageFromThing(v5, v4, SITH_MESSAGE_TOUCHED);

    if ( v4->moveType != SITH_MT_PHYSICS || v4->physicsParams.mass == 0.0 )
    {
        if ( v5->moveType != SITH_MT_PHYSICS || v5->physicsParams.mass == 0.0 )
            return 1;
        v11 = rdVector_Dot3(&v4->field_268, &a2);
        v11 = stdMath_ClipPrecision(v11);
        if ( v11 < 0.0 )
        {
            sendera = -v11 * 1.0001;
            rdVector_Neg3(&v19, &a2);
            v15 = sithCollision_UpdateThingCollision(v5, &v19, sendera, 0);
            if ( v15 < sendera )
            {
                if ( (v4->thingflags & SITH_TF_NOIMPACTDAMAGE) == 0 )
                {
                    a1a = v15;
                    a3a = (sendera - a1a) * 100.0;
                    sithThing_Damage(v5, v4, a3a, SITH_DAMAGE_IMPACT, -1);
                }
                rdVector_Zero3(&v4->field_268);
            }
            return 1;
        }
        return 0;
    }
    if ( v5->moveType == SITH_MT_PHYSICS && v5->physicsParams.mass != 0.0 )
    {
        v6 = rdVector_Dot3(&v5->physicsParams.vel, &a2) - rdVector_Dot3(&v4->physicsParams.vel, &a2);
        v6 = stdMath_ClipPrecision(v6);
        if ( v6 <= 0.0 )
            return 0;

        if ( (v4->physicsParams.physflags & SITH_PF_SURFACEBOUNCE) == 0 )
            v6 = v6 * 0.5;
        if ( (v5->physicsParams.physflags & SITH_PF_SURFACEBOUNCE) == 0 )
            v6 = v6 * 0.5;
        
		// (2*mass^2) / (2*mass)
		senderb = (v5->physicsParams.mass * v4->physicsParams.mass + v5->physicsParams.mass * v4->physicsParams.mass)
				/ (v5->physicsParams.mass + v4->physicsParams.mass);

		rdVector_Scale3(&forceVec, &a2, v6 * senderb);

		//rdVector3 impulse;
		//rdVector_Copy3(&impulse, &forceVec);

	#ifdef PUPPET_PHYSICS
		//if (v5->type != SITH_THING_CORPSE)
	#endif
			sithPhysics_ThingApplyForce(v4, &forceVec);
		rdVector_Neg3Acc(&forceVec);
#ifdef PUPPET_PHYSICS
		//if (v4->type != SITH_THING_CORPSE)
#endif
			sithPhysics_ThingApplyForce(v5, &forceVec);

#ifdef PUPPET_PHYSICS
		if(v4->physicsParams.physflags & SITH_PF_ANGIMPULSE || v5->physicsParams.physflags & SITH_PF_ANGIMPULSE)
		{
			rdVector3 contactA = v4->position;
			rdVector_MultAcc3(&contactA, &a2, -v4->moveSize);

			rdVector3 contactB = v5->position;
			rdVector_MultAcc3(&contactB, &a2, v5->moveSize);

			float invMassA = 1.0f / v4->physicsParams.mass;
			float invMassB = 1.0f / v5->physicsParams.mass;
			float restitution = 0.5f;
			float impulseMagnitude = (1.0f + restitution) * v6;
			impulseMagnitude /= invMassA + invMassB;

			rdVector3 rotForce;
			rdVector_Scale3(&rotForce, &a2, impulseMagnitude);

			if (v4->physicsParams.physflags & SITH_PF_ANGIMPULSE)// && v5->type != SITH_THING_CORPSE)
				sithPhysics_ThingApplyRotForce(v4, &contactA, &rotForce, 0.0f);
			rdVector_Neg3Acc(&rotForce);
			if (v5->physicsParams.physflags & SITH_PF_ANGIMPULSE)// && v4->type != SITH_THING_CORPSE)
				sithPhysics_ThingApplyRotForce(v5, &contactB, &rotForce, 0.0f);
		}
		
		// corpses shouldn't block player movement
		if ((v4->type == SITH_THING_PLAYER && v5->type == SITH_THING_CORPSE)
			|| (v5->type == SITH_THING_PLAYER && v4->type == SITH_THING_CORPSE)
		)
		{
			return 0;
		}
	#ifndef RIGID_BODY
		// if both bodies are corpses don't block
		if(v4->type == SITH_THING_CORPSE && v5->type == SITH_THING_CORPSE)
			return 0;
	#endif
#endif

        return sithCollision_CollideHurt(v4, &a2, a3->distance, 0);
    }
    sender = 0.0f;
    if (v4->moveType == SITH_MT_PHYSICS) // Added
        sender = -rdVector_Dot3(&v4->physicsParams.vel, &a2);
    if ( !sithCollision_CollideHurt(v4, &a2, a3->distance, 0) )
        return 0;
    if ( sender <= 0.15000001 )
        return 1;
    if ( sender > 1.0 )
        sender = 1.0;
#ifdef PUPPET_PHYSICS
	if (v5->type == SITH_THING_CORPSE && v5->prev_thing)
		sithSoundClass_PlayThingSoundclass(v5->prev_thing, SITH_SC_CORPSEHIT, sender);
	else
#endif
    if ( (v5->thingflags & SITH_TF_METAL) != 0 )
        sithSoundClass_PlayThingSoundclass(v4, SITH_SC_HITMETAL, sender);
    else
        sithSoundClass_PlayThingSoundclass(v4, SITH_SC_HITHARD, sender);
    return 1;
}

int sithCollision_CollideHurt(sithThing *a1, rdVector3 *a2, float a3, int a4)
{
    int result; // eax
    double v10; // st6
    double v19; // st7
    double v22; // st7
    double v26; // st7
    double v31; // st6
    double v32; // st7
    double v33; // st5
    double v35; // st7
    double v36; // st7
    double v39; // st7
    double v40; // st7
    float v43; // [esp+8h] [ebp-4h]
    float a1a; // [esp+10h] [ebp+4h]
    float amount; // [esp+14h] [ebp+8h]

    if ( a1->moveType != SITH_MT_PHYSICS )
        return 0;
    amount = -rdVector_Dot3(&a1->field_268, a2);
    a1a = stdMath_ClipPrecision(amount);
    if ( a1a <= 0.0 )
        return 0;
    v43 = 1.9;
    if ( (a1->physicsParams.physflags & SITH_PF_SURFACEBOUNCE) == 0 )
        v43 = 1.0001;
    if ( a3 == 0.0 && sithCollision_dword_8B4BE4 )
    {
        if ( amount <= 0.0 )
        {
            result = 0;
        }
        else
        {
            v10 = -rdVector_Dot3(&a1->physicsParams.vel, a2);
            rdVector_MultAcc3(&a1->field_268, a2, amount);
            if ( v10 > 0.0 )
            {
                rdVector_MultAcc3(&a1->physicsParams.vel, a2, v10);
            }
            v19 = -rdVector_Dot3(a2, &sithCollision_collideHurtIdk);
            rdVector_MultAcc3(&sithCollision_collideHurtIdk, a2, v19);
            rdVector_Normalize3Acc(&sithCollision_collideHurtIdk);
            v22 = -rdVector_Dot3(&a1->physicsParams.vel, &sithCollision_collideHurtIdk);
            if ( v22 > 0.0 )
            {
                rdVector_MultAcc3(&a1->physicsParams.vel, &sithCollision_collideHurtIdk, v22);
            }
            v26 = -rdVector_Dot3(&a1->field_268, &sithCollision_collideHurtIdk);
            if ( v26 > 0.0 )
            {
                rdVector_MultAcc3(&a1->field_268, &sithCollision_collideHurtIdk, v26);
            }
            result = 1;
        }
    }
    else
    {
        v31 = a1->physicsParams.vel.y * a2->y;
        v32 = a1->physicsParams.vel.x * a2->x;
        v33 = a1->physicsParams.vel.z * a2->z;
        sithCollision_dword_8B4BE4 = 1;
        sithCollision_collideHurtIdk.x = a2->x;
        sithCollision_collideHurtIdk.y = a2->y;
        sithCollision_collideHurtIdk.z = a2->z;
        v35 = -(v32 + v33 + v31);
        if ( v35 > 0.0 )
        {
            v36 = v43 * v35;
            rdVector_MultAcc3(&a1->physicsParams.vel, a2, v36);
            if ( !a4 && v35 > 2.5 )
            {
                v39 = (v35 - 2.5) * (v35 - 2.5) * 45.0;
                //printf("%f %f, %f %f %f\n", v39, v35, a1->physicsParams.vel.x, a1->physicsParams.vel.y, a1->physicsParams.vel.z);
                if ( v39 > 1.0 )
                {
                    sithSoundClass_PlayModeRandom(a1, SITH_SC_HITDAMAGED);
                    sithThing_Damage(a1, a1, v39, SITH_DAMAGE_FALL, -1);
                }
            }
        }
        v40 = v43 * a1a;
        rdVector_MultAcc3(&a1->field_268, a2, v40);
        result = 1;
    }
    return result;
}

int sithCollision_HasLos(sithThing *thing1, sithThing *thing2, int flag)
{
    int searchFlags; // edi
    int v4; // edi
    sithCollisionSearchEntry *v5; // ebp
    double v6; // st7
    sithCollisionSearchEntry *v7; // edx
    sithCollisionSearchEntry *v8; // ecx
    sithThing *v10; // edx
    int result; // eax
    int v12; // [esp+10h] [ebp-10h]
    rdVector3 a1a; // [esp+14h] [ebp-Ch] BYREF
    float a6; // [esp+2Ch] [ebp+Ch]

    v12 = 1;
    searchFlags = SITH_RAYCAST_ONLY_COG_THINGS | RAYCAST_100 | RAYCAST_20 | SITH_RAYCAST_IGNORE_ADJOINS;
    if ( flag )
        searchFlags = SITH_RAYCAST_ONLY_COG_THINGS | RAYCAST_20 | SITH_RAYCAST_IGNORE_ADJOINS;
    rdVector_Sub3(&a1a, &thing2->position, &thing1->position);
    a6 = rdVector_Normalize3Acc(&a1a);
    sithCollision_SearchRadiusForThings(thing1->sector, 0, &thing1->position, &a1a, a6, 0.0, searchFlags);
    v4 = sithCollision_searchStackIdx;
    v5 = sithCollision_searchStack[sithCollision_searchStackIdx].collisions;
    while ( 1 )
    {
        v6 = 3.4e38;
        v7 = 0;
        v8 = v5;
        for (int i = 0; i < sithCollision_searchNumResults[v4]; i++)
        {
            if ( !v8->hasBeenEnumerated )
            {
                if ( v6 <= v8->distance )
                {
                    if ( v6 == v8->distance 
                        && (v7->hitType & (SITHCOLLISION_THINGTOUCH|SITHCOLLISION_THINGCROSS)) 
                        && (v8->hitType & SITHCOLLISION_THINGADJOINCROSS))
                        v7 = v8;
                }
                else
                {
                    v6 = v8->distance;
                    v7 = v8;
                }
            }
            ++v8;
        }
        if ( v7 )
        {
            v7->hasBeenEnumerated = 1;
        }
        else
        {
            sithCollision_searchNumResults[v4] = 0;
            sithCollision_stackIdk[v4] = 0;
        }
        if ( !v7 )
            break;
        if ( (v7->hitType & SITHCOLLISION_THING) != 0 )
        {
            v10 = v7->receiver;
            if ( v10 == thing2 )
            {
                result = 1;
                sithCollision_searchStackIdx = v4 - 1;
                return result;
            }
            if ( v10 == thing1 )
                continue;
        }
        v12 = 0;
        break;
    }
    result = v12;
    sithCollision_searchStackIdx = v4 - 1;
    return result;
}

void sithCollision_sub_4E77A0(sithThing *thing, rdMatrix34 *a2)
{
    sithThing *v5; // edi
    rdVector3 a2a; // [esp+10h] [ebp-6Ch] BYREF
    rdMatrix34 out; // [esp+1Ch] [ebp-60h] BYREF
    rdMatrix34 mat1; // [esp+4Ch] [ebp-30h] BYREF
    float a1a; // [esp+84h] [ebp+8h]

    if ( thing->attachedParentMaybe )
    {
        rdMatrix_Normalize34(a2);
        rdVector_Copy3(&a2->scale, &thing->position);
        rdVector_Copy3(&thing->lookOrientation.scale, &thing->position);
        rdMatrix_InvertOrtho34(&mat1, &thing->lookOrientation);
        v5 = thing->attachedParentMaybe;
        while ( v5 )
        {
            rdVector_Copy3(&v5->lookOrientation.scale, &v5->position);
            rdMatrix_Multiply34(&out, &mat1, &v5->lookOrientation);
            rdMatrix_PostMultiply34(&out, a2);
            rdVector_Sub3(&a2a, &out.scale, &v5->position);
            a1a = rdVector_Normalize3Acc(&a2a);
            rdVector_Zero3(&out.scale);
            if ( a1a != 0.0 )
            {
                sithCollision_UpdateThingCollision(v5, &a2a, a1a, 64);
            }
            sithCollision_sub_4E77A0(v5, &out);
            if ( v5->moveType == SITH_MT_PHYSICS )
            {
                v5->physicsParams.physflags &= ~SITH_PF_ATTACHED;
            }
            v5 = v5->childThing;
        }
    }
    else if ( (((bShowInvisibleThings & 0xFF) + (thing->thingIdx & 0xFF)) & 7) == 0 )
    {
        rdMatrix_Normalize34(a2);
    }
    rdVector_Zero3(&a2->scale);
    _memcpy(&thing->lookOrientation, a2, sizeof(thing->lookOrientation));
}

int sithCollision_DebrisPlayerCollide(sithThing *thing, sithThing *thing2, sithCollisionSearchEntry *searchEnt, int isSolid)
{
    int result; // eax
    float mass; // [esp+14h] [ebp+4h]

    float tmp = 0.0; // Added 0.0, original game overwrites &searchEnt...

    // Added: check move type
    mass = (thing->moveType == SITH_MT_PHYSICS) ? thing->physicsParams.mass : 0.0;

    if ( isSolid )
        return sithCollision_DebrisDebrisCollide(thing, thing2, searchEnt, isSolid);

    if ( thing->moveType == SITH_MT_PHYSICS )
        tmp = -rdVector_Dot3(&searchEnt->hitNorm, &thing->physicsParams.vel);

    if (sithCollision_DebrisDebrisCollide(thing, thing2, searchEnt, 0))
    {
        if ( tmp > 0.25 )
        {
            sithThing_Damage(thing2, thing, mass * 0.3 * tmp, SITH_DAMAGE_IMPACT, -1);
        }
        return 1;
    }
    return 0;
}


#ifdef RAGDOLLS
void sithCollide_CollideRagdoll(sithThing* thing, sithThing* thing2, rdVector3* norm)
{
	if (thing->rdthing.pRagdoll && thing->physicsParams.mass != 0)// && thing->parentThing != thing2)
	{
		float velDiff = rdVector_Dot3(&thing->physicsParams.vel, norm) - rdVector_Dot3(&thing2->physicsParams.vel, norm);
		velDiff = stdMath_ClipPrecision(velDiff);
		if (velDiff <= 0.0)
			return;

		if ((thing->physicsParams.physflags & SITH_PF_SURFACEBOUNCE) == 0)
			velDiff = velDiff * 0.5;

		rdVector3 force;
		rdVector_Scale3(&force, &thing2->physicsParams.vel, velDiff);
		sithPhysics_ThingRagdollApplyForce(thing, &force, &thing2->position, thing2->collideSize);
	}
}

int sithCorpse_Collide(sithThing* thing1, sithThing* thing2, sithCollisionSearchEntry* searchEntry, int isInverse)
{
	sithThing* t1; // esi
	sithThing* t2; // edi
	if (isInverse)
	{
		t1 = thing2;
		t2 = thing1;
	}
	else
	{
		t1 = thing1;
		t2 = thing2;
	}

	if ((t1->thingflags & SITH_TF_CAPTURED) != 0 && (t1->thingflags & SITH_TF_INVULN) == 0)
		sithCog_SendMessageFromThing(t1, t2, SITH_MESSAGE_TOUCHED);
	if ((t2->thingflags & SITH_TF_CAPTURED) != 0 && (t2->thingflags & SITH_TF_INVULN) == 0)
		sithCog_SendMessageFromThing(t2, t1, SITH_MESSAGE_TOUCHED);

	// reactivate the ragdolls and apply forces
	if (t1->rdthing.pRagdoll)
	{
		if (t1->physicsParams.mass != 0)
		{
			rdVector3 negHit;
			rdVector_Neg3(&negHit, &searchEntry->hitNorm);
			sithCollide_CollideRagdoll(t1, t2, &negHit);
		}
	}

	if (t2->rdthing.pRagdoll)
	{
		if (t2->physicsParams.mass != 0)
			sithCollide_CollideRagdoll(t2, t1, &searchEntry->hitNorm);
	}

	// don't stop players
	//if(t1->type == SITH_THING_PLAYER)
		return 0;
		
	// for everything else, do typical full-object physics too
	//return sithCollision_DebrisDebrisCollide(thing1, thing2, searchEntry, isInverse);
}
#endif

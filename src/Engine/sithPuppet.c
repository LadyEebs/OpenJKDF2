#include "sithPuppet.h"

#include "General/stdHashTable.h"
#include "Engine/sithAnimClass.h"
#include "Gameplay/sithTime.h"
#include "World/sithSector.h"
#include "World/jkPlayer.h"
#include "Engine/sithCollision.h"
#include "Engine/sithIntersect.h"
#include "Gameplay/sithPlayerActions.h"
#include "Main/jkGame.h"
#include "Engine/rdPuppet.h"
#include "World/sithSoundClass.h"
#include "World/sithSurface.h"
#include "stdPlatform.h"
#include "AI/sithAI.h"
#include "jk.h"
#include "Primitives/rdQuat.h"

#include "sithPhysics.h"
#include "General/stdMath.h"

#include <math.h>

static const char* sithPuppet_animNames[43+2] = {
    "--RESERVED--",
    "stand",
    "walk",
    "run",
    "walkback",
    "strafeleft",
    "straferight",
    "death",
    "fire",
    "fire3",
    "fire4",
    "death2",
    "hit",
    "hit2",
    "rising",
    "toss",
    "place",
    "drop",
    "fire2",
    "fall",
    "land",
    "crouchforward",
    "crouchback",
    "activate",
    "magic",
    "choke",
    "leap",
    "jump",
    "reserved",
    "block",
    "block2",
    "turnleft",
    "turnright",
    "fidget",
    "fidget2",
    "magic2",
    "magic3",
    "victory",
    "windup",
    "holster",
    "drawfists",
    "drawgun",
    "drawsaber",

    // MOTS
    "charge",
    "buttpunch"
};

#ifdef ANIMCLASS_NAMES
stdHashTable* sithPuppet_jointNamesToIdxHashtable = NULL;

static const char* sithPuppet_jointNames[] =
{
	"head",
	"neck",
	"torso",
	"weapon",
	"weapon2",
	"aim",
	"aim2",
	"turretpitch",
	"turretyaw",
#ifdef REGIONAL_DAMAGE
	"hip",
	"rshoulder",
	"lshoulder",
	"rforearm",
	"lforearm",
	"rhand",
	"lhand",
	"rthigh",
	"lthigh",
	"rcalf",
	"lcalf",
	"rfoot",
	"lfoot",
#endif
};
#endif

int sithPuppet_Startup()
{
    sithPuppet_hashtable = stdHashTable_New(64);
    sithPuppet_keyframesHashtable = stdHashTable_New(256);

    if ( sithPuppet_hashtable && sithPuppet_keyframesHashtable )
    {
        sithPuppet_animNamesToIdxHashtable = stdHashTable_New(SITHPUPPET_NUMANIMS * 2);
        for (int i = 1; i < SITHPUPPET_NUMANIMS; i++)
        {
            stdHashTable_SetKeyVal(sithPuppet_animNamesToIdxHashtable, sithPuppet_animNames[i], (void *)(intptr_t)i);
        }
#ifdef ANIMCLASS_NAMES
		sithPuppet_jointNamesToIdxHashtable = stdHashTable_New(JOINTTYPE_NUM_JOINTS * 2);
		for (int i = 0; i < JOINTTYPE_NUM_JOINTS; i++)
		{
			stdHashTable_SetKeyVal(sithPuppet_jointNamesToIdxHashtable, sithPuppet_jointNames[i], (void*)(intptr_t)(i + 1));
		}
#endif
        return 1;
    }
    else
    {
        stdPrintf(pSithHS->errorPrint, ".\\Engine\\sithPuppet.c", 163, "Could not allocate memory of puppets.\n", 0, 0, 0, 0);
        return 0;
    }
}

void sithPuppet_Shutdown()
{
    if ( sithPuppet_hashtable )
    {
        stdHashTable_Free(sithPuppet_hashtable);
        sithPuppet_hashtable = 0;
    }
    if ( sithPuppet_keyframesHashtable )
    {
        stdHashTable_Free(sithPuppet_keyframesHashtable);
        sithPuppet_keyframesHashtable = 0;
    }
    if ( sithPuppet_animNamesToIdxHashtable )
    {
        stdHashTable_Free(sithPuppet_animNamesToIdxHashtable);
        sithPuppet_animNamesToIdxHashtable = 0;
    }
#ifdef ANIMCLASS_NAMES
	if (sithPuppet_jointNamesToIdxHashtable)
	{
		stdHashTable_Free(sithPuppet_jointNamesToIdxHashtable);
		sithPuppet_jointNamesToIdxHashtable = 0;
	}
#endif
}

sithPuppet* sithPuppet_NewEntry(sithThing *thing)
{
    sithPuppet *v1; // edi
    sithSector *sector; // eax
    sithPuppet *result; // eax

    v1 = (sithPuppet *)pSithHS->alloc(sizeof(sithPuppet));
    thing->puppet = v1;
    if ( !v1 )
	{
        thing->animclass = 0;
		return NULL;
	}

    _memset(v1, 0, sizeof(sithPuppet));
    sector = thing->sector;
    if ( sector && (sector->flags & SITH_SECTOR_UNDERWATER) != 0 )
    {
        result = thing->puppet;
        result->field_4 = 1;
        result->otherTrack = -1;
        result->field_18 = -1;
        result->currentTrack = -1;
    }
    else
    {
        result = thing->puppet;
        result->field_4 = 0;
        result->otherTrack = -1;
        result->field_18 = -1;
        result->currentTrack = -1;
    }
    return result;
}

void sithPuppet_FreeEntry(sithThing *puppet)
{
    if ( puppet->puppet )
    {
#ifdef PUPPET_PHYSICS
		if (puppet->puppet->physics)
			sithPuppet_StopPhysics(puppet);
#endif
        pSithHS->free(puppet->puppet);
        puppet->puppet = 0;
    }
}

void sithPuppet_sub_4E4760(sithThing *thing, int a2)
{
    sithPuppet *puppet; // eax

    if ( thing->animclass )
    {
        puppet = thing->puppet;
        if ( puppet )
        {
            if ( puppet->field_4 != a2 )
            {
                puppet->field_4 = a2;
                puppet->majorMode = puppet->field_0 + 3 * a2;
            }
        }
    }
}

// MOTS altered
int sithPuppet_PlayMode(sithThing *thing, signed int anim, rdPuppetTrackCallback_t callback)
{
    sithAnimclass *v4; // ebx
    sithPuppet *v6; // edx
    sithAnimclassEntry *v7; // eax
    rdKeyframe *keyframe; // ebx
    int flags; // ebp
    int v10; // eax
    rdPuppet *v11; // ecx
    signed int result; // eax
    int highPri; // [esp+14h] [ebp+4h]
    int lowPri; // [esp+18h] [ebp+8h]

    v4 = thing->animclass;
    if ( !v4 )
        return -1;
    if ( anim < 0 )
        return -1;
    if ( anim >= SITHPUPPET_NUMANIMS )
        return -1;
    v6 = thing->puppet;
    v7 = &v4->modes[v6->majorMode].keyframe[anim];
    keyframe = v7->keyframe;
    if ( !v7->keyframe )
        return -1;
    flags = v7->flags;
    lowPri = v7->lowPri;
    highPri = v7->highPri;
    if ( anim != SITH_ANIM_FIDGET && anim != SITH_ANIM_FIDGET2 )
    {
        v6->animStartedMs = sithTime_curMs;
        v10 = v6->currentTrack;
        if ( v10 >= 0 )
        {
            v11 = thing->rdthing.puppet;
            if ( v11->tracks[v10].keyframe )
                rdPuppet_ResetTrack(v11, v10);
            thing->puppet->currentTrack = -1;
        }
    }
    
    result = sithPuppet_StartKey(thing->rdthing.puppet, keyframe, lowPri, highPri, flags, callback);
    if ( result < 0 )
        return -1;
    return result;
}

int sithPuppet_StartKey(rdPuppet *puppet, rdKeyframe *keyframe, int a3, int a4, int a5, rdPuppetTrackCallback_t callback)
{
    int v6; // ecx
    int trackNum; // esi
    signed int result; // eax

    v6 = 1;
    if ( (a5 & 8) != 0 )
    {
        trackNum = 0;
        while ( puppet->tracks[trackNum].keyframe != keyframe )
        {
            ++trackNum;
            if ( trackNum >= 4 )
                goto LABEL_8;
        }
        rdPuppet_unk(puppet, trackNum);
        v6 = 0;
    }
    else
    {
        trackNum = a5;
    }
LABEL_8:
    if ( v6 )
    {
        trackNum = rdPuppet_AddTrack(puppet, keyframe, a3, a4);
        if ( trackNum < 0 )
            return -1;
    }
    if ( callback )
        rdPuppet_SetCallback(puppet, trackNum, callback);
    else
        rdPuppet_SetCallback(puppet, trackNum, sithPuppet_DefaultCallback);
    if ( (a5 & 2) != 0 )
    {
        rdPuppet_SetStatus(puppet, trackNum, 0x20);
    }
    else if ( (a5 & 0x20) != 0 )
    {
        rdPuppet_SetStatus(puppet, trackNum, 0x80);
    }
    else if ( (a5 & 4) != 0 )
    {
        rdPuppet_SetStatus(puppet, trackNum, 0x40);
    }
    if ( (a5 & 1) != 0 )
        rdPuppet_SetTrackSpeed(puppet, trackNum, 0.0);
    if ( (a5 & 0x10) != 0 )
        rdPuppet_PlayTrack(puppet, trackNum);
    else
        rdPuppet_FadeInTrack(puppet, trackNum, 0.1);
    result = trackNum;
    puppet->tracks[trackNum].field_130 = ((playerThingIdx + 1) << 16) | (uint16_t)(trackNum + 1);
    return result;
}

void sithPuppet_ResetTrack(sithThing *puppet)
{
    unsigned int trackNum; // esi
    sithPuppet *v2; // eax

    for ( trackNum = 0; trackNum < 4; ++trackNum )
        rdPuppet_ResetTrack(puppet->rdthing.puppet, trackNum);
    v2 = puppet->puppet;
    v2->playingAnim = NULL;
    v2->otherTrack = -1;
    v2->field_18 = -1;
    v2->currentTrack = -1;
}

// MOTS altered?
void sithPuppet_Tick(sithThing *thing, float deltaSeconds)
{
    double v3; // st7
    sithPuppet *v4; // eax
    sithAnimclassEntry *v5; // ecx
    int v6; // ecx
    double v8; // st7
    char v9; // c0
    sithPuppet *v10; // eax
    double v11; // st7
    sithAnimclass *v12; // edx
    sithAnimclassEntry *v13; // eax
    int v14; // eax
    sithAnimclass *v17; // edx
    sithAnimclassEntry *v18; // eax
    int v19; // eax
    rdMatrix34 *v20; // eax
    rdMatrix34 *v23; // ecx
    float *v27; // eax
    int i; // edx
    float v31; // [esp+0h] [ebp-18h]
    rdVector3 a1a; // [esp+Ch] [ebp-Ch] BYREF
    float thinga; // [esp+1Ch] [ebp+4h]
    float a2a; // [esp+20h] [ebp+8h]

    if ( thing->animclass && thing->puppet && thing->rdthing.puppet && (g_debugmodeFlags & DEBUGFLAG_NO_PUPPETS) == 0 )
    {
        if ( thing->moveType == SITH_MT_PHYSICS )
        {
		#ifdef PUPPET_PHYSICS
			// corpses have physicalized animation
			if((thing->animclass->flags & SITH_PUPPET_PHYSICS) && (thing->type == SITH_THING_CORPSE) && jkPlayer_ragdolls)
			{
			//	rdPuppet_ResetTrack(thing->rdthing.puppet, 0);
			//	rdPuppet_ResetTrack(thing->rdthing.puppet, 1);
			//	rdPuppet_ResetTrack(thing->rdthing.puppet, 2);
			//	rdPuppet_ResetTrack(thing->rdthing.puppet, 3);

				if(!thing->puppet->physics)
					sithPuppet_StartPhysics(thing, &thing->physicsParams.vel, deltaSeconds);
				sithPuppet_UpdatePhysicsAnim(thing, deltaSeconds);
			}
			else // otherwise update the animation
		#endif
			{
				if (thing->puppet->physics)
					sithPuppet_StopPhysics(thing);

				v3 = sithPuppet_sub_4E4380(thing);
				v4 = thing->puppet;
				v5 = v4->playingAnim;
				if ( v5 )
				{
					if ( (v5->flags & 1) != 0 )
					{
						v6 = v4->otherTrack;
						if ( v6 >= 0 )
						{
							thinga = v3 * deltaSeconds;
							v8 = thinga;
							if ( v8 < 0.0 )
								v8 = -v8;
							v31 = v8 * 280.0;
							rdPuppet_AdvanceTrack(thing->rdthing.puppet, v6, v31);
						}
					}
				}
				sithPuppet_FidgetAnim(thing);
			}
        }
        if ( rdPuppet_UpdateTracks(thing->rdthing.puppet, deltaSeconds) && thing->moveType == SITH_MT_PATH )
        {
            rdVector_Zero3(&thing->lookOrientation.scale);
            thing->rdthing.field_18 = 0;
            rdPuppet_BuildJointMatrices(&thing->rdthing, &thing->lookOrientation);
            v20 = thing->rdthing.hierarchyNodeMatrices;
            thing->rdthing.field_18 = 1;
            rdVector_Add3(&a1a, &thing->trackParams.moveFrameOrientation.scale, &v20->scale);
            rdVector_Sub3Acc(&a1a, &thing->position);
            if (!rdVector_IsZero3(&a1a))
            {
                a2a = rdVector_Normalize3Acc(&a1a);
                sithCollision_UpdateThingCollision(thing, &a1a, a2a, 0);
            }
            v23 = thing->rdthing.hierarchyNodeMatrices;
            rdVector_Sub3(&a1a, &thing->position, &v23->scale);
            for ( i = thing->rdthing.model3->numHierarchyNodes; i != 0; i--)
            {
                rdVector_Add3Acc(&v23->scale, &a1a);
                v23++;
            }
        }
    }
}

float sithPuppet_sub_4E4380(sithThing *thing)
{
    double v2; // st7
    int v3; // ecx
    double v5; // st6
    char missing_1; // c0
    double v8; // st5
    char missing_2; // c0
    sithSector *v10 = NULL; // eax
    sithAnimclass *v11 = NULL; // ebp
    sithPuppet *v12 = NULL; // eax
    double v14; // st6
    char missing_3; // c0
    int anim; // ecx
    sithPuppet *v18 = NULL; // edx
    sithAnimclassEntry *v19 = NULL; // edi
    int v20; // eax
    float v23; // [esp+10h] [ebp-10h]
    rdVector3 a1a; // [esp+14h] [ebp-Ch] BYREF
    float thinga; // [esp+24h] [ebp+4h]

    v23 = 0.5;
    if ( !thing->sector
      || rdVector_IsZero3(&thing->physicsParams.vel) )
    {
        v2 = 0.0;
        thinga = 0.0;
        v3 = 0;
    }
    else
    {
        rdMatrix_TransformVector34Acc_0(&a1a, &thing->physicsParams.vel, &thing->lookOrientation);
        if ( thing->attach_flags || (thing->physicsParams.physflags & SITH_PF_FLY) != 0 || (thing->sector->flags & SITH_ANIM_WALK) != 0 )
        {
            v2 = a1a.y;
            v5 = fabs(a1a.y);
            v8 = fabs(a1a.x);

            if ( v5 <= v8 )
            {
                v3 = 0;
                thinga = a1a.x;
            }
            else
            {
                v3 = SITH_ANIM_STAND;
                thinga = a1a.y;
            }
        }
        else
        {
            v2 = a1a.y;
            thinga = thing->physicsParams.vel.z;
            v3 = SITH_ANIM_WALK;
        }
    }

    if ( thing->sector && (thing->sector->flags & SITH_SECTOR_UNDERWATER) != 0 )
    {
        v11 = thing->animclass;
        if ( v11 )
        {
            v12 = thing->puppet;
            if ( v12 )
            {
                if ( v12->field_4 != SITH_ANIM_STAND )
                {
                    v12->field_4 = SITH_ANIM_STAND;
                    v12->majorMode = v12->field_0 + 3;
                }
            }
        }
    }
    else
    {
        if ( thing->type == SITH_THING_PLAYER )
            v23 = 1.0;
        v11 = thing->animclass;
        if ( v11 )
        {
            if ( thing->puppet )
            {
                if ( thing->puppet->field_4 )
                {
                    thing->puppet->field_4 = 0;
                    thing->puppet->majorMode = thing->puppet->field_0;
                }
            }
        }
        if ( thing->moveType == SITH_ANIM_STAND && thing->attach_flags && (thing->physicsParams.physflags & (SITH_PF_200000|SITH_PF_CROUCHING)) )
        {
            if ( v3 == SITH_ANIM_STAND && thinga < 0.0 )
                anim = SITH_ANIM_CROUCHBACK;
            else
                anim = SITH_ANIM_CROUCHFORWARD;
            goto LABEL_51;
        }
    }
    v14 = thinga;
    if ( v14 < 0.0 )
        v14 = -v14;
    if ( v14 <= 0.02 )
    {
        if ( thing->controlType == SITH_CT_AI && thing->actor )
        {
            thinga = 0.2;
            anim = (thing->actor->flags & SITHAI_MODE_TURNING) != 0 ? SITH_ANIM_TURNLEFT : SITH_ANIM_STAND;
        }
        else
        {
            thinga = thing->physicsParams.angVel.y * 0.0002;
            if ( (((bShowInvisibleThings & 0xFF) + (thing->thingIdx & 0xFF)) & 3) != 0 )
                return thinga;
            if ( thinga >= -0.01 )
            {
                anim = SITH_ANIM_TURNRIGHT;
                if ( thinga <= 0.01 )
                    anim = SITH_ANIM_STAND;
            }
            else
            {
                anim = SITH_ANIM_TURNLEFT;
            }
        }
    }
    else if ( v3 )
    {
        if ( v3 == SITH_SECTOR_NOGRAVITY )
        {
            if ( thinga >= 0.0 )
            {
                if ( thinga < (double)v23 )
                    anim = SITH_ANIM_WALK;
                else
                    anim = SITH_ANIM_RUN;
            }
            else
            {
                anim = SITH_ANIM_WALKBACK;
            }
        }
        else if ( thinga <= 0.0 )
        {
            if ( thinga <= -3.0 )
                anim = SITH_ANIM_FALL;
            else
                anim = SITH_ANIM_DROP;
        }
        else if ( v2 <= 0.02 )
        {
            anim = SITH_ANIM_RISING;
        }
        else
        {
            anim = SITH_ANIM_LEAP;
        }
    }
    else if ( thinga <= 0.0 )
    {
        anim = SITH_ANIM_STRAFELEFT;
    }
    else
    {
        anim = SITH_ANIM_STRAFERIGHT;
    }
LABEL_51:
    v18 = thing->puppet;
    v18->currentAnimation = anim;
    v19 = &v11->modes[v18->majorMode].keyframe[anim];
    if ( v19 != v18->playingAnim )
    {
        if ( anim == SITH_ANIM_FALL )
        {
            if ( (thing->thingflags & SITH_TF_DEAD) != 0 || (thing->actorParams.typeflags & SITH_AF_SCREAMING) != 0 )
                goto LABEL_60;
            sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_FALLING);
            v20 = thing->actorParams.typeflags | SITH_AF_SCREAMING;
        }
        else
        {
            if ( (thing->actorParams.typeflags & SITH_AF_SCREAMING) == 0 )
            {
LABEL_60:
                sithPuppet_sub_4E4A20(thing, v19);
                return thinga;
            }
            sithSoundClass_ThingPauseSoundclass(thing, SITH_SC_FALLING);
            v20 = thing->actorParams.typeflags & ~SITH_AF_SCREAMING;
        }
        thing->actorParams.typeflags = v20;
        goto LABEL_60;
    }
    return thinga;
}

void sithPuppet_sub_4E4A20(sithThing *thing, sithAnimclassEntry *animClass)
{
    rdPuppet *rdPup; // ecx
    sithPuppet *sithPup; // esi
    rdKeyframe **v4; // eax
    int v5; // eax
    int v6; // eax
    rdPuppet *v7; // ecx

    rdPup = thing->rdthing.puppet;
    if ( rdPup && thing->animclass )
    {
        sithPup = thing->puppet;
        v4 = &sithPup->playingAnim->keyframe;
        if ( !v4 || *v4 != animClass->keyframe )
        {
            sithPup->animStartedMs = sithTime_curMs;
            v5 = sithPup->currentTrack;
            if ( v5 >= 0 )
            {
                if ( rdPup->tracks[v5].keyframe )
                    rdPuppet_ResetTrack(rdPup, v5);
                sithPup = thing->puppet;
                sithPup->currentTrack = -1;
            }
            v6 = sithPup->otherTrack;
            if ( v6 >= 0 )
            {
                v7 = thing->rdthing.puppet;
                if ( v7->tracks[v6].keyframe )
                    rdPuppet_FadeOutTrack(v7, v6, 0.1);
                sithPup = thing->puppet;
                sithPup->otherTrack = -1;
            }
            if ( animClass->keyframe )
            {
                sithPup = thing->puppet;
                sithPup->otherTrack = sithPuppet_StartKey(
                                          thing->rdthing.puppet,
                                          animClass->keyframe,
                                          animClass->lowPri,
                                          animClass->highPri,
                                          animClass->flags,
                                          sithPuppet_DefaultCallback);
            }
        }
        sithPup->playingAnim = animClass;
    }
}

// MOTS altered
void sithPuppet_DefaultCallback(sithThing *thing, int track, uint32_t a3)
{
    unsigned int v3; // esi
    sithPuppet *sithPup; // eax
    uint32_t soundToPlay_base; // edi
    sithThing *v8; // eax
    int v10; // eax
    sithThing *v11; // esi
    sithActor *v12; // eax

    v3 = 0;
    switch ( a3 )
    {
        case 0u:
            sithPup = thing->puppet;
            if ( sithPup )
            {
                if ( track == sithPup->currentTrack )
                    sithPup->currentTrack = -1;
            }
            return;
        case 1u:
        case 2u:
        case 8u:
        case 9u:
            if ( thing->rdthing.puppet->tracks[track].playSpeed < 0.5 )
                return;
            if ( (thing->thingflags & SITH_TF_DEAD) != 0 )
                return;
            if ( thing->type == SITH_THING_CORPSE )
                return;

            if ( !thing->attach_flags || !thing->soundclass )
                return;
            soundToPlay_base = a3 - 1;
            if ( a3 - 1 > 1 )
                soundToPlay_base = a3 - 6;
            if ( !(thing->attach_flags & SITH_ATTACH_THINGSURFACE) )
            {
                v10 = thing->attachedSurface->surfaceFlags;
                if ( (v10 & (SITH_SURFACE_VERYDEEPWATER|SITH_SURFACE_EARTH|SITH_SURFACE_PUDDLE|SITH_SURFACE_WATER|SITH_SURFACE_METAL)) != 0 )
                {
                    if ( (v10 & SITH_SURFACE_METAL) != 0 )
                        goto LABEL_14;
                    if ( (v10 & SITH_SURFACE_WATER) != 0 )
                    {
                        sithSoundClass_PlayModeRandom(thing, (soundToPlay_base + SITH_SC_LWALKWATER));
                        return;
                    }
                    if ( (v10 & SITH_SURFACE_PUDDLE) != 0 )
                    {
                        sithSoundClass_PlayModeRandom(thing, (soundToPlay_base + SITH_SC_LWALKPUDDLE));
                        return;
                    }
                    v3 = (~v10 & SITH_SURFACE_EARTH | (unsigned int)SITH_SURFACE_200000) >> 19;
                }
            }
            else
            {
                if ( (thing->attachedThing->thingflags & SITH_TF_METAL) != 0 )
                {
LABEL_14:
                    sithSoundClass_PlayModeRandom(thing, (soundToPlay_base + SITH_SC_LWALKMETAL));
                    return;
                }
                if ( (thing->attachedThing->thingflags & SITH_TF_EARTH) != 0 )
                {
                    sithSoundClass_PlayModeRandom(thing, (soundToPlay_base + SITH_SC_LWALKEARTH));
                    return;
                }
            }
            sithSoundClass_PlayModeRandom(thing, (soundToPlay_base + 4 * v3 + 6));
            return;
        case 3u:
            if ( thing->controlType == SITH_CT_AI )
            {
                v12 = thing->actor;
                if ( v12 )
                    sithAI_FireWeapon(v12, 0.0, 0.0, 0.0, v12->field_264, v12->field_26C, v12->field_268);
            }
            return;
        case 4u:
            thing->jkFlags |= JKFLAG_SABERDAMAGE;
            return;
        case 5u:
            thing->jkFlags &= ~JKFLAG_SABERDAMAGE;
            return;
        case 6u:
            if ( thing->rdthing.puppet->tracks[track].playSpeed >= 0.5 && thing->soundclass )
            {
                if ( (thing->physicsParams.physflags & SITH_PF_WATERSURFACE) != 0 )
                    sithSoundClass_PlayModeRandom(thing, SITH_SC_LSWIMSURFACE);
                else
                    sithSoundClass_PlayModeRandom(thing, SITH_SC_LSWIMUNDER);
            }
            return;
        case 7u:
            if ( thing->rdthing.puppet->tracks[track].playSpeed >= 0.5 && thing->soundclass )
            {
                if ( (thing->physicsParams.physflags & SITH_PF_WATERSURFACE) != 0 )
                    sithSoundClass_PlayModeRandom(thing, SITH_SC_TREADSURFACE);
                else
                    sithSoundClass_PlayModeRandom(thing, SITH_SC_TREADUNDER);
            }
            return;
        case 0xAu:
            if ( thing->rdthing.puppet->tracks[track].playSpeed >= 0.5 && thing->attach_flags )
			#ifdef PUPPET_PHYSICS
				if (!thing->puppet->physics) // let the physics system handle this
			#endif
                sithSoundClass_PlayModeRandom(thing, SITH_SC_CORPSEHIT);
            return;
        case 0xBu:
            v11 = thing;
            if ( thing->attach_flags )
            {
                sithPlayerActions_JumpWithVel(thing, 1.0);
                goto LABEL_50;
            }
            return;
        case 0xCu:
            v11 = thing;
            if ( thing->attach_flags )
            {
                sithPlayerActions_JumpWithVel(thing, 2.0);
LABEL_50:
                if ( v11->controlType == SITH_CT_AI )
                    v11->actor->flags |= 1u;
            }
            return;
        case 0xDu:
            if ( thing->rdthing.puppet->tracks[track].playSpeed >= 0.5 && thing->soundclass )
            {
                if ( (thing->physicsParams.physflags & SITH_PF_WATERSURFACE) != 0 )
                    sithSoundClass_PlayModeRandom(thing, SITH_SC_RSWIMSURFACE);
                else
                    sithSoundClass_PlayModeRandom(thing, SITH_SC_RSWIMUNDER);
            }
            return;
        case 0xEu:
            thing->jkFlags |= JKFLAG_40;
            return;

        // MoTS added
        case 0xF:
            if (!Main_bMotsCompat) return;

            if ((thing->controlType == SITH_CT_AI) && (v12 = thing->actor, v12 != (sithActor *)0x0)) {
                sithAI_Leap(v12,0.0,0.0,0.0,v12->field_26C,v12->field_264,v12->field_268);
                return;
            }
            return;
        case 0x10:
            if (!Main_bMotsCompat) return;

            if ((thing->controlType == SITH_CT_AI) && (v12 = thing->actor, v12 != (sithActor *)0x0)) {
                sithAI_FUN_0053a520(v12,0.0,0.0,0.0,v12->field_26C,v12->field_264,v12->field_268);
            }
            return;
        default:
            return;
    }
}

int sithPuppet_StopKey(rdPuppet *pupper, int track, float a3)
{
    if ( !pupper->tracks[track].keyframe )
        return 0;
    if ( a3 <= 0.0 )
        rdPuppet_ResetTrack(pupper, track);
    else
        rdPuppet_FadeOutTrack(pupper, track, a3);
    return 1;
}

void sithPuppet_SetArmedMode(sithThing *thing, int mode)
{
    sithPuppet *v2; // ecx

    if ( thing->animclass )
    {
        v2 = thing->puppet;
        v2->field_0 = mode;
        v2->majorMode = mode + 2 * v2->field_4 + v2->field_4;
    }
}

void sithPuppet_FidgetAnim(sithThing *pThing)
{
    sithPuppet *puppet; // eax
    double v2; // st7
    sithAnimclass *v3; // edx
    sithAnimclassEntry *v4; // eax
    int v5; // eax
    sithPuppet *v6; // esi
    unsigned int v7; // edx
    sithAnimclass *v8; // edx
    sithAnimclassEntry *v9; // eax
    int v10; // eax

    puppet = pThing->puppet;
    if ( puppet->currentTrack < 0 && puppet->currentAnimation == 1 && (double)(unsigned int)puppet->animStartedMs - -30000.0 < (double)sithTime_curMs )
    {
        v2 = _frand();
        if ( v2 >= 0.3 )
        {
            if ( v2 < 0.6 )
            {
                v8 = pThing->animclass;
                if ( !v8
                  || (v9 = &v8->modes[pThing->puppet->majorMode].keyframe[SITH_ANIM_FIDGET2], !v9->keyframe)
                  || (v10 = sithPuppet_StartKey(
                                pThing->rdthing.puppet,
                                v9->keyframe,
                                v9->lowPri,
                                v9->highPri,
                                v8->modes[pThing->puppet->majorMode].keyframe[SITH_ANIM_FIDGET2].flags,
                                0),
                      v10 < 0) )
                {
                    v10 = -1;
                }
                pThing->puppet->currentTrack = v10;
            }
        }
        else
        {
            v3 = pThing->animclass;
            if ( !v3
              || (v4 = &v3->modes[pThing->puppet->majorMode].keyframe[SITH_ANIM_FIDGET], !v4->keyframe)
              || (v5 = sithPuppet_StartKey(
                           pThing->rdthing.puppet,
                           v4->keyframe,
                           v4->lowPri,
                           v4->highPri,
                           v3->modes[pThing->puppet->majorMode].keyframe[SITH_ANIM_FIDGET].flags,
                           0),
                  v5 < 0) )
            {
                v5 = -1;
            }
            v6 = pThing->puppet;
            v7 = sithTime_curMs;
            v6->currentTrack = v5;
            v6->animStartedMs = v7;
        }
    }
}

void sithPuppet_resetidk(sithThing *pThing)
{
    sithPuppet *puppet; // eax
    int v2; // eax
    rdPuppet *v3; // ecx

    puppet = pThing->puppet;
    puppet->animStartedMs = sithTime_curMs;
    v2 = puppet->currentTrack;
    if ( v2 >= 0 )
    {
        v3 = pThing->rdthing.puppet;
        if ( v3->tracks[v2].keyframe )
            rdPuppet_ResetTrack(v3, v2);
        pThing->puppet->currentTrack = -1;
    }
}

void sithPuppet_advanceidk(sithThing *pThing, float a2)
{
    double v3; // st7
    sithPuppet *puppet; // eax
    sithAnimclassEntry *v5; // ecx
    int v6; // ecx
    double v8; // st7
    float a3; // [esp+0h] [ebp-8h]
    float thinga; // [esp+Ch] [ebp+4h]

    v3 = sithPuppet_sub_4E4380(pThing);
    puppet = pThing->puppet;
    v5 = puppet->playingAnim;
    if ( v5 )
    {
        if ( (v5->flags & 1) != 0 )
        {
            v6 = puppet->otherTrack;
            if ( v6 >= 0 )
            {
                thinga = v3 * a2;
                v8 = thinga;
                if ( v8 < 0.0 )
                    v8 = -v8;
                a3 = v8 * 280.0;
                rdPuppet_AdvanceTrack(pThing->rdthing.puppet, v6, a3);
            }
        }
    }
}

// Added
int sithPuppet_FindHitLoc(sithThing* pReceiverThing, rdVector3* pPos)
{
	if (pReceiverThing->rdthing.model3 && pReceiverThing->animclass)
	{
		if (pReceiverThing->rdthing.frameTrue != rdroid_frameTrue)
			rdPuppet_BuildJointMatrices(&pReceiverThing->rdthing, &pReceiverThing->lookOrientation);
		
		// find the closest joint to this position
		//rdHierarchyNode* pFoundNode = NULL;
		int foundJoint = -1;
		float dist = 10000.0f;

		uint64_t jointBits = pReceiverThing->animclass->jointBits;
		while (jointBits != 0)
		{
			int jointIdx = stdMath_FindLSB64(jointBits);
			jointBits ^= 1ull << jointIdx;

#ifdef ANIMCLASS_NAMES
			int nodeIdx = pReceiverThing->animclass->bodypart[jointIdx].nodeIdx;
#else
			int nodeIdx = pReceiverThing->animclass->bodypart_to_joint[jointIdx];
#endif
			if(nodeIdx < 0 || nodeIdx >= pReceiverThing->rdthing.model3->numHierarchyNodes)
				continue;
		
			rdHierarchyNode* pNode = &pReceiverThing->rdthing.model3->hierarchyNodes[nodeIdx];
			int meshIdx = pNode->meshIdx;
			if (meshIdx == 0xffffffff)
				continue;
		
			rdModel3* pModel = pReceiverThing->rdthing.model3;
			int geoset = pReceiverThing->rdthing.geosetSelect;
			if (geoset == 0xffffffff)
				geoset = pModel->geosetSelect;
		
			rdMatrix34* meshMatrix = &pReceiverThing->rdthing.hierarchyNodeMatrices[pNode->idx];
		
			//float radius = pModel->geosets[geoset].meshes[pNode->meshIdx].radius;
			float distToCenter = rdVector_Dist3(pPos, &meshMatrix->scale);
			if (distToCenter < dist)
			{
				dist = distToCenter;
				foundJoint = jointIdx;
				//pFoundNode = pNode;
			}
		}
		
		//if(pFoundNode)
		//	std_pHS->messagePrint("hit node %s\n", pFoundNode->name);
		
		return foundJoint;
	}
	return -1;
}

#ifdef PUPPET_PHYSICS

#include "Modules/sith/Engine/sithConstraint.h"

void sithPuppet_AddDistanceConstraint(sithThing* pThing, int joint, int target)
{
	int hasJoint = pThing->animclass->jointBits & (1ull << joint);
	int hasTarget = pThing->animclass->jointBits & (1ull << target);
	if (!hasJoint || !hasTarget)
		return;

	sithThing* targetThing = &pThing->puppet->physics->joints[target].thing;
	sithThing* jointThing = &pThing->puppet->physics->joints[joint].thing;

	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pThing->animclass->bodypart[joint].nodeIdx];

	rdMatrix34 invTargetMat;
	targetThing->lookOrientation.scale = targetThing->position;
	rdMatrix_InvertOrtho34(&invTargetMat, &targetThing->lookOrientation);
	targetThing->lookOrientation.scale = rdroid_zeroVector3;

	rdMatrix34 invJointMat;
	jointThing->lookOrientation.scale = jointThing->position;
	rdMatrix_InvertOrtho34(&invJointMat, &jointThing->lookOrientation);
	jointThing->lookOrientation.scale = rdroid_zeroVector3;

	rdVector3 direction;
	rdVector_Sub3(&direction, &targetThing->position, &jointThing->position);
	rdVector_Normalize3Acc(&direction);
		
	rdVector3 contactPointA, contactPointB;
	rdVector_Scale3(&contactPointA, &direction, targetThing->moveSize);
	rdVector_Add3Acc(&contactPointA, &targetThing->position);
	
	rdVector_Scale3(&contactPointB, &direction, -jointThing->moveSize);
	rdVector_Add3Acc(&contactPointB, &jointThing->position);

	float d = rdVector_Dist3(&targetThing->position, &jointThing->position);
	float penetrationDepth = jointThing->moveSize + targetThing->moveSize - d;
	//if (penetrationDepth > 0.0f)
	//{
	//	contactPointA.x -= direction.x * (penetrationDepth / 2.0f);
	//	contactPointA.y -= direction.y * (penetrationDepth / 2.0f);
	//	contactPointA.z -= direction.z * (penetrationDepth / 2.0f);
	//
	//	contactPointB.x += direction.x * (penetrationDepth / 2.0f);
	//	contactPointB.y += direction.y * (penetrationDepth / 2.0f);
	//	contactPointB.z += direction.z * (penetrationDepth / 2.0f);
	//}

	rdVector3 anchor;
	rdVector_Add3(&anchor, &contactPointA, &contactPointB);
	rdVector_Scale3Acc(&anchor, 0.5f);

	rdMatrix_TransformPoint34(&contactPointA, &anchor, &invTargetMat);
	rdMatrix_TransformPoint34(&contactPointB, &anchor, &invJointMat);

	rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[target].nodeIdx].scale;
	rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[joint].nodeIdx].scale;

	// invert the matrix so we can get a local position for the constraint
	rdMatrix34 inv;
	rdMatrix_InvertOrtho34(&inv, &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[target].nodeIdx]);

	// derive the anchor from the base pose and the inverse target matrix
	rdMatrix_TransformPoint34(&anchor, pBasePosB, &inv);

	//anchor = pNode->pos;
	//rdVector_Add3Acc(&anchor, &pNode->pivot);
	//rdVector_Sub3Acc(&anchor, &pNode->parent->pivot);

	//rdVector_Sub3(&coneAxis, &pThing->rdthing.hierarchyNodeMatrices[pThing->animclass->bodypart[joint].nodeIdx].scale, &targetThing->position);
	//rdVector_Normalize3Acc(&coneAxis);

	sithConstraint_AddDistanceConstraint(pThing, jointThing, targetThing, &anchor, &rdroid_zeroVector3, 0.0f);
}

void sithPuppet_AddConeConstraint(sithThing* pThing, int joint, int target, int axis, int flip, float angle)
{
	int hasJoint = pThing->animclass->jointBits & (1ull << joint);
	int hasTarget = pThing->animclass->jointBits & (1ull << target);
	if (!hasJoint || !hasTarget)
		return;

	sithThing* targetThing = &pThing->puppet->physics->joints[target].thing;
	sithThing* jointThing = &pThing->puppet->physics->joints[joint].thing;

	rdVector3 coneAxis;
	if (axis == 0)
		coneAxis = rdroid_xVector3;
	else if (axis == 1)
		coneAxis = rdroid_yVector3;
	else
		coneAxis = rdroid_zVector3;

	if (flip)
		rdVector_Neg3Acc(&coneAxis);
	
	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pThing->animclass->bodypart[joint].nodeIdx];

	rdMatrix34 invTargetMat;
	targetThing->lookOrientation.scale = targetThing->position;
	rdMatrix_InvertOrtho34(&invTargetMat, &targetThing->lookOrientation);
	targetThing->lookOrientation.scale = rdroid_zeroVector3;

	rdMatrix34 invJointMat;
	jointThing->lookOrientation.scale = jointThing->position;
	rdMatrix_InvertOrtho34(&invJointMat, &jointThing->lookOrientation);
	jointThing->lookOrientation.scale = rdroid_zeroVector3;

	rdVector3 direction;
	rdVector_Sub3(&direction, &targetThing->position, &jointThing->position);
	rdVector_Normalize3Acc(&direction);

	rdVector3 contactPointA, contactPointB;
	rdVector_Scale3(&contactPointA, &direction, targetThing->moveSize);
	rdVector_Add3Acc(&contactPointA, &targetThing->position);

	rdVector_Scale3(&contactPointB, &direction, -jointThing->moveSize);
	rdVector_Add3Acc(&contactPointB, &jointThing->position);

	rdVector3 anchor;
	rdVector_Add3(&anchor, &contactPointA, &contactPointB);
	rdVector_Scale3Acc(&anchor, 0.5f);

	rdMatrix_TransformPoint34(&contactPointA, &anchor, &invTargetMat);
	rdMatrix_TransformPoint34(&contactPointB, &anchor, &invJointMat);

	// figure out the cone axis from the pose

//	rdVector3 basePosA = pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[target].nodeIdx].scale;
//	rdVector3 basePosB = pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[joint].nodeIdx].scale;
//
//	rdMatrix34 invParent;
//	rdMatrix_InvertOrtho34(&invParent, &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[target].nodeIdx]);
//
//	rdVector3 localPos;
//	rdMatrix_TransformVector34(&localPos, &basePosB, &invParent);
//
//	//rdVector_Neg3Acc(&localPos);
//	rdVector_Normalize3(&coneAxis, &localPos);

	sithConstraint_AddConeConstraint(pThing, jointThing, targetThing, &contactPointA, &coneAxis, angle, &coneAxis);
}



void sithPuppet_AddHingeConstraint(sithThing* pThing, int joint, int target, int axis, int flip, float minAngle, float maxAngle)
{
	int hasJoint = pThing->animclass->jointBits & (1ull << joint);
	int hasTarget = pThing->animclass->jointBits & (1ull << target);
	if (!hasJoint || !hasTarget)
		return;

	sithThing* targetThing = &pThing->puppet->physics->joints[target].thing;
	sithThing* jointThing = &pThing->puppet->physics->joints[joint].thing;

	rdVector3 coneAxis;
	if (axis == 0)
		coneAxis = rdroid_xVector3;
	else if (axis == 1)
		coneAxis = rdroid_yVector3;
	else
		coneAxis = rdroid_zVector3;

	if (flip)
		rdVector_Neg3Acc(&coneAxis);

	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pThing->animclass->bodypart[joint].nodeIdx];

	rdMatrix34 invTargetMat;
	targetThing->lookOrientation.scale = targetThing->position;
	rdMatrix_InvertOrtho34(&invTargetMat, &targetThing->lookOrientation);
	targetThing->lookOrientation.scale = rdroid_zeroVector3;

	rdMatrix34 invJointMat;
	jointThing->lookOrientation.scale = jointThing->position;
	rdMatrix_InvertOrtho34(&invJointMat, &jointThing->lookOrientation);
	jointThing->lookOrientation.scale = rdroid_zeroVector3;

	rdVector3 direction;
	rdVector_Sub3(&direction, &targetThing->position, &jointThing->position);
	rdVector_Normalize3Acc(&direction);

	rdVector3 contactPointA, contactPointB;
	rdVector_Scale3(&contactPointA, &direction, targetThing->moveSize);
	rdVector_Add3Acc(&contactPointA, &targetThing->position);

	rdVector_Scale3(&contactPointB, &direction, -jointThing->moveSize);
	rdVector_Add3Acc(&contactPointB, &jointThing->position);

	rdVector3 anchor;
	rdVector_Add3(&anchor, &contactPointA, &contactPointB);
	rdVector_Scale3Acc(&anchor, 0.5f);

	rdMatrix_TransformPoint34(&contactPointA, &anchor, &invTargetMat);
	rdMatrix_TransformPoint34(&contactPointB, &anchor, &invJointMat);

	// figure out the cone axis from the pose

//	rdVector3 basePosA = pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[target].nodeIdx].scale;
//	rdVector3 basePosB = pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[joint].nodeIdx].scale;
//
//	rdMatrix34 invParent;
//	rdMatrix_InvertOrtho34(&invParent, &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[target].nodeIdx]);
//
//	rdVector3 localPos;
//	rdMatrix_TransformVector34(&localPos, &basePosB, &invParent);
//
//	//rdVector_Neg3Acc(&localPos);
//	rdVector_Normalize3(&coneAxis, &localPos);

	rdVector3 jointAxis;
	rdVector_Neg3(&jointAxis, &coneAxis);

	sithConstraint_AddHingeConstraint(pThing, jointThing, targetThing, &coneAxis, &jointAxis, minAngle, maxAngle);
}

void sithPuppet_SetupJointThing(sithThing* pThing, sithThing* pJointThing, sithBodyPart* pBodyPart, rdHierarchyNode* pNode, int jointIdx, const rdVector3* pInitialVel)
{
	sithThing_DoesRdThingInit(pJointThing);
	pJointThing->rdthing.curGeoMode = 0;
	pJointThing->rdthing.desiredGeoMode = 0;
	pJointThing->thingflags = SITH_TF_INVISIBLE;
	pJointThing->thingIdx = (pThing->thingIdx << 16) + jointIdx;
	extern int sithThing_bInitted2;
	pJointThing->signature = sithThing_bInitted2++;
	pJointThing->thing_id = -1;
	pJointThing->type = SITH_THING_CORPSE;
	pJointThing->moveType = SITH_MT_PHYSICS;
	pJointThing->prev_thing = pThing;
	pJointThing->child_signature = pThing->signature;

	// initialize the position velocity using the animation frames
	rdVector_Copy3(&pJointThing->position, &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx].scale);

	// orient to the joint matrix
	rdMatrix_Copy34(&pJointThing->lookOrientation, &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx]);
	rdVector_Zero3(&pJointThing->lookOrientation.scale);

//	rdVector3 worldPivot;
//	rdMatrix_TransformVector34(&worldPivot, &pNode->pivot, &pThing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);
//	rdVector_Sub3Acc(&pJointThing->position, &worldPivot);
//	
//	rdVector3 worldParentPivot;
//	rdMatrix_TransformVector34(&worldParentPivot, &pNode->parent->pivot, &pThing->rdthing.hierarchyNodeMatrices[pNode->idx]);
//	rdVector_Add3Acc(&pJointThing->position, &worldParentPivot);


	rdVector3 pos;
	rdVector_Copy3(&pos, &pJointThing->position);
	
	pJointThing->moveSize = pJointThing->collideSize = 0.01f;

	// todo: precompute this
	if(pNode->meshIdx >= 0)
	{
		int lastGeoSet = pThing->rdthing.model3->numGeosets - 1;
		rdMesh* pMesh = &pThing->rdthing.model3->geosets[lastGeoSet].meshes[pNode->meshIdx];

		rdVector3 meshCenter = rdroid_zeroVector3;
		float minDist = 10000.0f;
		float maxDist = -1000.0f;
		for (int j = 0; j < pMesh->numVertices; j++)
		{
			rdVector3* vtx = &pMesh->vertices[j];
			float dist = rdVector_Len3(vtx);

			if (dist < minDist)
				minDist = dist;

			if (dist > maxDist)
				maxDist = dist;

			rdVector_Add3Acc(&meshCenter, vtx);
		}

		rdVector_InvScale3Acc(&meshCenter, (float)pMesh->numVertices);

		rdMatrix_TransformVector34Acc(&meshCenter, &pJointThing->lookOrientation);
		//rdVector_Add3Acc(&pos, &meshCenter);

		float avgDist = (minDist + maxDist) * 0.5f;
		pJointThing->moveSize = avgDist;
		pJointThing->collideSize = minDist * 0.5;
	}

	sithCollision_GetSectorLookAt(pThing->sector, &pJointThing->position, &pos, 0.0f);
	rdVector_Copy3(&pJointThing->position, &pos);

	//rdVector3 pivot;
	//rdMatrix_TransformVector34(&pivot, &pNode->pivot, &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx]);
	//rdVector_Add3Acc(&pJoint->thing.position, &pivot);

	if (pBodyPart->flags & JOINTFLAGS_PHYSICS)
		pJointThing->collide = SITH_COLLIDE_SPHERE;
	else
		pJointThing->collide = SITH_COLLIDE_NONE;

	if (pBodyPart->flags & JOINTFLAGS_PHYSICS)
	{
		// setup physics params
		_memcpy(&pJointThing->physicsParams, &pThing->physicsParams, sizeof(sithThingPhysParams));
		pJointThing->physicsParams.mass *= pBodyPart->mass;
		pJointThing->physicsParams.buoyancy *= pBodyPart->buoyancy;
		pJointThing->physicsParams.height = 0.0f;

		pJointThing->physicsParams.physflags = SITH_PF_FEELBLASTFORCE;
		pJointThing->physicsParams.physflags |= SITH_PF_ANGIMPULSE;

		// todo: it would probably be better to simply attach the root
		// and constrain the hip as a fixed joint and let everything else follow
		pJointThing->physicsParams.physflags |= SITH_PF_FLOORSTICK;

		//SITH_PF_USEGRAVITY = 0x1,
		//	SITH_PF_USESTHRUST = 0x2,
		//	SITH_PF_4 = 0x4,
		//	SITH_PF_8 = 0x8,
		//	SITH_PF_SURFACEALIGN = 0x10,
		//	SITH_PF_SURFACEBOUNCE = 0x20,
		//	SITH_PF_FLOORSTICK = 0x40,
		//	SITH_PF_WALLSTICK = 0x80,
		//	SITH_PF_ATTACHED = 0x100,
		//	SITH_PF_ROTVEL = 0x200,
		//	SITH_PF_BANKEDTURNS = 0x400,
		//	SITH_PF_NOWALLGRAVITY = 0x800,
		//	SITH_PF_ANGTHRUST = 0x1000,
		//	SITH_PF_FLY = 0x2000,
		//	SITH_PF_FEELBLASTFORCE = 0x4000,
		//	SITH_PF_HAS_FORCE = 0x8000,
		//	SITH_PF_CROUCHING = 0x10000,
		//	SITH_PF_DONTROTATEVEL = 0x20000,
		//	SITH_PF_PARTIALGRAVITY = 0x40000,
		//	SITH_PF_80000 = 0x80000,
		//	SITH_PF_WATERSURFACE = 0x100000,
		//	SITH_PF_200000 = 0x200000,
		//	SITH_PF_NOTHRUST = 0x400000,
		//	SITH_PF_800000 = 0x800000,
		//	SITH_PF_1000000 = 0x1000000, // Jones: minecar
		//	/ = 0x2000000, // Jones: raft
		//	SITH_PF_4000000 = 0x4000000, // Jones: jeep
		//	SITH_PF_8000000 = 0x8000000,

		if (pThing->physicsParams.physflags & SITH_PF_USEGRAVITY)
			pJointThing->physicsParams.physflags |= SITH_PF_USEGRAVITY;

		if (pThing->physicsParams.physflags & SITH_PF_PARTIALGRAVITY)
			pJointThing->physicsParams.physflags |= SITH_PF_PARTIALGRAVITY;

		if (pThing->physicsParams.physflags & SITH_PF_NOTHRUST)
			pJointThing->physicsParams.physflags |= SITH_PF_NOTHRUST;

		if (pBodyPart->flags & JOINTFLAGS_BOUNCE)
			pJointThing->physicsParams.physflags |= SITH_PF_SURFACEBOUNCE;

		pJointThing->physicsParams.staticDrag = fmax(pJointThing->physicsParams.staticDrag, 0.05f);
		//pJointThing->physicsParams.airDrag *= 5.0f;
		//pJointThing->physicsParams.surfaceDrag *= 5.0f;

		rdVector3 vel;
		rdMatrix_TransformVector34(&vel, &pThing->rdthing.paHierarchyNodeVelocities[pNode->idx], &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx]);
		rdVector_Copy3(&pJointThing->physicsParams.vel, &vel);
		rdVector_Add3Acc(&pJointThing->physicsParams.vel, pInitialVel);
		//rdVector_Copy3(&pJointThing->physicsParams.angVel, &pThing->rdthing.paHierarchyNodeAngularVelocities[pNode->idx]);
		sithPhysics_AnglesToAngularVelocity(&pJointThing->physicsParams.angVel, &pThing->rdthing.paHierarchyNodeAngularVelocities[pNode->idx], &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx]);
	}

	// enter the things sector to start physics
	sithThing_EnterSector(pJointThing, pThing->sector, 1, 0);
}

void sithPuppet_StartPhysics(sithThing* pThing, rdVector3* pInitialVel, float deltaSeconds)
{
	if (!pThing->animclass || pThing->rdthing.type != RD_THINGTYPE_MODEL || !pThing->rdthing.model3 || !pThing->puppet || !pThing->rdthing.puppet || (g_debugmodeFlags & DEBUGFLAG_NO_PUPPETS))
		return;

	if(pThing->puppet->physics)
		sithPuppet_StopPhysics(pThing);

	sithPuppetPhysics* result = (sithPuppetPhysics*)pSithHS->alloc(sizeof(sithPuppetPhysics));
	if(!result)
		return;
	_memset(result, 0, sizeof(sithPuppetPhysics));

	pThing->puppet->physics = result;

	if (pThing->rdthing.paHiearchyNodeMatrixOverrides)
		memset(pThing->rdthing.paHiearchyNodeMatrixOverrides, NULL, sizeof(rdMatrix34*) * pThing->rdthing.model3->numHierarchyNodes);

	rdVector3 thingVel;
	rdVector_Scale3(&thingVel, pInitialVel, deltaSeconds);
	
	// give the animation some time to prime just in case this was trigger between animations
	//rdPuppet_UpdateTracks(pThing->rdthing.puppet, deltaSeconds);

	if(pThing->rdthing.frameTrue != rdroid_frameTrue)
	{
		rdVector_Copy3(&pThing->lookOrientation.scale, &pThing->position);
		rdPuppet_BuildJointMatrices(&pThing->rdthing, &pThing->lookOrientation);
		rdVector_Zero3(&pThing->lookOrientation.scale);
	}

	if (pThing->rdthing.paHierarchyNodeMatricesPrev) // todo: only do this when needed
		_memcpy(pThing->rdthing.paHierarchyNodeMatricesPrev, pThing->rdthing.hierarchyNodeMatrices, sizeof(rdMatrix34) * pThing->rdthing.model3->numHierarchyNodes);

	uint64_t jointBits = pThing->animclass->jointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
		rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];
		//if (pNode->meshIdx < 0)
			//continue;

		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];

		pJoint->localMat = pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx];
		sithPuppet_SetupJointThing(pThing, &pJoint->thing, pBodyPart, pNode, jointIdx, pInitialVel);
	}

	//sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, 2, 1, 35.0f);
	//sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, 2, 1, 35.0f);
	sithPuppet_AddHingeConstraint(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, 2, 1, -45.0, 5.0f);
	sithPuppet_AddHingeConstraint(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, 2, 1, -45.0, 5.0f);
	
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, 2, 1, 35.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, 2, 1, 35.0f);
	//sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 2, 1, 20.0f);
	//sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 2, 1, 20.0f);

	sithPuppet_AddHingeConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 2, 1, 0.0, 70);
	sithPuppet_AddHingeConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 2, 1, 0.0, 70);

	//sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 2, 1, 30.0f);
	//sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 2, 1, 30.0f);

	sithPuppet_AddHingeConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 2, 1, 0.0, 70);
	sithPuppet_AddHingeConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 2, 1, 0.0, 70);

	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, 2, 1, 30.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, 2, 1, 30.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 0, 0, 100.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 0, 1, 100.0f);
	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
	{
		sithPuppet_AddConeConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 2, 0, 15.0f);
		sithPuppet_AddHingeConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 2, 1, -5.0, 15.0f);
		//sithPuppet_AddConeConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 2, 0, 15.0f);
	}
	else
	{
		sithPuppet_AddConeConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 2, 0, 15.0f);
	}
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 2, 0, 25.0f);

	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF);
		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF);

	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO);
	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
	{
		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK);
		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO);
	}
	else
	{
		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO);
	}	
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP);
}

void sithPuppet_StopPhysics(sithThing* pThing)
{
	if (pThing->puppet && pThing->puppet->physics)
	{
		uint64_t jointBits = pThing->animclass->jointBits;
		while (jointBits != 0)
		{
			int jointIdx = stdMath_FindLSB64(jointBits);
			jointBits ^= 1ull << jointIdx;

			sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
			sithThing_LeaveSector(&pJoint->thing);
			sithThing_FreeEverything(&pJoint->thing);
			rdThing_FreeEntry(&pJoint->thing.rdthing);
		}
		pSithHS->free(pThing->puppet->physics);
		pThing->puppet->physics = 0;
	}
}

void sithPuppet_UpdateJointMatrices(sithThing* thing)
{
	rdMatrix34 invMat;
	rdMatrix_InvertOrtho34(&invMat, &thing->lookOrientation);

	uint64_t jointBits = thing->animclass->physicsJointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithBodyPart* pBodyPart = &thing->animclass->bodypart[jointIdx];
		if (pBodyPart->nodeIdx < thing->rdthing.rootJoint || thing->rdthing.amputatedJoints[pBodyPart->nodeIdx])
		{
			// make sure this is cleared
			thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = NULL;
			continue;
		}

		sithPuppetJoint* pJoint = &thing->puppet->physics->joints[jointIdx];
		rdHierarchyNode* pNode = &thing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];

		//rdMatrix_Copy34(&pJoint->localMat, &pJoint->thing.lookOrientation);
		//rdVector_Copy3(&pJoint->localMat.scale, &pJoint->thing.position);
		
		rdVector3 oldPos = pJoint->localMat.scale;
		rdQuat q0, q1;
		rdQuat_BuildFrom34(&q0, &pJoint->localMat);
		rdQuat_BuildFrom34(&q1, &pJoint->thing.lookOrientation);
	
		rdQuat q;
		rdQuat_Slerp(&q, &q0, &q1, 0.3f);
	
		rdQuat_ToMatrix(&pJoint->localMat, &q);
	
		rdVector_Lerp3(&pJoint->localMat.scale, &oldPos, &pJoint->thing.position, 0.3f);
		//pJoint->thing.lookOrientation.scale = pJoint->thing.position;
		//
		//rdMatrix34* parentWorldMatrix = &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx];
		//
		//// Step 1: Extract the parent's world transform and invert it to bring the node back to local space
		//rdMatrix34 parentWorldMatrixInv;
		//rdMatrix_InvertOrtho34(&parentWorldMatrixInv, parentWorldMatrix);
		//
		//// Step 2: Apply the parent's inverse transform to the current world matrix (undo parent's transform)
		//rdMatrix34 localMatrixTemp;
		//rdMatrix_Multiply34(&localMatrixTemp, &parentWorldMatrixInv, &pJoint->thing.lookOrientation);
		//
		//// Step 3: Undo the parent pivot (we subtracted it when going from local to world space, so we add it back)
		//rdMatrix34 parentPivotMatrix;
		//rdVector3 parentPivotNeg = { pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->parent->pivot. z };  // Positive pivot in local space
		//rdMatrix_BuildTranslate34(&parentPivotMatrix, &parentPivotNeg);
		//rdMatrix_PreMultiply34(&localMatrixTemp, &parentPivotMatrix);
		//
		//// Step 4: Now, the current node's local matrix is stored in localMatrixTemp
		//pJoint->localMat = localMatrixTemp;
		//
		//// Step 5: The position is extracted from the local transform, adjusting for the joint pivot
		//rdVector3 jointPivotNeg = { -pNode->pivot.x, -pNode->pivot.y, -pNode->pivot.z };  // Inverse of joint pivot in local space
		//rdMatrix34 localPositionMatrix;
		//rdMatrix_BuildTranslate34(&localPositionMatrix, &jointPivotNeg);
		//rdMatrix_Multiply34(&localPositionMatrix, &localMatrixTemp, &localPositionMatrix);
		//pJoint->localMat.scale = localPositionMatrix.scale;
		//
		//pJoint->thing.lookOrientation.scale = rdroid_zeroVector3;

		thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->localMat;
	}
}

inline void sithPuppet_UpdateJointThing(sithThing* pThing, sithThing* pJointThing, sithBodyPart* pBodyPart, float deltaSeconds)
{
	// don't collide if the node is amputated or lower than the root joint
	// (but update the position for sector traversal)
	int collide = pJointThing->collide;
	if (pBodyPart->nodeIdx < pThing->rdthing.rootJoint || pThing->rdthing.amputatedJoints[pBodyPart->nodeIdx])
		pJointThing->collide = SITH_COLLIDE_NONE;

 	sithPhysics_ThingTick(pJointThing, deltaSeconds);
	sithThing_TickPhysics(pJointThing, deltaSeconds);

	// reset collision
	pJointThing->collide = collide;
}

void sithPuppet_UpdateJoints(sithThing* pThing, float deltaSeconds)
{
	uint64_t jointBits = pThing->animclass->physicsJointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		sithPuppet_UpdateJointThing(pThing, &pJoint->thing, pBodyPart, deltaSeconds);
	}
}

void sithPuppet_StopAll(sithThing* pThing)
{
	uint64_t jointBits = pThing->animclass->physicsJointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		sithPhysics_ThingStop(&pJoint->thing);
	}
}

int sithbPuppet_IsAtRest(sithThing* thing, float deltaSeconds)
{
	if (thing->physicsParams.atRest > 0)
		return 1;
	return 0;
}

void sithPuppet_UpdatePhysicsAnim(sithThing* thing, float deltaSeconds)
{
	if (thing->physicsParams.atRest > 0)
		return;

	sithPuppet_UpdateJoints(thing, deltaSeconds);
	sithPuppet_UpdateJointMatrices(thing);

	// pin the thing to the root joint
	int rootJoint = thing->animclass->root < 0 ? JOINTTYPE_HIP : thing->animclass->root;
	sithPuppetJoint* pJoint = &thing->puppet->physics->joints[rootJoint];
	rdVector_Copy3(&thing->position, &pJoint->thing.position);
	sithThing_MoveToSector(thing, pJoint->thing.sector, 0);

	thing->physicsParams.atRest = sithbPuppet_IsAtRest(thing, deltaSeconds);
	if (thing->physicsParams.atRest)
	{
		sithPuppet_StopAll(thing);
	}
}

#endif

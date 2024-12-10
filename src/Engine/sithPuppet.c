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

#ifdef PUPPET_PHYSICS
static int sithPuppet_jointChildren[] =
{
	-1,							// JOINTTYPE_HEAD
	JOINTTYPE_HEAD,				// JOINTTYPE_NECK
	JOINTTYPE_NECK,				// JOINTTYPE_TORSO
	-1,							// JOINTTYPE_PRIMARYWEAP
	-1,							// JOINTTYPE_SECONDARYWEAP
	JOINTTYPE_PRIMARYWEAP,		// JOINTTYPE_PRIMARYWEAPJOINT
	JOINTTYPE_SECONDARYWEAP,	// JOINTTYPE_SECONDARYWEAPJOINT
	-1,							// JOINTTYPE_TURRETPITCH
	-1,							// JOINTTYPE_TURRETYAW
	JOINTTYPE_TORSO,			// JOINTTYPE_HIP
	JOINTTYPE_RFOREARM,			// JOINTTYPE_RSHOULDER
	JOINTTYPE_LFOREARM,			// JOINTTYPE_LSHOULDER
	JOINTTYPE_RHAND,			// JOINTTYPE_RFOREARM
	JOINTTYPE_LHAND,			// JOINTTYPE_LFOREARM
	-1,							// JOINTTYPE_RHAND
	-1,							// JOINTTYPE_LHAND
	JOINTTYPE_RCALF,			// JOINTTYPE_RTHIGH
	JOINTTYPE_LCALF,			// JOINTTYPE_LTHIGH
	JOINTTYPE_RFOOT,			// JOINTTYPE_RCALF
	JOINTTYPE_LFOOT,			// JOINTTYPE_LCALF
	-1,							// JOINTTYPE_RFOOT
	-1,							// JOINTTYPE_LFOOT
};
#endif

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
#ifdef PUPPET_PHYSICS
	result->pParent = thing;
	_memset(result->joints, 0, sizeof(result->joints));
	result->lastTimeStep = sithTime_deltaSeconds;
#endif
    return result;
}

void sithPuppet_FreeEntry(sithThing *puppet)
{
    if ( puppet->puppet )
    {
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
			// do physicalized animation if necessary
			if(thing->puppet->physicalized && jkPlayer_ragdolls)
			{
				sithPuppet_UpdatePhysicsAnim(thing, deltaSeconds);
			}
			else // otherwise update the animation
		#endif
			{

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
		for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
		{
#ifdef ANIMCLASS_NAMES
			int jointIdx = pReceiverThing->animclass->bodypart[i].jointIdx;
#else
			int jointIdx = pReceiverThing->animclass->bodypart_to_joint[i];
#endif
			if(jointIdx < 0 || jointIdx >= pReceiverThing->rdthing.model3->numHierarchyNodes)
				continue;
		
			rdHierarchyNode* pNode = &pReceiverThing->rdthing.model3->hierarchyNodes[jointIdx];
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
				foundJoint = i;
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

int sithPuppet_GetJointNodeIndex(sithThing* pThing, int idx)
{
	sithBodyPart* pBodypart = &pThing->animclass->bodypart[idx];
	if(pBodypart->flags & JOINTFLAGS_PHYSICS)
		return pThing->animclass->bodypart[idx].jointIdx;
	return -1;
}

void sithPuppet_CalculateRagdollCenterRadius(sithThing* pThing)
{
	rdVector_Zero3(&pThing->puppet->center);
	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
	{
		sithPuppetJoint* pJoint = &pThing->puppet->joints[i];
		rdVector_Add3Acc(&pThing->puppet->center, &pJoint->pos);
	}
	rdVector_InvScale3Acc(&pThing->puppet->center, (float)JOINTTYPE_NUM_JOINTS);

	pThing->puppet->radius = 0.0f;
	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
	{
		sithPuppetJoint* pJoint = &pThing->puppet->joints[i];
		pThing->puppet->radius = fmax(pThing->puppet->radius, rdVector_Dist3(&pJoint->pos, &pThing->puppet->center));
	}
}

void sithPuppet_Physicalize(sithThing* pThing, rdVector3* pInitialVel)
{
	if (!pThing->animclass || pThing->rdthing.type != RD_THINGTYPE_MODEL || !pThing->rdthing.model3 || !pThing->puppet || !pThing->rdthing.puppet || (g_debugmodeFlags & DEBUGFLAG_NO_PUPPETS))
		return;

//	rdPuppet_BuildJointMatrices(&pThing->rdthing, &pThing->lookOrientation);

	if (pThing->rdthing.paHiearchyNodeMatrixOverrides)
		memset(pThing->rdthing.paHiearchyNodeMatrixOverrides, NULL, sizeof(rdMatrix34*) * pThing->rdthing.model3->numHierarchyNodes);

	rdVector3 thingVel;
	rdVector_Scale3(&thingVel, pInitialVel, sithTime_deltaSeconds);

	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; i++)
	{
		int nodeIdx = pThing->animclass->bodypart[i].jointIdx;//sithPuppet_GetJointNodeIndex(pThing, i);
		if(nodeIdx < 0)
			continue;

		rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[nodeIdx];

		sithPuppetJoint* pJoint = &pThing->puppet->joints[i];
		pJoint->flags = pThing->animclass->bodypart[i].flags;

		// pin the hip to the thing origin
		//if(i == JOINTTYPE_HIP)
		//	pJoint->flags |= JOINTFLAGS_ROOT | JOINTFLAGS_PINNED;

		rdMatrix_Copy34(&pJoint->lookOrient, &pThing->rdthing.hierarchyNodeMatrices[nodeIdx]);

		//rdVector3 pivotWS;
		//rdMatrix_TransformVector34(&pivotWS, &pNode->pivot, &pThing->rdthing.hierarchyNodeMatrices[nodeIdx]);


		rdVector_Copy3(&pJoint->pos, &pThing->rdthing.hierarchyNodeMatrices[nodeIdx].scale);
		rdVector_Copy3(&pJoint->lastPos, &pThing->rdthing.paHierarchyNodeMatricesPrev[nodeIdx].scale);

		//rdVector_Sub3Acc(&pJoint->pos, &pivotWS);
	//	rdVector_Sub3Acc(&pJoint->lastPos, &pivotWS);

		// add the initial velocity
		//rdVector_Add3Acc(&pJoint->pos, &thingVel);

		// clear next position accumulator
		rdVector_Zero3(&pJoint->nextPosAcc);
		pJoint->nextPosWeight = 0.0f;

		// todo: we're gonna need a better way of deciding how big of a radius to make for each joint
		//if (pNode->meshIdx != -1)
		//	pJoint->radius = pThing->rdthing.model3->geosets[0].meshes[pNode->meshIdx].radius * 0.35f;
		//else
			pJoint->radius = 0.01f;

		// the joint contains a dummy thing for collision handling
		sithThing_DoesRdThingInit(&pJoint->thing);
		pJoint->thing.thingIdx = -1;
		pJoint->thing.signature = -1;
		pJoint->thing.thing_id = -1;
		pJoint->thing.type = SITH_THING_ACTOR;
		pJoint->thing.collide = SITH_COLLIDE_SPHERE;
		pJoint->thing.moveSize = pJoint->radius;
		pJoint->thing.collideSize = pJoint->radius;
		pJoint->thing.moveType = SITH_MT_NONE;
		pJoint->thing.prev_thing = pThing;
		pJoint->thing.child_signature = pThing->signature;
		_memcpy(&pJoint->thing.physicsParams, &pThing->physicsParams, sizeof(sithThingPhysParams));
		pJoint->thing.physicsParams.physflags = 0;
		rdMatrix_Identity34(&pJoint->thing.lookOrientation); // don't think we care about orientation?
		pJoint->thing.parentThing = pThing;
	}

	sithPuppet_UpdateJointMatrices(pThing, 1);
	sithPuppet_CalculateRagdollCenterRadius(pThing);

	// mark for physics
	pThing->puppet->physicalized = 1;
}

void sithPuppet_Unphysicalize(sithThing* pThing)
{
	pThing->puppet->physicalized = 0;
	if (pThing->rdthing.paHiearchyNodeMatrixOverrides)
		memset(pThing->rdthing.paHiearchyNodeMatrixOverrides, NULL, sizeof(rdMatrix34*) * pThing->rdthing.model3->numHierarchyNodes);
}

void sithPuppet_ConstrainJoints(sithThing* pThing, int jointA, int jointB, float minDistance)
{
	int nodeIdxA = sithPuppet_GetJointNodeIndex(pThing, jointA);
	int nodeIdxB = sithPuppet_GetJointNodeIndex(pThing, jointB);

	if(nodeIdxA < 0 || nodeIdxB < 0)
		return;

	//if (pThing->rdthing.amputatedJoints[nodeIdxA] || pThing->rdthing.amputatedJoints[nodeIdxB])
	//	return;

	// todo: precompute the constraint distance
	rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[nodeIdxA].scale;
	rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[nodeIdxB].scale;
	float distance = rdVector_Dist3(pBasePosB, pBasePosA) * 0.95f;

	sithPuppetJoint* pJointA = &pThing->puppet->joints[jointA];
	sithPuppetJoint* pJointB = &pThing->puppet->joints[jointB];

	// calculate the delta
	rdVector3 delta;
	rdVector_Sub3(&delta, &pJointB->pos, &pJointA->pos);

	//float deltaLen = rdVector_Normalize3Acc(&delta);
	float deltaLen = rdVector_Len3(&delta);
	if(minDistance >= 0 && deltaLen > distance * minDistance)
		return;
		
	float invMassA = 1.0f / (pThing->animclass->bodypart[jointA].mass);
	float invMassB = 1.0f / (pThing->animclass->bodypart[jointB].mass);
	float diff = (deltaLen - distance) / (deltaLen * (invMassA + invMassB));
		
	if (!(pJointA->flags & JOINTFLAGS_PINNED))
	{
		rdVector3 deltaA;
		rdVector_Scale3(&deltaA, &delta, (double)diff * invMassA);
		
		rdVector3 offsetA;
		rdVector_Add3(&offsetA, &pJointA->pos, &deltaA);
		rdVector_Add3Acc(&pJointA->nextPosAcc, &offsetA);
		pJointA->nextPosWeight++;
	}

	if (!(pJointB->flags & JOINTFLAGS_PINNED))
	{
		rdVector3 deltaB;
		rdVector_Scale3(&deltaB, &delta, (double)diff * invMassB);

		rdVector3 offsetB;
		rdVector_Sub3(&offsetB, &pJointB->pos, &deltaB);
		rdVector_Add3Acc(&pJointB->nextPosAcc, &offsetB);
		pJointB->nextPosWeight++;
	}
}

void sithPuppet_ConstrainBody(sithThing* pThing)
{
	// head constraints
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HEAD,      JOINTTYPE_NECK, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HEAD,     JOINTTYPE_TORSO, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HEAD, JOINTTYPE_RSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HEAD, JOINTTYPE_LSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HEAD,       JOINTTYPE_HIP, -1);

	// neck constraints
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_NECK,     JOINTTYPE_TORSO, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_NECK, JOINTTYPE_RSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_NECK, JOINTTYPE_LSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_NECK,       JOINTTYPE_HIP, -1);

	// torso constraints
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO,       JOINTTYPE_HIP, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO, JOINTTYPE_RSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO, JOINTTYPE_LSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO,    JOINTTYPE_RTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO,    JOINTTYPE_LTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO,     JOINTTYPE_LFOOT, 0.8f); // prevent torso getting too close to foot
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO,     JOINTTYPE_RFOOT, 0.8f); // prevent torso getting too close to foot
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO,     JOINTTYPE_LCALF, 0.9f); // prevent calf getting too close to torso
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_TORSO,     JOINTTYPE_RCALF, 0.9f); // prevent calf getting too close to torso

	// hip constraints
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HIP, JOINTTYPE_RSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HIP, JOINTTYPE_LSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HIP,    JOINTTYPE_RTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HIP,    JOINTTYPE_LTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HIP,     JOINTTYPE_LCALF, 0.9f); // prevent calf getting too close to hip
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_HIP,     JOINTTYPE_RCALF, 0.9f); // prevent calf getting too close to hip

	// arm constraints
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_LSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RFOREARM,   JOINTTYPE_LFOREARM, 0.75f); // prevent forearms from getting too close together
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RHAND,         JOINTTYPE_LHAND, 0.8f); // prevent hands getting too close together
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_LFOREARM,      JOINTTYPE_RHAND, 0.75f); // prevent calf getting too close to other foot
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RFOREARM,      JOINTTYPE_LHAND, 0.75f); // prevent calf getting too close to other foot

	// right arm
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing,    JOINTTYPE_RHAND,  JOINTTYPE_RFOREARM, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_RTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_LTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_LHAND, 0.75f); // prevent forearm getting too close to other hand

	// left arm
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, -1);
	sithPuppet_ConstrainJoints(pThing,    JOINTTYPE_LHAND,  JOINTTYPE_LFOREARM, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_RTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_LTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_RHAND, 0.75f); // prevent forearm getting too close to other hand

	// leg constraints
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_LTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RCALF, JOINTTYPE_LCALF, 0.8f); // prevent calfs getting too close together
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RFOOT, JOINTTYPE_LFOOT, 0.7f); // prevent feet getting too close together
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_LCALF, JOINTTYPE_RFOOT, 0.75f); // prevent calf getting too close to other foot
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RCALF, JOINTTYPE_LFOOT, 0.75f); // prevent calf getting too close to other foot

	// right leg
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_RFOOT,  JOINTTYPE_RCALF, -1);

	// left arm
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_LFOOT,  JOINTTYPE_LCALF, -1);

	// weapon
	sithPuppet_ConstrainJoints(pThing,   JOINTTYPE_PRIMARYWEAP, JOINTTYPE_RHAND, -1);
	sithPuppet_ConstrainJoints(pThing, JOINTTYPE_SECONDARYWEAP, JOINTTYPE_LHAND, -1);
}

// updates a joints matrix using a reference triangle to attempt to orient the joint correctly (since we don't handle rotation in the joint system)
void sithPuppet_UpdateJointMatrix(sithThing* pThing, int joint, int refJointA, int refJointB, int refJointC, int init)
{
	int nodeIdx     = pThing->animclass->bodypart[joint].jointIdx;//sithPuppet_GetJointNodeIndex(pThing, joint);
	int refNodeIdxA = pThing->animclass->bodypart[refJointA].jointIdx;//sithPuppet_GetJointNodeIndex(pThing, refJointA);
	int refNodeIdxB = pThing->animclass->bodypart[refJointB].jointIdx;//sithPuppet_GetJointNodeIndex(pThing, refJointB);
	int refNodeIdxC = pThing->animclass->bodypart[refJointC].jointIdx;//sithPuppet_GetJointNodeIndex(pThing, refJointC);

	if (nodeIdx < 0 || refNodeIdxA < 0 || refNodeIdxB < 0 || refNodeIdxC < 0)
		return;

	//if (pThing->rdthing.amputatedJoints[nodeIdx] || pThing->rdthing.amputatedJoints[refNodeIdxA] || pThing->rdthing.amputatedJoints[refNodeIdxB])
	//	return;
	
	sithPuppetJoint* pJoint = &pThing->puppet->joints[joint];
	//if(pJoint->flags & JOINTFLAGS_ROOT)
		//return;

	sithPuppetJoint* pRefJointA = &pThing->puppet->joints[refJointA];
	sithPuppetJoint* pRefJointB = &pThing->puppet->joints[refJointB];
	sithPuppetJoint* pRefJointC = &pThing->puppet->joints[refJointC];

	const rdVector3* pPos0 = &pJoint->pos;
	const rdVector3* pPos1 = &pRefJointA->pos;
	const rdVector3* pPos2 = &pRefJointB->pos;
	const rdVector3* pPos3 = &pRefJointC->pos;

	rdMatrix34 m;
	rdMatrix34* pMat = init ? &m : &pJoint->tmpMat;
	rdMatrix_Identity34(pMat);

	rdVector_Sub3(&pMat->uvec, pPos2, pPos1);
	rdVector_Normalize3Acc(&pMat->uvec);

	rdVector_Sub3(&pMat->rvec, pPos3, pPos1);
	rdVector_Normalize3Acc(&pMat->rvec);

	rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);
	rdVector_Normalize3Acc(&pMat->lvec);

	rdVector_Cross3(&pMat->rvec, &pMat->lvec, &pMat->uvec);
	rdVector_Normalize3Acc(&pMat->rvec);

	if(init)
	{
		rdMatrix34 invMat;
		rdMatrix_InvertOrtho34(&invMat, &m);
		rdMatrix_Multiply34(&pJoint->refMat, &invMat, &pThing->rdthing.hierarchyNodeMatrices[nodeIdx]);
	}
}

void sithPuppet_UpdateJointMatrices(sithThing* pThing, int init)
{
	// orient the head and torso with respect to the shoulders
	sithPuppet_UpdateJointMatrix(pThing,  JOINTTYPE_HEAD, JOINTTYPE_HEAD, JOINTTYPE_RSHOULDER, JOINTTYPE_LSHOULDER, init);
	sithPuppet_UpdateJointMatrix(pThing,  JOINTTYPE_NECK, JOINTTYPE_HEAD, JOINTTYPE_RSHOULDER, JOINTTYPE_LSHOULDER, init);
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_TORSO,  JOINTTYPE_HIP, JOINTTYPE_RSHOULDER, JOINTTYPE_LSHOULDER, init);

	// orient the hip with respect to the legs
	sithPuppet_UpdateJointMatrix(pThing,   JOINTTYPE_HIP, JOINTTYPE_TORSO, JOINTTYPE_LTHIGH, JOINTTYPE_RTHIGH, init);

	// orient the shoulders with respect to the arms and hip
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_LSHOULDER, JOINTTYPE_LFOREARM, JOINTTYPE_HIP, init);
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_RSHOULDER, JOINTTYPE_RFOREARM, JOINTTYPE_HIP, init);

	// orient the hands with respect to the forearms and hip
	sithPuppet_UpdateJointMatrix(pThing,     JOINTTYPE_LHAND, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, JOINTTYPE_HIP, init);
	sithPuppet_UpdateJointMatrix(pThing,     JOINTTYPE_RHAND, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, JOINTTYPE_HIP, init);
	
	// orient the thigh with respect to the calves and hip
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_LTHIGH, JOINTTYPE_LCALF, JOINTTYPE_HIP, init);
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_RTHIGH, JOINTTYPE_RCALF, JOINTTYPE_HIP, init);

	// orient the calves with respect to the thighs and hip
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_RCALF, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, init);
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_LCALF, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, init);

	// orient the forearms with respect to the shoulders and torso
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, init);
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, init);

	// orient the feet with respect to the calves and hip
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, JOINTTYPE_HIP, init);
	sithPuppet_UpdateJointMatrix(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, JOINTTYPE_HIP, init);
}

void sithPuppet_ApplyJointMatrices(sithThing* thing)
{
	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
	{
		int nodeIdx = sithPuppet_GetJointNodeIndex(thing, i);
		if (nodeIdx < 0 || thing->rdthing.amputatedJoints[nodeIdx]) // don't need to do anything for amputated joints
			continue;

		sithPuppetJoint* pJoint = &thing->puppet->joints[i];
		rdMatrix_Multiply34(&pJoint->lookOrient, &pJoint->tmpMat, &pJoint->refMat);
		rdVector_Copy3(&pJoint->lookOrient.scale, &pJoint->pos);

		thing->rdthing.paHiearchyNodeMatrixOverrides[nodeIdx] = &pJoint->lookOrient;
	}
}

void sithPuppet_UpdateOrientation(rdQuat* outQuat, const rdVector3* translation, const rdQuat* orientation)
{
	float angle = rdVector_Len3(translation) * (180.0f / 3.141592f);

	rdQuat rotation;
	rdQuat_BuildFromAxisAngle(&rotation, translation, angle);

	rdQuat_Mul(outQuat, &rotation, orientation);
}

int sithPuppet_CollideJoint(sithSector* sector, sithThing* pThing, rdVector3* pos, rdVector3* dir, float radius, rdVector3* hitNormOut)
{
	uint32_t collideFlags = /*SITH_RAYCAST_ONLY_COG_THINGS | */ /*RAYCAST_800 |*/ RAYCAST_2;

	int result = 0;
	rdVector3 dirNorm;
	float dirLen = rdVector_Normalize3(&dirNorm, dir);
	sithCollision_SearchRadiusForThings(sector, pThing, pos, &dirNorm, dirLen, radius, collideFlags);
	for (sithCollisionSearchEntry* pEntry = sithCollision_NextSearchResult(); pEntry; pEntry = sithCollision_NextSearchResult())
	{
		if ((pEntry->hitType & SITHCOLLISION_WORLD) != 0)
		{
			rdVector_Copy3(hitNormOut, &pEntry->hitNorm);
			result = 1;
			break;
		}
		if ((pEntry->hitType & SITHCOLLISION_THING) != 0)
		{
			int parent = sithThing_GetParent(pThing);

			if (pEntry->receiver != pThing && pEntry->receiver->thingIdx != parent && pEntry->face)
			{
				rdVector_Copy3(hitNormOut, &pEntry->hitNorm);
				result = 1;
				break;
			}
		}
	}
	sithCollision_SearchClose();

	if (!result)
		rdVector_Zero3(hitNormOut);

	return result;
}

void sithPuppet_UpdateJointPositions(sithSector* sector, sithThing* pThing, float deltaSeconds)
{
	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
	{
		int nodeIdx = sithPuppet_GetJointNodeIndex(pThing, i);
		if (nodeIdx < 0)// || pThing->rdthing.amputatedJoints[nodeIdx])
			continue;

		sithPuppetJoint* pJoint = &pThing->puppet->joints[i];
		//if (pJoint->flags & JOINTFLAGS_ROOT)
		//{
		//	rdVector_Copy3(&pJoint->pos, &pThing->position);
		//	continue;
		//}

		if (pJoint->nextPosWeight > 0.0)
		{
			rdVector_Copy3(&pJoint->thing.position, &pJoint->pos);

			// normalize the new position accumulator
			rdVector_InvScale3Acc(&pJoint->nextPosAcc, pJoint->nextPosWeight);

			rdVector3 vel;
			rdVector_Sub3(&vel, &pJoint->nextPosAcc, &pJoint->pos);
			rdVector_ClipPrecision3(&vel);
			if (!rdVector_IsZero3(&vel))
			{
				rdVector3 hitNorm;
				if (pThing->rdthing.amputatedJoints[nodeIdx] || !sithPuppet_CollideJoint(sector, &pJoint->thing, &pJoint->nextPosAcc, &vel, pJoint->radius, &hitNorm))
				{
					rdVector_Copy3(&pJoint->pos, &pJoint->nextPosAcc);
				}
				else
				{
					rdVector_Sub3(&vel, &pJoint->nextPosAcc, &pJoint->lastPos);

					float dot = rdVector_Dot3(&vel, &hitNorm);
					if (rdVector_Dot3(&vel, &hitNorm) < 0)
					{
						// bounce slightly on hit
						rdVector3 reflected;
						rdVector_Reflect3(&reflected, &vel, &hitNorm);
						rdVector_Scale3Acc(&reflected, pThing->animclass->bodypart[i].bounce);
						rdVector_Sub3(&pJoint->lastPos, &pJoint->pos, &reflected);
					}
					pJoint->flags |= JOINTFLAGS_COLLIDED;
				}
			}
		}

		rdVector_Copy3(&pJoint->thing.position, &pJoint->pos);
		rdVector_Zero3(&pJoint->nextPosAcc);
		pJoint->nextPosWeight = 0;
	}
}

void sithPuppet_Constrain(sithSector* pSector, sithThing* pThing, float deltaSeconds)
{
	// do fewer iterations if we're not directly visible
	int iterations = (pThing->isVisible + 1) == bShowInvisibleThings ? 5 : 1;
	for (int i = 0; i < iterations; ++i)
	{
		sithPuppet_ConstrainBody(pThing);
		sithPuppet_UpdateJointPositions(pSector, pThing, deltaSeconds);
	}
}

void sithPuppet_ApplyJointForce(sithThing* pThing, int joint, const rdVector3* forceVec)
{
	int nodeIdx = sithPuppet_GetJointNodeIndex(pThing, joint);
	if (nodeIdx < 0 || pThing->rdthing.amputatedJoints[nodeIdx])
		return;
	
	sithPuppetJoint* pJoint = &pThing->puppet->joints[joint];

	// apply the force
	float jointMass = pThing->physicsParams.mass * pThing->animclass->bodypart[joint].mass;
	rdVector_MultAcc3(&pJoint->forces, forceVec, 1.0f / jointMass);

	// reset the timer to activate the puppet
	pThing->puppet->expireMs = 0;
}

void sithPuppet_ApplyForce(sithThing* pThing, const rdVector3* forceVec)
{
	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
		sithPuppet_ApplyJointForce(pThing, i, forceVec);
}

static void sithPuppet_AccumulateJointForces(sithThing* pThing, float deltaSeconds)
{
	float gravity = sithWorld_pCurrentWorld->worldGravity;
	if ((pThing->physicsParams.physflags & SITH_PF_PARTIALGRAVITY) != 0)
		gravity *= 0.5;

	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
	{
		int nodeIdx = sithPuppet_GetJointNodeIndex(pThing, i);
		if (nodeIdx < 0 || pThing->rdthing.amputatedJoints[nodeIdx])
			continue;

		sithPuppetJoint* pJoint = &pThing->puppet->joints[i];

		if (pThing->physicsParams.mass != 0.0
			&& (pThing->sector->flags & SITH_SECTOR_HASTHRUST)
			&& !(pThing->physicsParams.physflags & SITH_PF_NOTHRUST))
		{
			rdVector_MultAcc3(&pJoint->forces, &pThing->sector->thrust, deltaSeconds);
		}

		// gravity
		if (pThing->physicsParams.mass != 0.0
			&& pThing->physicsParams.physflags & SITH_PF_USEGRAVITY
			&& !(pThing->sector->flags & SITH_SECTOR_NOGRAVITY))
		{
			pJoint->forces.z -= gravity * deltaSeconds;
		}
	}
}

static void sithPuppet_ResetJointForces(sithThing* pThing)
{
	// reset forces, leave sector, and update the joint matrix
	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
	{
		sithPuppetJoint* pJoint = &pThing->puppet->joints[i];
		rdVector_Zero3(&pJoint->forces);
		sithThing_LeaveSector(&pJoint->thing);
	}
}

void sithPuppet_UpdateJoints(sithThing* pThing, float deltaSeconds)
{
	// try to account for variable time steps
	// todo: fixed time step?
	float timestepRatio = pThing->puppet->lastTimeStep ? deltaSeconds / pThing->puppet->lastTimeStep : 1.0f;
	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
	{
		int nodeIdx = sithPuppet_GetJointNodeIndex(pThing, i);
		if (nodeIdx < 0 || pThing->rdthing.amputatedJoints[nodeIdx]) // don't update physics if amupated (let it simply be constrained)
			continue;

		sithPuppetJoint* pJoint = &pThing->puppet->joints[i];

		rdVector3 vel;
		rdVector_Sub3(&vel, &pJoint->pos, &pJoint->lastPos);
		
		// apply forces
		rdVector_MultAcc3(&vel, &pJoint->forces, deltaSeconds);

		// damping
		//rdVector_Scale3Acc(&vel, timestepRatio * powf((pJoint->flags & JOINTFLAGS_COLLIDED) ? 0.8f : 0.998f, deltaSeconds * 1000.0f));
		float drag = (pJoint->flags & JOINTFLAGS_COLLIDED) ? pJoint->thing.physicsParams.surfaceDrag : pJoint->thing.physicsParams.airDrag;
		sithPhysics_ApplyDrag(&vel, drag, 0.0f, deltaSeconds);

		rdVector_ClipPrecision3(&vel);
		if (rdVector_IsZero3(&vel))
			continue;

		// copy the old pos
		rdVector_Copy3(&pJoint->lastPos, &pJoint->pos);

		// update the joint thing position and sector
		rdVector_Copy3(&pJoint->thing.position, &pJoint->pos);
		rdVector_Copy3(&pJoint->thing.physicsParams.vel, &vel); // copy the vel in case we get modified (ex. entering water)
		sithThing_EnterSector(&pJoint->thing, pThing->sector, 1, 0);

		// add the vel
		//if (!(pJoint->flags & JOINTFLAGS_ROOT))
			rdVector_Add3Acc(&pJoint->pos, &pJoint->thing.physicsParams.vel);
	}
}

void sithPuppet_Collide(sithThing* pThing, float deltaSeconds)
{
	float totalImpactVolume = 0.0f;
	int anyCollision = 0; // did any joint collide?
	for (int i = 0; i < JOINTTYPE_NUM_JOINTS; ++i)
	{
		int nodeIdx = sithPuppet_GetJointNodeIndex(pThing, i);
		if (nodeIdx < 0 || pThing->rdthing.amputatedJoints[nodeIdx]) // don't bother colliding if amputated
			continue;
		
		sithPuppetJoint* pJoint = &pThing->puppet->joints[i];

		rdVector3 vel;
		rdVector_Sub3(&vel, &pJoint->pos, &pJoint->lastPos);
		rdVector_ClipPrecision3(&vel);
		if (rdVector_IsZero3(&vel))
			continue;

		rdVector3 hitNorm;
		int collided = sithPuppet_CollideJoint(pThing->sector, &pJoint->thing, &pJoint->pos, &vel, pJoint->radius, &hitNorm);
		if (collided)
		{
			pJoint->flags |= JOINTFLAGS_COLLIDED;

			anyCollision = 1;

			rdVector_Copy3(&pJoint->pos, &pJoint->lastPos);

			rdVector3 reflected;
			rdVector_Reflect3(&reflected, &vel, &hitNorm);
			rdVector_Scale3Acc(&reflected, pThing->animclass->bodypart[i].bounce);
			rdVector_Sub3(&pJoint->lastPos, &pJoint->pos, &reflected);

			float impactVolume = -rdVector_Dot3(&hitNorm, &vel) * 1000.0f;
			totalImpactVolume += impactVolume;
		}
		else
		{
			pJoint->flags &= ~JOINTFLAGS_COLLIDED;
		}
	}

	if (anyCollision)
	{
		totalImpactVolume /= (float)anyCollision;
		if (totalImpactVolume > 0.1f && (sithTime_curMs - pThing->puppet->lastCollideMs > 50))
		{
			if (totalImpactVolume > 1.0)
				totalImpactVolume = 1.0;
			sithSoundClass_PlayThingSoundclass(pThing, SITH_SC_CORPSEHIT, totalImpactVolume);
		}
		pThing->puppet->lastCollideMs = sithTime_curMs;
	}

	// if anything collide, set a timer for expiration
	if (anyCollision)
	{
		// only set a new timer if one wasn't already set
		pThing->puppet->expireMs = !pThing->puppet->expireMs ? sithTime_curMs + 1500 : pThing->puppet->expireMs;
	}
	// otherwise we're free-floating, let the sim run indefinitely until it settles
	else if (sithTime_curMs < pThing->puppet->expireMs)
	{
		pThing->puppet->expireMs = 0;
	}
}

//extern void sithPhysics_FindFloor(sithThing*, int); // fixme

void sithPuppet_UpdatePhysicsAnim(sithThing* thing, float deltaSeconds)
{
	// only run while expireMs is 0 or hasn't expired yet
	if (thing->puppet->expireMs && thing->puppet->expireMs < sithTime_curMs)
	{
		thing->puppet->lastCollideMs = sithTime_curMs;
		return;
	}

	sithPuppet_AccumulateJointForces(thing, deltaSeconds);
	sithPuppet_UpdateJoints(thing, deltaSeconds);
	sithPuppet_Collide(thing, deltaSeconds);

	thing->puppet->lastTimeStep = deltaSeconds;

	sithPuppet_Constrain(thing->sector, thing, deltaSeconds);
	sithPuppet_UpdateJointMatrices(thing, 0);
	sithPuppet_ApplyJointMatrices(thing);

	sithPuppet_ResetJointForces(thing);

	// the relative change in the center will be used to update the thing position
	rdVector3 lastCenter;
	rdVector_Copy3(&lastCenter, &thing->puppet->center);

	sithPuppet_CalculateRagdollCenterRadius(thing);
	thing->treeSize = thing->collideSize = thing->puppet->radius;

	rdVector3 centerVel;
	rdVector_Sub3(&centerVel, &thing->puppet->center, &lastCenter);
	rdVector_Copy3(&thing->physicsParams.vel, &centerVel);

	float velLen = rdVector_Normalize3Acc(&centerVel);
	sithCollision_UpdateThingCollision(thing, &centerVel, velLen, 0);

	//sithPhysics_FindFloor(thing, 0);
}

#endif

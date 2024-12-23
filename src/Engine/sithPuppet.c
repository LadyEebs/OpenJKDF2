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

#ifdef PUPPET_PHYSICS

static const sithPuppetConstraint sithPuppet_constraints[] =
{
	// head constraints
	{ JOINTTYPE_HEAD,      JOINTTYPE_NECK,   -1 },
	{ JOINTTYPE_HEAD,     JOINTTYPE_TORSO,   -1 },
	{ JOINTTYPE_HEAD, JOINTTYPE_RSHOULDER,   -1 },
	{ JOINTTYPE_HEAD, JOINTTYPE_LSHOULDER,   -1 },
	{ JOINTTYPE_HEAD,       JOINTTYPE_HIP, 0.5f },

	// neck constraints
	{ JOINTTYPE_NECK,     JOINTTYPE_TORSO, -1 },
	{ JOINTTYPE_NECK, JOINTTYPE_RSHOULDER, -1 },
	{ JOINTTYPE_NECK, JOINTTYPE_LSHOULDER, -1 },

	// torso constraints
	{ JOINTTYPE_TORSO,       JOINTTYPE_HIP,   -1 },
	{ JOINTTYPE_TORSO, JOINTTYPE_RSHOULDER,   -1 },
	{ JOINTTYPE_TORSO, JOINTTYPE_LSHOULDER,   -1 },
	{ JOINTTYPE_TORSO,    JOINTTYPE_RTHIGH, 0.8f },
	{ JOINTTYPE_TORSO,    JOINTTYPE_LTHIGH, 0.8f },
	{ JOINTTYPE_TORSO,     JOINTTYPE_LFOOT, 0.9f }, // prevent torso getting too close to foot
	{ JOINTTYPE_TORSO,     JOINTTYPE_RFOOT, 0.9f }, // prevent torso getting too close to foot
	{ JOINTTYPE_TORSO,     JOINTTYPE_LCALF, 0.9f }, // prevent calf getting too close to torso
	{ JOINTTYPE_TORSO,     JOINTTYPE_RCALF, 0.9f }, // prevent calf getting too close to torso

	// hip constraints
	{ JOINTTYPE_HIP, JOINTTYPE_RSHOULDER,  0.9f },
	{ JOINTTYPE_HIP, JOINTTYPE_LSHOULDER,  0.9f },
	{ JOINTTYPE_HIP,    JOINTTYPE_RTHIGH,   -1 },
	{ JOINTTYPE_HIP,    JOINTTYPE_LTHIGH,   -1 },
	{ JOINTTYPE_HIP,     JOINTTYPE_LCALF, 0.9f }, // prevent calf getting too close to hip
	{ JOINTTYPE_HIP,     JOINTTYPE_RCALF, 0.9f }, // prevent calf getting too close to hip

	// arm constraints
	{ JOINTTYPE_RSHOULDER, JOINTTYPE_LSHOULDER,    -1 },
	{ JOINTTYPE_RFOREARM,   JOINTTYPE_LFOREARM, 0.75f }, // prevent forearms from getting too close together
	{ JOINTTYPE_RHAND,         JOINTTYPE_LHAND,  0.8f }, // prevent hands getting too close together
	{ JOINTTYPE_LFOREARM,      JOINTTYPE_RHAND, 0.75f }, // prevent calf getting too close to other foot
	{ JOINTTYPE_RFOREARM,      JOINTTYPE_LHAND, 0.75f }, // prevent calf getting too close to other foot

	// right arm
	{ JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER,    -1 },
	{ JOINTTYPE_RHAND,     JOINTTYPE_RFOREARM,    -1 },
	{ JOINTTYPE_RSHOULDER,   JOINTTYPE_RTHIGH, 0.85f },
	{ JOINTTYPE_RSHOULDER,   JOINTTYPE_LTHIGH, 0.85f },
	{ JOINTTYPE_RFOREARM,       JOINTTYPE_HIP, 0.25f }, // prevent forearm getting too close to other hand
	{ JOINTTYPE_RHAND,    JOINTTYPE_RSHOULDER, 0.7f }, // prevent hand from getting too close to shoulder
	{ JOINTTYPE_RHAND,         JOINTTYPE_HEAD, 0.7f }, // prevent hand from getting too close to head

	// left arm
	{ JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER,    -1 },
	{ JOINTTYPE_LHAND,     JOINTTYPE_LFOREARM,    -1 },
	{ JOINTTYPE_LSHOULDER,   JOINTTYPE_RTHIGH, 0.85f },
	{ JOINTTYPE_LSHOULDER,   JOINTTYPE_LTHIGH, 0.85f },
	{ JOINTTYPE_LFOREARM,       JOINTTYPE_HIP, 0.25f }, // prevent forearm getting too close to other hand
	{ JOINTTYPE_LHAND,    JOINTTYPE_LSHOULDER, 0.7f }, // prevent hand from getting too close to shoulder
	{ JOINTTYPE_LHAND,         JOINTTYPE_HEAD, 0.7f }, // prevent hand from getting too close to head

	// leg constraints
	{ JOINTTYPE_RTHIGH,  JOINTTYPE_LTHIGH,    -1 },
	{ JOINTTYPE_RCALF,    JOINTTYPE_LCALF,  0.8f }, // prevent calfs getting too close together
	{ JOINTTYPE_RFOOT,    JOINTTYPE_LFOOT,  0.5f }, // prevent feet getting too close together
	{ JOINTTYPE_LCALF,    JOINTTYPE_RFOOT, 0.75f }, // prevent calf getting too close to other foot
	{ JOINTTYPE_RCALF,    JOINTTYPE_LFOOT, 0.75f }, // prevent calf getting too close to other foot
	{ JOINTTYPE_RCALF,   JOINTTYPE_LTHIGH,  0.7f }, // prevent calf getting too close to other thigh
	{ JOINTTYPE_LCALF,   JOINTTYPE_RTHIGH,  0.7f }, // prevent calf getting too close to other thigh
	{ JOINTTYPE_LCALF, JOINTTYPE_RFOREARM, 0.75f }, // prevent calf getting too close to other arm
	{ JOINTTYPE_RCALF, JOINTTYPE_LFOREARM, 0.75f }, // prevent calf getting too close to other arm

	// right leg
	{ JOINTTYPE_RCALF, JOINTTYPE_RTHIGH,    -1 },
	{ JOINTTYPE_RFOOT,  JOINTTYPE_RCALF,    -1 },
	{ JOINTTYPE_RFOOT, JOINTTYPE_RTHIGH,  0.5f }, // prevent foot getting too close to thigh

	// left arm
	{ JOINTTYPE_LCALF, JOINTTYPE_LTHIGH,    -1 },
	{ JOINTTYPE_LFOOT,  JOINTTYPE_LCALF,    -1 },
	{ JOINTTYPE_LFOOT, JOINTTYPE_LTHIGH,  0.5f }, // prevent foot getting too close to thigh

	// weapon
	{ JOINTTYPE_PRIMARYWEAP,   JOINTTYPE_RHAND, -1 },
	{ JOINTTYPE_SECONDARYWEAP, JOINTTYPE_LHAND, -1 },
};

static const sithPuppetJointFrame sithPuppet_jointFrames[] =
{
	{ JOINTTYPE_NECK,      JOINTTYPE_LSHOULDER, JOINTTYPE_RSHOULDER,              -1, 1, 70.0f, 20.0f },	// JOINTTYPE_HEAD
	{ JOINTTYPE_HEAD,      JOINTTYPE_LSHOULDER, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 0, 60.0f,  0.0f },	// JOINTTYPE_NECK
	{ JOINTTYPE_NECK,      JOINTTYPE_LSHOULDER, JOINTTYPE_RSHOULDER,   JOINTTYPE_HIP, 0, 20.0f, 10.0f },	// JOINTTYPE_TORSO
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_PRIMARYWEAP
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_SECONDARYWEAP
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_PRIMARYWEAPJOINT
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_SECONDARYWEAPJOINT
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_TURRETPITCH
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_TURRETYAW
	{ JOINTTYPE_TORSO,        JOINTTYPE_LTHIGH,    JOINTTYPE_RTHIGH,              -1, 0, 20.0f,  0.0f },	// JOINTTYPE_HIP
	{ JOINTTYPE_RFOREARM,                   -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_RSHOULDER
	{ JOINTTYPE_LFOREARM,                   -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_LSHOULDER
	{ JOINTTYPE_RHAND,                  -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_RFOREARM
	{ JOINTTYPE_LHAND,                  -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_LFOREARM
	{ JOINTTYPE_RFOREARM,                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_RHAND
	{ JOINTTYPE_LFOREARM,                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_LHAND
	{ JOINTTYPE_RCALF,                      -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_RTHIGH
	{ JOINTTYPE_LCALF,                      -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_LTHIGH
	{ JOINTTYPE_RTHIGH,                     -1,                  -1, JOINTTYPE_RFOOT, 0,  0.0f,  0.0f },	// JOINTTYPE_RCALF
	{ JOINTTYPE_LTHIGH,                     -1,                  -1, JOINTTYPE_LFOOT, 0,  0.0f,  0.0f },	// JOINTTYPE_LCALF
	{ JOINTTYPE_RCALF,                      -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_RFOOT
	{ JOINTTYPE_LCALF,                      -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_LFOOT
};


static const int sithPuppet_jointParent[] =
{
	JOINTTYPE_NECK,    	// JOINTTYPE_HEAD
	JOINTTYPE_TORSO,    // JOINTTYPE_NECK
	JOINTTYPE_HIP,    	// JOINTTYPE_TORSO
	-1,                	// JOINTTYPE_PRIMARYWEAP
	-1,                	// JOINTTYPE_SECONDARYWEAP
	-1,                	// JOINTTYPE_PRIMARYWEAPJOINT
	-1,                	// JOINTTYPE_SECONDARYWEAPJOINT
	-1,                	// JOINTTYPE_TURRETPITCH
	-1,                	// JOINTTYPE_TURRETYAW
	-1,   	            // JOINTTYPE_HIP
	JOINTTYPE_TORSO,	// JOINTTYPE_RSHOULDER
	JOINTTYPE_TORSO,	// JOINTTYPE_LSHOULDER
	JOINTTYPE_RSHOULDER,// JOINTTYPE_RFOREARM
	JOINTTYPE_LSHOULDER,// JOINTTYPE_LFOREARM
	JOINTTYPE_RFOREARM,	// JOINTTYPE_RHAND
	JOINTTYPE_LFOREARM,	// JOINTTYPE_LHAND
	JOINTTYPE_HIP,   	// JOINTTYPE_RTHIGH
	JOINTTYPE_HIP,   	// JOINTTYPE_LTHIGH
	JOINTTYPE_RTHIGH,  	// JOINTTYPE_RCALF
	JOINTTYPE_LTHIGH,  	// JOINTTYPE_LCALF
	JOINTTYPE_RCALF,   	// JOINTTYPE_RFOOT
	JOINTTYPE_LCALF,   	// JOINTTYPE_LFOOT
};

static const int sithPuppet_jointChild[] =
{
	-1,    	            // JOINTTYPE_HEAD
	JOINTTYPE_HEAD,     // JOINTTYPE_NECK
	JOINTTYPE_HIP,//JOINTTYPE_NECK,    	// JOINTTYPE_TORSO
	-1,                	// JOINTTYPE_PRIMARYWEAP
	-1,                	// JOINTTYPE_SECONDARYWEAP
	-1,                	// JOINTTYPE_PRIMARYWEAPJOINT
	-1,                	// JOINTTYPE_SECONDARYWEAPJOINT
	-1,                	// JOINTTYPE_TURRETPITCH
	-1,                	// JOINTTYPE_TURRETYAW
	JOINTTYPE_TORSO,   	// JOINTTYPE_HIP
	JOINTTYPE_RFOREARM,	// JOINTTYPE_RSHOULDER
	JOINTTYPE_LFOREARM,	// JOINTTYPE_LSHOULDER
	JOINTTYPE_RHAND,    // JOINTTYPE_RFOREARM
	JOINTTYPE_LHAND,    // JOINTTYPE_LFOREARM
	-1,	                // JOINTTYPE_RHAND
	-1,	                // JOINTTYPE_LHAND
	JOINTTYPE_RCALF,   	// JOINTTYPE_RTHIGH
	JOINTTYPE_LCALF,   	// JOINTTYPE_LTHIGH
	JOINTTYPE_RFOOT,  	// JOINTTYPE_RCALF
	JOINTTYPE_LFOOT,  	// JOINTTYPE_LCALF
	-1,   	// JOINTTYPE_RFOOT
	-1,   	// JOINTTYPE_LFOOT
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

#ifdef RIGID_BODY
#include "Modules/sith/Engine/sithConstraint.h"

void sithPuppet_AddDistanceConstraint(sithThing* pThing, int joint, int target, int axis, int flip, float angle)
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
	if (penetrationDepth > 0.0f)
	{
		contactPointA.x -= direction.x * (penetrationDepth / 2.0f);
		contactPointA.y -= direction.y * (penetrationDepth / 2.0f);
		contactPointA.z -= direction.z * (penetrationDepth / 2.0f);
	
		contactPointB.x += direction.x * (penetrationDepth / 2.0f);
		contactPointB.y += direction.y * (penetrationDepth / 2.0f);
		contactPointB.z += direction.z * (penetrationDepth / 2.0f);
	}

	rdVector3 anchor;
	rdVector_Add3(&anchor, &contactPointA, &contactPointB);
	rdVector_Scale3Acc(&anchor, 0.5f);

	//rdVector3 anchor = pThing->rdthing.hierarchyNodeMatrices[pThing->animclass->bodypart[joint].nodeIdx].scale;
	float distance = 0.0f;// rdVector_Dist3(&contactPointB, &contactPointA) * (targetThing->moveSize + jointThing->moveSize);
	//distance = stdMath_Fabs(targetThing->moveSize - jointThing->moveSize);

	rdMatrix_TransformPoint34(&contactPointA, &anchor, &invTargetMat);
	rdMatrix_TransformPoint34(&contactPointB, &anchor, &invJointMat);

	/*rdVector3 directionA, directionB;
	rdVector_Sub3(&directionA, &jointPos, &targetThing->position);
	rdVector_Sub3(&directionB, &jointPos, &jointThing->position);
	rdVector_Normalize3Acc(&directionA);
	rdVector_Normalize3Acc(&directionB);

	rdVector3 contactPointA, contactPointB;
	rdVector_Scale3(&contactPointA, &directionA, targetThing->moveSize);
	rdVector_Add3Acc(&contactPointA, &targetThing->position);
	
	rdVector_Scale3(&contactPointB, &directionB, jointThing->moveSize);
	rdVector_Add3Acc(&contactPointB, &jointThing->position);

	rdVector3 anchor;
	rdVector_Add3(&anchor, &contactPointA, &contactPointB);
	rdVector_Scale3Acc(&anchor, 0.5f);

	//rdVector3 anchor = pThing->rdthing.hierarchyNodeMatrices[pThing->animclass->bodypart[joint].nodeIdx].scale;
	float distance = 0.0f;// rdVector_Dist3(&contactPointB, &contactPointA) * (targetThing->moveSize + jointThing->moveSize);
	//distance = stdMath_Fabs(targetThing->moveSize - jointThing->moveSize);

	//rdMatrix_TransformPoint34(&contactPointA, &anchor, &invTargetMat);
	//rdMatrix_TransformPoint34(&contactPointB, &anchor, &invJointMat);
	rdMatrix_TransformPoint34Acc(&contactPointA, &invTargetMat);
	rdMatrix_TransformPoint34Acc(&contactPointB, &invJointMat);*/

//	sithConstraint_AddDistanceConstraint(pThing, jointThing, targetThing, &contactPointA, &contactPointB, distance);

//	rdVector3 coneAxis = ((rdVector3*)&relative.rvec)[axis];
	rdVector3 coneAxis;
	if (axis == 0)
		coneAxis = rdroid_xVector3;
	else if (axis == 1)
		coneAxis = rdroid_yVector3;
	else
		coneAxis = rdroid_zVector3;

	if(flip)
		rdVector_Neg3Acc(&coneAxis);

//	coneAxis.x = 0;
//	coneAxis.y = 0;
//	coneAxis.z = flip ? -1 : 1;

	//rdVector_Sub3(&coneAxis, &pThing->rdthing.hierarchyNodeMatrices[pThing->animclass->bodypart[joint].nodeIdx].scale, &targetThing->position);
	//rdVector_Normalize3Acc(&coneAxis);

	//if (jointThing->physicsParams.physflags & SITH_PF_4000000)
		//sithConstraint_AddConeConstraint(pThing, jointThing, targetThing, &rdroid_zeroVector3, &coneAxis, angle, &coneAxis);

//	if (jointThing->physicsParams.physflags & SITH_PF_4000000)
		sithConstraint_AddDistanceConstraint(pThing, jointThing, targetThing, &contactPointA, &contactPointB, distance);

//	sithThing* targetThing = &pThing->puppet->physics->joints[target].thing;
//	sithThing* jointThing = &pThing->puppet->physics->joints[joint].thing;
//
//	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
//	if(!constraint)
//		return;
//
//	constraint->type = SITH_CONSTRAINT_DISTANCE;
//	constraint->thingA = jointThing;
//	
//	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pThing->animclass->bodypart[joint].nodeIdx];
//
//	rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[target].nodeIdx].scale;
//	rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[joint].nodeIdx].scale;
//
//	// invert the matrix so we can get a local position for the constraint
//	rdMatrix34 inv;
//	rdMatrix_InvertOrtho34(&inv, &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[target].nodeIdx]);
//
//	// derive the anchor from the base pose and the inverse target matrix
//	rdMatrix_TransformPoint34(&constraint->distanceParams.constraintAnchor, pBasePosB, &inv);
////	rdVector_Copy3(&constraint->distanceParams.constraintAnchor, &pNode->pos);
//	constraint->distanceParams.constraintDistance = 0;//rdVector_Len3(&constraint->distanceParams.constraintAnchor);
//	
//	// link it
//	constraint->next = targetThing->constraints;
//	targetThing->constraints = constraint;
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


	sithConstraint_AddConeConstraint(pThing, jointThing, targetThing, &rdroid_zeroVector3, &coneAxis, angle, &coneAxis);
}


void sithPuppet_AddTwistConstraint(sithThing* pThing, int joint, int target, int axis, int flip, float angle)
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

	sithConstraint_AddTwistConstraint(pThing, jointThing, targetThing, &coneAxis, angle);
}

void sithPuppet_AddAngleConstraint(sithThing* pThing, int joint, int target, float minPitch, float maxPitch, float minYaw, float maxYaw, float minRoll, float maxRoll)
{
	int hasJoint = pThing->animclass->jointBits & (1ull << joint);
	int hasTarget = pThing->animclass->jointBits & (1ull << target);
	if (!hasJoint || !hasTarget)
		return;

	sithThing* targetThing = &pThing->puppet->physics->joints[target].thing;
	sithThing* jointThing = &pThing->puppet->physics->joints[joint].thing;

	rdVector3 minAngles = {minPitch, minYaw, minRoll};
	rdVector3 maxAngles = {maxPitch, maxYaw, maxRoll};
	sithConstraint_AddAngleConstraint(pThing, jointThing, targetThing, &minAngles, &maxAngles);

//	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
//	if (!constraint)
//		return;
//
//	constraint->type = SITH_CONSTRAINT_CONE;
//	constraint->thingA = jointThing;
//
//	constraint->coneParams.maxSwingAngle = maxSwingAngle;
//	constraint->coneParams.maxTwistAngle = maxTwistAngle;
//
//	// link it
//	constraint->next = targetThing->constraints;
//	targetThing->constraints = constraint;
}


void sithPuppet_AddLookConstraint(sithThing* pThing, int joint, int target, int flipUp)
{
	int hasJoint = pThing->animclass->jointBits & (1ull << joint);
	int hasTarget = pThing->animclass->jointBits & (1ull << target);
	if (!hasJoint || !hasTarget)
		return;

	sithThing* targetThing = &pThing->puppet->physics->joints[target].thing;
	sithThing* jointThing = &pThing->puppet->physics->joints[joint].thing;

	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pThing->animclass->bodypart[joint].nodeIdx];

	rdMatrix34 refMat;
	rdMatrix_Copy34(&refMat, &pNode->posRotMatrix);

	sithConstraint_AddLookConstraint(pThing, jointThing, targetThing, &refMat, flipUp);

//	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
//	if (!constraint)
//		return;
//
//	constraint->type = SITH_CONSTRAINT_LOOK;
//	constraint->thingA = jointThing;
//
//	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pThing->animclass->bodypart[joint].nodeIdx];
//	rdMatrix_Copy34(&constraint->lookParams.referenceMat, &pNode->posRotMatrix);
//	constraint->lookParams.flipUp = flipUp;
//
//	// link it
//	constraint->next = targetThing->constraints;
//	targetThing->constraints = constraint;
}
#endif

void sithPuppet_SetupJointThing(sithThing* pThing, sithThing* pJointThing, sithBodyPart* pBodyPart, rdHierarchyNode* pNode, const rdVector3* pOffset, int jointIdx)
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

	rdVector3 pos;
	rdVector_Copy3(&pos, &pJointThing->position);
	if (pOffset)
		rdVector_Add3Acc(&pos, pOffset);
	
	pJointThing->moveSize = pJointThing->collideSize = 0.01f;//pThing->moveSize * 0.1;
//#ifdef RIGID_BODY
	if(pNode->meshIdx >= 0)
	{
		int lastGeoSet = pThing->rdthing.model3->numGeosets - 1;
		rdMesh* pMesh = &pThing->rdthing.model3->geosets[lastGeoSet].meshes[pNode->meshIdx];

		rdVector3 meshCenter = rdroid_zeroVector3;
		float minDist = 10000.0f;
		float maxDist = 0.0f;
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
		rdVector_Add3Acc(&pos, &meshCenter);

		float avgDist = (minDist + maxDist) * 0.5f;
		pJointThing->moveSize = pJointThing->collideSize = avgDist;
	}
//#endif

	//sithCollision_GetSectorLookAt(pThing->sector, &pJointThing->position, &pos, 0.0f);
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
		//if(pOffset)
			//pJointThing->physicsParams.mass = 0.00001;
		pJointThing->physicsParams.buoyancy *= pBodyPart->buoyancy;
		pJointThing->physicsParams.height = 0.00001f;

		pJointThing->physicsParams.physflags = SITH_PF_FEELBLASTFORCE;

		// angthrust isn't actually used (we never set the thrust)
		// but this enables angular velocity drag and clamping
	//	pJointThing->physicsParams.physflags |= SITH_PF_ANGTHRUST;
#ifdef RIGID_BODY
	//	pJointThing->physicsParams.physflags |= SITH_PF_ANGIMPULSE;
#endif

		//if (jointIdx == JOINTTYPE_TORSO)
		{
			//pJointThing->physicsParams.physflags |= SITH_PF_SURFACEALIGN;
		//	pJointThing->actorParams.eyePYR.x = -90.0f;
			//pJointThing->physicsParams.maxRotVel = 15.0f;
			//pJointThing->physicsParams.physflags |= SITH_PF_FLOORSTICK;
		}
		//else
			//pJointThing->hysicsParams.maxRotVel = 10000.0;

		//if (jointIdx == JOINTTYPE_HIP)
			//pJointThing->thingflags |= SITH_TF_STANDABLE;

	//	pJointThing->physicsParams.physflags |= SITH_PF_SURFACEALIGN;
		pJointThing->physicsParams.physflags |= SITH_PF_FLOORSTICK;
	//	pJointThing->physicsParams.physflags |= SITH_PF_DONTROTATEVEL;

	//	pJointThing->physicsParams.physflags |= SITH_PF_USESTHRUST;
	//	pJointThing->physicsParams.physflags |= SITH_PF_FLY;

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

		if (jointIdx == JOINTTYPE_LHAND
			|| jointIdx == JOINTTYPE_RHAND
		//	|| jointIdx == JOINTTYPE_RFOREARM
		//	|| jointIdx == JOINTTYPE_LFOREARM
		//	|| jointIdx == JOINTTYPE_RSHOULDER
		//	|| jointIdx == JOINTTYPE_LSHOULDER
		//|| jointIdx == JOINTTYPE_HEAD
			//|| jointIdx == JOINTTYPE_NECK
			//jointIdx == JOINTTYPE_TORSO
		)
			pJointThing->physicsParams.physflags |= SITH_PF_4000000;// | SITH_PF_USEGRAVITY;

		//if (jointIdx == JOINTTYPE_HEAD)
		//	pJointThing->physicsParams.physflags |= SITH_PF_2000000;

		pJointThing->physicsParams.staticDrag = fmax(pJointThing->physicsParams.staticDrag, 0.01f);
		//pJointThing->physicsParams.airDrag *= 5.0f;
		//pJointThing->physicsParams.surfaceDrag *= 5.0f;

		rdVector3 vel;
		rdMatrix_TransformVector34(&vel, &pThing->rdthing.paHierarchyNodeVelocities[pNode->idx], &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx]);
		rdVector_Copy3(&pJointThing->physicsParams.vel, &vel);
		rdVector_Copy3(&pJointThing->physicsParams.angVel, &pThing->rdthing.paHierarchyNodeAngularVelocities[pNode->idx]);

		//printf("init velocity for joint %d is %f!\n", jointIdx, rdVector_Len3(&pJoint->thing.physicsParams.vel));

	#ifdef RIGID_BODY
		// disabled forn ow
		pJointThing->physicsParams.vel = rdroid_zeroVector3;
		pJointThing->physicsParams.angVel = rdroid_zeroVector3;
	#endif
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

	#ifdef RIGID_BODY
		rdVector3 forwardOffset = { 0, 0.001, 0 };

		sithPuppet_SetupJointThing(pThing, &pJoint->thing, pBodyPart, pNode, &forwardOffset, jointIdx);
	#else
		if (pBodyPart->flags & JOINTFLAGS_HINGE)
		{
			pJoint->numThings = 2;
		
			rdVector3 offsetRight = { 0.005f, 0.0f, 0.0f };
			rdMatrix_TransformVector34Acc(&offsetRight, &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx]);
			sithPuppet_SetupJointThing(pThing, &pJoint->things[0], pBodyPart, pNode, &offsetRight, jointIdx);
			
			rdVector3 offsetLeft = { -0.005f, 0.0f, 0.0f };
			rdMatrix_TransformVector34Acc(&offsetLeft, &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx]);
			sithPuppet_SetupJointThing(pThing, &pJoint->things[1], pBodyPart, pNode, &offsetLeft, jointIdx);
		}
		else
		{
			pJoint->numThings = 1;
			sithPuppet_SetupJointThing(pThing, &pJoint->things[0], pBodyPart, pNode, NULL, jointIdx);
		}
	#endif
	}

#ifndef RIGID_BODY
	// setup constraint distances from the current animation pose
	// we use this instead of the models base pose to ensure constraints are close from the start
	pThing->puppet->physics->constraintDistances = (float*)pSithHS->alloc(sizeof(float) * ARRAYSIZE(sithPuppet_constraints));
	memset(pThing->puppet->physics->constraintDistances, 0, sizeof(float)* ARRAYSIZE(sithPuppet_constraints));
	for(int i = 0; i < ARRAYSIZE(sithPuppet_constraints); ++i)
	{
		sithPuppetConstraint* pConstraint = &sithPuppet_constraints[i];

		int hasJointA = pThing->animclass->jointBits & (1ull << pConstraint->jointA);
		int hasJointB = pThing->animclass->jointBits & (1ull << pConstraint->jointB);
		if (!hasJointA || !hasJointB)
			continue;

		sithBodyPart* pBodyPartA = &pThing->animclass->bodypart[pConstraint->jointA];
		sithBodyPart* pBodyPartB = &pThing->animclass->bodypart[pConstraint->jointB];

		rdVector3 basePosA = pThing->rdthing.hierarchyNodeMatrices[pBodyPartA->nodeIdx].scale;
		rdVector3 basePosB = pThing->rdthing.hierarchyNodeMatrices[pBodyPartB->nodeIdx].scale;
		pThing->puppet->physics->constraintDistances[i] = rdVector_Dist3(&basePosB, &basePosA);
	}
#endif
#ifdef RIGID_BODY

//	sithPuppet_AddLookConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 1);
//	sithPuppet_AddLookConstraint(pThing, JOINTTYPE_HIP, JOINTTYPE_TORSO, 0);
//	
//	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 120.0f, 0.0f, 5.0f, -5.0f, 5.0f, -5.0f);
//	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 120.0f, 0.0f, 5.0f, -5.0f, 5.0f, -5.0f);
//
//	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 90.0f, -35.0f, 10.0f, -10.0f, 90.0f, 0.0f);
//	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 90.0f, -35.0f, 10.0f, -10.0f, 0.0f, -90.0f);
//
//	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 0.0f, -100.0f, 5.0f, -5.0f, 10.0f, -10.0f);
//	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 0.0f, -100.0f, 5.0f, -5.0f, 10.0f, -10.0f);
//
//	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, 60.0f, -5.0f, 5.0f, -5.0f, 5.0f, -5.0f);
//	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, 60.0f, -5.0f, 5.0f, -5.0f, 5.0f, -5.0f);
//
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, 0);
//
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 0);
//
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 0);
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 0);

//	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
//	{
//		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 2, 0, 10.0f);
//		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 2, 0, 10.0f);
//	}
//	else
//	{
//		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 2, 0, 10.0f);
//	}
//
//	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 2, 0, 5.0f);
//
//	//sithPuppet_AddConeConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 2, 0, 5.0f);
//
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 15.0f, -10.0f, 5.0f, -5.0f, 10.0f, -10.0f);
//	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
//	{
//		sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 5.0f, -5.0f, 5.0f, -5.0f, 5.0f, -5.0f);
//		sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 5.0f, -40.0f, 5.0f, -5.0f, 35.0f, -35.0f);
//	}
//	else
//	{
//		sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 5.0f, -40.0f, 5.0f, -5.0f, 35.0f, -35.0f);
//	}
//
//	//sithPuppet_AddTwistConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 2, 1, 10.0f);
//	//sithPuppet_AddTwistConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 2, 0, 5.0f);
//
//	//sithPuppet_AddLookConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 1);
//	//if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
//	//	sithPuppet_AddLookConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 1);

	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, 2, 1, 35.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, 2, 1, 35.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, 2, 1, 35.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, 2, 1, 35.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 2, 1, 20.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 2, 1, 20.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 2, 1, 30.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 2, 1, 30.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, 2, 1, 70.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, 2, 1, 70.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 0, 0, 150.0f);
	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 0, 1, 150.0f);


	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
	{
		sithPuppet_AddConeConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 2, 0, 15.0f);
		sithPuppet_AddConeConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 2, 0, 15.0f);
	}
	else
	{
		sithPuppet_AddConeConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 2, 0, 15.0f);
	}

	sithPuppet_AddConeConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 2, 0, 25.0f);

	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 15.0f, -10.0f, 5.0f, -5.0f, 10.0f, -10.0f);
	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, 60.0f, -15.0f, 10.0f, -10.0f, 10.0f, -10.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, 2, 0, 30.0f);
	
	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, 60.0f, -15.0f, 10.0f, -10.0f, 10.0f, -10.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, 2, 0, 30.0f);

	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, 5.0f, -5.0f, 25.0f, -25.0f, 35.0f, -5.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, 2, 1, 45.0f);

	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, 5.0f, -5.0f, 25.0f, -25.0f, 5.0f, -35.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, 2, 0, 45.0f);

	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 0.0f, -100.0f, 10.0f, -10.0f, 15.0f, -15.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 2, 0, 40.0f);
	
	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 0.0f, -100.0f, 10.0f, -10.0f, 15.0f, -15.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 2, 0, 40.0f);
	
	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 120.0f, 0.0f, 5.0f, -5.0f, 5.0f, -5.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 2, 0, 50.0f);

	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 120.0f, 0.0f, 5.0f, -5.0f, 5.0f, -5.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 2, 0, 50.0f);

	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, 60.0f, -15.0f, 20.0f, -20.0f, 40.0f, -40.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, 2, 0, 30.0f);
	
	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, 60.0f, -15.0f, 20.0f, -20.0f, 40.0f, -40.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, 2, 0, 30.0f);
	
	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 90.0f, -35.0f, 10.0f, -10.0f, 0.0f, -160.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 1, 0, 150.0f);

	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 90.0f, -35.0f, 10.0f, -10.0f, 160.0f, 0.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 1, 1, 150.0f);


	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
	{
		//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 10.0f, -40.0f, 55.0f, -55.0f, 15.0f, -15.0f);
		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 2, 0, 10.0f);
		
		//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f, -10.0f);
		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 2, 0, 10.0f);
	}
	else
	{
		//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 10.0f, -40.0f, 55.0f, -55.0f, 15.0f, -15.0f);
		sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 2, 0, 10.0f);
	}	

	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_HIP, JOINTTYPE_TORSO, 25.0f, -10.0f, 25.0f, -25.0f, 20.0f, -20.0f);
	sithPuppet_AddDistanceConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 2, 0, 45.0f);

	//sithPuppet_AddLookConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 1);



//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_HIP, JOINTTYPE_TORSO, 25.0f, -10.0f, 25.0f, -25.0f, 20.0f, -20.0f);
//	//sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 15.0f, -10.0f, 5.0f, -5.0f, 10.0f, -10.0f);
//	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
//	{
//		sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f, -10.0f);
//		sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 10.0f, -40.0f, 55.0f, -55.0f, 15.0f, -15.0f);
//	}
//	else
//	{
//		sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 10.0f, -40.0f, 55.0f, -55.0f, 15.0f, -15.0f);
//	}
//	
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, 60.0f, -15.0f, 10.0f, -10.0f, 10.0f, -10.0f);
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, 60.0f, -15.0f, 10.0f, -10.0f, 10.0f, -10.0f);
//	
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 0.0f, -100.0f, 10.0f, -10.0f, 15.0f, -15.0f);
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 0.0f, -100.0f, 10.0f, -10.0f, 15.0f, -15.0f);
//	
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 90.0f, -35.0f, 10.0f, -10.0f, 0.0f, -160.0f);
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 90.0f, -35.0f, 10.0f, -10.0f, 160.0f, 0.0f);
//	
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 120.0f, 0.0f, 5.0f, -5.0f, 5.0f, -5.0f);
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 120.0f, 0.0f, 5.0f, -5.0f, 5.0f, -5.0f);
//	
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, 5.0f, -5.0f, 25.0f, -25.0f, 35.0f, -5.0f);
//	sithPuppet_AddAngleConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, 5.0f, -5.0f, 25.0f, -25.0f, 5.0f, -35.0f);
	
	
//	sithPuppet_AddLookConstraint(pThing, JOINTTYPE_HIP, JOINTTYPE_TORSO, 0);
//	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
//		sithPuppet_AddLookConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 1);
//	else
//		sithPuppet_AddLookConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_HEAD, 0);


#endif







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
#ifdef RIGID_BODY
			sithThing_LeaveSector(&pJoint->thing);
			sithThing_FreeEverything(&pJoint->thing);
			rdThing_FreeEntry(&pJoint->thing.rdthing);
#else
			for(int i = 0; i < pJoint->numThings; ++i)
			{
				sithThing_LeaveSector(&pJoint->things[i]);
				sithThing_FreeEverything(&pJoint->things[i]);
				rdThing_FreeEntry(&pJoint->things[i].rdthing);
			}
#endif
		}

		if(pThing->puppet->physics->constraintDistances)
			pSithHS->free(pThing->puppet->physics->constraintDistances);

		pSithHS->free(pThing->puppet->physics);
		pThing->puppet->physics = 0;
	}
}

#ifdef RIGID_BODY
void sithPuppet_ApplyDistanceConstraints(sithThing* pThing, int jointIdx, int targetJointIdx, float deltaSeconds)
{
	int hasTargetJoint = targetJointIdx < 0 ? 0 : pThing->animclass->jointBits & (1ull << targetJointIdx);
	if (!hasTargetJoint)
		return;

	sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];
	sithThing* pJointThing = &pJoint->thing;

	sithBodyPart* pTargetBodyPart = &pThing->animclass->bodypart[targetJointIdx];
	sithPuppetJoint* pTargetJoint = &pThing->puppet->physics->joints[targetJointIdx];
	rdHierarchyNode* pTargetNode = &pThing->rdthing.model3->hierarchyNodes[pTargetBodyPart->nodeIdx];
	sithThing* pTargetThing = &pTargetJoint->thing;

	// todo: precompute the constraint distance
	rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pTargetBodyPart->nodeIdx].scale;
	rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPart->nodeIdx].scale;
	//float distance = rdVector_Dist3(pBasePosB, pBasePosA);


	// invert the target matrix so we can get a local position for the joint
	rdMatrix34 inv;
	rdMatrix_InvertOrtho34(&inv, &pThing->rdthing.model3->paBasePoseMatrices[pTargetBodyPart->nodeIdx]);
	
	// derive the anchor from the base pose and the inverse target matrix
	rdVector3 anchor;
	rdMatrix_TransformPoint34(&anchor, pBasePosB, &inv);
	//rdVector_Copy3(&anchor, &pNode->pos);
	float distance = 0.0f;//rdVector_Len3(&anchor) / 1.8;

	// move to the target things actual world matrix
	//rdVector_Copy3(&pTargetThing->lookOrientation.scale, &pTargetThing->position);
	rdMatrix_TransformPoint34Acc(&anchor, &pTargetThing->lookOrientation);
	//rdVector_Add3Acc(&anchor , &pTargetThing->position);
	//rdVector_Zero3(&pTargetThing->lookOrientation.scale);

	rdVector3 relativePos;
	rdVector_Sub3(&relativePos, &anchor, &pJointThing->position);

	float currentDistance = rdVector_Len3(&relativePos);
	float offset = distance - currentDistance;
	offset = stdMath_ClipPrecision(offset);

	if (stdMath_Fabs(offset) <= 0.0f)
		return;

	rdVector3 offsetDir;
	rdVector_Normalize3(&offsetDir, &relativePos);

	rdVector3 relativeVelocity;
	rdVector_Sub3(&relativeVelocity, &pTargetThing->physicsParams.vel, &pJointThing->physicsParams.vel);

	float invMassA = 1.0f / pTargetThing->physicsParams.mass;
	float invMassB = 1.0f / pJointThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;


//	float diff = -offset / (constraintMass);
//	sithCollision_UpdateThingCollision(pJointThing, &offsetDir, diff * invMassB, 0);
//
//	rdVector_Neg3Acc(&offsetDir);
//	sithCollision_UpdateThingCollision(pTargetThing, &offsetDir, diff * invMassA, 0);


//	float diff = -offset / (constraintMass);
//	rdVector_Neg3Acc(&offsetDir);
//	
//	sithCollision_UpdateThingCollision(pTargetThing, &offsetDir, diff * invMassA, 0);
//	
//	rdVector_Neg3Acc(&offsetDir);
//	sithCollision_UpdateThingCollision(pJointThing, &offsetDir, diff * invMassB, 0);
//

//	//if (!(pBodyPartA->flags & JOINTFLAGS_PINNED))
//	{
//		rdVector3 deltaA;
//		rdVector_Scale3(&deltaA, &offsetDir, (double)diff * invMassA);
//
//		rdVector3 offsetA;
//		rdVector_Add3(&offsetA, &pTargetThing->position, &deltaA);
//		rdVector_Add3Acc(&pTargetJoint->nextPosAcc, &offsetA);
//		pTargetJoint->nextPosWeight++;
//		pThing->puppet->physics->constrainedJoints |= 1ull << targetJointIdx;
//	}
//
//	//if (!(pBodyPartB->flags & JOINTFLAGS_PINNED))
//	{
//		rdVector3 deltaB;
//		rdVector_Scale3(&deltaB, &offsetDir, (double)diff * invMassB);
//
//		rdVector3 offsetB;
//		rdVector_Sub3(&offsetB, &pJointThing->position, &deltaB);
//		rdVector_Add3Acc(&pJoint->nextPosAcc, &offsetB);
//		pJoint->nextPosWeight++;
//		pThing->puppet->physics->constrainedJoints |= 1ull << jointIdx;
//	}

	// how much of their relative force is affecting the constraint
	float velocityDot = rdVector_Dot3(&relativeVelocity, &offsetDir);
	velocityDot = stdMath_ClipPrecision(velocityDot);

	const float biasFactor = 0.01f;
	float bias = -(biasFactor / deltaSeconds) * offset;
	bias = stdMath_ClipPrecision(bias);

	float lambda = -(velocityDot + bias) / constraintMass;
	lambda = stdMath_ClipPrecision(lambda);

	const float dampingFactor = 0.2f;
	//lambda *= 1.0f - dampingFactor;

	rdVector3 aImpulse;
	rdVector_Scale3(&aImpulse, &offsetDir, lambda);
	
	rdVector3 bImpulse;
	rdVector_Scale3(&bImpulse, &offsetDir, -lambda);

	//rdVector3 dampingForce;
	//rdVector_Scale3(&dampingForce, &aImpulse, dampingFactor);

	//rdVector_Sub3Acc(&aImpulse, &dampingForce);
	//rdVector_Sub3Acc(&bImpulse, &dampingForce);
	
	//if (forceVec->z * invMass > 0.5) // TODO verify
		//sithThing_DetachThing(pThing);
//	rdVector_MultAcc3(&pTargetThing->physicsParams.vel, &aImpulse, invMassA);
	//pTargetThing->physicsParams.physflags |= SITH_PF_HAS_FORCE;

//	rdVector_MultAcc3(&pJointThing->physicsParams.vel, &bImpulse, invMassB);
	//pJointThing->physicsParams.physflags |= SITH_PF_HAS_FORCE;

	sithPhysics_ThingApplyForce(pTargetThing, &aImpulse);
	sithPhysics_ThingApplyForce(pJointThing, &bImpulse);

	//sithPhysics_ApplyDrag(&pTargetThing->physicsParams.vel, 0.0f, 0.001f, deltaSeconds);
//	sithPhysics_ApplyDrag(&pJointThing->physicsParams.vel, 0.0f, 0.001f, deltaSeconds);

}


void sithPuppet_ApplyConeConstraint(sithThing* pThing,
	int jointIdx,
	int referenceJointIdx,
	float maxPitch,
	float minPitch,
	float maxYaw,
	float minYaw,
	float maxRoll,
	float minRoll,
	float deltaSeconds
)
{
	int hasReferenceJoint = referenceJointIdx < 0 ? 0 : pThing->animclass->jointBits & (1ull << referenceJointIdx);
	if(!hasReferenceJoint)
		return;

	sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];
	sithThing* pJointThing = &pJoint->thing;

	sithBodyPart* pReferenceBodyPart = &pThing->animclass->bodypart[referenceJointIdx];
	sithPuppetJoint* pReferenceJoint = &pThing->puppet->physics->joints[referenceJointIdx];
	rdHierarchyNode* pReferenceNode = &pThing->rdthing.model3->hierarchyNodes[pReferenceBodyPart->nodeIdx];
	sithThing* pReferenceThing = &pReferenceJoint->thing;

	// the base pose local up vector is the cone axis
	// transform it to world space relative to the parent
	//rdVector3* refAxis = &(&pReferenceThing->lookOrientation.rvec)[axis];

	rdMatrix34 parentRotTranspose, relativeRotation;
	rdMatrix_InvertOrtho34(&parentRotTranspose, &pThing->rdthing.hierarchyNodeMatrices[pReferenceBodyPart->nodeIdx]);// &pReferenceThing->lookOrientation);
	rdMatrix_Multiply34(&relativeRotation, &parentRotTranspose, &pJointThing->lookOrientation);

	rdVector3 angles;
	rdMatrix_ExtractAngles34(&relativeRotation, &angles);

	rdVector3 constrainedAngles = angles;
	constrainedAngles.x = stdMath_Clamp(constrainedAngles.x, minPitch, maxPitch);
	constrainedAngles.y = stdMath_Clamp(constrainedAngles.y, minYaw, maxYaw);
	constrainedAngles.z = stdMath_Clamp(constrainedAngles.z, minRoll, maxRoll);

	rdVector_ClipPrecision3(&constrainedAngles);

	//if (fabsf(constrainedAngles.x) > maxSwingAngle)
	//	constrainedAngles.x = maxSwingAngle * (constrainedAngles.x > 0 ? 1 : -1);
	//
	//if (fabsf(constrainedAngles.y) > maxTwistAngle)
	//	constrainedAngles.y = maxTwistAngle * (constrainedAngles.y > 0 ? 1 : -1);
	//
	//if (fabsf(constrainedAngles.z) > maxSwingAngle)
	//	constrainedAngles.z = maxSwingAngle * (constrainedAngles.z > 0 ? 1 : -1);

//	rdMatrix34 constrainedRotation;
//	rdMatrix_BuildRotate34(&constrainedRotation, &constrainedAngles);
//	rdMatrix_Multiply34(&pJointThing->lookOrientation, &pReferenceThing->lookOrientation, &constrainedRotation);

	rdVector3 angleDifferences;
	rdVector_Sub3(&angleDifferences, &constrainedAngles, &angles);
	
//	rdVector_MultAcc3(&pJointThing->physicsParams.angVel, &angleDifferences, 1.0f / deltaSeconds);
	rdVector_Add3Acc(&pJointThing->physicsParams.angVel, &angleDifferences);

//	sithPhysics_ApplyDrag(&pJointThing->physicsParams.angVel, 1.0f, 0.0, deltaSeconds);

//	rdVector_MultAcc3(&pJointThing->physicsParams.angVel, &angles, 1.0f / deltaSeconds);

	/*rdVector3 coneAxis;
	rdVector_Copy3(&coneAxis, refAxis);

	rdVector3 childForward;
	rdVector_Copy3(&childForward, &pJointThing->lookOrientation.uvec);

	rdVector_Normalize3Acc(&coneAxis);
	rdVector_Normalize3Acc(&childForward);

	float dotProd = rdVector_Dot3(&coneAxis, &childForward);
	float currentAngle = acosf(dotProd);
	maxConeAngle /= 180.0 / M_PI;
	if (currentAngle > maxConeAngle)
	{
		float correctionAngle = currentAngle - maxConeAngle;

		rdVector3 correctionAxis;
		rdVector_Cross3(&correctionAxis, &childForward, &coneAxis);
		rdVector_Normalize3Acc(&correctionAxis);

		rdMatrix34 rotMat;
		rdMatrix_BuildFromAxisAngle34(&rotMat, &correctionAxis, -correctionAngle * (180.0 / M_PI));

		rdVector3 tmp;
		rdVector_Sub3(&tmp, &pJointThing->position, &pReferenceThing->position);
		rdVector_Copy3(&pJointThing->lookOrientation.scale, &tmp);

		rdMatrix_PreMultiply34(&pJointThing->lookOrientation, &rotMat);

		rdVector3 a1a;
		rdVector_Sub3(&a1a, &pJointThing->lookOrientation.scale, &tmp);
		if (!rdVector_IsZero3(&a1a))
			sithCollision_UpdateThingCollision(pJointThing, &a1a, rdVector_Normalize3Acc(&a1a), 0);

		rdVector_Zero3(&pJointThing->lookOrientation.scale);
	}*/
}


void sithPuppet_ApplyLookConstraint(sithThing* pThing, int jointIdx, int targetJointIdx, int invertDir, float deltaSeconds)
{
	int hasTargetJoint = targetJointIdx < 0 ? 0 : pThing->animclass->jointBits & (1ull << targetJointIdx);
	if(!hasTargetJoint)
		return;

	sithPuppetJoint* pTargetJoint = &pThing->puppet->physics->joints[targetJointIdx];
	sithPuppetJoint* pJoint       = &pThing->puppet->physics->joints[jointIdx];
	rdHierarchyNode* pNode        = &pThing->rdthing.model3->hierarchyNodes[pThing->animclass->bodypart[jointIdx].nodeIdx];

	rdMatrix34 refMat;
	rdMatrix_Multiply34(&refMat, &pNode->posRotMatrix, &pTargetJoint->thing.lookOrientation);

	rdMatrix34* pMat = &pJoint->thing.lookOrientation;
	rdVector_Sub3(&pMat->uvec, &pTargetJoint->thing.position, &pJoint->thing.position);
	rdVector_Normalize3Acc(&pMat->uvec);

	if (invertDir)
		rdVector_Neg3Acc(&pMat->uvec);

	rdVector_Cross3(&pMat->rvec, &refMat.lvec, &pMat->uvec);
	rdVector_Normalize3Acc(&pMat->rvec);

	rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);
	rdVector_Normalize3Acc(&pMat->lvec);
}
#else

void sithPuppet_DoDistanceConstraint(sithThing* pThing, sithThing* pJointA, sithThing* pJointB, int jointA, int jointB, float distance, float minDist, float deltaSeconds)
{
	rdVector3 relativePos;
	rdVector_Sub3(&relativePos, &pJointA->position, &pJointB->position);
	float currentDistance = rdVector_Len3(&relativePos);
	if (minDist >= 0 && currentDistance > distance * minDist)
		return;

	float offset = distance - currentDistance;
	offset = stdMath_ClipPrecision(offset);
	if (stdMath_Fabs(offset) > 0.0f)
	{
		rdVector3 offsetDir;
		rdVector_Normalize3(&offsetDir, &relativePos);

		float invMassA = 1.0f / pJointA->physicsParams.mass;
		float invMassB = 1.0f / pJointB->physicsParams.mass;
		float constraintMass = invMassA + invMassB;
		if (constraintMass > 0.0f)
		{
			rdVector3 relativeVelocity;
			rdVector_Sub3(&relativeVelocity, &pJointA->physicsParams.vel, &pJointB->physicsParams.vel);

			// how much of their relative force is affecting the constraint
		//	float velocityDot = rdVector_Dot3(&physA->physicsParams.vel, &offsetDir) - rdVector_Dot3(&physB->physicsParams.vel, &offsetDir);
		//	velocityDot = stdMath_ClipPrecision(velocityDot);
		//	if (velocityDot <= 0.0)
		//		return;
		//
		//	float senderb = (physA->physicsParams.mass * physB->physicsParams.mass + physA->physicsParams.mass * physB->physicsParams.mass)
		//		/ (physA->physicsParams.mass + physB->physicsParams.mass);
		//
		//	rdVector3 forceVec;
		//	rdVector_Scale3(&forceVec, &offsetDir, (velocityDot + offset) * senderb);
		//
		//	sithPhysics_ThingApplyForce(physB, &forceVec);
		//
		//	rdVector_Neg3Acc(&forceVec);
		//	sithPhysics_ThingApplyForce(physA, &forceVec);

			float velocityDot = rdVector_Dot3(&relativeVelocity, &offsetDir);
			velocityDot = stdMath_ClipPrecision(velocityDot);

			//const float biasFactor = 0.5f;
			//float bias = -(biasFactor / deltaSeconds) * offset;
			//
			//float lambda = -(velocityDot + bias) / constraintMass;
			float lambda = offset / constraintMass;

			const float dampingFactor = 0.2f;
			//lambda *= 1.0f - dampingFactor;

			rdVector3 aImpulse;
			rdVector_Scale3(&aImpulse, &offsetDir, lambda * invMassA);// * deltaSeconds);

			rdVector3 bImpulse;
			rdVector_Scale3(&bImpulse, &offsetDir, -lambda * invMassB);// * deltaSeconds);

			//rdVector3 dampingForce;
			//rdVector_Scale3(&dampingForce, &aImpulse, dampingFactor);
			//
			//rdVector_Sub3Acc(&aImpulse, &dampingForce);
			//rdVector_Sub3Acc(&bImpulse, &dampingForce);

			//rdVector_MultAcc3(&pJointA->physicsParams.vel, &aImpulse, invMassA);
			//rdVector_MultAcc3(&pJointB->physicsParams.vel, &bImpulse, invMassB);

			rdVector3 posA, posB;
			rdVector_Add3(&posA, &pJointA->position, &aImpulse);
			rdVector_Add3(&posB, &pJointB->position, &bImpulse);
		
			rdVector_Add3Acc(&pJointA->nextPosAcc, &posA);
			rdVector_Add3Acc(&pJointB->nextPosAcc, &posB);
			
			++pJointA->nextPosWeight;
			++pJointB->nextPosWeight;
			pThing->puppet->physics->constrainedJoints |= 1ull << jointA;
			pThing->puppet->physics->constrainedJoints |= 1ull << jointB;

			//sithPhysics_ThingApplyForce(physA, &aImpulse);
			//sithPhysics_ThingApplyForce(physB, &bImpulse);
		}
	}
}
#endif

void sithPuppet_ApplyConstraints(sithThing* pThing, float deltaSeconds)
{
	// clear the constraint bitmask
	pThing->puppet->physics->constrainedJoints = 0;

	//uint64_t jointBits = pThing->animclass->jointBits;
	//while (jointBits != 0)
	//{
	//	int jointIdx = stdMath_FindLSB64(jointBits);
	//	jointBits ^= 1ull << jointIdx;
	//
	//	sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
	//	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
	//	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];
	//	sithThing* physB = &pJoint->thing;
	//	
	//	sithConstraint* constraint = physB->constraints;
	//	while (constraint)
	//	{
	//		switch (constraint->type)
	//		{
	//		case SITH_CONSTRAINT_DISTANCE:
	//			sithConstraint_ApplyDistanceConstraint(constraint, physB, sithTime_deltaSeconds);
	//			break;
	//		case SITH_CONSTRAINT_CONE:
	//			sithConstraint_ConeConstrain(constraint, physB, sithTime_deltaSeconds);
	//			break;
	//		case SITH_CONSTRAINT_LOOK:
	//			sithConstraint_ApplyLookConstraint(constraint, physB);
	//			break;
	//		default:
	//			break;
	//		}
	//		constraint = constraint->next;
	//	}
	//}

#if 0
	//sithPuppet_DoConstraints(pThing, JOINTTYPE_HEAD, deltaSeconds);
	//sithPuppet_DoConstraints(pThing, JOINTTYPE_NECK, deltaSeconds);
	//
	//
	//sithPuppet_DoConstraints(pThing, JOINTTYPE_TORSO, deltaSeconds);
	//sithPuppet_DoConstraints(pThing, JOINTTYPE_HIP, deltaSeconds);

	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, deltaSeconds);
	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, deltaSeconds);
	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, deltaSeconds);


	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 10.0f, 20.0f, deltaSeconds);
	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 5.0f, 5.0f, deltaSeconds);
	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 10.0f, 10.0f, deltaSeconds);


	sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_HIP, JOINTTYPE_TORSO, 0, deltaSeconds);

	//sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 1, deltaSeconds);
	//
	//sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_HIP, JOINTTYPE_TORSO, 0, deltaSeconds);
	//sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 1, deltaSeconds);
	//sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_NECK, 0, deltaSeconds);
	//
	//sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_HEAD, 0, deltaSeconds);
	//sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 1, deltaSeconds);
	//sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 1, deltaSeconds);


	return;
	uint64_t jointBits = pThing->animclass->jointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];
		sithThing* physB = &pJoint->thing;

		rdVector_Zero3(&physB->lookOrientation.scale);

		int parentIdx = sithPuppet_jointParent[jointIdx];
		int hasParentJoint = parentIdx < 0 ? 0 : pThing->animclass->jointBits & (1ull << parentIdx);
		if (hasParentJoint)
		{
			sithBodyPart* pParentBodyPart = &pThing->animclass->bodypart[parentIdx];
			sithPuppetJoint* pParentJoint = &pThing->puppet->physics->joints[parentIdx];
			rdHierarchyNode* pParentNode = &pThing->rdthing.model3->hierarchyNodes[pParentBodyPart->nodeIdx];
			sithThing* physA = &pParentJoint->thing;

			// distance constraint

			// todo: precompute the constraint distance
			rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPart->nodeIdx].scale;
			rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pParentBodyPart->nodeIdx].scale;
			float distance = rdVector_Dist3(pBasePosB, pBasePosA);

			rdVector_Copy3(&physA->lookOrientation.scale, &physA->position);

			rdMatrix34 inv;
			rdMatrix_InvertOrtho34(&inv, &pThing->rdthing.model3->paBasePoseMatrices[pParentBodyPart->nodeIdx]);

			rdVector3 anchor;
			rdMatrix_TransformPoint34(&anchor, pBasePosA, &inv);
			rdMatrix_TransformPoint34Acc(&anchor, &physA->lookOrientation);
			//rdVector_Add3Acc(&anchor, &physA->position);
			rdVector_Zero3(&physA->lookOrientation.scale);

			rdVector3 relativePos;
			rdVector_Sub3(&relativePos, &anchor, &pJoint->thing.position);

			float currentDistance = rdVector_Len3(&relativePos);
			float offset = distance - currentDistance;
			if (stdMath_Fabs(offset) > 0.0f)
			{
				rdVector3 offsetDir;
				rdVector_Normalize3(&offsetDir, &relativePos);

				rdVector3 relativeVelocity;
				rdVector_Sub3(&relativeVelocity, &physA->physicsParams.vel, &physB->physicsParams.vel);

				float invMassA = 1.0f / physA->physicsParams.mass;
				float invMassB = 1.0f / physB->physicsParams.mass;
				float constraintMass = invMassA + invMassB;
				if (constraintMass > 0.0f)
				{
				//	float diff = offset / (constraintMass);
				//
				//	//if (!(pBodyPartA->flags & JOINTFLAGS_PINNED))
				//	{
				//		rdVector3 deltaA;
				//		rdVector_Scale3(&deltaA, &offsetDir, (double)diff * invMassA);
				//
				//		rdVector3 offsetA;
				//		rdVector_Add3(&offsetA, &physA->position, &deltaA);
				//		rdVector_Add3Acc(&pParentJoint->nextPosAcc, &offsetA);
				//		pParentJoint->nextPosWeight++;
				//		pThing->puppet->physics->constrainedJoints |= 1ull << parentIdx;
				//	}
				//
				//	//if (!(pBodyPartB->flags & JOINTFLAGS_PINNED))
				//	{
				//		rdVector3 deltaB;
				//		rdVector_Scale3(&deltaB, &offsetDir, (double)diff * invMassB);
				//
				//		rdVector3 offsetB;
				//		rdVector_Sub3(&offsetB, &physB->position, &deltaB);
				//		rdVector_Add3Acc(&pJoint->nextPosAcc, &offsetB);
				//		pJoint->nextPosWeight++;
				//		pThing->puppet->physics->constrainedJoints |= 1ull << jointIdx;
				//	}

					// how much of their relative force is affecting the constraint
					float velocityDot = rdVector_Dot3(&relativeVelocity, &offsetDir);
					
					const float biasFactor = 0.01f;
					float bias = -(biasFactor / deltaSeconds) * offset;
					
					float lambda = -(velocityDot + bias) / constraintMass;
					rdVector3 aImpulse;
					rdVector_Scale3(&aImpulse, &offsetDir, lambda);
					
					rdVector3 bImpulse;
					rdVector_Scale3(&bImpulse, &offsetDir, -lambda);
					
					sithPhysics_ThingApplyForce(physA, &aImpulse);
					sithPhysics_ThingApplyForce(physB, &bImpulse);
				}
			}

			// the base pose local up vector is the cone axis
			// transform it to world space relative to the parent
			rdVector3 coneAxis;
			rdMatrix_TransformVector34(&coneAxis, &pNode->posRotMatrix.uvec, &physA->lookOrientation);
			//rdVector_Normalize3(&coneAxis, &pNode->pos);
			//rdMatrix_TransformVector34Acc(&coneAxis, &physA->lookOrientation);

			////rdVector_Set3(&coneAxis, 0.0f, 0.0f, 1.0f);

			rdVector3 childForward;
			rdVector_Copy3(&childForward, &physB->lookOrientation.uvec);


			float dotProd = rdVector_Dot3(&coneAxis, &childForward);
			float currentAngle = 90.0f - stdMath_ArcSin3(dotProd);

			float maxConeAngle = 5.0f;
			if (currentAngle > maxConeAngle)
			{
				float correctionAngle = currentAngle - maxConeAngle;
				
				rdVector3 correctionAxis;
				rdVector_Cross3(&correctionAxis, &coneAxis, &childForward);
				rdVector_Normalize3Acc(&correctionAxis);

				rdMatrix34 rotMat;
				rdMatrix_BuildFromVectorAngle34(&rotMat, &correctionAxis, -correctionAngle);
				
				rdVector3 pyr;
				rdMatrix_ExtractAngles34(&rotMat, &pyr);

				//rdQuat correctionQuat;
				//rdQuat_BuildFromAxisAngle(&correctionQuat, &axis, -correctionAngle);
				//rdQuat_NormalizeAcc(&correctionQuat);
				//
				//rdVector3 pyr;
				//rdQuat_ExtractAngles(&correctionQuat, &pyr);
			//	rdVector_MultAcc3(&physB->physicsParams.angVel, &pyr, 1.0f / deltaSeconds);

				//rdVector3 angVelCorrection;
				//rdVector_Scale3(&angVelCorrection, &axis, correctionAngle / deltaSeconds);
				//rdVector_Add3Acc(&physB->physicsParams.angVel, &angVelCorrection);

			//	rdMatrix34 rotMat;
			//	rdMatrix_BuildFromAxisAngle34(&rotMat, &axis, -correctionAngle);
			//
			////	rdMatrix_Multiply34(&physB->lookOrientation, &physA->lookOrientation, &rotMat);
			//	rdMatrix_PostMultiply34(&physB->lookOrientation, &rotMat);

				//rdQuat correctionQuat;
				//rdQuat_BuildFromAxisAngle(&correctionQuat, &axis, -correctionAngle);
				//rdQuat_NormalizeAcc(&correctionQuat);
				//
				//rdQuat childRot;
				//rdQuat_BuildFrom34(&childRot, &physB->lookOrientation);
				//
				//rdQuat newRotation;
				//rdQuat_Mul(&newRotation, &correctionQuat, &childRot);
				//rdQuat_ToMatrix(&physB->lookOrientation, &newRotation);
			}

			// make sure the up vector remains pointing away
			//rdVector3 expectedUpVector;
			//rdVector_Cross3(&expectedUpVector, &physA->lookOrientation.rvec, &physA->lookOrientation.lvec);
			//
			//if (rdVector_Dot3(&expectedUpVector, &physB->lookOrientation.uvec) < 0.0f)
			//{
			//	// The up vector is inverted, apply a correction
			//	rdVector_Scale3Acc(&parent->transform.m[1], -1.0f);
			//	rdVector_Cross3(&parent->transform.m[0], &parent->transform.m[2], &parent->transform.m[1]);
			//	rdVector_Normalize3Acc(&parent->transform.m[0]);
			//}
		}

		int childIdx = sithPuppet_jointChild[jointIdx];
		int hasChildJoint = childIdx < 0 ? 0 : pThing->animclass->jointBits & (1ull << childIdx);
		if (0)//hasChildJoint)
		{
			sithBodyPart* pChildBodyPart = &pThing->animclass->bodypart[childIdx];
			sithPuppetJoint* pChildJoint = &pThing->puppet->physics->joints[childIdx];
			rdHierarchyNode* pChildNode = &pThing->rdthing.model3->hierarchyNodes[pChildBodyPart->nodeIdx];
			sithThing* physA = &pChildJoint->thing;

			// lookat
		//	rdVector3 direction;
		//	rdVector_Sub3(&direction, &physA->position, &physB->position);
		//	rdVector_Normalize3Acc(&direction);
		//
		//	if(jointIdx > JOINTTYPE_HIP) // some joints have inverse relationship
		//		rdVector_Neg3Acc(&direction);
		//	
		//	rdMatrix34 transform;
		//	rdMatrix_BuildFromLook34(&physB->lookOrientation, &direction);
		//
		//	
		//	rdVector3 r = {-90.0f, 0.0f, 0.0f};
		//	rdMatrix34 rot;
		//	rdMatrix_PostRotate34(&physB->lookOrientation, &r);

			//rdMatrix_LookAt(&physB->lookOrientation, &direction, &physA->lookOrientation.uvec, 0.0f);

			rdMatrix34* pMat = &pJoint->thing.lookOrientation;
			
			// calculate the up vector
			rdVector_Sub3(&pMat->uvec, &pChildJoint->thing.position, &pJoint->thing.position);
			rdVector_Normalize3Acc(&pMat->uvec);
			
			if (jointIdx > JOINTTYPE_HIP) // some joints have inverse relationship
				rdVector_Neg3Acc(&pMat->uvec);
			
			rdVector_Cross3(&pMat->rvec, &pMat->lvec, &pMat->uvec);
			rdVector_Normalize3Acc(&pMat->rvec);
			
			rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);
			rdVector_Normalize3Acc(&pMat->lvec); 
			
			// ensure the vectors form an orthonormal set
			//rdVector_Cross3(&pMat->uvec, &pMat->rvec, &pMat->lvec);
			//rdVector_Normalize3Acc(&pMat->uvec);
		}
	}

#endif

#ifndef RIGID_BODY
	int numConstraints = ARRAYSIZE(sithPuppet_constraints) - 1;
	//for (int i = 0; i < ARRAYSIZE(sithPuppet_constraints); ++i)
	for (int i = numConstraints; i >= 0; --i)
	{
		sithPuppetConstraint* pConstraint = &sithPuppet_constraints[i];
	
		int hasJointA = pThing->animclass->jointBits & (1ull << pConstraint->jointA);
		int hasJointB = pThing->animclass->jointBits & (1ull << pConstraint->jointB);
		if(!hasJointA || !hasJointB)
			continue;
	
		float distance = pThing->puppet->physics->constraintDistances[i];// rdVector_Dist3(&basePosB, &basePosA);// * 0.95f;

		sithPuppetJoint* pJointA = &pThing->puppet->physics->joints[pConstraint->jointA];
		sithPuppetJoint* pJointB = &pThing->puppet->physics->joints[pConstraint->jointB];

		//rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[pConstraint->jointA].nodeIdx].scale;
		//rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pThing->animclass->bodypart[pConstraint->jointB].nodeIdx].scale;
		//float distance = rdVector_Dist3(pBasePosB, pBasePosA);

		if (pJointA->numThings > 1)
			distance += 0.005;
		if (pJointB->numThings > 1)
			distance += 0.005;

		for (int i = 0; i < pJointA->numThings; ++i)
			for (int j = 0; j < pJointB->numThings; ++j)
				sithPuppet_DoDistanceConstraint(pThing, &pJointA->things[i], &pJointB->things[j], pConstraint->jointA, pConstraint->jointB, distance, pConstraint->minDist, deltaSeconds);
	}

	uint64_t jointBits = pThing->animclass->physicsJointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		if(pJoint->numThings > 1)
			sithPuppet_DoDistanceConstraint(pThing, &pJoint->things[0], &pJoint->things[1], jointIdx, jointIdx, 0.01f, -1.0f, deltaSeconds);
	}
#endif
}

static void sithPuppet_ConstrainAxis(rdVector3* vec, const rdVector3* reference, float maxAngle)
{
	float dot = rdVector_Dot3(vec, reference);
	float angle = 90.0f - stdMath_ArcSin3(dot);
	if (angle > maxAngle)
	{
		float t = (angle - maxAngle) / angle;

		rdVector3 lerped;
		rdVector_Lerp3(&lerped, vec, reference, t);
		rdVector_Normalize3(vec, &lerped);
	}
}

#ifndef RIGID_BODY
void sithPuppet_GetJointCenter(rdVector3* center, sithPuppetJoint* pJoint)
{
	*center = rdroid_zeroVector3;
	for (int i = 0; i < pJoint->numThings; ++i)
		rdVector_Add3Acc(center, &pJoint->things[i].position);
	rdVector_InvScale3Acc(center, (float)pJoint->numThings);
}
#endif

void sithPuppet_BuildJointMatrices(sithThing* thing)
{
//	rdVector_Copy3(&thing->lookOrientation.scale, &thing->position);
	

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
#ifdef RIGID_BODY
		//rdMatrix_Copy34(&pJoint->localMat, &pJoint->thing.lookOrientation);
		//rdVector_Copy3(&pJoint->localMat.scale, &pJoint->thing.position);
		
		rdVector3 oldPos = pJoint->localMat.scale;
		rdQuat q0, q1;
		rdQuat_BuildFrom34(&q0, &pJoint->localMat);
		rdQuat_BuildFrom34(&q1, &pJoint->thing.lookOrientation);

		rdQuat q;
		rdQuat_Slerp(&q, &q0, &q1, 0.1f);

		rdQuat_ToMatrix(&pJoint->localMat, &q);

		rdVector_Lerp3(&pJoint->localMat.scale, &oldPos, &pJoint->thing.position, 0.1f);

		//rdVector_Copy3(&pJoint->localMat.scale, &pJoint->things[0].position);
		rdMatrix_Normalize34(&pJoint->localMat);

		rdMatrix_Normalize34(&pJoint->localMat);

		thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->localMat;

#else
	//	rdMatrix34 invParentMat;
	//	rdMatrix_InvertOrtho34(&invParentMat, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);
	//	rdMatrix_PostTranslate34(&invParentMat, &pNode->parent->pivot);
	//
	//	rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pJoint->thing.position);
	//
	//	rdMatrix_Multiply34(&pJoint->localMat, &invParentMat, &pJoint->thing.lookOrientation);
	//
	//	rdVector3 negPiv;
	//	rdVector_Neg3(&negPiv, &pNode->pivot);
	//	//rdMatrix_PreTranslate34(&pJoint->localMat, &negPiv);
	//
	//
	//	thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->localMat;
	//
	//	continue;


	//rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pJoint->thing.position);
	rdVector3 oldPos = pJoint->localMat.scale;
	//rdMatrix_Copy34(&pJoint->localMat, &pJoint->things[0].lookOrientation);

	rdQuat q0, q1;
	rdQuat_BuildFrom34(&q0, &pJoint->localMat);
	rdQuat_BuildFrom34(&q1, &pJoint->things[0].lookOrientation);

	rdQuat q;
	rdQuat_Slerp(&q, &q0, &q1, 0.1f);

	rdQuat_ToMatrix(&pJoint->localMat, &q);

	rdVector_Lerp3(&pJoint->localMat.scale, &oldPos, &pJoint->things[0].position, 0.1f);

	//rdVector_Copy3(&pJoint->localMat.scale, &pJoint->things[0].position);
	rdMatrix_Normalize34(&pJoint->localMat);

	{
		rdVector3 negNodePivot = { pNode->pivot.x, pNode->pivot.y, pNode->pivot.z };
		rdMatrix_TransformVector34Acc(&negNodePivot, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);

		rdVector3 negParentPivot = { -pNode->parent->pivot.x, -pNode->parent->pivot.y, -pNode->pivot.z };
		rdMatrix_TransformVector34Acc(&negParentPivot, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);

		//rdVector_Add3Acc(&pJoint->localMat.scale, &negNodePivot);
		//rdVector_Add3Acc(&pJoint->localMat.scale, &negParentPivot);
	}

	thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->localMat;
	continue;


		sithPuppetJointFrame* pFrame = &sithPuppet_jointFrames[jointIdx];
		if(pFrame->targetJoint < 0)
		{
			rdMatrix_Copy34(&pJoint->localMat, &pNode->posRotMatrix);
			sithPuppet_GetJointCenter(&pJoint->localMat.scale, pJoint);

			thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->localMat;// &pJoint->thing.lookOrientation;
			continue;
		}

		sithPuppetJoint* pTargetJoint = &thing->puppet->physics->joints[pFrame->targetJoint];
		rdMatrix34* pMat = &pJoint->things[0].lookOrientation;
	#if 1
		rdVector3 negNodePivot = { pNode->pivot.x, pNode->pivot.y, pNode->pivot.z };
		rdMatrix_TransformVector34Acc(&negNodePivot, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);

		rdVector3 negParentPivot = { -pNode->parent->pivot.x, -pNode->parent->pivot.y, -pNode->pivot.z };
		rdMatrix_TransformVector34Acc(&negParentPivot, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);

		rdVector3 pos;
		sithPuppet_GetJointCenter(&pos, pJoint);

		rdVector_Add3Acc(&pos, &negNodePivot);
		rdVector_Add3Acc(&pos, &negParentPivot);

		rdVector3 targetPos;
		sithPuppet_GetJointCenter(&targetPos, pTargetJoint);

		rdVector3 up;
		if (pFrame->pitchJoint >= 0)
		{
			sithPuppetJoint* pPitchJoint = &thing->puppet->physics->joints[pFrame->pitchJoint];

			rdVector3 pitchPos;
			sithPuppet_GetJointCenter(&pitchPos, pPitchJoint);

			rdVector_Sub3(&up, &targetPos, &pitchPos);
		}
		else
		{
			rdVector_Sub3(&up, &targetPos, &pos);
		}

		rdVector_Normalize3Acc(&up);
		if (pFrame->reversed)
			rdVector_Neg3Acc(&up);

		//up.x = stdMath_Lerp(pJoint->thing.lookOrientation.uvec.x, up.x, 0.5f);
		//up.y = stdMath_Lerp(pJoint->thing.lookOrientation.uvec.y, up.y, 0.5f);
		//up.z = stdMath_Lerp(pJoint->thing.lookOrientation.uvec.z, up.z, 0.5f);
		//rdVector_Normalize3Acc(&up);

		rdVector3 right;

//		if (pFrame->leftJoint >= 0 && pFrame->rightJoint >= 0)
//		{
//			sithPuppetJoint* pLeftJoint = &thing->puppet->physics->joints[pFrame->leftJoint];
//			sithPuppetJoint* pRightJoint = &thing->puppet->physics->joints[pFrame->rightJoint];
//			rdVector_Sub3(&right, &pRightJoint->things[0].position, &pLeftJoint->things[0].position);
//			rdVector_Normalize3Acc(&right);
//		}
		if (pJoint->numThings > 1)
		{
			rdVector3 right;
			rdVector_Sub3(&right, &pJoint->things[0], &pJoint->things[1]);
			rdVector_Normalize3Acc(&right);
		}
		else
		{
			right = pTargetJoint->things[0].lookOrientation.rvec;
		}

		//right.x = stdMath_Lerp(pJoint->thing.lookOrientation.rvec.x, right.x, 0.5f);
		//right.y = stdMath_Lerp(pJoint->thing.lookOrientation.rvec.y, right.y, 0.5f);
		//right.z = stdMath_Lerp(pJoint->thing.lookOrientation.rvec.z, right.z, 0.5f);
		//rdVector_Normalize3Acc(&right);

		rdVector3 front;
		rdVector_Cross3(&front, &up, &right);
		rdVector_Normalize3Acc(&front);

		rdVector_Cross3(&right, &front, &up);
		rdVector_Normalize3Acc(&right);

		pJoint->things[0].lookOrientation.rvec = right;
		pJoint->things[0].lookOrientation.uvec = up;
		pJoint->things[0].lookOrientation.lvec = front;

	//	//sithPhysics_ThingSetLook(&pJoint->thing, &up, 0.0f);
	//	float v20 = 0.0f;//speed * 10.0;
	//	pJoint->thing.lookOrientation.uvec.x = up.x;// * v20 + pThing->lookOrientation.uvec.x;
	//	pJoint->thing.lookOrientation.uvec.y = up.z;// * v20 + pThing->lookOrientation.uvec.y;
	//	pJoint->thing.lookOrientation.uvec.z = up.y;// * v20 + pThing->lookOrientation.uvec.z;
	//	rdVector_Normalize3Acc(&pJoint->thing.lookOrientation.uvec);
	//	pJoint->thing.lookOrientation.lvec.x = (pJoint->thing.lookOrientation.rvec.z * pJoint->thing.lookOrientation.uvec.y) - (pJoint->thing.lookOrientation.rvec.y * pJoint->thing.lookOrientation.uvec.z);
	//	pJoint->thing.lookOrientation.lvec.y = (pJoint->thing.lookOrientation.rvec.x * pJoint->thing.lookOrientation.uvec.z) - (pJoint->thing.lookOrientation.rvec.z * pJoint->thing.lookOrientation.uvec.x);
	//	pJoint->thing.lookOrientation.lvec.z = (pJoint->thing.lookOrientation.rvec.y * pJoint->thing.lookOrientation.uvec.x) - (pJoint->thing.lookOrientation.rvec.x * pJoint->thing.lookOrientation.uvec.y);
	//	rdVector_Normalize3Acc(&pJoint->thing.lookOrientation.lvec);
	//	pJoint->thing.lookOrientation.rvec.x = (pJoint->thing.lookOrientation.lvec.y * pJoint->thing.lookOrientation.uvec.z) - (pJoint->thing.lookOrientation.lvec.z * pJoint->thing.lookOrientation.uvec.y);
	//	pJoint->thing.lookOrientation.rvec.y = (pJoint->thing.lookOrientation.lvec.z * pJoint->thing.lookOrientation.uvec.x) - (pJoint->thing.lookOrientation.lvec.x * pJoint->thing.lookOrientation.uvec.z);
	//	pJoint->thing.lookOrientation.rvec.z = (pJoint->thing.lookOrientation.lvec.x * pJoint->thing.lookOrientation.uvec.y) - (pJoint->thing.lookOrientation.lvec.y * pJoint->thing.lookOrientation.uvec.x);
	//
	//
	//	float dot = rdVector_Dot3(&pJoint->thing.lookOrientation.rvec, &right);
	//	if(pFrame->reversed)
	//		dot = -dot;
	//	if (dot < 0.0f)
	//	{
	//		rdVector_Neg3Acc(&pJoint->thing.lookOrientation.rvec);
	//	}

		//rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pos );//&pJoint->thing.position);

	#elif 0
		rdMatrix34 invParentMat;
		rdMatrix_InvertOrtho34(&invParentMat, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);

		// move our position into the local coordinate frame
		rdVector3 localPos;
		rdMatrix_TransformPoint34(&localPos, &pJoint->thing.position, &invParentMat);
		//rdVector_Copy3(&pMat->scale, &localPos);


		// move the target position into the local coordinate frame
		rdVector3 localTargetPos;
		rdMatrix_TransformPoint34(&localTargetPos, &pTargetJoint->thing.position, &invParentMat);

		// calculate the up local vector
		rdVector_Sub3(&pMat->uvec, &localTargetPos, &localPos);
		rdVector_Normalize3Acc(&pMat->uvec);
		if (pFrame->reversed)
			rdVector_Neg3Acc(&pMat->uvec);

		if (pFrame->pitchJoint >= 0)
		{
			sithPuppetJoint* pPitchJoint = &thing->puppet->physics->joints[pFrame->pitchJoint];

			rdVector3 localPitchPos;
			rdMatrix_TransformPoint34(&localPitchPos, &pPitchJoint->thing.position, &invParentMat);

			rdVector3 referenceUp;
			rdVector_Sub3(&referenceUp, &localTargetPos, &localPitchPos);
			rdVector_Normalize3Acc(&referenceUp);
			if (pFrame->reversed)
				rdVector_Neg3Acc(&referenceUp);

			rdVector_Copy3(&pMat->uvec, &referenceUp);
			//sithPuppet_ConstrainAxis(&pMat->uvec, &referenceUp, pFrame->maxPitch);
		}

		// calculate the local right vector
		rdVector_Cross3(&pMat->rvec, &pNode->posRotMatrix.lvec, &pMat->uvec);
		rdVector_Normalize3Acc(&pMat->rvec);

		// if we have a left and right joint, we have yaw constraint
		if (pFrame->leftJoint >= 0 && pFrame->rightJoint >= 0)
		{
			sithPuppetJoint* pLeftJoint = &thing->puppet->physics->joints[pFrame->leftJoint];
			sithPuppetJoint* pRightJoint = &thing->puppet->physics->joints[pFrame->rightJoint];

			rdVector3 localLeftPos, localRightPos;
			rdMatrix_TransformPoint34(&localLeftPos, &pLeftJoint->thing.position, &invParentMat);
			rdMatrix_TransformPoint34(&localRightPos, &pRightJoint->thing.position, &invParentMat);

			rdVector3 referenceRight;
			rdVector_Sub3(&referenceRight, &localRightPos, &localLeftPos);
			rdVector_Normalize3Acc(&referenceRight);

			rdVector_Copy3(&pMat->rvec, &referenceRight);
			//sithPuppet_ConstrainAxis(&pMat->rvec, &referenceRight, pFrame->maxYaw);
		}

		// calculate the local look vector
		rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);

		// at this point we may have violated orthogonality, so recompute some stuff
		rdVector_Cross3(&pMat->uvec, &pMat->rvec, &pMat->lvec);
		rdVector_Normalize3Acc(&pMat->uvec);

		rdVector_Cross3(&pMat->rvec, &pMat->lvec, &pMat->uvec);
		rdVector_Normalize3Acc(&pMat->rvec);

		rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);

		// move to the local position
		//rdVector_Copy3(&pMat->scale, &localPos);
				// handle the pivot
		rdVector_Sub3Acc(&localPos, &pNode->pivot);
		rdVector_Add3Acc(&localPos, &pNode->parent->pivot);

		rdVector_Copy3(&pMat->scale, &localPos);

		// now we have a local rotation matrix

	#else

		rdMatrix34 refMat;
		rdMatrix_Multiply34(&refMat, &pNode->posRotMatrix, &pTargetJoint->thing.lookOrientation);

		rdVector3 pos = pJoint->thing.position;

		rdVector3 negNodePivot = { pNode->pivot.x, pNode->pivot.y, pNode->pivot.z };
		rdMatrix_TransformVector34Acc(&negNodePivot, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);
		//rdVector_Add3Acc(&pos, &negNodePivot);

		rdVector3 negParentPivot = { -pNode->parent->pivot.x, -pNode->parent->pivot.y, -pNode->pivot.z };
		rdMatrix_TransformVector34Acc(&negParentPivot, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);
		//rdVector_Add3Acc(&pos, &negParentPivot);

		// calculate the up vector
		rdVector_Sub3(&pMat->uvec, &pTargetJoint->thing.position, &pos );//&pJoint->thing.position);

		rdVector_Add3Acc(&pMat->uvec, &negNodePivot);
		rdVector_Add3Acc(&pMat->uvec, &negParentPivot);

		rdVector_Normalize3Acc(&pMat->uvec);
		if(pFrame->reversed)
			rdVector_Neg3Acc(&pMat->uvec);

		// if we have a pitch joint, we have a pitch constraint
		if (pFrame->pitchJoint >= 0)
		{
			sithPuppetJoint* pPitchJoint = &thing->puppet->physics->joints[pFrame->pitchJoint];
			
			rdVector3 referenceUp;
			rdVector_Sub3(&referenceUp, &pTargetJoint->thing.position, &pPitchJoint->thing.position);
			rdVector_Add3Acc(&referenceUp, &negNodePivot);
			rdVector_Add3Acc(&referenceUp, &negParentPivot);

			rdVector_Normalize3Acc(&referenceUp);
			if (pFrame->reversed)
				rdVector_Neg3Acc(&referenceUp);

			rdVector_Copy3(&pMat->uvec, &referenceUp);
			//sithPuppet_ConstrainAxis(&pMat->uvec, &referenceUp, pFrame->maxPitch);
		}

		// calculate the adjusted right vector
		rdVector_Cross3(&pMat->rvec, &refMat.lvec, &pMat->uvec);
		rdVector_Normalize3Acc(&pMat->rvec);

		// if we have a left and right joint, we have yaw constraint
		if (pFrame->leftJoint >= 0 && pFrame->rightJoint >= 0)
		{
			sithPuppetJoint* pLeftJoint = &thing->puppet->physics->joints[pFrame->leftJoint];
			sithPuppetJoint* pRightJoint = &thing->puppet->physics->joints[pFrame->rightJoint];
		
			rdVector3 referenceRight;
			rdVector_Sub3(&referenceRight, &pRightJoint->thing.position, &pLeftJoint->thing.position);
			rdVector_Add3Acc(&referenceRight, &negNodePivot);
			rdVector_Add3Acc(&referenceRight, &negParentPivot);

			rdVector_Normalize3Acc(&referenceRight);
			rdVector_Copy3(&pMat->rvec, &referenceRight);
			//sithPuppet_ConstrainAxis(&pMat->rvec, &referenceRight, pFrame->maxYaw);
		}
		
		rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);

		// at this point we may have violated orthogonality, so recompute some stuff
		rdVector_Cross3(&pMat->uvec, &pMat->rvec, &pMat->lvec);
		rdVector_Normalize3Acc(&pMat->uvec);
		
		rdVector_Cross3(&pMat->rvec, &pMat->lvec, &pMat->uvec);
		rdVector_Normalize3Acc(&pMat->rvec);
		
		rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);

		rdVector_Zero3(&pJoint->thing.lookOrientation.scale);
		
		
		//rdVector_Add3Acc(&pos, &negNodePivot);


//	rdVector3 negParentPivot = { pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->pivot.z };
//	rdMatrix_TransformVector34Acc(&negParentPivot, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);
//	rdVector_Sub3Acc(&pos, &negParentPivot);


		rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pos);// &pJoint->thing.position);

		rdMatrix_PreTranslate34(&pJoint->thing.lookOrientation, &negNodePivot);
		rdMatrix_PreTranslate34(&pJoint->thing.lookOrientation, &negParentPivot);

		sithBodyPart* pTargetBodyPart = &thing->animclass->bodypart[pFrame->targetJoint];
		rdHierarchyNode* pTargetNode = &thing->rdthing.model3->hierarchyNodes[pTargetBodyPart->nodeIdx];

		int parentJoint = thing->animclass->jointToBodypart[pNode->idx];
		pTargetBodyPart = &thing->animclass->bodypart[parentJoint];
		pTargetNode = pNode->parent;



		rdMatrix34 targetMat;
		rdMatrix_Copy34(&targetMat, &pTargetJoint->thing.lookOrientation);
		rdVector_Copy3(&targetMat.scale, &pTargetJoint->thing.position);
		//rdVector_Zero3(&targetMat.scale);

		// Initialize the matrix to the current world space matrix
		//rdMatrix_Copy34(&tempMatrix, &pJoint->thing.lookOrientation);


		rdMatrix_Copy34(&targetMat, &thing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);
	#endif

		rdMatrix34 tempMatrix;
		rdMatrix_Identity34(&tempMatrix);

		// Apply parent's pivot last (Post-translate by parent's pivot)
//		if (pNode->parent)
//		{
//			rdVector3 posParentPivot = { pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->parent->pivot.z };
//			rdMatrix_TransformVector34Acc(&posParentPivot, &targetMat);
//			rdMatrix_BuildTranslate34(&tempMatrix, &posParentPivot);
//		}

		rdMatrix_PostMultiply34(&tempMatrix, &pJoint->things[0].lookOrientation);

//		rdVector3 negNodePivot = { -pNode->pivot.x, -pNode->pivot.y, -pNode->pivot.z };
//		rdMatrix_TransformVector34Acc(&negNodePivot, &targetMat);
//		rdMatrix_PostTranslate34(&tempMatrix, &negNodePivot);


//		// Apply current node's pivot (Pre-translate by node's pivot)
//		rdVector3 negNodePivot = {-pNode->pivot.x, -pNode->pivot.y, -pNode->pivot.z};
//		rdMatrix_TransformVector34Acc(&negNodePivot, &targetMat);
//		
//		rdMatrix34 tempMatrix;
//		rdMatrix_BuildTranslate34(&tempMatrix, &negNodePivot);
//		
//		// Apply node's local transformation
//		rdMatrix_PreMultiply34(&tempMatrix, &pJoint->thing.lookOrientation);
//		
//		// Apply parent's pivot last (Post-translate by parent's pivot)
//		if (pNode->parent)
//		{
//			rdVector3 posParentPivot = { pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->parent->pivot.z};
//			rdMatrix_TransformVector34Acc(&posParentPivot, &targetMat);
//			rdMatrix_PreTranslate34(&tempMatrix, &posParentPivot);
//		}

//////////		// Initialize the matrix to the current world space matrix
//////////		rdMatrix34 tempMatrix;
//////////		rdMatrix_Copy34(&tempMatrix, &pJoint->thing.lookOrientation);
//////////		
//////////		// If the node has a parent, adjust for the parent's pivot
//////////		if (pNode->parent)
//////////		{
//////////			rdVector3 posPivot = { pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->parent->pivot.z};
//////////			rdMatrix_PostTranslate34(&tempMatrix, &posPivot);
//////////		}
//////////		
//////////		// Adjust for the node's pivot
//////////		rdVector3 negPivot = {-pNode->pivot.x, -pNode->pivot.y, -pNode->pivot.z};
//////////		rdMatrix_PreTranslate34(&tempMatrix, &negPivot);

	//////////	// Adjust for the current node's pivot
	//////////	rdVector3 negPivot = {-pNode->pivot.x, -pNode->pivot.y, -pNode->pivot.z};
	//////////	
	//////////	rdMatrix34 negPivotMatrix;
	//////////	rdMatrix_BuildTranslate34(&negPivotMatrix, &negPivot);
	//////////	rdMatrix_PreMultiply34(&tempMatrix, &negPivotMatrix);
	//////////	
	//////////	// Adjust for the parent's pivot if it exists
	//////////	if (pNode->parent)
	//////////	{
	//////////		rdVector3 posPivot = { pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->parent->pivot.z};
	//////////		rdMatrix34 posPivotMatrix;
	//////////		rdMatrix_BuildTranslate34(&posPivotMatrix, &posPivot);
	//////////		rdMatrix_PreMultiply34(&tempMatrix, &posPivotMatrix);
	//////////	}
		
		// Update the world matrix with the adjustments
		rdMatrix_Copy34(&pJoint->localMat, &tempMatrix);
		rdVector_Copy3(&pJoint->localMat.scale, &pos);


///////		// Calculate the inverse of the accumulated matrix (world space transform)
///////		rdMatrix34 invAccMatrix;
///////		rdMatrix_InvertOrtho34(&invAccMatrix, &targetMat);
///////		
///////		// Adjust for the current node's pivot
///////		rdVector3 negPivot = {-pNode->pivot.x, -pNode->pivot.y, -pNode->pivot.z};
///////		rdMatrix_PreTranslate34(&invAccMatrix, &negPivot);
///////		
///////		// Use the node's world space transformation
///////		rdMatrix34 nodeMatrix = pJoint->thing.lookOrientation;
///////		nodeMatrix.scale = pJoint->thing.position;
///////
///////		// Pre-multiply with node's matrix
///////		rdMatrix_PreMultiply34(&invAccMatrix, &nodeMatrix);
///////		
///////		// Adjust for the parent's pivot, if it exists
///////		if (pNode->parent)
///////		{
///////			rdVector3 posPivot = { pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->parent->pivot.z};
///////			rdMatrix_PreTranslate34(&invAccMatrix, &posPivot);
///////		}
///////		
///////		// Set the local matrix
///////		rdMatrix_Copy34(&pJoint->localMat, &invAccMatrix);
///////
///////		rdMatrix_PostMultiply34(&pJoint->localMat, &targetMat);

/////		// Calculate the inverse of the accumulated matrix (world space transform)
/////		rdMatrix34 invAccMatrix;
/////		rdMatrix_InvertOrtho34(&invAccMatrix, &targetMat);
/////		
/////		// Adjust for the current joint's pivot
/////		rdVector3 negPivot = {-pNode->pivot.x, -pNode->pivot.y, -pNode->pivot.z};
/////		rdMatrix34 tempMatrix;
/////		rdMatrix_BuildTranslate34(&tempMatrix, &negPivot);
/////		rdMatrix_PreMultiply34(&invAccMatrix, &tempMatrix);
/////		
/////		// Invert the current node's matrix
/////		rdMatrix34 invNodeMatrix;
/////		rdMatrix_InvertOrtho34(&invNodeMatrix, &pJoint->thing.lookOrientation);
/////		
/////		// Pre-multiply it with the inverse node matrix
/////		rdMatrix_PreMultiply34(&invAccMatrix, &invNodeMatrix);
/////		
/////		// Adjust for the parent's pivot, if it exists
/////		if (pNode->parent)
/////		{
/////			rdVector3 posPivot = {pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->parent->pivot.z};
/////			rdMatrix_PreTranslate34(&invAccMatrix, &posPivot);
/////		}
/////		
/////		// Set the local matrix
/////		rdMatrix_Copy34(&pJoint->localMat, &invAccMatrix);
/////		
/////		rdMatrix_PostMultiply34(&pJoint->localMat, &targetMat);

///		rdMatrix34 invAcc;
///		rdMatrix_InvertOrtho34(&invAcc, &targetMat);
///		
///		rdMatrix_Multiply34(&pJoint->localMat, &invAcc, &pJoint->thing.lookOrientation);
///		
///		if (pNode->parent)
///		{
///			rdVector3 posPivot;
///			posPivot.x = pNode->parent->pivot.x;
///			posPivot.y = pNode->parent->pivot.y;
///			posPivot.z = pNode->parent->pivot.z;
///			rdMatrix_PreTranslate34(&pJoint->localMat, &posPivot);
///		} 
///		
///		rdMatrix34 invNodeMatrix;
///		rdMatrix_InvertOrtho34(&invNodeMatrix, &pJoint->localMat);
///		
///		rdMatrix_PreMultiply34(&pJoint->localMat, &invNodeMatrix);
///		
///		rdVector3 negPivot = {-pNode->pivot.x, -pNode->pivot.y, -pNode->pivot.z};
///		rdMatrix_PreTranslate34(&pJoint->localMat, &negPivot);
		
//		rdMatrix34 invMat;
//		rdMatrix_InvertOrtho34(&invMat, &targetMat);
//
//
//	//	if (pNode->parent)
//	//	{
//	//		rdVector3 negPivot = {-pNode->parent->pivot.x, -pNode->parent->pivot.y, -pNode->parent->pivot.z};
//	//		
//	//		rdMatrix34 temp;
//	//		rdMatrix_BuildTranslate34(&temp, &negPivot);
//	//		rdMatrix_PostMultiply34(&invMat, &temp);
//	//
//	//	}
//	
//		rdVector3 negJointPivot = {pNode->pivot.x, pNode->pivot.y, pNode->pivot.z};
//		rdMatrix34 temp;
//		rdMatrix_BuildTranslate34(&temp, &negJointPivot);
//		rdMatrix_PostMultiply34(&invMat, &temp);
//
//		if (pNode->parent)
//		{
//			rdVector3 negPivot = { pNode->parent->pivot.x, pNode->parent->pivot.y, pNode->parent->pivot.z };
//			rdMatrix_PreTranslate34(&invMat, &negPivot);
//		}
//
//		rdMatrix_Multiply34(&pJoint->localMat, &invMat, &pJoint->thing.lookOrientation);
//
//		//if (pNode->parent)
//		//{
//		//	rdVector3 negPivot = { -pNode->parent->pivot.x, -pNode->parent->pivot.y, -pNode->parent->pivot.z };
//		//	rdMatrix_PostTranslate34(&pJoint->localMat, &negPivot);
//		//}
//
//		//rdMatrix_Copy34(&pJoint->localMat, &pJoint->thing.lookOrientation);
//
//		rdMatrix_PostMultiply34(&pJoint->localMat, &targetMat);

	//	rdMatrix_PostMultiply34(&pJoint->thing.lookOrientation, &invMat);
	//	
	//
	//	rdMatrix34 matrix;
	//	rdMatrix_BuildTranslate34(&matrix, &pNode->pivot);
	//	rdMatrix_PostMultiply34(&matrix, &pJoint->thing.lookOrientation);
	//	if (pNode->parent)
	//	{
	//		rdVector3 negPivot;
	//		negPivot.x = -pNode->parent->pivot.x;
	//		negPivot.y = -pNode->parent->pivot.y;
	//		negPivot.z = -pNode->parent->pivot.z;
	//		rdMatrix_PostTranslate34(&matrix, &negPivot);
	//	}
	//
	//	rdMatrix_Multiply34(&pJoint->thing.lookOrientation, &targetMat, &matrix);

		//rdMatrix_PostTranslate34(&pJoint->thing.lookOrientation, &pJoint->thing.position);


		//rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pJoint->thing.position);


		//rdMatrix_PreMultiply34(&pJoint->thing.lookOrientation, &pNode->posRotMatrix);
	//	rdMatrix_PreTranslate34(&pJoint->thing.lookOrientation, &pNode->pivot);
	//	rdMatrix_PostTranslate34(&pJoint->thing.lookOrientation, &pJoint->thing.position);
	//	if(pNode->parent)
	//	{
	//		rdVector3 negParentPivot;
	//		rdVector_Neg3(&negParentPivot, & pNode->parent->pivot);
	//		rdMatrix_PostTranslate34(&pJoint->thing.lookOrientation, &negParentPivot);
	//	}



		//rdMatrix_PostMultiply34(&pJoint->thing.lookOrientation, &invMat);


		thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->localMat;//pJoint->thing.lookOrientation;
#endif
	}
	
	//rdVector_Zero3(&thing->lookOrientation.scale);
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

	rdVector_Zero3(&pJointThing->physicsParams.accLinearForces);
	rdMatrix_Identity34(&pJointThing->physicsParams.accAngularForces);

	pJointThing->physicsParams.lastPos = pJointThing->position;
	pJointThing->physicsParams.lastOrient = pJointThing->lookOrientation;

	//rdVector_Zero3(&pJointThing->physicsParams.angVel);

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
#ifdef RIGID_BODY
		sithPuppet_UpdateJointThing(pThing, &pJoint->thing, pBodyPart, deltaSeconds);
#else
		for (int i = 0; i < pJoint->numThings; ++i)
			sithPuppet_UpdateJointThing(pThing, &pJoint->things[i], pBodyPart, deltaSeconds);
#endif
	}
}

#ifndef RIGID_BODY

void sithPuppet_UpdatePhysicsJointOrientations(sithThing* pThing, float deltaSeconds)
{
	uint64_t jointBits = pThing->animclass->physicsJointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
		if (pBodyPart->nodeIdx < pThing->rdthing.rootJoint || pThing->rdthing.amputatedJoints[pBodyPart->nodeIdx])
			continue;

		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];

		sithPuppetJointFrame* pFrame = &sithPuppet_jointFrames[jointIdx];
		if (pFrame->targetJoint < 0)
			continue;

		sithPuppetJoint* pTargetJoint = &pThing->puppet->physics->joints[pFrame->targetJoint];
		sithBodyPart* pTargetBodyPart = &pThing->animclass->bodypart[pFrame->targetJoint];
		rdMatrix34* pMat = &pJoint->things[0].lookOrientation;

		rdVector3 negNodePivot = { pNode->pivot.x, pNode->pivot.y, pNode->pivot.z };
		rdMatrix_TransformVector34Acc(&negNodePivot, &pThing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);

		rdVector3 negParentPivot = { -pNode->parent->pivot.x, -pNode->parent->pivot.y, -pNode->pivot.z };
		rdMatrix_TransformVector34Acc(&negParentPivot, &pThing->rdthing.hierarchyNodeMatrices[pNode->parent->idx]);

		rdVector3 pos = pJoint->things[0].position;
		rdVector_Add3Acc(&pos, &negNodePivot);
		rdVector_Add3Acc(&pos, &negParentPivot);

		rdVector3 up;
		if (pFrame->pitchJoint >= 0)
		{
			sithPuppetJoint* pPitchJoint = &pThing->puppet->physics->joints[pFrame->pitchJoint];
			rdVector_Sub3(&up, &pTargetJoint->things[0].position, &pPitchJoint->things[0].position);
		}
		else
		{
			rdVector_Sub3(&up, &pTargetJoint->things[0].position, &pos);
		}

		rdVector_Normalize3Acc(&up);
		if (pFrame->reversed)
			rdVector_Neg3Acc(&up);

		rdVector3 front;
		rdVector3 right;
		if (pFrame->leftJoint >= 0 && pFrame->rightJoint >= 0) // reference frame
		{
			sithPuppetJoint* pLeftJoint = &pThing->puppet->physics->joints[pFrame->leftJoint];
			sithPuppetJoint* pRightJoint = &pThing->puppet->physics->joints[pFrame->rightJoint];

			rdVector3 leftPos, rightPos;
			sithPuppet_GetJointCenter(&leftPos, pLeftJoint);
			sithPuppet_GetJointCenter(&rightPos, pRightJoint);

			rdVector_Sub3(&right, &rightPos, &leftPos);
			rdVector_Normalize3Acc(&right);

			//rdVector_Cross3(&front, &up, &right);
			//rdVector_Normalize3Acc(&front);

			//rdVector_Cross3(&right, &front, &up);
			//rdVector_Normalize3Acc(&right);
		}
		else if (pJoint->numThings > 1)
		{
			rdVector_Sub3(&right, &pJoint->things[0].position, &pJoint->things[1].position);
			rdVector_Normalize3Acc(&right);

		}
		else if (pTargetJoint->numThings > 1)
		{
			rdVector_Sub3(&right, &pTargetJoint->things[0].position, &pTargetJoint->things[1].position);
			rdVector_Normalize3Acc(&right);

		}
		//else if (pThing->animclass->hingeJointBits & (1ull << jointIdx)) // our own hinge
		//{
		//	sithPuppetHingeJoint* pHingeJoint = &pThing->puppet->physics->hingeJoints[jointIdx];
		//	rdVector_Sub3(&right, &pHingeJoint->hingeThings[0].position, &pHingeJoint->hingeThings[1].position);
		//	rdVector_Normalize3Acc(&right);
		//
		//	//rdVector_Sub3(&front, &pHingeJoint->hingeThings[2].position, &pJoint->thing.position);
		//	//rdVector_Normalize3Acc(&front);
		//	//
		//	//rdVector_Cross3(&up, &right, &front);
		//	//rdVector_Normalize3Acc(&up);
		//}
		//else if (pTargetJoint && pThing->animclass->hingeJointBits & (1ull << pFrame->targetJoint)) // target hinge
		//{
		//	sithPuppetHingeJoint* pHingeJoint = &pThing->puppet->physics->hingeJoints[pFrame->targetJoint];
		//	rdVector_Sub3(&right, &pHingeJoint->hingeThings[0].position, &pHingeJoint->hingeThings[1].position);
		//	rdVector_Normalize3Acc(&right);
		//	
		//	//rdVector_Sub3(&front, &pHingeJoint->hingeThings[2].position, &pTargetJoint->thing.position);
		//	//rdVector_Normalize3Acc(&front);
		//	//
		//	//rdVector_Cross3(&up, &right, &front);
		//	//rdVector_Normalize3Acc(&up);
		//}
		else // nothing, just use rvec
		{
			right = pTargetJoint->things[0].lookOrientation.rvec;

			//rdVector_Cross3(&front, &up, &right);
			//rdVector_Normalize3Acc(&front);

			//rdVector_Cross3(&right, &front, &up);
			//rdVector_Normalize3Acc(&right);
		}

		rdVector_Cross3(&front, &up, &right);
		rdVector_Normalize3Acc(&front);

		rdVector_Cross3(&right, &front, &up);
		rdVector_Normalize3Acc(&right);

		//right.x = stdMath_Lerp(pJoint->thing.lookOrientation.rvec.x, right.x, 0.5f);
		//right.y = stdMath_Lerp(pJoint->thing.lookOrientation.rvec.y, right.y, 0.5f);
		//right.z = stdMath_Lerp(pJoint->thing.lookOrientation.rvec.z, right.z, 0.5f);
		//rdVector_Normalize3Acc(&right);

		rdMatrix34 newOrient;
		newOrient.rvec = right;
		newOrient.lvec = front;
		newOrient.uvec = up;
		newOrient.scale = rdroid_zeroVector3;

		rdMatrix34 invOrient;
		rdMatrix_InvertOrtho34(&invOrient, &pJoint->things[0].lookOrientation);
		rdVector_Zero3(&invOrient.scale);

		rdMatrix34 localOrient;
		rdMatrix_Multiply34(&localOrient, &invOrient, &newOrient);

		rdVector3 localAngles;
		rdMatrix_ExtractAngles34(&localOrient, &localAngles);

		//sithPhysics_ApplyDrag(&localAngles, 1.0f, 0.0f, deltaSeconds);

		//rdVector_Scale3Acc(&localAngles, 2.0f);

		//rdVector_Add3Acc(&pJoint->thing.physicsParams.angVel, &localAngles);

		//sithPhysics_ThingTick(&pJoint->thing, deltaSeconds);
		pJoint->things[0].lookOrientation.rvec = right;
		pJoint->things[0].lookOrientation.uvec = up;
		pJoint->things[0].lookOrientation.lvec = front;
	}
}

void sithPuppet_ApplyIterativeCorrections(sithSector* pSector, sithThing* pThing, float deltaSeconds)
{
	//sithPuppet_ClearVelocities(pThing);
	
	int iterations = (pThing->isVisible + 1) == bShowInvisibleThings ? 10 : 1;
	//for (int i = 0; i < iterations; ++i)
	//{
	//	uint64_t jointBits = pThing->animclass->physicsJointBits;
	//	while (jointBits != 0)
	//	{
	//		int jointIdx = stdMath_FindLSB64(jointBits);
	//		jointBits ^= 1ull << jointIdx;
	//
	//		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
	//
	//		//int nodeIdx = pThing->animclass->bodypart[jointIdx].nodeIdx;
	//		//rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[nodeIdx];
	//
	//		//if(pNode->parent)
	//		{
	//			int childIdx = sithPuppet_jointChild[jointIdx];
	//			int parentIdx = sithPuppet_jointParent[jointIdx];//pThing->animclass->jointToBodypart[pNode->parent->idx];
	//			if(parentIdx >= 0 && (pThing->animclass->jointBits & (1ull << parentIdx)))
	//			{
	//				sithPuppet_ApplyDistanceConstraints(pThing, jointIdx, parentIdx, deltaSeconds);
	//
	//				sithPuppet_ApplyConeConstraint(pThing, jointIdx, parentIdx, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f, -10.0f, deltaSeconds);
	//			}
	//			else if (childIdx >= 0 && (pThing->animclass->jointBits & (1ull << childIdx)))
	//			{
	//				sithPuppet_ApplyLookConstraint(pThing, jointIdx, childIdx, 1, deltaSeconds);
	//			}
	//		}
	//	}
	//}







	//for (int i = 0; i < iterations; ++i)
	//{
	//	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
	//	{
	//		sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, deltaSeconds);
	//		sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, deltaSeconds);
	//	}
	//	else
	//	{
	//		sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, deltaSeconds);
	//	}
	//
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, deltaSeconds);
	//
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, deltaSeconds);
	//
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, deltaSeconds);
	//	sithPuppet_ApplyDistanceConstraints(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, deltaSeconds);
	//
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, 15.0f, -10.0f, 5.0f, -5.0f, 10.0f, -10.0f, deltaSeconds);
	//	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
	//	{
	//		sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, 5.0f, -5.0f, 5.0f, -5.0f, 5.0f, -5.0f, deltaSeconds);
	//		sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 5.0f, -40.0f, 5.0f, -5.0f, 35.0f, -35.0f, deltaSeconds);
	//	}
	//	else
	//	{
	//		sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_TORSO, 5.0f, -40.0f, 5.0f, -5.0f, 35.0f, -35.0f, deltaSeconds);
	//	}
	//
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, 60.0f, -5.0f, 5.0f, -5.0f, 5.0f, -5.0f, deltaSeconds);
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, 60.0f, -5.0f, 5.0f, -5.0f, 5.0f, -5.0f, deltaSeconds);
	//
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, 0.0f, -100.0f, 5.0f, -5.0f, 10.0f, -10.0f, deltaSeconds);
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, 0.0f, -100.0f, 5.0f, -5.0f, 10.0f, -10.0f, deltaSeconds);
	//
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 90.0f, -35.0f, 10.0f, -10.0f, 0.0f, -90.0f, deltaSeconds);
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, 90.0f, -35.0f, 10.0f, -10.0f, 90.0f, 0.0f, deltaSeconds);
	//
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, 120.0f, 0.0f, 5.0f, -5.0f, 5.0f, -5.0f, deltaSeconds);
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, 120.0f, 0.0f, 5.0f, -5.0f, 5.0f, -5.0f, deltaSeconds);
	//	
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, 5.0f, -5.0f, 25.0f, -25.0f, 35.0f, -5.0f, deltaSeconds);
	//	sithPuppet_ApplyConeConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, 5.0f, -5.0f, 25.0f, -25.0f, 5.0f, -35.0f, deltaSeconds);
	//
	//
	//	sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_HIP, JOINTTYPE_TORSO, 0, deltaSeconds);
	//	if (pThing->animclass->jointBits & (1ull << JOINTTYPE_NECK))
	//		sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, 1, deltaSeconds);
	//	else
	//		sithPuppet_ApplyLookConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_HEAD, 0, deltaSeconds);
	//
	//	sithPuppet_RunPhysics(pThing, deltaSeconds);
	//}

	// do fewer iterations if we're not directly visible
	for (int i = 0; i < iterations; ++i)
	{
		sithPuppet_ApplyConstraints(pThing, deltaSeconds);


		uint64_t jointBits = pThing->puppet->physics->constrainedJoints;
		while (jointBits != 0)
		{
			int jointIdx = stdMath_FindLSB64(jointBits);
			jointBits ^= 1ull << jointIdx;
		
			sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
			for (int j = 0; j < pJoint->numThings; ++j)
			{
				if (pJoint->things[j].nextPosWeight)
				{
					rdVector_InvScale3Acc(&pJoint->things[j].nextPosAcc, pJoint->things[j].nextPosWeight);
		
					pJoint->things[j].position = pJoint->things[j].nextPosAcc;

					//rdVector3 delta;
					//rdVector_Sub3(&delta, &pJoint->things[j].nextPosAcc, &pJoint->things[j].position);
					//float dist = rdVector_Normalize3Acc(&delta);
					//if (dist > 0.0)
					//{
					//	// I really hate having to do this update
					//	sithCollision_UpdateThingCollision(&pJoint->things[j], &delta, dist, 0);
					//}
				}
				pJoint->things[j].nextPosWeight = 0.0f;
				pJoint->things[j].nextPosAcc = rdroid_zeroVector3;
			}
		}
	}


	uint64_t jointBits = pThing->puppet->physics->constrainedJoints;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		for (int j = 0; j < pJoint->numThings; ++j)
		{
			rdVector3 delta;
			rdVector_Sub3(&delta, &pJoint->things[j].position, &pJoint->things[j].physicsParams.lastPos);
			//float dist = rdVector_Len3(&delta);
			float dist = rdVector_Normalize3Acc(&delta);
			if (dist > 0.0)
			{
				pJoint->things[j].position = pJoint->things[j].physicsParams.lastPos;
				//rdVector_Add3Acc(&pJoint->things[j].physicsParams.vel, &delta);
				// I really hate having to do this update
				sithCollision_UpdateThingCollision(&pJoint->things[j], &delta, dist, 0);
			}
		}
	}


	sithPuppet_UpdatePhysicsJointOrientations(pThing, deltaSeconds);
}
#endif

void sithPuppet_UpdatePhysicsAnim(sithThing* thing, float deltaSeconds)
{
	sithPuppet_UpdateJoints(thing, deltaSeconds);
#ifndef RIGID_BODY
	sithPuppet_ApplyIterativeCorrections(thing->sector, thing, deltaSeconds);
#endif
	sithPuppet_BuildJointMatrices(thing);

	// pin the thing to the root joint
	int rootJoint = thing->animclass->root < 0 ? JOINTTYPE_HIP : thing->animclass->root;
	sithPuppetJoint* pJoint = &thing->puppet->physics->joints[rootJoint];
#ifdef RIGID_BODY
	rdVector_Copy3(&thing->position, &pJoint->thing.position);
	sithThing_MoveToSector(thing, pJoint->thing.sector, 0);
#else
	rdVector_Copy3(&thing->position, &pJoint->things[0].position);
	sithThing_MoveToSector(thing, pJoint->things[0].sector, 0);
#endif
}

#endif

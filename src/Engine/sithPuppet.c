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
	{ JOINTTYPE_HEAD,      JOINTTYPE_NECK, -1 },
	{ JOINTTYPE_HEAD,     JOINTTYPE_TORSO, -1 },
	{ JOINTTYPE_HEAD, JOINTTYPE_RSHOULDER, -1 },
	{ JOINTTYPE_HEAD, JOINTTYPE_LSHOULDER, -1 },
	{ JOINTTYPE_HEAD,       JOINTTYPE_HIP, -1 },

	// neck constraints
	{ JOINTTYPE_NECK,     JOINTTYPE_TORSO, -1 },
	{ JOINTTYPE_NECK, JOINTTYPE_RSHOULDER, -1 },
	{ JOINTTYPE_NECK, JOINTTYPE_LSHOULDER, -1 },
	{ JOINTTYPE_NECK,       JOINTTYPE_HIP, -1 },

	// torso constraints
	{ JOINTTYPE_TORSO,       JOINTTYPE_HIP,   -1 },
	{ JOINTTYPE_TORSO, JOINTTYPE_RSHOULDER,   -1 },
	{ JOINTTYPE_TORSO, JOINTTYPE_LSHOULDER,   -1 },
	{ JOINTTYPE_TORSO,    JOINTTYPE_RTHIGH,   -1 },
	{ JOINTTYPE_TORSO,    JOINTTYPE_LTHIGH,   -1 },
	{ JOINTTYPE_TORSO,     JOINTTYPE_LFOOT, 0.8f }, // prevent torso getting too close to foot
	{ JOINTTYPE_TORSO,     JOINTTYPE_RFOOT, 0.8f }, // prevent torso getting too close to foot
	{ JOINTTYPE_TORSO,     JOINTTYPE_LCALF, 0.9f }, // prevent calf getting too close to torso
	{ JOINTTYPE_TORSO,     JOINTTYPE_RCALF, 0.9f }, // prevent calf getting too close to torso

	// hip constraints
	{ JOINTTYPE_HIP, JOINTTYPE_RSHOULDER,   -1 },
	{ JOINTTYPE_HIP, JOINTTYPE_LSHOULDER,   -1 },
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
	{ JOINTTYPE_RSHOULDER,   JOINTTYPE_RTHIGH,    -1 },
	{ JOINTTYPE_RSHOULDER,   JOINTTYPE_LTHIGH,    -1 },
	{ JOINTTYPE_RFOREARM,     JOINTTYPE_LHAND, 0.75f }, // prevent forearm getting too close to other hand

	// left arm
	{ JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER,    -1 },
	{ JOINTTYPE_LHAND,     JOINTTYPE_LFOREARM,    -1 },
	{ JOINTTYPE_LSHOULDER,   JOINTTYPE_RTHIGH,    -1 },
	{ JOINTTYPE_LSHOULDER,   JOINTTYPE_LTHIGH,    -1 },
	{ JOINTTYPE_LFOREARM,     JOINTTYPE_RHAND, 0.75f }, // prevent forearm getting too close to other hand

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
	{ JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, -1 },
	{ JOINTTYPE_RFOOT,  JOINTTYPE_RCALF, -1 },

	// left arm
	{ JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, -1 },
	{ JOINTTYPE_LFOOT,  JOINTTYPE_LCALF, -1 },

	// weapon
	{ JOINTTYPE_PRIMARYWEAP,   JOINTTYPE_RHAND, -1 },
	{ JOINTTYPE_SECONDARYWEAP, JOINTTYPE_LHAND, -1 },
};

static const sithPuppetJointFrame sithPuppet_jointFrames[] =
{
	{ JOINTTYPE_NECK,      JOINTTYPE_LSHOULDER, JOINTTYPE_RSHOULDER,              -1, 1, 70.0f, 20.0f },	// JOINTTYPE_HEAD
	{ JOINTTYPE_HEAD,      JOINTTYPE_LSHOULDER, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, 0, 60.0f,  0.0f },	// JOINTTYPE_NECK
	{ JOINTTYPE_NECK,      JOINTTYPE_LSHOULDER, JOINTTYPE_RSHOULDER,   JOINTTYPE_HIP, 0, 20.0f, 20.0f },	// JOINTTYPE_TORSO
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_PRIMARYWEAP
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_SECONDARYWEAP
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_PRIMARYWEAPJOINT
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_SECONDARYWEAPJOINT
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_TURRETPITCH
	{ -1,                                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_TURRETYAW
	{ JOINTTYPE_TORSO,        JOINTTYPE_LTHIGH,    JOINTTYPE_RTHIGH,              -1, 0, 20.0f,  0.0f },	// JOINTTYPE_HIP
	{ JOINTTYPE_RFOREARM,                   -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_RSHOULDER
	{ JOINTTYPE_LFOREARM,                   -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_LSHOULDER
	{ JOINTTYPE_RSHOULDER,                  -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_RFOREARM
	{ JOINTTYPE_LSHOULDER,                  -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_LFOREARM
	{ JOINTTYPE_RFOREARM,                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_RHAND
	{ JOINTTYPE_LFOREARM,                   -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_LHAND
	{ JOINTTYPE_RCALF,                      -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_RTHIGH
	{ JOINTTYPE_LCALF,                      -1,                  -1,              -1, 1,  0.0f,  0.0f },	// JOINTTYPE_LTHIGH
	{ JOINTTYPE_RTHIGH,                     -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_RCALF
	{ JOINTTYPE_LTHIGH,                     -1,                  -1,              -1, 0,  0.0f,  0.0f },	// JOINTTYPE_LCALF
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
					sithPuppet_StartPhysics(thing, &thing->physicsParams.vel);
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


void sithThing_AddConstraint(sithThing* pThing, sithThing* pConstraintThing, const rdVector3* pAnchor)
{
	if(pThing->numBallSocketJoints >= 4)
		return;

	sithThingBallSocketJoint* pBallSocket = &pThing->ballSocketJoints[pThing->numBallSocketJoints++];
	pBallSocket->thing = pConstraintThing;
	pBallSocket->anchor = *pAnchor;
}

void sithPuppet_AddConstraint(sithThing* pThing, int jointA, int jointB, float minDistance)
{
	int hasJointA = pThing->animclass->jointBits & (1ull << jointA);
	int hasJointB = pThing->animclass->jointBits & (1ull << jointB);
	if (!hasJointA || !hasJointB)
		return;
	
	sithBodyPart* pBodyPartA = &pThing->animclass->bodypart[jointA];
	sithBodyPart* pBodyPartB = &pThing->animclass->bodypart[jointB];
	
	// todo: precompute the constraint distance
	rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPartA->nodeIdx].scale;
	rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPartB->nodeIdx].scale;
	float distance = rdVector_Dist3(pBasePosB, pBasePosA) * 0.95f;
	
	sithPuppetJoint* pJointA = &pThing->puppet->physics->joints[jointA];
	sithPuppetJoint* pJointB = &pThing->puppet->physics->joints[jointB];

	rdVector3 anchor;
	rdVector_Sub3(&anchor, &pBasePosB, &pBasePosA);
	
	sithThing_AddConstraint(&pJointA->thing, &pJointB->thing, &anchor);
}

void sithPuppet_StartPhysics(sithThing* pThing, rdVector3* pInitialVel)
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
	rdVector_Scale3(&thingVel, pInitialVel, sithTime_deltaSeconds);

	uint64_t jointBits = pThing->animclass->jointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
		rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];
		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];

		// clear next position accumulator
		rdVector_Zero3(&pJoint->nextPosAcc);
		pJoint->nextPosWeight = 0.0f;

		// the joint contains a thing for collision and physics
		// acts sorta like a rigid body
		sithThing_DoesRdThingInit(&pJoint->thing);
		pJoint->thing.rdthing.curGeoMode = 0;
		pJoint->thing.rdthing.desiredGeoMode = 0;
		pJoint->thing.thingflags = SITH_TF_INVISIBLE;
		pJoint->thing.thingIdx = pThing->thingIdx | (jointIdx << 16);
		extern int sithThing_bInitted2;
		pJoint->thing.signature = sithThing_bInitted2++;
		pJoint->thing.thing_id = -1;
		pJoint->thing.type = SITH_THING_CORPSE;
		pJoint->thing.moveType = SITH_MT_PHYSICS;
		pJoint->thing.prev_thing = pThing;
		pJoint->thing.child_signature = pThing->signature;

		// setup physics params
		_memcpy(&pJoint->thing.physicsParams, &pThing->physicsParams, sizeof(sithThingPhysParams));
		pJoint->thing.physicsParams.mass *= pBodyPart->mass;
		pJoint->thing.physicsParams.buoyancy *= pBodyPart->buoyancy;
		pJoint->thing.physicsParams.height = 0;

		pJoint->thing.physicsParams.physflags = SITH_PF_FEELBLASTFORCE | SITH_PF_FLOORSTICK;// | SITH_PF_ANGTHRUST;

		if (pThing->physicsParams.physflags & SITH_PF_USEGRAVITY)		
			pJoint->thing.physicsParams.physflags |= SITH_PF_USEGRAVITY;

		if (pThing->physicsParams.physflags & SITH_PF_PARTIALGRAVITY)
			pJoint->thing.physicsParams.physflags |= SITH_PF_PARTIALGRAVITY;

		if (pThing->physicsParams.physflags & SITH_PF_NOTHRUST)
			pJoint->thing.physicsParams.physflags |= SITH_PF_NOTHRUST;

		if (pBodyPart->flags & JOINTFLAGS_BOUNCE)
			pJoint->thing.physicsParams.physflags |= SITH_PF_SURFACEBOUNCE;

		if (pBodyPart->flags & JOINTFLAGS_PHYSICS)
			pJoint->thing.collide = SITH_COLLIDE_SPHERE;
		else
			pJoint->thing.collide = SITH_COLLIDE_NONE;

		if (pNode->meshIdx != -1)
			pJoint->thing.collideSize = pJoint->thing.moveSize = pThing->rdthing.model3->geosets[0].meshes[pNode->meshIdx].radius * 0.25f;
		else
			pJoint->thing.collideSize = pJoint->thing.moveSize = 0.02f;

		pJoint->thing.collideSize = 0.01f;

		// initialize the position velocity using the animation frames
		rdVector_Copy3(&pJoint->thing.position, &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx].scale);
		
		rdVector3 lastPos;
		rdVector_Copy3(&lastPos, &pThing->rdthing.paHierarchyNodeMatricesPrev[pBodyPart->nodeIdx].scale);

		rdVector_Sub3(&pJoint->thing.physicsParams.vel, &pJoint->thing.position, &lastPos);
		rdVector_Scale3Acc(&pJoint->thing.physicsParams.vel, 1.0f / sithTime_deltaSeconds);

		//rdVector_Zero3(&pJoint->thing.physicsParams.vel);

	//	rdVector_Add3Acc(&pJoint->thing.physicsParams.vel, &thingVel);

		// orient to the joint matrix
		rdMatrix_Copy34(&pJoint->thing.lookOrientation, &pThing->rdthing.hierarchyNodeMatrices[pBodyPart->nodeIdx]);
		rdVector_Zero3(&pJoint->thing.lookOrientation.scale);

		// enter the things sector to start physics
		sithThing_EnterSector(&pJoint->thing, pThing->sector, 1, 0);
	}

	// attach things
	//jointBits = pThing->animclass->jointBits;
	//while (jointBits != 0)
	//{
	//	int jointIdx = stdMath_FindLSB64(jointBits);
	//	jointBits ^= 1ull << jointIdx;
	//
	//	sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
	//	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];
	//	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
	//
	//	int parentIdx = sithPuppet_jointParent[jointIdx];
	//	if (parentIdx >= 0)
	//	{
	//		sithPuppetJoint* pParentJoint = &pThing->puppet->physics->joints[parentIdx];
	//		sithBodyPart* pParentBodyPart = &pThing->animclass->bodypart[parentIdx];
	//		
	//		rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPart->nodeIdx].scale;
	//		rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pParentBodyPart->nodeIdx].scale;
	//		float distance = rdVector_Dist3(pBasePosB, pBasePosA) * 0.95f;
	//
	//		pJoint->thing.parentConstraintDist = distance;
	//		pJoint->thing.parentConstraint = pThing;
	//		//pJoint->thing.child_signature = pThing->signature;
	//	}
	//
	//
	////	if (parentIdx >= 0)
	////	{
	////		sithPuppetJoint* pParentJoint = &pThing->puppet->physics->joints[parentIdx];
	////		sithBodyPart* pParentBodyPart = &pThing->animclass->bodypart[parentIdx];
	////
	////		// todo: precompute the constraint distance
	////		rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPart->nodeIdx].scale;
	////		rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pParentBodyPart->nodeIdx].scale;
	////		float distance = rdVector_Dist3(pBasePosB, pBasePosA) * 0.95f;
	////
	////		rdVector3* axis1 = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx].posRotMatrix.lvec;
	////		rdVector3* axis2 = &pThing->rdthing.model3->hierarchyNodes[pParentBodyPart->nodeIdx].posRotMatrix.lvec;
	////
	////		sithThing_AddConstraint(&pParentJoint->thing, &pJoint->thing, distance, -1.0f, axis1, axis2);
	////
	////	//	pJoint->thing.firstChildThing = &pParentJoint->thing.firstChildThing;
	////	//	pParentJoint->thing.firstChildThing = &pJoint->thing;
	////		//sithThing_AttachThing(&pJoint->thing, &pParentJoint->thing);
	////	}
	////	else
	////	{
	////	//	pJoint->thing.firstChildThing = NULL;
	////		//sithThing_AttachThing(&pJoint->thing, pThing);
	////	}
	////	//pJoint->thing.attach_flags |= SITH_ATTACH_NO_MOVE;
	//}


	//for (int i = 0; i < ARRAYSIZE(sithPuppet_constraints); ++i)
	//{
	//	sithPuppetConstraint* pConstraint = &sithPuppet_constraints[i];
	//
	//	int hasJointA = pThing->animclass->jointBits & (1ull << pConstraint->jointA);
	//	int hasJointB = pThing->animclass->jointBits & (1ull << pConstraint->jointB);
	//	if (!hasJointA || !hasJointB)
	//		continue;
	//
	//	sithBodyPart* pBodyPartA = &pThing->animclass->bodypart[pConstraint->jointA];
	//	sithBodyPart* pBodyPartB = &pThing->animclass->bodypart[pConstraint->jointB];
	//
	//	// todo: precompute the constraint distance
	//	rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPartA->nodeIdx].scale;
	//	rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPartB->nodeIdx].scale;
	//	float distance = rdVector_Dist3(pBasePosB, pBasePosA) * 0.95f;
	//
	//	sithPuppetJoint* pJointA = &pThing->puppet->physics->joints[pConstraint->jointA];
	//	sithPuppetJoint* pJointB = &pThing->puppet->physics->joints[pConstraint->jointB];
	//
	//	sithThing_AddConstraint(&pJointA->thing, &pJointB->thing, distance, distance * pConstraint->minDist);
	//}

//	sithPuppet_AddConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_NECK, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_NECK, JOINTTYPE_TORSO, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_TORSO, JOINTTYPE_HIP, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_TORSO, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_TORSO, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_RSHOULDER, JOINTTYPE_HIP, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_LSHOULDER, JOINTTYPE_HIP, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_RFOREARM, JOINTTYPE_RSHOULDER, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_LFOREARM, JOINTTYPE_LSHOULDER, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_RHAND, JOINTTYPE_RFOREARM, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_LHAND, JOINTTYPE_LFOREARM, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_TORSO, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_TORSO, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_RTHIGH, JOINTTYPE_HIP, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_LTHIGH, JOINTTYPE_HIP, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_RCALF, JOINTTYPE_RTHIGH, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_LCALF, JOINTTYPE_LTHIGH, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_RFOOT, JOINTTYPE_RCALF, -1);
//	sithPuppet_AddConstraint(pThing, JOINTTYPE_LFOOT, JOINTTYPE_LCALF, -1);

	//sithPuppet_AddConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_HIP, 0.7f);
	//sithPuppet_AddConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_LTHIGH, 0.9f);
	//sithPuppet_AddConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_RTHIGH, 0.9f);
	//sithPuppet_AddConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_LFOOT, 1.0f);
	//sithPuppet_AddConstraint(pThing, JOINTTYPE_HEAD, JOINTTYPE_RFOOT, 1.0f);
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

			//sithThing_DetachThing(&pJoint->thing);

			sithThing_LeaveSector(&pJoint->thing);

			// probably not needed as we're not using any rendering but just in case...
			rdThing_FreeEntry(&pJoint->thing.rdthing);
		}
		pSithHS->free(pThing->puppet->physics);
		pThing->puppet->physics = 0;
	}
}

void sithPuppet_ApplyConstraints(sithThing* pThing)
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
	//	rdHierarchyNode* pNode = &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];
	//	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
	//
	//	int parentIdx = sithPuppet_jointParent[jointIdx];
	//	if (parentIdx >= 0)
	//	{
	//		sithPuppetJoint* pParentJoint = &pThing->puppet->physics->joints[parentIdx];
	//		sithBodyPart* pParentBodyPart = &pThing->animclass->bodypart[parentIdx];
	//
	//		rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pParentBodyPart->nodeIdx].scale;
	//		rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPart->nodeIdx].scale;
	//
	//		rdVector3 worldAnchor;
	//		rdVector_Zero3(&worldAnchor);
	//		
	//		rdHierarchyNode* node = pNode;
	//		while (node)
	//		{
	//			rdVector_Add3Acc(&worldAnchor, &node->pos);
	//			node = node->parent;
	//		}
	//
	//		//rdVector_Sub3(&worldAnchor, pBasePosB, pBasePosA);
	//		//rdVector_Copy3(&worldAnchor, &pThing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx].pivot);
	//		rdVector_Zero3(&pParentJoint->thing.lookOrientation.scale);
	//		rdMatrix_TransformPoint34Acc(&worldAnchor, &pParentJoint->thing.lookOrientation);
	//
	//		rdVector_Add3Acc(&worldAnchor, &pParentJoint->thing.position);
	//
	//		rdVector3 distanceA;
	//		rdVector_Sub3(&distanceA, &worldAnchor, &pParentJoint->thing.position);
	//		float lengthA = rdVector_Normalize3Acc(&distanceA);
	//
	//		rdVector3 distanceB;
	//		rdVector_Sub3(&distanceB, &worldAnchor, &pJoint->thing.position);
	//		float lengthB = rdVector_Normalize3Acc(&distanceB);
	//		
	//		if (lengthA > 0.0f || lengthB > 0.0f)
	//		{
	//			float totalInvMass = 2.0;//joint->bodyA->invMass + joint->bodyB->invMass;
	//			if (totalInvMass != 0)
	//			{
	//				float factorA = 0.5f;//0.0005f;//joint->bodyA->invMass / totalInvMass;
	//				float factorB = 0.5f;//0.5f;//joint->bodyB->invMass / totalInvMass;
	//
	//				//sithCollision_UpdateThingCollision(&pParentJoint->thing, &distanceA, lengthA * factorA, 0);
	//
	//				//rdVector_Neg3Acc(&distanceB);
	//				//sithCollision_UpdateThingCollision(&pJoint->thing, &distanceB, lengthB * factorB, 0);
	//
	//				rdVector3 correctionA, correctionB;
	//				rdVector_Scale3(&correctionA, &distanceA, factorA);
	//				rdVector_Scale3(&correctionB, &distanceB, factorB);
	//				
	//				rdVector3 deltaA;
	//				rdVector_Scale3(&deltaA, &correctionA, (double)lengthA * factorA);
	//
	//				rdVector3 offsetA;
	//				rdVector_Sub3(&offsetA, &pParentJoint->thing.position, &deltaA);
	//				rdVector_Add3Acc(&pParentJoint->nextPosAcc, &offsetA);
	//				pParentJoint->nextPosWeight++;
	//
	//				pThing->puppet->physics->constrainedJoints |= 1ull << parentIdx;
	//
	//				rdVector3 deltaB;
	//				rdVector_Scale3(&deltaB, &correctionB, (double)lengthB * factorB);
	//
	//				rdVector3 offsetB;
	//				rdVector_Add3(&offsetB, &pJoint->thing.position, &deltaB);
	//				rdVector_Add3Acc(&pJoint->nextPosAcc, &offsetB);
	//				pJoint->nextPosWeight++;					
	//				
	//				pThing->puppet->physics->constrainedJoints |= 1ull << jointIdx;
	//			}
	//		}
	//	}
	//}

	for (int i = 0; i < ARRAYSIZE(sithPuppet_constraints); ++i)
	{
		sithPuppetConstraint* pConstraint = &sithPuppet_constraints[i];
	
		int hasJointA = pThing->animclass->jointBits & (1ull << pConstraint->jointA);
		int hasJointB = pThing->animclass->jointBits & (1ull << pConstraint->jointB);
		if(!hasJointA || !hasJointB)
			continue;
	
		sithBodyPart* pBodyPartA = &pThing->animclass->bodypart[pConstraint->jointA];
		sithBodyPart* pBodyPartB = &pThing->animclass->bodypart[pConstraint->jointB];
		
		// todo: precompute the constraint distance
		rdVector3* pBasePosA = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPartA->nodeIdx].scale;
		rdVector3* pBasePosB = &pThing->rdthing.model3->paBasePoseMatrices[pBodyPartB->nodeIdx].scale;
		float distance = rdVector_Dist3(pBasePosB, pBasePosA) * 0.95f;
	
		sithPuppetJoint* pJointA = &pThing->puppet->physics->joints[pConstraint->jointA];
		sithPuppetJoint* pJointB = &pThing->puppet->physics->joints[pConstraint->jointB];
	
		// calculate the distance between the 2 joints
		rdVector3 delta;
		rdVector_Sub3(&delta, &pJointB->thing.position, &pJointA->thing.position);
		float deltaLen = rdVector_Normalize3Acc(&delta);
		if (pConstraint->minDist >= 0 && deltaLen > distance * pConstraint->minDist)
			continue;
	
		// if we're already at the right distance then don't bother constraining
		if (fabs(deltaLen - distance) < 1e-5)
			continue;
	
		// compute the difference in mass, used to adjust the positions
		float invMassA = 1.0f / (pThing->animclass->bodypart[pConstraint->jointA].mass);
		float invMassB = 1.0f / (pThing->animclass->bodypart[pConstraint->jointB].mass);
		float diff = (deltaLen - distance) / ((invMassA + invMassB));
	
		if (diff < 0.0)
		{
			rdVector_Neg3Acc(&delta);
			diff = -diff;
		}
	
		if (!(pBodyPartA->flags & JOINTFLAGS_PINNED))
		{
			rdVector3 deltaA;
			rdVector_Scale3(&deltaA, &delta, (double)diff * invMassA);
			
			rdVector3 offsetA;
			rdVector_Add3(&offsetA, &pJointA->thing.position, &deltaA);
			rdVector_Add3Acc(&pJointA->nextPosAcc, &offsetA);
			pJointA->nextPosWeight++;
	
			//sithCollision_UpdateThingCollision(&pJointA->thing, &delta, diff * invMassA, SITH_RAYCAST_IGNORE_THINGS | RAYCAST_4);//SITH_RAYCAST_IGNORE_ADJOINS | SITH_RAYCAST_IGNORE_THINGS | RAYCAST_4);
	
			pThing->puppet->physics->constrainedJoints |= 1ull << pConstraint->jointA;
		}
		
		if (!(pBodyPartB->flags & JOINTFLAGS_PINNED))
		{
			rdVector3 deltaB;
			rdVector_Scale3(&deltaB, &delta, (double)diff * invMassB);
			
			rdVector3 offsetB;
			rdVector_Sub3(&offsetB, &pJointB->thing.position, &deltaB);
			rdVector_Add3Acc(&pJointB->nextPosAcc, &offsetB);
			pJointB->nextPosWeight++;
	
			//rdVector_Neg3Acc(&delta);
			//
			//sithCollision_UpdateThingCollision(&pJointB->thing, &delta, diff * invMassB, SITH_RAYCAST_IGNORE_THINGS | RAYCAST_4);//SITH_RAYCAST_IGNORE_ADJOINS | SITH_RAYCAST_IGNORE_THINGS | RAYCAST_4);
	
			pThing->puppet->physics->constrainedJoints |= 1ull << pConstraint->jointB;
		}
	}
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

static void sithPuppet_BuildJointMatrices(sithThing* thing)
{
	rdVector_Copy3(&thing->lookOrientation.scale, &thing->position);
	

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

	//	sithPuppetJointFrame* pFrame = &sithPuppet_jointFrames[jointIdx];
	//	if(pFrame->targetJoint < 0)
	//	{
	//		rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pJoint->thing.position);
	//		thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->thing.lookOrientation;
	//		continue;
	//	}
	//	
	//	sithPuppetJoint* pTargetJoint = &thing->puppet->physics->joints[pFrame->targetJoint];
	//
	//	rdMatrix34 baseMat;
	//	rdMatrix_Multiply34(&baseMat, &thing->rdthing.model3->paBasePoseMatrices[pBodyPart->nodeIdx], &thing->lookOrientation);
	//
	//	rdMatrix34* pMat = &pJoint->thing.lookOrientation;
	//
	//	// calculate the up vector
	//	rdVector_Sub3(&pMat->uvec, &pTargetJoint->thing.position, &pJoint->thing.position);
	//	rdVector_Normalize3Acc(&pMat->uvec);
	//	if(pFrame->reversed)
	//		rdVector_Neg3Acc(&pMat->uvec);
	//
	//	// calculate the adjusted right vector
	//	rdVector_Cross3(&pMat->rvec, &baseMat.lvec, &pMat->uvec);
	//	rdVector_Normalize3Acc(&pMat->rvec);
	//
	//	rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);
	//
	//	rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pJoint->thing.position);
	//
	//	thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->thing.lookOrientation;

		// to model space
	//	rdVector3 localPos;
	//	rdMatrix_TransformPoint34(&localPos, &pJoint->thing.position, &invMat);
	//
	//	//int parentIdx = sithPuppet_jointParent[jointIdx];
	//	//if (parentIdx >= 0)
	//	{
	//		//sithPuppetJoint* pParentJoint = &thing->puppet->physics->joints[parentIdx];
	//		//sithBodyPart* pParentBodyPart = &thing->animclass->bodypart[parentIdx];
	//		
	//		// to parent local frame
	//		rdMatrix34 invParent;
	//		rdMatrix_InvertOrtho34(&invParent, &thing->rdthing.model3->paBasePoseMatrices[thing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx].parent->idx]);
	//
	//		rdMatrix_TransformPoint34Acc(&localPos, &invParent);
	//	}
	//		
	//	rdMatrix34* pMat = &pJoint->thing.lookOrientation;
	//
	//	rdVector_Normalize3(&pMat->uvec, &localPos);
	//
	//	// calculate the adjusted right vector
	//	rdVector_Cross3(&pMat->rvec, &pMat->lvec, &pMat->uvec);
	//	rdVector_Normalize3Acc(&pMat->rvec);
	//
	//	rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);
	//		
	//	rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &localPos);// &pJoint->thing.position);

	//	rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pJoint->thing.position);
	//thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->thing.lookOrientation;
	//continue;





		rdHierarchyNode* pNode = &thing->rdthing.model3->hierarchyNodes[pBodyPart->nodeIdx];

		sithPuppetJointFrame* pFrame = &sithPuppet_jointFrames[jointIdx];
		if(pFrame->targetJoint < 0)
		{
			rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pJoint->thing.position);
			thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->thing.lookOrientation;
			continue;
		}
		
		sithPuppetJoint* pTargetJoint = &thing->puppet->physics->joints[pFrame->targetJoint];
		rdMatrix34* pMat = &pJoint->thing.lookOrientation;

		// calculate the up vector
		rdVector_Sub3(&pMat->uvec, &pTargetJoint->thing.position, &pJoint->thing.position);
		rdVector_Normalize3Acc(&pMat->uvec);
		if(pFrame->reversed)
			rdVector_Neg3Acc(&pMat->uvec);

		// if we have a pitch joint, we have a pitch constraint
		if (pFrame->pitchJoint >= 0) // causes skew atm
		{
			sithPuppetJoint* pPitchJoint = &thing->puppet->physics->joints[pFrame->pitchJoint];
			
			rdVector3 referenceUp;
			rdVector_Sub3(&referenceUp, &pTargetJoint->thing.position, &pPitchJoint->thing.position);
			rdVector_Normalize3Acc(&referenceUp);
			if (pFrame->reversed)
				rdVector_Neg3Acc(&referenceUp);
			sithPuppet_ConstrainAxis(&pMat->uvec, &referenceUp, pFrame->maxPitch);
		}

		// calculate the adjusted right vector
		rdVector_Cross3(&pMat->rvec, &pMat->lvec, &pMat->uvec);
		rdVector_Normalize3Acc(&pMat->rvec);

		// if we have a left and right joint, we have yaw constraint
		if (pFrame->leftJoint >= 0 && pFrame->rightJoint >= 0)
		{
			sithPuppetJoint* pLeftJoint = &thing->puppet->physics->joints[pFrame->leftJoint];
			sithPuppetJoint* pRightJoint = &thing->puppet->physics->joints[pFrame->rightJoint];
		
			rdVector3 referenceRight;
			rdVector_Sub3(&referenceRight, &pRightJoint->thing.position, &pLeftJoint->thing.position);
			rdVector_Normalize3Acc(&referenceRight);
			sithPuppet_ConstrainAxis(&pMat->rvec, &referenceRight, pFrame->maxYaw);
		}
		
		rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);

		// at this point we may have violated orthogonality, so recompute some stuff
		rdVector_Cross3(&pMat->uvec, &pMat->rvec, &pMat->lvec);
		rdVector_Normalize3Acc(&pMat->uvec);

		rdVector_Cross3(&pMat->rvec, &pMat->lvec, &pMat->uvec);
		rdVector_Normalize3Acc(&pMat->rvec);
		
		rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);

		rdVector_Copy3(&pJoint->thing.lookOrientation.scale, &pJoint->thing.position);

		thing->rdthing.paHiearchyNodeMatrixOverrides[pBodyPart->nodeIdx] = &pJoint->thing.lookOrientation;
	}
	
	rdVector_Zero3(&thing->lookOrientation.scale);
}

void sithPuppet_FixupPositions(sithSector* sector, sithThing* pThing, float deltaSeconds)
{
	while (pThing->puppet->physics->constrainedJoints != 0)
	{
		int jointIdx = stdMath_FindLSB64(pThing->puppet->physics->constrainedJoints);
		pThing->puppet->physics->constrainedJoints ^= 1ull << jointIdx;

		sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];

		// normalize the new position accumulator
		rdVector_InvScale3Acc(&pJoint->nextPosAcc, pJoint->nextPosWeight);

		rdVector3 vel;
		rdVector_Sub3(&vel, &pJoint->nextPosAcc, &pJoint->thing.position);
		rdVector_ClipPrecision3(&vel);
		if (!rdVector_IsZero3(&vel))
		{
			//rdVector_Add3Acc(&pJoint->thing.physicsParams.vel, &vel);
			//pJoint->thing.physicsParams.physflags |= SITH_PF_HAS_FORCE;
			
			rdVector3 v1;
			float arg4a = rdVector_Normalize3(&v1, &vel);
			sithCollision_UpdateThingCollision(&pJoint->thing, &v1, arg4a, 0);//400 | SITH_RAYCAST_IGNORE_THINGS | RAYCAST_4);
		}
		
		//rdVector_Copy3(&pJoint->thing.position, &pJoint->nextPosAcc);

		// if the sector is different than the root, teleport the mesh instead to avoid getting stuck
		//int rootJoint = pThing->animclass->root < 0 ? JOINTTYPE_TORSO : pThing->animclass->root;
		//sithPuppetJoint* pRootJoint = &pThing->puppet->physics->joints[rootJoint];
		//if (pRootJoint->thing.sector != pJoint->thing.sector)
		//{
		//	//rdVector_Copy3(&pJoint->thing.position, &pJoint->nextPosAcc);
		//	sithThing_MoveToSector(&pJoint->thing, pRootJoint->thing.sector, 0);
		//}

		rdVector_Zero3(&pJoint->nextPosAcc);
		pJoint->nextPosWeight = 0;
	}
}

void sithPuppet_ApplyIterativeCorrections(sithSector* pSector, sithThing* pThing, float deltaSeconds)
{
	// do fewer iterations if we're not directly visible
	int iterations = (pThing->isVisible + 1) == bShowInvisibleThings ? 5 : 1;
	for (int i = 0; i < iterations; ++i)
	{
		sithPuppet_ApplyConstraints(pThing);
		sithPuppet_FixupPositions(pSector, pThing, deltaSeconds);
	}
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

		// if the node is amputated or lower than the root joint, don't collide (but update the position for sector traversal)
		int collide = pJoint->thing.collide;
		if (pBodyPart->nodeIdx < pThing->rdthing.rootJoint || pThing->rdthing.amputatedJoints[pBodyPart->nodeIdx])
			pJoint->thing.collide = SITH_COLLIDE_NONE;

		//int rootJoint = pThing->animclass->root < 0 ? JOINTTYPE_TORSO : pThing->animclass->root;
		//sithPuppetJoint* pRootJoint = &pThing->puppet->physics->joints[rootJoint];
		//if (pRootJoint->thing.sector != pJoint->thing.sector)
		//{
		//	//rdVector_Copy3(&pJoint->thing.position, &pJoint->nextPosAcc);
		//	printf("moving joint %d to root sector\n", jointIdx);
		//	sithThing_MoveToSector(&pJoint->thing, pRootJoint->thing.sector, 0);
		//}

		// don't get the velocity get too out of hand or we start missing collisions with adjoins
		//rdVector_ClampRange3(&pJoint->thing.physicsParams.vel, 0.0f, 1.0f);

		//rdVector_Zero3(&pJoint->thing.physicsParams.angVel);
		rdVector_Zero3(&pJoint->thing.physicsParams.velocityMaybe);
		rdVector_Zero3(&pJoint->thing.physicsParams.addedVelocity);

		sithPhysics_ThingTick(&pJoint->thing, deltaSeconds);
		sithThing_TickPhysics(&pJoint->thing, deltaSeconds);
//		sithPhysics_FindFloor(&pJoint->thing, 0);
		
		//rdVector_Copy3(&pJoint->thing.field_268, &pJoint->thing.physicsParams.velocityMaybe);
		//rdVector3 v1;
		//float arg4a = rdVector_Normalize3(&v1, &pJoint->thing.physicsParams.velocityMaybe);
		//sithCollision_UpdateThingCollision(&pJoint->thing, &v1, arg4a, 0);

		//if (pRootJoint->thing.sector != pJoint->thing.sector)
		//{
		//	//rdVector_Copy3(&pJoint->thing.position, &pJoint->nextPosAcc);
		//	printf("moving joint %d to root sector\n", jointIdx);
		//	sithThing_MoveToSector(&pJoint->thing, pRootJoint->thing.sector, 0);
		//}

		// reset collision
		pJoint->thing.collide = collide;
	}
}
void sithPuppet_UpdateJoints2(sithThing* pThing, float deltaSeconds)
{
	uint64_t jointBits = pThing->animclass->physicsJointBits;
	while (jointBits != 0)
	{
		int jointIdx = stdMath_FindLSB64(jointBits);
		jointBits ^= 1ull << jointIdx;

		sithBodyPart* pBodyPart = &pThing->animclass->bodypart[jointIdx];
		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];

		// if the node is amputated or lower than the root joint, don't collide (but update the position for sector traversal)
		int collide = pJoint->thing.collide;
		if (pBodyPart->nodeIdx < pThing->rdthing.rootJoint || pThing->rdthing.amputatedJoints[pBodyPart->nodeIdx])
			pJoint->thing.collide = SITH_COLLIDE_NONE;

		sithThing_TickPhysics(&pJoint->thing, deltaSeconds);

		// reset collision
		pJoint->thing.collide = collide;
	}
}
void sithPuppet_UpdatePhysicsAnim(sithThing* thing, float deltaSeconds)
{
	//sithPuppet_BuildJointMatrices(thing);
	//return;

	// collision can fuck up attachment
	//uint64_t jointBits = thing->animclass->jointBits;
	//while (jointBits != 0)
	//{
	//	int jointIdx = stdMath_FindLSB64(jointBits);
	//	jointBits ^= 1ull << jointIdx;
	//
	//	sithPuppetJoint* pJoint = &thing->puppet->physics->joints[jointIdx];
	//
	//	int parentIdx = sithPuppet_jointParent[jointIdx];
	//	if (parentIdx >= 0)
	//	{
	//		sithPuppetJoint* pParentJoint = &thing->puppet->physics->joints[parentIdx];
	//		sithThing_AttachThing(&pJoint->thing, &pParentJoint->thing);
	//	}
	//	else
	//	{
	//		sithThing_AttachThing(&pJoint->thing, thing);
	//	}
	//	//pJoint->thing.attach_flags |= SITH_ATTACH_NO_MOVE;
	//}

	sithPuppet_UpdateJoints(thing, deltaSeconds);
	//sithPuppet_UpdateJoints2(thing, deltaSeconds);
	sithPuppet_ApplyIterativeCorrections(thing->sector, thing, deltaSeconds);
	sithPuppet_BuildJointMatrices(thing);

	// pin the thing to the root joint
	int rootJoint = thing->animclass->root < 0 ? JOINTTYPE_TORSO : thing->animclass->root;
	sithPuppetJoint* pJoint = &thing->puppet->physics->joints[rootJoint];
	rdVector_Copy3(&thing->position, &pJoint->thing.position);
	sithThing_MoveToSector(thing, pJoint->thing.sector, 0);

	//if(pJoint->thing.attach_flags & SITH_ATTACH_WORLDSURFACE)
	//	sithThing_AttachToSurface(thing, pJoint->thing.attachedSurface, 0);
	//else if (pJoint->thing.attach_flags & SITH_ATTACH_THING)
	//	sithThing_AttachThing(thing, pJoint->thing.attachedThing);
	//sithPhysics_FindFloor(thing, 0);
}

#endif

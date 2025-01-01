#include "sithAnimClass.h"

#include "Engine/sithPuppet.h"
#include "World/sithWorld.h"
#include "General/stdString.h"
#include "General/stdHashTable.h"
#include "jk.h"
#ifdef ANIMCLASS_NAMES
#include "World/sithModel.h"
#endif

#ifdef PUPPET_PHYSICS
int sithAnimClass_ConstraintTypeFromName(const char* name)
{
	static const char* sithAnimClass_constraintNames[SITH_CONSTRAINT_COUNT] =
	{
		"ballsocket",
		"cone",
		"hinge"
	};

	for (int i = 0; i < SITH_CONSTRAINT_COUNT; ++i)
	{
		if(stricmp(sithAnimClass_constraintNames[i], name) == 0)
			return i;
	}
	return -1;
}
#endif

int sithAnimClass_Load(sithWorld *world, int a2)
{
    int num_animclasses; // ebx
    sithAnimclass *animclasses; // edi
    sithAnimclass *animclass; // esi
    char pup_path[128]; // [esp+10h] [ebp-80h] BYREF

    if ( a2 )
        return 0;
    stdConffile_ReadArgs();
    if ( _strcmp(stdConffile_entry.args[0].value, "world") || _strcmp(stdConffile_entry.args[1].value, "puppets") )
        return 0;
    num_animclasses = _atoi(stdConffile_entry.args[2].value);
    if ( !num_animclasses )
        return 1;
    animclasses = (sithAnimclass *)pSithHS->alloc(sizeof(sithAnimclass) * num_animclasses);
    world->animclasses = animclasses;
    if ( !animclasses )
        return 0;
    world->numAnimClasses = num_animclasses;
    world->numAnimClassesLoaded = 0;
    _memset(animclasses, 0, sizeof(sithAnimclass) * num_animclasses);
    while ( stdConffile_ReadArgs() )
    {
        if ( !_strcmp(stdConffile_entry.args[0].value, "end") )
            break;
        if ( !stdHashTable_GetKeyVal(sithPuppet_hashtable, stdConffile_entry.args[1].value) )
        {
            if ( sithWorld_pLoading->numAnimClassesLoaded != sithWorld_pLoading->numAnimClasses )
            {
                animclass = &sithWorld_pLoading->animclasses[sithWorld_pLoading->numAnimClassesLoaded];
                _memset(animclass, 0, sizeof(sithAnimclass));
                _strncpy(animclass->name, stdConffile_entry.args[1].value, 0x1Fu);
                animclass->name[31] = 0;
                // Added: sprintf -> snprintf
                stdString_snprintf(pup_path, 128, "%s%c%s", "misc\\pup", 92, stdConffile_entry.args[1].value);
                if ( sithAnimClass_LoadPupEntry(animclass, pup_path) )
                {
                    ++sithWorld_pLoading->numAnimClassesLoaded;
                    stdHashTable_SetKeyVal(sithPuppet_hashtable, animclass->name, animclass);
                }
            }
        }
    }
    return 1;
}

sithAnimclass* sithAnimClass_LoadEntry(char *a1)
{
    sithAnimclass *result; // eax
    int v3; // ecx
    sithAnimclass *v4; // esi
    stdHashTable *v5; // [esp-Ch] [ebp-9Ch]
    char v6[128]; // [esp+10h] [ebp-80h] BYREF

    result = (sithAnimclass *)stdHashTable_GetKeyVal(sithPuppet_hashtable, a1);
    if ( !result )
    {
        v3 = sithWorld_pLoading->numAnimClassesLoaded;
        if ( v3 == sithWorld_pLoading->numAnimClasses
          || (v4 = &sithWorld_pLoading->animclasses[v3],
              _memset(v4, 0, sizeof(sithAnimclass)),
              _strncpy(v4->name, a1, 0x1Fu),
              v4->name[31] = 0,
              _sprintf(v6, "%s%c%s", "misc\\pup", 92, a1),
              !sithAnimClass_LoadPupEntry(v4, v6)) )
        {
            result = 0;
        }
        else
        {
            v5 = sithPuppet_hashtable;
            ++sithWorld_pLoading->numAnimClassesLoaded;
            stdHashTable_SetKeyVal(v5, v4->name, v4);
            result = v4;
        }
    }
    return result;
}

int sithAnimClass_LoadPupEntry(sithAnimclass *animclass, char *fpath)
{
    int mode; // ebx
    int bodypart_idx; // esi
    int joint_idx; // eax
    intptr_t animNameIdx; // ebp
    sithWorld *world; // esi
    char *key_fname; // edi
    rdKeyframe *v10; // eax
    unsigned int v12; // eax
    rdKeyframe *keyframe; // edi
    int lowpri; // [esp+4h] [ebp-8Ch]
    int flags; // [esp+8h] [ebp-88h] BYREF
    int hipri; // [esp+Ch] [ebp-84h]
    char keyframe_fpath[128]; // [esp+10h] [ebp-80h] BYREF
#ifdef ANIMCLASS_NAMES
	rdModel3* model = NULL;
	int namedBodypart = 0;
#ifdef PUPPET_PHYSICS
	float mass = 1.0f / JOINTTYPE_NUM_JOINTS;
	float buoyancy = 1.0f;
	float health = 1000.0f;
	float damage = 1.0f;
	animclass->root = -1;
#endif
#endif

    mode = 0;
    if (!stdConffile_OpenRead(fpath))
        return 0;

#ifdef ANIMCLASS_NAMES
	_memset(animclass->bodypart, 0xFFu, sizeof(animclass->bodypart));
	animclass->jointToBodypart = NULL;
#else
	_memset(animclass->bodypart_to_joint, 0xFFu, sizeof(animclass->bodypart_to_joint));
#endif
	while ( stdConffile_ReadArgs() )
    {
        if ( !stdConffile_entry.numArgs )
            continue;
#ifdef ANIMCLASS_NAMES
		if (!_strcmp(stdConffile_entry.args[0].key, "model"))
		{
			model = sithModel_LoadEntry(stdConffile_entry.args[0].value, 0);
			animclass->jointToBodypart = (int*)rdroid_pHS->alloc(sizeof(int) * model->numHierarchyNodes);
			if(animclass->jointToBodypart)
				_memset(animclass->jointToBodypart, -1, sizeof(int) * model->numHierarchyNodes);
		}
		else
#endif
#ifdef PUPPET_PHYSICS
		if (!_strcmp(stdConffile_entry.args[0].key, "flags"))
		{
			_sscanf(stdConffile_entry.args[0].value, "%x", &animclass->flags);
		}
		else
#endif
        if ( !_strcmp(stdConffile_entry.args[0].key, "mode") )
        {
            mode = _atoi(stdConffile_entry.args[0].value);
            if ( stdConffile_entry.numArgs > 1u && !_strcmp(stdConffile_entry.args[1].key, "basedon") )
                _memcpy(&animclass->modes[mode], &animclass->modes[_atoi(stdConffile_entry.args[1].value)], sizeof(animclass->modes[mode]));
        }
        else if ( !_strcmp(stdConffile_entry.args[0].value, "joints") )
        {
            while ( stdConffile_ReadArgs() )
            {
                if ( !stdConffile_entry.numArgs || !_strcmp(stdConffile_entry.args[0].key, "end") )
                    break;
#ifdef ANIMCLASS_NAMES
				// new name or old index syntax
				if(model && stdConffile_entry.numArgs > 1 && !isdigit(stdConffile_entry.args[0].key[0]))
				{
					bodypart_idx = -1;
					joint_idx = -1;

					bodypart_idx = (intptr_t)stdHashTable_GetKeyVal(sithPuppet_jointNamesToIdxHashtable, stdConffile_entry.args[0].key) - 1;

					for (int node = 0; node < model->numHierarchyNodes; ++node)
					{
						if (!stricmp(model->hierarchyNodes[node].name, stdConffile_entry.args[1].key))
						{
							joint_idx = node;
							if(animclass->jointToBodypart)
								animclass->jointToBodypart[node] = bodypart_idx;
							break;
						}
					}
#ifdef PUPPET_PHYSICS
					if (stdConffile_entry.numArgs <= 2)
						flags = 0;
					else
						_sscanf(stdConffile_entry.args[2].key, "%x", &flags);

					if (stdConffile_entry.numArgs <= 3)
						mass = 1.0;
					else
						mass = _atof(stdConffile_entry.args[3].key);

					if (stdConffile_entry.numArgs <= 4)
						buoyancy = 1.0;
					else
						buoyancy = _atof(stdConffile_entry.args[4].key);

					if (stdConffile_entry.numArgs <= 5)
						health = 1000.0f;
					else
						health = _atof(stdConffile_entry.args[5].key);
					
					if (stdConffile_entry.numArgs <= 6)
						damage = 1.0;
					else
						damage = _atof(stdConffile_entry.args[6].key);
#endif
				}
				else // old syntax
				{
					bodypart_idx = _atoi(stdConffile_entry.args[0].key);
					joint_idx = _atoi(stdConffile_entry.args[0].value);
					flags = 0;
					mass = 1.0f;
					buoyancy = 1.0f;
					health = 1000.0f;
					damage = 1.0f;
				}
				if (bodypart_idx < JOINTTYPE_NUM_JOINTS && bodypart_idx >= 0) // Added: check for negative
				{
					animclass->bodypart[bodypart_idx].nodeIdx = joint_idx;
#ifdef PUPPET_PHYSICS
					if(joint_idx >= 0)
					{
						animclass->jointBits |= (1ull << bodypart_idx);
						if (flags & JOINTFLAGS_PHYSICS)
							animclass->physicsJointBits |= (1ull << bodypart_idx);
						animclass->bodypart[bodypart_idx].flags = flags;
						animclass->bodypart[bodypart_idx].mass = mass;
						animclass->bodypart[bodypart_idx].buoyancy = buoyancy;
						animclass->bodypart[bodypart_idx].health = health;
						animclass->bodypart[bodypart_idx].damage = damage;
						if (flags & JOINTFLAGS_ROOT)
							animclass->root = bodypart_idx;
					}
#endif
				}
#else
                bodypart_idx = _atoi(stdConffile_entry.args[0].key);
				joint_idx = _atoi(stdConffile_entry.args[0].value);
                if ( bodypart_idx < JOINTTYPE_NUM_JOINTS && bodypart_idx >= 0) // Added: check for negative
                    animclass->bodypart_to_joint[bodypart_idx] = joint_idx;
#endif
			}
        }
#ifdef PUPPET_PHYSICS
		else if (!_strcmp(stdConffile_entry.args[0].value, "constraints"))
		{
			while (stdConffile_ReadArgs())
			{
				if (!stdConffile_entry.numArgs || !_strcmp(stdConffile_entry.args[0].key, "end"))
					break;

				int type, targetJoint, constrainedJoint;
				if (model && stdConffile_entry.numArgs > 2)
				{
					type = sithAnimClass_ConstraintTypeFromName(stdConffile_entry.args[0].key);
					if(type != -1)
					{
						constrainedJoint = (intptr_t)stdHashTable_GetKeyVal(sithPuppet_jointNamesToIdxHashtable, stdConffile_entry.args[1].key) - 1;
						targetJoint = (intptr_t)stdHashTable_GetKeyVal(sithPuppet_jointNamesToIdxHashtable, stdConffile_entry.args[2].key) - 1;
				
						if (targetJoint != -1 && constrainedJoint != -1)
						{
							sithAnimclassConstraint* constraint = (sithAnimclassConstraint*)rdroid_pHS->alloc(sizeof(sithAnimclassConstraint));
							memset(constraint, 0, sizeof(sithAnimclassConstraint));
							constraint->type = type;
							constraint->jointB = constrainedJoint;
							constraint->jointA = targetJoint;
							constraint->next = animclass->constraints;
							animclass->constraints = constraint;
					
							if (stdConffile_entry.numArgs > 3)
							{
								if (_sscanf(stdConffile_entry.args[3].key,
											"(%f/%f/%f)",
											&constraint->axisB.x,
											&constraint->axisB.y,
											&constraint->axisB.z) != 3
									)
								{
									continue;
								}
							}
							if (stdConffile_entry.numArgs > 4)
							{
								if (_sscanf(stdConffile_entry.args[4].key,
											"(%f/%f/%f)",
											&constraint->axisA.x,
											&constraint->axisA.y,
											&constraint->axisA.z) != 3
									)
								{
									continue;
								}
							}
							if (stdConffile_entry.numArgs > 5)
								constraint->minAngle = atof(stdConffile_entry.args[5].key);

							if (stdConffile_entry.numArgs > 6)
								constraint->maxAngle = atof(stdConffile_entry.args[6].key);
						}
					}
				}
			}
		}
#endif
        else if ( stdConffile_entry.numArgs > 1u )
        {
            animNameIdx = (intptr_t)stdHashTable_GetKeyVal(sithPuppet_animNamesToIdxHashtable, stdConffile_entry.args[0].value);
            if ( animNameIdx )
            {
                if ( stdConffile_entry.numArgs <= 2u )
                    flags = 0;
                else
                    _sscanf(stdConffile_entry.args[2].value, "%x", &flags);
                if ( stdConffile_entry.numArgs <= 3u )
                    lowpri = 0;
                else
                    lowpri = _atoi(stdConffile_entry.args[3].value);
                if ( stdConffile_entry.numArgs <= 4u )
                    hipri = lowpri;
                else
                    hipri = _atoi(stdConffile_entry.args[4].value);
                if ( _strcmp(stdConffile_entry.args[1].value, "none") )
                {
                    world = sithWorld_pLoading;
                    key_fname = stdConffile_entry.args[1].value;
                    if ( sithWorld_pLoading->keyframes )
                    {
                        _sprintf(keyframe_fpath, "%s%c%s", "3do\\key", 92, stdConffile_entry.args[1].value);
                        v10 = (rdKeyframe *)stdHashTable_GetKeyVal(sithPuppet_keyframesHashtable, key_fname);
                        if ( v10 )
                        {
LABEL_39:
                            animclass->modes[mode].keyframe[animNameIdx].keyframe = v10;
                            animclass->modes[mode].keyframe[animNameIdx].flags = flags;
                            animclass->modes[mode].keyframe[animNameIdx].lowPri = lowpri;
                            animclass->modes[mode].keyframe[animNameIdx].highPri = hipri;

                            continue;
                        }
                        v12 = world->numKeyframesLoaded;
                        if ( v12 < world->numKeyframes )
                        {
                            keyframe = &world->keyframes[v12];
                            if ( rdKeyframe_LoadEntry(keyframe_fpath, keyframe) )
                            {
                                keyframe->id = world->numKeyframesLoaded;
                                if ( (world->level_type_maybe & 1) )
                                {
								#ifdef STATIC_JKL_EXT
									keyframe->id |= world->idx_offset;
								#else
                                    keyframe->id |= 0x8000u;
								#endif
                                }
                                stdHashTable_SetKeyVal(sithPuppet_keyframesHashtable, keyframe->name, keyframe);
                                v10 = keyframe;
                                ++world->numKeyframesLoaded;
                                goto LABEL_39;
                            }
                        }
                    }
                }
                v10 = NULL;
                goto LABEL_39;
            }
        }
    }
    stdConffile_Close();
    return 1;
}

void sithAnimClass_Free(sithWorld *world)
{
    unsigned int v1; // edi
    int v2; // ebx

    if ( world->numAnimClasses )
    {
        v1 = 0;
        if ( world->numAnimClassesLoaded )
        {
            v2 = 0;
            do
            {
#ifdef ANIMCLASS_NAMES
				if(world->animclasses[v2].jointToBodypart)
					rdroid_pHS->free(world->animclasses[v2].jointToBodypart);
#endif
#ifdef PUPPET_PHYSICS
				sithAnimclassConstraint* constraint = world->animclasses[v2].constraints;
				while (constraint)
				{
					sithAnimclassConstraint* next = constraint->next;
					if(constraint)
						rdroid_pHS->free(constraint);
					constraint = next;
				}
#endif
                stdHashTable_FreeKey(sithPuppet_hashtable, world->animclasses[v2].name);
                ++v1;
                ++v2;
            }
            while ( v1 < world->numAnimClassesLoaded );
        }
        pSithHS->free(world->animclasses);
        world->animclasses = 0;
        world->numAnimClassesLoaded = 0;
        world->numAnimClasses = 0;
    }
}


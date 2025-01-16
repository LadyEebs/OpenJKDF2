#include "types.h"
#include "globals.h"

#include "stdProfiler.h"
#include "Win95/std.h"
#include "stdPlatform.h"
#include "General/stdHashTable.h"
#include "General/stdString.h"
#include "stdPlatform.h"

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_PROFILER_LABELS 32

// todo: some kind of hierarchy
typedef struct stdProfiler_Label
{
	char                      functionName[128];
	int64_t                   startTime;
	int64_t                   duration;
	uint32_t                  frameIdx;
	struct stdProfiler_Label* next;
} stdProfiler_Label;

int stdProfiler_bInit = 0;
stdHashTable* stdProfiler_labelHashmap = NULL;
stdProfiler_Label* stdProfiler_labels = NULL;
uint32_t stdProfiler_frameIdx;

int stdProfiler_enable = 0;

void stdProfiler_Startup()
{
	if(stdProfiler_bInit)
		return;

	stdProfiler_labelHashmap = stdHashTable_New(64);
	
	if (!stdProfiler_labelHashmap)
		return;
	
	stdProfiler_bInit = 1;
	stdProfiler_frameIdx = 0;
}

void stdProfiler_Shutdown()
{
	if(!stdProfiler_bInit)
		return;

	stdProfiler_Label* label = stdProfiler_labels;
	while (label)
	{
		stdProfiler_Label* next = label->next;
		pHS->free(label);
		label = next;
	}

	stdHashTable_Free(stdProfiler_labelHashmap);
	stdProfiler_bInit = 0;
}

void stdProfiler_Tick()
{
	if(stdProfiler_enable)
		++stdProfiler_frameIdx;
}

void stdProfiler_BeginLabel(const char* name)
{
	if (!stdProfiler_enable)
		return;

	stdProfiler_Label* label = (stdProfiler_Label*)stdHashTable_GetKeyVal(stdProfiler_labelHashmap, name);
	if (!label)
	{
		label = pHS->alloc(sizeof(stdProfiler_Label));
		if (!label)
			return;

		stdString_SafeStrCopy(label->functionName, name, 128);
		stdHashTable_SetKeyVal(stdProfiler_labelHashmap, name, label);

		// alphabetical ordered insert
		stdProfiler_Label** firstLabel = &stdProfiler_labels;
		for (; *firstLabel != NULL; firstLabel = &(*firstLabel)->next)
		{
			if (stricmp((*firstLabel)->functionName, label->functionName) > 0)
				break;
		}

		label->next = *firstLabel;
		*firstLabel = label;
	}

	label->startTime = Linux_TimeUs();
}

void stdProfiler_EndLabel(const char* name)
{
	if (!stdProfiler_enable)
		return;

	stdProfiler_Label* label = (stdProfiler_Label*)stdHashTable_GetKeyVal(stdProfiler_labelHashmap, name);
	if (!label)
		return;
	
	int64_t duration = Linux_TimeUs() - label->startTime;
	if (label->frameIdx != stdProfiler_frameIdx)
		label->duration = duration;
	else
		label->duration += duration;

	label->frameIdx = stdProfiler_frameIdx;
	label->startTime = 0;
}

void stdProfiler_Enumerate(void(*func)(const char*, int64_t))
{
	if (!stdProfiler_enable)
		return;

	stdProfiler_Label* label = stdProfiler_labels;
	while (label)
	{
		func(label->functionName, label->duration);
		label = label->next;
	}
}
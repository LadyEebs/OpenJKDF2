#ifndef _STDJOB_H
#define _STDJOB_H

#include "types.h"
#include "globals.h"

#ifdef JOB_SYSTEM

typedef struct stdJobSystem stdJobSystem;
// JOBTODO the group job func can be a union or something?
typedef struct stdJobGroupArgs
{
	void          (*job)(uint32_t, uint32_t);
	stdJobSystem* jobs;
	uint32_t      jobCount;
	uint32_t      groupSize;
	uint32_t      groupIndex;
	void*         data;
} stdJobGroupArgs;

typedef void (*stdJob_function_t)(stdJobGroupArgs*);

void     stdJob_Startup();
void     stdJob_Shutdown();
int      stdJob_IsBusy();
void     stdJob_Wait();
uint32_t stdJob_Execute(stdJob_function_t job, void* args);
void     stdJob_Dispatch(uint32_t jobCount, uint32_t groupSize, void (*job)(uint32_t, uint32_t));

#endif

#endif // _STDJOB_H

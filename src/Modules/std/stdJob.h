#ifndef _STDJOB_H
#define _STDJOB_H

#include "types.h"
#include "globals.h"

#ifdef JOB_SYSTEM

typedef void (*stdJob_function_t)(void);

void     stdJob_Startup();
void     stdJob_Shutdown();
int      stdJob_IsBusy();
void     stdJob_Wait();
uint32_t stdJob_ExecutestdJob_Execute(stdJob_function_t job);
void     stdJob_Dispatch(uint32_t jobCount, uint32_t groupSize, void (*job)(uint32_t, uint32_t));

#endif

#endif // _STDJOB_H

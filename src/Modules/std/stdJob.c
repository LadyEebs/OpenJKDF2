#include "stdJob.h"
#include "Win95/std.h"
#include "stdPlatform.h"

#ifdef JOB_SYSTEM
#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h> // Requires C11 or later...
#endif

typedef struct stdRingBuffer
{
	stdJob_function_t* buffer;
	uint32_t           size;
	uint32_t           front;
	uint32_t           back;
#ifdef _WIN32
	CRITICAL_SECTION   lock;     // Mutex for thread safety
	HANDLE             notEmpty; // Event to signal buffer is not empty
#else
	pthread_mutex_t    lock;     // Mutex for thread safety (POSIX)
	pthread_cond_t     notEmpty; // Condition variable to signal buffer is not empty
#endif
} stdRingBuffer;

typedef struct stdJobSystem
{
	uint32_t          numThreads;
	stdRingBuffer     jobPool;
#ifdef _WIN32
	HANDLE*           workerThreads;
	HANDLE            wakeEvent;
	volatile LONG     currentLabel;
	volatile LONG     finishedLabel;
#else
	pthread_t*        workerThreads;
	pthread_mutex_t   wakeMutex;
	pthread_cond_t    wakeCondition;
	atomic_uint_fast64_t currentLabel;
	atomic_uint_fast64_t finishedLabel;
#endif
} stdJobSystem;

typedef struct stdJobGroupArgs
{
	stdJobSystem* jobs;
	uint32_t jobCount;
	uint32_t groupSize;
	void (*job)(uint32_t, uint32_t);
	uint32_t groupIndex;
} stdJobGroupArgs;

stdJobSystem stdJob_jobSystem;

void stdJob_InitRingBuffer(stdRingBuffer* rb, uint32_t size)
{
	rb->buffer = (stdJob_function_t*)std_pHS->alloc(size * sizeof(stdJob_function_t));
	if (!rb->buffer)
	{
		stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Failed to allocate ring buffer\n");
		exit(EXIT_FAILURE);
	}
	rb->size = size;
	rb->front = 0;
	rb->back = 0;
#ifdef _WIN32
	InitializeCriticalSection(&rb->lock);
	rb->notEmpty = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
	pthread_mutex_init(&rb->lock, NULL);
	pthread_cond_init(&rb->notEmpty, NULL);
#endif
}

int stdJob_PushRingBuffer(stdRingBuffer* rb, stdJob_function_t job)
{
	int result = 0;

#ifdef _WIN32
	EnterCriticalSection(&rb->lock);
#else
	pthread_mutex_lock(&rb->lock);
#endif

	uint32_t next_back = (rb->back + 1) % rb->size;
	if (next_back != rb->front) // Check if buffer is not full
	{
		rb->buffer[rb->back] = job;
		rb->back = next_back;
		result = 1;

#ifdef _WIN32
		SetEvent(rb->notEmpty); // Signal that the buffer is not empty
#else
		pthread_cond_signal(&rb->notEmpty); // Signal that the buffer is not empty
#endif
	}

#ifdef _WIN32
	LeaveCriticalSection(&rb->lock);
#else
	pthread_mutex_unlock(&rb->lock);
#endif
	return result;
}

int stdJob_PopRingBuffer(stdRingBuffer* rb, stdJob_function_t* job)
{
	int result = 0;

#ifdef _WIN32
	EnterCriticalSection(&rb->lock);
#else
	pthread_mutex_lock(&rb->lock);
#endif

	while (rb->front == rb->back) // Wait if the buffer is empty
	{
#ifdef _WIN32
		LeaveCriticalSection(&rb->lock);
		WaitForSingleObject(rb->notEmpty, INFINITE);
		EnterCriticalSection(&rb->lock);
#else
		pthread_cond_wait(&rb->notEmpty, &rb->lock);
#endif
	}

	if (rb->front != rb->back) // Buffer is not empty
	{
		*job = rb->buffer[rb->front];
		rb->front = (rb->front + 1) % rb->size;
		result = 1;
	}

#ifdef _WIN32
	LeaveCriticalSection(&rb->lock);
#else
	pthread_mutex_unlock(&rb->lock);
#endif

	return result;
}

void stdJob_FreeRingBuffer(stdRingBuffer* rb)
{
	if(rb->buffer)
	{
		std_pHS->free(rb->buffer);
		rb->buffer = NULL;
	}
#ifdef _WIN32
	DeleteCriticalSection(&rb->lock);
	CloseHandle(rb->notEmpty);
#else
	pthread_mutex_destroy(&rb->lock);
	pthread_cond_destroy(&rb->notEmpty);
#endif
}

void stdJob_AddToCurrentLabel(uint32_t value)
{
#ifdef _WIN32
	InterlockedAdd(&stdJob_jobSystem.currentLabel, value);
#else
	atomic_fetch_add_explicit(&stdJob_jobSystem.currentLabel, value, memory_order_relaxed);
#endif
}

void stdJob_IncrementCurrentLabel()
{
#ifdef _WIN32
	InterlockedIncrement(&stdJob_jobSystem.currentLabel);
#else
	atomic_fetch_add_explicit(&stdJob_jobSystem.currentLabel, 1, memory_order_relaxed);
#endif
}

void stdJob_IncrementFinishedLabel()
{
#ifdef _WIN32
	InterlockedIncrement(&stdJob_jobSystem.finishedLabel);
#else
	atomic_fetch_add_explicit(&stdJob_jobSystem.finishedLabel, 1, memory_order_relaxed);
#endif
}

uint64_t stdJob_GetCurrentLabel()
{
#ifdef _WIN32
	return InterlockedCompareExchange(&stdJob_jobSystem.currentLabel, 0, 0);
#else
	return atomic_load_explicit(&stdJob_jobSystem.currentLabel, memory_order_relaxed);
#endif
}

uint64_t stdJob_GetFinishedLabel()
{
#ifdef _WIN32
	return InterlockedCompareExchange(&stdJob_jobSystem.finishedLabel, 0, 0);
#else
	return atomic_load_explicit(&stdJob_jobSystem.finishedLabel, memory_order_relaxed);
#endif
}

// Adjust stdJob_IsBusy and stdJob_Wait to use atomic operations
int stdJob_IsBusy()
{
	return stdJob_GetFinishedLabel() < stdJob_GetCurrentLabel();
}

#ifdef _WIN32
DWORD WINAPI stdJob_WorkerThread(LPVOID param)
{
#else
void* stdJob_WorkerThread(void* param)
{
#endif
	stdJobSystem* jobs = (stdJobSystem*)param;
	stdJob_function_t job;
	while (1)
	{
		if (stdJob_PopRingBuffer(&jobs->jobPool, &job))
		{
			job(); // Execute job
			stdJob_IncrementFinishedLabel();
		}
		else
		{
#ifdef _WIN32
			WaitForSingleObject(jobs->wakeEvent, INFINITE); // Wait on Windows event
#else
			pthread_mutex_lock(&jobs->wakeMutex);
			pthread_cond_wait(&jobs->wakeCondition, &jobs->wakeMutex);
			pthread_mutex_unlock(&jobs->wakeMutex);
#endif
		}
	}
#ifdef _WIN32
	return 0;
#endif
}

void stdJob_TestJob(void* data)
{
	stdPlatform_Printf("Test job run successfully\n");
}

void stdJob_Startup()
{
	stdJob_jobSystem.finishedLabel = 0;
	stdJob_jobSystem.currentLabel = 0;

#ifdef _WIN32
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	uint32_t numCores = sysInfo.dwNumberOfProcessors;
#else
	long cores = sysconf(_SC_NPROCESSORS_ONLN);
	uint32_t numCores = (cores > 0) ? (uint32_t)cores : 1; // Fallback to 1 if sysconf fails
#endif
	stdPlatform_Printf("Starting job system with %d threads\n", numCores);
	stdJob_jobSystem.numThreads = numCores > 0 ? numCores : 1;

	stdJob_InitRingBuffer(&stdJob_jobSystem.jobPool, 256);
#ifdef _WIN32
	stdJob_jobSystem.workerThreads = (HANDLE*)std_pHS->alloc(stdJob_jobSystem.numThreads * sizeof(HANDLE));
	if (!stdJob_jobSystem.workerThreads)
	{
		stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Failed to allocate worker thread handles\n");
		exit(EXIT_FAILURE);
	}

	stdJob_jobSystem.wakeEvent = CreateEvent(NULL, FALSE, FALSE, NULL); // Create event for signaling workers
	if (!stdJob_jobSystem.wakeEvent)
	{
		stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Failed to create wake event\n");
		exit(EXIT_FAILURE);
	}

	for (uint32_t threadID = 0; threadID < stdJob_jobSystem.numThreads; ++threadID)
	{
		stdJob_jobSystem.workerThreads[threadID] = CreateThread(NULL, 0, stdJob_WorkerThread, &stdJob_jobSystem, 0, NULL);
		if (!stdJob_jobSystem.workerThreads[threadID])
		{
			stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Failed to create worker thread %u\n", threadID);
			exit(EXIT_FAILURE);
		}

		// Set thread affinity
		DWORD_PTR affinityMask = 1ull << threadID;
		SetThreadAffinityMask(stdJob_jobSystem.workerThreads[threadID], affinityMask);
		
		// Set thread name
		SetThreadDescription(stdJob_jobSystem.workerThreads[threadID], L"stdJobThread");
	}
#else
	stdJob_jobSystem.workerThreads = (pthread_t*)std_pHS->alloc(stdJob_jobSystem.numThreads * sizeof(pthread_t));
	if (!stdJob_jobSystem.workerThreads)
	{
		stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Failed to allocate worker thread handles\n");
		exit(EXIT_FAILURE);
	}
	
	if (pthread_mutex_init(&stdJob_jobSystem.wakeMutex, NULL) != 0)
	{
		stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Failed to initialize wake mutex\n");
		exit(EXIT_FAILURE);
	}

	if (pthread_cond_init(&stdJob_jobSystem.wakeCondition, NULL) != 0)
	{
		stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Failed to initialize wake condition variable\n");
		exit(EXIT_FAILURE);
	}

	for (uint32_t threadID = 0; threadID < stdJob_jobSystem.numThreads; ++threadID)
	{
		if (pthread_create(&stdJob_jobSystem.workerThreads[threadID], NULL, stdJob_WorkerThread, &stdJob_jobSystem) != 0)
		{
			stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Failed to create worker thread %u\n", threadID);
			exit(EXIT_FAILURE);
		}
	}
#endif

	stdPlatform_Printf("Kicking off test job\n");
	stdJob_Execute(stdJob_TestJob);
}

void stdJob_Shutdown()
{
	stdJob_FreeRingBuffer(&stdJob_jobSystem.jobPool);

#ifdef _WIN32
	for (uint32_t i = 0; i < stdJob_jobSystem.numThreads; ++i)
		CloseHandle(stdJob_jobSystem.workerThreads[i]);

	CloseHandle(stdJob_jobSystem.wakeEvent); // Close the event handle
#else
	for (uint32_t i = 0; i < stdJob_jobSystem.numThreads; ++i)
	{
		pthread_cancel(stdJob_jobSystem.workerThreads[i]); // Terminate thread
		pthread_join(stdJob_jobSystem.workerThreads[i], NULL); // Wait for thread to finish
	}
	pthread_mutex_destroy(&stdJob_jobSystem.wakeMutex);
	pthread_cond_destroy(&stdJob_jobSystem.wakeCondition);
#endif
	if (stdJob_jobSystem.workerThreads)
	{
		std_pHS->free(stdJob_jobSystem.workerThreads);
		stdJob_jobSystem.workerThreads = 0;
	}
}

void stdJob_Wait()
{
	// Polling
	while (stdJob_IsBusy())
	{
#ifdef _WIN32
		SetEvent(stdJob_jobSystem.wakeEvent); // Signal all workers on Windows to check for jobs
#else
		pthread_cond_signal(&stdJob_jobSystem.wakeCondition); // Signal workers in POSIX
		sched_yield();  // Yield the processor
#endif
	}
}

void stdJob_Execute(stdJob_function_t job)
{
	stdJob_IncrementCurrentLabel();
	while (!stdJob_PushRingBuffer(&stdJob_jobSystem.jobPool, job))
	{
#ifdef _WIN32
		SetEvent(stdJob_jobSystem.wakeEvent); // Signal workers on Windows to check for jobs
#else
		pthread_cond_signal(&stdJob_jobSystem.wakeCondition); // Signal workers in POSIX
		sched_yield();
#endif
	}
#ifdef _WIN32
	SetEvent(stdJob_jobSystem.wakeEvent); // Notify worker threads on Windows
#else
	pthread_cond_signal(&stdJob_jobSystem.wakeCondition); // Notify worker threads in POSIX
#endif
}

void stdJob_ExecuteGroup(void* param)
{
	stdJobGroupArgs* args = (stdJobGroupArgs*)param;

	uint32_t groupJobOffset = args->groupIndex * args->groupSize;
	uint32_t groupJobEnd = (groupJobOffset + args->groupSize > args->jobCount) ? args->jobCount : (groupJobOffset + args->groupSize);

	for (uint32_t i = groupJobOffset; i < groupJobEnd; ++i)
		args->job(i, args->groupIndex);

	std_pHS->free(args);
}

void stdJob_Dispatch(uint32_t jobCount, uint32_t groupSize, void (*job)(uint32_t, uint32_t))
{
	if (jobCount == 0 || groupSize == 0)
		return;

	uint32_t groupCount = (jobCount + groupSize - 1) / groupSize;
	stdJob_AddToCurrentLabel(groupCount);

	for (uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
	{
		stdJobGroupArgs* args = (stdJobGroupArgs*)std_pHS->alloc(sizeof(stdJobGroupArgs));
		if (!args)
		{
			stdPrintf(std_pHS->errorPrint, ".\\General\\stdJob.c", __LINE__, "Memory allocation failed for job group arguments\n");
			exit(EXIT_FAILURE);
		}

		args->jobs = &stdJob_jobSystem;
		args->jobCount = jobCount;
		args->groupSize = groupSize;
		args->job = job;
		args->groupIndex = groupIndex;

		stdJob_function_t jobGroup = stdJob_ExecuteGroup;
		while (!stdJob_PushRingBuffer(&stdJob_jobSystem.jobPool, jobGroup))
		{
#ifdef _WIN32
			SetEvent(stdJob_jobSystem.wakeEvent); // Notify worker threads on Windows
#else
			pthread_cond_signal(&stdJob_jobSystem.wakeCondition); // Notify worker threads in POSIX
			sched_yield();
#endif
		}

#ifdef _WIN32
		SetEvent(stdJob_jobSystem.wakeEvent); // Notify worker threads on Windows
#else
		pthread_cond_signal(&stdJob_jobSystem.wakeCondition); // Notify worker threads in POSIX
#endif
	}
}

#endif

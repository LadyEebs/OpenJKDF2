#ifndef _RDRECT_H
#define _RDRECT_H

#include <stdint.h>

typedef struct rdRect
{
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} rdRect;


typedef struct rdRectf
{
	float x;
	float y;
	float width;
	float height;
} rdRectf;

#endif // _RDRECT_H

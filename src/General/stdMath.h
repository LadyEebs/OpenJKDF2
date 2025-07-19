#ifndef _STDMATH_H
#define _STDMATH_H

#include "hook.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define stdMath_FlexPower_ADDR (0x00431E20)
#define stdMath_NormalizeAngle_ADDR (0x00431E5B)
#define stdMath_NormalizeAngleAcute_ADDR (0x00431F10)
#define stdMath_NormalizeDeltaAngle_ADDR (0x00431F45)
#define stdMath_SinCos_ADDR (0x00431FBD)
#define stdMath_Tan_ADDR (0x00432396)
#define stdMath_ArcSin1_ADDR (0x00432623)
#define stdMath_ArcSin2_ADDR (0x004326FC)
#define stdMath_ArcSin3_ADDR (0x00432809)
#define stdMath_ArcTan1_ADDR (0x0043294A)
#define stdMath_ArcTan2_ADDR (0x00432AC6)
#define stdMath_ArcTan3_ADDR (0x00432C5C)
#define stdMath_ArcTan4_ADDR (0x00432E0C)
#define stdMath_FloorDivMod_ADDR (0x00432FD6)
#define stdMath_Dist2D1_ADDR (0x0043303E)
#define stdMath_Dist2D2_ADDR (0x004330E2)
#define stdMath_Dist2D3_ADDR (0x00433186)
#define stdMath_Dist2D4_ADDR (0x0043322A)
#define stdMath_Dist3D1_ADDR (0x00433302)
#define stdMath_Dist3D2_ADDR (0x00433418)
#define stdMath_Dist3D3_ADDR (0x0043352E)
#define stdMath_Floor_ADDR (0x00433650)
#define stdMath_Sqrt_ADDR (0x00433670)

flex_t stdMath_FlexPower(flex_t num, int32_t exp);
flex_t stdMath_NormalizeAngle(flex_t angle);
flex_t stdMath_NormalizeAngleAcute(flex_t angle);
flex_t stdMath_NormalizeDeltaAngle(flex_t a1, flex_t a2);
void stdMath_SinCos(flex_t angle, flex_t *pSinOut, flex_t *pCosOut);
flex_t stdMath_Tan(flex_t a1);
flex_t stdMath_ArcSin1(flex_t val);
flex_t stdMath_ArcSin2(flex_t val);
flex_t stdMath_ArcSin3(flex_t val);
flex_t stdMath_ArcTan1(flex_t a1, flex_t a2);
flex_t stdMath_ArcTan2(flex_t a1, flex_t a2);
flex_t stdMath_ArcTan3(flex_t a1, flex_t a2);
flex_t stdMath_ArcTan4(flex_t a1, flex_t a2);
int32_t stdMath_FloorDivMod(int32_t in1, int32_t in2, int32_t *out1, int32_t *out2);

//IMPORT_FUNC(stdMath_SinCos, void, (flex_t, flex_t*, flex_t*), stdMath_SinCos_ADDR)
//IMPORT_FUNC(stdMath_Tan, flex_t, (flex_t), stdMath_Tan_ADDR)
//IMPORT_FUNC(stdMath_ArcSin1, flex_t, (flex_t), stdMath_ArcSin1_ADDR)
//IMPORT_FUNC(stdMath_ArcSin2, flex_t, (flex_t), stdMath_ArcSin2_ADDR)
//IMPORT_FUNC(stdMath_ArcSin3, flex_t, (flex_t), stdMath_ArcSin3_ADDR)
//IMPORT_FUNC(stdMath_ArcTan1, flex_t, (flex_t, flex_t), stdMath_ArcTan1_ADDR)
//IMPORT_FUNC(stdMath_ArcTan2, flex_t, (flex_t, flex_t), stdMath_ArcTan2_ADDR)
//IMPORT_FUNC(stdMath_ArcTan3, flex_t, (flex_t, flex_t), stdMath_ArcTan3_ADDR)
//IMPORT_FUNC(stdMath_ArcTan4, flex_t, (flex_t, flex_t), stdMath_ArcTan4_ADDR)
//IMPORT_FUNC(stdMath_FloorDivMod, int, (int, int, int*, int*), stdMath_FloorDivMod_ADDR)

//static void (*_stdMath_SinCos)(flex_t angle, flex_t *pSinOut, flex_t *pCosOut) = (void*)stdMath_SinCos_ADDR;

flex_t stdMath_Dist2D1(flex_t a1, flex_t a2);
flex_t stdMath_Dist2D2(flex_t a1, flex_t a2);
flex_t stdMath_Dist2D3(flex_t a1, flex_t a2);
flex_t stdMath_Dist2D4(flex_t a1, flex_t a2);
flex_t stdMath_Dist3D1(flex_t a1, flex_t a2, flex_t a3);
flex_t stdMath_Dist3D2(flex_t a1, flex_t a2, flex_t a3);
flex_t stdMath_Dist3D3(flex_t a1, flex_t a2, flex_t a3);
flex_t stdMath_Floor(flex_t a);
flex_t stdMath_Sqrt(flex_t a);
flex_t stdMath_Frac(flex_t a);

//static inline int32_t stdFixed_Multiply(int param_1, int param_2)
//{
//	return (int32_t)(((int64_t)param_1 * (int64_t)param_2) >> 16);
//}

// Added
flex_t stdMath_ClipPrecision(flex_t val);
flex_t stdMath_Clamp(flex_t val, flex_t valMin, flex_t valMax);
flex_t stdMath_ClampValue(flex_t val, flex_t valAbsMax);

static inline flex_t stdMath_Saturate(flex_t val)
{
	return stdMath_Clamp(val, 0.0f, 1.0f);
}

static inline flex_t stdMath_Fabs(flex_t val)
{
    //return fabs(val);
    return (val < 0.0) ? -val : val;
}

static inline flex_t stdMath_Lerp(flex_t x, flex_t y, flex_t f)
{
	return x + (y - x) * f;
}

static inline int32_t stdMath_ClampInt(int32_t val, int32_t valMin, int32_t valMax)
{
    if (val < valMin)
        return valMin;
    
    if (val > valMax)
        return valMax;

    return val;
}

static inline uint8_t stdMath_ClampU8(uint8_t val, uint8_t valMin, uint8_t valMax)
{
    if (val < valMin)
        return valMin;
    
    if (val > valMax)
        return valMax;

    return val;
}

int stdMath_FindLSB64(uint64_t value);
int stdMath_FindMSB64(uint64_t value);
int stdMath_NextPow2(uint32_t value);

float stdMath_Sin(float angle);
float stdMath_Cos(float angle);

// signed 16 bit half precision float
uint16_t stdMath_FloatToHalf(float val); // fixme: flex_t
float stdMath_HalfToFloat(uint16_t value); // fixme: flex_t

uint32_t stdMath_PackHalf2x16(float x, float y); // fixme: flex_t

// unsigned 8 bit mini float, 4 bits exponent and 4 bits significand
uint8_t stdMath_FloatToMini8(float x); // fixme: flex_t
float stdMath_Mini8ToFloat(uint8_t x); // fixme: flex_t

uint32_t stdMath_FloatBitsToUint(float x); // fixme: flex_t

uint8_t stdMath_PackUnorm1x8(float f); // fixme: flex_t
uint32_t stdMath_PackUnorm4x8(const rdVector4* unpackedInput);

int8_t stdMath_PackSnorm1x8(float f); // fixme: flex_t
uint32_t stdMath_PackSnorm4x8(const rdVector4* unpackedInput);

extern const flex_t aSinTable[4096];
extern const flex_t aTanTable[4096];

#endif // _STDMATH_H

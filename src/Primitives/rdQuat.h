#ifndef _RDQUAT_H
#define _RDQUAT_H

#include "types.h"

void rdQuat_Set(rdQuat* out, float x, float y, float z, float w);

void rdQuat_BuildFromAxisAngle(rdQuat* out, rdVector3* axis, float angle);
void rdQuat_BuildFromVector(rdQuat* out, rdVector3* axis);
void rdQuat_BuildFrom34(rdQuat* out, const rdMatrix34* matrix);
void rdQuat_BuildFromVectors(rdQuat* out, const rdVector3* v1, const rdVector3* v2);
void rdQuat_BuildFromAngles(rdQuat* out, const rdVector3* angles);

void rdQuat_ExtractAxisAngle(rdQuat* q, rdVector3* axis, float* angle);
void rdQuat_ExtractAngles(rdQuat* q, rdVector3* angles);

void rdQuat_Mul(rdQuat* out, rdQuat* qa, rdQuat* qb);
void rdQuat_MulAcc(rdQuat* qa, rdQuat* qb);

float rdQuat_LenSq(rdQuat* q);

void rdQuat_TransformVector(rdVector3* out, const rdQuat* q, const rdVector3* v);

void rdQuat_Conjugate(rdQuat* out, const rdQuat* q);
void rdQuat_ConjugateAcc(rdQuat* q);

void rdQuat_ToMatrix(rdMatrix34* out, const rdQuat* q);
void rdQuat_Slerp(rdQuat* out, const rdQuat* qa, const rdQuat* qb, const float c);

void rdQuat_NormalizeAcc(rdQuat* q);
void rdQuat_Inverse(rdQuat* out, const rdQuat* q);

int rdQuat_IsZero(const rdQuat* q);

#endif // _RDQUAT_H

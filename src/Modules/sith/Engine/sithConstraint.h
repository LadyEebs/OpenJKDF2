#ifndef _SITH_CONSTRAINT_H
#define _SITH_CONSTRAINT_H

#include "types.h"
#include "globals.h"

void sithConstraint_SolveConstraints(sithThing* pThing, float deltaSeconds);

void sithConstraint_AddDistanceConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAnchor);
void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAxis, float angle);
void sithConstraint_AddAngleConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pMinAngles, const rdVector3* pMaxAngles);
void sithConstraint_AddLookConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdMatrix34* pRefMat, int flipUp);

void sithConstraint_SolveDistanceConstraint(sithConstraint* pConstraint, float deltaSeconds);
void sithConstraint_SolveConeConstraint(sithConstraint* pConstraint, float deltaSeconds);
void sithConstraint_SolveAngleConstrain(sithConstraint* pConstraint, float deltaSeconds);
void sithConstraint_SolveLookConstraint(sithConstraint* pConstraint, float deltaSeconds);

#endif

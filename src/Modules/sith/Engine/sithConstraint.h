#ifndef _SITH_CONSTRAINT_H
#define _SITH_CONSTRAINT_H

#include "types.h"
#include "globals.h"

void sithConstraint_SolveConstraints(sithThing* pThing, float deltaSeconds);

void sithConstraint_AddDistanceConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, const rdVector3* pAnchor);
void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, const rdVector3* pMinAngles, const rdVector3* pMaxAngles);
void sithConstraint_AddLookConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, const rdMatrix34* pRefMat, int flipUp);

void sithConstraint_SolveDistanceConstraint(sithConstraint* pConstraint, float deltaSeconds);
void sithConstraint_SolveConeConstrain(sithConstraint* pConstraint, float deltaSeconds);
void sithConstraint_SolveLookConstraint(sithConstraint* pConstraint);

#endif

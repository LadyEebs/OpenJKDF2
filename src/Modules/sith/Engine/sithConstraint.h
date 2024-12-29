#ifndef _SITH_CONSTRAINT_H
#define _SITH_CONSTRAINT_H

#include "types.h"
#include "globals.h"

void sithConstraint_SolveConstraints(sithThing* pThing, float deltaSeconds);

void sithConstraint_AddDistanceConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAnchor, const rdVector3* pConstrainedAnchor, float distance);
void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAnchor, const rdVector3* pAxis, float angle, const rdVector3* pJointAxis);
void sithConstraint_AddHingeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAxis, const rdVector3* pJointAxis, float minAngle, float maxAngle);
void sithConstraint_RemoveConstraint(sithConstraint* pConstraint);

void sithConstraint_Draw(sithConstraint* pConstraint);

#endif

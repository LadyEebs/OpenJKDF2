#include "sithConstraint.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"

#include "General/stdMath.h"
#include "Primitives/rdMath.h"
#include "Primitives/rdQuat.h"

#include "Primitives/rdSprite.h"
#include "Primitives/rdPolyline.h"
#include "Engine/rdThing.h"

#ifdef PUPPET_PHYSICS

extern float jkPlayer_puppetAngBias;
extern float jkPlayer_puppetPosBias;
extern float jkPlayer_puppetFriction;

void sithConstraint_AddDistanceConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAnchor, const rdVector3* pConstrainedAnchor, float distance)
{
	sithConstraint* constraint = (sithConstraint*)pSithHS->alloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_DISTANCE;
	constraint->parentThing = pThing;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;
	constraint->distanceParams.targetAnchor = *pTargetAnchor;
	constraint->distanceParams.constraintAnchor = *pConstrainedAnchor;
	
	pConstrainedThing->constraintParent = pTargetThing;
	pConstrainedThing->constraintRoot = pThing;

 	constraint->next = pThing->constraints;
	pThing->constraints = constraint;

	// new
//	constraint->thing = pConstrainedThing;
//	constraint->thing->constraintParent = pTargetThing;
//	
//	constraint->next = pTargetThing->constraints;
//	pTargetThing->constraints = constraint;
}

void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAxis, float angle, const rdVector3* pJointAxis)
{
	sithConstraint* constraint = (sithConstraint*)pSithHS->alloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_CONE;
	constraint->parentThing = pThing;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

	constraint->coneParams.coneAxis = *pAxis;
	constraint->coneParams.coneAngle = angle * 0.5f;
	constraint->coneParams.coneAngleCos = stdMath_Cos(angle * 0.5f);
	constraint->coneParams.jointAxis = *pJointAxis;

	pConstrainedThing->constraintParent = pTargetThing;
	pConstrainedThing->constraintRoot = pThing;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;

	// new
//	constraint->thing = pConstrainedThing;
//	constraint->thing->constraintParent = pTargetThing;
//
//	constraint->next = pTargetThing->constraints;
//	pTargetThing->constraints = constraint;
}

void sithConstraint_AddHingeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAxis, const rdVector3* pJointAxis, float minAngle, float maxAngle)
{
	sithConstraint* constraint = (sithConstraint*)pSithHS->alloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_HINGE;
	constraint->parentThing = pThing;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

	constraint->hingeParams.targetAxis = *pTargetAxis;
	constraint->hingeParams.jointAxis = *pJointAxis;
	constraint->hingeParams.minCosAngle = stdMath_Cos(minAngle * 0.5f);
	constraint->hingeParams.maxCosAngle = stdMath_Cos(maxAngle * 0.5f);

	pConstrainedThing->constraintParent = pTargetThing;
	pConstrainedThing->constraintRoot = pThing;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;

	// new
//	constraint->thing = pConstrainedThing;
//	constraint->thing->constraintParent = pTargetThing;
//
//	constraint->next = pTargetThing->constraints;
//	pTargetThing->constraints = constraint;
}

void sithConstraint_RemoveConstraint(sithConstraint* pConstraint)
{
	if(!pConstraint)
		return;

	sithConstraint* pConstraintList = pConstraint->parentThing->constraints;
	if (pConstraintList == pConstraint)
	{
		pConstraint->parentThing->constraints = pConstraint->next;
		pSithHS->free(pConstraint);
		return;
	}
		
	sithConstraint* current = pConstraintList;
	while (current->next && current->next != pConstraint)
		current = current->next;

	if (current->next == pConstraint)
	{
		current->next = pConstraint->next;
		pSithHS->free(pConstraint);
	}
}

// 1 / (J * M^-1 * J^T)
float sithConstraint_ComputeEffectiveMass(sithConstraint* constraint)
{
	float effectiveMass =
		  rdVector_Dot3(&constraint->result.JvA, &constraint->result.JvA) / constraint->targetThing->physicsParams.mass
		+ rdVector_Dot3(&constraint->result.JvB, &constraint->result.JvB) / constraint->constrainedThing->physicsParams.mass
		+ rdVector_Dot3(&constraint->result.JrA, &constraint->result.JrA) / constraint->targetThing->physicsParams.inertia
		+ rdVector_Dot3(&constraint->result.JrB, &constraint->result.JrB) / constraint->constrainedThing->physicsParams.inertia;
	return (effectiveMass > 0.0001f) ? (1.0f / effectiveMass) : 0.0001f;
}

static void sithConstraint_SolveDistanceConstraint(sithConstraintResult* pResult, sithConstraint* pConstraint, float deltaSeconds)
{
	// anchor A offset in world space
	rdVector3 offsetA;
	rdMatrix_TransformVector34(&offsetA, &pConstraint->distanceParams.targetAnchor, &pConstraint->targetThing->lookOrientation);

	// anchor B offset in world space
	rdVector3 offsetB;
	rdMatrix_TransformVector34(&offsetB, &pConstraint->distanceParams.constraintAnchor, &pConstraint->constrainedThing->lookOrientation);

	// anchor positions in world space
	rdVector3 anchorA, anchorB;
	rdVector_Add3(&anchorA, &offsetA, &pConstraint->targetThing->position);
	rdVector_Add3(&anchorB, &offsetB, &pConstraint->constrainedThing->position);

	// vector between the anchors and the distance
	rdVector3 constraint, unitConstraint;
	rdVector_Sub3(&constraint, &anchorA, &anchorB);

	// calculate constraint force
	// currently scalar, can be vector for more accuracy
	pResult->C = rdVector_Normalize3(&unitConstraint, &constraint);
	if(stdMath_Fabs(pResult->C) < 0.00001f)
	{
		pResult->C = 0.0f;
		return;
	}

	// add bias for error correction
	pResult->C = -(jkPlayer_puppetPosBias / deltaSeconds) * pResult->C;
	pResult->C = stdMath_Clamp(pResult->C, -2.0f, 2.0f);

	rdVector3 unitConstraintN;
	rdVector_Neg3(&unitConstraintN, &unitConstraint);

	// Calculate Jacobians
	pResult->JvA = unitConstraint;
	pResult->JvB = unitConstraintN;	
	rdVector_Cross3(&pResult->JrA, &offsetA, &unitConstraintN);
	rdVector_Cross3(&pResult->JrB, &offsetB, &unitConstraint);
}

static void sithConstraint_SolveConeConstraint(sithConstraintResult* pResult, sithConstraint* pConstraint, float deltaSeconds)
{
	// cone axis to world space
	rdVector3 coneAxis;
	rdMatrix_TransformVector34(&coneAxis, &pConstraint->coneParams.coneAxis, &pConstraint->targetThing->lookOrientation);
	rdVector_Normalize3Acc(&coneAxis);

	// joint axis to world space
	rdVector3 thingAxis;
	rdMatrix_TransformVector34(&thingAxis, &pConstraint->coneParams.jointAxis, &pConstraint->constrainedThing->lookOrientation);
	rdVector_Normalize3Acc(&thingAxis);

	sithThing* bodyA = pConstraint->targetThing;
	sithThing* bodyB = pConstraint->constrainedThing;
	
	rdVector3 relativeAxis;
	//rdVector_Cross3(&relativeAxis, &coneAxis, &thingAxis);
	rdVector_Cross3(&relativeAxis, &thingAxis, &coneAxis);
	rdVector_Normalize3Acc(&relativeAxis);

	float dotProduct = rdVector_Dot3(&coneAxis, &thingAxis);
	if (dotProduct >= pConstraint->coneParams.coneAngleCos)
	{
		pResult->C = 0.0f;
		return;
	}

	rdVector3 correctionAxis;
	rdVector_Cross3(&correctionAxis, &coneAxis, &relativeAxis);
	rdVector_Normalize3Acc(&correctionAxis);

	rdVector3 JwA = relativeAxis;
	rdVector3 JwB = relativeAxis;
	rdVector_Neg3Acc(&JwB);

	pResult->JvA = rdroid_zeroVector3;
	pResult->JvB = rdroid_zeroVector3;
	pResult->JrA = JwA;
	pResult->JrB = JwB;
	pResult->C = pConstraint->coneParams.coneAngleCos - dotProduct;
	pResult->C = (jkPlayer_puppetAngBias / deltaSeconds) * pResult->C;
	pResult->C = stdMath_Clamp(pResult->C, -2.0f, 2.0f);
}

static void sithConstraint_SolveHingeConstraint(sithConstraintResult* pResult, sithConstraint* pConstraint, float deltaSeconds)
{
	rdVector_Zero3(&pResult->JvA);
	rdVector_Zero3(&pResult->JvB);

	rdVector3 hingeAxisA, hingeAxisB;	
	rdMatrix_TransformVector34(&hingeAxisA, &pConstraint->hingeParams.targetAxis, &pConstraint->targetThing->lookOrientation);
	rdMatrix_TransformVector34(&hingeAxisB, &pConstraint->hingeParams.jointAxis, &pConstraint->constrainedThing->lookOrientation);

	float cosAngle = rdVector_Dot3(&hingeAxisA, &hingeAxisB);
	pResult->C = 1.0f - cosAngle;

	rdVector3 rotationAxis;
	rdVector_Cross3(&rotationAxis, &hingeAxisA, &hingeAxisB);
	rdVector_Normalize3Acc(&rotationAxis);

	pResult->JrA = rotationAxis;
	pResult->JrB = rotationAxis;
	rdVector_Neg3Acc(&pResult->JrA);

	// Compute relative orientation of the bodies
	//rdMatrix34 invMat;
	//rdMatrix_InvertOrtho34(&invMat, &pConstraint->targetThing->lookOrientation);
	//
	//rdMatrix34 relativeRotation;
	//rdMatrix_Multiply34(&relativeRotation, &invMat, &pConstraint->constrainedThing->lookOrientation);
	//
	//// calculate the rotated hinge axis
	//rdVector3 rotatedHingeAxisA, rotatedHingeAxisB;
	//rdMatrix_TransformVector34(&rotatedHingeAxisA, &pConstraint->hingeParams.targetAxis, &relativeRotation);
	//rdMatrix_TransformVector34(&rotatedHingeAxisB, &pConstraint->hingeParams.jointAxis, &relativeRotation);
	//
	//// Compute the cosine of the angle between the two hinge axes
	//float twistAngle = rdVector_Dot3(&rotatedHingeAxisA, &rotatedHingeAxisB);
	//if (twistAngle < pConstraint->hingeParams.minCosAngle)
	//{
	//	// Push up towards the minAngle
	//	pResult->C += pConstraint->hingeParams.minCosAngle - twistAngle;
	//}
	//else if (twistAngle > pConstraint->hingeParams.maxCosAngle)
	//{
	//	// Pull down towards the maxAngle
	//	pResult->C += twistAngle - pConstraint->hingeParams.maxCosAngle;
	//}
	
	pResult->C = (jkPlayer_puppetAngBias / deltaSeconds) * pResult->C;
	pResult->C = stdMath_Clamp(pResult->C, -2.0f, 2.0f);
	
	//// Combine swing and twist Jacobians
	//// JrA and JrB already hold contributions from swing; add twist contribution
	//rdVector3 twistAxis = hingeAxisA; // The hinge axis contributes to twist
	//rdVector_MultAcc3(&pResult->JrA, &twistAxis, -twistAngle); // Add twist correction to body A
	//rdVector_MultAcc3(&pResult->JrB, &twistAxis,  twistAngle); // Add twist correction to body B
}

// Projected Gauss-Seidel
void sithConstraint_SatisfyConstraints(sithThing* thing, int iterations, float deltaSeconds)
{
	for (int iter = 0; iter < iterations; iter++)
	{
		// try to bail early if we've converged enough
		int allConverged = 1;

		sithConstraint* c = thing->constraints;
		for (; c; c = c->next)
		{
			if(stdMath_Fabs(c->result.C) < 0.001f)
				continue;
			
			sithThing* bodyA = c->targetThing;
			sithThing* bodyB = c->constrainedThing;

			// Compute J * v
			float Jv = rdVector_Dot3(&c->result.JvA, &bodyA->physicsParams.vel) +
				rdVector_Dot3(&c->result.JrA, &bodyA->physicsParams.rotVel) +
				rdVector_Dot3(&c->result.JvB, &bodyB->physicsParams.vel) +
				rdVector_Dot3(&c->result.JrB, &bodyB->physicsParams.rotVel);
			Jv = stdMath_ClipPrecision(Jv);
			if (stdMath_Fabs(Jv) < 0.001f)
				continue;

			// Compute corrective impulse
			float deltaLambda = (c->result.C - Jv) * c->effectiveMass;
			deltaLambda *= 0.8; // todo: what's a good damping value?
			deltaLambda = stdMath_ClipPrecision(deltaLambda);

			if(stdMath_Fabs(deltaLambda) < 0.0005f)
				continue;

			// Add deltaLambda to the current lambda
			float newLambda = c->lambda + deltaLambda;
			//newLambda = stdMath_Clamp(newLambda, -100.0f, 100.0f);//c->lambdaMin, c->lambdaMax);
			deltaLambda = newLambda - c->lambda;
			c->lambda = newLambda;

			// Apply impulse to body A
			rdVector3 linearImpulseA, angularImpulseA;
			rdVector_Scale3(&linearImpulseA, &c->result.JvA, deltaLambda / c->targetThing->physicsParams.mass);
			rdVector_Scale3(&angularImpulseA, &c->result.JrA, deltaLambda / c->targetThing->physicsParams.inertia);

			// Apply impulse to body B
			rdVector3 linearImpulseB, angularImpulseB;
			rdVector_Scale3(&linearImpulseB, &c->result.JvB, deltaLambda / c->constrainedThing->physicsParams.mass);
			rdVector_Scale3(&angularImpulseB, &c->result.JrB, deltaLambda / c->constrainedThing->physicsParams.inertia);

			rdVector_Add3Acc(&bodyA->physicsParams.vel, &linearImpulseA);
			rdVector_Add3Acc(&bodyA->physicsParams.rotVel, &angularImpulseA);

			rdVector_Add3Acc(&bodyB->physicsParams.vel, &linearImpulseB);
			rdVector_Add3Acc(&bodyB->physicsParams.rotVel, &angularImpulseB);

			allConverged = 0;
		}

		if(allConverged)
			break;
	}
}

void sithConstraint_ApplyFriction(sithThing* pThing, float deltaSeconds)
{
	// apply friction as impulses
	sithConstraint* constraint = pThing->constraints;
	for (; constraint; constraint = constraint->next)
	{
		if (constraint->flags & SITH_CONSTRAINT_DISABLED)
			continue;

		float invMassA = 1.0f / constraint->targetThing->physicsParams.mass;
		float invMassB = 1.0f / constraint->constrainedThing->physicsParams.mass;
		float totalInvMass = invMassA + invMassB;

		rdVector3 omegaRel;
		rdVector_Sub3(&omegaRel, &constraint->targetThing->physicsParams.rotVel, &constraint->constrainedThing->physicsParams.rotVel);
		rdVector_Scale3Acc(&omegaRel, jkPlayer_puppetFriction / totalInvMass);

		rdVector3 impulseA, impulseB;
		rdVector_Scale3(&impulseA, &omegaRel, -invMassA);
		rdVector_Scale3(&impulseB, &omegaRel, invMassB);

		rdVector_Add3Acc(&constraint->targetThing->physicsParams.rotVel, &impulseA);
		rdVector_Add3Acc(&constraint->constrainedThing->physicsParams.rotVel, &impulseB);
	}
}

void sithConstraint_TickConstraints(sithThing* pThing, float deltaSeconds)
{
	if (pThing->physicsParams.physflags & SITH_PF_RESTING)
		return;

	if (!pThing->constraints || !pThing->sector)
		return;

	// try to skip satisfying constraints if everything is mostly resting
	int atRest = 1;

	float iterationStep = deltaSeconds;
	sithConstraint* constraint = pThing->constraints;
	for (; constraint; constraint = constraint->next)
	{
		memset(&constraint->result, 0, sizeof(sithConstraintResult));
		if (constraint->flags & SITH_CONSTRAINT_DISABLED)
			continue;

		constraint->lambda *= 0.9f;
		switch (constraint->type)
		{
		case SITH_CONSTRAINT_DISTANCE:
			sithConstraint_SolveDistanceConstraint(&constraint->result, constraint, iterationStep);
			break;
		case SITH_CONSTRAINT_CONE:
			sithConstraint_SolveConeConstraint(&constraint->result, constraint, iterationStep);
			break;
		case SITH_CONSTRAINT_HINGE:
			sithConstraint_SolveHingeConstraint(&constraint->result, constraint, iterationStep);
			break;
		default:
			break;
		}
		constraint->result.C = stdMath_ClipPrecision(constraint->result.C);
		constraint->effectiveMass = sithConstraint_ComputeEffectiveMass(constraint);

		if(stdMath_Fabs(constraint->result.C) > 0.005f)
			atRest = 0;
	}

	if(!atRest)
	{
		int iterations = (pThing->isVisible + 1) == bShowInvisibleThings ? 10 : 1;
		sithConstraint_SatisfyConstraints(pThing, iterations, deltaSeconds);
		sithConstraint_ApplyFriction(pThing, deltaSeconds);
	}
	else
	{
		pThing->physicsParams.physflags |= SITH_PF_RESTING;
	}
}

static void sithConstraint_DrawDistance(sithConstraint* pConstraint)
{
	rdVector3 targetAnchor;
	pConstraint->targetThing->lookOrientation.scale = pConstraint->targetThing->position;
	rdMatrix_TransformPoint34(&targetAnchor, &pConstraint->distanceParams.targetAnchor, &pConstraint->targetThing->lookOrientation);
	rdVector_Zero3(&pConstraint->targetThing->lookOrientation.scale);

	rdVector3 constrainedAnchor;
	pConstraint->constrainedThing->lookOrientation.scale = pConstraint->constrainedThing->position;
	rdMatrix_TransformPoint34(&constrainedAnchor, &pConstraint->distanceParams.constraintAnchor, &pConstraint->constrainedThing->lookOrientation);
	rdVector_Zero3(&pConstraint->constrainedThing->lookOrientation.scale);

	for (int i = 0; i < 2; ++i)
	{
		rdSprite debugSprite;
		rdSprite_NewEntry(&debugSprite, "dbgragoll", 0, i == 0 ? "saberred0.mat" : "saberblue0.mat", 0.005f, 0.005f, RD_GEOMODE_TEXTURED, RD_LIGHTMODE_FULLYLIT, RD_TEXTUREMODE_AFFINE, 1.0f, &rdroid_zeroVector3);

		rdThing debug;
		rdThing_NewEntry(&debug, pConstraint->parentThing);
		rdThing_SetSprite3(&debug, &debugSprite);
		rdMatrix34 mat;
		rdMatrix_BuildTranslate34(&mat, i == 0 ? &targetAnchor : &constrainedAnchor);

		rdSprite_Draw(&debug, &mat);

		rdSprite_FreeEntry(&debugSprite);
		rdThing_FreeEntry(&debug);
	}
}

static void sithConstraint_DrawCone(sithConstraint* pConstraint)
{
	rdVector3 coneAxis;
	rdMatrix_TransformVector34(&coneAxis, &pConstraint->coneParams.coneAxis, &pConstraint->targetThing->lookOrientation);
	rdVector_Normalize3Acc(&coneAxis);

	rdVector3 thingAxis;
	rdMatrix_TransformVector34(&thingAxis, &pConstraint->coneParams.jointAxis, &pConstraint->constrainedThing->lookOrientation);

	for (int i = 0; i < 2; ++i)
	{
		const char* mat0;
		const char* mat1;
		if (i == 0)
		{
			mat0 = "saberyellow0.mat";
			mat1 = "saberyellow1.mat";
		}
		else if (i == 1)
		{
			mat0 = "saberred0.mat";
			mat1 = "saberred1.mat";
		}

		float len = 0.01f;
		float sizex = 0.002f;
		float sizey = 0.002f;
		rdVector3 lookPos;
		rdVector_Add3(&lookPos, &pConstraint->targetThing->position, i == 0 ? &coneAxis : &thingAxis);

		if (i == 0)
		{
			sizey = 0.05f * (pConstraint->coneParams.coneAngle / 180.0f);
			len = 0.01f;
		}

		rdPolyLine debugLine;
		_memset(&debugLine, 0, sizeof(rdPolyLine));
		if (rdPolyLine_NewEntry(&debugLine, "dbgragoll", mat1, mat0, len, sizex, sizey, RD_GEOMODE_TEXTURED, RD_LIGHTMODE_FULLYLIT, RD_TEXTUREMODE_PERSPECTIVE, 1.0f))
		{
			rdThing debug;
			rdThing_NewEntry(&debug, pConstraint->parentThing);
			rdThing_SetPolyline(&debug, &debugLine);

			rdMatrix34 look;
			rdMatrix_LookAt(&look, &pConstraint->targetThing->position, &lookPos, 0.0f);

			rdPolyLine_Draw(&debug, &look);

			rdPolyLine_FreeEntry(&debugLine);
			rdThing_FreeEntry(&debug);
		}
	}
}

void sithConstraint_Draw(sithConstraint* pConstraint)
{
	switch (pConstraint->type)
	{
		case SITH_CONSTRAINT_DISTANCE:
			sithConstraint_DrawDistance(pConstraint);
			break;
		case SITH_CONSTRAINT_CONE:
			sithConstraint_DrawCone(pConstraint);
			break;
		case SITH_CONSTRAINT_HINGE:
			// todo
			break;
		default:
			break;
	}
}

#endif

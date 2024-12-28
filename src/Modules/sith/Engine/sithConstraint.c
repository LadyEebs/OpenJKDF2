#include "sithConstraint.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"

#include "General/stdMath.h"
#include "Primitives/rdMath.h"
#include "Primitives/rdQuat.h"

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

void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pConeAnchor, const rdVector3* pAxis, float angle, const rdVector3* pJointAxis)
{
	sithConstraint* constraint = (sithConstraint*)pSithHS->alloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_CONE;
	constraint->parentThing = pThing;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

	float s, c;
	stdMath_SinCos(angle * 0.5f, &s, &c);

	constraint->coneParams.coneAnchor = *pConeAnchor;
	constraint->coneParams.coneAxis = *pAxis;
	constraint->coneParams.coneAngle = angle * 0.5f;
	constraint->coneParams.coneAngleCos = c;
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
	constraint->hingeParams.minAngle = cosf(minAngle * (M_PI / 180.0f));
	constraint->hingeParams.maxAngle = cosf(maxAngle * (M_PI / 180.0f));

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

void sithConstraint_ApplyFrictionToRotationalImpulses(
	const rdVector3* pOmegaRel,
	float frictionCoefficient,
	rdVector3* pImpulseA,
	rdVector3* pImpulseB
)
{
	// Calculate the magnitude of the relative angular velocity
	if (rdVector_Len3(pOmegaRel) > 0.0f)
	{
		// Compute the frictional torque: opposite direction of relative angular velocity
		rdVector3 frictionTorque;
		rdVector_Scale3(&frictionTorque, pOmegaRel, frictionCoefficient);

		// Add the frictional torque to the impulse
		rdVector_Add3Acc(pImpulseA, &frictionTorque);
		rdVector_Sub3Acc(pImpulseB, &frictionTorque);
	}
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
	//pResult->C = stdMath_Clamp(pResult->C, -2.0f, 2.0f); // todo: what's a good clamp value?

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

	// cone anchor to world space
	rdVector3 coneAnchor;
	rdMatrix_TransformVector34(&coneAnchor, &pConstraint->coneParams.coneAnchor, &pConstraint->targetThing->lookOrientation);
	rdVector_Add3Acc(&coneAnchor, &pConstraint->targetThing->position);

	// joint axis to world space
	rdVector3 thingAxis;
	rdMatrix_TransformVector34(&thingAxis, &pConstraint->coneParams.jointAxis, &pConstraint->constrainedThing->lookOrientation);
	rdVector_Normalize3Acc(&thingAxis);

#if 1
	sithThing* bodyA = pConstraint->targetThing;
	sithThing* bodyB = pConstraint->constrainedThing;
	
	rdVector3 relativeAxis;
	//rdVector_Cross3(&relativeAxis, &coneAxis, &thingAxis);
	rdVector_Cross3(&relativeAxis, &thingAxis, &coneAxis);
	rdVector_Normalize3Acc(&relativeAxis);

	float dotProduct = rdVector_Dot3(&coneAxis, &thingAxis);
	dotProduct = stdMath_Clamp(dotProduct, -1.0f, 1.0f); // just in case cuz we're catching problems with NaN
	if (dotProduct >= pConstraint->coneParams.coneAngleCos)
	{
		pResult->C = 0.0f;
		return;
	}

	float angle = acosf(dotProduct);

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

#else
	// calculate the angle and test for violation
	float angle = rdVector_Dot3(&coneAxis, &thingAxis);
	angle = stdMath_ClipPrecision(angle);
	if (angle <= pConstraint->coneParams.coneAngleCos)
	{
		rdVector3 normal;
		rdVector_Cross3(&normal, &thingAxis, &coneAxis);
		rdVector_Normalize3Acc(&normal);

		rdMatrix34 qm;
		rdMatrix_BuildFromVectorAngle34(&qm, &normal, pConstraint->coneParams.coneAngle);
		//rdMatrix_Normalize34(&qm);

		//rdQuat q;
		//rdQuat_BuildFromAxisAngle(&q, &normal, pConstraint->coneParams.coneAngle);

		rdVector3 coneVector;
		//rdQuat_TransformVector(&coneVector, &q, &coneAxis);
		rdMatrix_TransformVector34(&coneVector, &coneAxis, &qm);
		rdVector_Normalize3Acc(&coneVector);
	
		rdVector3 hitNormal;
		rdVector_Cross3(&hitNormal, &coneVector, &coneAxis);
		rdVector_Cross3(&normal, &hitNormal, &coneVector);
		rdVector_Normalize3Acc(&normal);

		rdVector3 p1 = coneAnchor;
		//rdVector_Sub3(&p1, &coneAnchor, &pConstraint->constrainedThing->position);
	//	rdVector_MultAcc3(&p1, &coneVector, pConstraint->constrainedThing->moveSize);
		rdVector_Add3Acc(&p1, &coneVector);
		rdVector_Sub3Acc(&p1, &pConstraint->constrainedThing->position);

		//rdVector_Neg3Acc(&normal);

		rdVector_Copy3(&pConeResult->JvB, &normal);
		rdVector_Cross3(&pConeResult->JrB, &p1, &normal);

		pConeResult->C = (jkPlayer_puppetAngBias / deltaSeconds) * rdVector_Dot3(&normal, &thingAxis);
		//pConeResult->C = rdVector_Dot3(&normal, &thingAxis) ;// / (180.0f / M_PI);
		pConeResult->C = stdMath_ClipPrecision(pConeResult->C);

		rdVector_Neg3Acc(&normal);

		rdVector3 p2 = coneAnchor;
		//rdVector_Sub3(&p2, &coneAnchor, &pConstraint->targetThing->position);
		//rdVector_MultAcc3(&p2, &coneVector, pConstraint->targetThing->moveSize);
		rdVector_Add3Acc(&p2, &coneVector);
		rdVector_Sub3Acc(&p2, &pConstraint->targetThing->position);

		rdVector_Copy3(&pConeResult->JvA, &normal);
		rdVector_Cross3(&pConeResult->JrA, &p2, &normal);
	}
	else
	{
		pConeResult->C = 0.0f;
	}
#endif
}

static void sithConstraint_SolveHingeConstraint(sithConstraintResult* pResult, sithConstraint* pConstraint, float deltaSeconds)
{
	rdVector3 hingeAxisA, hingeAxisB;
	
	rdMatrix_TransformVector34(&hingeAxisA, &pConstraint->hingeParams.targetAxis, &pConstraint->targetThing->lookOrientation);
	rdMatrix_TransformVector34(&hingeAxisB, &pConstraint->hingeParams.jointAxis, &pConstraint->constrainedThing->lookOrientation);
	
	float cosAngle = rdVector_Dot3(&hingeAxisA, &hingeAxisB);

	// Step 3: Calculate the constraint violation C
	rdVector3 axisOfRotation;  // Axis around which the constraint operates
	rdVector_Cross3(&axisOfRotation, &hingeAxisA, &hingeAxisB);  // Cross product to get the axis of rotation
	rdVector_Normalize3Acc(&axisOfRotation);  // Normalize to get the direction of the axis

	// Determine the constraint value (C) – this represents the violation (error) between the current configuration and the desired constraint
	pResult->C = 1.0f - cosAngle;  // This is the angle between the two hinge axes (could use an angle limit)

	// Step 5: Apply angular limits
	if (cosAngle > pConstraint->hingeParams.minAngle)
		pConstraint->result.C = cosAngle - pConstraint->hingeParams.minAngle;
	else if (cosAngle < pConstraint->hingeParams.maxAngle)
		pConstraint->result.C = cosAngle - pConstraint->hingeParams.maxAngle;
	else
	{
		// No correction needed if the angle is within the limits
		pConstraint->result.C = 0;
		return;
	}

	// Step 4: Compute Jacobians
	pResult->JrA = axisOfRotation;  // Angular velocity Jacobian for body A
	pResult->JrB = axisOfRotation;  // Angular velocity Jacobian for body B
	//rdVector_Neg3Acc(&pResult->JrB);  // Angular velocity Jacobian for body B

	rdVector_Zero3(&pResult->JvA);
	rdVector_Zero3(&pResult->JvB);
}

// Projected Gauss-Seidel
void sithConstraint_SatisfyConstraints(sithThing* thing, int iterations, float deltaSeconds)
{
	for (int iter = 0; iter < iterations; iter++)
	{
		// try to bail early if we've converged enough
		int allConverged = 1;

		sithConstraint* c = thing->constraints;
		while (c)
		{
			if(stdMath_Fabs(c->result.C) < 0.001)
			{
				c = c->next;
				continue;
			}
			
			sithThing* bodyA = c->targetThing;
			sithThing* bodyB = c->constrainedThing;

			// Compute J * v
			float Jv = rdVector_Dot3(&c->result.JvA, &bodyA->physicsParams.vel) +
				rdVector_Dot3(&c->result.JrA, &bodyA->physicsParams.rotVel) +
				rdVector_Dot3(&c->result.JvB, &bodyB->physicsParams.vel) +
				rdVector_Dot3(&c->result.JrB, &bodyB->physicsParams.rotVel);
			Jv = stdMath_ClipPrecision(Jv);

			if (stdMath_Fabs(Jv) < 0.001)
			{
				c = c->next;
				continue;
			}

			// Compute corrective impulse
			float deltaLambda = (c->result.C - Jv) * c->effectiveMass;
			deltaLambda *= 0.8; // todo: what's a good damping value?
			deltaLambda = stdMath_ClipPrecision(deltaLambda);

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

			// Friction
			rdVector3 omega_rel;
			rdVector_Sub3(&omega_rel, &c->constrainedThing->physicsParams.rotVel, &c->targetThing->physicsParams.rotVel);
			sithConstraint_ApplyFrictionToRotationalImpulses(&omega_rel, jkPlayer_puppetFriction, &angularImpulseA, &angularImpulseB);
		
			rdVector_Add3Acc(&bodyA->physicsParams.vel, &linearImpulseA);
			rdVector_Add3Acc(&bodyA->physicsParams.rotVel, &angularImpulseA);

			rdVector_Add3Acc(&bodyB->physicsParams.vel, &linearImpulseB);
			rdVector_Add3Acc(&bodyB->physicsParams.rotVel, &angularImpulseB);

			allConverged = 0;

			c = c->next;
		}

		if(allConverged)
			break;
	}
}

void sithConstraint_SolveConstraints(sithThing* pThing, float deltaSeconds)
{
	if (pThing->constraints && pThing->sector)
	{
		if (pThing->physicsParams.atRest)
			return;
		
		// try to skip satisfying constraints if everything is mostly resting
		int atRest = 1;

		float iterationStep = deltaSeconds;
		sithConstraint* constraint = pThing->constraints;
		while (constraint)
		{
			memset(&constraint->result, 0, sizeof(sithConstraintResult));
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

			if(stdMath_Fabs(constraint->result.C) > 0.001)
				atRest = 0;

			constraint = constraint->next;
		}

		if(!atRest)
		{
			int iterations = (pThing->isVisible + 1) == bShowInvisibleThings ? 10 : 1;
			sithConstraint_SatisfyConstraints(pThing, iterations, deltaSeconds);
		}
	}
}

#endif

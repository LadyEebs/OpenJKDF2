#include "sithConstraint.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"

#include "General/stdMath.h"
#include "Primitives/rdMath.h"
#include "Primitives/rdQuat.h"

#include "Primitives/rdSprite.h"
#include "Primitives/rdPolyline.h"
#include "Engine/rdThing.h"
#ifdef JOB_SYSTEM
#include "Modules/std/stdJob.h"
#endif

#ifdef PUPPET_PHYSICS

#ifdef JOB_SYSTEM
#define USE_JOBS 0 // not faster atm
#else
#define USE_JOBS 0
#endif

extern float jkPlayer_puppetAngBias;
extern float jkPlayer_puppetPosBias;
extern float jkPlayer_puppetFriction;

void sithConstraint_InitConstraint(sithConstraint* pConstraint, int type, sithThing* pParentThing, sithThing* pConstrainedThing, sithThing* pTargetThing)
{
	pConstraint->type = type;
	pConstraint->flags = 0;
	pConstraint->parentThing = pParentThing;
	pConstraint->constrainedThing = pConstrainedThing;
	pConstraint->targetThing = pTargetThing;

	pConstraint->next = pParentThing->constraints;
	pParentThing->constraints = pConstraint;
	
	pConstrainedThing->constraintParent = pTargetThing;
	pConstrainedThing->constraintRoot = pParentThing;
}

void sithConstraint_AddBallSocketConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAnchor, const rdVector3* pConstrainedAnchor, float distance)
{
	sithBallSocketConstraint* constraint = (sithBallSocketConstraint*)pSithHS->alloc(sizeof(sithBallSocketConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithBallSocketConstraint));
	sithConstraint_InitConstraint(&constraint->base, SITH_CONSTRAINT_BALLSOCKET, pThing, pConstrainedThing, pTargetThing);

	constraint->targetAnchor = *pTargetAnchor;
	constraint->constraintAnchor = *pConstrainedAnchor;
}

void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAxis, float angle, const rdVector3* pJointAxis)
{
	sithConeLimitConstraint* constraint = (sithConeLimitConstraint*)pSithHS->alloc(sizeof(sithConeLimitConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConeLimitConstraint));
	sithConstraint_InitConstraint(&constraint->base, SITH_CONSTRAINT_CONE, pThing, pConstrainedThing, pTargetThing);

	constraint->coneAxis = *pAxis;
	constraint->coneAngle = angle * 0.5f;
	constraint->coneAngleCos = stdMath_Cos(angle * 0.5f);
	constraint->jointAxis = *pJointAxis;
}

void sithConstraint_AddHingeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAxis, const rdVector3* pJointAxis, float minAngle, float maxAngle)
{
	sithHingeLimitConstraint* constraint = (sithHingeLimitConstraint*)pSithHS->alloc(sizeof(sithHingeLimitConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithHingeLimitConstraint));
	sithConstraint_InitConstraint(&constraint->base, SITH_CONSTRAINT_HINGE, pThing, pConstrainedThing, pTargetThing);

	constraint->targetAxis = *pTargetAxis;
	constraint->jointAxis = *pJointAxis;
	constraint->minCosAngle = minAngle;//stdMath_Cos(minAngle * 0.5f);
	constraint->maxCosAngle = maxAngle;//stdMath_Cos(maxAngle * 0.5f);
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

static void sithConstraint_SolveBallSocketConstraint(sithBallSocketConstraint* pConstraint, float deltaSeconds)
{
	sithConstraintResult* pResult = &pConstraint->base.result;

	sithThing* bodyA = pConstraint->base.targetThing;
	sithThing* bodyB = pConstraint->base.constrainedThing;

	// anchor A offset in world space
	rdVector3 offsetA;
	rdMatrix_TransformVector34(&offsetA, &pConstraint->targetAnchor, &bodyA->lookOrientation);

	// anchor B offset in world space
	rdVector3 offsetB;
	rdMatrix_TransformVector34(&offsetB, &pConstraint->constraintAnchor, &bodyB->lookOrientation);

	// anchor positions in world space
	rdVector3 anchorA, anchorB;
	rdVector_Add3(&anchorA, &offsetA, &bodyA->position);
	rdVector_Add3(&anchorB, &offsetB, &bodyB->position);

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

static void sithConstraint_SolveConeConstraint(sithConeLimitConstraint* pConstraint, float deltaSeconds)
{
	sithConstraintResult* pResult = &pConstraint->base.result;

	sithThing* bodyA = pConstraint->base.targetThing;
	sithThing* bodyB = pConstraint->base.constrainedThing;

	// cone axis to world space
	rdVector3 coneAxis;
	rdMatrix_TransformVector34(&coneAxis, &pConstraint->coneAxis, &bodyA->lookOrientation);
	rdVector_Normalize3Acc(&coneAxis);

	// joint axis to world space
	rdVector3 thingAxis;
	rdMatrix_TransformVector34(&thingAxis, &pConstraint->jointAxis, &bodyB->lookOrientation);
	rdVector_Normalize3Acc(&thingAxis);
	
	rdVector3 relativeAxis;
	//rdVector_Cross3(&relativeAxis, &coneAxis, &thingAxis);
	rdVector_Cross3(&relativeAxis, &thingAxis, &coneAxis);
	rdVector_Normalize3Acc(&relativeAxis);

	float dotProduct = rdVector_Dot3(&coneAxis, &thingAxis);
	if (dotProduct >= pConstraint->coneAngleCos)
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
	pResult->C = pConstraint->coneAngleCos - dotProduct;
	pResult->C = (jkPlayer_puppetAngBias / deltaSeconds) * pResult->C;
	pResult->C = stdMath_Clamp(pResult->C, -2.0f, 2.0f);
}

static void sithConstraint_SolveHingeConstraint(sithHingeLimitConstraint* pConstraint, float deltaSeconds)
{
	sithConstraintResult* pResult = &pConstraint->base.result;
	
	sithThing* bodyA = pConstraint->base.targetThing;
	sithThing* bodyB = pConstraint->base.constrainedThing;

	rdVector_Zero3(&pResult->JvA);
	rdVector_Zero3(&pResult->JvB);

	rdVector3 hingeAxisA, hingeAxisB;	
	rdMatrix_TransformVector34(&hingeAxisA, &pConstraint->targetAxis, &bodyA->lookOrientation);
	rdMatrix_TransformVector34(&hingeAxisB, &pConstraint->jointAxis, &bodyB->lookOrientation);

	float cosAngle = rdVector_Dot3(&hingeAxisA, &hingeAxisB);
	pResult->C = 1.0f - cosAngle;

	rdVector3 rotationAxis;
	rdVector_Cross3(&rotationAxis, &hingeAxisA, &hingeAxisB);
	rdVector_Normalize3Acc(&rotationAxis);

	pResult->JrA = rotationAxis;
	pResult->JrB = rotationAxis;
	rdVector_Neg3Acc(&pResult->JrA);

//	float dotA = rdVector_Dot3(&rotationAxis, &hingeAxisA);
//	float dotB = rdVector_Dot3(&rotationAxis, &hingeAxisB);
//	float twistAngle = stdMath_ArcTan3(dotA, dotB);
//	if (twistAngle < pConstraint->minCosAngle)
//	{
//		// Push up towards the minAngle
//		pResult->C += (pConstraint->minCosAngle - twistAngle) * (M_PI / 180.0f);
//	}
//	else if (twistAngle > pConstraint->maxCosAngle)
//	{
//		// Pull down towards the maxAngle
//		pResult->C += (twistAngle - pConstraint->maxCosAngle) * (M_PI / 180.0f);
//	}
//	
//	rdVector3 twistAxis = hingeAxisA; // The hinge axis contributes to twist
//	rdVector_MultAcc3(&pResult->JrA, &twistAxis, -twistAngle * (M_PI / 180.0f)); // Add twist correction to body A
//	rdVector_MultAcc3(&pResult->JrB, &twistAxis,  twistAngle * (M_PI / 180.0f)); // Add twist correction to body B
//	rdVector_Normalize3Acc(&pResult->JrA);
//	rdVector_Normalize3Acc(&pResult->JrB);
	
	pResult->C = (jkPlayer_puppetAngBias / deltaSeconds) * pResult->C;
	pResult->C = stdMath_Clamp(pResult->C, -2.0f, 2.0f);
}

void sithConstraint_ApplyImpulses(sithThing* pThing)
{
	sithConstraint* constraint = pThing->constraints;
	for (; constraint; constraint = constraint->next)
	{
		if (constraint->flags & SITH_CONSTRAINT_DISABLED)
			continue;
		
		if (constraint->result.C == 0.0f)
			continue;

		rdVector_Add3Acc(&constraint->targetThing->physicsParams.vel, &constraint->result.linearImpulseA);
		rdVector_Add3Acc(&constraint->targetThing->physicsParams.rotVel, &constraint->result.angularImpulseA);

		rdVector_Add3Acc(&constraint->constrainedThing->physicsParams.vel, &constraint->result.linearImpulseB);
		rdVector_Add3Acc(&constraint->constrainedThing->physicsParams.rotVel, &constraint->result.angularImpulseB);
	}
}

int sithConstraint_SatisfyConstraint(sithConstraint* c, float deltaSeconds)
{
	if (stdMath_Fabs(c->result.C) < 0.001f)
		return 0;

	sithThing* bodyA = c->targetThing;
	sithThing* bodyB = c->constrainedThing;

	// Compute J * v
	float Jv = rdVector_Dot3(&c->result.JvA, &bodyA->physicsParams.vel) +
		rdVector_Dot3(&c->result.JrA, &bodyA->physicsParams.rotVel) +
		rdVector_Dot3(&c->result.JvB, &bodyB->physicsParams.vel) +
		rdVector_Dot3(&c->result.JrB, &bodyB->physicsParams.rotVel);
	Jv = stdMath_ClipPrecision(Jv);
	if (stdMath_Fabs(Jv) < 0.001f)
		return 0;

	// Compute corrective impulse
	float deltaLambda = (c->result.C - Jv) * c->effectiveMass;
	deltaLambda *= 0.8; // todo: what's a good damping value?
	deltaLambda = stdMath_ClipPrecision(deltaLambda);

	if (stdMath_Fabs(deltaLambda) < 0.0005f)
		return 0;

	// Add deltaLambda to the current lambda
	float newLambda = c->lambda + deltaLambda;
	//newLambda = stdMath_Clamp(newLambda, -100.0f, 100.0f);//c->lambdaMin, c->lambdaMax);
	deltaLambda = newLambda - c->lambda;
	c->lambda = newLambda;

	// Apply impulse to body A
	rdVector_Scale3(&c->result.linearImpulseA, &c->result.JvA, deltaLambda / c->targetThing->physicsParams.mass);
	rdVector_Scale3(&c->result.angularImpulseA, &c->result.JrA, deltaLambda / c->targetThing->physicsParams.inertia);

	// Apply impulse to body B
	rdVector_Scale3(&c->result.linearImpulseB, &c->result.JvB, deltaLambda / c->constrainedThing->physicsParams.mass);
	rdVector_Scale3(&c->result.angularImpulseB, &c->result.JrB, deltaLambda / c->constrainedThing->physicsParams.inertia);

	return 1;
}

#if USE_JOBS
void sithConstraint_SatisfyConstraintJob(void* arg)
{
	sithConstraint* c = (sithConstraint*)arg;
	sithConstraint_SatisfyConstraint(c, sithTime_deltaSeconds);
}
#endif

// Projected Gauss-Seidel
void sithConstraint_SatisfyConstraints(sithThing* thing, int iterations, float deltaSeconds)
{
	// each iteration is dependent on the previous result, so we can't thread that part
	for (int iter = 0; iter < iterations; iter++)
	{
		sithConstraint* c = thing->constraints;
		for (; c; c = c->next)
		{
		#if USE_JOBS
			stdJob_Execute(sithConstraint_SatisfyConstraintJob, c);
		#else
			sithConstraint_SatisfyConstraint(c, deltaSeconds);
		#endif
		}
	#if USE_JOBS
		stdJob_Wait();
	#endif
		sithConstraint_ApplyImpulses(thing);
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

void sithConstraint_SolveConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	memset(&pConstraint->result, 0, sizeof(sithConstraintResult));
	if (pConstraint->flags & SITH_CONSTRAINT_DISABLED)
		return;

	pConstraint->lambda *= 0.9f;
	switch (pConstraint->type)
	{
	case SITH_CONSTRAINT_BALLSOCKET:
		sithConstraint_SolveBallSocketConstraint((sithBallSocketConstraint*)pConstraint, deltaSeconds);
		break;
	case SITH_CONSTRAINT_CONE:
		sithConstraint_SolveConeConstraint((sithConeLimitConstraint*)pConstraint, deltaSeconds);
		break;
	case SITH_CONSTRAINT_HINGE:
		sithConstraint_SolveHingeConstraint((sithHingeLimitConstraint*)pConstraint, deltaSeconds);
		break;
	default:
		break;
	}
	pConstraint->result.C = stdMath_ClipPrecision(pConstraint->result.C);
	pConstraint->effectiveMass = sithConstraint_ComputeEffectiveMass(pConstraint);
}

#if USE_JOBS
void sithConstraint_SolveConstraintJob(void* arg)
{
	sithConstraint* pConstraint = (sithConstraint*)arg;
	sithConstraint_SolveConstraint(pConstraint, sithTime_deltaSeconds);
}
#endif

void sithConstraint_TickConstraints(sithThing* pThing, float deltaSeconds)
{
	if (pThing->physicsParams.physflags & SITH_PF_RESTING)
		return;

	if (!pThing->constraints || !pThing->sector)
		return;

	sithConstraint* pConstraint = pThing->constraints;
	for (; pConstraint; pConstraint = pConstraint->next)
	{
	#if USE_JOBS
		stdJob_Execute(sithConstraint_SolveConstraintJob, pConstraint);
	#else
		sithConstraint_SolveConstraint(pConstraint, deltaSeconds);
	#endif
	}

#if USE_JOBS
	stdJob_Wait();
#endif
	
	int iterations = (pThing->isVisible + 1) == bShowInvisibleThings ? 10 : 1;
	sithConstraint_SatisfyConstraints(pThing, iterations, deltaSeconds);
	sithConstraint_ApplyFriction(pThing, deltaSeconds);
}

static void sithConstraint_DrawBallSocket(sithBallSocketConstraint* pConstraint)
{
	sithThing* bodyA = pConstraint->base.targetThing;
	sithThing* bodyB = pConstraint->base.constrainedThing;

	rdVector3 targetAnchor;
	bodyA->lookOrientation.scale = bodyA->position;
	rdMatrix_TransformPoint34(&targetAnchor, &pConstraint->targetAnchor, &bodyA->lookOrientation);
	rdVector_Zero3(&bodyA->lookOrientation.scale);

	rdVector3 constrainedAnchor;
	bodyB->lookOrientation.scale = bodyB->position;
	rdMatrix_TransformPoint34(&constrainedAnchor, &pConstraint->constraintAnchor, &bodyB->lookOrientation);
	rdVector_Zero3(&bodyB->lookOrientation.scale);

	for (int i = 0; i < 2; ++i)
	{
		rdSprite debugSprite;
		rdSprite_NewEntry(&debugSprite, "dbgragoll", 0, i == 0 ? "saberred0.mat" : "saberblue0.mat", 0.005f, 0.005f, RD_GEOMODE_TEXTURED, RD_LIGHTMODE_FULLYLIT, RD_TEXTUREMODE_AFFINE, 1.0f, &rdroid_zeroVector3);

		rdThing debug;
		rdThing_NewEntry(&debug, pConstraint->base.parentThing);
		rdThing_SetSprite3(&debug, &debugSprite);
		rdMatrix34 mat;
		rdMatrix_BuildTranslate34(&mat, i == 0 ? &targetAnchor : &constrainedAnchor);

		rdSprite_Draw(&debug, &mat);

		rdSprite_FreeEntry(&debugSprite);
		rdThing_FreeEntry(&debug);
	}
}

static void sithConstraint_DrawCone(sithConeLimitConstraint* pConstraint)
{
	sithThing* bodyA = pConstraint->base.targetThing;
	sithThing* bodyB = pConstraint->base.constrainedThing;

	rdVector3 coneAxis;
	rdMatrix_TransformVector34(&coneAxis, &pConstraint->coneAxis, &bodyA->lookOrientation);
	rdVector_Normalize3Acc(&coneAxis);

	rdVector3 thingAxis;
	rdMatrix_TransformVector34(&thingAxis, &pConstraint->jointAxis, &bodyB->lookOrientation);

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
		rdVector_Add3(&lookPos, &bodyA->position, i == 0 ? &coneAxis : &thingAxis);

		if (i == 0)
		{
			sizey = 0.05f * (pConstraint->coneAngle / 180.0f);
			len = 0.01f;
		}

		rdPolyLine debugLine;
		_memset(&debugLine, 0, sizeof(rdPolyLine));
		if (rdPolyLine_NewEntry(&debugLine, "dbgragoll", mat1, mat0, len, sizex, sizey, RD_GEOMODE_TEXTURED, RD_LIGHTMODE_FULLYLIT, RD_TEXTUREMODE_PERSPECTIVE, 1.0f))
		{
			rdThing debug;
			rdThing_NewEntry(&debug, pConstraint->base.parentThing);
			rdThing_SetPolyline(&debug, &debugLine);

			rdMatrix34 look;
			rdMatrix_LookAt(&look, &bodyA->position, &lookPos, 0.0f);

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
		case SITH_CONSTRAINT_BALLSOCKET:
			sithConstraint_DrawBallSocket(pConstraint);
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

void sithConstraint_DebugDrawConstraints(sithThing* pThing)
{
	sithConstraint* constraint = pThing->constraints;
	for (; constraint; constraint = constraint->next)
		sithConstraint_Draw(constraint);
}

#endif

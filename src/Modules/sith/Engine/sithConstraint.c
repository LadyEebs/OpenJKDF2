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

#ifdef RIGID_BODY

void sithConstraint_AddDistanceConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAnchor, const rdVector3* pConstrainedAnchor, float distance)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_DISTANCE;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;
	constraint->distanceParams.targetAnchor = *pTargetAnchor;
	constraint->distanceParams.constraintAnchor = *pConstrainedAnchor;
	constraint->distanceParams.constraintDistance = distance;//rdVector_Len3(&constraint->distanceParams.constraintAnchor);
	constraint->distanceParams.prevLambda = 0.0f;
	
	constraint->constrainedThing->constraintParent = pTargetThing;

 	//constraint->next = pThing->constraints;
	//pThing->constraints = constraint;

	// new
	constraint->thing = pConstrainedThing;
	constraint->thing->constraintParent = pTargetThing;
	
	constraint->next = pTargetThing->constraints;
	pTargetThing->constraints = constraint;
}

void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pConeAnchor, const rdVector3* pAxis, float angle, const rdVector3* pJointAxis)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_CONE;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

	float s, c;
	stdMath_SinCos(angle * 0.5f, &s, &c);

	rdVector_Zero3(&constraint->coneParams.prevImpulse);
	constraint->coneParams.coneAnchor = *pConeAnchor;
	constraint->coneParams.coneAxis = *pAxis;
	constraint->coneParams.coneAngle = angle * 0.5f;
	constraint->coneParams.coneAngleCos = c;
	constraint->coneParams.jointAxis = *pJointAxis;

	constraint->constrainedThing->constraintParent = pTargetThing;

	//constraint->next = pThing->constraints;
	//pThing->constraints = constraint;

	// new
	constraint->thing = pConstrainedThing;
	constraint->thing->constraintParent = pTargetThing;

	constraint->next = pTargetThing->constraints;
	pTargetThing->constraints = constraint;
}


void sithConstraint_AddAngleConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pMinAngles, const rdVector3* pMaxAngles)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_ANGLES;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

	constraint->angleParams.minAngles = *pMinAngles;
	constraint->angleParams.maxAngles = *pMaxAngles;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;

}

void sithConstraint_AddLookConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, const rdMatrix34* pRefMat, int flipUp)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_LOOK;
	constraint->constrainedThing = pThingA;
	constraint->targetThing = pThingB;
	rdMatrix_Copy34(&constraint->lookParams.referenceMat, pRefMat);
	constraint->lookParams.flipUp = flipUp;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}


void sithConstraint_AddTwistConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAxis, float angle)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;
	memset(constraint, 0, sizeof(sithConstraint));

	constraint->type = SITH_CONSTRAINT_TWIST;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

	rdVector_Zero3(&constraint->coneParams.prevImpulse);
	constraint->twistParams.twistAxis = *pAxis;
	constraint->twistParams.maxTwistAngle = angle;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}

float sithConstraint_ComputeEffectiveMass(rdVector3* r1, rdVector3* r2, sithThing* bodyA, sithThing* bodyB)
{
	float invMassA = 1.0f / bodyA->physicsParams.mass;
	float invMassB = 1.0f / bodyB->physicsParams.mass;
	float invInertiaA = 1.0f / bodyA->physicsParams.inertia;
	float invInertiaB = 1.0f / bodyB->physicsParams.inertia;
	float JWJT = invMassA + invMassB +
		invInertiaA * rdVector_Dot3(r1, r1) +
		invInertiaB * rdVector_Dot3(r2, r2);

	return 1.0f / JWJT;
}

#if 0

static void sithConstraint_SolveDistanceConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	sithThing* bodyA = pConstraint->targetThing;
	sithThing* bodyB = pConstraint->constrainedThing;

	rdVector3 r1, r2;
	rdMatrix_TransformVector34(&r1, &pConstraint->distanceParams.targetAnchor, &bodyA->lookOrientation);
	rdMatrix_TransformVector34(&r2, &pConstraint->distanceParams.constraintAnchor, &bodyB->lookOrientation);

	// Compute position and velocity constraints
	rdVector3 p2_plus_r2, p1_plus_r1;
	rdVector_Add3(&p2_plus_r2, &bodyB->position, &r2);
	rdVector_Add3(&p1_plus_r1, &bodyA->position, &r1);

	// Position constraint
	rdVector3 C;
	rdVector_Sub3(&C, &p2_plus_r2, &p1_plus_r1);

	// Compute relative velocity at pivot points
	rdVector3 omega2_cross_r2, omega1_cross_r1;
	rdVector_Cross3(&omega2_cross_r2, &bodyB->physicsParams.accAngularForces, &r2);
	rdVector_Cross3(&omega1_cross_r1, &bodyB->physicsParams.accAngularForces, &r1);

	rdVector3 v1, v2;
	rdVector_Add3(&v1, &bodyA->physicsParams.accLinearForces, &bodyA->physicsParams.vel);
	rdVector_Add3(&v2, &bodyB->physicsParams.accLinearForces, &bodyB->physicsParams.vel);

	rdVector3 v2_plus_omega2_cross_r2, v1_plus_omega1_cross_r1;
	rdVector_Add3(&v2_plus_omega2_cross_r2 , &v2, &omega2_cross_r2);
	rdVector_Add3(&v1_plus_omega1_cross_r1 , &v1, &omega1_cross_r1);

	rdVector3 dCdt;
	rdVector_Sub3(&dCdt, &v2_plus_omega2_cross_r2, &v1_plus_omega1_cross_r1);
	
	// Compute the effective mass matrix
	float Me = sithConstraint_ComputeEffectiveMass(&r1, &r2, bodyA, bodyB);

	// Compute the Lagrange multiplier (lambda)
	float beta = jkPlayer_puppetPosBias;
	rdVector3 rhs = {
		-dCdt.x - (beta * C.x / deltaSeconds),
		-dCdt.y - (beta * C.y / deltaSeconds),
		-dCdt.z - (beta * C.z / deltaSeconds)
	};

	rdVector3 lambda = {
		rhs.x * Me,
		rhs.y * Me,
		rhs.z * Me
	};

	float invMassA = 1.0f / bodyA->physicsParams.mass;
	float invMassB = 1.0f / bodyB->physicsParams.mass;
	float invInertiaA = 1.0f / bodyA->physicsParams.inertia;
	float invInertiaB = 1.0f / bodyB->physicsParams.inertia;

	rdVector3 impulseA = { -lambda.x, -lambda.y, -lambda.z };
	rdVector3 impulseB = {  lambda.x,  lambda.y,  lambda.z };

	bodyA->physicsParams.accLinearForces.x -= lambda.x * invMassA* deltaSeconds;
	bodyA->physicsParams.accLinearForces.y -= lambda.y * invMassA* deltaSeconds;
	bodyA->physicsParams.accLinearForces.z -= lambda.z * invMassA* deltaSeconds;
	
	bodyB->physicsParams.accLinearForces.x += lambda.x * invMassB * deltaSeconds;
	bodyB->physicsParams.accLinearForces.y += lambda.y * invMassB * deltaSeconds;
	bodyB->physicsParams.accLinearForces.z += lambda.z * invMassB * deltaSeconds;

		// Convert angular accelerations to local coordinates
	rdMatrix34 invOrientationA, invOrientationB;
	rdMatrix_InvertOrtho34(&invOrientationA, &bodyA->lookOrientation);
	rdMatrix_InvertOrtho34(&invOrientationB, &bodyB->lookOrientation);

	rdVector3 c1, c2;
	rdVector_Cross3(&c1, &r1, &impulseA);
	rdVector_Cross3(&c2, &r2, &impulseB);

	rdVector3 a1, a2;
	a1.x = invInertiaA * c1.x* deltaSeconds;
	a1.y = invInertiaA * c1.y* deltaSeconds;
	a1.z = invInertiaA * c1.z* deltaSeconds;
	a2.x = invInertiaB * c2.x* deltaSeconds;
	a2.y = invInertiaB * c2.y* deltaSeconds;
	a2.z = invInertiaB * c2.z* deltaSeconds;

	bodyA->physicsParams.accAngularForces.x += a1.x;
	bodyA->physicsParams.accAngularForces.y += a1.y;
	bodyA->physicsParams.accAngularForces.z += a1.z;
	
	bodyB->physicsParams.accAngularForces.x += a2.x;
	bodyB->physicsParams.accAngularForces.y += a2.y;
	bodyB->physicsParams.accAngularForces.z += a2.z;
}

static void sithConstraint_SolveConeConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
}

static void sithConstraint_SolveAngleConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
}

static void sithConstraint_SolveLookConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
}

static void sithConstraint_SolveTwistConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
}

void sithConstraint_ApplyConstraint(sithThing* pThing, sithConstraint* constraint, float deltaSeconds)
{
}

void sithConstraint_SolveConstraints(sithThing* pThing, float deltaSeconds)
{
	if (pThing->constraints)
	{
		// iteratively solve constraints
		for (int k = 0; k < 10; ++k)
		{
			sithConstraint* constraint = pThing->constraints;
			while (constraint)
			{
				switch (constraint->type)
				{
				case SITH_CONSTRAINT_DISTANCE:
					sithConstraint_SolveDistanceConstraint(constraint, deltaSeconds / 10.0f);
					break;
				case SITH_CONSTRAINT_CONE:
					break;
				case SITH_CONSTRAINT_ANGLES:
					break;
				case SITH_CONSTRAINT_LOOK:
					break;
				case SITH_CONSTRAINT_TWIST:
					break;
				default:
					break;
				}
				sithConstraint_ApplyConstraint(pThing, constraint, deltaSeconds / 10.0f);
				constraint = constraint->next;
			}

			// apply the corrections
			uint64_t jointBits = pThing->animclass->physicsJointBits;
			while (jointBits != 0)
			{
				int jointIdx = stdMath_FindLSB64(jointBits);
				jointBits ^= 1ull << jointIdx;

				sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];

				//sithPhysics_ApplyDrag(&pJoint->things[0].physicsParams.accLinearForces, 1.0f, 0.0f, deltaSeconds);
				rdVector_MultAcc3(&pJoint->things[0].position, &pJoint->things[0].physicsParams.accLinearForces, 1.0f);//deltaSeconds);

				if(pJoint->things[0].physicsParams.physflags & SITH_PF_4000000)
				{
					//sithPhysics_ApplyDrag(&pJoint->things[0].physicsParams.accAngularForces, 1.0f, 0.0f, deltaSeconds);

				// rotate the thing
				rdVector3 angles;
				rdVector_Scale3(&angles, &pJoint->things[0].physicsParams.accAngularForces, 1.0f);//deltaSeconds);

				// build a rotation matrix from the angular impulse
				rdVector3 axis;
				float angle = rdVector_Normalize3(&axis, &angles) * (180.0f / M_PI);

				rdMatrix34 rotMatrix;
				rdMatrix_BuildFromVectorAngle34(&rotMatrix, &axis, angle);

				// rotate it to the local frame
				rdMatrix34 invOrientation;
				rdMatrix_InvertOrtho34(&invOrientation, &pJoint->things[0].lookOrientation);
				rdMatrix_PostMultiply34(&rotMatrix, &invOrientation);

				//rdMatrix34 rotMatrix;
				//rdMatrix_BuildRotate34(&rotMatrix, &angles);
				sithCollision_sub_4E7670(&pJoint->things[0], &rotMatrix);
				rdMatrix_Normalize34(&pJoint->things[0].lookOrientation);
				}

				// reset the forces
				rdVector_Zero3(&pJoint->things[0].physicsParams.accLinearForces);
				rdVector_Zero3(&pJoint->things[0].physicsParams.accAngularForces);
			}
		}

		// finally apply the solution with collision at the end
		uint64_t jointBits = pThing->animclass->physicsJointBits;
		while (jointBits != 0)
		{
			int jointIdx = stdMath_FindLSB64(jointBits);
			jointBits ^= 1ull << jointIdx;

			sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];

			rdVector3 dir;
			rdVector_Sub3(&dir, &pJoint->things[0].position, &pJoint->things[0].physicsParams.lastPos);
			if (!rdVector_IsZero3(&dir))
			{
				float dist = rdVector_Normalize3Acc(&dir);
				pJoint->things[0].position = pJoint->things[0].physicsParams.lastPos;
				sithCollision_UpdateThingCollision(&pJoint->things[0], &dir, dist, 0);

				//rdVector_Add3Acc(&pJoint->things[0].physicsParams.vel, &dir);
			}
		}

		sithPuppet_BuildJointMatrices(pThing);
	}
}

#else

void compute_angular_velocity_from_rotation(rdVector3* omega_rel, const rdMatrix34* R_rel)
{
	float angle;
	rdVector3 axis;
	rdMatrix_ExtractAxisAngle34(R_rel, &axis, &angle);

	float theta = angle * M_PI / 180.0f;

	// Compute angular velocity: omega_rel = (theta / deltaTime) * rotationAxis
	omega_rel->x = (theta) * axis.x;
	omega_rel->y = (theta) * axis.y;
	omega_rel->z = (theta) * axis.z;

//	rdMatrix34 R_diff = *R_rel;
//	R_diff.rvec.x -= 1.0f;
//	R_diff.lvec.y -= 1.0f;
//	R_diff.uvec.z -= 1.0f;
//
//	// Use the skew-symmetric matrix to extract angular velocity (ignoring higher-order terms for small rotations)
//	omega_rel->x = 0.5f * (R_diff.uvec.y - R_diff.lvec.z);
//	omega_rel->y = 0.5f * (R_diff.rvec.z - R_diff.uvec.x);
//	omega_rel->z = 0.5f * (R_diff.lvec.x - R_diff.rvec.y);
}

void compute_twist_component(rdVector3* result, rdVector3* omega_rel, rdVector3* jointAxisB)
{
	float dot_product = rdVector_Dot3(omega_rel, jointAxisB);
	rdVector_Scale3(result, jointAxisB, dot_product);
}

void apply_friction_to_rotational_impulse(const rdVector3* omega_rel, float friction_coefficient,
										  rdVector3* impulseA, rdVector3* impulseB)
{
	// Calculate the magnitude of the relative angular velocity (omega_rel)
	float omega_magnitude = rdVector_Len3(omega_rel);
	if (omega_magnitude > 0.0f)
	{
		// Compute the frictional torque: opposite direction of relative angular velocity
		rdVector3 friction_torque;
		rdVector_Scale3(&friction_torque, omega_rel, friction_coefficient);// * omega_magnitude / omega_magnitude);

		// Add the frictional torque to the impulse
		rdVector_Add3Acc(impulseA, &friction_torque);
		rdVector_Sub3Acc(impulseB, &friction_torque);
	}
}

void ApplyImpulse(sithThing* pThing, const rdVector3* linearImpulse, const rdVector3* angularImpulse)
{
	rdVector3 dampedLinearImpulse = *linearImpulse;

	// apply impulse to the velocity
	//rdVector_MultAcc3(&pThing->physicsParams.vel, &dampedLinearImpulse, 1.0);
	//pThing->physicsParams.physflags |= SITH_PF_HAS_FORCE;

	// apply impulse directly to the position of the thing
	// todo: accumulate position changes and do this once per frame or iteration instead of on every constraint, it's expensive
//	sithCollision_UpdateThingCollision(pThing, &dampedLinearImpulse, sithTime_deltaSeconds*rdVector_Normalize3Acc(&dampedLinearImpulse), 0);//SITH_RAYCAST_IGNORE_THINGS | RAYCAST_4);
//	rdVector_MultAcc3(&pThing->position, &dampedLinearImpulse, sithTime_deltaSeconds);

	// apply impulse to an accumulator
	rdVector_MultAcc3(&pThing->physicsParams.accLinearForces, &dampedLinearImpulse, 1.0f);//sithTime_deltaSeconds);

	// ignore angular impulses if there isn't any
	if(rdVector_IsZero3(angularImpulse))
		return;

	rdVector3 dampedAngularImpulse = *angularImpulse;

	//rdVector3 angVel;
	//sithPhysics_AngularVelocityToAngles(&angVel, &dampedAngularImpulse, &pThing->lookOrientation);
	rdVector_Add3Acc(&pThing->physicsParams.accAngularForces, &dampedAngularImpulse);





//	// local frame, to get local orientation for the thing
//	rdMatrix34 invObjectOrientation;
//	rdMatrix_InvertOrtho34(&invObjectOrientation, &pThing->physicsParams.lastOrient);//lookOrientation);
//
//	rdVector3 localAngImpulse;
//	rdMatrix_TransformVector34(&localAngImpulse, &dampedAngularImpulse, &invObjectOrientation);
//
//	rdVector_Scale3Acc(&localAngImpulse, sithTime_deltaSeconds);
//
//	// build a rotation matrix from the angular impulse
//	rdVector3 axis;
//	float angle = rdVector_Normalize3(&axis, &localAngImpulse) * (180.0f / M_PI);
//		
//	rdMatrix34 rotMatrix;
//	rdMatrix_BuildFromVectorAngle34(&rotMatrix, &axis, angle);
//	//rdMatrix_Normalize34(&rotMatrix); // rdMatrix_BuildFromVectorAngle34 doesn't generate normalized matrices? No idea
//
//	// rotate it to the local frame
//	//rdMatrix_PostMultiply34(&rotMatrix, &invObjectOrientation);
//
//	//rdMatrix_PostMultiply34(&pThing->physicsParams.accAngularForces, &rotMatrix);
//
//	// apply rotation directly to the thing
//	//sithCollision_sub_4E7670(pThing, &rotMatrix);
//	rdMatrix_PreMultiply34(&pThing->lookOrientation, &rotMatrix);
//	rdMatrix_Normalize34(&pThing->lookOrientation);

	// apply rotation to the angular velocity
	// again for whatever reason this needs to be scaled by invMass and inertia or it explodes???
//	rdVector3 angles;
//	rdMatrix_ExtractAngles34(&rotMatrix, &angles);
//	rdVector_MultAcc3(&pThing->physicsParams.accAngularForces, &angles, 1.0f);
	
	//rdVector_MultAcc3(&pThing->physicsParams.angVel, &angles, invMass * pThing->physicsParams.inertia);
}

static void sithConstraint_SolveDistanceConstraint(sithConstraintResult* pResult, sithConstraint* pConstraint, float deltaSeconds)
{
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
	{
		pResult->C = 0.0f;
		return;
	}

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


#if 1
	// vector between the anchors and the distance
	rdVector3 constraint, unitConstraint;
	rdVector_Sub3(&constraint, &anchorB, &anchorA);
	float len = rdVector_Normalize3(&unitConstraint , &constraint);

	// calculate constraint force, add bias for error correction
	pResult->C = len - pConstraint->distanceParams.constraintDistance;
	pResult->C = -(jkPlayer_puppetPosBias / deltaSeconds) * pResult->C;
//	pResult->C=-pResult->C; // no correction
//	pConstraint->C = stdMath_Clamp(pConstraint->C, -2.0f, 2.0f);

	// Calculate Jacobians
	
	if (1)//!(pConstraint->targetThing->physicsParams.physflags & SITH_PF_2000000))
	{
		pResult->JvA.x = -unitConstraint.x;
		pResult->JvA.y = -unitConstraint.y;
		pResult->JvA.z = -unitConstraint.z;
	}
	else
		pResult->JvA = rdroid_zeroVector3;

	pResult->JvB.x = unitConstraint.x;
	pResult->JvB.y = unitConstraint.y;
	pResult->JvB.z = unitConstraint.z;

	if(1)//pConstraint->constrainedThing->physicsParams.physflags & SITH_PF_4000000)
	{
		rdVector_Cross3(&pResult->JrB, &offsetB, &unitConstraint);
		if (1)//!(pConstraint->targetThing->physicsParams.physflags & SITH_PF_2000000))
		{
			rdVector_Neg3Acc(&unitConstraint);
			rdVector_Cross3(&pResult->JrA, &offsetA, &unitConstraint);
		}
		else
			rdVector_Zero3(&pResult->JrA);
	}
	else
	{
		rdVector_Zero3(&pResult->JrA);
		rdVector_Zero3(&pResult->JrB);

	}

#else

	rdVector3 relativePos;
	rdVector_Sub3(&relativePos, &targetAnchor, &constrainedAnchor);

	float currentDistance = rdVector_Len3(&relativePos);
	float offset = pConstraint->distanceParams.constraintDistance - currentDistance;
	offset = stdMath_ClipPrecision(offset);
	if (stdMath_Fabs(offset) <= 0.0f)
		return;

	rdVector3 offsetDir;
	rdVector_Normalize3(&offsetDir, &relativePos);

	rdVector3 relativeVelocity;
	rdVector_Sub3(&relativeVelocity, &pConstraint->targetThing->physicsParams.vel, &pConstraint->constrainedThing->physicsParams.vel);

	// Apply spring force
	float stiffness = 0.5f;
	rdVector3 springForce;
	rdVector_Scale3(&springForce, &offsetDir, -stiffness * offset);
	
	// Apply damping force
	float dampingFactor = 0.2f;
	float dampingForceMag = dampingFactor * rdVector_Dot3(&relativeVelocity, &offsetDir);
	rdVector3 dampingForce;
	rdVector_Scale3(&dampingForce, &offsetDir, -dampingForceMag);
	
	rdVector3 totalForce;
	rdVector_Add3(&totalForce, &springForce, &dampingForce);

	// how much of their relative force is affecting the constraint
	float velocityDot = rdVector_Dot3(&relativeVelocity, &offsetDir);
	velocityDot = stdMath_ClipPrecision(velocityDot);

	const float biasFactor = jkPlayer_puppetPosBias;
	float bias = -(biasFactor / deltaSeconds) * offset;
	bias = stdMath_ClipPrecision(bias);

	float lambda = pConstraint->distanceParams.prevLambda;
	lambda = -(velocityDot + bias) / constraintMass;
	lambda = stdMath_ClipPrecision(lambda);

	lambda *= 1.0 - dampingFactor;

	//lambda = offset / constraintMass;

//	// Calculate the relative velocity
//	float dC = velocityDot;
//	float C = -offset;
//	// Calculate the Baumgarte stabilization term
//	float beta = 0.5f; // Velocity correction coefficient
//	float gamma = 0.1f; // Position correction coefficient
//	float baumgarteTerm = (beta * dC + gamma * C) / deltaSeconds;
//	float lambda = baumgarteTerm / constraintMass;

	rdVector3 aImpulse;
	rdVector_Scale3(&aImpulse, &offsetDir, lambda);

	rdVector3 bImpulse;
	rdVector_Scale3(&bImpulse, &offsetDir, -lambda);

	// Apply total force to impulses
	//rdVector_Add3Acc(&aImpulse, &totalForce);
	//rdVector_Sub3Acc(&bImpulse, &totalForce);

	//rdVector_Scale3Acc(&aImpulse, invMassA);
	//rdVector_Scale3Acc(&bImpulse, invMassB);




//	rdVector_Neg3Acc(&offsetDir);
//	sithCollision_UpdateThingCollision(pConstraint->targetThing, &offsetDir, -lambda * invMassA,  0);
//	rdVector_Neg3Acc(&offsetDir);
//	sithCollision_UpdateThingCollision(pConstraint->constrainedThing, &offsetDir, -lambda * invMassB, 0 );

	rdVector_MultAcc3(&pConstraint->targetThing->physicsParams.vel, &aImpulse, invMassA);
	rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.vel, &bImpulse, invMassB);
	

	//sithPhysics_ThingApplyForce(pConstraint->targetThing, &aImpulse);
	//sithPhysics_ThingApplyForce(pConstraint->constrainedThing, &bImpulse);

	rdVector_Scale3Acc(&aImpulse, invMassA);
	rdVector_Scale3Acc(&bImpulse, invMassB);

	if(pConstraint->constrainedThing->physicsParams.physflags & SITH_PF_4000000)
	{
		//sithPhysics_ThingApplyRotForce(pConstraint->targetThing, &centerToAnchorA, &aImpulse);
		//sithPhysics_ThingApplyRotForce(pConstraint->constrainedThing, &centerToAnchorB, &bImpulse);

		//sithConstraint_RotateThingTest(pConstraint->targetThing, &targetAnchor, &aImpulse);
		//sithConstraint_RotateThingTest(pConstraint->constrainedThing, &constrainedAnchor, &bImpulse);

	//	rdMatrix34 invOrientConstrained;
	//	rdMatrix_InvertOrtho34(&invOrientConstrained, &pConstraint->constrainedThing->lookOrientation);
	//	
	//	rdMatrix34 invOrientTarget;
	//	rdMatrix_InvertOrtho34(&invOrientTarget, &pConstraint->targetThing->lookOrientation);
	//	
	//	// Calculate relative orientation
	//	rdMatrix34 relativeOrientation;
	//	rdMatrix_Multiply34(&relativeOrientation, &invOrientTarget, &pConstraint->constrainedThing->lookOrientation);
	//	
	//	// Extract relative angles
	//	rdVector3 relativeAngles;
	//	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);
	//	
	//	// Clamp the angles to the feasible set
	//	rdVector3 projectedAngles = relativeAngles;
	//	//rdVector_Clamp3(&projectedAngles, &pConstraint->angleParams.minAngles, &pConstraint->angleParams.maxAngles);
	//	rdVector_ClampRange3(&projectedAngles, -15.0f, 15.0f);
	//
	//	// Calculate angle error and bias term
	//	rdVector3 angleError;
	//	rdVector_Sub3(&angleError, &relativeAngles, &projectedAngles);
	//	
	//	// Apply bias
	//	rdVector3 biasTerm;
	//	rdVector_Scale3(&biasTerm, &angleError, 0.5 / deltaSeconds);
	//	
	//	// Calculate angular correction in PYR
	//	rdVector3 angularCorrection = angleError;
	//	//rdVector_Add3(&angularCorrection, &angleError, &biasTerm);
	//	//rdVector_InvScale3Acc(&angularCorrection, correctionFactor);
	//	
	//	// Apply the correction to angular velocity
	//	rdVector3 correctionParent;
	//	rdVector_Scale3(&correctionParent, &angularCorrection, 1.0f / pConstraint->targetThing->physicsParams.mass);
	////	rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionParent);
	//	
	//	rdVector3 correctionChild;
	//	rdVector_Scale3(&correctionChild, &angularCorrection, -1.0f / pConstraint->constrainedThing->physicsParams.mass);
	////	rdVector_Sub3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionChild);
	
		sithPhysics_ThingApplyRotForce(pConstraint->targetThing, &targetAnchor, &aImpulse, jkPlayer_puppetAngBias);
		sithPhysics_ThingApplyRotForce(pConstraint->constrainedThing, &constrainedAnchor, &bImpulse, jkPlayer_puppetAngBias);

		// friction
	//	rdVector3 relativeAngVel;
	//	//rdVector_Sub3(&relativeAngVel, &pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->targetThing->physicsParams.angVel);
	//
	//	rdVector3 worldTargetAngVel;
	//	rdMatrix_TransformVector34(&worldTargetAngVel, &pConstraint->targetThing->physicsParams.angVel, &pConstraint->targetThing->lookOrientation);
	//
	//	rdVector3 worldConstrainedAngVel;
	//	rdMatrix_TransformVector34(&worldConstrainedAngVel, &pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->constrainedThing->lookOrientation);
	//
	//	rdVector_Sub3(&relativeAngVel, &worldConstrainedAngVel, &worldTargetAngVel);
	//
	//	float angFriction = 0.5f;
	//	rdVector_Scale3Acc(&relativeAngVel, angFriction / constraintMass);
	//
	//	rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.angVel, &relativeAngVel, -invMassB);
	//	rdVector_MultAcc3(&pConstraint->targetThing->physicsParams.angVel, &relativeAngVel, invMassA);
	}


//	rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.vel, &aImpulse);
//	rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.vel, &bImpulse);

	// Apply velocity correction to maintain the contact point

	//VectorAdd(&constraint->bodyA->velocity, &constraint->bodyA->velocity, &velocityCorrection);
	//VectorSubtract(&constraint->bodyB->velocity, &constraint->bodyB->velocity, &velocityCorrection);


	if (0)//pConstraint->constrainedThing->physicsParams.physflags & SITH_PF_4000000)
	{
		rdVector3 centerToAnchorA, centerToAnchorB;
		rdVector_Sub3(&centerToAnchorA, &targetAnchor, &pConstraint->targetThing->position);
		rdVector_Sub3(&centerToAnchorB, &constrainedAnchor, &pConstraint->constrainedThing->position);
	
		// Calculate and apply angular corrections
		rdVector3 torqueA, torqueB;
		rdVector_Cross3(&torqueA, &centerToAnchorA, &aImpulse);
		rdVector_Cross3(&torqueB, &centerToAnchorB, &bImpulse);
		
	//	rdVector3 angularAccelA, angularAccelB;
	//	rdVector_Scale3(&angularAccelA, &torqueA, (180.0f / M_PI) / inertiaA);
	//	rdVector_Scale3(&angularAccelB, &torqueB, (180.0f / M_PI) / inertiaB);
		

		// Calculate angular lambda
		rdVector3 angularLambdaA, angularLambdaB;
		rdVector_Scale3(&angularLambdaA, &torqueA, 1.0f);// / pConstraint->targetThing->physicsParams.inertia);
		rdVector_Scale3(&angularLambdaB, &torqueB, 1.0f);// / pConstraint->constrainedThing->physicsParams.inertia);

		// Calculate angular bias
		const float angularBiasFactor = jkPlayer_puppetAngBias;
		// Adjust as needed
		rdVector3 angularBiasA, angularBiasB;
		rdVector_Scale3(&angularBiasA, &angularLambdaA, angularBiasFactor / deltaSeconds);
		rdVector_Scale3(&angularBiasB, &angularLambdaB, angularBiasFactor / deltaSeconds);

		// Apply angular bias
		rdVector_Add3Acc(&angularLambdaA, &angularBiasA);
		rdVector_Add3Acc(&angularLambdaB, &angularBiasB);
		
		rdVector_Scale3Acc(&angularLambdaA, (180.0f / M_PI));
		rdVector_Scale3Acc(&angularLambdaB, (180.0f / M_PI));

		// Apply angular damping
		const float angularDampingFactor = 0.3f;
		angularLambdaA.x *= (1.0f - angularDampingFactor);
		angularLambdaA.y *= (1.0f - angularDampingFactor);
		angularLambdaA.z *= (1.0f - angularDampingFactor);
		
		angularLambdaB.x *= (1.0f - angularDampingFactor);
		angularLambdaB.y *= (1.0f - angularDampingFactor);
		angularLambdaB.z *= (1.0f - angularDampingFactor);
		
		// Convert angular accelerations to local coordinates
		rdMatrix34 invTargetOrientation, invConstrainedOrientation;
		rdMatrix_InvertOrtho34(&invTargetOrientation, &pConstraint->targetThing->lookOrientation);
		rdMatrix_InvertOrtho34(&invConstrainedOrientation, &pConstraint->constrainedThing->lookOrientation);

		rdVector3 localAngularAccelA, localAngularAccelB;
		rdMatrix_TransformVector34(&localAngularAccelA, &angularLambdaA, &invTargetOrientation);
		rdMatrix_TransformVector34(&localAngularAccelB, &angularLambdaB, &invConstrainedOrientation);

		//rdVector_ClampRange3(&localAngularAccelA, -15.0f, 15.0f);
		//rdVector_ClampRange3(&localAngularAccelB, -15.0f, 15.0f);


	//	rdQuat qA, qB;
	//	rdQuat_BuildFromAxisAngle(&qA, &localAngularAccelA, rdVector_Normalize3Acc(&localAngularAccelA) * deltaSeconds);
	//	rdQuat_BuildFromAxisAngle(&qB, &localAngularAccelB, rdVector_Normalize3Acc(&localAngularAccelB) * deltaSeconds);
	//
	//	rdQuat_NormalizeAcc(&qA);
	//	rdQuat_NormalizeAcc(&qB);
	//
	//	if (rdQuat_IsZero(&pConstraint->targetThing->physicsParams.angVelQ))
	//		pConstraint->targetThing->physicsParams.angVelQ = qA;
	//	else
	//		rdQuat_MulAcc(&pConstraint->targetThing->physicsParams.angVelQ, &qA);
	//
	//	if (rdQuat_IsZero(&pConstraint->constrainedThing->physicsParams.angVelQ))
	//		pConstraint->constrainedThing->physicsParams.angVelQ = qB;
	//	else
	//		rdQuat_MulAcc(&pConstraint->constrainedThing->physicsParams.angVelQ, &qB);


		// Apply the angular acceleration to the thing's angular velocity
		rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &localAngularAccelA);
		rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &localAngularAccelB);



//// Apply the rotational relationship correction to the velocity
//rdVector3 relativeContactPoint;
//rdVector_Sub3(&relativeContactPoint, &anchor, & pConstraint->constrainedThing->position);
//
//rdVector3 rotationalCorrection;
//rdVector_Cross3(&rotationalCorrection, &relativeContactPoint, &pConstraint->constrainedThing->physicsParams.angVel);
//rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.vel, &rotationalCorrection);
	
	//rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &angularAccelA);
	}
#endif
}

static void sithPhysics_ThingApplyAngularImpulse(sithThing* pThing, const rdVector3* angularImpulse)
{
	// Calculate the change in angular velocity
	float inertia = (2.0f / 5.0f) * pThing->physicsParams.mass * pThing->collideSize * pThing->collideSize;

	rdVector3 axis;
	float angle = rdVector_Normalize3(&axis, angularImpulse);

	//delta time* (torque / moment of inertia)

	rdMatrix34 impulseMat;
	rdMatrix_BuildFromAxisAngle34(&impulseMat, &axis, angle * 180.0 / M_PI);

	rdVector3 angles;
	rdMatrix_ExtractAngles34(&impulseMat, &angles);

	//rdVector_Add3Acc(&pThing->physicsParams.angVel, &angles);
	rdVector_MultAcc3(&pThing->physicsParams.angVel, &angles, sithTime_deltaSeconds / inertia);
}


static void sithConstraint_SolveConeConstraint(sithConstraintResult* pConeResult, sithConstraintResult* pTwistResult, sithConstraint* pConstraint, float deltaSeconds)
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
	
#if 0
	rdMatrix34 I;
	rdMatrix_InvertOrtho34(&I, &pConstraint->targetThing->lookOrientation);

	rdMatrix34 R_rel;
	rdMatrix_Multiply34(&R_rel, &I, &pConstraint->constrainedThing->lookOrientation);

	rdVector3 omega_rel;
	compute_angular_velocity_from_rotation(&omega_rel, &R_rel);

	float dot_product = rdVector_Dot3(&omega_rel, &coneAxis);
	float cone_angle = acosf(dot_product);

	float cone_error = cone_angle - (pConstraint->coneParams.coneAngle * (M_PI / 180.0f));
	if (cone_error > 0.0f)
	{
		// Project omega_rel onto the plane perpendicular to coneAxisA
		rdVector3 omega_rel_perp;
		rdVector_Cross3(&omega_rel_perp, &omega_rel, &coneAxis);
		rdVector_Normalize3Acc(&omega_rel_perp);

		// Jacobian for body A angular velocity contribution
		pConeResult->JrA = omega_rel_perp;
		rdVector_Neg3Acc(&pConeResult->JrA);

		// Jacobian for body B angular velocity contribution
		pConeResult->JrB = omega_rel_perp;

		// Linear velocity contributions (not relevant for this constraint)
		pConeResult->JvA = rdroid_zeroVector3;
		pConeResult->JvB = rdroid_zeroVector3;

		pConeResult->C = cone_error;
	}
	else
	{
		pConeResult->C = 0.0f;
	}

	//rdVector3 omega_rel_twist;
	//compute_twist_component(&omega_rel_twist, &omega_rel, &coneAxis); // thingAxis
	//
	//float maxTwistAngle = 5.0f * M_PI / 180.0f;
	//float twist_error = acosf(rdVector_Dot3(&omega_rel_twist, &coneAxis)) - maxTwistAngle;
	//if (twist_error > 0.0f)
	//{
	//	pTwistResult->JrA = omega_rel_twist;
	//	pTwistResult->JrB = omega_rel_twist;
	//	pTwistResult->JvA = rdroid_zeroVector3;
	//	pTwistResult->JvB = rdroid_zeroVector3;
	//}

#elif 1
	sithThing* bodyA = pConstraint->targetThing;
	sithThing* bodyB = pConstraint->constrainedThing;
	
	rdVector3 relativeAxis;
	//rdVector_Cross3(&relativeAxis, &coneAxis, &thingAxis);
	rdVector_Cross3(&relativeAxis, &thingAxis, &coneAxis);
	rdVector_Normalize3Acc(&relativeAxis);

	float dotProduct = rdVector_Dot3(&coneAxis, &thingAxis);
	dotProduct = stdMath_Clamp(dotProduct, -1.0f, 1.0f); // just in case cuz we're catching problems with NaN
	if (dotProduct < pConstraint->coneParams.coneAngleCos)
	{
		float angle = acosf(dotProduct);

		rdVector3 correctionAxis;
		rdVector_Cross3(&correctionAxis, &coneAxis, &relativeAxis);
		rdVector_Normalize3Acc(&correctionAxis);

		rdVector3 JwA = relativeAxis;
		rdVector3 JwB = relativeAxis;
		rdVector_Neg3Acc(&JwA);

		pConeResult->JvA = rdroid_zeroVector3;
		pConeResult->JvB = rdroid_zeroVector3;
		pConeResult->JrA = JwA;
		pConeResult->JrB = JwB;
		pConeResult->C = pConstraint->coneParams.coneAngleCos - dotProduct;//(angle - pConstraint->coneParams.coneAngle * (M_PI / 180.0f));

		pConeResult->C = (jkPlayer_puppetAngBias / deltaSeconds) * pConeResult->C;
	}
	else
	{
		pConeResult->C = 0.0f;
	}

	//rdVector3 upVec = pConstraint->targetThing->lookOrientation.uvec;
	//if (stdMath_Fabs(rdVector_Dot3(&coneAxis, &upVec)) > 0.9999f)
	//	upVec = pConstraint->targetThing->lookOrientation.rvec;
	//
	//rdVector3 planeRight;
	//rdVector_Cross3(&planeRight, &coneAxis, &upVec);
	//rdVector_Normalize3Acc(&planeRight);
	//
	//rdVector3 planeUp;
	//rdVector_Cross3(&planeUp, &planeRight, &coneAxis);
	//rdVector_Normalize3Acc(&planeUp);
	
	float twistRange = 5.0f / 180.0 * M_PI;
	//float twistAngle = atan2f(rdVector_Dot3(&thingAxis, &relativeAxis), rdVector_Dot3(&thingAxis, &coneAxis));
	
	rdMatrix34 invOrientTarget;
	rdMatrix_InvertOrtho34(&invOrientTarget, &pConstraint->targetThing->lookOrientation);

	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &invOrientTarget, &pConstraint->constrainedThing->lookOrientation);

//	// Calculate the trace of the matrix
//	float trace = relativeOrientation.rvec.x + relativeOrientation.lvec.y + relativeOrientation.uvec.z;
//	float twistAngle = acosf(fmaxf(fminf((trace - 1.0f) / 2.0f, 1.0f), -1.0f));
//
//	rdVector3 twistAxis;
//	// If the angle is very small (near zero rotation), use thingAxis as the twist axis
//	if (fabs(twistAngle) < 1e-6f)
//	{
//		twistAxis = thingAxis;
//	}
//	else
//	{
//	 // Otherwise, calculate the axis of rotation from the skew-symmetric part of the rotation matrix
//		twistAxis.x = 0.5f * (relativeOrientation.uvec.y - relativeOrientation.lvec.z) / sinf(twistAngle);
//		twistAxis.y = 0.5f * (relativeOrientation.rvec.z - relativeOrientation.uvec.x) / sinf(twistAngle);
//		twistAxis.z = 0.5f * (relativeOrientation.lvec.x - relativeOrientation.rvec.y) / sinf(twistAngle);
//	}
//	rdVector_Normalize3Acc(&twistAxis);
//	
//	
//	float clampedTwistAngle = stdMath_Clamp(twistAngle, -twistRange, twistRange);
//	float twistError = twistAngle - clampedTwistAngle;//copysignf(twistRange, twistAngle);
//	
//	//rdVector3 JwA = thingAxis;
//	//rdVector3 JwB = thingAxis;
//	//rdVector_Neg3Acc(&JwA);
//
//	rdVector3 rA;
//	rdVector_Sub3(&rA, &coneAnchor, &pConstraint->targetThing->position);
//
//	rdVector3 rB;
//	rdVector_Sub3(&rB, &coneAnchor, &pConstraint->constrainedThing->position);
//
//	rdVector3 JwA, JwB;
//	rdVector_Cross3(&JwA, &rA, &thingAxis);  // Jacobian for body A
//	rdVector_Cross3(&JwB, &rB, &thingAxis);  // Jacobian for body B
//	rdVector_Normalize3Acc(&JwA);  // Jacobian for body A
//	rdVector_Normalize3Acc(&JwB);  // Jacobian for body B
//

	rdVector3 rotatedAxis;
	rdMatrix_TransformVector34(&rotatedAxis, &pConstraint->coneParams.coneAxis, &relativeOrientation);
	
	float dotTwist = rdVector_Dot3(&rotatedAxis, &pConstraint->coneParams.jointAxis);
	dotTwist = stdMath_Clamp(dotTwist, -1.0f, 1.0f);
	
	float twistAngle = acosf(dotTwist);

	//rdVector3 twistAxis;  // This should be your cone axis, typically
	//float twistAngle;
	//rdMatrix_ExtractAxisAngle34(&relativeOrientation, &twistAxis, &twistAngle);

	//twistAngle *= (M_PI / 180.0f);

	rdVector3 JwA = thingAxis;
	rdVector3 JwB = thingAxis;
	//rdVector_Neg3Acc(&JwA);


	float twistError = twistAngle - copysignf(twistRange, twistAngle);

	//if(stdMath_Fabs(twistAngle) > twistRange)
	//{
	//
	//	pTwistResult->JvA = rdroid_zeroVector3;
	//	pTwistResult->JvB = rdroid_zeroVector3;
	//	pTwistResult->JrA = JwA;
	//	pTwistResult->JrB = JwB;
	//	pTwistResult->C = 0.5f * twistError;
	//}
	//else
	{
		pTwistResult->C = 0.0f;
	}

	//if (pConstraint->constrainedThing->physicsParams.physflags & SITH_PF_2000000)
	//{
	//	printf("Twist Angle: %f\n", twistAngle);
	//	printf("Twist Error: %f\n", twistError);
	//	printf("JrA: (%f, %f, %f)\n", JwA.x, JwA.y, JwA.z);
	//	printf("JrB: (%f, %f, %f)\n", JwB.x, JwB.y, JwB.z);
	//}
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
	//	rdVector_Add3Acc(&p1, &coneVector);
		rdVector_Sub3Acc(&p1, &pConstraint->constrainedThing->position);

		rdVector_Neg3Acc(&normal);

		rdVector_Copy3(&pConeResult->JvB, &normal);
		rdVector_Cross3(&pConeResult->JrB, &p1, &normal);

		pConeResult->C = (jkPlayer_puppetAngBias / deltaSeconds) * rdVector_Dot3(&normal, &thingAxis);
		//pConeResult->C = rdVector_Dot3(&normal, &thingAxis) ;// / (180.0f / M_PI);
		pConeResult->C = stdMath_ClipPrecision(pConeResult->C);

		rdVector_Neg3Acc(&normal);

		rdVector3 p2 = coneAnchor;
		//rdVector_Sub3(&p2, &coneAnchor, &pConstraint->targetThing->position);
		//rdVector_MultAcc3(&p2, &coneVector, pConstraint->targetThing->moveSize);
	//	rdVector_Add3Acc(&p2, &coneVector);
		rdVector_Sub3Acc(&p2, &pConstraint->targetThing->position);

		rdVector_Copy3(&pConeResult->JvA, &normal);
		rdVector_Cross3(&pConeResult->JrA, &p2, &normal);
	}
	else
	{
		pConeResult->C = 0.0f;
	}

	rdVector3 relativeAxis;
	rdVector_Cross3(&relativeAxis, &thingAxis, &coneAxis);
	rdVector_Normalize3Acc(&relativeAxis);
	
	float twistRange = 5.0f / 180.0 * M_PI;
	//float twistAngle = atan2f(rdVector_Dot3(&thingAxis, &relativeAxis), rdVector_Dot3(&thingAxis, &coneAxis));
	
	rdMatrix34 invOrientTarget;
	rdMatrix_InvertOrtho34(&invOrientTarget, &pConstraint->targetThing->lookOrientation);
	
	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &invOrientTarget, &pConstraint->constrainedThing->lookOrientation);
	
//	rdVector3 rotatedAxis;
//	rdMatrix_TransformVector34(&rotatedAxis, &pConstraint->coneParams.coneAxis, &relativeOrientation);
//	
//	float dotTwist = rdVector_Dot3(&rotatedAxis, &pConstraint->coneParams.jointAxis);
//	dotTwist = stdMath_Clamp(dotTwist, -1.0f, 1.0f);
//	
//	float twistAngle = acosf(dotTwist);
//	
//	rdVector_Neg3Acc(&relativeAxis);
//
	rdVector3 JwA = relativeAxis;
	rdVector3 JwB = relativeAxis;
	rdVector_Neg3Acc(&JwA);
	
//	float twistError = twistAngle - copysignf(twistRange, twistAngle);

	rdVector3 twistAxis;  // This should be your cone axis, typically
	float twistAngle;
	rdMatrix_ExtractAxisAngle34(&relativeOrientation, &twistAxis, &twistAngle);
	twistAngle /= (180.0f / M_PI);

	float twistError = twistAngle - copysignf(twistRange, twistAngle);
	if(stdMath_Fabs(twistAngle) > twistRange)
	{
	
		pTwistResult->JvA = rdroid_zeroVector3;
		pTwistResult->JvB = rdroid_zeroVector3;
		pTwistResult->JrA = JwA;
		pTwistResult->JrB = JwB;
		pTwistResult->C = -(jkPlayer_puppetAngBias / deltaSeconds) * twistError;
	}
	//else
	{
		pTwistResult->C = 0.0f;
	}
#endif

//	rdVector3 p1;
//	rdVector_Add3(&p1, &coneAnchor, &coneVector);
//	//rdVector_Sub3Acc(&p1, &pConstraint->constrainedThing->position);
//
//
//	rdVector3 p2;
//	rdVector_Add3(&p2, &coneAnchor, &coneVector);
//	//rdVector_Sub3Acc(&p2, &pConstraint->targetThing->position);
//
//	rdVector3 impulseA, impulseB;
//	rdVector_Scale3(&impulseA, &normal, invMassA/ constraintMass * 180.0/M_PI);
//	rdVector_Scale3(&impulseB, &normal, -invMassB / constraintMass * 180.0 / M_PI);
//
//	sithPhysics_ThingApplyRotForce(pConstraint->targetThing, &p1, &impulseA, 0);
//	sithPhysics_ThingApplyRotForce(pConstraint->constrainedThing, &p2, &impulseB, 0);


	/*rdVector3 relativeVelocity;
	rdVector_Sub3(&relativeVelocity, &pConstraint->targetThing->physicsParams.vel, &pConstraint->constrainedThing->physicsParams.vel);

	rdVector3 normRelativeVelocity;
	rdVector_Normalize3(&normRelativeVelocity, &relativeVelocity);
	
	float cosTheta = rdVector_Dot3(&normRelativeVelocity, &coneAxis);
	float theta = acosf(cosTheta) * 180.0f / M_PI;
	
	if (theta > pConstraint->coneParams.coneAngle)
	{
		float scaleFactor = cosf(pConstraint->coneParams.coneAngle / (180.0f / M_PI)) / cosTheta;
		rdVector_Scale3Acc(&relativeVelocity, scaleFactor);
		
		rdVector3 correction;
		rdVector_Sub3(&correction, &relativeVelocity, &relativeVelocity);
		
		rdVector_MultAcc3(&pConstraint->targetThing->physicsParams.vel, &correction, -invMassA / constraintMass);

		rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.vel, &correction, invMassB / constraintMass);

	}*/


#if 0
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
	{
		pConstraint->C = 0.0f;
		return;
	}
	float velocityDot = rdVector_Dot3(&relativeVelocity, &coneAxis);

	const float biasFactor = 0.1f;
	float bias = 0.0f;// -(biasFactor / deltaSeconds) * (cosf(pConstraint->coneParams.coneAngle/(180.0*M_PI)) - velocityDot);

	float lambda = -(velocityDot + bias) / constraintMass;

	rdVector3 impulseIncrement;
	rdVector_Scale3(&impulseIncrement, &coneAxis, lambda);

	//rdVector3 currentImpulse;
	//rdVector_Add3(&currentImpulse, &pConstraint->coneParams.prevImpulse, &impulseIncrement);

	rdVector_MultAcc3(&pConstraint->targetThing->physicsParams.vel, &impulseIncrement, invMassB);
	rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.vel, &impulseIncrement, -invMassA);

	//sithPhysics_ThingApplyForce(pConstraint->targetThing, &impulseIncrement);
	// 
	//sithPhysics_ThingApplyForce(pConstraint->constrainedThing, &impulseIncrement);

//	rdVector_Copy3(&pConstraint->coneParams.prevImpulse, &currentImpulse);
#endif

#if 0
//	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
//	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
//	float constraintMass = invMassA + invMassB;
//	if (constraintMass <= 0.0f)
//		return;
//	
//	rdVector3 coneAxis;
//	rdMatrix_TransformVector34(&coneAxis, &pConstraint->coneParams.coneAxis, &pConstraint->targetThing->lookOrientation);
//	rdVector_Normalize3Acc(&coneAxis);
//
//	// Calculate the relative orientation
//	rdVector3 currentDir;
//	rdMatrix_TransformVector34(&currentDir, &coneAxis, &pConstraint->constrainedThing->lookOrientation);
//	rdVector_Normalize3Acc(&currentDir);
//	
//	// Calculate the angle between the current direction and cone axis
//	float dotProduct = rdVector_Dot3(&currentDir, &coneAxis);
//	float currentAngle = 90.0f - stdMath_ArcSin3(dotProduct);
//	
//	// Check if the current orientation is outside the cone angle
//	if (currentAngle > pConstraint->coneParams.coneAngle)
//	{
//		// Calculate the corrective impulse
//		float angleError = currentAngle - pConstraint->coneParams.coneAngle;
//
//		const float biasFactor = 0.05f;
//		float bias = -(biasFactor / deltaSeconds) * angleError;
//
//		float lambda = -(angleError + bias) / constraintMass;
//		
//		// Apply damping to the impulse to prevent high velocity
//		const float dampingFactor = 0.2f;
//		// Adjust as needed
//		//lambda *= (1.0f - dampingFactor);
//		
//		// Calculate the corrective axis (cross product)
//		rdVector3 correctiveAxis;
//		rdVector_Cross3(&correctiveAxis, &coneAxis, &currentDir);
//		rdVector_Normalize3Acc(&correctiveAxis);
//		
//		// Compute the impulse vector
//		rdVector3 impulse;
//		impulse.x = correctiveAxis.x * lambda;
//		impulse.y = correctiveAxis.y * lambda;
//		impulse.z = correctiveAxis.z * lambda;
//
//		rdVector3 currentImpulse;
//		rdVector_Add3(&currentImpulse, &pConstraint->coneParams.prevImpulse, &impulse);
//		//currentImpulse = impulse;
//
//		rdMatrix34 invOrient;
//		rdMatrix_InvertOrtho34(&invOrient, &pConstraint->targetThing->lookOrientation);
//		rdVector_Zero3(&invOrient.scale);
//
//		rdMatrix_TransformVector34Acc(&currentImpulse, &invOrient);
//
//		
//		// Apply the current angular impulse to the object being constrained (pThing)
//		sithPhysics_ThingApplyAngularImpulse(pConstraint->constrainedThing, &currentImpulse);
//		
//		// Optionally, apply the equal and opposite angular impulse to the base target (pTargetThing)
//		rdVector3 oppositeAngularImpulse;
//		rdVector_Scale3(&oppositeAngularImpulse, &currentImpulse, -1.0f);
//		sithPhysics_ThingApplyAngularImpulse(pConstraint->targetThing, &oppositeAngularImpulse);
//
//		// Combine the previous impulse with the current impulse
//		//rdVector3 currentImpulse;
//		//rdVector_Add3(&currentImpulse, &prevImpulse, &impulse);
//		
//		// Apply the current impulse to both objects
//		//sithPhysics_ThingApplyForce(pTargetThing, &currentImpulse);
//		//sithPhysics_ThingApplyForce(pThing, &currentImpulse);
//		
//		// Store the current impulse for the next frame
//		pConstraint->coneParams.prevImpulse = currentImpulse;
//	}
#endif
}

static void sithConstraint_SolveAngleConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

#if 1
	rdMatrix34 invOrientConstrained;
	rdMatrix_InvertOrtho34(&invOrientConstrained, &pConstraint->constrainedThing->lookOrientation);

	// need to get the relative frame from the constrained thing to the target
	rdMatrix34 invOrientTarget;
	rdMatrix_InvertOrtho34(&invOrientTarget, &pConstraint->targetThing->lookOrientation);

	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &invOrientTarget, &pConstraint->constrainedThing->lookOrientation);

	// extract the angles for constraints
	rdVector3 relativeAngles;
	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);

	// project angles to the feasible set a.k.a clamp 'em
	rdVector3 projectedAngles = relativeAngles;
	rdVector_Clamp3(&projectedAngles, &pConstraint->angleParams.minAngles, &pConstraint->angleParams.maxAngles);

	rdVector3 angleError;
	rdVector_Sub3(&angleError, &relativeAngles, &projectedAngles);

	// Calculate the relative angular velocity
	rdVector3 relativeAngVel;
	//rdVector_NormalizeDeltaAngle3(&relativeAngVel, &pConstraint->targetThing->physicsParams.angVel, &pConstraint->constrainedThing->physicsParams.angVel);
	//rdVector_Sub3(&relativeAngVel, &pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->targetThing->physicsParams.angVel);

	rdVector3 worldTargetAngVel;
	rdMatrix_TransformVector34(&worldTargetAngVel, &pConstraint->targetThing->physicsParams.angVel, &pConstraint->targetThing->lookOrientation);
	
	rdVector3 worldConstrainedAngVel;
	rdMatrix_TransformVector34(&worldConstrainedAngVel, &pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->constrainedThing->lookOrientation);
	
	rdVector_Sub3(&relativeAngVel, &worldConstrainedAngVel, &worldTargetAngVel);

	float bias = 0.1f;
	rdVector3 biasTerm;
	rdVector_Scale3(&biasTerm, &angleError, bias / deltaSeconds);

	rdVector3 angularCorrection;
	rdVector_Add3(&angularCorrection, &angleError, &biasTerm);
	//rdVector_NormalizeAngleAcute3(&angularCorrection);

	//angularCorrection = angleError;
	rdVector_InvScale3Acc(&angularCorrection, constraintMass);

	//float dampingFactor = 0.3;
	//rdVector_MultAcc3(&angularCorrection, &relativeAngVel, -dampingFactor);


	// apply the correction to angular velocity
	rdVector3 correctionParent;
	rdVector_Scale3(&correctionParent, &angularCorrection, invMassA * deltaSeconds);
	rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionParent);
//	rdMatrix_PreRotate34(&pConstraint->targetThing->lookOrientation, &correctionParent);
	//rdVector_NormalizeAngleAcute3(&correctionParent);

	rdMatrix34 rotA;
	rdMatrix_BuildRotate34(&rotA, &correctionParent);
//	sithCollision_sub_4E7670(pConstraint->targetThing, &rotA);

	rdVector3 correctionChild;
	rdVector_Scale3(&correctionChild, &angularCorrection, -invMassB * deltaSeconds);
	rdVector_Sub3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionChild);
//	rdMatrix_PreRotate34(&pConstraint->constrainedThing->lookOrientation, &correctionChild);
	//rdVector_NormalizeAngleAcute3(&correctionChild);
	// 
//	rdMatrix34 rotB;
//	rdMatrix_BuildRotate34(&rotB, &correctionChild);
//	sithCollision_sub_4E7670(pConstraint->constrainedThing, &rotB);



//	// Apply the relative angular velocity correction to conserve angular momentum
//	rdVector3 velCorrection = relativeAngVel;
//	//rdVector_Neg3(&velCorrection, &relativeAngVel);
//	
//	rdVector3 velCorrectionParent = velCorrection;
//	rdMatrix_TransformVector34Acc(&velCorrectionParent, &invOrientTarget);
//	rdVector_Scale3Acc(&velCorrectionParent, invMassA / constraintMass);
//	rdMatrix34 rotVelA;
//	rdMatrix_BuildRotate34(&rotVelA, &velCorrectionParent);
//	sithCollision_sub_4E7670(pConstraint->targetThing, &rotVelA);
//
//	//rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &velCorrectionParent);
//	
//	rdVector3 velCorrectionChild = velCorrection;
//	rdMatrix_TransformVector34Acc(&velCorrectionChild, &invOrientConstrained);
//	rdVector_Scale3Acc(&velCorrectionChild, -invMassB / constraintMass);
//	rdMatrix34 rotVelB;
//	rdMatrix_BuildRotate34(&rotVelB, &velCorrectionChild);
//	sithCollision_sub_4E7670(pConstraint->constrainedThing, &rotVelB);
//
//	//rdVector_Sub3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &velCorrectionChild);

#else
	rdMatrix34 parentRotTranspose, relativeRotation;
	rdMatrix_InvertOrtho34(&parentRotTranspose, &pConstraint->targetThing->lookOrientation);
	rdMatrix_Multiply34(&relativeRotation, &parentRotTranspose, &pConstraint->constrainedThing->lookOrientation);

	rdVector3 angles;
	rdMatrix_ExtractAngles34(&relativeRotation, &angles);

	rdVector3 constrainedAngles;
	constrainedAngles.x = stdMath_Clamp(angles.x, pConstraint->angleParams.minAngles.x, pConstraint->angleParams.maxAngles.x);
	constrainedAngles.y = stdMath_Clamp(angles.y, pConstraint->angleParams.minAngles.y, pConstraint->angleParams.maxAngles.y);
	constrainedAngles.z = stdMath_Clamp(angles.z, pConstraint->angleParams.minAngles.z, pConstraint->angleParams.maxAngles.z);
	rdVector_ClipPrecision3(&constrainedAngles);

	rdVector3 angleDifferences;
	rdVector_Sub3(&angleDifferences, &constrainedAngles, &angles);
	//rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.angVel, &angleDifferences, 1.0f / deltaSeconds);
	
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;
	
	rdVector3 diffA;
	rdVector_Scale3(&diffA, &angleDifferences, invMassB / constraintMass);
	rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &angleDifferences);
//	rdVector3 diffB;
//	rdVector_Scale3(&diffA, &angleDifferences, -invMassA / constraintMass);
//	rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &diffB);

//	rdMatrix34 constrainedRotation;
//	rdMatrix_BuildRotate34(&constrainedRotation, &constrainedAngles);
//	rdMatrix_Multiply34(&pConstraint->thingA->lookOrientation, &pConstraint->thingB->lookOrientation, &constrainedRotation);
#endif
}

static void sithConstraint_SolveLookConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	rdVector3 newUp;
	rdVector_Sub3(&newUp, &pConstraint->targetThing->position, &pConstraint->constrainedThing->position);
	rdVector_Normalize3Acc(&newUp);
	if(pConstraint->lookParams.flipUp)
		rdVector_Neg3Acc(&newUp);

	sithPhysics_ThingSetLook(pConstraint->constrainedThing, &newUp, 0.0f);

	return;







	// Calculate the target orientation
	rdMatrix34 targetOrientation = pConstraint->targetThing->lookOrientation;
	
	// Invert the current hip orientation
	rdMatrix34 invHipOrientation;
	rdMatrix_InvertOrtho34(&invHipOrientation, &pConstraint->constrainedThing->lookOrientation);
	
	// Calculate the relative orientation (target * invHip)
	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &targetOrientation, &invHipOrientation);
	
	// Extract Euler angles from the relative orientation matrix
	rdVector3 relativeAngles;
	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);
	
	// Calculate the necessary angular velocity
	rdVector3 angularVelocityCorrection;
	angularVelocityCorrection.x = relativeAngles.x;// / deltaSeconds;
	angularVelocityCorrection.y = relativeAngles.y;// / deltaSeconds;
	angularVelocityCorrection.z = relativeAngles.z;// / deltaSeconds;
	
	// Apply the angular velocity correction
//	rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &angularVelocityCorrection);

//	rdMatrix34* pMat = &pConstraint->thingA->lookOrientation;
//	rdVector_Sub3(&pMat->uvec, &pConstraint->thingB->position, &pConstraint->thingA->position);
//	rdVector_Normalize3Acc(&pMat->uvec);
//
//	if (pConstraint->lookParams.flipUp)
//		rdVector_Neg3Acc(&pMat->uvec);
//
//	rdVector_Cross3(&pMat->rvec, &pConstraint->lookParams.referenceMat.lvec, &pMat->uvec);
//	rdVector_Normalize3Acc(&pMat->rvec);
//
//	rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);
//	rdVector_Normalize3Acc(&pMat->lvec);
}
void eulerToWorldCoordinates(rdVector3* worldAngularVelocity, const rdMatrix34* orientation)
{ 
	rdVector3 localX = {1.0f, 0.0f, 0.0f};
	rdVector3 localY = {0.0f, 1.0f, 0.0f};
	rdVector3 localZ = {0.0f, 0.0f, 1.0f};
	
	rdVector3 worldX, worldY, worldZ;
	rdMatrix_TransformVector34(&worldX, &localX, orientation);
	rdMatrix_TransformVector34(&worldY, &localY, orientation);
	rdMatrix_TransformVector34(&worldZ, &localZ, orientation);
	
	rdVector3 euler = *worldAngularVelocity;
	worldAngularVelocity->x = euler.x * worldX.x + euler.y * worldY.x + euler.z * worldZ.x;
	worldAngularVelocity->y = euler.x * worldX.y + euler.y * worldY.y + euler.z * worldZ.y;
	worldAngularVelocity->z = euler.x * worldX.z + euler.y * worldY.z + euler.z * worldZ.z;
}

void worldToEulerCoordinates(rdVector3* euler, const rdMatrix34* orientation)
{
	rdVector3 localX = {1.0f, 0.0f, 0.0f};
	rdVector3 localY = {0.0f, 1.0f, 0.0f};
	rdVector3 localZ = {0.0f, 0.0f, 1.0f};
	
	rdVector3 worldX, worldY, worldZ;
	rdMatrix_TransformVector34(&worldX, &localX, orientation);
	rdMatrix_TransformVector34(&worldY, &localY, orientation);
	rdMatrix_TransformVector34(&worldZ, &localZ, orientation);

	rdVector3 worldAngularVelocity = *euler;
	
	euler->x = rdVector_Dot3(&worldAngularVelocity, &worldX);
	euler->y = rdVector_Dot3(&worldAngularVelocity, &worldY); 
	euler->z = rdVector_Dot3(&worldAngularVelocity, &worldZ);
}

void sithConstraint_ApplyAngularCorrection(sithThing* parent, sithThing* child, const rdVector3* corrections, const rdVector3* relativeAngVel)
{
	// Calculate the inverse of the child's correction for the parent
	rdVector3 inverseCorrections = {-corrections->x, -corrections->y, -corrections->z};

	// Calculate the angular velocity correction to apply
	rdVector3 angularCorrection = *corrections;
	
	// Ensure angular correction is valid
	if (!isnan(angularCorrection.x) && !isnan(angularCorrection.y) && !isnan(angularCorrection.z))
	{
		// Calculate the inverse masses
		float invMassParent = 1.0f / parent->physicsParams.mass;
		float invMassChild = 1.0f / child->physicsParams.mass;
		float totalInvMass = invMassParent + invMassChild;
		
		// Apply corrections to angular velocities
		rdVector3 correctionParent = inverseCorrections;
		rdVector_Scale3Acc(&correctionParent, invMassParent / totalInvMass);
		//rdVector_Sub3Acc(&parent->physicsParams.angVel, &correctionParent);
		
		rdVector3 correctionChild = angularCorrection;
		rdVector_Scale3Acc(&correctionChild, invMassChild / totalInvMass);
		rdVector_Add3Acc(&child->physicsParams.angVel, &correctionChild);

		// Apply the relative angular velocity correction to both bodies
		rdVector3 velCorrection;
		rdVector_Scale3(&velCorrection, relativeAngVel, -1.0f);
		
		if (!isnan(velCorrection.x) && !isnan(velCorrection.y) && !isnan(velCorrection.z))
		{
			rdVector3 velCorrectionParent = velCorrection;
			rdVector_Scale3Acc(&velCorrectionParent, invMassParent / totalInvMass);
			//rdVector_Sub3Acc(&parent->physicsParams.angVel, &velCorrectionParent);
			
			rdVector3 velCorrectionChild = velCorrection;
			rdVector_Scale3Acc(&velCorrectionChild, invMassChild / totalInvMass);
			
			rdVector_Add3Acc(&child->physicsParams.angVel, &velCorrectionChild);
		}
	}

//	// Calculate the necessary angle correction to bring the twist angle within the limit
//	float correctionAngleDegrees = twistAngleDegrees - copysign(maxTwistAngle, twistAngleDegrees);
//
//	// Proportional correction factor to control severity of corrections
//	float correctionFactor = correctionAngleDegrees / twistAngleDegrees;
//
//	rdVector3 correction;
//	rdVector_Scale3(&correction, &twistAxis, correctionAngleDegrees * correctionFactor);
//
//	if (!isnan(correction.x) && !isnan(correction.y) && !isnan(correction.z))
//	{
//	// Step 4: Apply corrections to angular velocities
//		rdVector3 correctionA;
//		rdVector_Scale3(&correctionA, &correction, invMassA / constraintMass);
//		rdVector_Sub3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionA);
//
//		rdVector3 correctionB;
//		rdVector_Scale3(&correctionB, &correction, invMassB / constraintMass);
//		rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionB);
//	}
//	else
//		printf("Fuck\n");
}

#include "Primitives/rdQuat.h"
void applyAngularDamping(sithThing* body, float dampingFactor) {
	rdVector3 dampedAngVel;
	rdVector_Scale3(&dampedAngVel, &body->physicsParams.angVel, dampingFactor);
	rdVector_Sub3Acc(&body->physicsParams.angVel, &dampedAngVel);
}

static void sithConstraint_SolveTwistConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

	#if 1


#if 0 // quat
	// Convert current orientations to quaternions
	rdQuat parentQuat;
	rdQuat_BuildFrom34(&parentQuat, &pConstraint->targetThing->lookOrientation);

	rdQuat childQuat;
	rdQuat_BuildFrom34(&childQuat, &pConstraint->constrainedThing->lookOrientation);

	// Calculate relative quaternion
	rdQuat invParentQuat;
	rdQuat_Inverse(&invParentQuat, &parentQuat);
	
	rdQuat relativeQuat;
	rdQuat_Mul(&relativeQuat, &childQuat, &invParentQuat);

	// Extract Euler angles from relative quaternion
	rdVector3 relativeAngles;
	rdQuat_ExtractAngles(&relativeQuat, &relativeAngles);
	//if(pConstraint->twistParams.twistAxis.z > 0)
	//	relativeAngles.y -= 90.0f;
	//else
	//	relativeAngles.y += 90.0f;
#else // mat
	rdVector_Zero3(&pConstraint->targetThing->lookOrientation.scale);
	rdVector_Zero3(&pConstraint->constrainedThing->lookOrientation.scale);

	rdMatrix34 invOrientTarget;
	rdMatrix_InvertOrtho34(&invOrientTarget, &pConstraint->targetThing->lookOrientation);

	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &invOrientTarget, &pConstraint->constrainedThing->lookOrientation);

	rdVector3 relativeAngles;
	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);

	relativeAngles.y = -relativeAngles.y;
	//relativeAngles.y = stdMath_ArcTan3(relativeOrientation.rvec.z, relativeOrientation.uvec.z);
	//relativeAngles.x = stdMath_ArcTan3(-relativeOrientation.lvec.z, sqrt(relativeOrientation.rvec.z * relativeOrientation.rvec.z + relativeOrientation.uvec.z * relativeOrientation.uvec.z));
	//relativeAngles.z = stdMath_ArcTan3(relativeOrientation.lvec.x, relativeOrientation.lvec.y);
#endif

	//printf("relative angles (%f / %f / %f)\n", relativeAngles.x, relativeAngles.y, relativeAngles.z);

	rdVector_NormalizeAngleAcute3(&relativeAngles);

	float maxPitchAngleDegrees = 5.0f;
	float maxTwistAngleDegrees = 5.0f;
	float maxRollAngleDegrees = 5.0f;

	// Apply constraints
	rdVector3 projectedAngles;
	projectedAngles.x = fmin(fmax(relativeAngles.x, -maxPitchAngleDegrees), maxPitchAngleDegrees);
	projectedAngles.y = fmin(fmax(relativeAngles.y, -maxTwistAngleDegrees), maxTwistAngleDegrees);
	projectedAngles.z = fmin(fmax(relativeAngles.z, -maxRollAngleDegrees), maxRollAngleDegrees);

#if 0 // quat
	// Convert back to constrained quaternion
	rdQuat constrainedQuat;
	rdQuat_BuildFromAngles(&constrainedQuat, &relativeAngles);

	// Use Slerp to smoothly interpolate towards the constrained quaternion
	rdQuat newChildQuat;
	rdQuat_Slerp(&newChildQuat, &childQuat, &constrainedQuat, 0.95f);
	
	// Calculate corrections for angular velocities based on constrained quaternion
	rdQuat angularCorrectionQuat;
	rdQuat_Mul(&angularCorrectionQuat, &constrainedQuat, &invParentQuat);
	
	rdVector3 angularCorrection;
	rdQuat_ExtractAngles(&angularCorrectionQuat, &angularCorrection);
#else // mat
	//rdMatrix34 constrainedMat;
	//rdMatrix_BuildRotate34(&constrainedMat, &relativeAngles);
	//
	//rdMatrix34 angularCorrectionMat;
	//rdMatrix_Multiply34(&angularCorrectionMat, &invOrientTarget, &constrainedMat);//, &invOrientA);
	//
	//rdVector3 angularCorrection = relativeAngles;
	//rdMatrix_ExtractAngles34(&angularCorrectionMat, &angularCorrection);
	//angularCorrection.y = stdMath_ArcTan3(angularCorrectionMat.rvec.z, angularCorrectionMat.uvec.z);
	//angularCorrection.x = stdMath_ArcTan3(-angularCorrectionMat.lvec.z, sqrt(angularCorrectionMat.rvec.z * angularCorrectionMat.rvec.z + angularCorrectionMat.uvec.z * angularCorrectionMat.uvec.z));
	//angularCorrection.z = stdMath_ArcTan3(angularCorrectionMat.lvec.x, angularCorrectionMat.lvec.y);

	rdVector3 angularCorrection;
	angularCorrection.x = stdMath_NormalizeDeltaAngle(projectedAngles.x, relativeAngles.x);
	angularCorrection.y = stdMath_NormalizeDeltaAngle(projectedAngles.y, relativeAngles.y);
	angularCorrection.z = stdMath_NormalizeDeltaAngle(projectedAngles.z, relativeAngles.z);
#endif

	rdVector_NormalizeAngleAcute3(&angularCorrection);

	// Increase the correction strength
	//rdVector_Scale3Acc(&angularCorrection, 2.0f); // Scale the correction by 2 for stronger enforcement

	float inertiaA = (2.0f / 5.0f) * pConstraint->targetThing->physicsParams.mass * pConstraint->targetThing->collideSize * pConstraint->targetThing->collideSize;
	float inertiaB = (2.0f / 5.0f) * pConstraint->constrainedThing->physicsParams.mass * pConstraint->constrainedThing->collideSize * pConstraint->constrainedThing->collideSize;

	// Calculate the relative angular velocity
	rdVector3 relativeAngVel;
	//rdVector_Sub3(&relativeAngVel, &pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->targetThing->physicsParams.angVel);
	relativeAngVel.x = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.x, pConstraint->constrainedThing->physicsParams.angVel.x);
	relativeAngVel.y = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.y, pConstraint->constrainedThing->physicsParams.angVel.y);
	relativeAngVel.z = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.z, pConstraint->constrainedThing->physicsParams.angVel.z);

	// Calculate the inverse of the child's correction for the parent
	rdVector3 inverseCorrection = { -angularCorrection.x, -angularCorrection.y, -angularCorrection.z };

	// Ensure corrections are valid
	if (!isnan(angularCorrection.x) && !isnan(angularCorrection.y) && !isnan(angularCorrection.z))
	{
		//rdVector3 correctionChild;
		//rdVector_Scale3(&correctionChild, &angularCorrection, 1.0f / inertiaB);
		//rdVector_Sub3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionChild);

		// Apply corrections to both parent and child angular velocities
		rdVector3 correctionParent;
		rdVector_Scale3(&correctionParent, &inverseCorrection, invMassA / constraintMass);
		rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionParent);
		
		rdVector3 correctionChild;
		rdVector_Scale3(&correctionChild, &angularCorrection, invMassB / constraintMass);
		rdVector_Sub3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionChild);

		// Apply the relative angular velocity correction to conserve angular momentum
		//rdVector3 velCorrection;
		//rdVector_Scale3(&velCorrection, &relativeAngVel, -1.0f);
		//
		//if (!isnan(velCorrection.x) && !isnan(velCorrection.y) && !isnan(velCorrection.z))
		//{
		//	rdVector3 velCorrectionParent = velCorrection;
		//	rdVector_Scale3Acc(&velCorrectionParent, 1.0f / inertiaA);
		//	rdVector_Sub3Acc(&pConstraint->targetThing->physicsParams.field_1F8, &velCorrectionParent);
		//
		//	rdVector3 velCorrectionChild = velCorrection;
		//	rdVector_Scale3Acc(&velCorrectionChild, -1.0f / inertiaB);
		//	rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.field_1F8, &velCorrectionChild);
		//}
	}

//	applyAngularDamping(pConstraint->targetThing, 0.5f);
//	applyAngularDamping(pConstraint->constrainedThing, 0.5f);
	
	//rdMath_ClampVectorRange(&pConstraint->targetThing->physicsParams.angVel, -pConstraint->targetThing->physicsParams.maxRotVel, pConstraint->targetThing->physicsParams.maxRotVel);
	//rdMath_ClampVector(&pConstraint->targetThing->physicsParams.angVel, 0.00001);
	//
	//rdMath_ClampVectorRange(&pConstraint->constrainedThing->physicsParams.angVel, -pConstraint->constrainedThing->physicsParams.maxRotVel, pConstraint->constrainedThing->physicsParams.maxRotVel);
	//rdMath_ClampVector(&pConstraint->constrainedThing->physicsParams.angVel, 0.00001);

		//worldToEulerCoordinates(&pConstraint->targetThing->physicsParams.angVel, &pConstraint->targetThing->lookOrientation);
	//worldToEulerCoordinates(&pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->constrainedThing->lookOrientation);

	#elif 1
	
	rdQuat parentQuat;
	rdQuat_BuildFromAngles(&parentQuat , &pConstraint->targetThing->physicsParams.angVel);

	rdQuat childQuat;
	rdQuat_BuildFromAngles(&childQuat, &pConstraint->constrainedThing->physicsParams.angVel);

	// Calculate relative quaternion
	rdQuat invParentQuat = { -parentQuat.x, -parentQuat.y, -parentQuat.z, parentQuat.w };
	rdQuat relativeQuat;
	rdQuat_Mul(&relativeQuat, &childQuat, &invParentQuat);

	// Extract Euler angles from relative quaternion
	rdVector3 angles;
	rdQuat_ExtractAngles(&relativeQuat, &angles);
	
	rdVector3 maxAngles = { 5.0f, 5.0f, 5.0f};


	// Apply constraints
	angles.x = fmin(fmax(angles.x, -maxAngles.x), maxAngles.x);
	angles.y = fmin(fmax(angles.y, -maxAngles.y), maxAngles.y);
	angles.z = fmin(fmax(angles.z, -maxAngles.z), maxAngles.z);

	// Convert back to quaternions
	rdQuat constrainedQuat;
	rdQuat_BuildFromAngles(&constrainedQuat, &angles);

	// Calculate corrections
	rdQuat correctionQuat;
	rdQuat_Mul(&correctionQuat, &constrainedQuat, &invParentQuat);
	
	rdQuat_ExtractAngles(&correctionQuat, &pConstraint->constrainedThing->physicsParams.angVel);

	// Apply the corrections to the parent as well to conserve energy
	rdQuat inverseCorrectionQuat = { -correctionQuat.x, -correctionQuat.y, -correctionQuat.z, correctionQuat.w };
	rdQuat_ExtractAngles(&inverseCorrectionQuat, &pConstraint->targetThing->physicsParams.angVel);


	#else
	//eulerToWorldCoordinates(&pConstraint->targetThing->physicsParams.angVel, &pConstraint->targetThing->lookOrientation);
	//eulerToWorldCoordinates(&pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->constrainedThing->lookOrientation);

	rdMatrix34 invOrientA;
	rdMatrix_InvertOrtho34(&invOrientA, &pConstraint->targetThing->lookOrientation);
	
	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &pConstraint->constrainedThing->lookOrientation, &invOrientA);
	
//	rdVector3 twistAxis;
//	rdMatrix_TransformVector34(&twistAxis, &pConstraint->twistParams.twistAxis, &pConstraint->targetThing->lookOrientation);
//
//	// Normalize the twist axis
//	rdVector3 normTwistAxis;
//	rdVector_Normalize3(&normTwistAxis, &twistAxis);
//	
//	rdMatrix34 twistMatrix;
//	rdMatrix_Copy34(&twistMatrix, &relativeOrientation);
//	
//	// Project the relative orientation onto the twist axis to extract the twist component
//	rdVector3 projectedUp;
//	rdMatrix_TransformVector34(&projectedUp, &twistMatrix.uvec, &twistMatrix);
//	
//	// Calculate the angle around the twist axis
//	float twistAngleRadians = atan2(projectedUp.y, projectedUp.x);
//	if (isnan(twistAngleRadians))
//		printf("Fuck\n");
//
//	float twistAngleDegrees = twistAngleRadians * (180.0 / M_PI);
//	
//	// Step 3: Check the twist constraint
//	float maxTwistAngle = pConstraint->twistParams.maxTwistAngle;// / (180.0f / M_PI);
//	if (fabs(twistAngleDegrees) > maxTwistAngle)
//	{
//		// Calculate the necessary angle correction to bring the twist angle within the limit
//		float correctionAngleDegrees = twistAngleDegrees - copysign(maxTwistAngle, twistAngleDegrees);
//		sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &twistAxis, correctionAngleDegrees);
//	}

	// Extract pitch and roll angles
	rdVector3 relativeAngles;
	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);
	
	float* anglesAsFloats = &relativeAngles.x;

	rdVector3 corrections;
	float* correctionsAsFloats = &corrections.x;

	rdVector3 maxAngles = { 5.0f, 10.0f, 5.0f };
	float* maxAnglesAsFloats = &maxAngles.x;

	for (int i = 0; i < 3; ++i)
	{
		if (fabs(anglesAsFloats[i]) > maxAnglesAsFloats[i])
		{
			correctionsAsFloats[i] = anglesAsFloats[i] - copysign(maxAnglesAsFloats[i], anglesAsFloats[i]);
		}
		else
		{
			correctionsAsFloats[i] = 0.0f;
		}
	}

	rdVector3 relativeAngVel;
	rdVector_Sub3(&relativeAngVel, &pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->targetThing->physicsParams.angVel);

	sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &corrections, &relativeAngVel);

//	// Constrain yaw (twist)
//	float maxTwistAngleDegrees = 10.0f;
//	if (fabs(relativeAngles.y) > maxTwistAngleDegrees)
//	{
//		float correctionYawDegrees = relativeAngles.y - copysign(maxTwistAngleDegrees, relativeAngles.y);
//		
//		rdVector3 yawCorrection = {0.0f, correctionYawDegrees, 0.0f};
//		
//		// Apply corrections to angular velocities
//		sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &yawCorrection);
//	}
//
//	// Check and correct the pitch constraint
//	float maxPitchAngleDegrees = 5.0f;
//	if (fabs(relativeAngles.x) > maxPitchAngleDegrees)
//	{
//		float correctionPitchDegrees = relativeAngles.x - copysign(maxPitchAngleDegrees, relativeAngles.x);
//		rdVector3 pitchCorrection = {correctionPitchDegrees, 0.0f, 0.0f};
//		sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &pitchCorrection);
//	}
//	
//	// Check and correct the roll constraint
//	float maxRollAngleDegrees = 5.0f;
//	if (fabs(relativeAngles.z) > maxRollAngleDegrees)
//	{
//		float correctionRollDegrees = relativeAngles.z - copysign(maxRollAngleDegrees, relativeAngles.z);
//		rdVector3 rollCorrection = {0.0f, 0.0f, correctionRollDegrees};
//		sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &rollCorrection);
//	}

//	// Calculate the relative angular velocity
//	rdVector3 relativeAngVel;
//	rdVector_Sub3(&relativeAngVel, &pConstraint->targetThing->physicsParams.angVel, &pConstraint->constrainedThing->physicsParams.angVel);
//	
//	// Project the relative angular velocity onto the world twist axis
//	float projAngVel = rdVector_Dot3(&relativeAngVel, &twistAxis);
//	if(isnan(projAngVel))
//		printf("Fuck\n");
//	
//	// Apply the velocity correction to both bodies
//	rdVector3 velCorrection;
//	rdVector_Scale3(&velCorrection, &twistAxis, projAngVel);
//	
//	if (!isnan(velCorrection.x) && !isnan(velCorrection.y) && !isnan(velCorrection.z))
//	{
//		rdVector3 velCorrectionParent = velCorrection;
//		rdVector_Scale3Acc(&velCorrectionParent, invMassA / constraintMass);
//		rdVector_Sub3Acc(&pConstraint->targetThing->physicsParams.angVel, &velCorrectionParent);
//	
//		rdVector3 velCorrectionChild = velCorrection;
//		rdVector_Scale3Acc(&velCorrectionChild, invMassB / constraintMass);
//		rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &velCorrectionChild);
//	}
//	else
//		printf("Fuck\n");

	//worldToEulerCoordinates(&pConstraint->targetThing->physicsParams.angVel, &pConstraint->targetThing->lookOrientation);
	//worldToEulerCoordinates(&pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->constrainedThing->lookOrientation);
	#endif

#if 0
	rdVector3 crossLook;
	rdVector_Cross3(&crossLook, &pConstraint->targetThing->lookOrientation.lvec, &pConstraint->constrainedThing->lookOrientation.lvec);
	float dotLook = rdVector_Dot3(&pConstraint->targetThing->lookOrientation.lvec, &pConstraint->constrainedThing->lookOrientation.lvec);
	
	float angleChange = atan2f(rdVector_Len3(&crossLook), dotLook);
	
	// Calculate the relative angular velocity
	rdVector3 relativeAngVel;
	rdVector_Sub3(&relativeAngVel, &pConstraint->targetThing->physicsParams.angVel, &pConstraint->constrainedThing->physicsParams.angVel);
	
	// Normalize the twist axis
	rdVector3 normTwistAxis;
	rdMatrix_TransformVector34(&normTwistAxis, &pConstraint->twistParams.twistAxis, &pConstraint->targetThing->lookOrientation);
	
	// Project the relative angular velocity onto the twist axis
	float projAngVel = rdVector_Dot3(&relativeAngVel, &normTwistAxis);
	
	// Check if the twist exceeds the max allowed twist angle
	float twistAngleRadians = pConstraint->twistParams.maxTwistAngle / (180.0f / M_PI);
	if (fabs(projAngVel) > twistAngleRadians || angleChange > twistAngleRadians)
	{
		float correctionFactor = fminf(twistAngleRadians / fabs(projAngVel), twistAngleRadians / angleChange);

		// Correct the angular velocity to stay within the twist constraint
		//float correctionFactor = twistAngleRadians / fabs(projAngVel);
		//rdVector3 correction;
		//scaleVector(&relativeAngVel, correctionFactor);
		//ubtractVectors(&relativeAngVel, &correction, &correction);
		rdVector3 correction = {
			normTwistAxis.x * projAngVel * (1.0f - correctionFactor),
			normTwistAxis.y * projAngVel * (1.0f - correctionFactor),
			normTwistAxis.z * projAngVel * (1.0f - correctionFactor)
		};
		
		// Apply the correction to each body's angular velocity based on their masses
		rdVector3 correctionA;
		rdVector_Scale3(&correctionA, &correction, invMassA / constraintMass);
		rdVector_Sub3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionA);
		
		rdVector3 correctionB;
		rdVector_Scale3(&correctionB, &correction, invMassB / constraintMass);
		rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionB);
	}
#endif
}

void sithConstraint_ApplyConstraint(sithThing* pThing, sithConstraint* constraint, sithConstraintResult* result, float deltaSeconds)
{
	result->C = stdMath_ClipPrecision(result->C);
	if (stdMath_Fabs(result->C) > 0.0)
	{
		float invMassA = 1.0f / constraint->targetThing->physicsParams.mass;
		float invMassB = 1.0f / constraint->constrainedThing->physicsParams.mass;

		float invInertiaTensorA = 1.0f / constraint->targetThing->physicsParams.inertia;
		float invInertiaTensorB = 1.0f / constraint->constrainedThing->physicsParams.inertia;

		float Me = sithConstraint_ComputeEffectiveMass(&result->JrA, &result->JrB, constraint->targetThing, constraint->constrainedThing);
		//if (constraint->type == SITH_CONSTRAINT_CONE)
			//Me = 1.0f / (invInertiaTensorA + invInertiaTensorB);

		float lambda = result->C * Me;
		lambda = stdMath_ClipPrecision(lambda);

		// calculate linear impulses
		rdVector3 impulseLinearA, impulseLinearB;
		rdVector_Scale3(&impulseLinearA, &result->JvA, lambda * invMassA);
		rdVector_Scale3(&impulseLinearB, &result->JvB, lambda * invMassB);

		// calculate angular impulses
		rdVector3 impulseAngularA, impulseAngularB;
		rdVector_Scale3(&impulseAngularA, &result->JrA, lambda * invInertiaTensorA);
		rdVector_Scale3(&impulseAngularB, &result->JrB, lambda * invInertiaTensorB);
	
		// friction
		rdMatrix34 I;
		rdMatrix_InvertOrtho34(&I, &constraint->targetThing->lookOrientation);

		rdMatrix34 R_rel;
		rdMatrix_Multiply34(&R_rel, &I, &constraint->constrainedThing->lookOrientation);

		rdVector3 omega_rel;
		compute_angular_velocity_from_rotation(&omega_rel, &R_rel);

		apply_friction_to_rotational_impulse(&omega_rel, jkPlayer_puppetFriction, &impulseAngularA, &impulseAngularB);

		// apply the impulses
		ApplyImpulse(constraint->targetThing, &impulseLinearA, &impulseAngularA);
		ApplyImpulse(constraint->constrainedThing, &impulseLinearB, &impulseAngularB);
	}
}

void CalculateDesiredPositionDistance(rdVector3* desiredPosition, const rdVector3* anchor, const rdVector3* bodyPosition, float distance)
{
	rdVector3 direction;
	rdVector_Sub3(&direction, bodyPosition, anchor);
	rdVector_Normalize3Acc(&direction);
	desiredPosition->x = anchor->x + direction.x * distance;
	desiredPosition->y = anchor->y + direction.y * distance;
	desiredPosition->z = anchor->z + direction.z * distance;
}

void sithConstraint_SolveConstraints(sithThing* pThing, float deltaSeconds)
{
	if (pThing->constraints)
	{
		// iteratively solve constraints
		for (int k = 0; k < 10; ++k)
		{
			float iterationStep = deltaSeconds;// / 10.0f;

			sithConstraintResult result;
			sithConstraintResult result2;
			memset(&result, 0, sizeof(sithConstraintResult));
			memset(&result2, 0, sizeof(sithConstraintResult));

			//uint64_t jointBits = pThing->animclass->jointBits;
			//while (jointBits != 0)
			//{
			//	int jointIdx = stdMath_FindLSB64(jointBits);
			//	jointBits ^= 1ull << jointIdx;
			//
			//	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
			//	pJoint->things[0].physicsParams.lastPos = pJoint->things[0].position;
			//	pJoint->things[0].physicsParams.lastOrient = pJoint->things[0].lookOrientation;
			//}

			sithConstraint* constraint = pThing->constraints;
			while (constraint)
			{
				switch (constraint->type)
				{
				case SITH_CONSTRAINT_DISTANCE:
					sithConstraint_SolveDistanceConstraint(&result, constraint, iterationStep);
					break;
				case SITH_CONSTRAINT_CONE:
					sithConstraint_SolveConeConstraint(&result, &result2, constraint, iterationStep);
					break;
				case SITH_CONSTRAINT_ANGLES:
					//sithConstraint_SolveAngleConstraint(constraint, iterationStep);
					break;
				case SITH_CONSTRAINT_LOOK:
				//	sithConstraint_SolveLookConstraint(constraint, iterationStep);
					break;
				case SITH_CONSTRAINT_TWIST:
				//	sithConstraint_SolveTwistConstraint(constraint, iterationStep);
					break;
				default:
					break;
				}
				sithConstraint_ApplyConstraint(pThing, constraint, &result, iterationStep);
				sithConstraint_ApplyConstraint(pThing, constraint, &result2, iterationStep);
				constraint = constraint->next;
			}

			uint64_t jointBits = pThing->animclass->physicsJointBits;
			while (jointBits != 0)
			{
				int jointIdx = stdMath_FindLSB64(jointBits);
				jointBits ^= 1ull << jointIdx;
		   	
		   		sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
			
				if(!rdVector_IsZero3(&pThing->physicsParams.accAngularForces))
				{
					rdVector3 a3;
					rdVector_Scale3(&a3, &pThing->physicsParams.accAngularForces, deltaSeconds);
					float angle = (180.0f / M_PI) * rdVector_Normalize3Acc(&a3);

					rdMatrix34 a;
					rdMatrix_BuildFromVectorAngle34(&a, &a3, angle);
					sithCollision_sub_4E7670(&pJoint->thing, &a);
					rdMatrix_Normalize34(&pJoint->thing.lookOrientation);
				}
			
				if(!rdVector_IsZero3(&pJoint->thing.physicsParams.accLinearForces))
				{
					rdVector_Scale3(&pJoint->thing.physicsParams.velocityMaybe, &pJoint->thing.physicsParams.accLinearForces, deltaSeconds);
					sithThing_TickPhysics(&pJoint->thing, deltaSeconds);
				}
			
				rdVector_Zero3(&pJoint->thing.physicsParams.accLinearForces);
				rdVector_Zero3(&pJoint->thing.physicsParams.accAngularForces);
			}
		}


		//uint64_t jointBits = pThing->animclass->physicsJointBits;
		//while (jointBits != 0)
		//{
		//	int jointIdx = stdMath_FindLSB64(jointBits);
		//	jointBits ^= 1ull << jointIdx;
		//
		//	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		//
		//	// calculate the delta between the original position and corrected position	
		//	rdVector3 posDelta;
		//	rdVector_Sub3(&posDelta, &pJoint->things[0].position, &pJoint->things[0].physicsParams.lastPos);
		//
		//	// calculate the delta between the original rotation and corrected rotation
		//	rdMatrix34 invOrient;
		//	rdMatrix_InvertOrtho34(&invOrient, &pJoint->things[0].physicsParams.lastOrient);
		//
		//	rdMatrix34 rotDelta;
		//	rdMatrix_Multiply34(&rotDelta, &invOrient, &pJoint->things[0].lookOrientation);
		//
		//	// reset the state and run a proper update
		//	pJoint->things[0].position = pJoint->things[0].physicsParams.lastPos;
		//	pJoint->things[0].lookOrientation = pJoint->things[0].physicsParams.lastOrient;
		//
		//	rdVector_Add3Acc(&pJoint->things[0].physicsParams.vel, &posDelta);
		//	pJoint->things[0].physicsParams.physflags |= SITH_PF_HAS_FORCE;
		//
		////	rdVector3 angles;
		////	rdMatrix_ExtractAngles34(&rotDelta, &angles);
		////	rdVector_Add3Acc(&pJoint->things[0].physicsParams.angVel, &angles);
		//
		//
		//	//if (!rdVector_IsZero3(&posDelta))
		//	//{
		//	//	sithCollision_UpdateThingCollision(&pJoint->things[0], &posDelta, rdVector_Normalize3Acc(&posDelta), 0);
		//	//}
		//	
		//	//sithCollision_sub_4E7670(&pJoint->things[0], &rotDelta); // rotate
		//}

		//uint64_t jointBits = pThing->animclass->physicsJointBits;
		//while (jointBits != 0)
		//{
		//	int jointIdx = stdMath_FindLSB64(jointBits);
		//	jointBits ^= 1ull << jointIdx;
		//
		//	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		//
		//	rdVector3 dir;
		//	rdVector_Sub3(&dir, &pJoint->things[0].position, &pJoint->things[0].physicsParams.lastPos);
		//	if(!rdVector_IsZero3(&dir))
		//	{
		//		float dist = rdVector_Normalize3Acc(&dir);
		//		pJoint->things[0].position = pJoint->things[0].physicsParams.lastPos;
		//		sithCollision_UpdateThingCollision(&pJoint->things[0], &dir, dist, 0);
		//	}
		//}

		// post stabilization
		sithConstraint* constraint = pThing->constraints;
		while (0)//constraint)
		{
			switch (constraint->type)
			{
			case SITH_CONSTRAINT_DISTANCE:
				{
					rdVector3 offsetA;
					rdMatrix_TransformVector34(&offsetA, &constraint->distanceParams.targetAnchor, &constraint->targetThing->lookOrientation);

					rdVector3 offsetB;
					rdMatrix_TransformVector34(&offsetB, &constraint->distanceParams.constraintAnchor, &constraint->constrainedThing->lookOrientation);

					//rdVector_Zero3(&pConstraint->targetThing->lookOrientation.scale);
					rdVector3 anchorA, anchorB;
					rdVector_Add3(&anchorA, &offsetA, &constraint->targetThing->position);
					rdVector_Add3(&anchorB, &offsetB, &constraint->constrainedThing->position);

					rdVector3 desiredPositionA, desiredPositionB;
					CalculateDesiredPositionDistance(&desiredPositionA, &anchorA, &constraint->targetThing->position, constraint->distanceParams.constraintDistance);
					CalculateDesiredPositionDistance(&desiredPositionB, &anchorB, &constraint->constrainedThing->position, constraint->distanceParams.constraintDistance);

					rdVector3 positionalErrorA, positionalErrorB;
					rdVector_Sub3(&positionalErrorA, &desiredPositionA, &constraint->targetThing->position);
					rdVector_Sub3(&positionalErrorB, &desiredPositionB, &constraint->constrainedThing->position);

					float correctionFactor = 0.1;
					rdVector3 correctivePositionA, correctivePositionB;
					rdVector_Scale3(&correctivePositionA, &positionalErrorA, correctionFactor);
					rdVector_Scale3(&correctivePositionB, &positionalErrorB, correctionFactor);

					sithCollision_UpdateThingCollision(constraint->targetThing, &correctivePositionA, rdVector_Normalize3Acc(&correctivePositionA), 0);
					sithCollision_UpdateThingCollision(constraint->constrainedThing, &correctivePositionB, rdVector_Normalize3Acc(&correctivePositionB), 0);

					//rdVector_Add3Acc(&constraint->targetThing->position, &correctivePositionA);
					//rdVector_Add3Acc(&constraint->constrainedThing->position, &correctivePositionB);
				}
				break;
			case SITH_CONSTRAINT_CONE:
				break;
			case SITH_CONSTRAINT_ANGLES:
				break;
			case SITH_CONSTRAINT_LOOK:
				break;
			case SITH_CONSTRAINT_TWIST:
				break;
			default:
				break;
			}
			constraint = constraint->next;
		}

		sithPuppet_BuildJointMatrices(pThing);

		//uint64_t jointBits = pThing->animclass->physicsJointBits;
		//while (jointBits != 0)
		//{
		//	int jointIdx = stdMath_FindLSB64(jointBits);
		//	jointBits ^= 1ull << jointIdx;
		//
		//	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		//	
		//	// would it make sense to split this so we're not diving head first into collision code?
		//	//sithPhysics_ThingTick(&pJoint->thing, deltaSeconds);
		//	rdVector_Scale3(&pJoint->things[0].physicsParams.velocityMaybe, &pJoint->things[0].physicsParams.vel, deltaSeconds);
		//
		////	sithThing_TickPhysics(&pJoint->things[0], deltaSeconds);
		//
		//	//rdVector_Copy3(&pJoint->things[0].field_268, &pJoint->things[0].physicsParams.velocityMaybe);
		//	rdVector_Zero3(&pJoint->things[0].field_268);
		//
		//	rdVector3 offsetDir;
		//	float len = rdVector_Normalize3(&offsetDir, &pJoint->things->physicsParams.velocityMaybe);
		//	sithCollision_UpdateThingCollision(&pJoint->things[0], &offsetDir, len, 0);
		//	
		//	rdVector_Zero3(&pJoint->things[0].lookOrientation.scale);
		//}
	}
}

#endif


#endif

#endif
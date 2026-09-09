#if WITH_DEV_AUTOMATION_TESTS

#include "Cart/CartBase.h"
#include "Cart/CartStabilizationMath.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

namespace Ch4CartStabilizationTests
{
	constexpr float DeadZoneDegrees = 8.0f;
	constexpr float FullAssistDegrees = 30.0f;
	constexpr float ProportionalGain = 6.0f;
	constexpr float DerivativeGain = 3.0f;
	constexpr float MaximumAngularAcceleration = 8.0f;

	FVector TiltUpAroundForward(const float Degrees)
	{
		return FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Degrees))
			.RotateVector(FVector::UpVector);
	}

	Ch4CartStabilization::FCorrectionResult Calculate(
		const FVector& CartUp,
		const FVector& TargetUp = FVector::UpVector,
		const FVector& AngularVelocity = FVector::ZeroVector,
		const float Kp = ProportionalGain,
		const float Kd = DerivativeGain,
		const float MaxAcceleration = MaximumAngularAcceleration)
	{
		return Ch4CartStabilization::CalculateCorrection(
			CartUp,
			TargetUp,
			AngularVelocity,
			DeadZoneDegrees,
			FullAssistDegrees,
			Kp,
			Kd,
			MaxAcceleration);
	}

	struct FCartTestWorld
	{
		UWorld* World = nullptr;

		bool Initialize()
		{
			if (!GEngine)
			{
				return false;
			}

			const UWorld::InitializationValues InitializationValues = UWorld::InitializationValues()
				.RequiresHitProxies(false)
				.ShouldSimulatePhysics(true)
				.EnableTraceCollision(true)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(true);

			World = UWorld::CreateWorld(
				EWorldType::Game,
				false,
				NAME_None,
				nullptr,
				true,
				ERHIFeatureLevel::Num,
				&InitializationValues);
			if (!World)
			{
				return false;
			}

			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			return true;
		}

		~FCartTestWorld()
		{
			if (World)
			{
				World->EndPlay(EEndPlayReason::Quit);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CartStabilizationMathTest,
	"Ch4_multiGame.Cart.StabilizationMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CartStabilizationMathTest::RunTest(const FString& Parameters)
{
	using namespace Ch4CartStabilizationTests;

	TestTrue(TEXT("Zero tilt is valid"), Calculate(FVector::UpVector).bIsValid);
	TestTrue(TEXT("Zero tilt has no correction"),
		Calculate(FVector::UpVector).AngularAcceleration.IsNearlyZero());
	TestTrue(TEXT("Five degrees remains inside the natural wobble dead zone"),
		Calculate(TiltUpAroundForward(5.0f)).AngularAcceleration.IsNearlyZero());
	TestTrue(TEXT("Eight degrees is the zero-assist boundary"),
		FMath::IsNearlyZero(Ch4CartStabilization::CalculateAssistAlpha(8.0f, 8.0f, 30.0f)));
	TestTrue(TEXT("Nineteen degrees is the midpoint of the smooth assist range"),
		FMath::IsNearlyEqual(Ch4CartStabilization::CalculateAssistAlpha(19.0f, 8.0f, 30.0f), 0.5f));
	TestTrue(TEXT("Thirty degrees receives full assist"),
		FMath::IsNearlyEqual(Ch4CartStabilization::CalculateAssistAlpha(30.0f, 8.0f, 30.0f), 1.0f));
	TestTrue(TEXT("Forty-five degrees remains at full assist"),
		FMath::IsNearlyEqual(Ch4CartStabilization::CalculateAssistAlpha(45.0f, 8.0f, 30.0f), 1.0f));

	const Ch4CartStabilization::FAngleSettings ValidAngles =
		Ch4CartStabilization::SanitizeAngleSettings(8.0f, 30.0f, 55.0f);
	TestFalse(TEXT("Valid ordered angles remain unchanged"), ValidAngles.bWasAdjusted);
	const Ch4CartStabilization::FAngleSettings InvalidAngles =
		Ch4CartStabilization::SanitizeAngleSettings(30.0f, 10.0f, 20.0f);
	TestTrue(TEXT("Invalid angle order is reported as adjusted"), InvalidAngles.bWasAdjusted);
	TestTrue(TEXT("Sanitized dead zone remains below full assist"),
		InvalidAngles.DeadZoneDegrees < InvalidAngles.FullAssistDegrees);
	TestTrue(TEXT("Sanitized full assist remains below the safety limit"),
		InvalidAngles.FullAssistDegrees < InvalidAngles.MaximumTiltDegrees);
	TestTrue(TEXT("Limit-approach damping begins at the full-assist angle"),
		FMath::IsNearlyZero(Ch4CartStabilization::CalculateAssistAlpha(30.0f, 30.0f, 55.0f)));
	TestTrue(TEXT("Limit-approach damping reaches its configured multiplier at maximum tilt"),
		FMath::IsNearlyEqual(Ch4CartStabilization::CalculateAssistAlpha(55.0f, 30.0f, 55.0f), 1.0f));

	const Ch4CartStabilization::FCorrectionResult NineteenDegreeResult = Calculate(TiltUpAroundForward(19.0f));
	TestTrue(TEXT("The PD error retains the measured tilt angle"),
		FMath::IsNearlyEqual(NineteenDegreeResult.TiltAngleDegrees, 19.0f, 0.01f));
	TestTrue(TEXT("Mid-range tilt creates a correction"),
		!NineteenDegreeResult.AngularAcceleration.IsNearlyZero());

	const FVector GroundNormal = TiltUpAroundForward(20.0f);
	const Ch4CartStabilization::FCorrectionResult GroundRelativeResult = Calculate(GroundNormal, GroundNormal);
	const Ch4CartStabilization::FCorrectionResult WorldRelativeResult = Calculate(GroundNormal, FVector::UpVector);
	TestTrue(TEXT("A Cart aligned with sloped ground needs no correction"),
		GroundRelativeResult.AngularAcceleration.IsNearlyZero());
	TestTrue(TEXT("The same Cart would be tilted relative to WorldUp"),
		FMath::IsNearlyEqual(WorldRelativeResult.TiltAngleDegrees, 20.0f, 0.01f));

	const FVector TiltedUp = TiltUpAroundForward(30.0f);
	const Ch4CartStabilization::FCorrectionResult NoYawResult = Calculate(TiltedUp);
	const Ch4CartStabilization::FCorrectionResult YawResult = Calculate(
		TiltedUp, FVector::UpVector, FVector::UpVector * 4.0f);
	TestTrue(TEXT("Yaw angular velocity is removed from stabilization damping"),
		NoYawResult.AngularAcceleration.Equals(YawResult.AngularAcceleration, 0.001f));

	const FVector RollAngularVelocity = FVector::ForwardVector * 2.0f;
	const Ch4CartStabilization::FCorrectionResult DampingOnlyResult = Calculate(
		TiltedUp, FVector::UpVector, RollAngularVelocity, 0.0f, DerivativeGain);
	TestTrue(TEXT("Roll damping opposes roll angular velocity"),
		FVector::DotProduct(DampingOnlyResult.AngularAcceleration, RollAngularVelocity) < 0.0f);

	const Ch4CartStabilization::FCorrectionResult ClampedResult = Calculate(
		TiltUpAroundForward(60.0f), FVector::UpVector, FVector::ForwardVector * 100.0f,
		ProportionalGain, DerivativeGain, MaximumAngularAcceleration);
	TestTrue(TEXT("Correction respects the angular acceleration cap"),
		ClampedResult.AngularAcceleration.Size() <= MaximumAngularAcceleration + UE_KINDA_SMALL_NUMBER);

	TArray<FVector> ContactNormals = { GroundNormal, GroundNormal, FVector::ForwardVector, FVector::ZeroVector };
	FVector AverageGroundNormal = FVector::ZeroVector;
	TestTrue(TEXT("Two valid ground contacts produce an average normal"),
		Ch4CartStabilization::TryCalculateAverageGroundNormal(ContactNormals, 2, 0.2f, AverageGroundNormal));
	TestTrue(TEXT("Wall and zero normals do not contaminate the ground reference"),
		AverageGroundNormal.Equals(GroundNormal, 0.001f));

	ContactNormals.SetNum(1);
	TestFalse(TEXT("One contact is insufficient for normal stabilization"),
		Ch4CartStabilization::TryCalculateAverageGroundNormal(ContactNormals, 2, 0.2f, AverageGroundNormal));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CartStabilizationConfigurationTest,
	"Ch4_multiGame.Cart.StabilizationConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CartStabilizationConfigurationTest::RunTest(const FString& Parameters)
{
	using namespace Ch4CartStabilizationTests;

	FCartTestWorld TestWorld;
	if (!TestWorld.Initialize())
	{
		AddError(TEXT("Failed to initialize the Cart stabilization test world."));
		return false;
	}

	UClass* CartClass = LoadClass<ACartBase>(nullptr, TEXT("/Game/CartTest/BP_ShoppingCart.BP_ShoppingCart_C"));
	if (!TestNotNull(TEXT("The gameplay BP_ShoppingCart class loads"), CartClass))
	{
		return false;
	}

	ACartBase* Cart = TestWorld.World->SpawnActor<ACartBase>(CartClass, FTransform::Identity);
	if (!TestNotNull(TEXT("The gameplay Cart spawns"), Cart))
	{
		return false;
	}
	if (!Cart->HasActorBegunPlay())
	{
		Cart->DispatchBeginPlay();
	}

	UStaticMeshComponent* CartMesh = Cast<UStaticMeshComponent>(Cart->GetRootComponent());
	UPhysicsConstraintComponent* Constraint = Cart->FindComponentByClass<UPhysicsConstraintComponent>();
	if (!TestNotNull(TEXT("CartMesh remains the physics root"), CartMesh)
		|| !TestNotNull(TEXT("The upright safety constraint exists"), Constraint))
	{
		return false;
	}

	FBodyInstance* BodyInstance = CartMesh->GetBodyInstance();
	if (!TestNotNull(TEXT("CartMesh has a BodyInstance"), BodyInstance))
	{
		return false;
	}

	// NullRHI automation worlds do not create the runtime rigid actor used by PIE, but the authoritative
	// BeginPlay path must still request simulation and configure all mass/constraint source data.
	TestTrue(TEXT("The authoritative gameplay Cart requests physics simulation"), BodyInstance->bSimulatePhysics);
	TestTrue(TEXT("Cart mass remains 220 kg"), FMath::IsNearlyEqual(BodyInstance->GetMassOverride(), 220.0f));
	TestTrue(TEXT("The gameplay Blueprint inherits the lowered COM offset"),
		BodyInstance->COMNudge.Equals(FVector(-20.0f, 0.0f, -18.0f), 0.001f));
	TestTrue(TEXT("Roll and pitch inertia increase while yaw inertia is unchanged"),
		BodyInstance->InertiaTensorScale.Equals(FVector(2.0f, 2.0f, 1.0f), 0.001f));
	TestTrue(TEXT("Maximum angular velocity is 240 degrees per second"),
		FMath::IsNearlyEqual(BodyInstance->GetMaxAngularVelocityInRadians(),
			FMath::DegreesToRadians(240.0f), 0.001f));
	TestTrue(TEXT("Angular damping remains 3"), FMath::IsNearlyEqual(CartMesh->GetAngularDamping(), 3.0f));

	UPrimitiveComponent* ConstrainedComponent1 = nullptr;
	UPrimitiveComponent* ConstrainedComponent2 = nullptr;
	FName BoneName1;
	FName BoneName2;
	Constraint->GetConstrainedComponents(ConstrainedComponent1, BoneName1, ConstrainedComponent2, BoneName2);
	TestTrue(TEXT("Constraint body one is CartMesh"), ConstrainedComponent1 == CartMesh);
	TestNull(TEXT("Constraint body two is World"), ConstrainedComponent2);
	TestEqual(TEXT("Linear X remains free"), Constraint->ConstraintInstance.GetLinearXMotion(), LCM_Free);
	TestEqual(TEXT("Linear Y remains free"), Constraint->ConstraintInstance.GetLinearYMotion(), LCM_Free);
	TestEqual(TEXT("Linear Z remains free"), Constraint->ConstraintInstance.GetLinearZMotion(), LCM_Free);
	TestEqual(TEXT("Twist around the primary World-up axis remains free"),
		Constraint->ConstraintInstance.GetAngularTwistMotion(), ACM_Free);
	TestEqual(TEXT("Swing 1 is limited"), Constraint->ConstraintInstance.GetAngularSwing1Motion(), ACM_Limited);
	TestEqual(TEXT("Swing 2 is limited"), Constraint->ConstraintInstance.GetAngularSwing2Motion(), ACM_Limited);
	TestTrue(TEXT("Swing 1 limit is 55 degrees"),
		FMath::IsNearlyEqual(Constraint->ConstraintInstance.GetAngularSwing1Limit(), 55.0f));
	TestTrue(TEXT("Swing 2 limit is 55 degrees"),
		FMath::IsNearlyEqual(Constraint->ConstraintInstance.GetAngularSwing2Limit(), 55.0f));
	TestFalse(TEXT("The final swing limit is hard"), Constraint->ConstraintInstance.GetIsSoftSwingLimit());
	TestTrue(TEXT("Optional soft-limit stiffness defaults to the UE cone value"),
		FMath::IsNearlyEqual(Constraint->ConstraintInstance.GetSoftSwingLimitStiffness(), 50.0f));
	TestTrue(TEXT("Optional soft-limit damping defaults to the UE cone value"),
		FMath::IsNearlyEqual(Constraint->ConstraintInstance.GetSoftSwingLimitDamping(), 5.0f));
	TestTrue(TEXT("Constraint restitution remains zero"),
		FMath::IsNearlyZero(Constraint->ConstraintInstance.GetSoftSwingLimitRestitution()));
	TestFalse(TEXT("The safety constraint cannot break"), Constraint->ConstraintInstance.IsAngularBreakable());
	TestFalse(TEXT("Projection teleport correction stays disabled"),
		Constraint->ConstraintInstance.IsProjectionEnabled());
	TestTrue(TEXT("Cart local Z is the primary twist axis"),
		Constraint->ConstraintInstance.PriAxis1.Equals(FVector::UpVector, 0.001f));
	TestTrue(TEXT("World Z is the matching primary twist axis"),
		Constraint->ConstraintInstance.PriAxis2.Equals(FVector::UpVector, 0.001f));

	const FTransform SoftCartTransform(FVector(1000.0f, 0.0f, 0.0f));
	ACartBase* SoftCart = TestWorld.World->SpawnActorDeferred<ACartBase>(CartClass, SoftCartTransform);
	if (!TestNotNull(TEXT("A deferred gameplay Cart is available for tunable settings validation"), SoftCart))
	{
		return false;
	}
	SoftCart->StabilizationDeadZoneDegrees = 30.0f;
	SoftCart->StabilizationFullAssistDegrees = 10.0f;
	SoftCart->MaximumTiltAngleDegrees = 20.0f;
	SoftCart->bUseSoftAngularLimit = true;
	SoftCart->SoftAngularLimitStiffness = 75.0f;
	SoftCart->SoftAngularLimitDamping = 10.0f;
	SoftCart->ConstraintRestitution = 0.25f;
	UGameplayStatics::FinishSpawningActor(SoftCart, SoftCartTransform);
	if (!SoftCart->HasActorBegunPlay())
	{
		SoftCart->DispatchBeginPlay();
	}

	UPhysicsConstraintComponent* SoftConstraint = SoftCart->FindComponentByClass<UPhysicsConstraintComponent>();
	if (!TestNotNull(TEXT("The tunable Cart keeps its safety constraint"), SoftConstraint))
	{
		return false;
	}
	TestTrue(TEXT("Invalid Blueprint angles are defensively ordered at runtime"),
		SoftCart->EffectiveStabilizationDeadZoneDegrees < SoftCart->EffectiveStabilizationFullAssistDegrees
		&& SoftCart->EffectiveStabilizationFullAssistDegrees < SoftCart->EffectiveMaximumTiltAngleDegrees);
	TestTrue(TEXT("The sanitized maximum angle configures Swing 1"),
		FMath::IsNearlyEqual(SoftConstraint->ConstraintInstance.GetAngularSwing1Limit(), 20.0f));
	TestTrue(TEXT("The Blueprint soft-limit switch reaches Chaos configuration"),
		SoftConstraint->ConstraintInstance.GetIsSoftSwingLimit());
	TestTrue(TEXT("Blueprint soft-limit stiffness reaches Chaos configuration"),
		FMath::IsNearlyEqual(SoftConstraint->ConstraintInstance.GetSoftSwingLimitStiffness(), 75.0f));
	TestTrue(TEXT("Blueprint soft-limit damping reaches Chaos configuration"),
		FMath::IsNearlyEqual(SoftConstraint->ConstraintInstance.GetSoftSwingLimitDamping(), 10.0f));
	TestTrue(TEXT("Blueprint constraint restitution reaches Chaos configuration"),
		FMath::IsNearlyEqual(SoftConstraint->ConstraintInstance.GetSoftSwingLimitRestitution(), 0.25f));

	return true;
}

#endif

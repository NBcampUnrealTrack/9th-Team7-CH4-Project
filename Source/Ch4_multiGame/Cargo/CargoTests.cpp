// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cargo/CargoActor.h"
#include "Cargo/CargoDataAsset.h"
#include "Ch4_multiGameGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowRuleInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/UnrealType.h"

namespace Ch4CargoTests
{
	struct FCargoTestWorld
	{
		UWorld* World = nullptr;

		bool Initialize()
		{
			if (!GEngine)
			{
				return false;
			}

			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World)
			{
				return false;
			}

			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			return true;
		}

		IGameFlowRuleInterface* InitializeGameFlow(ACh4_multiGameGameState*& OutGameState) const
		{
			OutGameState = World ? World->SpawnActor<ACh4_multiGameGameState>() : nullptr;
			if (!OutGameState)
			{
				return nullptr;
			}

			World->SetGameState(OutGameState);
			ACh4_multiGameGameMode* GameMode = World->SpawnActor<ACh4_multiGameGameMode>();
			return Cast<IGameFlowRuleInterface>(GameMode);
		}

		~FCargoTestWorld()
		{
			if (World)
			{
				World->SetGameState(nullptr);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CargoDataApplicationTest,
	"Ch4_multiGame.Cargo.DataApplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CargoDataApplicationTest::RunTest(const FString& Parameters)
{
	using namespace Ch4CargoTests;

	FCargoTestWorld TestWorld;
	if (!TestWorld.Initialize())
	{
		AddError(TEXT("Failed to initialize the Cargo data test world."));
		return false;
	}

	ACargoActor* Cargo = TestWorld.World->SpawnActor<ACargoActor>();
	TestNotNull(TEXT("Cargo Actor is valid"), Cargo);
	if (!Cargo)
	{
		return false;
	}

	TestFalse(TEXT("CargoData is required for data application"), Cargo->ApplyCargoDataForTesting());
	TestTrue(TEXT("Cargo Actor replication is enabled"), Cargo->GetIsReplicated());
	TestTrue(TEXT("Cargo movement replication is enabled"), Cargo->IsReplicatingMovement());
	TestFalse(TEXT("Cargo Tick is disabled"), Cargo->PrimaryActorTick.bCanEverTick);

	UCargoDataAsset* CargoData = NewObject<UCargoDataAsset>(GetTransientPackage());
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UPhysicalMaterial* PhysicalMaterial = NewObject<UPhysicalMaterial>(CargoData);
	TestNotNull(TEXT("Cargo DataAsset is valid"), CargoData);
	TestNotNull(TEXT("Engine test mesh is valid"), CubeMesh);
	TestNotNull(TEXT("Physical Material is valid"), PhysicalMaterial);
	if (!CargoData || !CubeMesh || !PhysicalMaterial)
	{
		return false;
	}

	TestTrue(TEXT("Ground impact breaking is enabled by default"), CargoData->bBreakableFromGroundImpact);
	TestEqual(TEXT("Default delivery score is 100"), CargoData->DeliveryScore, 100);
	TestEqual(TEXT("Default break threshold is three impacts"), CargoData->GroundImpactsToBreak, 3);
	TestEqual(TEXT("Default minimum ground impulse is practical for a 20 kg Cargo"),
		CargoData->MinimumGroundImpactImpulse, 8000.0f);
	TestEqual(TEXT("Default ground impact cooldown is 0.4 seconds"),
		CargoData->GroundImpactCooldownSeconds, 0.4f);
	TestEqual(TEXT("Default minimum ground normal Z is 0.6"), CargoData->MinimumGroundNormalZ, 0.6f);
	TestTrue(TEXT("Break effect is optional by default"), CargoData->BreakEffect == nullptr);
	TestTrue(TEXT("Break effect scale defaults to one"), CargoData->BreakEffectScale == FVector::OneVector);

	CargoData->StaticMesh = CubeMesh;
	CargoData->MassKg = 42.0f;
	CargoData->LinearDamping = 0.35f;
	CargoData->AngularDamping = 0.65f;
	CargoData->PhysicalMaterial = PhysicalMaterial;
	CargoData->bEnableGravity = false;
	Cargo->SetCargoDataForTesting(CargoData);

	TestTrue(TEXT("Valid CargoData applies successfully"), Cargo->ApplyCargoDataForTesting());
	UStaticMeshComponent* CargoMesh = Cast<UStaticMeshComponent>(Cargo->GetRootComponent());
	TestNotNull(TEXT("StaticMeshComponent is the root"), CargoMesh);
	if (!CargoMesh)
	{
		return false;
	}

	TestTrue(TEXT("Cargo Mesh requests rigid-body hit notifications"),
		CargoMesh->GetBodyInstance()->bNotifyRigidBodyCollision);
	TestTrue(TEXT("Static Mesh is applied"), CargoMesh->GetStaticMesh() == CubeMesh);
	TestEqual(TEXT("Mass is applied"), CargoMesh->GetBodyInstance()->GetMassOverride(), 42.0f);
	TestEqual(TEXT("Linear damping is applied"), CargoMesh->GetLinearDamping(), 0.35f);
	TestEqual(TEXT("Angular damping is applied"), CargoMesh->GetAngularDamping(), 0.65f);
	TestTrue(TEXT("Physical Material is applied"),
		CargoMesh->GetBodyInstance()->GetPhysMaterialOverride() == PhysicalMaterial);
	TestFalse(TEXT("Gravity setting is applied"), CargoMesh->IsGravityEnabled());

	CargoData->MassKg = 0.0f;
	CargoData->LinearDamping = -1.0f;
	CargoData->AngularDamping = -1.0f;
	TestTrue(TEXT("Invalid numeric values use safe fallbacks without rejecting the mesh"), Cargo->ApplyCargoDataForTesting());
	TestEqual(TEXT("Invalid mass falls back to one kilogram"), CargoMesh->GetBodyInstance()->GetMassOverride(), 1.0f);
	TestEqual(TEXT("Negative linear damping clamps to zero"), CargoMesh->GetLinearDamping(), 0.0f);
	TestEqual(TEXT("Negative angular damping clamps to zero"), CargoMesh->GetAngularDamping(), 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CargoScoreTest,
	"Ch4_multiGame.Cargo.Score",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CargoScoreTest::RunTest(const FString& Parameters)
{
	using namespace Ch4CargoTests;

	FCargoTestWorld TestWorld;
	if (!TestWorld.Initialize())
	{
		AddError(TEXT("Failed to initialize the Cargo score test world."));
		return false;
	}

	ACargoActor* Cargo = TestWorld.World->SpawnActor<ACargoActor>();
	UCargoDataAsset* CargoData = NewObject<UCargoDataAsset>(GetTransientPackage());
	TestNotNull(TEXT("Score-test Cargo Actor is valid"), Cargo);
	TestNotNull(TEXT("Score-test CargoData is valid"), CargoData);
	if (!Cargo || !CargoData)
	{
		return false;
	}

	TestEqual(TEXT("Cargo without data has zero delivery score"), Cargo->GetDeliveryScore(), 0);
	CargoData->DeliveryScore = 100;
	Cargo->SetCargoDataForTesting(CargoData);
	TestEqual(TEXT("Active Cargo returns its configured delivery score"), Cargo->GetDeliveryScore(), 100);

	CargoData->DeliveryScore = -50;
	TestEqual(TEXT("Invalid negative delivery score clamps to zero"), Cargo->GetDeliveryScore(), 0);
	CargoData->DeliveryScore = 300;
	TestEqual(TEXT("Active Cargo reflects updated static score data"), Cargo->GetDeliveryScore(), 300);

	ACh4_multiGameGameState* GameState = nullptr;
	IGameFlowRuleInterface* GameFlowRule = TestWorld.InitializeGameFlow(GameState);
	TestNotNull(TEXT("Score-test GameFlow state is valid"), GameState);
	TestNotNull(TEXT("Score-test GameFlow rule is valid"), GameFlowRule);
	if (!GameState || !GameFlowRule)
	{
		return false;
	}

	TestTrue(TEXT("Score-test GameFlow initializes"), GameFlowRule->RequestCargoInitialization(1));
	TestTrue(TEXT("Score-test GameFlow starts"), GameFlowRule->RequestGameStart());
	Cargo->SetGameFlowRuleOverrideForTesting(GameFlowRule);
	TestTrue(TEXT("Cargo can become Lost through the existing path"), Cargo->MarkAsLost());
	TestEqual(TEXT("Lost Cargo contributes zero delivery score"), Cargo->GetDeliveryScore(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CargoGroundImpactTest,
	"Ch4_multiGame.Cargo.GroundImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CargoGroundImpactTest::RunTest(const FString& Parameters)
{
	using namespace Ch4CargoTests;

	FCargoTestWorld TestWorld;
	if (!TestWorld.Initialize())
	{
		AddError(TEXT("Failed to initialize the Cargo ground-impact test world."));
		return false;
	}

	ACh4_multiGameGameState* GameState = nullptr;
	IGameFlowRuleInterface* GameFlowRule = TestWorld.InitializeGameFlow(GameState);
	UCargoDataAsset* CargoData = NewObject<UCargoDataAsset>(GetTransientPackage());
	ACargoActor* RejectedCargo = TestWorld.World->SpawnActor<ACargoActor>();
	TestNotNull(TEXT("GameFlow state is valid"), GameState);
	TestNotNull(TEXT("GameFlow rule is valid"), GameFlowRule);
	TestNotNull(TEXT("Ground-impact CargoData is valid"), CargoData);
	TestNotNull(TEXT("Rejected Cargo Actor is valid"), RejectedCargo);
	if (!GameState || !GameFlowRule || !CargoData || !RejectedCargo)
	{
		return false;
	}

	CargoData->GroundImpactsToBreak = 0;
	CargoData->MinimumGroundImpactImpulse = 8000.0f;
	CargoData->GroundImpactCooldownSeconds = 0.4f;
	CargoData->MinimumGroundNormalZ = 0.6f;
	CargoData->BreakEffect = nullptr;
	RejectedCargo->SetCargoDataForTesting(CargoData);
	RejectedCargo->SetGameFlowRuleOverrideForTesting(GameFlowRule);

	TestEqual(TEXT("Cargo starts with zero ground impacts"), RejectedCargo->GetGroundImpactCount(), 0);
	TestTrue(TEXT("GameFlow initializes two Cargo"), GameFlowRule->RequestCargoInitialization(2));
	TestTrue(TEXT("Invalid zero threshold safely falls back to one impact"),
		RejectedCargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 9000.0f, 1.0));
	TestFalse(TEXT("GameFlow rejection leaves Cargo Active"), RejectedCargo->IsLost());
	TestFalse(TEXT("GameFlow rejection does not destroy Cargo"), RejectedCargo->IsActorBeingDestroyed());
	TestEqual(TEXT("Rejected threshold hit is rolled back"), RejectedCargo->GetGroundImpactCount(), 0);
	TestEqual(TEXT("Rejected loss does not decrement GameFlow"), GameState->GetRemainingCargoCount(), 2);

	TestTrue(TEXT("GameFlow starts"), GameFlowRule->RequestGameStart());
	TestTrue(TEXT("A new strong impact after Playing is accepted"),
		RejectedCargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 9000.0f, 1.5));
	TestTrue(TEXT("Accepted threshold marks Cargo Lost"), RejectedCargo->IsLost());
	TestTrue(TEXT("Lost Cargo is scheduled for destruction"), RejectedCargo->IsActorBeingDestroyed());
	TestEqual(TEXT("First destroyed Cargo decrements GameFlow once"), GameState->GetRemainingCargoCount(), 1);

	CargoData->GroundImpactsToBreak = 3;
	ACargoActor* Cargo = TestWorld.World->SpawnActor<ACargoActor>();
	TestNotNull(TEXT("Filter-test Cargo Actor is valid"), Cargo);
	if (!Cargo)
	{
		return false;
	}

	Cargo->SetCargoDataForTesting(CargoData);
	Cargo->SetGameFlowRuleOverrideForTesting(GameFlowRule);

	CargoData->bBreakableFromGroundImpact = false;
	TestFalse(TEXT("Disabled ground-impact breaking ignores otherwise valid collision"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 12000.0f, 1.0));
	CargoData->bBreakableFromGroundImpact = true;
	TestFalse(TEXT("Physics-body Cargo or Cart collision is ignored"),
		Cargo->ProcessGroundImpactForTesting(ECC_PhysicsBody, false, 1.0f, 12000.0f, 1.0));
	TestFalse(TEXT("Movable WorldStatic collision is ignored"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, false, 1.0f, 12000.0f, 1.0));
	TestFalse(TEXT("WorldStatic wall collision is ignored"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 0.2f, 12000.0f, 1.0));
	TestFalse(TEXT("Weak static-ground collision is ignored"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 7000.0f, 1.0));
	TestEqual(TEXT("Rejected collisions do not change the count"), Cargo->GetGroundImpactCount(), 0);

	TestTrue(TEXT("First valid ground impact is accepted"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 9000.0f, 2.0));
	TestEqual(TEXT("First valid impact increments count"), Cargo->GetGroundImpactCount(), 1);
	TestFalse(TEXT("Bounce inside cooldown is ignored"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 9000.0f, 2.2));
	TestEqual(TEXT("Cooldown hit does not increment count"), Cargo->GetGroundImpactCount(), 1);
	TestTrue(TEXT("Second valid impact after cooldown is accepted"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 9000.0f, 2.5));
	TestEqual(TEXT("Second valid impact increments count"), Cargo->GetGroundImpactCount(), 2);
	TestFalse(TEXT("Cargo remains Active before threshold"), Cargo->IsLost());

	TestTrue(TEXT("Third valid impact reaches threshold"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 9000.0f, 3.0));
	TestEqual(TEXT("Threshold count is retained on successful break"), Cargo->GetGroundImpactCount(), 3);
	TestTrue(TEXT("Threshold marks Cargo Lost"), Cargo->IsLost());
	TestTrue(TEXT("Null BreakEffect still allows destruction"), Cargo->IsActorBeingDestroyed());
	TestEqual(TEXT("Second destroyed Cargo decrements GameFlow once"), GameState->GetRemainingCargoCount(), 0);
	TestFalse(TEXT("Impacts after Lost are ignored"),
		Cargo->ProcessGroundImpactForTesting(ECC_WorldStatic, true, 1.0f, 9000.0f, 4.0));

	const FProperty* GroundImpactCountProperty =
		FindFProperty<FProperty>(ACargoActor::StaticClass(), TEXT("GroundImpactCount"));
	TestTrue(TEXT("GroundImpactCount is intentionally not replicated"),
		GroundImpactCountProperty && !GroundImpactCountProperty->HasAnyPropertyFlags(CPF_Net));

	const UFunction* BreakEffectFunction =
		ACargoActor::StaticClass()->FindFunctionByName(TEXT("MulticastPlayBreakEffect"));
	TestNotNull(TEXT("Break effect multicast exists"), BreakEffectFunction);
	if (BreakEffectFunction)
	{
		TestTrue(TEXT("Break effect RPC is multicast"), BreakEffectFunction->HasAnyFunctionFlags(FUNC_NetMulticast));
		TestTrue(TEXT("Break effect RPC is reliable"), BreakEffectFunction->HasAnyFunctionFlags(FUNC_NetReliable));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CargoStateTest,
	"Ch4_multiGame.Cargo.State",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CargoStateTest::RunTest(const FString& Parameters)
{
	using namespace Ch4CargoTests;

	FCargoTestWorld TestWorld;
	if (!TestWorld.Initialize())
	{
		AddError(TEXT("Failed to initialize the Cargo state test world."));
		return false;
	}

	ACh4_multiGameGameState* GameState = nullptr;
	IGameFlowRuleInterface* GameFlowRule = TestWorld.InitializeGameFlow(GameState);
	ACargoActor* Cargo = TestWorld.World->SpawnActor<ACargoActor>();
	TestNotNull(TEXT("GameFlow state is valid"), GameState);
	TestNotNull(TEXT("GameFlow rule is valid"), GameFlowRule);
	TestNotNull(TEXT("Cargo Actor is valid"), Cargo);
	if (!GameState || !GameFlowRule || !Cargo)
	{
		return false;
	}

	const FProperty* CargoStateProperty = FindFProperty<FProperty>(ACargoActor::StaticClass(), TEXT("CargoState"));
	TestTrue(TEXT("CargoState property is replicated"),
		CargoStateProperty && CargoStateProperty->HasAnyPropertyFlags(CPF_Net));
	TestEqual(TEXT("Initial state is Active"),
		static_cast<uint8>(Cargo->GetCargoState()), static_cast<uint8>(ECargoState::Active));
	TestFalse(TEXT("Initial Cargo is not Lost"), Cargo->IsLost());

	TestTrue(TEXT("GameFlow initializes two Cargo"), GameFlowRule->RequestCargoInitialization(2));
	TestTrue(TEXT("GameFlow starts"), GameFlowRule->RequestGameStart());
	Cargo->SetGameFlowRuleOverrideForTesting(GameFlowRule);
	TestTrue(TEXT("First Lost transition succeeds"), Cargo->MarkAsLost());
	TestTrue(TEXT("Cargo reports Lost"), Cargo->IsLost());
	TestEqual(TEXT("GameFlow decrements exactly once"), GameState->GetRemainingCargoCount(), 1);

	TestFalse(TEXT("Duplicate Lost transition is rejected"), Cargo->MarkAsLost());
	TestEqual(TEXT("Duplicate Lost does not notify GameFlow again"), GameState->GetRemainingCargoCount(), 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

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

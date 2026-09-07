// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cart/CartCargoTrackerComponent.h"
#include "Cargo/CargoActor.h"
#include "Cargo/CargoDataAsset.h"
#include "Ch4_multiGameGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowRuleInterface.h"

namespace Ch4CartTests
{
	struct FCartTestWorld
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
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			return true;
		}

		UCartCargoTrackerComponent* CreateTracker() const
		{
			AActor* Owner = World->SpawnActor<AActor>();
			if (!Owner)
			{
				return nullptr;
			}
			UCartCargoTrackerComponent* Tracker = NewObject<UCartCargoTrackerComponent>(Owner);
			Owner->AddInstanceComponent(Tracker);
			Owner->SetRootComponent(Tracker);
			Tracker->RegisterComponent();
			return Tracker;
		}

		ACargoActor* CreateCargo(int32 Score) const
		{
			ACargoActor* Cargo = World->SpawnActor<ACargoActor>();
			if (Cargo)
			{
				UCargoDataAsset* Data = NewObject<UCargoDataAsset>(Cargo);
				Data->DeliveryScore = Score;
				Cargo->SetCargoDataForTesting(Data);
			}
			return Cargo;
		}

		~FCartTestWorld()
		{
			if (World)
			{
				World->EndPlay(EEndPlayReason::Quit);
				World->SetGameState(nullptr);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void Enter(UCartCargoTrackerComponent* Tracker, AActor* Actor)
	{
		Tracker->OnComponentBeginOverlap.Broadcast(Tracker, Actor,
			Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr, 0, false, FHitResult());
	}

	void Leave(UCartCargoTrackerComponent* Tracker, ACargoActor* Cargo)
	{
		Tracker->OnComponentEndOverlap.Broadcast(Tracker, Cargo,
			Cast<UPrimitiveComponent>(Cargo->GetRootComponent()), 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CartCargoTrackingTest,
	"Ch4_multiGame.Cart.CargoTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CartCargoTrackingTest::RunTest(const FString& Parameters)
{
	using namespace Ch4CartTests;
	FCartTestWorld TestWorld;
	if (!TestWorld.Initialize())
	{
		AddError(TEXT("Failed to initialize the Cart tracking test world."));
		return false;
	}

	UCartCargoTrackerComponent* Tracker = TestWorld.CreateTracker();
	ACargoActor* CargoA = TestWorld.CreateCargo(100);
	ACargoActor* CargoB = TestWorld.CreateCargo(300);
	ACargoActor* OutsideCargo = TestWorld.CreateCargo(900);
	ACh4_multiGameGameState* GameState = TestWorld.World->SpawnActor<ACh4_multiGameGameState>();
	TestWorld.World->SetGameState(GameState);
	ACh4_multiGameGameMode* GameMode = TestWorld.World->SpawnActor<ACh4_multiGameGameMode>();
	if (!Tracker || !CargoA || !CargoB || !OutsideCargo || !GameState || !GameMode)
	{
		AddError(TEXT("Failed to create the Cart tracking fixtures."));
		return false;
	}
	Tracker->GetOwner()->DispatchBeginPlay();

	TestFalse(TEXT("Tracker never ticks"), Tracker->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Tracker does not replicate membership"), Tracker->GetIsReplicated());
	TestFalse(TEXT("Detection box cannot auto-weld into the chassis"), Tracker->BodyInstance.bAutoWeld);
	TestFalse(TEXT("Detection box does not simulate physics"), Tracker->BodyInstance.bSimulatePhysics);
	TestEqual(TEXT("Detection is query-only"), Tracker->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Detection object type is WorldDynamic"), Tracker->GetCollisionObjectType(), ECC_WorldDynamic);
	TestTrue(TEXT("Detection generates overlap events"), Tracker->GetGenerateOverlapEvents());
	for (int32 Channel = 0; Channel < ECC_OverlapAll_Deprecated; ++Channel)
	{
		TestEqual(FString::Printf(TEXT("Collision response for channel %d"), Channel),
			Tracker->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Channel)),
			Channel == ECC_PhysicsBody ? ECR_Overlap : ECR_Ignore);
	}

	FCh4DeliveryScoreSummary Summary = Tracker->BuildDeliveryScoreSummary();
	TestTrue(TEXT("An empty authoritative Cart has score data"), Summary.bHasScoreData);
	TestEqual(TEXT("Empty count is zero"), Summary.DeliveredCargoCount, 0);
	TestEqual(TEXT("Empty score is zero"), Summary.DeliveredCargoScore, 0);
	Enter(Tracker, nullptr);
	Enter(Tracker, Tracker->GetOwner());
	TestEqual(TEXT("Null and non-Cargo actors are ignored"), Tracker->GetTrackedCargoCount(), 0);

	Enter(Tracker, CargoA);
	TestEqual(TEXT("A enters"), Tracker->GetTrackedCargoCount(), 1);
	Enter(Tracker, CargoA);
	TestEqual(TEXT("Duplicate A is not counted twice"), Tracker->GetTrackedCargoCount(), 1);
	Enter(Tracker, CargoB);
	TestEqual(TEXT("B enters"), Tracker->GetTrackedCargoCount(), 2);
	TestEqual(TEXT("Only A and B score; outside Cargo is excluded"), Tracker->GetTrackedCargoScore(), 400);
	Leave(Tracker, CargoA);
	TestEqual(TEXT("A leaves"), Tracker->GetTrackedCargoCount(), 1);
	TestFalse(TEXT("Leaving does not mark A Lost"), CargoA->IsLost());
	TestEqual(TEXT("Leaving removes A's score"), Tracker->GetTrackedCargoScore(), 300);
	Enter(Tracker, CargoA);
	TestEqual(TEXT("A can be reloaded"), Tracker->GetTrackedCargoCount(), 2);
	TestEqual(TEXT("Reload restores score"), Tracker->GetTrackedCargoScore(), 400);
	TestEqual(TEXT("Tracking does not initialize GameFlow Cargo"), GameState->GetInitialCargoCount(), 0);
	TestTrue(TEXT("Tracking does not start GameFlow"), GameState->GetCurrentGamePhase() == ECh4GamePhase::Waiting);

	AActor* Owner = Tracker->GetOwner();
	Owner->SetRole(ROLE_SimulatedProxy);
	Leave(Tracker, CargoA);
	Enter(Tracker, OutsideCargo);
	Summary = Tracker->BuildDeliveryScoreSummary();
	TestFalse(TEXT("Clients do not provide authoritative score data"), Summary.bHasScoreData);
	TestEqual(TEXT("Client score is unavailable and zero"), Summary.DeliveredCargoScore, 0);
	Owner->SetRole(ROLE_Authority);
	TestEqual(TEXT("Client overlap events did not change server membership"), Tracker->GetTrackedCargoCount(), 2);
	TestEqual(TEXT("Client events did not alter score"), Tracker->GetTrackedCargoScore(), 400);

	IGameFlowRuleInterface* GameRule = Cast<IGameFlowRuleInterface>(GameMode);
	TestTrue(TEXT("Test fixture initializes GameFlow explicitly"), GameRule->RequestCargoInitialization(3));
	TestTrue(TEXT("Test fixture starts GameFlow explicitly"), GameRule->RequestGameStart());
	CargoA->SetGameFlowRuleOverrideForTesting(GameRule);
	TestTrue(TEXT("Existing Cargo Lost path succeeds"), CargoA->MarkAsLost());
	TestEqual(TEXT("Lost A is excluded without an EndOverlap"), Tracker->GetTrackedCargoCount(), 1);
	TestEqual(TEXT("Lost A contributes no score"), Tracker->GetTrackedCargoScore(), 300);
	Enter(Tracker, CargoA);
	TestEqual(TEXT("Lost Cargo cannot re-register"), Tracker->GetTrackedCargoCount(), 1);

	// Use the existing GameMode test seam for the Blueprint's one-node summary forwarding.
	// The actual Blueprint interface wiring and Goal overlap remain Editor/PIE acceptance steps.
	GameMode->SetDeliveryScoreSummaryForTesting(Owner, Tracker->BuildDeliveryScoreSummary());
	TestTrue(TEXT("Goal accepts the Tracker summary"), GameRule->NotifyGoalReached(Owner));
	TestEqual(TEXT("GameFlow finalizes only the loaded Cargo score"), GameState->GetFinalCargoScore(), 300);
	TestEqual(TEXT("Delivery does not overwrite the existing remaining count"), GameState->GetRemainingCargoCount(), 2);

	TestTrue(TEXT("B can be destroyed without a Lost/EndOverlap sequence"), CargoB->Destroy());
	Summary = Tracker->BuildDeliveryScoreSummary();
	TestTrue(TEXT("Empty Cart still reports score data after destruction"), Summary.bHasScoreData);
	TestEqual(TEXT("Destroyed B is excluded from count"), Summary.DeliveredCargoCount, 0);
	TestEqual(TEXT("Destroyed B is excluded from score"), Summary.DeliveredCargoScore, 0);
	TestEqual(TEXT("Repeated snapshots remain empty"), Tracker->GetTrackedCargoCount(), 0);

	ACargoActor* NoDataCargo = TestWorld.World->SpawnActor<ACargoActor>();
	if (!NoDataCargo)
	{
		AddError(TEXT("Failed to create Cargo without a DataAsset."));
		return false;
	}
	Enter(Tracker, NoDataCargo);
	TestEqual(TEXT("Active Cargo without data still counts"), Tracker->GetTrackedCargoCount(), 1);
	TestEqual(TEXT("Missing data reuses Cargo's zero score"), Tracker->GetTrackedCargoScore(), 0);
	Leave(Tracker, NoDataCargo);

	ACargoActor* LargeScoreCargo = TestWorld.CreateCargo(MAX_int32);
	if (!LargeScoreCargo)
	{
		AddError(TEXT("Failed to create the large-score Cargo."));
		return false;
	}
	Enter(Tracker, LargeScoreCargo);
	Enter(Tracker, OutsideCargo);
	TestEqual(TEXT("Large positive sums do not overflow int32"), Tracker->GetTrackedCargoScore(), MAX_int32);
	Tracker->GetOwner()->Destroy();
	TestEqual(TEXT("EndPlay clears membership"), Tracker->GetTrackedCargoCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CartCargoOverlapTest,
	"Ch4_multiGame.Cart.CargoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CartCargoOverlapTest::RunTest(const FString& Parameters)
{
	using namespace Ch4CartTests;
	FCartTestWorld TestWorld;
	if (!TestWorld.Initialize())
	{
		AddError(TEXT("Failed to initialize the Cart overlap test world."));
		return false;
	}
	UCartCargoTrackerComponent* Tracker = TestWorld.CreateTracker();
	ACargoActor* Cargo = TestWorld.CreateCargo(100);
	if (!Tracker || !Cargo)
	{
		AddError(TEXT("Failed to create the Cart overlap fixtures."));
		return false;
	}
	// Two query shapes on one Cargo exercise engine overlap bookkeeping without physics timing.
	UStaticMesh* TestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestMesh)
	{
		AddError(TEXT("Failed to load the engine's test mesh."));
		return false;
	}
	Cargo->GetCargoData()->StaticMesh = TestMesh;
	UPrimitiveComponent* CargoMesh = Cast<UPrimitiveComponent>(Cargo->GetRootComponent());
	CargoMesh->SetSimulatePhysics(false);
	CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	auto AddCargoShape = [Cargo]()
	{
		UBoxComponent* Shape = NewObject<UBoxComponent>(Cargo);
		Cargo->AddInstanceComponent(Shape);
		Shape->SetupAttachment(Cargo->GetRootComponent());
		Shape->InitBoxExtent(FVector(10.0f));
		Shape->BodyInstance.bAutoWeld = false;
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Shape->SetCollisionObjectType(ECC_PhysicsBody);
		Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
		Shape->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Shape->SetGenerateOverlapEvents(true);
		Shape->RegisterComponent();
		return Shape;
	};
	UBoxComponent* FirstShape = AddCargoShape();
	UBoxComponent* SecondShape = AddCargoShape();
	// Scene queries reject uninitialized actors in a Game world.
	TestWorld.World->InitializeActorsForPlay(FURL());
	Cargo->DispatchBeginPlay();
	Cargo->UpdateOverlaps();
	TestTrue(TEXT("Cargo establishes overlap before the Tracker's BeginPlay"), Tracker->IsOverlappingActor(Cargo));
	Tracker->GetOwner()->DispatchBeginPlay();
	TestEqual(TEXT("BeginPlay seeds pre-existing overlaps exactly once"), Tracker->GetTrackedCargoCount(), 1);
	TestEqual(TEXT("Preloaded Cargo contributes its score"), Tracker->GetTrackedCargoScore(), 100);
	// Both actors have begun play; enable the world's overlap delegate notifications as well.
	TestWorld.World->SetBegunPlay(true);

	FirstShape->SetWorldLocation(FVector(500.0f, 0.0f, 0.0f));
	TestTrue(TEXT("Second component still overlaps"), Tracker->IsOverlappingActor(Cargo));
	TestEqual(TEXT("Partial component exit retains Cargo"), Tracker->GetTrackedCargoCount(), 1);
	SecondShape->SetWorldLocation(FVector(500.0f, 0.0f, 0.0f));
	TestFalse(TEXT("All Cargo components left"), Tracker->IsOverlappingActor(Cargo));
	TestEqual(TEXT("Last component exit removes Cargo"), Tracker->GetTrackedCargoCount(), 0);
	TestFalse(TEXT("Actual EndOverlap never marks Cargo Lost"), Cargo->IsLost());
	FirstShape->SetWorldLocation(FVector::ZeroVector);
	TestEqual(TEXT("Actual re-entry registers Cargo again"), Tracker->GetTrackedCargoCount(), 1);
	SecondShape->SetWorldLocation(FVector::ZeroVector);
	TestEqual(TEXT("Second component entry does not duplicate Cargo"), Tracker->GetTrackedCargoCount(), 1);
	TestTrue(TEXT("Tracked Cargo can be destroyed during overlap"), Cargo->Destroy());
	TestEqual(TEXT("Destruction during overlap contributes no score"), Tracker->GetTrackedCargoScore(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

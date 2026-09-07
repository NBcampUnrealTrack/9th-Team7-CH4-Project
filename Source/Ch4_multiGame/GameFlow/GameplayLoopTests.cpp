// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Cargo/CargoActor.h"
#include "Cargo/CargoDataAsset.h"
#include "Cart/CartCargoTrackerComponent.h"
#include "Ch4_multiGameGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/FinalDeliveryZone.h"
#include "GameFlow/GameplayPhaseTransitionPoint.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Map/FinalDeliveryZoneComponent.h"
#include "TimerManager.h"

namespace Ch4GameplayLoopTests
{
	struct FWorldFixture
	{
		UWorld* World = nullptr;
		ACh4_multiGameGameMode* Rule = nullptr;
		ACh4_multiGameGameState* State = nullptr;
		bool Initialize()
		{
			if (!GEngine) return false;
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) return false;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			State = World->SpawnActor<ACh4_multiGameGameState>();
			World->SetGameState(State);
			Rule = World->SpawnActor<ACh4_multiGameGameMode>();
			return State && Rule;
		}
		void TickTimers(float Delta = 0.1f) const
		{
			// FTimerManager runs once per engine frame. No solver step or wall-clock sleep is required.
			++GFrameCounter;
			World->GetTimerManager().Tick(Delta);
		}
		~FWorldFixture()
		{
			if (!World) return;
			World->EndPlay(EEndPlayReason::Quit);
			World->SetGameState(nullptr);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4DeliveredCargoRuleTest,
	"Ch4_multiGame.GameFlow.DeliveredCargoRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4DeliveredCargoRuleTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameplayLoopTests;
	FWorldFixture F;
	if (!F.Initialize()) return false;
	FCh4GameRuleConfig Config;
	Config.MinimumCargoCountToClear = 2;
	F.Rule->SetGameRuleConfigForTesting(Config);
	F.Rule->RequestCargoInitialization(5);
	F.Rule->RequestGameStart();
	AActor* Target = F.World->SpawnActor<AActor>();
	FCh4DeliveryScoreSummary Summary;
	Summary.bHasScoreData = true;
	F.Rule->SetDeliveryScoreSummaryForTesting(Target, Summary);
	TestFalse(TEXT("Five active Cargo elsewhere cannot clear an empty Cart"), F.Rule->NotifyGoalReached(Target));
	Summary.DeliveredCargoCount = 1;
	Summary.DeliveredCargoScore = 100;
	F.Rule->SetDeliveryScoreSummaryForTesting(Target, Summary);
	TestFalse(TEXT("One delivered Cargo is below the configured minimum of two"), F.Rule->NotifyGoalReached(Target));
	TestEqual(TEXT("Rejected delivery keeps Playing and zero final score"), F.State->GetCurrentGamePhase(), ECh4GamePhase::Playing);
	TestEqual(TEXT("Score is not committed on rejection"), F.State->GetFinalCargoScore(), 0);
	Summary.DeliveredCargoCount = 2;
	Summary.DeliveredCargoScore = 400;
	F.Rule->SetDeliveryScoreSummaryForTesting(Target, Summary);
	TestTrue(TEXT("Two actually delivered Cargo satisfy the minimum"), F.Rule->NotifyGoalReached(Target));
	TestEqual(TEXT("Only delivered score is committed"), F.State->GetFinalCargoScore(), 400);
	TestEqual(TEXT("Global survival state is not overwritten by delivered count"), F.State->GetRemainingCargoCount(), 5);
	Summary.DeliveredCargoScore = 999;
	F.Rule->SetDeliveryScoreSummaryForTesting(Target, Summary);
	TestFalse(TEXT("Duplicate terminal score is rejected"), F.Rule->NotifyGoalReached(Target));
	TestEqual(TEXT("Terminal score stays immutable"), F.State->GetFinalCargoScore(), 400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4LobbyReturnTest,
	"Ch4_multiGame.GameFlow.LobbyReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4LobbyReturnTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameplayLoopTests;
	FWorldFixture F;
	if (!F.Initialize()) return false;
	TestTrue(TEXT("Automatic clear return defaults on"), F.Rule->bAutoReturnToLobbyOnClear);
	TestEqual(TEXT("Result display defaults to ten seconds"), F.Rule->ReturnToLobbyDelaySeconds, 10.0f);
	int32 Travels = 0;
	FString LastURL;
	F.Rule->LobbyTravelForTesting = [&Travels, &LastURL](const FString& URL) { ++Travels; LastURL = URL; return true; };
	TestFalse(TEXT("Waiting cannot schedule a return"), F.Rule->ScheduleReturnToLobby());
	TestFalse(TEXT("Waiting cannot travel"), F.Rule->ReturnToLobby());
	F.Rule->RequestCargoInitialization(1);
	F.Rule->RequestGameStart();
	TestFalse(TEXT("Playing cannot schedule a return"), F.Rule->ScheduleReturnToLobby());
	F.Rule->ReturnToLobbyDelaySeconds = 0.5f;
	TestTrue(TEXT("Providerless debug target keeps legacy clear behavior"), F.Rule->NotifyGoalReached(F.World->SpawnActor<AActor>()));
	TestTrue(TEXT("Clear schedules one return"), F.Rule->IsReturnToLobbyScheduled());
	TestFalse(TEXT("Duplicate schedule is rejected"), F.Rule->ScheduleReturnToLobby());
	F.TickTimers();
	TestEqual(TEXT("Result delay does not travel immediately"), Travels, 0);
	for (int32 I = 0; I < 8; ++I) F.TickTimers();
	TestEqual(TEXT("Timer calls the server travel route once"), Travels, 1);
	TestEqual(TEXT("Configured actual Lobby package is used"), LastURL, FString(TEXT("/Game/Lobby/L_Lobby")));
	TestFalse(TEXT("Duplicate travel is rejected"), F.Rule->ReturnToLobby());
	TestFalse(TEXT("Travel cannot be scheduled again after dispatch"), F.Rule->ScheduleReturnToLobby());

	FWorldFixture Failed;
	if (!Failed.Initialize()) return false;
	Failed.Rule->RequestCargoInitialization(1);
	Failed.Rule->RequestGameStart();
	Failed.Rule->NotifyCargoLost(1);
	TestFalse(TEXT("GameOver has no automatic return"), Failed.Rule->IsReturnToLobbyScheduled());
	TestFalse(TEXT("GameOver does not enter the clear-only route"), Failed.Rule->ReturnToLobby());

	FWorldFixture Disabled;
	if (!Disabled.Initialize()) return false;
	Disabled.Rule->bAutoReturnToLobbyOnClear = false;
	Disabled.Rule->RequestCargoInitialization(1);
	Disabled.Rule->RequestGameStart();
	Disabled.Rule->NotifyGoalReached(Disabled.World->SpawnActor<AActor>());
	TestFalse(TEXT("Editor option disables automatic scheduling"), Disabled.Rule->IsReturnToLobbyScheduled());
	int32 Attempts = 0;
	Disabled.Rule->LobbyTravelForTesting = [&Attempts](const FString&) { return ++Attempts > 1; };
	TestFalse(TEXT("Rejected ServerTravel reports failure"), Disabled.Rule->ReturnToLobby());
	TestTrue(TEXT("A rejected request can be retried manually"), Disabled.Rule->ReturnToLobby());
	TestFalse(TEXT("Successful retry is guarded"), Disabled.Rule->ReturnToLobby());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4PreparationTransitionTest,
	"Ch4_multiGame.GameFlow.PreparationTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4PreparationTransitionTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameplayLoopTests;
	FWorldFixture F;
	if (!F.Initialize()) return false;
	// This read-only asset fixture also checks the current BP still has the expected native tracker/interface.
	UClass* CartClass = LoadClass<AActor>(nullptr, TEXT("/Game/CartTest/BP_ShoppingCart.BP_ShoppingCart_C"));
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CartClass || !Mesh) { AddError(TEXT("Actual Cart or test mesh could not load")); return false; }
	AActor* Cart = F.World->SpawnActor<AActor>(CartClass);
	AGameplayPhaseTransitionPoint* Point = F.World->SpawnActor<AGameplayPhaseTransitionPoint>();
	UCartCargoTrackerComponent* Tracker = Cart ? Cart->FindComponentByClass<UCartCargoTrackerComponent>() : nullptr;
	if (!Cart || !Point || !Tracker) return false;
	Point->bStartPreparationOnBeginPlay = false;
	Point->SetGameModeForTesting(F.Rule);
	Point->CartActor = Cart;
	Point->CartDestination = F.World->SpawnActor<ATargetPoint>();
	Point->CartDestination->SetActorLocationAndRotation(FVector(5000, 3000, 800), FRotator(0, 90, 0));
	TestEqual(TEXT("Preparation defaults to 60 seconds"), Point->PreparationDurationSeconds, 60.0f);
	TestFalse(TEXT("Transition never ticks"), Point->PrimaryActorTick.bCanEverTick);
	TArray<ACharacter*> Players;
	for (int32 I = 0; I < 2; ++I)
	{
		ATargetPoint* Destination = F.World->SpawnActor<ATargetPoint>();
		Destination->SetActorLocation(FVector(4500, 2500 + 300 * I, 950));
		Point->PlayerDestinationPoints.Add(Destination);
		APlayerController* PC = F.World->SpawnActor<APlayerController>();
		ACharacter* Pawn = F.World->SpawnActor<ACharacter>();
		if (!PC || !Pawn) return false;
		PC->Possess(Pawn);
		Players.Add(Pawn);
	}
	TArray<ACargoActor*> Cargo;
	for (int32 I = 0; I < 3; ++I)
	{
		ACargoActor* Item = F.World->SpawnActor<ACargoActor>();
		UCargoDataAsset* Data = NewObject<UCargoDataAsset>(Item);
		Data->StaticMesh = Mesh;
		Data->DeliveryScore = I == 0 ? 100 : I == 1 ? 300 : 500;
		Item->SetCargoDataForTesting(Data);
		Item->SetGameFlowRuleOverrideForTesting(F.Rule);
		Item->ApplyCargoDataForTesting();
		Item->SetActorScale3D(FVector(0.1f));
		Item->GetGrabbableComponent()->SetEnableGravity(false);
		Item->GetGrabbableComponent()->SetGenerateOverlapEvents(true);
		Item->SetActorLocation(Tracker->GetComponentLocation() + FVector(0, I * 15, 0), false, nullptr, ETeleportType::TeleportPhysics);
		Cargo.Add(Item);
	}
	F.World->InitializeActorsForPlay(FURL());
	Cart->DispatchBeginPlay();
	for (ACargoActor* Item : Cargo) Item->DispatchBeginPlay();
	Point->DispatchBeginPlay();
	F.World->SetBegunPlay(true);
	for (ACargoActor* Item : Cargo) Item->UpdateOverlaps();
	Tracker->UpdateOverlaps();
	TestEqual(TEXT("Current load has three tracked Cargo"), Tracker->GetTrackedCargoSnapshot().Num(), 3);
	const FTransform OldCart = Cart->GetActorTransform();
	TArray<FTransform> Relative;
	for (ACargoActor* Item : Cargo) Relative.Add(Item->GetActorTransform().GetRelativeTransform(OldCart));
	UPrimitiveComponent* FirstBody = Cargo[0]->GetGrabbableComponent();
	FirstBody->SetSimulatePhysics(true);
	FirstBody->SetPhysicsLinearVelocity(FVector(80, 40, 30));
	FirstBody->SetPhysicsAngularVelocityInRadians(FVector(0, 0, 3));
	const bool bOriginalSim = FirstBody->IsSimulatingPhysics();
	const ECollisionEnabled::Type OriginalCollision = FirstBody->GetCollisionEnabled();
	TestTrue(TEXT("Physics fixture actually simulates"), bOriginalSim);
	TestFalse(TEXT("Manual call cannot skip preparation"), Point->TryStartMainGameplay());
	Point->PreparationDurationSeconds = 0.0f;
	TestTrue(TEXT("Preparation timer schedules once"), Point->StartPreparationTimer());
	TestFalse(TEXT("Duplicate preparation timer is rejected"), Point->StartPreparationTimer());
	TestEqual(TEXT("Before timer expiry the phase is Waiting"), F.State->GetCurrentGamePhase(), ECh4GamePhase::Waiting);
	F.TickTimers();
	TestEqual(TEXT("Snapshot count initializes GameFlow exactly once"), F.State->GetInitialCargoCount(), 3);
	TestEqual(TEXT("Restore is deferred; still Waiting in teleport frame"), F.State->GetCurrentGamePhase(), ECh4GamePhase::Waiting);
	TestFalse(TEXT("Another caller cannot start during suspended physics"), F.Rule->RequestGameStart());
	TestFalse(TEXT("Load physics is suspended in teleport frame"), FirstBody->IsSimulatingPhysics());
	TestEqual(TEXT("Load collision is suspended"), FirstBody->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	for (int32 I = 0; I < Cargo.Num(); ++I)
	{
		TestTrue(TEXT("Cargo relative placement survives translated and rotated Cart move"), Cargo[I]->GetActorTransform().GetRelativeTransform(Cart->GetActorTransform()).Equals(Relative[I], 0.01f));
	}
	TestFalse(TEXT("A second transition cannot initialize or teleport again"), Point->TryStartMainGameplay());
	F.TickTimers();
	TestTrue(TEXT("Next tick completes the production transition"), Point->IsTransitionComplete());
	TestEqual(TEXT("Game starts only after physics restoration"), F.State->GetCurrentGamePhase(), ECh4GamePhase::Playing);
	TestEqual(TEXT("Original simulation state restored"), FirstBody->IsSimulatingPhysics(), bOriginalSim);
	TestEqual(TEXT("Original collision state restored"), FirstBody->GetCollisionEnabled(), OriginalCollision);
	TestTrue(TEXT("Old linear velocity is cleared"), FirstBody->GetPhysicsLinearVelocity().IsNearlyZero());
	TestTrue(TEXT("Old angular velocity is cleared"), FirstBody->GetPhysicsAngularVelocityInRadians().IsNearlyZero());
	TestEqual(TEXT("Tracker repopulates after temporary collision disable"), Tracker->GetTrackedCargoCount(), 3);
	TestTrue(TEXT("Existing Pawn identity and possession are preserved"), Players[0]->GetController() && Players[1]->GetController());
	TestFalse(TEXT("Players are not sent to the same point"), Players[0]->GetActorLocation().Equals(Players[1]->GetActorLocation()));
	for (ACharacter* Player : Players)
	{
		TestTrue(TEXT("Each player reaches one configured destination"), Point->PlayerDestinationPoints.ContainsByPredicate([Player](const ATargetPoint* Destination)
		{
			return Player->GetActorLocation().Equals(Destination->GetActorLocation(), 0.01f);
		}));
	}
	ACargoActor* ShopCargo = F.World->SpawnActor<ACargoActor>();
	ShopCargo->SetGameFlowRuleOverrideForTesting(F.Rule);
	TestFalse(TEXT("Cargo outside the initial load cannot decrement its count"), ShopCargo->MarkAsLost());
	TestEqual(TEXT("Unselected Cargo leaves Remaining unchanged"), F.State->GetRemainingCargoCount(), 3);
	Cargo[2]->SetActorLocation(FVector(10000, 10000, 10000), false, nullptr, ETeleportType::TeleportPhysics);
	Cargo[2]->UpdateOverlaps();
	// Do not inject a score here: exercise the actual Cart interface/native tracker provider path.
	TestTrue(TEXT("Arrival uses current Cart load, not the initial snapshot"), F.Rule->NotifyGoalReached(Cart));
	TestEqual(TEXT("Only remaining 100 + 300 Cargo contribute final score"), F.State->GetFinalCargoScore(), 400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4PreparationEmptyTest,
	"Ch4_multiGame.GameFlow.PreparationEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4PreparationEmptyTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameplayLoopTests;
	FWorldFixture F;
	if (!F.Initialize()) return false;
	UClass* CartClass = LoadClass<AActor>(nullptr, TEXT("/Game/CartTest/BP_ShoppingCart.BP_ShoppingCart_C"));
	if (!CartClass) return false;
	AActor* Cart = F.World->SpawnActor<AActor>(CartClass);
	AGameplayPhaseTransitionPoint* Point = F.World->SpawnActor<AGameplayPhaseTransitionPoint>();
	Point->bStartPreparationOnBeginPlay = false;
	Point->SetGameModeForTesting(F.Rule);
	Point->CartActor = Cart;
	Point->CartDestination = F.World->SpawnActor<ATargetPoint>();
	Point->CartDestination->SetActorLocation(FVector(5000, 0, 1000));
	Point->PreparationDurationSeconds = 0.0f;
	const FTransform Before = Cart->GetActorTransform();
	Point->StartPreparationTimer();
	F.TickTimers();
	F.TickTimers();
	TestEqual(TEXT("Empty preparation keeps Waiting"), F.State->GetCurrentGamePhase(), ECh4GamePhase::Waiting);
	TestEqual(TEXT("Empty preparation never initializes Cargo"), F.State->GetInitialCargoCount(), 0);
	TestTrue(TEXT("Empty Cart never teleports"), Cart->GetActorTransform().Equals(Before));
	TestFalse(TEXT("Empty transition remains incomplete"), Point->IsTransitionComplete());
	AFinalDeliveryZone* Goal = F.World->SpawnActor<AFinalDeliveryZone>();
	TArray<UBoxComponent*> Boxes;
	Goal->GetComponents(Boxes);
	TestEqual(TEXT("Thin wrapper owns exactly one collision box"), Boxes.Num(), 1);
	TestNotNull(TEXT("TriggerCollision is the implementation component"), Goal->FindComponentByClass<UFinalDeliveryZoneComponent>());
	TestEqual(TEXT("Original collision subobject name is retained"), Boxes[0]->GetFName(), FName(TEXT("TriggerCollision")));
	TestEqual(TEXT("Original root name is retained"), Goal->GetRootComponent()->GetFName(), FName(TEXT("Root")));
	return true;
}

#endif

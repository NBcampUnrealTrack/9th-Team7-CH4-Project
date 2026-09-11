// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Cargo/CargoActor.h"
#include "Cargo/CargoDataAsset.h"
#include "Cart/CartBase.h"
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
#include "UObject/UnrealType.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4EmptyCartGoalFailureTest,
	"Ch4_multiGame.GameFlow.EmptyCartGoalFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4EmptyCartGoalFailureTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameplayLoopTests;
	FWorldFixture F;
	if (!F.Initialize()) return false;
	F.Rule->SetResultDisplayDurationForTesting(1.0f);
	F.Rule->RequestCargoInitialization(3);
	F.Rule->RequestGameStart();
	F.Rule->SetGameplayTimesForTesting(100.0f, 200.0f);
	int32 LobbyTravels = 0;
	F.Rule->LobbyTravelForTesting = [&LobbyTravels](const FString&) { ++LobbyTravels; return true; };
	AActor* Target = F.World->SpawnActor<AActor>();
	FCh4DeliveryScoreSummary EmptySummary;
	EmptySummary.bHasScoreData = true;
	F.Rule->SetDeliveryScoreSummaryForTesting(Target, EmptySummary);

	TestTrue(TEXT("Empty scored Goal notification is accepted once as a pending failure"),
		F.Rule->NotifyGoalReached(Target));
	TestEqual(TEXT("Empty Goal remains Playing during the delay"),
		F.State->GetCurrentGamePhase(), ECh4GamePhase::Playing);
	TestEqual(TEXT("Pending empty Goal keeps final score zero"), F.State->GetFinalCargoScore(), 0);
	const FCh4GameResult EmptyResult = F.State->GetGameResult();
	TestTrue(TEXT("Empty Goal publishes its result immediately"), EmptyResult.bResultAvailable);
	TestFalse(TEXT("Immediate empty result is a failure"), EmptyResult.bSucceeded);
	TestEqual(TEXT("Empty result freezes time at Goal arrival"), EmptyResult.ClearTimeSeconds, 100.0f);
	TestEqual(TEXT("Empty result freezes delivered Cargo at zero"), EmptyResult.DeliveredCargoCount, 0);
	TestEqual(TEXT("Empty result freezes score at zero"), EmptyResult.FinalCargoScore, 0);
	// A synthetic world's first manual TimerManager tick establishes its internal time base.
	F.TickTimers(0.0f);
	F.TickTimers(0.6f);
	TestEqual(TEXT("Empty Goal has not failed before the configured delay"),
		F.State->GetCurrentGamePhase(), ECh4GamePhase::Playing);
	TestTrue(TEXT("Duplicate empty Goal notification reuses the existing countdown"),
		F.Rule->NotifyGoalReached(Target));
	F.TickTimers(0.5f);
	TestEqual(TEXT("Original one-shot countdown reaches GameOver without being restarted"),
		F.State->GetCurrentGamePhase(), ECh4GamePhase::GameOver);
	TestEqual(TEXT("Empty Cart GameOver uses the existing cargo-rule reason"),
		F.State->GetGameEndReason(), ECh4GameEndReason::CargoRuleFailed);
	TestEqual(TEXT("Empty Cart GameOver finalizes score at zero"), F.State->GetFinalCargoScore(), 0);
	TestEqual(TEXT("Empty Goal travels immediately after GameOver"), LobbyTravels, 1);
	TestEqual(TEXT("GameOver does not add another result countdown"),
		F.State->GetGameResult().ClearTimeSeconds, 100.0f);
	TestFalse(TEXT("Terminal GameOver rejects further Goal notifications"),
		F.Rule->NotifyGoalReached(Target));

	FWorldFixture Terminal;
	if (!Terminal.Initialize()) return false;
	Terminal.Rule->SetResultDisplayDurationForTesting(1.0f);
	Terminal.Rule->RequestCargoInitialization(1);
	Terminal.Rule->RequestGameStart();
	Terminal.Rule->SetGameplayTimesForTesting(10.0f, 25.0f);
	int32 TerminalTravels = 0;
	Terminal.Rule->LobbyTravelForTesting = [&TerminalTravels](const FString&) { ++TerminalTravels; return true; };
	AActor* TerminalTarget = Terminal.World->SpawnActor<AActor>();
	Terminal.Rule->SetDeliveryScoreSummaryForTesting(TerminalTarget, EmptySummary);
	TestTrue(TEXT("Second fixture schedules an empty failure"), Terminal.Rule->NotifyGoalReached(TerminalTarget));
	FCh4DeliveryScoreSummary DeliveredSummary;
	DeliveredSummary.bHasScoreData = true;
	DeliveredSummary.DeliveredCargoCount = 1;
	DeliveredSummary.DeliveredCargoScore = 250;
	Terminal.Rule->SetDeliveryScoreSummaryForTesting(TerminalTarget, DeliveredSummary);
	TestTrue(TEXT("A later duplicate Goal reuses the first empty result"),
		Terminal.Rule->NotifyGoalReached(TerminalTarget));
	Terminal.TickTimers(0.0f);
	Terminal.TickTimers(2.0f);
	TestEqual(TEXT("First empty result still reaches GameOver"),
		Terminal.State->GetCurrentGamePhase(), ECh4GamePhase::GameOver);
	TestEqual(TEXT("Later Cargo cannot mutate the first Goal score"),
		Terminal.State->GetGameResult().FinalCargoScore, 0);
	TestEqual(TEXT("Later Cargo cannot mutate the first Goal count"),
		Terminal.State->GetGameResult().DeliveredCargoCount, 0);
	TestEqual(TEXT("Later Cargo cannot mutate the first Goal time"),
		Terminal.State->GetGameResult().ClearTimeSeconds, 15.0f);
	TestEqual(TEXT("Empty result performs one Lobby travel"), TerminalTravels, 1);

	const FProperty* DelayProperty = FindFProperty<FProperty>(
		ACh4_multiGameGameMode::StaticClass(), TEXT("ResultDisplayDurationSeconds"));
	TestTrue(TEXT("Shared result duration is editable in GameMode Class Defaults"),
		DelayProperty && DelayProperty->HasAnyPropertyFlags(CPF_Edit));
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
	TestEqual(TEXT("Result display defaults to ten seconds"), F.Rule->ResultDisplayDurationSeconds, 10.0f);
	int32 Travels = 0;
	FString LastURL;
	F.Rule->LobbyTravelForTesting = [&Travels, &LastURL](const FString& URL) { ++Travels; LastURL = URL; return true; };
	TestFalse(TEXT("Waiting cannot schedule a return"), F.Rule->ScheduleReturnToLobby());
	TestFalse(TEXT("Waiting cannot travel"), F.Rule->ReturnToLobby());
	F.Rule->RequestCargoInitialization(1);
	F.Rule->RequestGameStart();
	TestFalse(TEXT("Playing cannot schedule a return"), F.Rule->ScheduleReturnToLobby());
	F.Rule->SetResultDisplayDurationForTesting(0.5f);
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

	FWorldFixture Retry;
	if (!Retry.Initialize()) return false;
	Retry.Rule->SetResultDisplayDurationForTesting(0.0f);
	Retry.Rule->RequestCargoInitialization(1);
	Retry.Rule->RequestGameStart();
	int32 Attempts = 0;
	Retry.Rule->LobbyTravelForTesting = [&Attempts](const FString&) { return ++Attempts > 1; };
	TestTrue(TEXT("Required clear return is scheduled"), Retry.Rule->NotifyGoalReached(Retry.World->SpawnActor<AActor>()));
	Retry.TickTimers();
	TestEqual(TEXT("Rejected scheduled ServerTravel attempted once"), Attempts, 1);
	TestTrue(TEXT("A rejected request can be retried manually"), Retry.Rule->ReturnToLobby());
	TestFalse(TEXT("Successful retry is guarded"), Retry.Rule->ReturnToLobby());

	FWorldFixture PreparationAbort;
	if (!PreparationAbort.Initialize()) return false;
	int32 PreparationTravels = 0;
	FString PreparationURL;
	PreparationAbort.Rule->LobbyTravelForTesting = [&PreparationTravels, &PreparationURL](const FString& URL)
	{
		++PreparationTravels;
		PreparationURL = URL;
		return true;
	};
	TestTrue(TEXT("Uninitialized Waiting can use the dedicated preparation-abort route"),
		PreparationAbort.Rule->ReturnToLobbyFromPreparation());
	TestEqual(TEXT("Preparation abort dispatches one Lobby travel"), PreparationTravels, 1);
	TestEqual(TEXT("Preparation abort uses the configured Lobby package"),
		PreparationURL, FString(TEXT("/Game/Lobby/L_Lobby")));
	TestEqual(TEXT("Preparation abort does not change Waiting to a terminal phase"),
		PreparationAbort.State->GetCurrentGamePhase(), ECh4GamePhase::Waiting);
	TestFalse(TEXT("Preparation abort travel is guarded against duplicates"),
		PreparationAbort.Rule->ReturnToLobbyFromPreparation());
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
	ACartBase* CartBase = Cast<ACartBase>(Cart);
	AGameplayPhaseTransitionPoint* Point = F.World->SpawnActor<AGameplayPhaseTransitionPoint>();
	UCartCargoTrackerComponent* Tracker = Cart ? Cart->FindComponentByClass<UCartCargoTrackerComponent>() : nullptr;
	if (!Cart || !CartBase || !Point || !Tracker) return false;
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
	UPrimitiveComponent* CartBody = Cast<UPrimitiveComponent>(Cart->GetRootComponent());
	if (!CartBody) return false;
	const ECollisionEnabled::Type CartCollision = CartBody->GetCollisionEnabled();
	TestTrue(TEXT("Physics fixture actually simulates"), bOriginalSim);
	TestFalse(TEXT("Manual call cannot skip preparation"), Point->TryStartMainGameplay());
	Point->PreparationDurationSeconds = 0.0f;
	TestTrue(TEXT("Preparation timer schedules once"), Point->StartPreparationTimer());
	TestTrue(TEXT("Preparation timer explicitly locks the Cart"), CartBase->IsPreparationLocked());
	TestFalse(TEXT("Locked Cart does not simulate during shopping"), CartBody->IsSimulatingPhysics());
	TestEqual(TEXT("Locked Cart keeps collision so Cargo can be loaded"), CartBody->GetCollisionEnabled(), CartCollision);
	TestTrue(TEXT("Cargo remains independently simulated while the Cart is locked"), FirstBody->IsSimulatingPhysics());
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
	TestFalse(TEXT("Main Area restoration releases the preparation Cart lock"), CartBase->IsPreparationLocked());
	TestTrue(TEXT("Main Area restoration resumes authoritative Cart simulation"), CartBody->IsSimulatingPhysics());
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
	int32 LobbyTravels = 0;
	FString LobbyURL;
	F.Rule->LobbyTravelForTesting = [&LobbyTravels, &LobbyURL](const FString& URL)
	{
		++LobbyTravels;
		LobbyURL = URL;
		return true;
	};
	const FTransform Before = Cart->GetActorTransform();
	Point->StartPreparationTimer();
	F.TickTimers();
	F.TickTimers();
	TestEqual(TEXT("Empty preparation remains non-terminal Waiting during Lobby travel"),
		F.State->GetCurrentGamePhase(), ECh4GamePhase::Waiting);
	TestEqual(TEXT("Empty preparation never initializes Cargo"), F.State->GetInitialCargoCount(), 0);
	TestTrue(TEXT("Empty Cart never teleports"), Cart->GetActorTransform().Equals(Before));
	TestFalse(TEXT("Empty transition remains incomplete"), Point->IsTransitionComplete());
	TestEqual(TEXT("Empty preparation dispatches exactly one Lobby return"), LobbyTravels, 1);
	TestEqual(TEXT("Empty preparation uses the configured Lobby package"),
		LobbyURL, FString(TEXT("/Game/Lobby/L_Lobby")));
	AFinalDeliveryZone* Goal = F.World->SpawnActor<AFinalDeliveryZone>();
	TArray<UBoxComponent*> Boxes;
	Goal->GetComponents(Boxes);
	TestEqual(TEXT("Thin wrapper owns exactly one collision box"), Boxes.Num(), 1);
	TestNotNull(TEXT("TriggerCollision is the implementation component"), Goal->FindComponentByClass<UFinalDeliveryZoneComponent>());
	TestEqual(TEXT("Original collision subobject name is retained"), Boxes[0]->GetFName(), FName(TEXT("TriggerCollision")));
	TestEqual(TEXT("Original root name is retained"), Goal->GetRootComponent()->GetFName(), FName(TEXT("Root")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4PreparationValidationTest,
	"Ch4_multiGame.GameFlow.PreparationValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4PreparationValidationTest::RunTest(const FString&)
{
	using namespace Ch4GameplayLoopTests;
	FWorldFixture F;
	if (!F.Initialize()) return false;
	UClass* CartClass = LoadClass<AActor>(nullptr, TEXT("/Game/CartTest/BP_ShoppingCart.BP_ShoppingCart_C"));
	if (!CartClass) return false;
	AActor* Cart = F.World->SpawnActor<AActor>(CartClass);
	auto* Point = F.World->SpawnActor<AGameplayPhaseTransitionPoint>();
	Point->SetGameModeForTesting(F.Rule);
	Point->PreparationDurationSeconds = 0.0f;
	const FTransform Before = Cart->GetActorTransform();
	TestTrue(TEXT("Missing references do not prevent observing timer completion"), Point->StartPreparationTimer());
	F.TickTimers();
	TestFalse(TEXT("Missing Cart rejects transition safely"), Point->TryStartMainGameplay());
	Point->CartActor = Cart;
	int32 RejectedLobbyTravels = 0;
	F.Rule->LobbyTravelForTesting = [&RejectedLobbyTravels](const FString&)
	{
		++RejectedLobbyTravels;
		return false;
	};
	TestFalse(TEXT("Rejected empty-Cart Lobby travel reports failure and allows a retry"),
		Point->TryStartMainGameplay());
	TestEqual(TEXT("Empty-Cart validation attempted the dedicated Lobby route once"),
		RejectedLobbyTravels, 1);
	TestEqual(TEXT("Validation never partially initializes Cargo"), F.State->GetInitialCargoCount(), 0);
	TestTrue(TEXT("All failed retries leave Cart in place"), Cart->GetActorTransform().Equals(Before));
	TestEqual(TEXT("All failed retries leave Waiting"), F.State->GetCurrentGamePhase(), ECh4GamePhase::Waiting);
	TestTrue(TEXT("Separate start path fixture initializes"), F.Rule->RequestCargoInitialization(1));
	TestTrue(TEXT("Separate start path fixture starts"), F.Rule->RequestGameStart());
	TestFalse(TEXT("Already Playing rejects a second preparation start"), Point->TryStartMainGameplay());
	TestEqual(TEXT("Rejected duplicate leaves Playing intact"), F.State->GetCurrentGamePhase(), ECh4GamePhase::Playing);
	return true;
}

#endif

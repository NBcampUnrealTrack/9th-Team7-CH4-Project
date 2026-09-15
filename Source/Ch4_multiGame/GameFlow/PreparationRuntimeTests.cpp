// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Cargo/CargoActor.h"
#include "Cart/CartBase.h"
#include "Cart/CartCargoTrackerComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameplayPhaseTransitionPoint.h"
#include "GameFramework/Pawn.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerController.h"

// Explicit two-process fixture. It keeps the SAVED Forest references and real
// 60-second timer, but places one existing Cargo in the actual tracker before the
// countdown expires so NullRHI cannot take the intended Cargo-zero failure route.
// This never saves an asset, initializes GameFlow itself, or changes timer duration.
class FCh4PreparationRuntimeCommand final : public IAutomationLatentCommand
{
public:
	explicit FCh4PreparationRuntimeCommand(FAutomationTestBase* InTest)
		: Test(InTest), Started(FPlatformTime::Seconds()) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 200.0)
		{
			Test->AddError(FString::Printf(TEXT("Preparation runtime timeout, stage=%d"), Stage));
			return true;
		}
		UWorld* World = nullptr;
		APlayerController* PC = nullptr;
		if (!GEngine) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (!Candidate || Context.WorldType != EWorldType::Game || !Candidate->HasBegunPlay() || Candidate->IsInSeamlessTravel()) continue;
			APlayerController* Local = Candidate->GetFirstPlayerController();
			if (Local && Local->IsLocalController() && Local->GetPawn()) { World = Candidate; PC = Local; break; }
		}
		if (!World || !PC) return false;
		if (World->GetPackage()->GetName() == TEXT("/Game/Lobby/L_Lobby"))
		{
			auto* Lobby = World->GetGameState<ACh4_multiGameLobbyGameState>();
			auto* LobbyPC = Cast<ACh4_multiGameLobbyPlayerController>(PC);
			if (Lobby && LobbyPC && Lobby->GetCurrentPlayerCount() == 2 && !bReadySent)
			{
				LobbyPC->LobbyReady();
				bReadySent = true;
			}
			return false;
		}
		if (World->GetPackage()->GetName() != TEXT("/Game/Map/Level/ForestLevel")) return false;
		auto* Flow = World->GetGameState<ACh4_multiGameGameState>();
		if (!Flow) return false;
		AGameplayPhaseTransitionPoint* Point = nullptr;
		for (TActorIterator<AGameplayPhaseTransitionPoint> It(World); It; ++It) { Point = *It; break; }
		if (!Point || !Point->CartDestination) return false;
		if (!Cart.IsValid())
		{
			Cart = Cast<ACartBase>(Point->CartActor);
			if (!Cart.IsValid())
			{
				for (TActorIterator<ACartBase> It(World); It; ++It)
				{
					Cart = *It;
					break;
				}
			}
		}
		if (!Cargo.IsValid())
			for (TActorIterator<ACargoActor> It(World); It; ++It)
				if (IsValid(*It) && !It->IsActorBeingDestroyed()) { Cargo = *It; break; }
		if (!Cart.IsValid() || !Cargo.IsValid()) return false;
		if (Stage == 0)
		{
			UPrimitiveComponent* CartBody = Cast<UPrimitiveComponent>(
				Cart->GetDefaultSubobjectByName(TEXT("CartMesh")));
			OriginalPawn = PC->GetPawn();
			Test->TestEqual(TEXT("Actual Forest begins Waiting"), Flow->GetCurrentGamePhase(), ECh4GamePhase::Waiting);
			Test->TestEqual(TEXT("Production duration remains 60 seconds"), Point->PreparationDurationSeconds, 60.0f);
			Test->TestTrue(TEXT("Actual Cart replicates actor and movement"), Cart->GetIsReplicated() && Cart->IsReplicatingMovement());
			Test->TestNotNull(TEXT("Actual Cart has the dedicated physics body"), CartBody);
			if (CartBody)
			{
				Test->TestFalse(TEXT("Preparation Cart physics is disabled on host and remote client"),
					CartBody->IsSimulatingPhysics());
				Test->TestFalse(TEXT("CartMesh has no independent component replication"), CartBody->GetIsReplicated());
			}
			Test->TestTrue(TEXT("Actual Cargo replicates actor and movement"), Cargo->GetIsReplicated() && Cargo->IsReplicatingMovement());
			if (PC->HasAuthority())
			{
				auto* Tracker = Cart->FindComponentByClass<UCartCargoTrackerComponent>();
				if (!Test->TestNotNull(TEXT("Actual Cart tracker exists"), Tracker)) return true;
				UPrimitiveComponent* Body = Cargo->GetGrabbableComponent();
				if (!Test->TestNotNull(TEXT("Runtime fixture Cargo has a physics body"), Body)) return true;
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
				Body->SetEnableGravity(false);
				Cargo->SetActorLocation(Tracker->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
				Cargo->UpdateOverlaps();
				Tracker->UpdateOverlaps();
				ExpectedCargo = Tracker->GetTrackedCargoSnapshot().Num();
				Test->TestTrue(TEXT("Runtime fixture registers Cargo before the real timer expires"),
					Tracker->GetTrackedCargoSnapshot().Contains(Cargo.Get()) && ExpectedCargo > 0);
			}
			Test->AddInfo(FString::Printf(TEXT("PREPARATION_MAP Role=%s Point=%s SavedCart=%s Destination=%s PlayerPoints=%d Remaining=%.2f CartPhysics=%d"),
				PC->HasAuthority() ? TEXT("Host") : TEXT("Client"), *Point->GetName(), *GetNameSafe(Point->CartActor),
				*Point->CartDestination->GetActorLocation().ToString(), Point->PlayerDestinationPoints.Num(), Flow->GetRemainingPreparationTime(),
				CartBody && CartBody->IsSimulatingPhysics() ? 1 : 0));
			ForestEntered = Now;
			Stage = 1;
		}
		if (Stage == 1 && PC->HasAuthority() && Now - ForestEntered > 64.0)
		{
			Test->AddError(FString::Printf(TEXT("Real 60-second preparation did not enter Playing; phase=%d initialCargo=%d expectedCargo=%d"),
				static_cast<int32>(Flow->GetCurrentGamePhase()), Flow->GetInitialCargoCount(), ExpectedCargo));
			return true;
		}
		if (Flow->GetCurrentGamePhase() != ECh4GamePhase::Playing) return false;
		if (PlayingObserved == 0.0) PlayingObserved = Now;
		if (Now - PlayingObserved < 2.0) return false;
		Test->TestTrue(TEXT("Player keeps the same Pawn and possession"), OriginalPawn.IsValid() && PC->GetPawn() == OriginalPawn.Get());
		UPrimitiveComponent* CartBody = Cast<UPrimitiveComponent>(
			Cart->GetDefaultSubobjectByName(TEXT("CartMesh")));
		const double CartDistance = FVector::Distance(Cart->GetActorLocation(), Point->CartDestination->GetActorLocation());
		const double CargoDistance = FVector::Distance(Cargo->GetActorLocation(), Cart->GetActorLocation());
		const double CartVisualError = CartBody
			? FVector::Distance(CartBody->GetComponentLocation(), Cart->GetActorLocation())
			: TNumericLimits<double>::Max();
		const double CartRotationError = CartBody
			? FMath::RadiansToDegrees(CartBody->GetComponentQuat().AngularDistance(Cart->GetActorQuat()))
			: TNumericLimits<double>::Max();
		double PlayerDistance = TNumericLimits<double>::Max();
		for (const ATargetPoint* Destination : Point->PlayerDestinationPoints)
			if (IsValid(Destination)) PlayerDistance = FMath::Min(PlayerDistance, FVector::Distance(PC->GetPawn()->GetActorLocation(), Destination->GetActorLocation()));
		Test->AddInfo(FString::Printf(TEXT("PREPARATION_RUNTIME_RESULT Role=%s InitialCargo=%d CartToDestination=%.2f CargoToCart=%.2f PlayerToDestination=%.2f CartPhysics=%d VisualPositionError=%.2f VisualRotationError=%.2f Cart=%s Cargo=%s Pawn=%s"),
			PC->HasAuthority() ? TEXT("Host") : TEXT("Client"), Flow->GetInitialCargoCount(), CartDistance, CargoDistance, PlayerDistance,
			CartBody && CartBody->IsSimulatingPhysics() ? 1 : 0, CartVisualError, CartRotationError,
			*Cart->GetActorLocation().ToString(), *Cargo->GetActorLocation().ToString(), *PC->GetPawn()->GetActorLocation().ToString()));
		// Two seconds of resumed physics/character movement must not restore the Shop transform.
		Test->TestTrue(TEXT("Cart reached the main area"), CartDistance < 1000.0);
		Test->TestTrue(TEXT("Cargo reached the main area with Cart"), CargoDistance < 1000.0);
		Test->TestTrue(TEXT("Player reached a configured main destination"), PlayerDistance < 1000.0);
		Test->TestTrue(TEXT("Cart visual converges to the replicated Actor target"), CartVisualError < 100.0);
		Test->TestTrue(TEXT("Cart visual rotation converges to the replicated Actor target"), CartRotationError < 10.0);
		if (PC->HasAuthority())
		{
			Test->TestTrue(TEXT("Server Cart physics resumes in Playing"), CartBody && CartBody->IsSimulatingPhysics());
		}
		else
		{
			Test->TestFalse(TEXT("Remote client Cart physics remains disabled in Playing"),
				CartBody && CartBody->IsSimulatingPhysics());
		}
		Test->TestTrue(TEXT("Cargo physics restored"), Cargo->GetGrabbableComponent()->IsSimulatingPhysics());
		Test->TestTrue(TEXT("Cargo collision restored"), Cargo->GetGrabbableComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
		if (PC->HasAuthority()) Test->TestTrue(TEXT("Server transition complete"), Point->IsTransitionComplete());
		return true;
	}
private:
	FAutomationTestBase* Test;
	double Started;
	double ForestEntered = 0.0;
	double PlayingObserved = 0.0;
	int32 Stage = 0;
	int32 ExpectedCargo = 0;
	bool bReadySent = false;
	TWeakObjectPtr<ACartBase> Cart;
	TWeakObjectPtr<ACargoActor> Cargo;
	TWeakObjectPtr<APawn> OriginalPawn;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4PreparationRuntimeTest,
	"Ch4_multiGame.GameFlow.RuntimePreparation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4PreparationRuntimeTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("Ch4PreparationRepairFixture")))
	{
		AddInfo(TEXT("Requires explicit -Ch4PreparationRepairFixture with two players; no gameplay modified."));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCh4PreparationRuntimeCommand(this));
	return true;
}

#endif

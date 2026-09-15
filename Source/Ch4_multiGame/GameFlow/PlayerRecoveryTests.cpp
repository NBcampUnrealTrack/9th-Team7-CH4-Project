// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cart/CartBase.h"
#include "Ch4_multiGameGameMode.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/Ch4_PlayerCharacter.h"
#include "TimerManager.h"

#include <limits>

class FCh4PlayerRecoveryNetworkRuntimeCommand final : public IAutomationLatentCommand
{
public:
	explicit FCh4PlayerRecoveryNetworkRuntimeCommand(FAutomationTestBase* InTest)
		: Test(InTest), StartedAt(FPlatformTime::Seconds())
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - StartedAt > 45.0)
		{
			Test->AddError(FString::Printf(TEXT("Player recovery network timeout at stage %d"), Stage));
			return true;
		}

		UWorld* World = FindForestWorld();
		APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
		ACh4_PlayerCharacter* LocalCharacter = LocalController
			? Cast<ACh4_PlayerCharacter>(LocalController->GetPawn()) : nullptr;
		ACartBase* Cart = FindCart(World);
		ACh4_multiGameGameState* State = World ? World->GetGameState<ACh4_multiGameGameState>() : nullptr;
		if (!World || !LocalController || !LocalCharacter || !Cart || !State)
		{
			return false;
		}

		return LocalController->HasAuthority()
			? UpdateHost(World, LocalController, LocalCharacter, Cart, State, Now)
			: UpdateClient(LocalController, LocalCharacter, Cart, State);
	}

private:
	static UWorld* FindForestWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && Context.WorldType == EWorldType::Game && World->HasBegunPlay()
				&& !World->IsInSeamlessTravel()
				&& World->GetPackage()->GetName() == TEXT("/Game/Map/Level/ForestLevel"))
			{
				return World;
			}
		}
		return nullptr;
	}

	static ACartBase* FindCart(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ACartBase> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed())
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool UpdateHost(
		UWorld* World,
		APlayerController* LocalController,
		ACh4_PlayerCharacter* LocalCharacter,
		ACartBase* Cart,
		ACh4_multiGameGameState* State,
		const double Now)
	{
		if (Stage == 2)
		{
			if (Now - RecoveryObservedAt < 8.0)
			{
				return false;
			}
			Test->AddInfo(FString::Printf(
				TEXT("PLAYER_RECOVERY_NETWORK_HOST_PASS HostDistance=%.1f ClientDistance=%.1f"),
				HostRecoveryDistance, RemoteRecoveryDistance));
			return true;
		}

		ACh4_PlayerCharacter* RemoteCharacter = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			ACh4_PlayerCharacter* Candidate = Controller
				? Cast<ACh4_PlayerCharacter>(Controller->GetPawn()) : nullptr;
			if (Candidate && Candidate != LocalCharacter)
			{
				RemoteCharacter = Candidate;
				break;
			}
		}
		if (!RemoteCharacter)
		{
			return false;
		}
		if (Stage == 0 && RemoteConnectedAt <= 0.0)
		{
			RemoteConnectedAt = Now;
			return false;
		}
		// Each process owns its Automation controller. Give the client controller time to start
		// its latent command before the server publishes the deliberately out-of-bounds state.
		if (Stage == 0 && Now - RemoteConnectedAt < 6.0)
		{
			return false;
		}

		if (Stage == 0)
		{
			ACh4_multiGameGameMode* Rule = World->GetAuthGameMode<ACh4_multiGameGameMode>();
			if (!Rule || !Rule->RegisterGameplayCart(Cart))
			{
				Test->AddError(TEXT("Actual Forest GameMode could not register its Cart"));
				return true;
			}
			// Leave enough time for the remote process to observe the replicated out-of-bounds state.
			Rule->PlayerRecoveryCheckIntervalSeconds = 2.0f;
			// The checked-in Blueprint still serializes the former 4000 cm override. Exercise the new
			// 12000 cm policy in memory without modifying the user's binary asset.
			Rule->MaximumPlayerCartDistance = 12000.0f;
			if (State->GetCurrentGamePhase() == ECh4GamePhase::Waiting
				&& (!Rule->RequestCargoInitialization(1) || !Rule->RequestGameStart()))
			{
				Test->AddError(TEXT("Actual Forest GameMode could not enter Playing for recovery"));
				return true;
			}
			if (State->GetCurrentGamePhase() != ECh4GamePhase::Playing)
			{
				Test->AddError(TEXT("Player recovery network fixture is not Playing"));
				return true;
			}

			HostPawn = LocalCharacter;
			RemotePawn = RemoteCharacter;
			const FVector CartLocation = Cart->GetActorLocation();
			LocalCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			LocalCharacter->GetCharacterMovement()->StopMovementImmediately();
			LocalCharacter->SetActorLocation(
				CartLocation + FVector(12500.0, 0.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
			RemoteCharacter->SetActorLocation(
				CartLocation - FVector(0.0, 0.0, 2000.0), false, nullptr, ETeleportType::TeleportPhysics);
			LocalCharacter->ForceNetUpdate();
			RemoteCharacter->ForceNetUpdate();
			OutOfBoundsAt = Now;
			Stage = 1;
			return false;
		}

		if (!HostPawn.IsValid() || !RemotePawn.IsValid())
		{
			Test->AddError(TEXT("Recovery replaced or destroyed an existing server Pawn"));
			return true;
		}
		if (Now - OutOfBoundsAt < 0.5)
		{
			return false;
		}

		const double HostDistance = FVector::Distance(HostPawn->GetActorLocation(), Cart->GetActorLocation());
		const double RemoteDistance = FVector::Distance(RemotePawn->GetActorLocation(), Cart->GetActorLocation());
		if (HostDistance >= 1000.0 || RemoteDistance >= 1000.0)
		{
			return false;
		}

		Test->TestTrue(TEXT("Host retains the same possessed Pawn"), LocalController->GetPawn() == HostPawn.Get());
		Test->TestEqual(TEXT("Server recovery leaves the match Playing"),
			State->GetCurrentGamePhase(), ECh4GamePhase::Playing);
		HostRecoveryDistance = HostDistance;
		RemoteRecoveryDistance = RemoteDistance;
		RecoveryObservedAt = Now;
		Stage = 2;
		return false;
	}

	bool UpdateClient(
		APlayerController* LocalController,
		ACh4_PlayerCharacter* LocalCharacter,
		ACartBase* Cart,
		ACh4_multiGameGameState* State)
	{
		if (Stage == 0 && State->GetCurrentGamePhase() == ECh4GamePhase::Playing)
		{
			LocalPawn = LocalCharacter;
			Stage = 1;
		}
		if (Stage != 1)
		{
			return false;
		}

		const ECh4PlayerRecoveryReason Reason = ACh4_multiGameGameMode::EvaluatePlayerRecovery(
			State->GetCurrentGamePhase(),
			LocalCharacter->GetActorLocation(),
			Cart->GetActorLocation(),
			12000.0f,
			1500.0f);
		if (Reason != ECh4PlayerRecoveryReason::None)
		{
			bObservedReplicatedOutOfBoundsState = true;
		}
		const double CartDistance = FVector::Distance(LocalCharacter->GetActorLocation(), Cart->GetActorLocation());
		if (!bObservedReplicatedOutOfBoundsState || CartDistance >= 1000.0)
		{
			return false;
		}

		Test->TestTrue(TEXT("Client retains the same possessed Pawn after server recovery"),
			LocalPawn.IsValid() && LocalController->GetPawn() == LocalPawn.Get());
		Test->AddInfo(FString::Printf(
			TEXT("PLAYER_RECOVERY_NETWORK_CLIENT_PASS Distance=%.1f"), CartDistance));
		return true;
	}

	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
	double OutOfBoundsAt = 0.0;
	double RecoveryObservedAt = 0.0;
	double RemoteConnectedAt = 0.0;
	double HostRecoveryDistance = 0.0;
	double RemoteRecoveryDistance = 0.0;
	int32 Stage = 0;
	bool bObservedReplicatedOutOfBoundsState = false;
	TWeakObjectPtr<ACh4_PlayerCharacter> HostPawn;
	TWeakObjectPtr<ACh4_PlayerCharacter> RemotePawn;
	TWeakObjectPtr<ACh4_PlayerCharacter> LocalPawn;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4PlayerRecoveryPolicyTest,
	"Ch4_multiGame.GameFlow.PlayerRecoveryPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4PlayerRecoveryPolicyTest::RunTest(const FString& Parameters)
{
	using EReason = ECh4PlayerRecoveryReason;
	const FVector CartLocation(100.0, 200.0, 800.0);
	const auto Evaluate = [&CartLocation](const ECh4GamePhase Phase, const FVector& PlayerLocation)
	{
		return ACh4_multiGameGameMode::EvaluatePlayerRecovery(
			Phase, PlayerLocation, CartLocation, 12000.0f, 1500.0f);
	};

	TestEqual(TEXT("Waiting never recovers a distant Player"),
		static_cast<uint8>(Evaluate(ECh4GamePhase::Waiting, CartLocation + FVector(12500.0, 0.0, 0.0))),
		static_cast<uint8>(EReason::None));
	TestEqual(TEXT("A nearby Player remains in place while Playing"),
		static_cast<uint8>(Evaluate(ECh4GamePhase::Playing, CartLocation + FVector(1000.0, 0.0, 0.0))),
		static_cast<uint8>(EReason::None));
	TestEqual(TEXT("A Player beyond the former 4000 cm threshold remains inside the new range"),
		static_cast<uint8>(Evaluate(ECh4GamePhase::Playing, CartLocation + FVector(4500.0, 0.0, 0.0))),
		static_cast<uint8>(EReason::None));
	TestEqual(TEXT("A Player exactly at 12000 cm remains in place"),
		static_cast<uint8>(Evaluate(ECh4GamePhase::Playing, CartLocation + FVector(12000.0, 0.0, 0.0))),
		static_cast<uint8>(EReason::None));
	TestEqual(TEXT("A Player just beyond 12000 cm is recovered"),
		static_cast<uint8>(Evaluate(ECh4GamePhase::Playing, CartLocation + FVector(12001.0, 0.0, 0.0))),
		static_cast<uint8>(EReason::DistanceExceeded));
	TestEqual(TEXT("A horizontally nearby Player 3000 cm below the Cart is recovered"),
		static_cast<uint8>(Evaluate(ECh4GamePhase::Playing, CartLocation + FVector(100.0, 0.0, -3000.0))),
		static_cast<uint8>(EReason::BelowCart));
	TestEqual(TEXT("Cleared never recovers a distant Player"),
		static_cast<uint8>(Evaluate(ECh4GamePhase::Cleared, CartLocation + FVector(12500.0, 0.0, 0.0))),
		static_cast<uint8>(EReason::None));
	FVector InvalidLocation = CartLocation;
	InvalidLocation.X = std::numeric_limits<double>::quiet_NaN();
	TestEqual(TEXT("A non-finite Player location is identified explicitly"),
		static_cast<uint8>(Evaluate(ECh4GamePhase::Playing, InvalidLocation)),
		static_cast<uint8>(EReason::InvalidLocation));

	TArray<FVector> TiltedCandidates;
	TArray<FVector> FlatCandidates;
	ACh4_multiGameGameMode::BuildPlayerRecoveryCandidates(
		CartLocation, FRotator(40.0, 90.0, 30.0), 300.0f, 100.0f, 150.0f, TiltedCandidates);
	ACh4_multiGameGameMode::BuildPlayerRecoveryCandidates(
		CartLocation, FRotator(0.0, 90.0, 0.0), 300.0f, 100.0f, 150.0f, FlatCandidates);
	TestEqual(TEXT("Recovery provides six ordered rear-only fallback candidates"), TiltedCandidates.Num(), 6);
	TestTrue(TEXT("Cart pitch and roll do not move candidates underground or sideways"),
		TiltedCandidates == FlatCandidates);
	TestTrue(TEXT("The first candidate is 300 cm behind yaw and 100 cm above the Cart"),
		TiltedCandidates.IsValidIndex(0)
			&& TiltedCandidates[0].Equals(FVector(100.0, -100.0, 900.0), 0.01));
	TestTrue(TEXT("The final fallback is farther behind and to the right of the Cart"),
		TiltedCandidates.IsValidIndex(5)
			&& FVector::DistSquared2D(TiltedCandidates[5], CartLocation)
				> FVector::DistSquared2D(TiltedCandidates[0], CartLocation));
	const FVector YawForward = FRotationMatrix(FRotator(0.0, 90.0, 0.0)).GetUnitAxis(EAxis::X);
	for (int32 CandidateIndex = 0; CandidateIndex < TiltedCandidates.Num(); ++CandidateIndex)
	{
		const FVector HorizontalOffset = FVector(
			TiltedCandidates[CandidateIndex].X - CartLocation.X,
			TiltedCandidates[CandidateIndex].Y - CartLocation.Y,
			0.0);
		TestTrue(FString::Printf(TEXT("Candidate %d stays in the Cart's rear hemisphere"), CandidateIndex),
			FVector::DotProduct(HorizontalOffset, YawForward) <= KINDA_SMALL_NUMBER);
	}

	const ACh4_multiGameGameMode* Defaults = GetDefault<ACh4_multiGameGameMode>();
	TestTrue(TEXT("Player Cart recovery is enabled by default"), Defaults->bEnablePlayerCartRecovery);
	TestEqual(TEXT("Recovery checks default to 0.5 seconds"),
		Defaults->PlayerRecoveryCheckIntervalSeconds, 0.5f);
	TestEqual(TEXT("The default maximum Cart distance is exactly three times the former 4000 cm"),
		Defaults->MaximumPlayerCartDistance, 12000.0f);
	TestEqual(TEXT("The default below-Cart threshold is 1500 cm"),
		Defaults->MaximumVerticalDistanceBelowCart, 1500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4PlayerRecoveryWorldTest,
	"Ch4_multiGame.GameFlow.PlayerRecoveryWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4PlayerRecoveryWorldTest::RunTest(const FString& Parameters)
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
	UWorld* World = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		NAME_None,
		nullptr,
		true,
		ERHIFeatureLevel::Num,
		&InitializationValues);
	if (!TestNotNull(TEXT("Recovery fixture World"), World))
	{
		return false;
	}
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	ACh4_multiGameGameMode* Rule = World->SpawnActor<ACh4_multiGameGameMode>();
	ACh4_multiGameGameState* State = World->GetGameState<ACh4_multiGameGameState>();
	if (!State)
	{
		State = World->SpawnActor<ACh4_multiGameGameState>();
		World->SetGameState(State);
	}
	ACartBase* Cart = World->SpawnActor<ACartBase>();
	AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>();
	AStaticMeshActor* HeldCargo = World->SpawnActor<AStaticMeshActor>();
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	bool bSuccess = TestNotNull(TEXT("Recovery fixture GameState"), State)
		&& TestNotNull(TEXT("Recovery fixture GameMode"), Rule)
		&& TestNotNull(TEXT("Recovery fixture Cart"), Cart)
		&& TestNotNull(TEXT("Recovery fixture Ground"), Ground)
		&& TestNotNull(TEXT("Recovery fixture held Cargo body"), HeldCargo)
		&& TestNotNull(TEXT("Recovery fixture Cube mesh"), CubeMesh);
	if (!bSuccess)
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->SetGameState(nullptr);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	Ground->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Ground->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
	Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Ground->SetActorScale3D(FVector(60.0, 60.0, 1.0));
	Ground->SetActorLocation(FVector(0.0, 0.0, -50.0));
	Ground->GetStaticMeshComponent()->RecreatePhysicsState();
	HeldCargo->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	HeldCargo->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
	HeldCargo->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("PhysicsActor"));
	HeldCargo->GetStaticMeshComponent()->SetSimulatePhysics(false);
	Cart->SetActorLocation(FVector::ZeroVector);

	TArray<APlayerController*> Controllers;
	TArray<ACh4_PlayerCharacter*> Players;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		ACh4_PlayerCharacter* Player = World->SpawnActor<ACh4_PlayerCharacter>();
		if (!Controller || !Player)
		{
			bSuccess = false;
			break;
		}
		Controller->Possess(Player);
		Controllers.Add(Controller);
		Players.Add(Player);
	}
	if (bSuccess)
	{
		HeldCargo->SetActorLocation(Players[0]->GetActorLocation());
		HeldCargo->AttachToComponent(Players[0]->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		Players[0]->GrabbedComponent = HeldCargo->GetStaticMeshComponent();
		Players[0]->SetActorLocation(FVector(12500.0, 0.0, 100.0));
		Players[0]->GetCharacterMovement()->Velocity = FVector(200.0, 0.0, -1000.0);
		USceneComponent* GrabAnchor = Cast<USceneComponent>(Cart->GetDefaultSubobjectByName(TEXT("Anchor_1")));
		bSuccess &= TestNotNull(TEXT("Cart recovery fixture has its normal first grab anchor"), GrabAnchor);
		if (GrabAnchor)
		{
			Players[1]->SetActorLocation(GrabAnchor->GetComponentLocation());
			bSuccess &= TestTrue(TEXT("Second Player uses the normal Cart grab path before recovery"),
				Cart->TryGrabPlayer(Players[1]));
		}
		Players[1]->SetActorLocation(FVector(12500.0, 200.0, 100.0));
		Players[1]->GetCharacterMovement()->Velocity = FVector(-200.0, 0.0, -1000.0);

		bSuccess &= TestTrue(TEXT("Transition actor can register its authoritative Cart once"),
			Rule->RegisterGameplayCart(Cart));
		bSuccess &= Rule->RequestCargoInitialization(1);
		bSuccess &= Rule->RequestGameStart();
		bSuccess &= TestEqual(TEXT("Recovery fixture enters Playing before its timer fires"),
			static_cast<uint8>(State->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Playing));
		++GFrameCounter;
		World->GetTimerManager().Tick(0.0f);
		++GFrameCounter;
		World->GetTimerManager().Tick(0.6f);

		bSuccess &= TestTrue(TEXT("First Player is recovered near the current Cart"),
			FVector::Distance(Players[0]->GetActorLocation(), Cart->GetActorLocation()) < 1000.0);
		bSuccess &= TestTrue(TEXT("Second Player is recovered near the current Cart"),
			FVector::Distance(Players[1]->GetActorLocation(), Cart->GetActorLocation()) < 1000.0);
		bSuccess &= TestFalse(TEXT("Capsule safety sends the second Player to a different fallback"),
			Players[0]->GetActorLocation().Equals(Players[1]->GetActorLocation(), 1.0));
		bSuccess &= TestTrue(TEXT("Existing Pawn possession survives recovery"),
			Controllers[0]->GetPawn() == Players[0] && Controllers[1]->GetPawn() == Players[1]);
		bSuccess &= TestNull(TEXT("Cart grab state is released before recovery"), Players[1]->GrabbedCart.Get());
		bSuccess &= TestNull(TEXT("Cargo grab state is released before recovery"), Players[0]->GrabbedComponent.Get());
		bSuccess &= TestNull(TEXT("Released Cargo is detached from the existing Pawn"),
			HeldCargo->GetStaticMeshComponent()->GetAttachParent());
		bSuccess &= TestTrue(TEXT("Released Cargo resumes its normal physics state"),
			HeldCargo->GetStaticMeshComponent()->IsSimulatingPhysics());
		bSuccess &= TestTrue(TEXT("Recovered Players have zero movement velocity"),
			Players[0]->GetCharacterMovement()->Velocity.IsNearlyZero()
				&& Players[1]->GetCharacterMovement()->Velocity.IsNearlyZero());
		bSuccess &= TestEqual(TEXT("Recovery does not change GameFlow"),
			static_cast<uint8>(State->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Playing));
	}

	World->EndPlay(EEndPlayReason::Quit);
	World->SetGameState(nullptr);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4PlayerRecoveryNetworkRuntimeTest,
	"Ch4_multiGame.GameFlow.RuntimePlayerRecoveryNetwork",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4PlayerRecoveryNetworkRuntimeTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("Ch4PlayerRecoveryNetworkFixture")))
	{
		AddInfo(TEXT("Requires explicit -Ch4PlayerRecoveryNetworkFixture with a listen server and one client."));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCh4PlayerRecoveryNetworkRuntimeCommand(this));
	return true;
}

#endif

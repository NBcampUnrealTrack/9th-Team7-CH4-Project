// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "Ch4_multiGamePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerController.h"
#include "Player/Ch4_PlayerCharacter.h"

namespace Ch4InputRuntime
{
	constexpr TCHAR LobbyMap[] = TEXT("/Game/Lobby/L_Lobby");
	constexpr TCHAR ForestMap[] = TEXT("/Game/Map/Level/ForestLevel");

	FString MovementModeName(const UCharacterMovementComponent* Movement)
	{
		return Movement ? UEnum::GetValueAsString(Movement->MovementMode) : TEXT("Invalid");
	}

	FVector2D ReadActionValue(const APlayerController* Controller, const UInputAction* Action)
	{
		const UEnhancedPlayerInput* Input = Controller ? Cast<UEnhancedPlayerInput>(Controller->PlayerInput) : nullptr;
		return Input && Action ? Input->GetActionValue(Action).Get<FVector2D>() : FVector2D::ZeroVector;
	}
}

class FCh4PlayerInputRuntimeCommand final : public IAutomationLatentCommand
{
public:
	explicit FCh4PlayerInputRuntimeCommand(FAutomationTestBase* InTest)
		: Test(InTest), StartedAt(FPlatformTime::Seconds())
	{
		FParse::Value(FCommandLine::Get(), TEXT("Ch4InputExpectedPlayers="), ExpectedPlayers);
		ExpectedPlayers = FMath::Max(ExpectedPlayers, 1);
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - StartedAt > 100.0)
		{
			ReleaseW();
			Test->AddError(FString::Printf(TEXT("Player input lifecycle timed out: map=%s stage=%d"), *ActiveMap, Stage));
			return true;
		}

		UWorld* World = nullptr;
		ACh4_multiGamePlayerController* Controller = nullptr;
		ACh4_PlayerCharacter* Pawn = nullptr;
		if (!FindRuntime(World, Controller, Pawn)) return false;

		const FString Map = World->GetPackage()->GetName();
		if (Map != Ch4InputRuntime::LobbyMap && Map != Ch4InputRuntime::ForestMap) return false;
		if (Map == Ch4InputRuntime::LobbyMap)
		{
			const ACh4_multiGameLobbyGameState* LobbyState = World->GetGameState<ACh4_multiGameLobbyGameState>();
			if (!LobbyState || LobbyState->GetCurrentPlayerCount() != ExpectedPlayers) return false;
		}

		if (ActiveMap != Map)
		{
			ReleaseW();
			ActiveMap = Map;
			Stage = 0;
			StageAt = Now;
			SettledAt = Now;
			bMidInputLogged = false;
		}
		if (Now - SettledAt < 1.5) return false;

		if (ActiveMap == Ch4InputRuntime::LobbyMap)
		{
			return UpdateLobby(World, Controller, Pawn, Now);
		}
		return UpdateForest(World, Controller, Pawn, Now);
	}

private:
	bool FindRuntime(UWorld*& OutWorld, ACh4_multiGamePlayerController*& OutController, ACh4_PlayerCharacter*& OutPawn) const
	{
		if (!GEngine) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (!Candidate || Context.WorldType != EWorldType::Game || !Candidate->HasBegunPlay() || Candidate->IsInSeamlessTravel()) continue;
			auto* CandidateController = Cast<ACh4_multiGamePlayerController>(Candidate->GetFirstPlayerController());
			auto* CandidatePawn = CandidateController ? Cast<ACh4_PlayerCharacter>(CandidateController->GetPawn()) : nullptr;
			if (CandidateController && CandidateController->IsLocalController() && CandidatePawn)
			{
				OutWorld = Candidate;
				OutController = CandidateController;
				OutPawn = CandidatePawn;
				return true;
			}
		}
		return false;
	}

	void Inspect(const TCHAR* Phase, UWorld* World, ACh4_multiGamePlayerController* Controller, ACh4_PlayerCharacter* Pawn)
	{
		const UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
		Test->AddInfo(FString::Printf(
			TEXT("INPUT_STATE Phase=%s Map=%s Role=%s Controller=%s Pawn=%s PlayerState=%s Local=%d Authority=%d PawnController=%s PawnOwner=%s Possessed=%d InputMode=%s ViewportIgnore=%d MoveIgnored=%d LookIgnored=%d PauseOpen=%d MovementMode=%s MaxWalkSpeed=%.2f Velocity=%s MovementActive=%d UpdatedComponent=%s"),
			Phase, *World->GetPackage()->GetName(), Controller->HasAuthority() ? TEXT("Host") : TEXT("Client"),
			*Controller->GetClass()->GetPathName(), *Pawn->GetClass()->GetPathName(), *GetPathNameSafe(Controller->PlayerState.Get()),
			int32(Controller->IsLocalController()), int32(Controller->HasAuthority()), *GetNameSafe(Pawn->GetController()), *GetNameSafe(Pawn->GetOwner()),
			int32(Pawn->GetController() == Controller), *Controller->GetCurrentInputModeDebugString(),
			int32(GEngine && GEngine->GameViewport && GEngine->GameViewport->IgnoreInput()), int32(Controller->IsMoveInputIgnored()),
			int32(Controller->IsLookInputIgnored()), int32(Controller->IsPauseMenuOpen()), *Ch4InputRuntime::MovementModeName(Movement),
			Movement ? Movement->MaxWalkSpeed : 0.0f, Movement ? *Movement->Velocity.ToString() : TEXT("Invalid"),
			int32(Movement && Movement->IsActive()), Movement ? *GetNameSafe(Movement->UpdatedComponent) : TEXT("Invalid")));

		Test->TestTrue(FString::Printf(TEXT("%s Pawn is possessed"), Phase), Pawn->GetController() == Controller);
		Test->TestFalse(FString::Printf(TEXT("%s viewport accepts input"), Phase), GEngine && GEngine->GameViewport && GEngine->GameViewport->IgnoreInput());
		Test->TestFalse(FString::Printf(TEXT("%s fresh movement is not ignored"), Phase), Controller->IsMoveInputIgnored());
		Test->TestNotNull(FString::Printf(TEXT("%s CharacterMovement exists"), Phase), Movement);
		if (Movement)
		{
			Test->TestTrue(FString::Printf(TEXT("%s CharacterMovement is active"), Phase), Movement->IsActive());
			Test->TestTrue(FString::Printf(TEXT("%s MovementMode permits motion"), Phase), Movement->MovementMode != MOVE_None);
			Test->TestTrue(FString::Printf(TEXT("%s MaxWalkSpeed is positive"), Phase), Movement->MaxWalkSpeed > 0.0f);
			Test->TestNotNull(FString::Printf(TEXT("%s UpdatedComponent exists"), Phase), Movement->UpdatedComponent.Get());
		}

		const auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
		const auto* InputComponent = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
		int32 MoveBindingCount = 0;
		if (InputComponent)
		{
			for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : InputComponent->GetActionEventBindings())
			{
				if (Binding && Binding->GetAction() == Pawn->MoveAction) ++MoveBindingCount;
			}
		}
		Test->AddInfo(FString::Printf(TEXT("INPUT_BINDING Phase=%s IA_PlayerMove=%s Count=%d"), Phase, *GetNameSafe(Pawn->MoveAction), MoveBindingCount));
		Test->TestTrue(FString::Printf(TEXT("%s IA_PlayerMove has a handler binding"), Phase), MoveBindingCount > 0);
		for (const TCHAR* Path : {
			TEXT("/Game/Player/Input/IMC_Player.IMC_Player"),
			TEXT("/Game/Input/Lobby/IMC_Lobby.IMC_Lobby"),
			TEXT("/Game/Input/IMC_Default.IMC_Default")})
		{
			const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, Path);
			int32 Priority = INDEX_NONE;
			const bool bApplied = Subsystem && Context && Subsystem->HasMappingContext(Context, Priority);
			Test->AddInfo(FString::Printf(TEXT("INPUT_MAPPING Phase=%s Context=%s Applied=%d Priority=%d"), Phase, Path, int32(bApplied), Priority));
			if (Context == Pawn->InputMappingContext) Test->TestTrue(FString::Printf(TEXT("%s IMC_Player is applied"), Phase), bApplied);
		}
	}

	void LogHeldInput(const TCHAR* Phase, ACh4_multiGamePlayerController* Controller, ACh4_PlayerCharacter* Pawn)
	{
		const UInputAction* TemplateMove = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
		const FVector2D PlayerValue = Ch4InputRuntime::ReadActionValue(Controller, Pawn->MoveAction);
		const FVector2D TemplateValue = Ch4InputRuntime::ReadActionValue(Controller, TemplateMove);
		const UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
		Test->AddInfo(FString::Printf(
			TEXT("INPUT_HELD Phase=%s IA_PlayerMove=%s IA_TemplateMove=%s Pending=%s Last=%s Acceleration=%s Velocity=%s Ignored=%d"),
			Phase, *PlayerValue.ToString(), *TemplateValue.ToString(), *Pawn->GetPendingMovementInputVector().ToString(),
			*Pawn->GetLastMovementInputVector().ToString(), Movement ? *Movement->GetCurrentAcceleration().ToString() : TEXT("Invalid"),
			Movement ? *Movement->Velocity.ToString() : TEXT("Invalid"), int32(Controller->IsMoveInputIgnored())));
	}

	bool UpdateLobby(UWorld* World, ACh4_multiGamePlayerController* Controller, ACh4_PlayerCharacter* Pawn, double Now)
	{
		switch (Stage)
		{
		case 0:
			Inspect(TEXT("Lobby"), World, Controller, Pawn);
			StartLocation = Pawn->GetActorLocation();
			SendW(IE_Pressed);
			Advance(Now);
			return false;
		case 1:
			if (!bMidInputLogged && Now - StageAt > 0.15)
			{
				LogHeldInput(TEXT("Lobby"), Controller, Pawn);
				bMidInputLogged = true;
			}
			if (Now - StageAt < 0.8) return false;
			SendW(IE_Released);
			LogMovementResult(TEXT("Lobby"), Pawn, true);
			if (auto* LobbyController = Cast<ACh4_multiGameLobbyPlayerController>(Controller))
			{
				LobbyController->LobbyReady();
				Test->AddInfo(TEXT("INPUT_LOBBY_READY Sent=1"));
			}
			else Test->AddError(TEXT("Lobby uses an unexpected PlayerController class"));
			Stage = 99;
			StageAt = Now;
			return false;
		default:
			return false;
		}
	}

	bool UpdateForest(UWorld* World, ACh4_multiGamePlayerController* Controller, ACh4_PlayerCharacter* Pawn, double Now)
	{
		switch (Stage)
		{
		case 0:
			Inspect(TEXT("Forest"), World, Controller, Pawn);
			StartLocation = Pawn->GetActorLocation();
			SendW(IE_Pressed);
			Advance(Now);
			return false;
		case 1:
			if (!bMidInputLogged && Now - StageAt > 0.15)
			{
				LogHeldInput(TEXT("Forest"), Controller, Pawn);
				bMidInputLogged = true;
			}
			if (Now - StageAt < 0.8) return false;
			SendW(IE_Released);
			LogMovementResult(TEXT("Forest"), Pawn, true);
			StartRotation = Controller->GetControlRotation();
			Advance(Now);
			return false;
		case 2:
			if (Now - StageAt < 0.35)
			{
				SendAxis(EKeys::MouseX, 2.0f);
				return false;
			}
			Test->TestTrue(TEXT("Forest mouse reaches existing Character look"),
				!Controller->GetControlRotation().Equals(StartRotation, 0.5f));
			Test->AddInfo(FString::Printf(TEXT("INPUT_LOOK_RESULT Phase=Forest Before=%s After=%s"),
				*StartRotation.ToString(), *Controller->GetControlRotation().ToString()));
			if (UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement()) Movement->StopMovementImmediately();
			Controller->ShowPauseMenu();
			Test->TestTrue(TEXT("Pause opens locally"), Controller->IsPauseMenuOpen());
			Test->TestTrue(TEXT("Pause blocks move locally"), Controller->IsMoveInputIgnored());
			StartLocation = Pawn->GetActorLocation();
			SendW(IE_Pressed);
			Advance(Now);
			return false;
		case 3:
			if (!bMidInputLogged && Now - StageAt > 0.15)
			{
				LogHeldInput(TEXT("ForestPaused"), Controller, Pawn);
				const FVector2D PausedValue = Ch4InputRuntime::ReadActionValue(Controller, Pawn->MoveAction);
				Test->TestTrue(TEXT("Pause blocks IA_PlayerMove before the Character handler"), PausedValue.IsNearlyZero());
				bMidInputLogged = true;
			}
			if (Now - StageAt < 0.6) return false;
			SendW(IE_Released);
			LogMovementResult(TEXT("ForestPaused"), Pawn, false);
			Controller->HidePauseMenu();
			Test->TestFalse(TEXT("Continue clears local move block"), Controller->IsMoveInputIgnored());
			Test->TestFalse(TEXT("Continue closes Pause"), Controller->IsPauseMenuOpen());
			Advance(Now);
			return false;
		case 4:
			// Give Enhanced Input one frame with W released after UI focus is removed.
			if (Now - StageAt < 0.2) return false;
			StartLocation = Pawn->GetActorLocation();
			bMidInputLogged = false;
			SendW(IE_Pressed);
			Advance(Now);
			return false;
		case 5:
			if (!bMidInputLogged && Now - StageAt > 0.15)
			{
				LogHeldInput(TEXT("ForestAfterContinue"), Controller, Pawn);
				bMidInputLogged = true;
			}
			if (Now - StageAt < 0.8) return false;
			SendW(IE_Released);
			LogMovementResult(TEXT("ForestAfterContinue"), Pawn, true);
			JumpKey = KeyForAction(Pawn, Pawn->JumpAction);
			Test->TestTrue(TEXT("Forest existing Jump mapping exists"), JumpKey.IsValid());
			StartZ = Pawn->GetActorLocation().Z;
			SendKey(JumpKey, IE_Pressed);
			Advance(Now);
			return false;
		case 6:
			if (!bJumpReleased && Now - StageAt > 0.15)
			{
				SendKey(JumpKey, IE_Released);
				bJumpReleased = true;
			}
			if (Now - StageAt < 0.45) return false;
			Test->TestTrue(TEXT("Forest existing Jump reaches Character"),
				Pawn->GetActorLocation().Z > StartZ + 5.0 || Pawn->GetCharacterMovement()->IsFalling());
			Test->AddInfo(FString::Printf(TEXT("INPUT_JUMP_RESULT Phase=Forest StartZ=%.2f CurrentZ=%.2f MovementMode=%s"),
				StartZ, Pawn->GetActorLocation().Z, *Ch4InputRuntime::MovementModeName(Pawn->GetCharacterMovement())));
			GrabKey = KeyForAction(Pawn, Pawn->GrabAction);
			Test->TestTrue(TEXT("Forest existing Grab mapping exists"), GrabKey.IsValid());
			SendKey(GrabKey, IE_Pressed);
			Advance(Now);
			return false;
		case 7:
			if (Now - StageAt < 0.15) return false;
			if (const auto* Input = Cast<UEnhancedPlayerInput>(Controller->PlayerInput))
			{
				const bool bGrabActive = Input->GetActionValue(Pawn->GrabAction).Get<bool>();
				Test->TestTrue(TEXT("Forest existing Grab key activates its action"), bGrabActive);
				Test->AddInfo(FString::Printf(TEXT("INPUT_GRAB_RESULT Phase=Forest Active=%d Key=%s"), int32(bGrabActive), *GrabKey.ToString()));
			}
			SendKey(GrabKey, IE_Released);
			Test->AddInfo(FString::Printf(TEXT("INPUT_LIFECYCLE_COMPLETE Role=%s Controller=%s Pawn=%s"),
				Controller->HasAuthority() ? TEXT("Host") : TEXT("Client"), *Controller->GetClass()->GetPathName(), *Pawn->GetClass()->GetPathName()));
			return true;
		default:
			return false;
		}
	}

	void LogMovementResult(const TCHAR* Phase, const ACh4_PlayerCharacter* Pawn, bool bExpectMovement)
	{
		const double Distance = FVector::Dist2D(StartLocation, Pawn->GetActorLocation());
		Test->AddInfo(FString::Printf(TEXT("INPUT_MOVE_RESULT Phase=%s Distance=%.2f Start=%s End=%s Expected=%s"),
			Phase, Distance, *StartLocation.ToString(), *Pawn->GetActorLocation().ToString(), bExpectMovement ? TEXT("Move") : TEXT("Blocked")));
		if (bExpectMovement) Test->TestTrue(FString::Printf(TEXT("%s W reaches Character movement"), Phase), Distance > 10.0);
		// The pawn may settle a few centimeters on uneven ground after its velocity
		// is cleared. The held action value is separately required to remain zero.
		else Test->TestTrue(FString::Printf(TEXT("%s W remains locally blocked"), Phase), Distance < 25.0);
	}

	void Advance(double Now)
	{
		++Stage;
		StageAt = Now;
		bMidInputLogged = false;
	}

	void SendW(EInputEvent Event) const
	{
		SendKey(EKeys::W, Event);
	}

	void SendKey(FKey Key, EInputEvent Event) const
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->InputKey(FInputKeyEventArgs(
				GEngine->GameViewport->Viewport, FInputDeviceId::CreateFromInternalId(0), Key, Event, FPlatformTime::Cycles64()));
		}
	}

	void SendAxis(FKey Key, float Delta) const
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->InputKey(FInputKeyEventArgs(
				GEngine->GameViewport->Viewport, FInputDeviceId::CreateFromInternalId(0), Key, Delta, 1.0f / 30.0f, 1, FPlatformTime::Cycles64()));
		}
	}

	FKey KeyForAction(const ACh4_PlayerCharacter* Pawn, const UInputAction* Action) const
	{
		if (Pawn && Pawn->InputMappingContext)
		{
			for (const FEnhancedActionKeyMapping& Mapping : Pawn->InputMappingContext->GetMappings())
			{
				if (Mapping.Action == Action) return Mapping.Key;
			}
		}
		return FKey();
	}

	void ReleaseW() const { SendW(IE_Released); }

	FAutomationTestBase* Test;
	double StartedAt;
	double SettledAt = 0.0;
	double StageAt = 0.0;
	int32 Stage = 0;
	int32 ExpectedPlayers = 1;
	bool bMidInputLogged = false;
	bool bJumpReleased = false;
	FString ActiveMap;
	FVector StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;
	double StartZ = 0.0;
	FKey JumpKey;
	FKey GrabKey;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4PlayerInputRuntimeTest,
	"Ch4_multiGame.Player.RuntimeInputLifecycle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4PlayerInputRuntimeTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("Ch4InputLifecycleSmoke")))
	{
		AddInfo(TEXT("Requires explicit -Ch4InputLifecycleSmoke; no input is injected by the normal suite."));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCh4PlayerInputRuntimeCommand(this));
	return true;
}

#endif

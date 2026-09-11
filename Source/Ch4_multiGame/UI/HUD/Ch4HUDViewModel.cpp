#include "UI/HUD/Ch4HUDViewModel.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UCh4HUDViewModel::UCh4HUDViewModel()
{
	PhaseText = FText::FromString(TEXT("Press [R] to Ready"));
	CargoText = FText::FromString(TEXT("Cargo: -- / --"));
	PhaseBannerVisibility = ESlateVisibility::Visible;
}

void UCh4HUDViewModel::BeginDestroy()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerTickHandle);
		World->GetTimerManager().ClearTimer(GameStateBindTimer);
		World->GetTimerManager().ClearTimer(LobbyBindTimer);
		World->GetTimerManager().ClearTimer(PhaseBannerAutoHideHandle);
		World->GetTimerManager().ClearTimer(MicAnimTimerHandle);
	}
	Super::BeginDestroy();
}

UWorld* UCh4HUDViewModel::GetWorld() const
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (CachedPC.IsValid())
	{
		return CachedPC->GetWorld();
	}

	if (UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}

	if (GEngine && GEngine->GetWorldContexts().Num() > 0)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}
	}

	return nullptr;
}

void UCh4HUDViewModel::InitializeWithWorld(UWorld* World, APlayerController* OwningController)
{
	if (!World)
	{
		World = GetWorld();
	}
	if (!World) return;

	if (OwningController)
	{
		CachedPC = OwningController;
	}
	else if (!CachedPC.IsValid())
	{
		CachedPC = World->GetFirstPlayerController();
	}

	World->GetTimerManager().ClearTimer(GameStateBindTimer);
	World->GetTimerManager().ClearTimer(LobbyBindTimer);
	World->GetTimerManager().ClearTimer(PhaseBannerAutoHideHandle);

	GameStateBindRetryCount = 0;
	TryBindGameState();

	bIsInitialized = true;
}

void UCh4HUDViewModel::TryBindGameState()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 1. 인게임 Gameplay GameState가 있으면 이벤트 델리게이트 바인딩
	if (ACh4_multiGameGameState* GS = World->GetGameState<ACh4_multiGameGameState>())
	{
		World->GetTimerManager().ClearTimer(GameStateBindTimer);

		GS->OnCargoCountChanged.RemoveDynamic(this, &UCh4HUDViewModel::OnCargoCountChanged);
		GS->OnGamePhaseChanged.RemoveDynamic(this, &UCh4HUDViewModel::OnGamePhaseChanged);
		GS->OnPreparationTimerUpdated.RemoveDynamic(this, &UCh4HUDViewModel::OnPreparationTimerUpdated);

		GS->OnCargoCountChanged.AddDynamic(this, &UCh4HUDViewModel::OnCargoCountChanged);
		GS->OnGamePhaseChanged.AddDynamic(this, &UCh4HUDViewModel::OnGamePhaseChanged);
		GS->OnPreparationTimerUpdated.AddDynamic(this, &UCh4HUDViewModel::OnPreparationTimerUpdated);

		OnCargoCountChanged(GS->GetRemainingCargoCount(), GS->GetInitialCargoCount());
		OnGamePhaseChanged(GS->GetCurrentGamePhase());

		if (GS->GetCurrentGamePhase() == ECh4GamePhase::Waiting && GS->GetRemainingPreparationTime() > 0.0f)
		{
			StartTimerTick();
		}
		return;
	}

	// 2. 로비 맵 GameState인 경우
	if (ACh4_multiGameLobbyGameState* LobbyGS = World->GetGameState<ACh4_multiGameLobbyGameState>())
	{
		World->GetTimerManager().ClearTimer(GameStateBindTimer);

		PhaseText = FText::FromString(TEXT("Press [R] to Ready"));
		PhaseBannerVisibility = ESlateVisibility::Visible;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseText);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseBannerVisibility);

		TryBindLobbyPlayerState();
		return;
	}

	// 3. 아직 GameState가 레플리케이션되지 않았을 수 있으므로 재시도 (최대 5초, 50회)
	if (GameStateBindRetryCount < 50)
	{
		GameStateBindRetryCount++;
		World->GetTimerManager().SetTimer(
			GameStateBindTimer,
			this,
			&UCh4HUDViewModel::TryBindGameState,
			0.1f,
			false);
	}
	else
	{
		// 재시도 초과 시 로비 상태 바인딩 시도
		TryBindLobbyPlayerState();
	}
}

void UCh4HUDViewModel::TryBindLobbyPlayerState()
{
	APlayerController* PC = CachedPC.Get();
	if (!PC)
	{
		if (UWorld* World = GetWorld())
		{
			PC = World->GetFirstPlayerController();
			CachedPC = PC;
		}
	}

	if (!PC) return;

	if (ACh4_multiGameLobbyPlayerState* LPS = PC->GetPlayerState<ACh4_multiGameLobbyPlayerState>())
	{
		LPS->OnReadyStateChanged.RemoveDynamic(this, &UCh4HUDViewModel::OnLobbyReadyChanged);
		LPS->OnReadyStateChanged.AddDynamic(this, &UCh4HUDViewModel::OnLobbyReadyChanged);
		OnLobbyReadyChanged(LPS->IsReady());
	}
	else
	{
		// PlayerState가 복제/초기화될 때까지 0.1초마다 재시도
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				LobbyBindTimer,
				this,
				&UCh4HUDViewModel::TryBindLobbyPlayerState,
				0.1f,
				false);
		}
	}
}

void UCh4HUDViewModel::OnLobbyReadyChanged(bool bNewReady)
{
	bIsLobbyReady = bNewReady;

	if (bIsLobbyReady)
	{
		PhaseText = FText::FromString(TEXT("READY! (Press [R] to Cancel)"));
	}
	else
	{
		PhaseText = FText::FromString(TEXT("Press [R] to Ready"));
	}

	PhaseBannerVisibility = ESlateVisibility::Visible;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsLobbyReady);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseBannerVisibility);
}

void UCh4HUDViewModel::OnCargoCountChanged(int32 Remaining, int32 Initial)
{
	CargoText = FText::FromString(
		FString::Printf(TEXT("Cargo: %d / %d"), Remaining, Initial));
	CargoRatio = (Initial > 0) ? (float)Remaining / (float)Initial : 1.0f;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CargoText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CargoRatio);
}

void UCh4HUDViewModel::ToggleMic()
{
	SetMicActive(!bIsMicActive);
}

void UCh4HUDViewModel::SetMicActive(bool bActive)
{
	if (bIsMicActive != bActive)
	{
		bIsMicActive = bActive;
		MicIconIndex = bIsMicActive ? 1 : 0;
		MicOnVisibility = bIsMicActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		MicOffVisibility = bIsMicActive ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;

		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsMicActive);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicIconIndex);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicOnVisibility);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicOffVisibility);

		TriggerMicPop();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				bIsMicActive ? FColor::Green : FColor::Red,
				bIsMicActive ? TEXT("[VOICE] Mic UNMUTED (ON)") : TEXT("[VOICE] Mic MUTED (OFF)"));
		}
	}
}

void UCh4HUDViewModel::TriggerMicPop()
{
	CurrentMicScale = 1.35f;
	MicVelocity = 0.0f;
	MicScale = FVector2D(CurrentMicScale, CurrentMicScale);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicScale);

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(MicAnimTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				MicAnimTimerHandle,
				this,
				&UCh4HUDViewModel::UpdateMicAnim,
				0.016f,
				true);
		}
	}
}

void UCh4HUDViewModel::UpdateMicAnim()
{
	const float DeltaTime = 0.016f;
	const float SpringStiffness = 320.0f;
	const float SpringDamping = 18.0f;

	const float Displacement = CurrentMicScale - 1.0f;
	const float Acceleration = (-SpringStiffness * Displacement) - (SpringDamping * MicVelocity);
	MicVelocity += Acceleration * DeltaTime;
	CurrentMicScale += MicVelocity * DeltaTime;

	if (FMath::Abs(CurrentMicScale - 1.0f) < 0.005f && FMath::Abs(MicVelocity) < 0.02f)
	{
		CurrentMicScale = 1.0f;
		MicVelocity = 0.0f;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(MicAnimTimerHandle);
		}
	}

	MicScale = FVector2D(CurrentMicScale, CurrentMicScale);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicScale);
}

void UCh4HUDViewModel::OnGamePhaseChanged(ECh4GamePhase NewPhase)
{
	CurrentPhase = NewPhase;

	switch (NewPhase)
	{
	case ECh4GamePhase::Waiting:
		PhaseText = FText::FromString(TEXT("Load as many items into the cart as possible!"));
		PhaseBannerVisibility = ESlateVisibility::Visible;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PhaseBannerAutoHideHandle);
		}
		break;

	case ECh4GamePhase::Playing:
	{
		StopTimerTick();
		UWorld* World = GetWorld();
		const ACh4_multiGameGameState* GS = World ? World->GetGameState<ACh4_multiGameGameState>() : nullptr;
		float ElapsedPlaying = 0.0f;
		if (GS && GS->GetPreparationEndTime() > 0.0f && World)
		{
			ElapsedPlaying = World->GetTimeSeconds() - GS->GetPreparationEndTime();
		}

		if (ElapsedPlaying >= 10.0f)
		{
			PhaseBannerVisibility = ESlateVisibility::Collapsed;
			if (World)
			{
				World->GetTimerManager().ClearTimer(PhaseBannerAutoHideHandle);
			}
		}
		else
		{
			PhaseText = FText::FromString(TEXT("Deliver the cart to the destination!"));
			PhaseBannerVisibility = ESlateVisibility::Visible;
			const float RemainingShowTime = FMath::Clamp(10.0f - ElapsedPlaying, 0.5f, 10.0f);
			if (World)
			{
				World->GetTimerManager().ClearTimer(PhaseBannerAutoHideHandle);
				World->GetTimerManager().SetTimer(
					PhaseBannerAutoHideHandle,
					this,
					&UCh4HUDViewModel::HidePhaseBanner,
					RemainingShowTime,
					false);
			}
		}
		break;
	}

	case ECh4GamePhase::Cleared:
	case ECh4GamePhase::GameOver:
	default:
		PhaseBannerVisibility = ESlateVisibility::Collapsed;
		StopTimerTick();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PhaseBannerAutoHideHandle);
		}
		break;
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CurrentPhase);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseBannerVisibility);
}

void UCh4HUDViewModel::HidePhaseBanner()
{
	PhaseBannerVisibility = ESlateVisibility::Collapsed;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseBannerVisibility);
}

void UCh4HUDViewModel::OnPreparationTimerUpdated(float RemainingSeconds, float TotalSeconds)
{
	if (RemainingSeconds > 0.0f && CurrentPhase == ECh4GamePhase::Waiting)
	{
		StartTimerTick();
	}
	else
	{
		StopTimerTick();
	}
}

void UCh4HUDViewModel::StartTimerTick()
{
	bIsTimerActive = true;
	TimerVisibility = ESlateVisibility::Visible;
	CurrentTimerScale = 1.0f;
	TimerVelocity = 0.0f;
	TimerScale = FVector2D(1.0f, 1.0f);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsTimerActive);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerVisibility);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerScale);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimerTickHandle,
			this,
			&UCh4HUDViewModel::UpdateTimerTick,
			0.016f,
			true);
	}
	UpdateTimerTick();
}

void UCh4HUDViewModel::StopTimerTick()
{
	bIsTimerActive = false;
	TimerVisibility = ESlateVisibility::Collapsed;
	CurrentTimerScale = 1.0f;
	TimerVelocity = 0.0f;
	TimerScale = FVector2D(1.0f, 1.0f);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsTimerActive);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerVisibility);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerScale);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerTickHandle);
	}
}

void UCh4HUDViewModel::UpdateTimerTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ACh4_multiGameGameState* GS = World->GetGameState<ACh4_multiGameGameState>();
	if (!GS || GS->GetCurrentGamePhase() != ECh4GamePhase::Waiting)
	{
		StopTimerTick();
		return;
	}

	const float Remaining = GS->GetRemainingPreparationTime();
	const float Ratio = GS->GetPreparationTimeRatio();

	TimerRatio = Ratio;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerRatio);

	const int32 CurrentSeconds = FMath::CeilToInt(Remaining);
	if (CurrentSeconds != LastDisplaySeconds)
	{
		LastDisplaySeconds = CurrentSeconds;
		TimerText = FText::AsNumber(CurrentSeconds);
		bIsUrgent = (CurrentSeconds <= 10 && CurrentSeconds > 0);

		// 색상: 10초 이하 위기 상태면 코랄 레드(#FF3B30), 평소에는 밝은 웜화이트(#FFFFF0)
		TimerColor = bIsUrgent ? FSlateColor(FLinearColor(1.0f, 0.25f, 0.20f, 1.0f)) : FSlateColor(FLinearColor(1.0f, 0.98f, 0.92f, 1.0f));

		// 1초 단위 탄성 펄스 시작:
		// 5초 이하(초긴급): 1.42배 강력한 심장 박동 효과
		// 10초 이하(긴급): 1.32배
		// 평소(60~11초): 1.18배로 쫀득한 시계 틱 탄성
		if (CurrentSeconds <= 5 && CurrentSeconds > 0)
		{
			CurrentTimerScale = 1.42f;
		}
		else if (bIsUrgent)
		{
			CurrentTimerScale = 1.32f;
		}
		else
		{
			CurrentTimerScale = 1.18f;
		}
		TimerVelocity = 0.0f;

		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerText);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsUrgent);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerColor);
	}

	// 60Hz 댐핑 스프링 물리 시뮬레이션
	const float DeltaTime = 0.016f;
	const float SpringStiffness = bIsUrgent ? 350.0f : 280.0f;
	const float SpringDamping = bIsUrgent ? 16.0f : 20.0f;

	const float Displacement = CurrentTimerScale - 1.0f;
	const float Acceleration = (-SpringStiffness * Displacement) - (SpringDamping * TimerVelocity);
	TimerVelocity += Acceleration * DeltaTime;
	CurrentTimerScale += TimerVelocity * DeltaTime;

	if (FMath::Abs(CurrentTimerScale - 1.0f) < 0.005f && FMath::Abs(TimerVelocity) < 0.02f)
	{
		CurrentTimerScale = 1.0f;
		TimerVelocity = 0.0f;
	}

	TimerScale = FVector2D(CurrentTimerScale, CurrentTimerScale);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerScale);

	if (Remaining <= 0.0f)
	{
		StopTimerTick();
	}
}

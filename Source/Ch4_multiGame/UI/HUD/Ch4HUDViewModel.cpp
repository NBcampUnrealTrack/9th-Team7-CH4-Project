#include "UI/HUD/Ch4HUDViewModel.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "Engine/World.h"

void UCh4HUDViewModel::InitializeWithWorld(UWorld* World)
{
	if (!World) return;

	ACh4_multiGameGameState* GS = World->GetGameState<ACh4_multiGameGameState>();
	if (!GS) return;

	// GameState 델리게이트 바인딩 (Tick 폴링 대신 이벤트 기반)
	GS->OnCargoCountChanged.AddDynamic(this, &UCh4HUDViewModel::OnCargoCountChanged);
	GS->OnGamePhaseChanged.AddDynamic(this, &UCh4HUDViewModel::OnGamePhaseChanged);

	// 현재 값으로 초기값 세팅
	OnCargoCountChanged(GS->GetRemainingCargoCount(), GS->GetInitialCargoCount());
	OnGamePhaseChanged(GS->GetCurrentGamePhase());
}

void UCh4HUDViewModel::OnCargoCountChanged(int32 Remaining, int32 Initial)
{
	CargoText = FText::FromString(
		FString::Printf(TEXT("Cargo: %d / %d"), Remaining, Initial));
	CargoRatio = (Initial > 0) ? (float)Remaining / (float)Initial : 1.0f;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CargoText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CargoRatio);
}

void UCh4HUDViewModel::OnGamePhaseChanged(ECh4GamePhase NewPhase)
{
	CurrentPhase = NewPhase;

	switch (NewPhase)
	{
	case ECh4GamePhase::Waiting:
		PhaseText = FText::FromString(TEXT("Waiting for Players..."));
		break;
	case ECh4GamePhase::Playing:
		PhaseText = FText::FromString(TEXT("Game Start!"));
		break;
	case ECh4GamePhase::Cleared:
		PhaseText = FText::FromString(TEXT("Mission Complete!"));
		break;
	case ECh4GamePhase::GameOver:
		PhaseText = FText::FromString(TEXT("Mission Failed..."));
		break;
	default:
		break;
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CurrentPhase);
}

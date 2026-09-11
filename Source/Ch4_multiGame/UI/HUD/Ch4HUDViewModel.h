#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Components/SlateWrapperTypes.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Ch4HUDViewModel.generated.h"

class ACh4_multiGameGameState;

UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4HUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UCh4HUDViewModel();

	virtual UWorld* GetWorld() const override;
	virtual void BeginDestroy() override;

	// GameState 및 PlayerState 이벤트 바인딩 초기화
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void InitializeWithWorld(UWorld* World, APlayerController* OwningController = nullptr);
	
	// --- 화물 -----
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Cargo")
	FText CargoText = FText::FromString(TEXT("Cargo: -- / --"));
	
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Cargo")
	float CargoRatio = 1.0f; // 0.0 ~ 1.0 (30% 이하면 빨강 처리용)
	
	// --- 게임 페이즈 / 로비 레디 상태 ------
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Phase")
	FText PhaseText = FText::FromString(TEXT("Press [R] to Ready"));

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Phase")
	ESlateVisibility PhaseBannerVisibility = ESlateVisibility::Visible;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Phase")
	ECh4GamePhase CurrentPhase = ECh4GamePhase::Waiting;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Lobby")
	bool bIsLobbyReady = false;

	UFUNCTION(BlueprintCallable, Category = "HUD|Phase")
	void HidePhaseBanner();

	// --- 보이스 마이크 ON/OFF ------
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Voice")
	bool bIsMicActive = false; // 기본값 OFF (꺼짐)

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Voice")
	int32 MicIconIndex = 0; // 0 = OFF, 1 = ON (Active Widget Index와 1:1 직결용)

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Voice")
	ESlateVisibility MicOnVisibility = ESlateVisibility::Collapsed; // 켜짐 아이콘 가시성

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Voice")
	ESlateVisibility MicOffVisibility = ESlateVisibility::Visible; // 꺼짐 아이콘 가시성

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Voice")
	FVector2D MicScale = FVector2D(1.0f, 1.0f); // 마이크 팝 탄성 스케일

	// --- 60초 카운트다운 타이머 -----
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Timer")
	FText TimerText = FText::FromString(TEXT("60"));

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Timer")
	float TimerRatio = 1.0f; // 0.0 ~ 1.0 (원형 프로그레스 바 Fill용)

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Timer")
	bool bIsTimerActive = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Timer")
	bool bIsUrgent = false; // 10초 이하 시 true (위기 알림용)

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Timer")
	FSlateColor TimerColor = FSlateColor(FLinearColor(1.0f, 0.95f, 0.85f));

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Timer")
	ESlateVisibility TimerVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "HUD|Timer")
	FVector2D TimerScale = FVector2D(1.0f, 1.0f);

	UFUNCTION(BlueprintCallable, Category = "HUD|Timer")
	void StartTimerTick();

	UFUNCTION(BlueprintCallable, Category = "HUD|Timer")
	void StopTimerTick();

	UFUNCTION(BlueprintCallable, Category = "HUD|Voice")
	void ToggleMic();

	UFUNCTION(BlueprintCallable, Category = "HUD|Voice")
	void SetMicActive(bool bActive);

private:
	UFUNCTION()
	void OnCargoCountChanged(int32 Remaining, int32 Initial);
	
	UFUNCTION()
	void OnGamePhaseChanged(ECh4GamePhase NewPhase);

	UFUNCTION()
	void OnLobbyReadyChanged(bool bNewReady);

	UFUNCTION()
	void OnPreparationTimerUpdated(float RemainingSeconds, float TotalSeconds);

	void UpdateTimerTick();
	void TriggerMicPop();
	void UpdateMicAnim();
	void TryBindGameState();
	void TryBindLobbyPlayerState();
	void AutoInitializeIfPossible();

private:
	TWeakObjectPtr<APlayerController> CachedPC;
	FTimerHandle LobbyBindTimer;
	FTimerHandle GameStateBindTimer;
	FTimerHandle PhaseBannerAutoHideHandle;
	FTimerHandle TimerTickHandle;
	FTimerHandle MicAnimTimerHandle;
	int32 GameStateBindRetryCount = 0;
	int32 LastDisplaySeconds = -1;
	float CurrentTimerScale = 1.0f;
	float TimerVelocity = 0.0f;
	float CurrentMicScale = 1.0f;
	float MicVelocity = 0.0f;
	bool bIsInitialized = false;
};
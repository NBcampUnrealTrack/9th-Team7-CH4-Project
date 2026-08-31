#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Ch4HUDViewModel.generated.h"

class ACh4_multiGameGameState;

UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4HUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	// GameState 이벤트 바인딩 초기화
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void InitializeWithWorld(UWorld* World);
	
	// --- 화물 -----
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Cargo")
	FText CargoText;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Cargo")
	float CargoRatio = 1.0f; // 0.0 ~ 1.0 (30% 이하면 빨강 처리횽)
	
	// --- 게임 페이즈 ------
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Phase")
	FText PhaseText;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Phase")
	ECh4GamePhase CurrentPhase = ECh4GamePhase::Waiting;
	
private:
	UFUNCTION()
	void OnCargoCountChanged(int32 Remaining, int32 Initial);
	
	UFUNCTION()
	void OnGamePhaseChanged(ECh4GamePhase NewPhase);
	
};
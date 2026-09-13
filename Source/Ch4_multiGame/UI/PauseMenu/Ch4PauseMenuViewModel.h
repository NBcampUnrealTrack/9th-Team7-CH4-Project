#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Ch4PauseMenuViewModel.generated.h"

// PauseMenu UI 전반의 상태와 버튼 로직을 제어하는 ViewModel

UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4PauseMenuViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	void InitializeWithPlayerController(class ACh4_multiGamePlayerController* PlayerController);
	// ----------------------------------------------------------------
	// [State] View Binding용 데이터 속성
	// ----------------------------------------------------------------

	// Settings 패널이 열려있는지 여부 (기본값 false로 하여 PauseMenu 메인 카드가 기본 활성화)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "PauseMenu|State")
	bool bIsSettingsVisible = false;

	// bIsSettingsVisible Setter (FieldNotify 변경 알림)
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|State")
	void SetIsSettingsVisible(bool bNewValue);

	// bIsSettingsVisible Getter
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PauseMenu|State")
	bool GetIsSettingsVisible() const { return bIsSettingsVisible; }

	// ----------------------------------------------------------------
	// [Actions] 버튼 클릭 명령 함수
	// ----------------------------------------------------------------

	// [Resume] 버튼 클릭 시 : 메뉴 닫기
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void ResumeGame();
	
	// [Settings] 버튼 클릭 시 : 설정 패널 열기
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void OpenSettings();

	// [Back / Close] 설정 패널 닫기 (메인 메뉴로 복귀)
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void CloseSettings();
	
	// [Main Menu] 버튼 클릭 시 : 메인 메뉴로 퇴장
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void ReturnToMainMenu();
	
	// [Quit Game] 버튼 클릭 시 : 게임 완전 종료
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void QuitGame();
	
private:
	TWeakObjectPtr<class ACh4_multiGamePlayerController> OwningPlayerController;
	class ACh4_multiGamePlayerController* GetOwningCh4PlayerController() const;
	
};


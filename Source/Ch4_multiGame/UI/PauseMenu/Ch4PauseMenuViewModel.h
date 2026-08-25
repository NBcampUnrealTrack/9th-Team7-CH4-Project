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
	// [Resume] 버튼 클릭 시 : 메뉴 닫기
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void ResumeGame();
	
	// [Settings] 버튼 클릭 시 : 설정 패널 열기 (추후 구현)
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void OpenSettings();
	
	// [Main Menu] 버튼 클릭 시 : 메인 메뉴로 퇴장
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void ReturnToMainMenu();
	
	// [Quit Game] 버튼 클릭 시 : 게임 완전 종료
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void QuitGame();
	
private:
	class ACh4_multiGamePlayerController* GetOwningCh4PlayerController() const;
	
};

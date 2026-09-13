#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ch4PauseMenuWidget.generated.h"

class UButton;
class UCh4SettingsWidget;

/**
 * WBP_PauseMenu 전용 C++ 베이스 위젯 클래스.
 * Resume, Settings, MainMenu, QuitGame 버튼 이벤트 및
 * 자식 설정창(WBP_Settings)의 수명주기를 안전하게 중재합니다.
 */
UCLASS()
class CH4_MULTIGAME_API UCh4PauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void OnResumeClicked();

	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void OnSettingsClicked();

	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void OnMainMenuClicked();

	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Actions")
	void OnQuitGameClicked();

	// 자식 설정창이 현재 열려있는지 여부 확인
	UFUNCTION(BlueprintPure, Category = "PauseMenu|Settings")
	bool IsSettingsOpen() const;

	// 자식 설정창 닫기 및 일시정지 메뉴 포커스 회수
	UFUNCTION(BlueprintCallable, Category = "PauseMenu|Settings")
	void CloseSettingsWindow();

	// 자식 설정창이 완전히 닫혔을 때 일시정지 메뉴로 포커스 복귀
	UFUNCTION()
	void HandleSettingsClosed();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Resume;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Settings;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_MainMenu;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_QuitGame;

	// 자식 설정창 위젯 레퍼런스
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCh4SettingsWidget> WBP_Settings;

private:
	class ACh4_multiGamePlayerController* GetOwningCh4PlayerController() const;
};

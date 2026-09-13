#include "UI/PauseMenu/Ch4PauseMenuWidget.h"
#include "UI/Settings/Ch4SettingsWidget.h"
#include "Ch4_multiGamePlayerController.h"
#include "Components/Button.h"

void UCh4PauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.AddUniqueDynamic(this, &UCh4PauseMenuWidget::OnResumeClicked);
	}
	if (Btn_Settings)
	{
		Btn_Settings->OnClicked.AddUniqueDynamic(this, &UCh4PauseMenuWidget::OnSettingsClicked);
	}
	if (Btn_MainMenu)
	{
		Btn_MainMenu->OnClicked.AddUniqueDynamic(this, &UCh4PauseMenuWidget::OnMainMenuClicked);
	}
	if (Btn_QuitGame)
	{
		Btn_QuitGame->OnClicked.AddUniqueDynamic(this, &UCh4PauseMenuWidget::OnQuitGameClicked);
	}

	// 일시정지 메뉴가 열릴 때 자식 설정창은 기본적으로 닫힌 상태로 시작 및 닫힘 이벤트 연결
	if (WBP_Settings)
	{
		WBP_Settings->OnSettingsClosed.AddUniqueDynamic(this, &UCh4PauseMenuWidget::HandleSettingsClosed);
		WBP_Settings->CloseSettingsImmediate();
	}
}

void UCh4PauseMenuWidget::NativeDestruct()
{
	// 일시정지 메뉴가 닫힐 때 열려있던 설정창도 깨끗하게 정리
	if (WBP_Settings)
	{
		WBP_Settings->CloseSettingsImmediate();
	}

	Super::NativeDestruct();
}

ACh4_multiGamePlayerController* UCh4PauseMenuWidget::GetOwningCh4PlayerController() const
{
	return Cast<ACh4_multiGamePlayerController>(GetOwningPlayer());
}

void UCh4PauseMenuWidget::OnResumeClicked()
{
	if (WBP_Settings)
	{
		WBP_Settings->CloseSettingsImmediate();
	}

	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->HidePauseMenu();
	}
}

void UCh4PauseMenuWidget::OnSettingsClicked()
{
	if (WBP_Settings)
	{
		WBP_Settings->OpenSettings();
	}
}

void UCh4PauseMenuWidget::OnMainMenuClicked()
{
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->ReturnToMainMenu();
	}
}

void UCh4PauseMenuWidget::OnQuitGameClicked()
{
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->QuitGame();
	}
}

bool UCh4PauseMenuWidget::IsSettingsOpen() const
{
	return WBP_Settings && WBP_Settings->IsVisible();
}

void UCh4PauseMenuWidget::CloseSettingsWindow()
{
	if (WBP_Settings)
	{
		WBP_Settings->CloseSettings();
	}
	SetFocus();
}

void UCh4PauseMenuWidget::HandleSettingsClosed()
{
	SetFocus();
}


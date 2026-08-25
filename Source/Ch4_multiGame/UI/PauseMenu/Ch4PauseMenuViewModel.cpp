#include "UI/PauseMenu/Ch4PauseMenuViewModel.h"
#include "Ch4_multiGamePlayerController.h"
#include "Kismet/GameplayStatics.h"

ACh4_multiGamePlayerController* UCh4PauseMenuViewModel::GetOwningCh4PlayerController() const
{
	if (UWorld* World = GetWorld())
	{
		return Cast<ACh4_multiGamePlayerController>(World->GetFirstPlayerController());
	}
	return nullptr;
}

// Settings 패널 표시 여부 변경 (FieldNotify 변경 알림 포함)
void UCh4PauseMenuViewModel::SetIsSettingsVisible(bool bNewValue)
{
	if (bIsSettingsVisible != bNewValue)
	{
		bIsSettingsVisible = bNewValue;
		// FieldNotify 변경 알림 → View Binding이 자동으로 위젯 Visibility에 반영
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsSettingsVisible);
	}
}

void UCh4PauseMenuViewModel::ResumeGame()
{
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->HidePauseMenu();
	}
}

void UCh4PauseMenuViewModel::OpenSettings()
{
	// Settings 패널 토글 (열려있으면 닫기, 닫혀있으면 열기)
	SetIsSettingsVisible(!bIsSettingsVisible);
}

void UCh4PauseMenuViewModel::ReturnToMainMenu()
{
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->ReturnToMainMenu();
	}
}

void UCh4PauseMenuViewModel::QuitGame()
{
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->QuitGame();
	}
}

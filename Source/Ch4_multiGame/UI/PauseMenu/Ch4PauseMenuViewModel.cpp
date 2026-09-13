#include "UI/PauseMenu/Ch4PauseMenuViewModel.h"
#include "Ch4_multiGamePlayerController.h"
#include "Blueprint/UserWidget.h"

void UCh4PauseMenuViewModel::InitializeWithPlayerController(ACh4_multiGamePlayerController* PlayerController)
{
	OwningPlayerController = PlayerController;
}

ACh4_multiGamePlayerController* UCh4PauseMenuViewModel::GetOwningCh4PlayerController() const
{
	ACh4_multiGamePlayerController* Controller = OwningPlayerController.Get();
	if (!Controller) Controller = GetTypedOuter<ACh4_multiGamePlayerController>();
	if (!Controller)
	{
		if (const UUserWidget* Widget = GetTypedOuter<UUserWidget>())
		{
			Controller = Cast<ACh4_multiGamePlayerController>(Widget->GetOwningPlayer());
		}
	}
	return Controller && Controller->IsLocalController() ? Controller : nullptr;
}

// Settings 패널 표시 여부 변경 (FieldNotify 변경 알림)
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
	SetIsSettingsVisible(true);
}

void UCh4PauseMenuViewModel::CloseSettings()
{
	SetIsSettingsVisible(false);
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

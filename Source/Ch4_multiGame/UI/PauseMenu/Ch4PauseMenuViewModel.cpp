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

void UCh4PauseMenuViewModel::ResumeGame()
{
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->HidePauseMenu();
	}
}

void UCh4PauseMenuViewModel::OpenSettings()
{
	// 추후 설정창 (볼륨/FOV/그래픽 패널) 열기 로직 추가 예정
	UE_LOG(LogTemp, Warning, TEXT("OpenSettings called from ViewModel"));
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

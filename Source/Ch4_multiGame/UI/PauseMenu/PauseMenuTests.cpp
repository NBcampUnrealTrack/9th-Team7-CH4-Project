// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Ch4_multiGamePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"

class FCh4PauseMenuRuntimeCommand final : public IAutomationLatentCommand
{
public:
	explicit FCh4PauseMenuRuntimeCommand(FAutomationTestBase* InTest)
		: Test(InTest), Started(FPlatformTime::Seconds()) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 30.0)
		{
			Test->AddError(TEXT("Timed out waiting for local ESC menu input."));
			if (Controller.IsValid()) Controller->HidePauseMenu();
			return true;
		}
		if (!Controller.IsValid())
		{
			if (!GEngine) return false;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (!World || Context.WorldType != EWorldType::Game || !World->HasBegunPlay()) continue;
				Controller = Cast<ACh4_multiGamePlayerController>(World->GetFirstPlayerController());
				if (Controller.IsValid() && Controller->IsLocalController()) break;
				Controller.Reset();
			}
			if (!Controller.IsValid()) return false;
			bMoveIgnoredBefore = Controller->IsMoveInputIgnored();
			bLookIgnoredBefore = Controller->IsLookInputIgnored();
			bBlockedBefore = Controller->InputComponent && Controller->InputComponent->bBlockInput;
		}
		if (Now - StageStarted < 0.25) return false;
		switch (Stage)
		{
		case 0:
			SendEscape(IE_Pressed);
			break;
		case 1:
			SendEscape(IE_Released);
			break;
		case 2:
			if (!Controller->IsPauseMenuOpen())
			{
				Test->AddError(TEXT("ESC did not open the cooked Pause widget through IA_Pause."));
				return true;
			}
			Controller->ShowPauseMenu(); // Repeated requests are idempotent.
			// Reproduce a late ClientRestart arriving after the menu was opened.
			// Its engine reset must not release the menu's independent local lock.
			Controller->ResetIgnoreInputFlags();
			Test->TestTrue(TEXT("Only the local menu captures Move and Look input"),
				Controller->IsMoveInputIgnored() && Controller->IsLookInputIgnored());
			Test->TestTrue(TEXT("Pawn action input is blocked while controller menu input stays active"),
				Controller->InputComponent && Controller->InputComponent->bBlockInput);
			Test->TestFalse(TEXT("Opening a menu does not pause the multiplayer world"), Controller->GetWorld()->IsPaused());
			WorldTimeAtOpen = Controller->GetWorld()->GetTimeSeconds();
			if (FApp::CanEverRender())
			{
				const FString Screenshot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/PauseMenuSmoke.png"));
				FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
				Test->AddInfo(FString::Printf(TEXT("Pause screenshot: %s"), *Screenshot));
			}
			break;
		case 3:
			if (Now - StageStarted < 2.0) return false;
			Test->TestTrue(TEXT("World simulation continues while the local menu is open"),
				Controller->GetWorld()->GetTimeSeconds() > WorldTimeAtOpen + 0.1f);
			for (const TCHAR* ButtonName : {TEXT("Btn_MainMenu"), TEXT("Btn_QuitGame")})
			{
				const UButton* Button = Cast<UButton>(Controller->PauseMenuWidget->GetWidgetFromName(ButtonName));
				Test->TestTrue(FString::Printf(TEXT("Existing %s button has its action binding"), ButtonName),
					Button && Button->OnClicked.IsBound());
			}
			if (UButton* Resume = Cast<UButton>(Controller->PauseMenuWidget->GetWidgetFromName(TEXT("Btn_Resume"))))
			{
				Resume->OnClicked.Broadcast(); // Exercise the existing Blueprint -> ViewModel -> owning PC route.
			}
			else Test->AddError(TEXT("Existing Btn_Resume was not found."));
			break;
		case 4:
			VerifyClosed();
			SendEscape(IE_Pressed);
			break;
		case 5:
			SendEscape(IE_Released);
			break;
		case 6:
			Test->TestTrue(TEXT("ESC can reopen the menu after Continue"), Controller->IsPauseMenuOpen());
			SendEscape(IE_Pressed);
			break;
		case 7:
			SendEscape(IE_Released);
			break;
		case 8:
			VerifyClosed();
			Test->AddInfo(TEXT("LOCAL_PAUSE_CHECKS_COMPLETE: ESC, Continue, repeat toggle and world tick checked."));
			return true;
		}
		++Stage;
		StageStarted = Now;
		return false;
	}
private:
	void SendEscape(EInputEvent Event)
	{
		Controller->InputKey(FInputKeyEventArgs(GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr,
			FInputDeviceId::CreateFromInternalId(0), EKeys::Escape, Event, FPlatformTime::Cycles64()));
	}
	void VerifyClosed()
	{
		Test->TestFalse(TEXT("Menu is closed"), Controller->IsPauseMenuOpen());
		Test->TestEqual(TEXT("Move input state is restored"), Controller->IsMoveInputIgnored(), bMoveIgnoredBefore);
		Test->TestEqual(TEXT("Look input state is restored"), Controller->IsLookInputIgnored(), bLookIgnoredBefore);
		Test->TestEqual(TEXT("Prior input blocking is restored"), bool(Controller->InputComponent->bBlockInput), bBlockedBefore);
		Test->TestFalse(TEXT("Cursor is hidden after Continue or ESC"), bool(Controller->bShowMouseCursor));
		Test->TestFalse(TEXT("The world remains unpaused"), Controller->GetWorld()->IsPaused());
	}
	FAutomationTestBase* Test;
	TWeakObjectPtr<ACh4_multiGamePlayerController> Controller;
	double Started;
	double StageStarted = 0.0;
	float WorldTimeAtOpen = 0.0f;
	int32 Stage = 0;
	bool bMoveIgnoredBefore = false;
	bool bLookIgnoredBefore = false;
	bool bBlockedBefore = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4PauseMenuRuntimeTest,
	"Ch4_multiGame.Pause.RuntimeMenu",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4PauseMenuRuntimeTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("Ch4PauseSmoke")))
	{
		AddInfo(TEXT("Requires -Ch4PauseSmoke in an opt-in game process; no input sent."));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCh4PauseMenuRuntimeCommand(this));
	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Ch4_multiGameLobbyGameMode.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerController.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "Player/Ch4_PlayerCharacter.h"
#include "Player/Ch4CharacterTypes.h"
#include "UI/HUD/Ch4HUDViewModel.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4LobbyInitializationContract,
    "Ch4_multiGame.Lobby.InitializationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4LobbyInitializationContract::RunTest(const FString&)
{
    const UWorld* Lobby = LoadObject<UWorld>(nullptr, TEXT("/Game/Lobby/L_Lobby.L_Lobby"));
    if (!TestNotNull(TEXT("Actual Lobby world exists"), Lobby)) return false;
    const UClass* ModeClass = Lobby->GetWorldSettings()->DefaultGameMode;
    if (!TestTrue(TEXT("World override uses Lobby GameMode"), ModeClass && ModeClass->IsChildOf(ACh4_multiGameLobbyGameMode::StaticClass()))) return false;
    const auto* Mode = ModeClass->GetDefaultObject<ACh4_multiGameLobbyGameMode>();
    TestTrue(TEXT("Lobby controller class"), Mode->PlayerControllerClass->IsChildOf(ACh4_multiGameLobbyPlayerController::StaticClass()));
    TestTrue(TEXT("Lobby player state class"), Mode->PlayerStateClass->IsChildOf(ACh4_multiGameLobbyPlayerState::StaticClass()));
    TestTrue(TEXT("Lobby game state class"), Mode->GameStateClass->IsChildOf(ACh4_multiGameLobbyGameState::StaticClass()));
    for (int32 Slot = 0; Slot < 4; ++Slot)
    {
        UClass* CharacterClass = LoadClass<ACh4_PlayerCharacter>(nullptr, Ch4Character::GetClassPath(Ch4Character::FromIndex(Slot)));
        if (!TestNotNull(FString::Printf(TEXT("Character slot %d uses existing player class"), Slot + 1), CharacterClass)) continue;
        const auto* Pawn = CharacterClass->GetDefaultObject<ACh4_PlayerCharacter>();
        TestTrue(TEXT("Pawn retains existing player mapping and actions"), Pawn->InputMappingContext && Pawn->MoveAction && Pawn->LookAction && Pawn->JumpAction && Pawn->GrabAction);
    }
    TestNotNull(TEXT("Existing local HUD class can load"), LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/WBP_HUD.WBP_HUD_C")));
    return true;
}

// Run on one participant at a time with a passive peer so testing R does not
// start a match. Keyboard/mouse events enter at the viewport, not at the Pawn or
// InputAction, which exposes UI-only state and competing mappings after travel.
class FCh4LobbyInitializationCommand final : public IAutomationLatentCommand
{
public:
    explicit FCh4LobbyInitializationCommand(FAutomationTestBase* InTest)
        : Test(InTest), Started(FPlatformTime::Seconds())
    {
        FParse::Value(FCommandLine::Get(), TEXT("Ch4LobbyInitDestination="), Destination);
    }

    virtual bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 100.0)
        {
            ReleaseKeys();
            Test->AddError(FString::Printf(TEXT("Lobby initialization smoke timed out at stage %d"), Stage));
            return true;
        }
        if (!GEngine) return false;
        UWorld* World = nullptr;
        APlayerController* LocalPC = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* Candidate = Context.World();
            if (!Candidate || Context.WorldType != EWorldType::Game || !Candidate->HasBegunPlay()) continue;
            APlayerController* CandidatePC = Candidate->GetFirstPlayerController();
            if (CandidatePC && CandidatePC->IsLocalController()) { World = Candidate; LocalPC = CandidatePC; break; }
        }
        if (!World || !LocalPC) return false;
        if (World->GetPackage()->GetName() == TEXT("/Game/Maps/L_MainMenu"))
        {
            if (!bTravelRequested && !Destination.IsEmpty() && Now - Started > 2.0)
            {
                Test->AddInfo(FString::Printf(TEXT("LOBBY_MENU_BOUNDARY InputMode=%s ViewportIgnore=%d"),
                    *LocalPC->GetCurrentInputModeDebugString(), int32(GEngine->GameViewport->IgnoreInput())));
                bTravelRequested = true;
                if (Destination.StartsWith(TEXT("/Game/")))
                    UGameplayStatics::OpenLevel(World, FName(*Destination), true, TEXT("listen"));
                else LocalPC->ClientTravel(Destination, TRAVEL_Absolute);
            }
            return false;
        }
        if (World->GetPackage()->GetName() != TEXT("/Game/Lobby/L_Lobby")) return false;
        Controller = Cast<ACh4_multiGameLobbyPlayerController>(LocalPC);
        Pawn = Cast<ACh4_PlayerCharacter>(LocalPC->GetPawn());
        auto* State = LocalPC->GetPlayerState<ACh4_multiGameLobbyPlayerState>();
        const auto* GameState = World->GetGameState<ACh4_multiGameLobbyGameState>();
        if (!Controller.IsValid() || !Pawn.IsValid() || !State || !GameState || GameState->GetCurrentPlayerCount() != 2) return false;
        if (SettledAt == 0.0) SettledAt = Now;
        if (Now - SettledAt < 2.0) return false;
        if (Stage == 3 && Now - StageAt < 0.35)
        {
            SendAxis(EKeys::MouseX, 2.0f);
            SendAxis(EKeys::MouseY, 1.0f);
            return false;
        }
        if (Now - StageAt < 0.3) return false;
        switch (Stage)
        {
        case 0:
            InspectInitialization(World, State);
            if (FParse::Param(FCommandLine::Get(), TEXT("Ch4LobbyInitScreenshot")))
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/LobbyInitialization.png"), true, false);
            StartLocation = Pawn->GetActorLocation();
            SendKey(EKeys::W, IE_Pressed);
            break;
        case 1:
            if (Now - StageAt < 0.6) return false;
            SendKey(EKeys::W, IE_Released);
            Test->TestTrue(TEXT("W reaches existing Character movement"), FVector::Dist2D(StartLocation, Pawn->GetActorLocation()) > 10.0f);
            Test->AddInfo(FString::Printf(TEXT("LOBBY_MOVE Distance=%.2f"), FVector::Dist2D(StartLocation, Pawn->GetActorLocation())));
            break;
        case 2:
            StartRotation = Controller->GetControlRotation();
            SendAxis(EKeys::MouseX, 2.0f);
            break;
        case 3:
            Test->TestTrue(TEXT("Mouse reaches existing Character look"), !Controller->GetControlRotation().Equals(StartRotation, 0.5f));
            Test->AddInfo(FString::Printf(TEXT("LOBBY_LOOK Before=%s After=%s"), *StartRotation.ToString(), *Controller->GetControlRotation().ToString()));
            JumpKey = KeyForAction(Pawn->JumpAction);
            Test->TestTrue(TEXT("Existing Jump mapping exists"), JumpKey.IsValid());
            StartLocation = Pawn->GetActorLocation();
            SendKey(JumpKey, IE_Pressed);
            break;
        case 4:
            SendKey(JumpKey, IE_Released);
            Test->TestTrue(TEXT("Existing Jump key makes the pawn jump"), Pawn->GetActorLocation().Z > StartLocation.Z + 5.0);
            break;
        case 5:
            if (Now - StageAt < 1.5) return false;
            GrabKey = KeyForAction(Pawn->GrabAction);
            Test->TestTrue(TEXT("Existing Grab mapping exists"), GrabKey.IsValid());
            SendKey(GrabKey, IE_Pressed);
            break;
        case 6:
            if (const auto* Input = Cast<UEnhancedPlayerInput>(Controller->PlayerInput))
                Test->TestTrue(TEXT("Existing Grab key activates its action"), Input->GetActionValue(Pawn->GrabAction).Get<bool>());
            SendKey(GrabKey, IE_Released);
            SendKey(EKeys::R, IE_Pressed);
            break;
        case 7:
            SendKey(EKeys::R, IE_Released);
            break;
        case 8:
            Test->TestTrue(TEXT("R reaches the Ready RPC"), State->IsReady());
            CheckReadyViewModel(true);
            SendKey(EKeys::R, IE_Pressed);
            break;
        case 9:
            SendKey(EKeys::R, IE_Released);
            break;
        case 10:
            Test->TestFalse(TEXT("R cancels Ready"), State->IsReady());
            CheckReadyViewModel(false);
            Test->AddInfo(FString::Printf(TEXT("LOBBY_INITIALIZATION_CHECKS_COMPLETE Role=%s Pawn=%s"),
                Controller->HasAuthority() ? TEXT("Host") : TEXT("Client"), *Pawn->GetClass()->GetPathName()));
            return true;
        }
        ++Stage;
        StageAt = Now;
        return false;
    }

private:
    void InspectInitialization(UWorld* World, ACh4_multiGameLobbyPlayerState* State)
    {
        Test->AddInfo(FString::Printf(TEXT("LOBBY_INIT Role=%s GameMode=%s Controller=%s PlayerState=%s Pawn=%s Possessed=%d InputMode=%s ViewportIgnore=%d MoveIgnored=%d LookIgnored=%d PauseOpen=%d"),
            Controller->HasAuthority() ? TEXT("Host") : TEXT("Client"), *GetNameSafe(World->GetAuthGameMode()),
            *Controller->GetClass()->GetPathName(), *State->GetClass()->GetPathName(), *Pawn->GetClass()->GetPathName(),
            int32(Pawn->GetController() == Controller.Get()), *Controller->GetCurrentInputModeDebugString(),
            int32(GEngine->GameViewport->IgnoreInput()), int32(Controller->IsMoveInputIgnored()), int32(Controller->IsLookInputIgnored()), int32(Controller->IsPauseMenuOpen())));
        Test->TestTrue(TEXT("Local Pawn is possessed"), Pawn->GetController() == Controller.Get());
        Test->TestFalse(TEXT("Fresh Lobby viewport accepts game input"), GEngine->GameViewport->IgnoreInput());
        Test->TestFalse(TEXT("Fresh Lobby has no Pause input lock"), Controller->IsPauseMenuOpen() || Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored());
        auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
        for (const TCHAR* Path : {TEXT("/Game/Player/Input/IMC_Player.IMC_Player"), TEXT("/Game/Input/Lobby/IMC_Lobby.IMC_Lobby"), TEXT("/Game/Input/IMC_Default.IMC_Default")})
        {
            auto* Context = LoadObject<UInputMappingContext>(nullptr, Path);
            int32 Priority = INDEX_NONE;
            const bool bApplied = Subsystem && Context && Subsystem->HasMappingContext(Context, Priority);
            Test->AddInfo(FString::Printf(TEXT("LOBBY_MAPPING %s Applied=%d Priority=%d"), Path, int32(bApplied), Priority));
            if (Context == Pawn->InputMappingContext || FString(Path).Contains(TEXT("IMC_Lobby"))) Test->TestTrue(TEXT("Player and Ready contexts are applied"), bApplied);
        }
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UUserWidget::StaticClass(), true);
        for (UUserWidget* Widget : Widgets)
        {
            Test->AddInfo(FString::Printf(TEXT("LOBBY_WIDGET Class=%s Owner=%s Viewport=%d Visibility=%d Opacity=%.2f"),
                *Widget->GetClass()->GetPathName(), *GetNameSafe(Widget->GetOwningPlayer()), int32(Widget->IsInViewport()), int32(Widget->GetVisibility()), Widget->GetRenderOpacity()));
            if (Widget->GetClass()->GetName() != TEXT("WBP_HUD_C") || Widget->GetOwningPlayer() != Controller.Get()) continue;
            HUD = Widget;
#if UE_WITH_MVVM_DEBUGGING
            if (const UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget))
            {
                if (const UMVVMViewClass* ViewClass = View->GetViewClass())
                    for (const auto& Binding : ViewClass->GetBindings())
                        Test->AddInfo(TEXT("LOBBY_HUD_BINDING ") + Binding.ToString(ViewClass, FMVVMViewClass_Binding::FToStringArgs::All()));
            }
#endif
            TArray<UWidget*> Children;
            Widget->WidgetTree->GetAllWidgets(Children);
            for (UWidget* Child : Children)
            {
                if (const auto* Text = Cast<UTextBlock>(Child))
                {
                    Test->AddInfo(FString::Printf(TEXT("LOBBY_HUD_TEXT %s Visibility=%d Text=%s"), *Text->GetName(), int32(Text->GetVisibility()), *Text->GetText().ToString()));
                    for (const UWidget* Parent = Text; Parent; Parent = Parent->GetParent())
                        Test->AddInfo(FString::Printf(TEXT("LOBBY_HUD_LAYOUT %s Visibility=%d Opacity=%.2f Size=%s"), *Parent->GetName(), int32(Parent->GetVisibility()), Parent->GetRenderOpacity(), *Parent->GetCachedGeometry().GetAbsoluteSize().ToString()));
                }
            }
        }
        Test->TestTrue(TEXT("Existing local Lobby HUD is on screen and visible"), HUD.IsValid() && HUD->IsInViewport() && HUD->IsVisible() && HUD->GetRenderOpacity() > 0.0f);
        CheckReadyViewModel(false);
    }
    void CheckReadyViewModel(bool bReady)
    {
        UMVVMView* View = HUD.IsValid() ? UMVVMSubsystem::GetViewFromUserWidget(HUD.Get()) : nullptr;
        auto* ViewModel = View ? Cast<UCh4HUDViewModel>(View->GetViewModel(TEXT("Ch4HUDViewModel")).GetObject()) : nullptr;
        Test->TestTrue(TEXT("Displayed HUD uses the initialized local Ready ViewModel"), ViewModel && ViewModel->bIsLobbyReady == bReady);
        const auto* Banner = HUD.IsValid() ? Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("Text_PhaseBanner"))) : nullptr;
        Test->TestTrue(TEXT("Visible phase banner displays the local Ready ViewModel text"), Banner && ViewModel && Banner->GetText().EqualTo(ViewModel->PhaseText));
    }
    FKey KeyForAction(const UInputAction* Action) const
    {
        if (Pawn.IsValid() && Pawn->InputMappingContext)
            for (const FEnhancedActionKeyMapping& Mapping : Pawn->InputMappingContext->GetMappings())
                if (Mapping.Action == Action) return Mapping.Key;
        return FKey();
    }
    void SendKey(FKey Key, EInputEvent Event)
    {
        if (GEngine && GEngine->GameViewport && Key.IsValid())
            GEngine->GameViewport->InputKey(FInputKeyEventArgs(GEngine->GameViewport->Viewport, FInputDeviceId::CreateFromInternalId(0), Key, Event, FPlatformTime::Cycles64()));
    }
    void SendAxis(FKey Key, float Delta)
    {
        GEngine->GameViewport->InputKey(FInputKeyEventArgs(GEngine->GameViewport->Viewport, FInputDeviceId::CreateFromInternalId(0), Key, Delta, 1.0f / 30.0f, 1, FPlatformTime::Cycles64()));
    }
    void ReleaseKeys()
    {
        for (FKey Key : {EKeys::W, EKeys::R, JumpKey, GrabKey}) SendKey(Key, IE_Released);
    }
    FAutomationTestBase* Test;
    double Started;
    double SettledAt = 0.0;
    double StageAt = 0.0;
    int32 Stage = 0;
    bool bTravelRequested = false;
    FString Destination;
    TWeakObjectPtr<ACh4_multiGameLobbyPlayerController> Controller;
    TWeakObjectPtr<ACh4_PlayerCharacter> Pawn;
    TWeakObjectPtr<UUserWidget> HUD;
    FVector StartLocation;
    FRotator StartRotation;
    FKey JumpKey;
    FKey GrabKey;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4LobbyInitializationRuntime,
    "Ch4_multiGame.Lobby.RuntimeInitialization",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4LobbyInitializationRuntime::RunTest(const FString&)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("Ch4LobbyInitSmoke")))
    {
        AddInfo(TEXT("Requires -Ch4LobbyInitSmoke and a passive second player; no input or travel requested."));
        return true;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FCh4LobbyInitializationCommand(this));
    return true;
}

#endif

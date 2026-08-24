// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#include "SimpleVoiceChatLog.h"

#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Slate/SceneViewport.h"
#include "SimpleVoiceChatSettings.h"
#include "SimpleVoiceChatSubsystem.h"

DEFINE_LOG_CATEGORY(LogSimpleVoiceChat);

namespace SimpleVoiceChat
{
class FVoiceToggleInputProcessor final : public IInputProcessor
{
public:
    virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
    {
        // Input is handled only by key events; no per-frame voice work is performed here.
    }

    virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
    {
        const USimpleVoiceChatSettings* Settings = GetDefault<USimpleVoiceChatSettings>();
        if (KeyEvent.IsRepeat()
            || !Settings->bEnableVoicePlugin
            || !Settings->bEnableAutomaticToggleKey
            || KeyEvent.GetKey() != Settings->ToggleKey
            || !GEngine)
        {
            return false;
        }

        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            UWorld* World = WorldContext.World();
            UGameViewportClient* ViewportClient = WorldContext.GameViewport;
            if (!World
                || !ViewportClient
                || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
            {
                continue;
            }

            FSceneViewport* SceneViewport = ViewportClient->GetGameViewport();
            const bool bConsoleOpen = ViewportClient->ViewportConsole
                && ViewportClient->ViewportConsole->ConsoleState != NAME_None;
            if (!SceneViewport || !SceneViewport->HasFocus() || bConsoleOpen)
            {
                continue;
            }

            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                if (USimpleVoiceChatSubsystem* VoiceSubsystem = GameInstance->GetSubsystem<USimpleVoiceChatSubsystem>())
                {
                    VoiceSubsystem->ToggleMicrophone();
                    break;
                }
            }
        }

        // Never consume the key globally; existing game input remains intact.
        return false;
    }
};
}

class FSimpleVoiceChatModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (FSlateApplication::IsInitialized())
        {
            RegisterInputProcessor();
        }
        else
        {
            PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
                this,
                &FSimpleVoiceChatModule::RegisterInputProcessor);
        }
    }

    virtual void ShutdownModule() override
    {
        if (PostEngineInitHandle.IsValid())
        {
            FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
            PostEngineInitHandle.Reset();
        }

        if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
        }
        InputProcessor.Reset();
    }

private:
    void RegisterInputProcessor()
    {
        if (PostEngineInitHandle.IsValid())
        {
            FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
            PostEngineInitHandle.Reset();
        }

        if (!InputProcessor.IsValid() && FSlateApplication::IsInitialized())
        {
            InputProcessor = MakeShared<SimpleVoiceChat::FVoiceToggleInputProcessor>();
            FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
        }
    }

    TSharedPtr<SimpleVoiceChat::FVoiceToggleInputProcessor> InputProcessor;
    FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_MODULE(FSimpleVoiceChatModule, SimpleVoiceChat)

// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Provider boundary used by SimpleVoiceChat. Implementations adapt a concrete voice service
 * without leaking it into the subsystem's Blueprint/C++ facade.
 */
class ISimpleVoiceBackend
{
public:
    virtual ~ISimpleVoiceBackend() = default;

    virtual bool Initialize(UWorld* World) = 0;
    virtual void Shutdown() = 0;
    virtual void HandleWorldChanged(UWorld* NewWorld) = 0;
    virtual void RefreshTalkers(UWorld* World) = 0;
    virtual bool SetMicrophoneEnabled(bool bEnable) = 0;
    virtual bool IsAvailable() const = 0;
    virtual bool IsMicrophoneEnabled() const = 0;
    virtual bool IsLocalPlayerTalking() const = 0;
};

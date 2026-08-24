// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#pragma once

#include "Net/VoiceConfig.h"
#include "SimpleVoiceChatTalkerComponent.generated.h"

/** Internal remote-talker component that applies the plugin's playback-only volume setting. */
UCLASS(Transient)
class USimpleVoiceChatTalkerComponent final : public UVOIPTalker
{
    GENERATED_BODY()

public:
    void SetPlaybackVolumeMultiplier(float InVolumeMultiplier);
    virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;

private:
    float PlaybackVolumeMultiplier = 1.0f;
};

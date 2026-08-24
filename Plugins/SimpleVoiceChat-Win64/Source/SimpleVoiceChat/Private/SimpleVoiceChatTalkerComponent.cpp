// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#include "SimpleVoiceChatTalkerComponent.h"

#include "Components/AudioComponent.h"

void USimpleVoiceChatTalkerComponent::SetPlaybackVolumeMultiplier(const float InVolumeMultiplier)
{
    PlaybackVolumeMultiplier = FMath::Clamp(InVolumeMultiplier, 0.0f, 4.0f);
}

void USimpleVoiceChatTalkerComponent::OnTalkingBegin(UAudioComponent* AudioComponent)
{
    if (AudioComponent)
    {
        AudioComponent->SetVolumeMultiplier(PlaybackVolumeMultiplier);
    }

    Super::OnTalkingBegin(AudioComponent);
}

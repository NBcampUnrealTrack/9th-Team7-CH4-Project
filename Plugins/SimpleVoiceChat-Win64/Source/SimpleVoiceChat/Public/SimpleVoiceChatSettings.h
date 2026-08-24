// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "SimpleVoiceChatSettings.generated.h"

/** Project Settings > Plugins > Simple Voice Chat. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Simple Voice Chat"))
class SIMPLEVOICECHAT_API USimpleVoiceChatSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    USimpleVoiceChatSettings();

    virtual FName GetContainerName() const override;
    virtual FName GetCategoryName() const override;
    virtual FName GetSectionName() const override;

    /** Master switch for subsystem initialization and automatic input handling. */
    UPROPERTY(Config, EditAnywhere, Category="Voice")
    bool bEnableVoicePlugin = true;

    /** Toggles the microphone from the configured key while the game viewport has focus. */
    UPROPERTY(Config, EditAnywhere, Category="Input")
    bool bEnableAutomaticToggleKey = true;

    /** Default: T. The input preprocessor never consumes this key. */
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(EditCondition="bEnableAutomaticToggleKey"))
    FKey ToggleKey;

    /** Privacy-safe default is off. */
    UPROPERTY(Config, EditAnywhere, Category="Voice")
    bool bMicrophoneEnabledOnStart = false;

    /** Opus target bitrate in bits per second. 32000 is a clear voice-oriented default for small groups. */
    UPROPERTY(Config, EditAnywhere, Category="Quality", meta=(ClampMin="6000", ClampMax="64000", UIMin="6000", UIMax="64000"))
    int32 EncoderBitrate = 32000;

    /** Opus encoder effort. Higher values can improve quality but use more CPU. */
    UPROPERTY(Config, EditAnywhere, Category="Quality", meta=(ClampMin="0", ClampMax="10", UIMin="0", UIMax="10"))
    int32 EncoderComplexity = 5;

    /** Keeps Opus variable-bitrate encoding enabled for better quality per transmitted byte. */
    UPROPERTY(Config, EditAnywhere, Category="Quality")
    bool bUseVariableBitrate = true;

    /** Volume applied only when a remote voice is played. This does not amplify encoded microphone samples. */
    UPROPERTY(Config, EditAnywhere, Category="Quality", meta=(ClampMin="0.0", ClampMax="4.0", UIMin="0.0", UIMax="2.0"))
    float PlaybackVolumeMultiplier = 1.5f;

    /** Input gain applied before encoding. Leave at 1.0 unless the Windows microphone level is still too low. */
    UPROPERTY(Config, EditAnywhere, Category="Quality", meta=(ClampMin="0.0", ClampMax="4.0", UIMin="0.0", UIMax="2.0"))
    float MicrophoneInputGain = 1.0f;

    /** Buffered receive audio in seconds. Higher values tolerate jitter at the cost of more delay. */
    UPROPERTY(Config, EditAnywhere, Category="Quality", meta=(ClampMin="0.05", ClampMax="1.0", UIMin="0.05", UIMax="0.5"))
    float JitterBufferDelaySeconds = 0.3f;

    /**
     * Creates a temporary local identity when using OnlineSubsystem NULL for direct-IP testing.
     * Other OnlineSubsystem providers are never logged in by this option.
     */
    UPROPERTY(Config, EditAnywhere, Category="Testing")
    bool bAutoLoginNullSubsystemForDirectIp = true;

    /**
     * Creates a private, non-advertised NULL session so Unreal's legacy voice interface can
     * register remote talkers during direct-IP tests. Existing game sessions are reused.
     */
    UPROPERTY(Config, EditAnywhere, Category="Testing")
    bool bAutoCreateNullVoiceSessionForDirectIp = true;
};

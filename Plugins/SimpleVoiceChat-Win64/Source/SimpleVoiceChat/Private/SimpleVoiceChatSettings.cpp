// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#include "SimpleVoiceChatSettings.h"

USimpleVoiceChatSettings::USimpleVoiceChatSettings()
    : ToggleKey(EKeys::T)
{
}

FName USimpleVoiceChatSettings::GetContainerName() const
{
    return TEXT("Project");
}

FName USimpleVoiceChatSettings::GetCategoryName() const
{
    return TEXT("Plugins");
}

FName USimpleVoiceChatSettings::GetSectionName() const
{
    return TEXT("Simple Voice Chat");
}

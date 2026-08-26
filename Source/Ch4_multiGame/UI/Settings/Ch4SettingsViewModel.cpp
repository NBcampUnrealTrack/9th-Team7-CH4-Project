#include "UI/Settings/Ch4SettingsViewModel.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/ConstructorHelpers.h"

// 프레임 제한 목록 (0 = 무제한)
const int32 UCh4SettingsViewModel::FrameRateTable[] = { 30, 60, 120, 0 };

UCh4SettingsViewModel::UCh4SettingsViewModel()
{
	// 0. Sound Class 및 Sound Mix 기본 에셋 자동 로드
	static ConstructorHelpers::FObjectFinder<USoundClass> MasterClassFinder(TEXT("/Game/Sound/SM_Master.SM_Master"));
	if (MasterClassFinder.Succeeded())
	{
		SoundClass_Master = MasterClassFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundClass> BGMClassFinder(TEXT("/Game/Sound/SM_BGM.SM_BGM"));
	if (BGMClassFinder.Succeeded())
	{
		SoundClass_BGM = BGMClassFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundClass> SFXClassFinder(TEXT("/Game/Sound/SM_SFX.SM_SFX"));
	if (SFXClassFinder.Succeeded())
	{
		SoundClass_SFX = SFXClassFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundMix> GameMixFinder(TEXT("/Game/Sound/SM_GameMix.SM_GameMix"));
	if (GameMixFinder.Succeeded())
	{
		SoundMix_Game = GameMixFinder.Object;
	}

	// 1. 현재 모니터가 지원하는 해상도 목록 자동 감지
	SupportedResolutions.Empty();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(SupportedResolutions);
	
	// 2. 저장된 설정 불러오기
	LoadSettings();
}

// ─────────────────────────────────────────────────────────────────
// 저장 / 불러오기
// ─────────────────────────────────────────────────────────────────

void UCh4SettingsViewModel::LoadSettings()
{
	// 1. 게임 전용 설정값 불러오기 (Game.ini)
	GConfig->GetFloat(ConfigSection, TEXT("FOV"), FOV, GGameIni);
	GConfig->GetFloat(ConfigSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameIni);
	GConfig->GetBool(ConfigSection, TEXT("InvertY"), bInvertY, GGameIni);
	GConfig->GetFloat(ConfigSection, TEXT("MasterVolume"), MasterVolume, GGameIni);
	GConfig->GetFloat(ConfigSection, TEXT("BGMVolume"), BGMVolume, GGameIni);
	GConfig->GetFloat(ConfigSection, TEXT("SFXVolume"), SFXVolume, GGameIni);
	GConfig->GetInt(ConfigSection, TEXT("VoiceMode"), VoiceMode, GGameIni);
	GConfig->GetFloat(ConfigSection, TEXT("MicSensitivity"), MicSensitivity, GGameIni);
	
	// 2. 그래픽 설정값 불러오기 (GameUserSettings.ini)
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		bVSync = GUS->IsVSyncEnabled();
		QualityPreset = FMath::Clamp(GUS->GetOverallScalabilityLevel(), 0, 3);
		
		// 창 모드 역산 (0:전체화면, 1:창모드, 2:전체화면 창모드)
		switch (GUS->GetFullscreenMode())
		{
		case EWindowMode::Fullscreen: WindowModeIndex = 0; break;
		case EWindowMode::Windowed: WindowModeIndex = 1; break;
		case EWindowMode::WindowedFullscreen:
		default:
			WindowModeIndex = 2;
			break;
		}
		
		// 해상도 인덱스 역산 (현재 해상도가 SupportedResolutions 중 몇 번째인지)
		FIntPoint CurrentRes = GUS->GetScreenResolution();
		ResolutionIndex = 0;
		for (int32 i = 0; i < SupportedResolutions.Num(); ++i)
		{
			if (SupportedResolutions[i] == CurrentRes)
			{
				ResolutionIndex = i;
				break;
			}
		}
		
		// 프레임 제한 인덱스 역산
		int32 CurrentFPS = FMath::RoundToInt(GUS->GetFrameRateLimit());
		FrameRateIndex = 3;	// 기본값 무제한
		for (int32 i = 0; i < 3; ++i)
		{
			if (CurrentFPS == FrameRateTable[i])
			{
				FrameRateIndex = i;
				break;
			}
		}
	}
	
	// 3. 불러온 설정값 즉시 적용
	ApplyFOV(FOV);
	ApplySoundVolume(SoundClass_Master, MasterVolume / 100.0f);
	ApplySoundVolume(SoundClass_BGM, BGMVolume / 100.0f);
	ApplySoundVolume(SoundClass_SFX, SFXVolume / 100.0f);
}

void UCh4SettingsViewModel::SaveSettings()
{
	// 1. 게임 전용 설정값 저장 (Game.ini)
	GConfig->SetFloat(ConfigSection, TEXT("FOV"), FOV, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameIni);
	GConfig->SetBool(ConfigSection, TEXT("InvertY"), bInvertY, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MasterVolume"), MasterVolume, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("BGMVolume"), BGMVolume, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("SFXVolume"), SFXVolume, GGameIni);
	GConfig->SetInt(ConfigSection, TEXT("VoiceMode"), VoiceMode, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MicSensitivity"), MicSensitivity, GGameIni);
	GConfig->Flush(false, GGameIni);
	
	// 2. 그래픽 설정 저장 (GameUserSettings.ini)
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->ApplySettings(false);
	}
}

// ─────────────────────────────────────────────────────────────────
// 시스템 (System)
// ─────────────────────────────────────────────────────────────────

void UCh4SettingsViewModel::SetFOV(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 60.0f, 120.0f);
	if (!FMath::IsNearlyEqual(FOV, Clamped))
	{
		FOV = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOV);
		ApplyFOV(FOV);
		SaveSettings();
	}
}

void UCh4SettingsViewModel::ApplyFOV(float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* Cam = PC->PlayerCameraManager)
			{
				Cam->SetFOV(Value);
			}
		}
	}
}

void UCh4SettingsViewModel::SetMouseSensitivity(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.1f, 3.0f);
	if (!FMath::IsNearlyEqual(MouseSensitivity, Clamped))
	{
		MouseSensitivity = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MouseSensitivity);
		SaveSettings();
	}
}

void UCh4SettingsViewModel::SetInvertY(bool NewValue)
{
	if (bInvertY != NewValue)
	{
		bInvertY = NewValue;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bInvertY);
		SaveSettings();
	}
}

// ─────────────────────────────────────────────────────────────────
// 그래픽 (Graphics)
// ─────────────────────────────────────────────────────────────────

void UCh4SettingsViewModel::SetResolutionIndex(int32 NewIndex)
{
	if (SupportedResolutions.IsValidIndex(NewIndex) && ResolutionIndex != NewIndex)
	{
		ResolutionIndex = NewIndex;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ResolutionIndex);
		ApplyResolution(ResolutionIndex);
	}
}

void UCh4SettingsViewModel::ApplyResolution(int32 Index)
{
	if (SupportedResolutions.IsValidIndex(Index))
	{
		if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			GUS->SetScreenResolution(SupportedResolutions[Index]);
			GUS->ApplyResolutionSettings(false);
		}
	}
}

void UCh4SettingsViewModel::SetWindowModeIndex(int32 NewIndex)
{
	const int32 Clamped = FMath::Clamp(NewIndex, 0, 2);
	if (WindowModeIndex != Clamped)
	{
		WindowModeIndex = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(WindowModeIndex);
		ApplyWindowMode(WindowModeIndex);
	}
}

void UCh4SettingsViewModel::ApplyWindowMode(int32 Index)
{
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		EWindowMode::Type Mode = EWindowMode::WindowedFullscreen;
		if (Index == 0) Mode = EWindowMode::Fullscreen;
		else if (Index == 1) Mode = EWindowMode::Windowed;
		else if (Index == 2) Mode = EWindowMode::WindowedFullscreen;

		GUS->SetFullscreenMode(Mode);
		GUS->ApplySettings(false);
	}
}

void UCh4SettingsViewModel::SetQualityPreset(int32 NewPreset)
{
	const int32 Clamped = FMath::Clamp(NewPreset, 0, 3);
	if (QualityPreset != Clamped)
	{
		QualityPreset = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(QualityPreset);
		ApplyQuality(QualityPreset);
	}
}

void UCh4SettingsViewModel::ApplyQuality(int32 Preset)
{
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->SetOverallScalabilityLevel(Preset);
		GUS->ApplySettings(false);
	}
}

void UCh4SettingsViewModel::SetVSync(bool NewValue)
{
	if (bVSync != NewValue)
	{
		bVSync = NewValue;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bVSync);
		ApplyVSync(bVSync);
	}
}

void UCh4SettingsViewModel::ApplyVSync(bool bEnabled)
{
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->SetVSyncEnabled(bEnabled);
		GUS->ApplySettings(false);
	}
}

void UCh4SettingsViewModel::SetFrameRateIndex(int32 NewIndex)
{
	const int32 Clamped = FMath::Clamp(NewIndex, 0, 3);
	if (FrameRateIndex != Clamped)
	{
		FrameRateIndex = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FrameRateIndex);
		ApplyFrameRate(FrameRateIndex);
	}
}

void UCh4SettingsViewModel::ApplyFrameRate(int32 Index)
{
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->SetFrameRateLimit(static_cast<float>(FrameRateTable[Index]));
		GUS->ApplySettings(false);
	}
}

// ─────────────────────────────────────────────────────────────────
// 사운드 (Sound)
// ─────────────────────────────────────────────────────────────────

void UCh4SettingsViewModel::ApplySoundVolume(USoundClass* SoundClass, float NormalizedVolume)
{
	if (!SoundClass || !SoundMix_Game) return;

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetSoundMixClassOverride(
			World, SoundMix_Game, SoundClass,
			NormalizedVolume, 1.0f, 0.0f, true);
		UGameplayStatics::PushSoundMixModifier(World, SoundMix_Game);
	}
}

void UCh4SettingsViewModel::SetMasterVolume(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(MasterVolume, Clamped))
	{
		MasterVolume = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MasterVolume);
		ApplySoundVolume(SoundClass_Master, MasterVolume / 100.0f);
		SaveSettings();
	}
}

void UCh4SettingsViewModel::SetBGMVolume(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(BGMVolume, Clamped))
	{
		BGMVolume = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BGMVolume);
		ApplySoundVolume(SoundClass_BGM, BGMVolume / 100.0f);
		SaveSettings();
	}
}

void UCh4SettingsViewModel::SetSFXVolume(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(SFXVolume, Clamped))
	{
		SFXVolume = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SFXVolume);
		ApplySoundVolume(SoundClass_SFX, SFXVolume / 100.0f);
		SaveSettings();
	}
}

// ─────────────────────────────────────────────────────────────────
// 음성 (Voice)
// ─────────────────────────────────────────────────────────────────

void UCh4SettingsViewModel::SetVoiceMode(int32 NewMode)
{
	const int32 Clamped = FMath::Clamp(NewMode, 0, 3);
	if (VoiceMode != Clamped)
	{
		VoiceMode = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(VoiceMode);
		SaveSettings();
	}
}

void UCh4SettingsViewModel::SetMicSensitivity(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(MicSensitivity, Clamped))
	{
		MicSensitivity = Clamped;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicSensitivity);
		SaveSettings();
	}
}

#include "UI/Settings/Ch4SettingsViewModel.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
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
	
	// 해상도 문자열 목록 채우기 (예: "1920 x 1080")
	ResolutionStringOptions.Empty();
	for (const FIntPoint& Res : SupportedResolutions)
	{
		ResolutionStringOptions.Add(FString::Printf(TEXT("%d x %d"), Res.X, Res.Y));
	}
	
	// 2. 저장된 설정 불러오기
	LoadSettings();
}

// ─────────────────────────────────────────────────────────────────
// 저장 / 불러오기
// ─────────────────────────────────────────────────────────────────

void UCh4SettingsViewModel::LoadSettings()
{
	if (bIsInternalUpdating) return;
	bIsInternalUpdating = true;

	// 1. 게임 전용 설정값 불러오기 (Game.ini)
	GConfig->GetInt(ConfigSection, TEXT("FOV"), FOV, GGameIni);
	FOV = FMath::Clamp(FOV, 80, 100);
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
	
	// 3. MVVM View Binding용 모든 속성 일괄 브로드캐스트
	UpdateFOVRepresentations(FOV);
	UpdateMouseSensRepresentations(MouseSensitivity);
	UpdateInvertYRepresentations(bInvertY);
	UpdateVSyncRepresentations(bVSync);
	UpdateMasterVolumeRepresentations(MasterVolume);
	UpdateBGMVolumeRepresentations(BGMVolume);
	UpdateSFXVolumeRepresentations(SFXVolume);
	UpdateMicSensRepresentations(MicSensitivity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ResolutionIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(WindowModeIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(QualityPreset);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FrameRateIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(VoiceMode);

	// 4. 불러온 설정값 즉시 적용
	ApplyFOV(static_cast<float>(FOV));
	ApplySoundVolume(SoundClass_Master, MasterVolume / 100.0f);
	ApplySoundVolume(SoundClass_BGM, BGMVolume / 100.0f);
	ApplySoundVolume(SoundClass_SFX, SFXVolume / 100.0f);

	bIsInternalUpdating = false;
}

void UCh4SettingsViewModel::SaveSettings()
{
	// 메모리 세팅 즉시 동기화
	GConfig->SetInt(ConfigSection, TEXT("FOV"), FOV, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameIni);
	GConfig->SetBool(ConfigSection, TEXT("InvertY"), bInvertY, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MasterVolume"), MasterVolume, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("BGMVolume"), BGMVolume, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("SFXVolume"), SFXVolume, GGameIni);
	GConfig->SetInt(ConfigSection, TEXT("VoiceMode"), VoiceMode, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MicSensitivity"), MicSensitivity, GGameIni);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SaveTimerHandle);
	}
	FlushSaveToDisk();
}

void UCh4SettingsViewModel::RequestDebouncedSave()
{
	// 1. 메모리(GConfig)에는 0ms 즉시 기록! (인게임 플레이어 컨트롤러는 즉각 새 감도로 반응)
	GConfig->SetInt(ConfigSection, TEXT("FOV"), FOV, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameIni);
	GConfig->SetBool(ConfigSection, TEXT("InvertY"), bInvertY, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MasterVolume"), MasterVolume, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("BGMVolume"), BGMVolume, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("SFXVolume"), SFXVolume, GGameIni);
	GConfig->SetInt(ConfigSection, TEXT("VoiceMode"), VoiceMode, GGameIni);
	GConfig->SetFloat(ConfigSection, TEXT("MicSensitivity"), MicSensitivity, GGameIni);

	// 2. 무거운 하드디스크 I/O Flush는 조작이 멈추고 0.3초 뒤 1회만 지연 실행 (슬라이더 렉 완전 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SaveTimerHandle);
		World->GetTimerManager().SetTimer(
			SaveTimerHandle, this, &UCh4SettingsViewModel::FlushSaveToDisk, 0.3f, false);
	}
	else
	{
		FlushSaveToDisk();
	}
}

void UCh4SettingsViewModel::FlushSaveToDisk()
{
	// 1. 게임 ini 디스크 파일 쓰기
	GConfig->Flush(false, GGameIni);
	
	// 2. 그래픽 설정 ini 파일 쓰기
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->ApplySettings(false);
	}
}

// ─────────────────────────────────────────────────────────────────
// 시스템 (System)
// ─────────────────────────────────────────────────────────────────

void UCh4SettingsViewModel::UpdateFOVRepresentations(int32 NewFOV)
{
	FOV = NewFOV;
	FOVSliderValue = static_cast<float>(FOV);
	FOVText = FText::AsNumber(FOV);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOV);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOVSliderValue);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOVText);
}

void UCh4SettingsViewModel::SetFOV(int32 NewValue)
{
	if (bIsInternalUpdating) return;
	
	const int32 Clamped = FMath::Clamp(NewValue, 80, 100);
	if (FOV != Clamped)
	{
		bIsInternalUpdating = true;
		UpdateFOVRepresentations(Clamped);
		bIsInternalUpdating = false;
		
		ApplyFOV(static_cast<float>(FOV));
		SaveSettings();
	}
}

void UCh4SettingsViewModel::SetFOVSliderValue(float NewValue)
{
	if (bIsInternalUpdating) return;

	const float Clamped = FMath::Clamp(NewValue, 80.0f, 100.0f);
	FOVSliderValue = Clamped;

	const int32 NewFOV = FMath::RoundToInt(Clamped);
	if (FOV != NewFOV)
	{
		FOV = NewFOV;
		FOVText = FText::AsNumber(FOV);

		// FOVSliderValue는 슬라이더가 이미 알고 있는 자기 값이므로 슬라이더에게 역으로 재방송하지 않음! (에코 무한루프 차단!)
		bIsInternalUpdating = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOV);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOVText);
		bIsInternalUpdating = false;

		ApplyFOV(static_cast<float>(FOV));
		RequestDebouncedSave();
	}
}

void UCh4SettingsViewModel::SetFOVText(const FText& NewText)
{
	if (bIsInternalUpdating) return;

	// 1. 공백 제거 및 순수 숫자 여부 검증
	const FString Str = NewText.ToString().TrimStartAndEnd();
	int32 Parsed = FOV;
	if (Str.IsNumeric())
	{
		Parsed = FCString::Atoi(*Str);
	}

	// 2. 80 ~ 100 범위로 강제 클램프
	const int32 Clamped = FMath::Clamp(Parsed, 80, 100);

	bIsInternalUpdating = true;
	// 사용자가 104나 오타를 입력했을 때 무조건 올바른 텍스트("100")로 강제 덮어쓰기 위해 항상 갱신 및 방송!
	FOV = Clamped;
	FOVSliderValue = static_cast<float>(FOV);
	FOVText = FText::AsNumber(FOV);

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOV);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOVSliderValue);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FOVText);
	bIsInternalUpdating = false;

	ApplyFOV(static_cast<float>(FOV));
	SaveSettings();
}

void UCh4SettingsViewModel::ApplyFOV(float Value)
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			// 1. CameraManager FOV 및 DefaultFOV 설정
			if (APlayerCameraManager* Cam = PC->PlayerCameraManager)
			{
				Cam->SetFOV(Value);
				Cam->DefaultFOV = Value;
			}

			// 2. 현재 조종 중인 캐릭터 폰의 CameraComponent FOV 직접 변경 (ThirdPerson 덮어쓰기 방지)
			if (APawn* Pawn = PC->GetPawn())
			{
				if (UCameraComponent* CamComp = Pawn->FindComponentByClass<UCameraComponent>())
				{
					CamComp->SetFieldOfView(Value);
				}
			}
		}
	}
}

void UCh4SettingsViewModel::UpdateMouseSensRepresentations(float NewSens)
{
	MouseSensitivity = NewSens;
	MouseSensText = FText::FromString(FString::Printf(TEXT("%.1f"), MouseSensitivity));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MouseSensitivity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MouseSensText);
}

void UCh4SettingsViewModel::SetMouseSensitivity(float NewValue)
{
	if (bIsInternalUpdating) return;

	const float Clamped = FMath::Clamp(NewValue, 0.1f, 3.0f);
	if (!FMath::IsNearlyEqual(MouseSensitivity, Clamped))
	{
		MouseSensitivity = Clamped;
		MouseSensText = FText::FromString(FString::Printf(TEXT("%.1f"), MouseSensitivity));

		bIsInternalUpdating = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MouseSensText);
		bIsInternalUpdating = false;

		RequestDebouncedSave();
	}
}

void UCh4SettingsViewModel::SetMouseSensText(const FText& NewText)
{
	if (bIsInternalUpdating) return;

	// 1. 공백 제거
	const FString Str = NewText.ToString().TrimStartAndEnd();
	float Parsed = MouseSensitivity;

	// 2. 소수점 및 숫자 외계어("3-.0" 등) 검증 및 정제
	if (Str.IsNumeric() || (Str.Len() > 0 && Str.Contains(TEXT(".")) && Str.Replace(TEXT("."), TEXT("")).IsNumeric()))
	{
		Parsed = FCString::Atof(*Str);
	}

	// 3. 0.1 ~ 3.0 범위 강제 클램프
	const float Clamped = FMath::Clamp(Parsed, 0.1f, 3.0f);

	bIsInternalUpdating = true;
	// 오타나 초과값을 무조건 깔끔한 소수점 한 자리("3.0", "1.5")로 강제 덮어쓰기!
	MouseSensitivity = Clamped;
	MouseSensText = FText::FromString(FString::Printf(TEXT("%.1f"), MouseSensitivity));

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MouseSensitivity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MouseSensText);
	bIsInternalUpdating = false;

	SaveSettings();
}

void UCh4SettingsViewModel::UpdateInvertYRepresentations(bool bNewInvertY)
{
	bInvertY = bNewInvertY;
	InvertYState = bInvertY ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bInvertY);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InvertYState);
}

void UCh4SettingsViewModel::SetInvertY(bool NewValue)
{
	if (bIsInternalUpdating) return;
	
	if (bInvertY != NewValue)
	{
		bInvertY = NewValue;
		InvertYState = bInvertY ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

		bIsInternalUpdating = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bInvertY);
		// InvertYState는 체크박스 자기 자신이 바꾼 값이므로 체크박스에 역방송하지 않음! (에코 차단)
		bIsInternalUpdating = false;
		
		SaveSettings();
	}
}

void UCh4SettingsViewModel::SetInvertYState(ECheckBoxState NewState)
{
	SetInvertY(NewState == ECheckBoxState::Checked);
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

void UCh4SettingsViewModel::UpdateVSyncRepresentations(bool bNewVSync)
{
	bVSync = bNewVSync;
	VSyncState = bVSync ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bVSync);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(VSyncState);
}

void UCh4SettingsViewModel::SetVSync(bool NewValue)
{
	if (bIsInternalUpdating) return;
	
	if (bVSync != NewValue)
	{
		bVSync = NewValue;
		VSyncState = bVSync ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

		bIsInternalUpdating = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bVSync);
		// VSyncState 역방송 차단!
		bIsInternalUpdating = false;
		
		ApplyVSync(bVSync);
		SaveSettings();
	}
}

void UCh4SettingsViewModel::SetVSyncState(ECheckBoxState NewState)
{
	SetVSync(NewState == ECheckBoxState::Checked);
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

void UCh4SettingsViewModel::UpdateMasterVolumeRepresentations(float NewVolume)
{
	MasterVolume = NewVolume;
	MasterVolumeText = FText::AsNumber(FMath::RoundToInt(MasterVolume));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MasterVolume);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MasterVolumeText);
}

void UCh4SettingsViewModel::SetMasterVolume(float NewValue)
{
	if (bIsInternalUpdating) return;

	const float Clamped = FMath::Clamp(NewValue, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(MasterVolume, Clamped))
	{
		MasterVolume = Clamped;
		MasterVolumeText = FText::AsNumber(FMath::RoundToInt(MasterVolume));

		bIsInternalUpdating = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MasterVolumeText);
		bIsInternalUpdating = false;

		ApplySoundVolume(SoundClass_Master, MasterVolume / 100.0f);
		RequestDebouncedSave();
	}
}

void UCh4SettingsViewModel::UpdateBGMVolumeRepresentations(float NewVolume)
{
	BGMVolume = NewVolume;
	BGMVolumeText = FText::AsNumber(FMath::RoundToInt(BGMVolume));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BGMVolume);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BGMVolumeText);
}

void UCh4SettingsViewModel::SetBGMVolume(float NewValue)
{
	if (bIsInternalUpdating) return;

	const float Clamped = FMath::Clamp(NewValue, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(BGMVolume, Clamped))
	{
		BGMVolume = Clamped;
		BGMVolumeText = FText::AsNumber(FMath::RoundToInt(BGMVolume));

		bIsInternalUpdating = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BGMVolumeText);
		bIsInternalUpdating = false;

		ApplySoundVolume(SoundClass_BGM, BGMVolume / 100.0f);
		RequestDebouncedSave();
	}
}

void UCh4SettingsViewModel::UpdateSFXVolumeRepresentations(float NewVolume)
{
	SFXVolume = NewVolume;
	SFXVolumeText = FText::AsNumber(FMath::RoundToInt(SFXVolume));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SFXVolume);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SFXVolumeText);
}

void UCh4SettingsViewModel::SetSFXVolume(float NewValue)
{
	if (bIsInternalUpdating) return;

	const float Clamped = FMath::Clamp(NewValue, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(SFXVolume, Clamped))
	{
		SFXVolume = Clamped;
		SFXVolumeText = FText::AsNumber(FMath::RoundToInt(SFXVolume));

		bIsInternalUpdating = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SFXVolumeText);
		bIsInternalUpdating = false;

		ApplySoundVolume(SoundClass_SFX, SFXVolume / 100.0f);
		RequestDebouncedSave();
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

void UCh4SettingsViewModel::UpdateMicSensRepresentations(float NewSens)
{
	MicSensitivity = NewSens;
	MicSensText = FText::AsNumber(FMath::RoundToInt(MicSensitivity));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicSensitivity);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicSensText);
}

void UCh4SettingsViewModel::SetMicSensitivity(float NewValue)
{
	if (bIsInternalUpdating) return;

	const float Clamped = FMath::Clamp(NewValue, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(MicSensitivity, Clamped))
	{
		MicSensitivity = Clamped;
		MicSensText = FText::AsNumber(FMath::RoundToInt(MicSensitivity));

		bIsInternalUpdating = true;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicSensText);
		bIsInternalUpdating = false;

		RequestDebouncedSave();
	}
}

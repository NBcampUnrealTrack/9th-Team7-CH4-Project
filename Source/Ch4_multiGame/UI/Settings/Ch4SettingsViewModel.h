#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Styling/SlateTypes.h"
#include "Ch4SettingsViewModel.generated.h"

UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4SettingsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	UCh4SettingsViewModel();

	// ─────────────────────────────────────────────────────────────────
	// 시스템 (System)
	// ─────────────────────────────────────────────────────────────────
	
	// 시야각 정수 (내부 저장값: 80~100, 기본값: 90)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|System")
	int32 FOV = 90;
	
	// FOV 슬라이더 바인딩용 (float)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetFOVSliderValue", Category = "Settings|System")
	float FOVSliderValue = 90.0f;
	
	// FOV 텍스트 상자 바인딩용 (FText)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetFOVText", Category = "Settings|System")
	FText FOVText = FText::FromString(TEXT("90"));
	
	// 마우스 감도 (0.1 ~ 3.0, 기본값: 1.0)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetMouseSensitivity", Category = "Settings|System")
	float MouseSensitivity = 1.0f;
	
	// 마우스 감도 텍스트 바인딩용 (FText)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetMouseSensText", Category = "Settings|System")
	FText MouseSensText = FText::FromString(TEXT("1.0"));
	
	// Y축 반전 여부 (내부 bool)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|System")
	bool bInvertY = false;
	
	// Y축 반전 체크박스 바인딩용 (ECheckBoxState)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetInvertYState", Category = "Settings|System")
	ECheckBoxState InvertYState = ECheckBoxState::Unchecked;
	
	// Setter 함수들
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetFOV(int32 NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetFOVSliderValue(float NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetFOVText(const FText& NewText);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetMouseSensitivity(float NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetMouseSensText(const FText& NewText);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetInvertY(bool NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetInvertYState(ECheckBoxState NewState);
	
	// ─────────────────────────────────────────────────────────────────
	// 그래픽 (Graphics)
	// ─────────────────────────────────────────────────────────────────
	
	// 선택된 해상도 인덱스
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	int32 ResolutionIndex = 0;
	
	// 모니터 지원 해상도 목록 (FIntPoint)
	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	TArray<FIntPoint> SupportedResolutions;

	// [추가] 콤보박스에 바로 채워 넣을 수 있는 해상도 문자열 목록 (예: "1920 x 1080")
	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	TArray<FString> ResolutionStringOptions;
	
	// 창 모드 (0: 전체화면, 1: 창모드, 2: 전체화면 창모드)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	int32 WindowModeIndex = 0;
	
	// 전체 품질 프리셋 (0: 낮음, 1: 중간, 2: 높음, 3: 최고)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	int32 QualityPreset = 2;
	
	// VSync 사용 여부 (내부 bool)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	bool bVSync = true;
	
	// VSync 체크박스 바인딩용 (ECheckBoxState)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetVSyncState", Category = "Settings|Graphics")
	ECheckBoxState VSyncState = ECheckBoxState::Checked;
	
	// 프레임 제한 인덱스 (0: 30, 1: 60, 2: 120, 3: 무제한)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	int32 FrameRateIndex = 1;
	
	// Setter 함수들
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetResolutionIndex(int32 NewIndex);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetWindowModeIndex(int32 NewIndex);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetQualityPreset(int32 NewPreset);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetVSync(bool NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetVSyncState(ECheckBoxState NewState);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void SetFrameRateIndex(int32 NewIndex);
	
	// ─────────────────────────────────────────────────────────────────
	// 사운드 (Sound)
	// ─────────────────────────────────────────────────────────────────
	
	// 마스터 볼륨 (0 ~ 100)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetMasterVolume", Category = "Settings|Sound")
	float MasterVolume = 100.0f;
	
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Sound")
	FText MasterVolumeText = FText::FromString(TEXT("100"));
	
	// BGM 볼륨 (0 ~ 100)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetBGMVolume", Category = "Settings|Sound")
	float BGMVolume = 100.0f;
	
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Sound")
	FText BGMVolumeText = FText::FromString(TEXT("100"));
	
	// 효과음 볼륨 (0 ~ 100)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetSFXVolume", Category = "Settings|Sound")
	float SFXVolume = 100.0f;
	
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Sound")
	FText SFXVolumeText = FText::FromString(TEXT("100"));
	
	// Sound Class/Mix 에셋
	UPROPERTY(EditAnywhere, Category = "Settings|Sound")
	TObjectPtr<class USoundClass> SoundClass_Master;
	
	UPROPERTY(EditAnywhere, Category = "Settings|Sound")
	TObjectPtr<class USoundClass> SoundClass_BGM;
	
	UPROPERTY(EditAnywhere, Category = "Settings|Sound")
	TObjectPtr<class USoundClass> SoundClass_SFX;
	
	UPROPERTY(EditAnywhere, Category = "Settings|Sound")
	TObjectPtr<class USoundMix> SoundMix_Game;
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
	void SetMasterVolume(float NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
	void SetBGMVolume(float NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
	void SetSFXVolume(float NewValue);
	
	// ─────────────────────────────────────────────────────────────────
	// 음성 (Voice)
	// ─────────────────────────────────────────────────────────────────
	
	// 음성 전송 방식 (0: 끄기, 1: 토글, 2: PTT, 3: 항상)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Voice")
	int32 VoiceMode = 3;
	
	// 마이크 입력 감도 (0 ~ 100)
	UPROPERTY(FieldNotify, BlueprintReadWrite, Setter = "SetMicSensitivity", Category = "Settings|Voice")
	float MicSensitivity = 20.0f;
	
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Voice")
	FText MicSensText = FText::FromString(TEXT("20"));
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceMode(int32 NewMode);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetMicSensitivity(float NewValue);
	
	// ─────────────────────────────────────────────────────────────────
	// 전체 저장 / 불러오기
	// ─────────────────────────────────────────────────────────────────
	
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void LoadSettings();
	
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SaveSettings();
	
private:
	void UpdateFOVRepresentations(int32 NewFOV);
	void UpdateMouseSensRepresentations(float NewSens);
	void UpdateInvertYRepresentations(bool bNewInvertY);
	void UpdateVSyncRepresentations(bool bNewVSync);
	void UpdateMasterVolumeRepresentations(float NewVolume);
	void UpdateBGMVolumeRepresentations(float NewVolume);
	void UpdateSFXVolumeRepresentations(float NewVolume);
	void UpdateMicSensRepresentations(float NewSens);

	void ApplyFOV(float Value);
	void ApplyResolution(int32 Index);
	void ApplyWindowMode(int32 Index);
	void ApplyQuality(int32 Preset);
	void ApplyVSync(bool bEnabled);
	void ApplyFrameRate(int32 Index);
	void ApplySoundVolume(USoundClass* SoundClass, float NormalizedVolume);
	
	static constexpr const TCHAR* ConfigSection = TEXT("Ch4_multiGame.Settings");
	static const int32 FrameRateTable[];
	
	// Two-Way 바인딩 무한 루프(에코) 방지 가드 플래그
	bool bIsInternalUpdating = false;
};
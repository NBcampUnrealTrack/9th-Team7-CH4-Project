#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Ch4SettingsViewModel.generated.h"

UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4SettingsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	UCh4SettingsViewModel();

	// -----
	// 시스템
	// -----
	
	// 시야각 (범위: 60~120, 기본값: 90)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|System")
	float FOV = 90.0f;
	
	// 마우스 감도 (범위: 0.1~3.0, 기본값: 1.0)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|System")
	float MouseSensitivity = 1.0f;
	
	// Y축 반전 여부
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|System")
	bool bInvertY = false;
	
	// Setter 함수들
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetFOV(float NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetMouseSensitivity(float NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|System")
	void SetInvertY(bool NewValue);
	
	// -----
	// 그래픽
	// -----
	
	// 선택된 해상도 인덱스 (SupportedResolutions 배열 기준)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	int32 ResolutionIndex = 0;
	
	// 현재 모니터가 지원하는 해상도 목록 (런타임에 자동으로 채워짐)
	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics")
	TArray<FIntPoint> SupportedResolutions;
	
	// 창 모드 (0 = 전체화면, 1 = 창모드, 2 = 테두리 없는...)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	int32 WindowModeIndex = 0;
	
	// 전체 품질 프리셋 (0 = 낮음, 1 = 중간, 2 = 높음, 3 = 최고)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	int32 QualityPreset = 2;
	
	// VSync 사용 여부 (수직 동기화)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Graphics")
	bool bVSync = true;
	
	// 프레임 제한 인덱스 (0 = 30fps, 1 = 60fps, 2 = 120fps, 3 = 무제한)
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
	void SetFrameRateIndex(int32 NewIndex);
	
	// -----
	// 사운드
	// -----
	
	// 마스터 볼륨 (0 ~ 100)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Sound")
	float MasterVolume = 100.0f;
	
	// BGM 볼륨 (0 ~ 100)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Sound")
	float BGMVolume = 100.0f;
	
	// 효과음 볼륨 (0 ~ 100)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Sound")
	float SFXVolume = 100.0f;
	
	// Sound Class/Mix 에셋 (에디터에서 지정)
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
	
	// ----
	// 음성
	// ----
	
	// 음성 전송 방식 (0 = 끄기, 1 = 토글, 2 = PTT, 3 = 항상)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Voice")
	int32 VoiceMode = 3;
	
	// 마이크 입력 감도 (0 ~ 100)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Settings|Voice")
	float MicSensitivity = 20.0f;
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceMode(int32 NewMode);
	
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetMicSensitivity(float NewValue);
	
	// -----------------
	// 전체 저장 / 불러오기
	// -----------------
	
	// 게임 시작 or Settings 열 때 -> 저장된 값 불러오기
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void LoadSettings();
	
	// 설정 변경할 때 마다 -> 파일에 저장
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SaveSettings();
	
private:
	// 내부 적용 함수들 (외부에서 직접 호출 불필요)
	void ApplyFOV(float Value);
	void ApplyResolution(int32 Index);
	void ApplyWindowMode(int32 Index);
	void ApplyQuality(int32 Preset);
	void ApplyVSync(bool bEnabled);
	void ApplyFrameRate(int32 Index);
	void ApplySoundVolume(USoundClass* SoundClass, float NormalizedVolume);
	
	static constexpr const TCHAR* ConfigSection = TEXT("Ch4_multiGame.Settings");
	static const int32 FrameRateTable[];
	
};
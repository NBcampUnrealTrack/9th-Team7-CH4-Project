// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Ch4_multiGamePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UInputAction;	// [추가]

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class ACh4_multiGamePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACh4_multiGamePlayerController();

	// 마우스 회전 입력 가로채기 (마우스 감도 및 Y축 반전 적용)
	virtual void AddPitchInput(float Val) override;
	virtual void AddYawInput(float Val) override;

	/**
	 * Safe development command for Hamachi direct-IP tests.
	 * Usage: JoinHamachi 25.x.x.x (an optional :7777 suffix is accepted).
	 */
	UFUNCTION(Exec, BlueprintCallable, Category="Network|Debug")
	void JoinHamachi(FString HostIPv4);
	
	// [추가] PauseMenu 관련 공개 함수들 선언
	// P 키(이후에 ESC키로 전환)를 눌렀을 때 열려있으면 닫고, 닫혀있으면 여는 토글 함수
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void TogglePauseMenu();
	
	// PauseMenu 열기
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void ShowPauseMenu();
	
	// PauseMenu 닫기 (Resume 버튼 클릭 시에도 호출)
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void HidePauseMenu();
	
	// 현재 PauseMenu가 켜져있는지 확인
	UFUNCTION(BlueprintPure, Category = "UI|Pause Menu")
	bool IsPauseMenuOpen() const;
	
	// 세션을 정리하고 메인 메뉴(L_MainMenu)로 돌아가기
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void ReturnToMainMenu();
	
	// 게임 완전히 종료하기
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void QuitGame();

protected:
	
	// [추가] 에디터에서 설정할 프로퍼티와 위젯 인스턴스 변수
	// 에디터에서 만든 IA_Pause를 넣어줄 변수
	UPROPERTY(EditAnywhere, Category = "Input|Pause Menu")
	TObjectPtr<UInputAction> PauseAction;
	
	// 에디터에서 디자인할 WBP_PauseMenu 위젯 클래스를 지정할 변수
	UPROPERTY(EditAnywhere, Category = "Input|Pause Menu")
	TSubclassOf<UUserWidget> PauseMenuWidgetClass;
	
	// 화면에 생성된 실제 PauseMenu 위젯의 주소를 기억할 포인터 변수
	UPROPERTY()
	TObjectPtr<UUserWidget> PauseMenuWidget;

	// PauseMenu 전용 MVVM ViewModel 인스턴스
	UPROPERTY(BlueprintReadOnly, Category = "UI|Pause Menu")
	TObjectPtr<class UCh4PauseMenuViewModel> PauseMenuViewModel;
	
	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};

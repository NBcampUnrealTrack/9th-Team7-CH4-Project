#include "Player/Ch4_PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Ch4_multiGame.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Cart/CartBase.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/Ch4_multiGameGameInstance.h"
#include "Player/Ch4_multiGamePlayerState.h"
#include "Player/EmotionDataAsset.h"
#include "Player/GrabbableInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Kismet/GameplayStatics.h"

ACh4_PlayerCharacter::ACh4_PlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(RootComponent);
	SpringArmComponent->TargetArmLength = 300.0f;
	SpringArmComponent->bUsePawnControlRotation = true;
	
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);
	CameraComponent->bUsePawnControlRotation = false;
	
	bUseControllerRotationYaw = false; // 카메라 이동에 따라 캐릭터 몸이 같이 움직이지 않음
	
	HatMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMeshComponent"));
	HatMeshComponent->SetupAttachment(GetMesh(), HatSocketName);
	HatMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HatMeshComponent->ComponentTags.Add(FName(TEXT("Headwear")));
	
	GrabBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("GrabBoxComponent"));
	GrabBoxComponent->SetupAttachment(GetMesh(), GrabSocketName);
	GrabBoxComponent->SetBoxExtent(FVector(70.0f, 70.0f, 70.0f));
	GrabBoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 평소엔 꺼둠
	GrabBoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	GrabBoxComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	GrabBoxComponent->SetGenerateOverlapEvents(false);	
	
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
}

void ACh4_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	AddPlayerInputMappingContext();
	ApplyCharacterTypeFromPlayerState();
	UpdateNameplate();

	// PlayerState가 아직 없으면 현재 BP의 기본 설정으로 1회만 초기화한다.
	if (!bCharacterPhysicsInitialized)
	{
		InitializeCharacterPhysics();
	}
}

void ACh4_PlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && GrabbedCart)
	{
		GrabbedCart->ReleasePlayer(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ACh4_PlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyCharacterTypeFromPlayerState();
	UpdateNameplate();
}

void ACh4_PlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyCharacterTypeFromPlayerState();
	UpdateNameplate();
}

void ACh4_PlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Remote clients commonly receive possession after BeginPlay. Register the pawn IMC here too.
	AddPlayerInputMappingContext();
	UpdateNameplate();
}

void ACh4_PlayerCharacter::AddPlayerInputMappingContext()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (!PC->IsLocalController())
		{
			return;
		}

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (InputMappingContext)
			{
				Subsystem->AddMappingContext(InputMappingContext, 0);
				UE_LOG(LogCh4_multiGame, Log,
					TEXT("[PlayerInput] Activated existing mapping %s for locally possessed %s"),
					*GetNameSafe(InputMappingContext),
					*GetNameSafe(this));
			}
		}
	}
}

void ACh4_PlayerCharacter::ApplyCharacterTypeFromPlayerState()
{
	if (const ACh4_multiGamePlayerState* CharacterPlayerState =
		GetPlayerState<ACh4_multiGamePlayerState>())
	{
		ApplyCharacterType(CharacterPlayerState->GetCharacterType());
		ApplyHeadwear(CharacterPlayerState->GetEquippedHeadwearID());
	}
}

void ACh4_PlayerCharacter::ApplyCharacterType(const ECh4CharacterType CharacterType)
{
	if (!Ch4Character::IsValidType(CharacterType) || !GetWorld() || !GetMesh())
	{
		return;
	}

	if (CurrentCharacterType == CharacterType && bCharacterPhysicsInitialized)
	{
		return;
	}
	CurrentCharacterType = CharacterType;

	const UCh4_multiGameGameInstance* GameInstance =
		GetWorld()->GetGameInstance<UCh4_multiGameGameInstance>();
	if (!GameInstance)
	{
		return;
	}

	const TSubclassOf<ACh4_PlayerCharacter> CharacterClass =
		GameInstance->LoadCharacterClass(CharacterType);
	ACh4_PlayerCharacter* AppearanceDefaults = CharacterClass
		? CharacterClass->GetDefaultObject<ACh4_PlayerCharacter>()
		: nullptr;
	USkeletalMeshComponent* AppearanceMesh = AppearanceDefaults
		? AppearanceDefaults->GetMesh()
		: nullptr;
	if (!AppearanceMesh || !AppearanceMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	// 자식 컴포넌트(모자 등)가 붙어있는 상태에서 피직스 에셋 교체 시 Welded Body 크래시 방지
	TArray<USceneComponent*> AttachedChildren;
	GetMesh()->GetChildrenComponents(true, AttachedChildren);
	for (USceneComponent* Child : AttachedChildren)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child))
		{
			Prim->SetSimulatePhysics(false);
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetSkeletalMeshAsset(AppearanceMesh->GetSkeletalMeshAsset());
	GetMesh()->SetAnimInstanceClass(AppearanceMesh->GetAnimClass());
	GetMesh()->SetRelativeTransform(AppearanceMesh->GetRelativeTransform());

	// 콜리전 프로파일 및 피직스 에셋 동기화
	GetMesh()->SetCollisionProfileName(AppearanceMesh->GetCollisionProfileName());
	GetMesh()->SetCollisionEnabled(AppearanceMesh->GetCollisionEnabled());
	if (UPhysicsAsset* NewPhysicsAsset = AppearanceMesh->GetPhysicsAsset())
	{
		GetMesh()->SetPhysicsAsset(NewPhysicsAsset, false);
	}
	else if (USkeletalMesh* SkeletalMeshAsset = AppearanceMesh->GetSkeletalMeshAsset())
	{
		if (UPhysicsAsset* MeshPhysicsAsset = SkeletalMeshAsset->GetPhysicsAsset())
		{
			GetMesh()->SetPhysicsAsset(MeshPhysicsAsset, false);
		}
	}

	// These assets contain character-specific animations, not control behavior.
	StunMontage = AppearanceDefaults->StunMontage;
	StunCameraShake = AppearanceDefaults->StunCameraShake;
	EmotionDataAsset = AppearanceDefaults->EmotionDataAsset;
	GrabMontage = AppearanceDefaults->GrabMontage;
	GrabReleaseMontage = AppearanceDefaults->GrabReleaseMontage;

	// 동물별 래그돌 물리 설정 동기화
	RagdollRootBone = AppearanceDefaults->RagdollRootBone;
	RagdollProfileName = AppearanceDefaults->RagdollProfileName;
	RagdollStrengthMultiplier = AppearanceDefaults->RagdollStrengthMultiplier;
	bRagdollIncludeSelf = AppearanceDefaults->bRagdollIncludeSelf;

	InitializeCharacterPhysics();
	UpdateNameplate();

	// E_AnimalType enum order: Dog=0, Otter=1, Gorilla=2, Cat=3
	uint8 MappedAnimalIndex = 3; // Default Cat
	switch (CharacterType)
	{
	case ECh4CharacterType::Dog:     MappedAnimalIndex = 0; break;
	case ECh4CharacterType::Otter:   MappedAnimalIndex = 1; break;
	case ECh4CharacterType::Gorilla: MappedAnimalIndex = 2; break;
	case ECh4CharacterType::Cat:     MappedAnimalIndex = 3; break;
	default: break;
	}

	// 블루프린트 래그돌 초기화용 이벤트 발생
	OnCharacterTypeApplied(CharacterType);
	OnCharacterTypeChanged(MappedAnimalIndex);

	if (!CurrentHeadwearID.IsNone())
	{
		ApplyHeadwear(CurrentHeadwearID);
	}
}

void ACh4_PlayerCharacter::InitializeCharacterPhysics()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return;
	}

	UPhysicsAsset* PhysAsset = MeshComp->GetPhysicsAsset();
	if (!PhysAsset)
	{
		return;
	}

	if (UPhysicalAnimationComponent* PhysAnimComp = FindComponentByClass<UPhysicalAnimationComponent>())
	{
		PhysAnimComp->SetSkeletalMeshComponent(MeshComp);

		// 피직스 에셋 내에 해당 본이 존재하는지 검증 후 안전하게 적용
		if (PhysAsset->FindBodyIndex(RagdollRootBone) != INDEX_NONE)
		{
			PhysAnimComp->ApplyPhysicalAnimationProfileBelow(RagdollRootBone, RagdollProfileName, bRagdollIncludeSelf);
			MeshComp->SetAllBodiesBelowSimulatePhysics(RagdollRootBone, true, bRagdollIncludeSelf);
			PhysAnimComp->SetStrengthMultiplyer(RagdollStrengthMultiplier);
			bCharacterPhysicsInitialized = true;
		}
	}
}

void ACh4_PlayerCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, bIsStunned);
	DOREPLIFETIME(ThisClass, GrabbedComponent);
	DOREPLIFETIME(ThisClass, GrabbedCart);
	DOREPLIFETIME(ThisClass, CartMoveInput);
	DOREPLIFETIME(ThisClass, bIsBraking);
}

void ACh4_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACh4_PlayerCharacter::InputActionMove);
		if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ACh4_PlayerCharacter::InputActionMove);
		if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACh4_PlayerCharacter::InputActionLook);
		if (JumpAction) EIC->BindAction(JumpAction, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionJump);
		if (Emote1Action) EIC->BindAction(Emote1Action, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionEmote1);
		if (Emote2Action) EIC->BindAction(Emote2Action, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionEmote2);
		if (Emote3Action) EIC->BindAction(Emote3Action, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionEmote3);
		if (Emote4Action) EIC->BindAction(Emote4Action, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionEmote4);
		if (GrabAction) EIC->BindAction(GrabAction, ETriggerEvent::Started, this, &ACh4_PlayerCharacter::InputActionGrab);
		if (CartGrabAction) EIC->BindAction(CartGrabAction, ETriggerEvent::Started, this, &ACh4_PlayerCharacter::InputActionCartGrab);
	}
}

void ACh4_PlayerCharacter::SetHatMesh(class UStaticMesh* NewHat)
{
	if (HatMeshComponent == nullptr)
	{
		return;
	}

	FName EffectiveSocketName = HatSocketName;
	if (GetMesh())
	{
		if (EffectiveSocketName.IsNone() || EffectiveSocketName == FName(TEXT("HatSocket")) || !GetMesh()->DoesSocketExist(EffectiveSocketName))
		{
			if (GetMesh()->DoesSocketExist(TEXT("S_Headwear")))
			{
				EffectiveSocketName = TEXT("S_Headwear");
			}
		}

		HatMeshComponent->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			EffectiveSocketName);
	}

	if (NewHat)
	{
		HatMeshComponent->SetStaticMesh(NewHat);
		HatMeshComponent->SetVisibility(true);
	}
	else
	{
		HatMeshComponent->SetStaticMesh(nullptr);
		HatMeshComponent->SetVisibility(false);
	}
}

void ACh4_PlayerCharacter::ApplyHeadwear(FName HeadwearID)
{
	CurrentHeadwearID = HeadwearID;

	FName EffectiveSocketName = HatSocketName;
	if (GetMesh())
	{
		if (EffectiveSocketName.IsNone() || EffectiveSocketName == FName(TEXT("HatSocket")) || !GetMesh()->DoesSocketExist(EffectiveSocketName))
		{
			if (GetMesh()->DoesSocketExist(TEXT("S_Headwear")))
			{
				EffectiveSocketName = TEXT("S_Headwear");
			}
		}

		if (HatMeshComponent)
		{
			HatMeshComponent->ComponentTags.AddUnique(FName(TEXT("Headwear")));
			HatMeshComponent->AttachToComponent(
				GetMesh(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				EffectiveSocketName);
		}
	}

	for (UActorComponent* Comp : GetComponents())
	{
		if (Comp && Comp->GetClass()->GetName().Contains(TEXT("BPC_AccessoryEquipment")))
		{
			// BPC_AccessoryEquipment의 HeadwearComponent를 HatMeshComponent로 사전 바인딩하여 Accessed None 방지
			if (FObjectProperty* MeshProp = CastField<FObjectProperty>(Comp->GetClass()->FindPropertyByName(TEXT("HeadwearComponent"))))
			{
				MeshProp->SetObjectPropertyValue_InContainer(Comp, HatMeshComponent);
			}

			if (FProperty* Prop = Comp->GetClass()->FindPropertyByName(TEXT("AnimalType")))
			{
				uint8 AnimalIndex = 3;
				switch (CurrentCharacterType)
				{
				case ECh4CharacterType::Dog:     AnimalIndex = 0; break;
				case ECh4CharacterType::Otter:   AnimalIndex = 1; break;
				case ECh4CharacterType::Gorilla: AnimalIndex = 2; break;
				case ECh4CharacterType::Cat:     AnimalIndex = 3; break;
				default: break;
				}

				if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
				{
					ByteProp->SetPropertyValue_InContainer(Comp, AnimalIndex);
				}
				else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
				{
					if (FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty())
					{
						UnderlyingProp->SetIntPropertyValue(
							EnumProp->ContainerPtrToValuePtr<void>(Comp),
							static_cast<int64>(AnimalIndex));
					}
				}
			}

			if (HeadwearID.IsNone())
			{
				// 현재 장착된 모자가 있었던 경우에만 UnequipHeadwear 호출
				FName CurrentlyEquipped = NAME_None;
				if (FProperty* HatProp = Comp->GetClass()->FindPropertyByName(TEXT("EquippedHeadwearID")))
				{
					if (FNameProperty* NameProp = CastField<FNameProperty>(HatProp))
					{
						CurrentlyEquipped = NameProp->GetPropertyValue_InContainer(Comp);
					}
				}

				if (!CurrentlyEquipped.IsNone())
				{
					if (UFunction* UnequipFunc = Comp->FindFunction(TEXT("UnequipHeadwear")))
					{
						Comp->ProcessEvent(UnequipFunc, nullptr);
					}
				}

				if (HatMeshComponent)
				{
					HatMeshComponent->SetStaticMesh(nullptr);
					HatMeshComponent->SetVisibility(false);
				}
			}
			else
			{
				if (UFunction* EquipFunc = Comp->FindFunction(TEXT("EquipHeadwear")))
				{
					struct { FName ID; } Params{ HeadwearID };
					Comp->ProcessEvent(EquipFunc, &Params);
					if (HatMeshComponent)
					{
						HatMeshComponent->SetVisibility(true);
						HatMeshComponent->MarkRenderStateDirty();
					}
				}
			}
			return;
		}
	}
}

UWidgetComponent* ACh4_PlayerCharacter::GetNameplateComponent() const
{
	if (CachedNameplateComponent.IsValid())
	{
		return CachedNameplateComponent.Get();
	}

	TArray<UWidgetComponent*> WidgetComps;
	GetComponents<UWidgetComponent>(WidgetComps);
	for (UWidgetComponent* Comp : WidgetComps)
	{
		if (Comp && Comp->GetName().Contains(TEXT("Nameplate")))
		{
			CachedNameplateComponent = Comp;
			return Comp;
		}
	}

	if (WidgetComps.Num() > 0 && WidgetComps[0])
	{
		CachedNameplateComponent = WidgetComps[0];
		return WidgetComps[0];
	}

	return nullptr;
}

void ACh4_PlayerCharacter::UpdateNameplate_Implementation()
{
	UWidgetComponent* NameplateComp = GetNameplateComponent();
	if (!NameplateComp)
	{
		return;
	}

	// 1. Hide locally controlled player's nameplate to avoid blocking third-person camera
	if (IsLocallyControlled())
	{
		NameplateComp->SetVisibility(false);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(NameplateRetryTimerHandle);
		}
		return;
	}

	NameplateComp->SetVisibility(true);

	// 2. Attach to head_socket (or fallback to head bone)
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		FName SocketToUse = NameplateSocketName;
		if (!CharacterMesh->DoesSocketExist(SocketToUse))
		{
			if (CharacterMesh->DoesSocketExist(TEXT("head")))
			{
				SocketToUse = TEXT("head");
			}
			else
			{
				SocketToUse = NAME_None;
			}
		}

		if (NameplateComp->GetAttachSocketName() != SocketToUse || NameplateComp->GetAttachParent() != CharacterMesh)
		{
			NameplateComp->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketToUse);
		}
		NameplateComp->SetRelativeLocation(NameplateOffset);
		NameplateComp->SetRelativeRotation(FRotator::ZeroRotator);
	}

	// 3. Screen space, desired size, centered pivot, distance culling
	NameplateComp->SetWidgetSpace(EWidgetSpace::Screen);
	NameplateComp->SetDrawAtDesiredSize(true);
	NameplateComp->SetPivot(FVector2D(0.5f, 0.5f));
	NameplateComp->SetCullDistance(NameplateMaxDrawDistance);

	// 4. Player name replication handling
	APlayerState* PS = GetPlayerState();
	FString PlayerName = PS ? PS->GetPlayerName() : FString();

	const bool bHasValidName = !PlayerName.IsEmpty() && PlayerName != TEXT("Player");

	if (!bHasValidName && NameplateRetryCount < 10)
	{
		++NameplateRetryCount;
		if (UWorld* World = GetWorld())
		{
			if (!World->GetTimerManager().IsTimerActive(NameplateRetryTimerHandle))
			{
				World->GetTimerManager().SetTimer(NameplateRetryTimerHandle, this, &ACh4_PlayerCharacter::UpdateNameplate, 0.2f, false);
			}
		}
		if (PlayerName.IsEmpty())
		{
			return;
		}
	}
	else
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(NameplateRetryTimerHandle);
		}
		NameplateRetryCount = 0;
	}

	// 5. Update the widget UI
	if (UUserWidget* UserWidget = NameplateComp->GetUserWidgetObject())
	{
		// Try calling SetPlayerName if present on the blueprint widget
		static const FName SetPlayerNameFuncName = TEXT("SetPlayerName");
		if (UFunction* SetPlayerNameFunc = UserWidget->FindFunction(SetPlayerNameFuncName))
		{
			for (TFieldIterator<FProperty> PropIt(SetPlayerNameFunc); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				if (CastField<FTextProperty>(*PropIt))
				{
					struct FTextParam { FText Val; } Params{ FText::FromString(PlayerName) };
					UserWidget->ProcessEvent(SetPlayerNameFunc, &Params);
					break;
				}
				else if (CastField<FStrProperty>(*PropIt))
				{
					struct FStrParam { FString Val; } Params{ PlayerName };
					UserWidget->ProcessEvent(SetPlayerNameFunc, &Params);
					break;
				}
			}
		}

		// Also directly set Text_PlayerName text block if found
		if (UTextBlock* TextBlock = Cast<UTextBlock>(UserWidget->GetWidgetFromName(TEXT("Text_PlayerName"))))
		{
			TextBlock->SetText(FText::FromString(PlayerName));
		}
	}
}

void ACh4_PlayerCharacter::InputActionMove(const struct FInputActionValue& Value)
{
	if (bIsStunned)
	{
		return;
	}
	
	const FVector2D MoveVec = Value.Get<FVector2D>();
	
	if (Controller == nullptr)
	{
		return;
	}
	
	if (GrabbedCart)
	{
		ServerRPC_SetCartMoveInput(MoveVec);
		return;   // 캐릭터는 걷지 않음
	}
	
	if (MoveVec.IsNearlyZero() == false)
	{
		InterruptEmotionMontage();
	}

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MoveVec.Y);
	AddMovementInput(RightDirection, MoveVec.X);
}

void ACh4_PlayerCharacter::InputActionLook(const struct FInputActionValue& Value)
{
	const FVector2D LookVec = Value.Get<FVector2D>();
	
	AddControllerYawInput(LookVec.X);
	AddControllerPitchInput(LookVec.Y);
}

void ACh4_PlayerCharacter::InputActionJump(const FInputActionValue& Value)
{
	if (bIsStunned || GrabbedCart)
	{
		return;
	}
	
	if (GetCharacterMovement()->IsFalling())
	{
		return;
    }

	InterruptEmotionMontage();
    	
	Jump();
}

void ACh4_PlayerCharacter::InputActionEmote1(const struct FInputActionValue& Value)
{
	if (bIsStunned)
	{
		return;
	}
	
	if (GrabbedComponent || GrabbedCart)
	{
		return;
	}
	
	PlayEmotion(EEmotionType::Emote1);
}

void ACh4_PlayerCharacter::InputActionEmote2(const struct FInputActionValue& Value)
{
	if (bIsStunned)
	{
		return;
	}
	
	if (GrabbedComponent || GrabbedCart)
	{
		return;
	}
	
	PlayEmotion(EEmotionType::Emote2);
}

void ACh4_PlayerCharacter::InputActionEmote3(const struct FInputActionValue& Value)
{
	if (bIsStunned)
	{
		return;
	}
	
	if (GrabbedComponent || GrabbedCart)
	{
		return;
	}
	
	PlayEmotion(EEmotionType::Emote3);
}

void ACh4_PlayerCharacter::InputActionEmote4(const struct FInputActionValue& Value)
{
	if (bIsStunned)
	{
		return;
	}
	
	if (GrabbedComponent || GrabbedCart)
	{
		return;
	}
	
	PlayEmotion(EEmotionType::Emote4);
}

void ACh4_PlayerCharacter::InputActionGrab(const FInputActionValue& Value)
{
	if (bIsStunned || GrabbedCart)
	{
		return;
	}
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return;
	}
	
	const bool bGrabMontagePlaying = GrabMontage && AnimInstance->Montage_IsPlaying(GrabMontage);
	const bool bReleaseMontagePlaying = GrabReleaseMontage && AnimInstance->Montage_IsPlaying(GrabReleaseMontage);
	
	if (bGrabMontagePlaying || bReleaseMontagePlaying)
	{
		return; // 몽타주 재생 중엔 입력 무시
	}
	
	if (GrabbedComponent != nullptr)
	{
		if (HasAuthority())
		{
			MulticastRPC_PlayGrabReleaseMontage();
		}
		else
		{
			ServerRPC_PlayGrabReleaseMontage();
		}
		return;
	}

	if (HasAuthority())
	{
		MulticastRPC_PlayGrabMontage();
	}
	else
	{
		ServerRPC_PlayGrabMontage();
	}
}

void ACh4_PlayerCharacter::InputActionCartGrab(const struct FInputActionValue& Value)
{
	if (bIsStunned || GrabbedComponent)
	{
		return;
	}
	
	if (GrabbedCart)
	{
		ServerRPC_RequestCartRelease();
	}
	else
	{
		FHitResult Hit;
		const FVector Start = GetActorLocation();
		const FVector End = Start + GetActorForwardVector() * 200.0f;

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(50.0f), Params))
		{
			if (ACartBase* Cart = Cast<ACartBase>(Hit.GetActor()))
			{
				ServerRPC_RequestCartGrab(Cart);
			}
		}
	}
}

void ACh4_PlayerCharacter::ServerRPC_RequestCartGrab_Implementation(ACartBase* TargetCart)
{
	if (bIsStunned || GrabbedComponent || GrabbedCart || !IsValid(TargetCart)
		|| TargetCart->GetWorld() != GetWorld())
	{
		return;
	}

	TargetCart->TryGrabPlayer(this);
}

void ACh4_PlayerCharacter::ServerRPC_RequestCartRelease_Implementation()
{
	if (GrabbedCart)
	{
		GrabbedCart->ReleasePlayer(this);
	}
}

void ACh4_PlayerCharacter::ServerRPC_SetCartMoveInput_Implementation(const FVector2D Input)
{
	if (GrabbedCart)
	{
		GrabbedCart->SetPlayerMoveInput(this, Input);
	}
}

void ACh4_PlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// Falling 상태로 진입했을 때
	if (GetCharacterMovement()->MovementMode == MOVE_Falling)
	{
		FallStartTime = GetWorld()->GetTimeSeconds();
	}
}

void ACh4_PlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (HasAuthority() == false)
	{
		return;
	}
	
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// 낙하한 시간 = 착지 시간 - 낙하 시작 시간
	const float FallDuration = CurrentTime - FallStartTime;

	// 설정한 시간 이상 낙하했다면 경직
	if (FallDuration >= FallStunThreshold)
	{
		OnStun();
	}
}

void ACh4_PlayerCharacter::OnStun()
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	if (bIsStunned)
	{
		return;
	}
	
	bIsStunned = true;
	
	// 서버(호스트)는 RepNotify가 자동으로 안 불리므로 직접 호출
    OnRep_IsStunned();
	
	// 현재 이동 중이었다면 즉시 정지
	GetCharacterMovement()->StopMovementImmediately();

	// 이동 비활성화
	GetCharacterMovement()->DisableMovement();

	// 기존 타이머 제거
	GetWorldTimerManager().ClearTimer(StunTimerHandle);

	// StunDuration 후 경직 종료
	GetWorldTimerManager().SetTimer(
		StunTimerHandle,
		this,
		&ACh4_PlayerCharacter::EndStun,
		StunDuration,
		false);
}

void ACh4_PlayerCharacter::EndStun()
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	bIsStunned = false;
	
	// 서버(호스트)는 RepNotify가 자동으로 안 불리므로 직접 호출
	OnRep_IsStunned();
	
	// 다시 걷기 가능
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ACh4_PlayerCharacter::OnRep_IsStunned()
{
	// 서버에서 복제된 bIsStunned 값이 클라이언트에 도착하는 순간 특별한 작업을 해야 할 때 사용
	// 이펙트, 사운드, 애니메이션 등등
	
	if (bIsStunned)
	{
		// 모든 클라이언트에서 경직 애니메이션 재생
		if (StunMontage)
		{
			PlayAnimMontage(StunMontage);
		}

		// 경직된 캐릭터를 직접 조작하는 클라이언트에서만 HitStop, CameraShake
		if (IsLocallyControlled())
		{
			StartHitStop();
			
			if (APlayerController* PC =	Cast<APlayerController>(GetController()))
			{
				if (StunCameraShake)
				{
					PC->ClientStartCameraShake(StunCameraShake);
				}
			}
		}
	}
	else
	{
		// HitStop 중이었다면 자신의 클라이언트에서만 복구
		if (IsLocallyControlled())
		{
			GetWorldTimerManager().ClearTimer(HitStopTimerHandle);

			CustomTimeDilation = 1.0f;
		}

		if (StunMontage)
		{
			StopAnimMontage(StunMontage);
		}
	}
}

void ACh4_PlayerCharacter::StartHitStop()
{
	// 경직 캐릭터의 시간만 살짝 지연
	CustomTimeDilation = HitStopTimeDilation;
	
	// 기존 HitStop 타이머가 있다면 제거
	GetWorldTimerManager().ClearTimer(HitStopTimerHandle);

	// HitStop 종료 타이머
	GetWorldTimerManager().SetTimer(
		HitStopTimerHandle,
		this,
		&ACh4_PlayerCharacter::EndHitStop,
		HitStopDuration,
		false
	);
}

void ACh4_PlayerCharacter::EndHitStop()
{
	// 시간 속도를 원래대로 복구
	CustomTimeDilation = 1.0f;
}

UAnimMontage* ACh4_PlayerCharacter::FindEmotionMontage(EEmotionType EmotionType) const
{
	if (EmotionDataAsset == nullptr)
	{
		return nullptr;
	}

	for (const FEmotionData& EmotionData : EmotionDataAsset->EmotionDataList)
	{
		if (EmotionData.EmotionType == EmotionType)
		{
			return EmotionData.EmoteMontage;
		}
	}

	return nullptr;
}

void ACh4_PlayerCharacter::PlayEmotion(EEmotionType EmotionType)
{
	if (HasAuthority())
	{
		MulticastRPC_PlayEmotion(EmotionType);
	}
	else
	{
		ServerRPC_PlayEmotion(EmotionType);
	}
}

void ACh4_PlayerCharacter::ServerRPC_PlayEmotion_Implementation(EEmotionType EmotionType)
{
	MulticastRPC_PlayEmotion(EmotionType);
}

void ACh4_PlayerCharacter::MulticastRPC_PlayEmotion_Implementation(EEmotionType EmotionType)
{
	UAnimMontage* EmotionMontage = FindEmotionMontage(EmotionType);

	if (EmotionMontage == nullptr)
	{
		return;
	}

	PlayAnimMontage(EmotionMontage);
}

void ACh4_PlayerCharacter::InterruptEmotionMontage()
{
	if (IsEmotionMontagePlaying() == false)
	{
		return;   // 재생 중인 감정이 없으면 RPC를 보낼 필요가 없음
	}
	
	StopEmotionMontages();

	if (HasAuthority())
	{
		MulticastRPC_InterruptEmotionMontage();
	}
	else
	{
		ServerRPC_InterruptEmotionMontage();
	}
}

void ACh4_PlayerCharacter::StopEmotionMontages(float BlendOutTime)
{
	if (EmotionDataAsset == nullptr)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return;
	}

	for (const FEmotionData& EmotionData : EmotionDataAsset->EmotionDataList)
	{
		if (EmotionData.EmoteMontage)
		{
			AnimInstance->Montage_Stop(BlendOutTime, EmotionData.EmoteMontage);
		}
	}
}

bool ACh4_PlayerCharacter::IsEmotionMontagePlaying() const
{
	if (EmotionDataAsset == nullptr)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	for (const FEmotionData& EmotionData : EmotionDataAsset->EmotionDataList)
	{
		if (EmotionData.EmoteMontage && AnimInstance->Montage_IsPlaying(EmotionData.EmoteMontage))
		{
			return true;
		}
	}

	return false;
}

void ACh4_PlayerCharacter::ServerRPC_InterruptEmotionMontage_Implementation()
{
	MulticastRPC_InterruptEmotionMontage();
}

void ACh4_PlayerCharacter::MulticastRPC_InterruptEmotionMontage_Implementation()
{
	StopEmotionMontages();
}

void ACh4_PlayerCharacter::OnRep_GrabbedComponent()
{
	// 클라이언트에서 잡음/놓음 시각 효과 처리용 (필요 시 사용)
}

void ACh4_PlayerCharacter::BeginGrabDetection()
{
	if (HasAuthority() == false && IsLocallyControlled() == false)
	{
		return; // 서버 또는 본인 조종 클라이언트만 판정
	}

	if (GrabbedComponent != nullptr || GrabBoxComponent == nullptr)
	{
		return;
	}

	GrabBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GrabBoxComponent->SetGenerateOverlapEvents(true);
	GrabBoxComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ACh4_PlayerCharacter::OnGrabBoxBeginOverlap);

	// 콜리전이 켜지기 전부터 이미 범위 안에 들어와 있던 대상도 놓치지 않도록
	// 활성화 직후 한 번 즉시 갱신해서 확인한다.
	GrabBoxComponent->UpdateOverlaps();

	TArray<AActor*> OverlappingActors;
	GrabBoxComponent->GetOverlappingActors(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (OverlappingActor && OverlappingActor->Implements<UGrabbableInterface>())
		{
			TryGrabActor(OverlappingActor);
			break;
		}
	}
}

void ACh4_PlayerCharacter::EndGrabDetection()
{

	if (GrabBoxComponent == nullptr)
	{
		return;
	}

	GrabBoxComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ACh4_PlayerCharacter::OnGrabBoxBeginOverlap);
	GrabBoxComponent->SetGenerateOverlapEvents(false);
	GrabBoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACh4_PlayerCharacter::TryGrabActor(AActor* TargetActor)
{
	if (GrabbedComponent != nullptr)
	{
		return;
	}

	IGrabbableInterface* GI = Cast<IGrabbableInterface>(TargetActor);
	if (GI == nullptr)
	{
		return;
	}

	ServerRPC_AttachGrab(GI->GetGrabbableComponent());

	// 한 번 잡았으면 이 구간에서 더 검사할 필요가 없으니 바로 꺼준다.
	EndGrabDetection();
}

void ACh4_PlayerCharacter::OnGrabBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->Implements<UGrabbableInterface>())
	{
		TryGrabActor(OtherActor);
	}
}

void ACh4_PlayerCharacter::ServerRPC_AttachGrab_Implementation(UPrimitiveComponent* TargetComponent)
{
	if (TargetComponent == nullptr || GrabbedComponent != nullptr)
	{
		return;
	}

	GrabbedComponent = TargetComponent;
	MulticastRPC_AttachGrab(TargetComponent);
}

void ACh4_PlayerCharacter::MulticastRPC_AttachGrab_Implementation(UPrimitiveComponent* TargetComponent)
{
	if (TargetComponent == nullptr)
	{
		return;
	}

	TargetComponent->SetSimulatePhysics(false);
	TargetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetComponent->AttachToComponent(
	   GetMesh(),
	   FAttachmentTransformRules::SnapToTargetNotIncludingScale,
	   GrabSocketName);
	
	if (IsValid(GrabSound) == true)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GrabSound, GetActorLocation());
	}
}

void ACh4_PlayerCharacter::ServerRPC_ReleaseGrab_Implementation()
{
	if (GrabbedComponent == nullptr)
	{
		return;
	}
	
	MulticastRPC_ReleaseGrab(GrabbedComponent);
	GrabbedComponent = nullptr;
}

void ACh4_PlayerCharacter::MulticastRPC_ReleaseGrab_Implementation(UPrimitiveComponent* TargetComponent)
{
	if (TargetComponent == nullptr)
	{
		return;
	}
	
	TargetComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	TargetComponent->SetSimulatePhysics(true);
	TargetComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	if (IsValid(GrabReleaseSound) == true)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GrabReleaseSound, GetActorLocation());
	}
}

void ACh4_PlayerCharacter::ServerRPC_PlayGrabMontage_Implementation()
{
	MulticastRPC_PlayGrabMontage();
}

void ACh4_PlayerCharacter::MulticastRPC_PlayGrabMontage_Implementation()
{
	if (GrabMontage)
	{
		PlayAnimMontage(GrabMontage);
	}
}

void ACh4_PlayerCharacter::OnGrabReleaseNotify()
{
	if (HasAuthority() == false && IsLocallyControlled() == false)
	{
		return;
	}
	
	if (GrabbedComponent != nullptr)
	{
		ServerRPC_ReleaseGrab();
	}
}

void ACh4_PlayerCharacter::ServerRPC_PlayGrabReleaseMontage_Implementation()
{
	MulticastRPC_PlayGrabReleaseMontage();
}

void ACh4_PlayerCharacter::MulticastRPC_PlayGrabReleaseMontage_Implementation()
{
	if (GrabReleaseMontage)
	{
		PlayAnimMontage(GrabReleaseMontage);
	}
}

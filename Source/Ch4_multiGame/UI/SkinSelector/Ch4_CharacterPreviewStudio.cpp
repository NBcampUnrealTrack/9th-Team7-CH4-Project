#include "UI/SkinSelector/Ch4_CharacterPreviewStudio.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/DataTable.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"
#include "Player/Ch4_multiGameGameInstance.h"
#include "Player/Ch4_PlayerCharacter.h"

ACh4_CharacterPreviewStudio::ACh4_CharacterPreviewStudio()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;
	SetReplicatingMovement(false);

	// 1. Root Component
	StudioRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StudioRoot"));
	RootComponent = StudioRoot;

	// 2. Preview Mannequin Mesh
	PreviewMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMeshComponent"));
	PreviewMeshComponent->SetupAttachment(StudioRoot);
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMeshComponent->SetCastShadow(false);
	PreviewMeshComponent->SetGenerateOverlapEvents(false);
	PreviewMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// 3. Hat Component
	HatMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMeshComponent"));
	HatMeshComponent->SetupAttachment(PreviewMeshComponent, HatSocketName);
	HatMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HatMeshComponent->SetCastShadow(false);
	HatMeshComponent->ComponentTags.Add(FName(TEXT("Headwear")));

	// 4. Backdrop Component (Clean Studio Background)
	BackdropMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackdropMeshComponent"));
	BackdropMeshComponent->SetupAttachment(StudioRoot);
	BackdropMeshComponent->SetRelativeLocation(FVector(-120.0f, 0.0f, -10.0f));
	BackdropMeshComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	BackdropMeshComponent->SetRelativeScale3D(FVector(40.0f, 40.0f, 1.0f));
	BackdropMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackdropMeshComponent->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		BackdropMeshComponent->SetStaticMesh(PlaneFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		BackdropMeshComponent->SetMaterial(0, MatFinder.Object);
	}

	// 5. SceneCapture2D Component
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureComponent"));
	CaptureComponent->SetupAttachment(StudioRoot);
	UpdateCameraTransform();
	CaptureComponent->CaptureSource = CaptureSource;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->CompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;

	// ShowFlags: Disable Lumen, Dynamic Shadows, AO, Bloom, Fog for clean, flat cartoon look
	CaptureComponent->ShowFlags.SetDynamicShadows(false);
	CaptureComponent->ShowFlags.SetLumenGlobalIllumination(false);
	CaptureComponent->ShowFlags.SetLumenReflections(false);
	CaptureComponent->ShowFlags.SetGlobalIllumination(false);
	CaptureComponent->ShowFlags.SetAmbientOcclusion(true);
	CaptureComponent->ShowFlags.SetBloom(false);
	CaptureComponent->ShowFlags.SetEyeAdaptation(false);
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetVolumetricFog(false);
	CaptureComponent->ShowFlags.SetMotionBlur(false);
	CaptureComponent->ShowFlags.SetToneCurve(false);

	// PostProcess: Fixed manual exposure, clean bright toon look without dirty AO noise
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureMethod = true;
	CaptureComponent->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CaptureComponent->PostProcessSettings.AutoExposureBias = 0.0f;
	CaptureComponent->PostProcessSettings.bOverride_BloomIntensity = true;
	CaptureComponent->PostProcessSettings.BloomIntensity = 0.0f;
	CaptureComponent->ShowFlags.SetAmbientOcclusion(false);
	CaptureComponent->PostProcessSettings.bOverride_AmbientOcclusionIntensity = true;
	CaptureComponent->PostProcessSettings.AmbientOcclusionIntensity = 0.0f;

	CaptureComponent->ShowOnlyComponents.Add(PreviewMeshComponent);
	CaptureComponent->ShowOnlyComponents.Add(HatMeshComponent);
	CaptureComponent->ShowOnlyComponents.Add(BackdropMeshComponent);

	// 5. 3-Point Shadowless Studio Lighting (화사하고 부드러운 스튜디오 조명)
	KeyLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLightComponent"));
	KeyLightComponent->SetupAttachment(StudioRoot);
	KeyLightComponent->SetRelativeRotation(FRotator(-15.0f, 150.0f, 0.0f));
	KeyLightComponent->SetIntensity(3.2f);
	KeyLightComponent->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f));
	KeyLightComponent->SetCastShadows(false);

	FillLightComponent = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLightComponent"));
	FillLightComponent->SetupAttachment(StudioRoot);
	FillLightComponent->SetRelativeRotation(FRotator(-10.0f, -150.0f, 0.0f));
	FillLightComponent->SetIntensity(2.2f);
	FillLightComponent->SetLightColor(FLinearColor(0.96f, 0.98f, 1.0f));
	FillLightComponent->SetCastShadows(false);

	SkyLightComponent = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLightComponent"));
	SkyLightComponent->SetupAttachment(StudioRoot);
	SkyLightComponent->SetIntensity(2.0f);
	SkyLightComponent->SetLightColor(FLinearColor(1.0f, 1.0f, 1.0f));
	SkyLightComponent->SetCastShadows(false);

	// 6. Default Asset References
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> RTFinder(
		TEXT("/Game/UI/RenderTarget/RT_CharacterPreview.RT_CharacterPreview"));
	if (RTFinder.Succeeded())
	{
		PreviewRenderTarget = RTFinder.Object;
		CaptureComponent->TextureTarget = PreviewRenderTarget;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> DTFinder(
		TEXT("/Game/Animal/Accessories/System/DT_Headwear.DT_Headwear"));
	if (DTFinder.Succeeded())
	{
		HeadwearDataTable = DTFinder.Object;
	}

	static ConstructorHelpers::FClassFinder<UActorComponent> BPCFinder(
		TEXT("/Game/Animal/Accessories/System/BPC_AccessoryEquipment.BPC_AccessoryEquipment_C"));
	if (BPCFinder.Succeeded())
	{
		AccessoryEquipmentClass = BPCFinder.Class;
	}
}

void ACh4_CharacterPreviewStudio::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->TickAnimation(DeltaSeconds, false);
		PreviewMeshComponent->RefreshBoneTransforms();
	}
}

void ACh4_CharacterPreviewStudio::BeginPlay()
{
	Super::BeginPlay();

	UpdateCameraTransform();

	// 런타임 조명 강도 보정 (화사하고 부드러운 스튜디오 룩)
	if (KeyLightComponent)
	{
		KeyLightComponent->SetIntensity(3.2f);
		KeyLightComponent->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f));
		KeyLightComponent->SetRelativeRotation(FRotator(-15.0f, 150.0f, 0.0f));
	}
	if (FillLightComponent)
	{
		FillLightComponent->SetIntensity(2.2f);
		FillLightComponent->SetLightColor(FLinearColor(0.96f, 0.98f, 1.0f));
		FillLightComponent->SetRelativeRotation(FRotator(-10.0f, -150.0f, 0.0f));
	}
	if (SkyLightComponent)
	{
		SkyLightComponent->SetIntensity(2.0f);
		SkyLightComponent->SetLightColor(FLinearColor(1.0f, 1.0f, 1.0f));
	}

	if (CaptureComponent)
	{
		CaptureComponent->CaptureSource = CaptureSource;
		CaptureComponent->LODDistanceFactor = 0.001f;

		// 털 노멀 노이즈 및 우중충한 검은 얼룩을 방지하기 위해 툰 스타일에 부적합한 SSAO 비활성화
		CaptureComponent->ShowFlags.SetAmbientOcclusion(false);
		CaptureComponent->PostProcessSettings.bOverride_AmbientOcclusionIntensity = true;
		CaptureComponent->PostProcessSettings.AmbientOcclusionIntensity = 0.0f;
	}

	// 2. 림 라이트: 과도한 역광 대비를 방지하고 모자/머리 윤곽에만 은은하게 1.0f 강도로 비춤
	UDirectionalLightComponent* RimLight = nullptr;
	for (UActorComponent* Comp : GetComponents())
	{
		if (Comp && Comp->ComponentHasTag(TEXT("RimLight")))
		{
			RimLight = Cast<UDirectionalLightComponent>(Comp);
			break;
		}
	}
	if (!RimLight)
	{
		RimLight = NewObject<UDirectionalLightComponent>(this, TEXT("RuntimeRimLight"));
		if (RimLight)
		{
			RimLight->ComponentTags.Add(TEXT("RimLight"));
			RimLight->SetupAttachment(StudioRoot);
			RimLight->RegisterComponent();
		}
	}
	if (RimLight)
	{
		RimLight->SetRelativeRotation(FRotator(-30.0f, 25.0f, 0.0f));
		RimLight->SetIntensity(1.0f);
		RimLight->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f));
		RimLight->SetCastShadows(false);
	}

	// 런타임 백드롭 생성 (라이브 코딩 CDO 누락 방지)
	if (!BackdropMeshComponent && bUseBackdrop)
	{
		BackdropMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("RuntimeBackdropMesh"));
		if (BackdropMeshComponent)
		{
			BackdropMeshComponent->SetupAttachment(StudioRoot);
			BackdropMeshComponent->SetRelativeLocation(FVector(-120.0f, 0.0f, -10.0f));
			BackdropMeshComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
			BackdropMeshComponent->SetRelativeScale3D(FVector(40.0f, 40.0f, 1.0f));
			BackdropMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BackdropMeshComponent->SetCastShadow(false);

			if (UStaticMesh* PlaneMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"))))
			{
				BackdropMeshComponent->SetStaticMesh(PlaneMesh);
			}
			if (UMaterialInterface* BaseMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))))
			{
				BackdropMeshComponent->SetMaterial(0, BaseMat);
			}
			BackdropMeshComponent->RegisterComponent();
		}
	}

	if (BackdropMeshComponent)
	{
		BackdropMeshComponent->SetRelativeLocation(FVector(-120.0f, 0.0f, -10.0f));
		BackdropMeshComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
		BackdropMeshComponent->SetRelativeScale3D(FVector(40.0f, 40.0f, 1.0f));

		if (bUseBackdrop)
		{
			BackdropMeshComponent->SetVisibility(true);
			if (UMaterialInstanceDynamic* DynMat = BackdropMeshComponent->CreateAndSetMaterialInstanceDynamic(0))
			{
				FLinearColor SafeBackdropColor = BackdropColor;
				if (SafeBackdropColor.R < 0.95f)
				{
					SafeBackdropColor = FLinearColor(0.96f, 0.96f, 0.97f, 1.0f);
				}
				DynMat->SetVectorParameterValue(TEXT("Color"), SafeBackdropColor);
			}
			if (CaptureComponent)
			{
				CaptureComponent->ShowOnlyComponents.AddUnique(BackdropMeshComponent);
			}
		}
		else
		{
			BackdropMeshComponent->SetVisibility(false);
			if (CaptureComponent)
			{
				CaptureComponent->ShowOnlyComponents.Remove(BackdropMeshComponent);
			}
		}
	}

	if (!AccessoryEquipmentClass)
	{
		AccessoryEquipmentClass = StaticLoadClass(
			UActorComponent::StaticClass(),
			nullptr,
			TEXT("/Game/Animal/Accessories/System/BPC_AccessoryEquipment.BPC_AccessoryEquipment_C"));
	}

	if (AccessoryEquipmentClass && !AccessoryEquipment)
	{
		AccessoryEquipment = NewObject<UActorComponent>(this, AccessoryEquipmentClass, TEXT("PreviewAccessoryEquipment"));
		if (AccessoryEquipment)
		{
			AccessoryEquipment->RegisterComponent();
		}
	}

	if (HatMeshComponent)
	{
		HatMeshComponent->ComponentTags.AddUnique(FName(TEXT("Headwear")));
	}

	if (!PreviewMeshComponent || !PreviewMeshComponent->GetSkeletalMeshAsset())
	{
		SetPreviewCharacterType(CurrentCharacterType);
		SetPreviewHeadwear(CurrentHeadwearID);
	}
}

void ACh4_CharacterPreviewStudio::UpdateCameraTransform()
{
	if (CaptureComponent)
	{
		// 카메라 위치: 캐릭터 얼굴과 머리가 화면 상단에 닿지 않고 적절한 여백(헤드룸)을 가지도록 조절
		const float SafeDistance = 215.0f;
		const float SafeHeight = 12.0f;
		const float SafePitch = 0.0f;
		const float SafeFOV = 36.0f;

		CaptureComponent->SetRelativeLocation(FVector(SafeDistance, 0.0f, SafeHeight));
		CaptureComponent->SetRelativeRotation(FRotator(SafePitch, 180.0f, 0.0f));
		CaptureComponent->FOVAngle = SafeFOV;
		CaptureComponent->LODDistanceFactor = 0.001f;

		CaptureComponent->ShowFlags.SetAmbientOcclusion(false);
		CaptureComponent->PostProcessSettings.bOverride_AmbientOcclusionIntensity = true;
		CaptureComponent->PostProcessSettings.AmbientOcclusionIntensity = 0.0f;
	}
}

void ACh4_CharacterPreviewStudio::SetPreviewCharacterType(ECh4CharacterType NewType)
{
	if (!Ch4Character::IsValidType(NewType))
	{
		return;
	}

	CurrentCharacterType = NewType;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UCh4_multiGameGameInstance* GI = World->GetGameInstance<UCh4_multiGameGameInstance>();
	if (!GI)
	{
		return;
	}

	const TSubclassOf<ACh4_PlayerCharacter> CharacterClass = GI->LoadCharacterClass(NewType);
	ACh4_PlayerCharacter* CDO = CharacterClass ? CharacterClass->GetDefaultObject<ACh4_PlayerCharacter>() : nullptr;
	USkeletalMeshComponent* CDOMesh = CDO ? CDO->GetMesh() : nullptr;

	if (CDOMesh && PreviewMeshComponent)
	{
		if (USkeletalMesh* SkelMesh = CDOMesh->GetSkeletalMeshAsset())
		{
			SkelMesh->NeverStream = true;
			PreviewMeshComponent->SetSkeletalMeshAsset(SkelMesh);
		}
		PreviewMeshComponent->SetRelativeTransform(CDOMesh->GetRelativeTransform());
		PreviewMeshComponent->MarkRenderStateDirty();

		// CDOMesh에 지정된 머티리얼 전체 복사 (누락된 머티리얼 슬롯 및 텍스처 깨짐 방지)
		const int32 NumMaterials = CDOMesh->GetNumMaterials();
		for (int32 MatIdx = 0; MatIdx < NumMaterials; ++MatIdx)
		{
			if (UMaterialInterface* Mat = CDOMesh->GetMaterial(MatIdx))
			{
				PreviewMeshComponent->SetMaterial(MatIdx, Mat);
			}
		}

		// 최고 퀄리티 LOD 0 강제 적용 및 스트리밍 해제
		PreviewMeshComponent->SetForcedLOD(1);
		PreviewMeshComponent->OverrideMinLOD(0);
		PreviewMeshComponent->StreamingDistanceMultiplier = 1000.0f;
		PreviewMeshComponent->PrestreamMeshLODs(99999.0f);
		PreviewMeshComponent->UpdateLODStatus();

		// 동물별 고유 Idle 애니메이션을 단일 노드로 즉각 재생하여 T-Pose 방지
		static const TCHAR* IdleAnimPaths[4] = {
			TEXT("/Game/Animal/Dog/V2/Anim/Dog_Idle_V2.Dog_Idle_V2"),       // 0: Dog
			TEXT("/Game/Animal/Otter/V2/Anim/Otter_Idle_V2.Otter_Idle_V2"),   // 1: Otter
			TEXT("/Game/Animal/Gorilla/V2/Anim/Gorilla_Idle_V2.Gorilla_Idle_V2"), // 2: Gorilla
			TEXT("/Game/Animal/Cat/V2/Anim/Cat_Idle_V2.Cat_Idle_V2")        // 3: Cat
		};

		uint8 AnimalIndex = 3;
		switch (NewType)
		{
		case ECh4CharacterType::Dog:     AnimalIndex = 0; break;
		case ECh4CharacterType::Otter:   AnimalIndex = 1; break;
		case ECh4CharacterType::Gorilla: AnimalIndex = 2; break;
		case ECh4CharacterType::Cat:     AnimalIndex = 3; break;
		default: break;
		}

		if (UAnimSequence* IdleAnim = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, IdleAnimPaths[AnimalIndex])))
		{
			PreviewMeshComponent->PlayAnimation(IdleAnim, true);
			PreviewMeshComponent->SetPlayRate(1.0f);
			PreviewMeshComponent->TickAnimation(0.0f, false);
			PreviewMeshComponent->RefreshBoneTransforms();
			PreviewMeshComponent->UpdateComponentToWorld();
		}
		else
		{
			PreviewMeshComponent->SetAnimInstanceClass(CDOMesh->GetAnimClass());
		}

		if (HatMeshComponent)
		{
			HatMeshComponent->AttachToComponent(
				PreviewMeshComponent,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				HatSocketName);
			HatMeshComponent->SetForcedLodModel(1);
		}

		UpdateAccessoryAnimalType();
		ApplyHeadwearInternal(CurrentHeadwearID);
	}
}

void ACh4_CharacterPreviewStudio::UpdateAccessoryAnimalType()
{
	if (!AccessoryEquipment)
	{
		return;
	}

	// E_AnimalType enum order: Dog=0, Otter=1, Gorilla=2, Cat=3
	uint8 AnimalIndex = 3;
	switch (CurrentCharacterType)
	{
	case ECh4CharacterType::Dog:     AnimalIndex = 0; break;
	case ECh4CharacterType::Otter:   AnimalIndex = 1; break;
	case ECh4CharacterType::Gorilla: AnimalIndex = 2; break;
	case ECh4CharacterType::Cat:     AnimalIndex = 3; break;
	default: break;
	}

	if (FProperty* Prop = AccessoryEquipment->GetClass()->FindPropertyByName(TEXT("AnimalType")))
	{
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
		{
			ByteProp->SetPropertyValue_InContainer(AccessoryEquipment, AnimalIndex);
		}
		else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			if (FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty())
			{
				UnderlyingProp->SetIntPropertyValue(
					EnumProp->ContainerPtrToValuePtr<void>(AccessoryEquipment),
					static_cast<int64>(AnimalIndex));
			}
		}
	}
}

void ACh4_CharacterPreviewStudio::SetPreviewHeadwear(FName HeadwearID)
{
	CurrentHeadwearID = HeadwearID;
	UpdateAccessoryAnimalType();
	ApplyHeadwearInternal(HeadwearID);
}

void ACh4_CharacterPreviewStudio::ApplyHeadwearInternal(FName HeadwearID)
{
	if (HeadwearID.IsNone())
	{
		if (AccessoryEquipment)
		{
			if (UFunction* UnequipFunc = AccessoryEquipment->FindFunction(TEXT("UnequipHeadwear")))
			{
				AccessoryEquipment->ProcessEvent(UnequipFunc, nullptr);
			}
		}
		if (HatMeshComponent)
		{
			HatMeshComponent->SetStaticMesh(nullptr);
			HatMeshComponent->SetVisibility(false);
		}
		return;
	}

	if (!AccessoryEquipment)
	{
		if (!AccessoryEquipmentClass)
		{
			AccessoryEquipmentClass = StaticLoadClass(
				UActorComponent::StaticClass(),
				nullptr,
				TEXT("/Game/Animal/Accessories/System/BPC_AccessoryEquipment.BPC_AccessoryEquipment_C"));
		}
		if (AccessoryEquipmentClass)
		{
			AccessoryEquipment = NewObject<UActorComponent>(this, AccessoryEquipmentClass, TEXT("RuntimeAccessoryEquipment"));
			if (AccessoryEquipment)
			{
				AccessoryEquipment->RegisterComponent();
				UpdateAccessoryAnimalType();
			}
		}
	}

	if (HatMeshComponent)
	{
		HatMeshComponent->ComponentTags.AddUnique(FName(TEXT("Headwear")));
	}

	if (AccessoryEquipment)
	{
		if (UFunction* EquipFunc = AccessoryEquipment->FindFunction(TEXT("EquipHeadwear")))
		{
			struct { FName ID; } Params{ HeadwearID };
			AccessoryEquipment->ProcessEvent(EquipFunc, &Params);
			if (HatMeshComponent)
			{
				HatMeshComponent->SetVisibility(true);
				HatMeshComponent->SetForcedLodModel(1);
				HatMeshComponent->MarkRenderStateDirty();
				if (CaptureComponent)
				{
					CaptureComponent->ShowOnlyComponents.AddUnique(HatMeshComponent);
				}
			}
		}
	}
}

void ACh4_CharacterPreviewStudio::AddPreviewYaw(float DeltaYaw)
{
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->AddLocalRotation(FRotator(0.0f, DeltaYaw, 0.0f));
	}
}

void ACh4_CharacterPreviewStudio::ResetPreviewRotation()
{
	if (PreviewMeshComponent)
	{
		FRotator NewRot = PreviewMeshComponent->GetRelativeRotation();
		NewRot.Yaw = -90.0f; // Standard UE character mesh forward offset
		PreviewMeshComponent->SetRelativeRotation(NewRot);
	}
}

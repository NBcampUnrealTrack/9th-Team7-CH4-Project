#include "Public/Map/LevelFloorBase.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "InstancedFoliageActor.h"

ALevelFloorBase::ALevelFloorBase()
{
    PrimaryActorTick.bCanEverTick = false;

    // [중요 1] LevelManager의 ArrangePlacedZones() 셔플 이동을 위해 Replicates는 유지합니다.
    bReplicates = true;
    
    // [중요 2] 서버와의 틱 단위 래플리케이션 이동 동기화는 끕니다.
    // LevelManager가 위치를 잡는데, 래플리케이션 무브먼트가 이중으로 덮어씌워 레그돌이 잠드는 것을 방지합니다.
    SetReplicateMovement(false);
    bAlwaysRelevant = true;

    // 루트 컴포넌트
    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComp);
    
    // 환경 충돌 영역
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(RootComponent);
    
    // [중요 3] SetCollisionObjectType(ECC_WorldStatic)을 제거하고 OverlapAllDynamic으로 고정하여 레그돌 뼈대와의 충돌을 완벽 차단합니다.
    CollisionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    CollisionBox->SetGenerateOverlapEvents(true);
}

void ALevelFloorBase::BeginPlay()
{
    Super::BeginPlay();

    // Existing environment Blueprints serialize the previous movement-replication
    // default. Their placement is already reproduced from LevelManager state.
    SetReplicateMovement(false);

    CollisionBox->OnComponentBeginOverlap.AddDynamic(
       this,
       &ALevelFloorBase::OnCollisionBoxBeginOverlap);
    
    CollisionBox->OnComponentEndOverlap.AddDynamic(
       this,
       &ALevelFloorBase::OnCollisionBoxEndOverlap);
}

void ALevelFloorBase::OnCollisionBoxBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor)
    {
       return;
    }

    ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
    if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
    {
       return;
    }

    // 1. 진입 시 숨길 태그 처리 (시각화 및 Collision 비활성화)
    for (const FName& Tag : BeginHideTargetTags)
    {
       if (Tag.IsNone()) continue;

       TArray<AActor*> HideActors;
       UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, HideActors);

       for (AActor* Actor : HideActors)
       {
          if (Actor)
          {
             Actor->SetActorHiddenInGame(true);
             Actor->SetActorEnableCollision(false); // [중요 4] 충돌체 꺼주기
          }
       }
    }

    // 2. 진입 시 보일 태그 처리 (시각화 및 Collision 활성화)
    for (const FName& Tag : BeginShowTargetTags)
    {
       if (Tag.IsNone()) continue;

       TArray<AActor*> ShowActors;
       UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, ShowActors);

       for (AActor* Actor : ShowActors)
       {
          if (Actor)
          {
             Actor->SetActorHiddenInGame(false);
             Actor->SetActorEnableCollision(true); // [중요 4] 충돌체 켜주기
          }
       }
    }
    
    // 3. 진입 시 인스턴스 폴리지 컴포넌트 숨김 처리
    for (const FName& Tag : BeginHideComponentTags)
    {
       if (Tag.IsNone()) continue;

       for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
       {
          AInstancedFoliageActor* FoliageActor = *It;

          if (FoliageActor)
          {
             TArray<UActorComponent*> Comps = FoliageActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), Tag);

             for (UActorComponent* Comp : Comps)
             {
                if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
                {
                   PrimComp->SetVisibility(false, true);
                   PrimComp->SetHiddenInGame(true, true);
                   PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // [중요 4] 충돌체 꺼주기
                }
             }
          }
       }
    }

    // 4. 진입 시 인스턴스 폴리지 컴포넌트 표시 처리
    for (const FName& Tag : BeginShowComponentTags)
    {
       if (Tag.IsNone()) continue;

       for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
       {
          AInstancedFoliageActor* FoliageActor = *It;

          if (FoliageActor)
          {
             TArray<UActorComponent*> Comps = FoliageActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), Tag);

             for (UActorComponent* Comp : Comps)
             {
                if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
                {
                   PrimComp->SetVisibility(true, true);
                   PrimComp->SetHiddenInGame(false, true);
                   PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // [중요 4] 충돌체 켜주기
                }
             }
          }
       }
    }
}

void ALevelFloorBase::OnCollisionBoxEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (!OtherActor)
    {
       return;
    }

    ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
    if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
    {
       return;
    }

    // 1. 이탈 시 숨길 태그 처리
    for (const FName& Tag : EndHideTargetTags)
    {
       if (Tag.IsNone()) continue;

       TArray<AActor*> HideActors;
       UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, HideActors);

       for (AActor* Actor : HideActors)
       {
          if (Actor)
          {
             Actor->SetActorHiddenInGame(true);
             Actor->SetActorEnableCollision(false);
          }
       }
    }

    // 2. 이탈 시 보일 태그 처리
    for (const FName& Tag : EndShowTargetTags)
    {
       if (Tag.IsNone()) continue;

       TArray<AActor*> ShowActors;
       UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, ShowActors);

       for (AActor* Actor : ShowActors)
       {
          if (Actor)
          {
             Actor->SetActorHiddenInGame(false);
             Actor->SetActorEnableCollision(true);
          }
       }
    }
    
    // 3. 이탈 시 인스턴스 폴리지 컴포넌트 숨김 처리
    for (const FName& Tag : EndHideComponentTags)
    {
       if (Tag.IsNone()) continue;

       for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
       {
          AInstancedFoliageActor* FoliageActor = *It;

          if (FoliageActor)
          {
             TArray<UActorComponent*> Comps = FoliageActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), Tag);

             for (UActorComponent* Comp : Comps)
             {
                if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
                {
                   PrimComp->SetVisibility(false, true);
                   PrimComp->SetHiddenInGame(true, true);
                   PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
             }
          }
       }
    }

    // 4. 이탈 시 인스턴스 폴리지 컴포넌트 표시 처리
    for (const FName& Tag : EndShowComponentTags)
    {
       if (Tag.IsNone()) continue;

       for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
       {
          AInstancedFoliageActor* FoliageActor = *It;

          if (FoliageActor)
          {
             TArray<UActorComponent*> Comps = FoliageActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), Tag);

             for (UActorComponent* Comp : Comps)
             {
                if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
                {
                   PrimComp->SetVisibility(true, true);
                   PrimComp->SetHiddenInGame(false, true);
                   PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                }
             }
          }
       }
    }
}

void ALevelFloorBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
}

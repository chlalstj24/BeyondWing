#include "GridTile.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AGridTile::AGridTile()
{
	PrimaryActorTick.bCanEverTick = true; 

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	
	// [개선] 히트박스를 타일 크기보다 아주 살짝 크게(120 -> 125) 설정하여 틈새 클릭 방지
	CollisionBox->SetBoxExtent(FVector(62.5f, 62.5f, 10.0f)); 
	
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	TileMesh->SetupAttachment(RootComponent);
	TileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	CurrentState = ETileState::None;
	DefaultZoneState = ETileState::None;
	DynamicMaterial = nullptr;
}

void AGridTile::BeginPlay()
{
	Super::BeginPlay();

	if (TileMesh)
	{
		DynamicMaterial = TileMesh->CreateDynamicMaterialInstance(0);
	}
}

void AGridTile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsAnimating)
	{
		AnimationTimer += DeltaTime;
		float Alpha = FMath::Clamp(AnimationTimer / AnimationDuration, 0.0f, 1.0f);
		
		// 3차 함수 기반 Ease-Out (이미지처럼 바닥에 부드럽게 안착)
		float CubicAlpha = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
		FVector NewLoc = FMath::Lerp(StartLocation, TargetLocation, CubicAlpha);
		SetActorLocation(NewLoc);

		if (Alpha >= 1.0f)
		{
			bIsAnimating = false;
		}
	}
}

void AGridTile::StartLandingAnimation(FVector TargetLoc, float Duration)
{
	TargetLocation = TargetLoc;
	StartLocation = GetActorLocation();
	AnimationDuration = Duration;
	AnimationTimer = 0.0f;
	bIsAnimating = true;
}

void AGridTile::SetTileState(ETileState NewState)
{
	CurrentState = NewState;
	if (!DynamicMaterial) return;

	ETileState ActiveState = (CurrentState == ETileState::None) ? DefaultZoneState : CurrentState;
	FLinearColor Color = FLinearColor::Transparent;

	switch (ActiveState)
	{
	case ETileState::Hovered:     Color = FLinearColor::Yellow; break;
	case ETileState::MoveRange:   Color = FLinearColor::Blue; break;
	case ETileState::AttackRange: Color = FLinearColor(0.0f, 0.65f, 1.0f); break; // [수정] 내 공격 사거리 (하늘색, 적 구역 빨강과 구분)
	case ETileState::EnemyAttackRange: Color = FLinearColor(0.8f, 0.0f, 1.0f); break; // 보라색 (가시성 확보)
	case ETileState::AttackTarget: Color = FLinearColor(1.0f, 0.6f, 0.0f); break; // [수정] 합을 걸 수 있는 적 (금색, 고르는 중에는 사거리 칸이 적 경로를 덮는다)
	case ETileState::AllyZone:    Color = FLinearColor(0.0f, 0.2f, 1.0f); break;
	case ETileState::EnemyZone:   Color = FLinearColor(1.0f, 0.02f, 0.02f); break;
	case ETileState::Wall:        Color = FLinearColor(0.12f, 0.12f, 0.12f); break; // [추가] 장애물 벽
	case ETileState::EnemyMovePath: Color = FLinearColor(1.0f, 0.5f, 0.05f); break;   // [추가] 적 돌진 경로 (주황)
	case ETileState::EnemyUnit:   Color = FLinearColor(1.0f, 0.08f, 0.15f); break;    // [추가] 적 발밑 (빨강)
	case ETileState::PlayerUnit:  Color = FLinearColor(0.1f, 0.9f, 0.6f); break;      // [추가] 내 발밑 (청록)
	default:                      Color = FLinearColor::Transparent; break;
	}
    
	// EmissiveColor 파라미터 업데이트 (블루프린트 머티리얼 설정 확인 필요)
	DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 5.0f);
}
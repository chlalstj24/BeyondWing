#include "GridTile.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AGridTile::AGridTile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
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
	case ETileState::AttackRange: Color = FLinearColor::Red; break;
	case ETileState::AllyZone:    Color = FLinearColor(0.0f, 0.2f, 1.0f); break;
	case ETileState::EnemyZone:   Color = FLinearColor(1.0f, 0.02f, 0.02f); break;
	default:                      Color = FLinearColor::Transparent; break;
	}
    
	DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 5.0f);
}
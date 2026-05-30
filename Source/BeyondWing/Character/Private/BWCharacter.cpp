#include "BWCharacter.h"
#include "DeckComponent.h"

ABWCharacter::ABWCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	OwnGridCoordinate = FIntPoint(2, 4);
	CharacterClass = EClassType::PHA; 

	PHA = 5;
	INT = 5;
	FTH = 5;
	TEC = 5;
	MaxHP = 100;
	CurrentHP = MaxHP;

	DeckComponent = CreateDefaultSubobject<UDeckComponent>(TEXT("DeckComponent"));
}

void ABWCharacter::BeginPlay()
{
	Super::BeginPlay();
}

int32 ABWCharacter::CalculateFinalCardDamage(const FCardData& CardInfo, bool& OutIsCritical)
{
	float StatValue = 0.0f;

	// 1. 카드가 요구하는 스펙에 맞춰 캐릭터의 순수 스탯 추출
	switch (CardInfo.ScalingStat)
	{
	case EClassType::PHA: StatValue = static_cast<float>(PHA); break;
	case EClassType::INT: StatValue = static_cast<float>(INT); break;
	case EClassType::FTH: StatValue = static_cast<float>(FTH); break;
	case EClassType::TEC: StatValue = static_cast<float>(TEC); break;
	case EClassType::HP:  StatValue = static_cast<float>(MaxHP); break;
	default: break;
	}

	// 2. 기본 피해량 + (스탯 * 계수) 최종 합산
	float FinalDamage = CardInfo.BaseDamage + (StatValue * CardInfo.DamageScale);

	// 3. TEC 기반 치명타 확률 계산 기획 반영 (TEC 20당 치명타 확률 10% 추가, 기본 0%)
	float CritChance = (static_cast<float>(TEC) / 20.0f) * 10.0f;
	CritChance = FMath::Clamp(CritChance, 0.0f, 100.0f); // 최대 100% 한계치 제한

	// 4. TEC 기반 치명타 피해량 계산 기획 반영 (TEC 20당 치명타 피해 30% 추가, 기본 150%)
	float CritMultiplier = 1.5f + ((static_cast<float>(TEC) / 20.0f) * 0.3f);

	// 난수 연산을 통한 치명타 확정 판정
	float RandomRoll = FMath::FRandRange(0.0f, 100.0f);
	if (RandomRoll <= CritChance)
	{
		OutIsCritical = true;
		FinalDamage *= CritMultiplier;
	}
	else
	{
		OutIsCritical = false;
	}

	return FMath::Max(0, FMath::RoundToInt(FinalDamage));
}
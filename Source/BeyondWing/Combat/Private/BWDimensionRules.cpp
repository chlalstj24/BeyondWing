#include "BWDimensionRules.h"

FBWBattlefieldState UBWDimensionRules::MakeBattlefield(EDimensionType Dimension, bool bOverloaded)
{
	FBWBattlefieldState State;
	State.Dimension = Dimension;
	State.bOverloaded = bOverloaded && Dimension != EDimensionType::None;
	if (Dimension == EDimensionType::Taiga)
	{
		State.TaigaChargesLeft = TaigaFreeHits;
		State.TaigaEnemyChargesLeft = State.bOverloaded ? TaigaOverloadEnemyHits : 0;
	}
	return State;
}

float UBWDimensionRules::RollPatrovasMultiplier(FBWBattlefieldState& State, FRandomStream& Stream)
{
	if (State.Dimension != EDimensionType::Patrovas)
	{
		State.PatrovasMaxMultiplier = 1.0f;
		return State.PatrovasMaxMultiplier;
	}

	const float GrowChance = State.bOverloaded ? PatrovasGrowChanceOverloaded : PatrovasGrowChance;
	const bool bGrow = Stream.GetFraction() < GrowChance;
	State.PatrovasMaxMultiplier = bGrow
		? Stream.FRandRange(1.0f, PatrovasMaxMultiplier)
		: Stream.FRandRange(PatrovasMinMultiplier, 1.0f);
	return State.PatrovasMaxMultiplier;
}

FBWDiceRange UBWDimensionRules::ScaleSkillMax(const FBWDiceRange& Range, float Multiplier)
{
	return UBWDuelRules::ScaleRangeMax(Range, Multiplier);
}

FBWDuelSide UBWDimensionRules::ApplyToPlayerSide(const FBWBattlefieldState& State, const FBWDuelSide& Side)
{
	FBWDuelSide Result = Side;
	// 방치 보너스 피해량은 겔레온 기믹과 더한다 (겔레온 +50% + 보너스 +50% = ×2.0)
	if (State.Dimension != EDimensionType::Geleon && State.BonusDamage > 0.0f) Result.DamageMultiplier *= 1.0f + State.BonusDamage;
	switch (State.Dimension)
	{
	case EDimensionType::Patrovas:
		// 내 스킬만 흔들린다 (속도와 적 스킬은 그대로)
		Result.SkillRange = ScaleSkillMax(Result.SkillRange, State.PatrovasMaxMultiplier);
		break;
	case EDimensionType::Geleon:
		Result.DamageMultiplier *= GeleonDamageMultiplier + State.BonusDamage;
		if (State.bOverloaded) Result.IncomingDamageMultiplier *= GeleonOverloadIncomingMultiplier;
		break;
	case EDimensionType::Taiga:
		if (State.TaigaChargesLeft > 0) Result.CritChance = 1.0f; // 최초 3회 공격은 치명타
		break;
	default:
		break;
	}
	return Result;
}

bool UBWDimensionRules::CanUseCard(const FBWBattlefieldState& State, const FBWCardData& Card)
{
	if (State.Dimension != EDimensionType::Geleon) return true;
	if (Card.CardKind == ECardKind::Effect) return State.bGeleonUseEffects;
	return !State.bGeleonUseEffects || State.GeleonAttackCardName.IsEmpty() || State.GeleonAttackCardName == Card.CardName;
}

void UBWDimensionRules::NoteAttackCardUsed(FBWBattlefieldState& State, const FBWCardData& Card)
{
	if (State.Dimension == EDimensionType::Geleon && State.bGeleonUseEffects && State.GeleonAttackCardName.IsEmpty() && Card.CardKind == ECardKind::Attack)
	{
		State.GeleonAttackCardName = Card.CardName;
	}
}

bool UBWDimensionRules::ConsumeTaigaCharge(FBWBattlefieldState& State)
{
	if (State.Dimension != EDimensionType::Taiga || State.TaigaChargesLeft <= 0) return false;
	--State.TaigaChargesLeft;
	return true;
}

bool UBWDimensionRules::ConsumeTaigaEnemyCharge(FBWBattlefieldState& State)
{
	if (State.Dimension != EDimensionType::Taiga || State.TaigaEnemyChargesLeft <= 0) return false;
	--State.TaigaEnemyChargesLeft;
	return true;
}

void UBWDimensionRules::GetWallSlamRule(const FBWBattlefieldState& State, bool bPusherIsPlayer, bool& bOutExtraDamage, bool& bOutStun)
{
	const bool bBerend = State.Dimension == EDimensionType::Berend;
	bOutExtraDamage = bBerend && (bPusherIsPlayer || State.bOverloaded);
	bOutStun = bBerend && bPusherIsPlayer;
}

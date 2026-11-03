#include "BWDimensionRules.h"

FBWBattlefieldState UBWDimensionRules::MakeBattlefield(EDimensionType Dimension, bool bOverloaded)
{
	FBWBattlefieldState State;
	State.Dimension = Dimension;
	State.bOverloaded = bOverloaded && Dimension != EDimensionType::None;
	if (Dimension == EDimensionType::Taiga)
	{
		// 은신은 맞닿은 적 없이 턴을 끝내야 생긴다 (컨트롤러 RequestEndTurn)
		State.TaigaEnemyChargesLeft = State.bOverloaded ? TaigaOverloadEnemyHits : 0;
	}
	return State;
}

void UBWDimensionRules::RollManaRows(FBWBattlefieldState& State, int32 RowCount, FRandomStream& Stream)
{
	State.ManaRows.Reset();
	if (State.Dimension != EDimensionType::Patrovas || RowCount <= 0) return;
	while (State.ManaRows.Num() < FMath::Min(ManaLineCount, RowCount)) State.ManaRows.AddUnique(Stream.RandRange(0, RowCount - 1));
}

bool UBWDimensionRules::IsOnManaLine(const FBWBattlefieldState& State, FIntPoint Coord)
{
	return State.Dimension == EDimensionType::Patrovas && State.ManaRows.Contains(Coord.X);
}

float UBWDimensionRules::GetGeleonAdvanceMultiplier(const FBWBattlefieldState& State, int32 Advance)
{
	if (State.Dimension != EDimensionType::Geleon) return 1.0f;
	return 1.0f + FMath::Min(GeleonMaxAdvanceBonus, GeleonPerAdvance * FMath::Max(0, Advance));
}

FBWDiceRange UBWDimensionRules::ScaleSkillMax(const FBWDiceRange& Range, float Multiplier)
{
	return UBWDuelRules::ScaleRangeMax(Range, Multiplier);
}

FBWDuelSide UBWDimensionRules::ApplyToPlayerSide(const FBWBattlefieldState& State, const FBWDuelSide& Side, int32 Advance, bool bOnManaLine)
{
	FBWDuelSide Result = Side;
	if (State.BonusDamage > 0.0f) Result.DamageMultiplier *= 1.0f + State.BonusDamage;
	switch (State.Dimension)
	{
	case EDimensionType::Patrovas:
		if (bOnManaLine) Result.SkillRange = ScaleSkillMax(Result.SkillRange, ManaLineMaxMultiplier);
		break;
	case EDimensionType::Berend:
		Result.bPushesTarget = true;
		break;
	case EDimensionType::Geleon:
		Result.DamageMultiplier *= GetGeleonAdvanceMultiplier(State, Advance);
		if (State.bOverloaded) Result.IncomingDamageMultiplier *= GeleonOverloadIncomingMultiplier;
		break;
	case EDimensionType::Taiga:
		if (State.TaigaChargesLeft > 0)
		{
			Result.CritChance = 1.0f; // 은신: 다음 공격은 치명타, 방어 카드로 깎이지 않는다
			Result.bIgnoreGuard = true;
		}
		break;
	default:
		break;
	}
	return Result;
}

FBWDuelSide UBWDimensionRules::ApplyToEnemySide(const FBWBattlefieldState& State, const FBWDuelSide& Side, int32 Advance, bool bOnManaLine)
{
	FBWDuelSide Result = Side;
	if (bOnManaLine) Result.SkillRange = ScaleSkillMax(Result.SkillRange, ManaLineMaxMultiplier);
	if (State.Dimension == EDimensionType::Berend && !Result.bGuard) Result.bPushesTarget = true;
	Result.DamageMultiplier *= GetGeleonAdvanceMultiplier(State, Advance);
	return Result;
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
	bOutExtraDamage = bBerend;
	bOutStun = bBerend && bPusherIsPlayer;
}

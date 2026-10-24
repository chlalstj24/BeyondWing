#include "BeyondWing/Core/Public/BWMapRules.h"

TArray<EDimensionType> UBWMapRules::MakeRowOrder(int32 Act, bool bShuffleAfterFirstAct, FRandomStream& Stream)
{
	TArray<EDimensionType> Rows = { EDimensionType::Patrovas, EDimensionType::Berend, EDimensionType::Geleon, EDimensionType::Taiga };
	if (Act > 1 && bShuffleAfterFirstAct)
	{
		for (int32 i = Rows.Num() - 1; i > 0; --i) Rows.Swap(i, Stream.RandRange(0, i));
	}
	return Rows;
}

bool UBWMapRules::GetFixedNodeType(int32 Step, EMapNodeType& OutType)
{
	switch (Step)
	{
	case FirstBattleStep: OutType = EMapNodeType::Battle; return true;
	case TreasureStep:    OutType = EMapNodeType::Chest;  return true;
	case RestStep:        OutType = EMapNodeType::Rest;   return true;
	case BossStep:        OutType = EMapNodeType::Boss;   return true;
	default:              return false;
	}
}

void UBWMapRules::GetNodeWeights(EDimensionType Dimension, TArray<TPair<EMapNodeType, int32>>& OutWeights)
{
	// 4장 기본 비중
	int32 Battle = 43, Mystery = 20, Elite = 10, Rest = 10, Shop = 8, Chest = 5, Altar = 2, Rift = 2;

	// 5장 차원별 노드 성향
	switch (Dimension)
	{
	case EDimensionType::Patrovas: Mystery += 5; Battle -= 5; break;
	case EDimensionType::Berend:   Battle += 5;  Rest -= 5;   break;
	case EDimensionType::Geleon:   Shop += 4;    Mystery -= 4; break;
	case EDimensionType::Taiga:    Elite += 5;   Battle -= 5; break;
	default: break;
	}

	OutWeights.Reset();
	OutWeights.Add({ EMapNodeType::Battle, Battle });
	OutWeights.Add({ EMapNodeType::Mystery, Mystery });
	OutWeights.Add({ EMapNodeType::Elite, Elite });
	OutWeights.Add({ EMapNodeType::Rest, Rest });
	OutWeights.Add({ EMapNodeType::Shop, Shop });
	OutWeights.Add({ EMapNodeType::Chest, Chest });
	OutWeights.Add({ EMapNodeType::DimensionAltar, Altar });
	OutWeights.Add({ EMapNodeType::Rift, Rift });
}

EMapNodeType UBWMapRules::PickNodeType(EDimensionType Dimension, FRandomStream& Stream)
{
	TArray<TPair<EMapNodeType, int32>> Weights;
	GetNodeWeights(Dimension, Weights);
	int32 Total = 0;
	for (const TPair<EMapNodeType, int32>& W : Weights) Total += W.Value;

	int32 Roll = Stream.RandRange(0, Total - 1);
	for (const TPair<EMapNodeType, int32>& W : Weights)
	{
		if (Roll < W.Value) return W.Key;
		Roll -= W.Value;
	}
	return EMapNodeType::Battle;
}

TArray<FMapStep> UBWMapRules::GenerateAct(const TArray<EDimensionType>& RowDimensions, FRandomStream& Stream)
{
	TArray<FMapStep> Map;
	Map.SetNum(BossStep + 1);

	// 1. 노드: 고정 칸은 전 줄 같은 노드, 나머지는 비중으로
	TArray<FIntPoint> RandomCells; // (칸, 줄)
	for (int32 Step = 0; Step < StepsPerAct; ++Step)
	{
		EMapNodeType FixedType;
		const bool bFixed = GetFixedNodeType(Step, FixedType);
		for (int32 Row = 0; Row < NumRows; ++Row)
		{
			FMapNode Node;
			Node.StepIdx = Step;
			Node.DimIdx = Row;
			Node.Dimension = RowDimensions.IsValidIndex(Row) ? RowDimensions[Row] : EDimensionType::None;
			Node.NodeType = bFixed ? FixedType : PickNodeType(Node.Dimension, Stream);
			if (!bFixed) RandomCells.Add(FIntPoint(Step, Row));
			Map[Step].Nodes.Add(Node);
		}
	}

	// 2. 균열은 막당 1~2개
	TArray<FIntPoint> Rifts;
	for (const FIntPoint& Cell : RandomCells)
	{
		if (Map[Cell.X].Nodes[Cell.Y].NodeType == EMapNodeType::Rift) Rifts.Add(Cell);
	}
	while (Rifts.Num() > MaxRiftsPerAct)
	{
		const FIntPoint Cell = Rifts[Stream.RandRange(0, Rifts.Num() - 1)];
		Map[Cell.X].Nodes[Cell.Y].NodeType = EMapNodeType::Battle;
		Rifts.Remove(Cell);
	}
	if (Rifts.Num() < MinRiftsPerAct)
	{
		TArray<FIntPoint> Battles;
		for (const FIntPoint& Cell : RandomCells)
		{
			if (Map[Cell.X].Nodes[Cell.Y].NodeType == EMapNodeType::Battle) Battles.Add(Cell);
		}
		const TArray<FIntPoint>& Pool = Battles.Num() > 0 ? Battles : RandomCells;
		const FIntPoint Cell = Pool[Stream.RandRange(0, Pool.Num() - 1)];
		Map[Cell.X].Nodes[Cell.Y].NodeType = EMapNodeType::Rift;
	}

	// 3. 차원문: 칸 사이 경계마다 70% 열림, 같은 경계에서 3칸 연속 닫힘은 만들지 않는다
	int32 ClosedRun[NumRows - 1] = { 0, 0, 0 };
	for (int32 Step = 0; Step < StepsPerAct - 1; ++Step)
	{
		Map[Step].GateOpenBelow.SetNum(NumRows - 1);
		for (int32 Boundary = 0; Boundary < NumRows - 1; ++Boundary)
		{
			const bool bOpen = ClosedRun[Boundary] >= MaxClosedInARow || Stream.GetFraction() < GateOpenChance;
			Map[Step].GateOpenBelow[Boundary] = bOpen;
			ClosedRun[Boundary] = bOpen ? 0 : ClosedRun[Boundary] + 1;
		}
	}

	// 4. 보스 칸: 네 줄이 하나로 합류 (차원은 마지막에 서 있던 줄을 따른다)
	FMapNode Boss;
	Boss.StepIdx = BossStep;
	Boss.DimIdx = 0;
	Boss.NodeType = EMapNodeType::Boss;
	Map[BossStep].Nodes.Add(Boss);
	return Map;
}

bool UBWMapRules::IsGateOpen(const TArray<FMapStep>& Map, int32 FromStep, int32 RowA, int32 RowB)
{
	if (FMath::Abs(RowA - RowB) != 1 || !Map.IsValidIndex(FromStep)) return false;
	const int32 Boundary = FMath::Min(RowA, RowB);
	const TArray<bool>& Gates = Map[FromStep].GateOpenBelow;
	return Gates.IsValidIndex(Boundary) && Gates[Boundary];
}

bool UBWMapRules::CanMove(const TArray<FMapStep>& Map, int32 FromStep, int32 FromRow, int32 ToStep, int32 ToRow, bool bRiftJumpReady)
{
	if (ToStep != FromStep + 1 || !Map.IsValidIndex(ToStep) || !Map[ToStep].Nodes.IsValidIndex(ToRow)) return false;
	if (FromStep < 0) return true;                 // 막 시작: 원하는 줄에서 시작
	if (ToStep == BossStep) return true;            // 보스 칸은 네 줄이 합류
	const int32 Distance = FMath::Abs(FromRow - ToRow);
	if (Distance == 0) return true;
	if (Distance == 1) return IsGateOpen(Map, FromStep, FromRow, ToRow);
	return Distance == 2 && bRiftJumpReady;         // 지도 끝은 순환하지 않는다
}

int32 UBWMapRules::GetNeglectCondition(EDimensionType Dimension)
{
	return Dimension == EDimensionType::Taiga ? 2 : 3;
}

int32 UBWMapRules::GetOverloadLevel(int32 Consecutive)
{
	if (Consecutive >= OverloadIIStart) return 2;
	if (Consecutive >= OverloadIStart) return 1;
	return 0;
}

int32 UBWMapRules::GetResistancePercent(int32 ClaimNumber)
{
	if (ClaimNumber <= 1) return 100;
	if (ClaimNumber == 2) return 75;
	return 50;
}

FBWNeglectBonus UBWMapRules::MakeNeglectBonus(EDimensionType Dimension, int32 ClaimNumber, bool bDeep)
{
	FBWNeglectBonus Bonus;
	Bonus.Dimension = Dimension;
	Bonus.Percent = GetResistancePercent(ClaimNumber);
	Bonus.bDeep = bDeep;
	Bonus.Gold = bDeep ? DeepNeglectGold : 0;

	// 7-2 표: 1회째 / 2회째 / 3회째 이상
	const int32 Tier = FMath::Clamp(ClaimNumber, 1, 3);
	switch (Dimension)
	{
	case EDimensionType::Patrovas:
		Bonus.UpgradedCardChoices = Tier == 1 ? 3 : 1;
		Bonus.NormalCardChoices = Tier == 1 ? 0 : (Tier == 2 ? 2 : 1);
		break;
	case EDimensionType::Berend:
		Bonus.VictoryHealFraction = Tier == 1 ? 0.2f : (Tier == 2 ? 0.15f : 0.1f);
		break;
	case EDimensionType::Geleon:
		Bonus.NextBattleDamageBonus = Tier == 1 ? 0.5f : (Tier == 2 ? 0.37f : 0.25f);
		break;
	case EDimensionType::Taiga:
		Bonus.CardRemovals = Tier == 1 ? 2 : 1;
		Bonus.EquipmentGrade = Tier == 3 ? 1 : 2;
		break;
	default:
		break;
	}
	return Bonus;
}

void UBWMapRules::AdvanceNeglect(TMap<EDimensionType, int32>& Neglected, EDimensionType Arrived)
{
	for (TPair<EDimensionType, int32>& Pair : Neglected)
	{
		Pair.Value = Pair.Key == Arrived ? 0 : FMath::Min(Pair.Value + 1, GetNeglectCap(Pair.Key));
	}
}

FBWCardData UBWMapRules::MakeUpgradedCard(const FBWCardData& Card)
{
	FBWCardData Up = Card;
	if (Card.CardName.EndsWith(TEXT("+"))) return Up;
	Up.CardName = Card.CardName + TEXT("+");
	if (Card.CardKind == ECardKind::Attack)
	{
		Up.SkillMin += 1;
		Up.SkillMax += 2;
	}
	else
	{
		auto Bump = [](int32& Value, int32 Amount) { if (Value > 0) Value += Amount; };
		Bump(Up.EffectHeal, 3);
		Bump(Up.EffectBlock, 3);
		Bump(Up.EffectMove, 1);
		Bump(Up.EffectMP, 1);
		Bump(Up.EffectDraw, 1);
		Bump(Up.EffectAttackMaxBonus, 1);
		if (Up.SelfStatus != EStatusEffect::None) Up.SelfStatusDuration += 1;
	}
	return Up;
}

FText UBWMapRules::GetNodeTypeName(EMapNodeType Type)
{
	switch (Type)
	{
	case EMapNodeType::Battle:         return FText::FromString(TEXT("전투"));
	case EMapNodeType::Elite:          return FText::FromString(TEXT("엘리트"));
	case EMapNodeType::Mystery:        return FText::FromString(TEXT("이벤트"));
	case EMapNodeType::Rest:           return FText::FromString(TEXT("휴식"));
	case EMapNodeType::Shop:           return FText::FromString(TEXT("상점"));
	case EMapNodeType::Chest:          return FText::FromString(TEXT("보물"));
	case EMapNodeType::Boss:           return FText::FromString(TEXT("보스"));
	case EMapNodeType::DimensionAltar: return FText::FromString(TEXT("차원 제단"));
	case EMapNodeType::Rift:           return FText::FromString(TEXT("균열"));
	default:                           return FText::GetEmpty();
	}
}

FText UBWMapRules::GetDimensionName(EDimensionType Dimension)
{
	switch (Dimension)
	{
	case EDimensionType::Patrovas: return FText::FromString(TEXT("파트로바스"));
	case EDimensionType::Berend:   return FText::FromString(TEXT("베렌드"));
	case EDimensionType::Geleon:   return FText::FromString(TEXT("겔레온"));
	case EDimensionType::Taiga:    return FText::FromString(TEXT("타이가"));
	default:                       return FText::FromString(TEXT("차원 없음"));
	}
}

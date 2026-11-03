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

EMapNodeType UBWMapRules::PickNodeType(EDimensionType Dimension, FRandomStream& Stream, bool bAllowRest)
{
	TArray<TPair<EMapNodeType, int32>> Weights;
	GetNodeWeights(Dimension, Weights);
	if (!bAllowRest) Weights.RemoveAll([](const TPair<EMapNodeType, int32>& W) { return W.Key == EMapNodeType::Rest; });
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

	// 1-2. 휴식 제한 (줄마다): 무작위 휴식은 MaxRandomRestsPerRow개까지, 휴식 바로 다음 칸에 또 휴식 없음,
	// 15칸 고정 휴식 바로 앞(14칸)에도 없음. 넘치는 휴식은 휴식을 뺀 비중으로 다시 뽑는다
	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		int32 Rests = 0;
		bool bPrevRest = false;
		for (int32 Step = 0; Step < StepsPerAct; ++Step)
		{
			FMapNode& Node = Map[Step].Nodes[Row];
			EMapNodeType FixedType;
			if (!GetFixedNodeType(Step, FixedType) && Node.NodeType == EMapNodeType::Rest)
			{
				if (Rests >= MaxRandomRestsPerRow || bPrevRest || Step == RestStep - 1)
					Node.NodeType = PickNodeType(Node.Dimension, Stream, false);
				else
					++Rests;
			}
			bPrevRest = Node.NodeType == EMapNodeType::Rest;
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

namespace BWUpgrade
{
	// 설명 글에서 "앞말 + 옛 숫자 + 뒷말"을 찾아 새 숫자로 바꾼다. 숫자가 이어 붙은 곳(10의 1 등)은 건너뛴다
	static bool ReplaceNumber(FString& Text, const FString& Before, int32 Old, int32 New, const FString& After)
	{
		const FString From = Before + FString::FromInt(Old) + After;
		int32 Pos = 0;
		while ((Pos = Text.Find(From, ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos)) != INDEX_NONE)
		{
			const int32 End = Pos + From.Len();
			const bool bDigitAfter = After.IsEmpty() && End < Text.Len() && FChar::IsDigit(Text[End]);
			const bool bDigitBefore = Before.IsEmpty() && Pos > 0 && FChar::IsDigit(Text[Pos - 1]);
			if (!bDigitAfter && !bDigitBefore)
			{
				Text = Text.Left(Pos) + Before + FString::FromInt(New) + After + Text.Mid(End);
				return true;
			}
			Pos = End;
		}
		return false;
	}

	struct FPattern { const TCHAR* Before; const TCHAR* After; };

	// 수치를 올리고 설명의 숫자도 바꾼다. 설명에 그 숫자가 없으면 Fallback 문장을 덧붙인다
	static void Bump(FString& Desc, int32& Value, int32 Amount, std::initializer_list<FPattern> Patterns, const TCHAR* FallbackFormat)
	{
		if (Value <= 0 || Amount == 0) return;
		const int32 Old = Value;
		Value += Amount;
		for (const FPattern& P : Patterns)
		{
			if (ReplaceNumber(Desc, P.Before, Old, Value, P.After)) return;
		}
		// FallbackFormat의 %d = 새 값, %% = %
		const FString Sentence = FString(FallbackFormat).Replace(TEXT("%d"), *FString::FromInt(Value)).Replace(TEXT("%%"), TEXT("%"));
		Desc = FString::Printf(TEXT("%s %s"), *Desc.TrimEnd(), *Sentence);
	}

	// 확률(0~1)은 % 정수로 바꿔서 같은 방식으로
	static void BumpPercent(FString& Desc, float& Value, int32 AmountPercent, std::initializer_list<FPattern> Patterns, const TCHAR* FallbackFormat)
	{
		if (Value <= 0.0f || Value >= 1.0f) return;
		int32 Percent = FMath::RoundToInt(Value * 100.0f);
		Bump(Desc, Percent, AmountPercent, Patterns, FallbackFormat);
		Value = FMath::Min(1.0f, Percent / 100.0f);
	}

	static const TCHAR* StatusName(EStatusEffect Status)
	{
		switch (Status)
		{
		case EStatusEffect::Stun:       return TEXT("기절");
		case EStatusEffect::Slow:       return TEXT("둔화");
		case EStatusEffect::Weak:       return TEXT("약화");
		case EStatusEffect::Vulnerable: return TEXT("취약");
		case EStatusEffect::Bleed:      return TEXT("출혈");
		case EStatusEffect::Confusion:  return TEXT("혼란");
		case EStatusEffect::Focus:      return TEXT("집중");
		default:                        return TEXT("");
		}
	}
}

FBWCardData UBWMapRules::MakeUpgradedCard(const FBWCardData& Card)
{
	using namespace BWUpgrade;
	FBWCardData Up = Card;
	if (Card.CardName.EndsWith(TEXT("+"))) return Up;
	Up.CardName = Card.CardName + TEXT("+");
	if (Card.CardKind == ECardKind::Attack)
	{
		// 공격카드는 숫자 범위만 오른다 (카드 왼쪽 위 숫자에 그대로 보인다)
		Up.SkillMin += 1;
		Up.SkillMax += 2;
		return Up;
	}

	// 효과카드: 카드에 있는 수치를 모두 올리고, 설명의 숫자도 같이 바꾼다
	FString& D = Up.Description;
	Bump(D, Up.EffectHeal, 3, { { TEXT("HP "), TEXT("") }, { TEXT("HP를 "), TEXT("") }, { TEXT("[치유] "), TEXT("") }, { TEXT("회복 "), TEXT("") } }, TEXT("회복 %d."));
	Bump(D, Up.EffectBlock, 3, { { TEXT("방어도 "), TEXT("") }, { TEXT("방어도를 "), TEXT("") } }, TEXT("방어도 %d."));
	Bump(D, Up.EffectMove, 1, { { TEXT("이동 +"), TEXT("") } }, TEXT("이동 +%d."));
	Bump(D, Up.EffectMP, 1, { { TEXT("MP +"), TEXT("") } }, TEXT("MP +%d."));
	Bump(D, Up.EffectDraw, 1, { { TEXT("카드 "), TEXT("장") } }, TEXT("카드 %d장 뽑기."));
	Bump(D, Up.EffectAttackMaxBonus, 1, { { TEXT("최댓값 +"), TEXT("") } }, TEXT("공격 최댓값 +%d."));
	Bump(D, Up.EffectCounter, 2, { { TEXT("반격 "), TEXT("") } }, TEXT("반격 %d."));
	Bump(D, Up.EffectDefenseBonus, 1, { { TEXT("방어 +"), TEXT("") } }, TEXT("이번 적 턴 방어 +%d."));
	Bump(D, Up.EffectDark, 1, { { TEXT("어둠 +"), TEXT("") } }, TEXT("어둠 +%d."));
	BumpPercent(D, Up.EffectEvadeBonus, 10, { { TEXT("회피 +"), TEXT("%") } }, TEXT("이번 적 턴 회피 +%d%%."));
	BumpPercent(D, Up.CritChanceBonus, 15, { { TEXT("치명타 +"), TEXT("%") } }, TEXT("이번 턴 치명타 +%d%%."));
	if (Up.Special == EBWCardSpecial::Chant) Bump(D, Up.SpecialValue, 2, { { TEXT("최댓값 +"), TEXT("") } }, TEXT("최댓값 +%d."));
	if (Up.SelfStatus != EStatusEffect::None)
	{
		const FString Name = StatusName(Up.SelfStatus);
		Bump(D, Up.SelfStatusDuration, 1, { { *(Name + TEXT(" ")), TEXT("턴") } }, *(Name + TEXT(" %d턴.")));
	}

	// 오를 수치가 없는 카드(장전·강림·질풍노도 등): MP가 들면 1 줄이고, 아니면 카드 1장을 더 뽑는다
	const bool bChanged = Up.EffectHeal != Card.EffectHeal || Up.EffectBlock != Card.EffectBlock || Up.EffectMove != Card.EffectMove || Up.EffectMP != Card.EffectMP
		|| Up.EffectDraw != Card.EffectDraw || Up.EffectAttackMaxBonus != Card.EffectAttackMaxBonus || Up.EffectCounter != Card.EffectCounter
		|| Up.EffectDefenseBonus != Card.EffectDefenseBonus || Up.EffectDark != Card.EffectDark || Up.EffectEvadeBonus != Card.EffectEvadeBonus
		|| Up.CritChanceBonus != Card.CritChanceBonus || Up.SpecialValue != Card.SpecialValue || Up.SelfStatusDuration != Card.SelfStatusDuration;
	if (!bChanged)
	{
		if (Up.MPCost > 0) Up.MPCost -= 1;
		else
		{
			Up.EffectDraw = 1;
			D = FString::Printf(TEXT("%s 카드 1장 뽑기."), *D.TrimEnd());
		}
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
	default:                       return FText::FromString(TEXT("탑 내부"));
	}
}

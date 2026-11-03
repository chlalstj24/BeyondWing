#include "BWAugmentRules.h"

FBWAugmentTotals UBWAugmentRules::Aggregate(const TArray<FBWAugmentData>& Augments)
{
	FBWAugmentTotals T;
	for (const FBWAugmentData& A : Augments)
	{
		switch (A.Effect)
		{
		case EBWAugmentEffect::SpeedMaxUp:        T.SpeedMaxBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::SpeedMinUp:        T.SpeedMinBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::DuelReroll:        T.DuelRerollsPerBattle += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::StartBlock:        T.StartBlockBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::HandSizeUp:        T.HandSizeBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::EffectCardLimitUp: T.EffectLimitBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::AttackMaxUp:       T.AttackMaxMultiplier *= 1.0f + A.Value; break;
		case EBWAugmentEffect::CritChanceUp:      T.CritChanceBonus += A.Value; break;
		case EBWAugmentEffect::MPRegenUp:         T.MPPerTurnBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::ThornArmor:        T.ThornFraction += A.Value; break;
		case EBWAugmentEffect::ReversedHymn:      T.bReversedHymn = true; break;
		case EBWAugmentEffect::SelfBlessing:      T.bSelfBlessing = true; break;
		case EBWAugmentEffect::DoubleOpening:
			T.DuelLimitBonus += FMath::Max(1, FMath::RoundToInt(A.Value));
			T.AttackCardLimitBonus += FMath::Max(1, FMath::RoundToInt(A.Value));
			break;
		}
	}

	// [추가] 개편안 효과 합산 + 세트 보너스 (세트 효과는 누적)
	T.Stats.Init(0.0f, (int32)EBWAugStat::Count);
	T.Stats[(int32)EBWAugStat::DamageMult] = 1.0f;
	T.SetCounts.Init(0, (int32)EBWAugmentSet::Count);
	for (const FBWAugmentData& A : Augments)
	{
		for (const FBWAugmentMod& Mod : A.Mods)
		{
			float& Slot = T.Stats[(int32)Mod.Stat];
			Slot = Mod.Stat == EBWAugStat::DamageMult ? Slot * Mod.Value : Slot + Mod.Value;
		}
		if (A.Set != EBWAugmentSet::None) ++T.SetCounts[(int32)A.Set];
		if (A.Set == EBWAugmentSet::Lifestyle) T.LifestyleClass = A.ClassType;
	}
	for (int32 i = 1; i < (int32)EBWAugmentSet::Count; ++i) AddSetBonus(T, (EBWAugmentSet)i, T.SetCounts[i]);

	// 예전 증강 칸으로도 흘려 보내 기존 연결을 그대로 쓴다
	T.SpeedMaxBonus += T.GetInt(EBWAugStat::SpeedMax);
	T.SpeedMinBonus += T.GetInt(EBWAugStat::SpeedMin);
	T.DuelRerollsPerBattle += T.GetInt(EBWAugStat::Rerolls);
	T.StartBlockBonus += T.GetInt(EBWAugStat::StartBlock);
	T.HandSizeBonus += T.GetInt(EBWAugStat::FirstDraw);
	T.EffectLimitBonus += T.GetInt(EBWAugStat::EffectLimit);
	T.CritChanceBonus += T.Get(EBWAugStat::CritChance);
	T.MPPerTurnBonus += T.GetInt(EBWAugStat::MPPerTurn);
	T.ThornFraction += T.Get(EBWAugStat::Thorn);
	T.DuelLimitBonus += T.GetInt(EBWAugStat::DuelLimit);
	T.AttackCardLimitBonus += T.GetInt(EBWAugStat::AttackLimit);
	T.AttackMaxMultiplier *= 1.0f + T.Get(EBWAugStat::CardMaxPct);
	return T;
}

FString UBWAugmentRules::GetKindName(EBWAugmentKind Kind)
{
	switch (Kind)
	{
	case EBWAugmentKind::Change: return TEXT("변화");
	case EBWAugmentKind::Gamble: return TEXT("도박");
	default: return TEXT("안정");
	}
}

FString UBWAugmentRules::GetSetName(EBWAugmentSet Set)
{
	switch (Set)
	{
	case EBWAugmentSet::Astrin: return TEXT("아스트린");
	case EBWAugmentSet::Sylpira: return TEXT("실피라");
	case EBWAugmentSet::Soltheris: return TEXT("솔테리스");
	case EBWAugmentSet::Urska: return TEXT("우르스카");
	case EBWAugmentSet::Lirin: return TEXT("리린");
	case EBWAugmentSet::Lunaris: return TEXT("루나리스");
	case EBWAugmentSet::Berend: return TEXT("베렌드");
	case EBWAugmentSet::Taiga: return TEXT("타이가");
	case EBWAugmentSet::Geleon: return TEXT("겔레온");
	case EBWAugmentSet::Patrovas: return TEXT("파트로바스");
	case EBWAugmentSet::Megenberta: return TEXT("메겐베르타");
	case EBWAugmentSet::Radiant: return TEXT("레디언트");
	case EBWAugmentSet::Lifestyle: return TEXT("각성");
	default: return FString();
	}
}

FString UBWAugmentRules::DescribeSet(EBWAugmentSet Set, int32 Have, EClassType Class)
{
	TArray<FString> Lines;
	for (int32 Tier = 2; Tier <= 4; ++Tier)
	{
		Lines.Add(FString::Printf(TEXT("%s %d개: %s"), Have >= Tier ? TEXT("●") : TEXT("○"), Tier, *DescribeSetTier(Set, Tier, Class)));
	}
	return FString::Join(Lines, TEXT("\n"));
}

FString UBWAugmentRules::DescribeSetTier(EBWAugmentSet Set, int32 Tier, EClassType Class)
{
	const int32 I = FMath::Clamp(Tier, 2, 4) - 2;
	if (Set == EBWAugmentSet::Lifestyle)
	{
		static const TCHAR* PHA[] = { TEXT("잃은 HP 10%당 피해 +2%"), TEXT("피해의 15% 흡혈, 자해 -1"), TEXT("피해 ×2") };
		static const TCHAR* TEC[] = { TEXT("치명타 +10%"), TEXT("치명 배율 +0.5, 원거리 최댓값 +2"), TEXT("항상 치명타") };
		static const TCHAR* INT[] = { TEXT("전투 시작 MP +3, 턴마다 MP +1"), TEXT("피해 +25%, 마나 가드 MP 1당 +1"), TEXT("피해 ×2, 효과카드 MP 없음") };
		static const TCHAR* FTH[] = { TEXT("회복 +25%, 턴마다 HP 2"), TEXT("피해 +25%"), TEXT("피해 ×2, 회복이 적 전체 피해로") };
		static const TCHAR* HP[] = { TEXT("전투 시작 방어도 +8"), TEXT("철혈 +2, 턴 끝 방어도 +3"), TEXT("방어도가 깎이지 않는다. 턴 끝 곰신의 포효: 방어도만큼 적 전체") };
		switch (Class)
		{
		case EClassType::PHA: return PHA[I];
		case EClassType::TEC: return TEC[I];
		case EClassType::INT: return INT[I];
		case EClassType::FTH: return FTH[I];
		case EClassType::HP: return HP[I];
		default: return FString();
		}
	}
	static const TCHAR* Astrin[] = { TEXT("동점이면 이긴다, 재굴림 +2"), TEXT("치명타 +10%, 전투 첫 치명타 ×2. 동점을 뒤집으면 별똥별 5"), TEXT("언제나 최댓값. 이기면 별의 심판: 적 전체에 굴린 수만큼 (턴당 1번)") };
	static const TCHAR* Sylpira[] = { TEXT("효과카드 2장째에 1장 뽑기"), TEXT("효과카드 한도 +1, 쓸 때마다 최댓값 +3, 3장째 +5. 쓸 때 바람 칼날 2"), TEXT("효과카드 제한 해제·MP 없음, 피해 ×2. 쓰면 폭풍의 장막: 적 전체 6") };
	static const TCHAR* Soltheris[] = { TEXT("회복 +50%, 턴마다 HP 1"), TEXT("턴마다 HP 2 더, 회복한 만큼 방어도. 회복의 절반이 빛이 되어 적에게"), TEXT("회복 +50% 더. 턴 시작에 태양 강림: HP 10 회복, 적 전체 10") };
	static const TCHAR* Urska[] = { TEXT("전투 시작 방어도 +8"), TEXT("턴 끝 방어도 +4, 방어도 10 이상이면 최댓값 +5. 턴 끝에 곰의 일격"), TEXT("방어도가 깎이지 않는다. 턴 끝 곰신의 포효: 방어도만큼 적 전체") };
	static const TCHAR* Lirin[] = { TEXT("나쁜 상태 1턴 짧게, 턴마다 HP 2"), TEXT("턴마다 HP 5 더. 걸린 나쁜 상태를 적에게도 건다"), TEXT("나쁜 상태 면역, 턴마다 HP 6 더. 턴 시작 망각의 안개: 적 전체 4, 기절") };
	static const TCHAR* Lunaris[] = { TEXT("치명 배율 +0.3, 처치하면 이동 +2·1장 뽑기"), TEXT("치명타 +25%, 전투 첫 공격 치명타. 치명타에 달빛 추가타"), TEXT("항상 치명타, 처치하면 다시 공격. 치명타에 월식: 적 전체에 피해 절반") };
	static const TCHAR* Berend[] = { TEXT("기본 공격 +1~2"), TEXT("기본 공격 최댓값 +1, 근접으로 이기면 방어도 +3. 이기면 곁의 적에게 충격파 4"), TEXT("기본 공격 3배. 이기면 대지 가르기: 같은 피해가 적 전체로") };
	static const TCHAR* Taiga[] = { TEXT("이동이 남은 채 공격하면 최솟값 +1"), TEXT("공격 최댓값 +2. 3칸 이상 달려 이기면 그림자 분신"), TEXT("속도 +20, 칸마다 최댓값 +1. 3칸 달리면 천 개의 그림자: 적 전체에 칸×2") };
	static const TCHAR* Geleon[] = { TEXT("원거리 최댓값 +3"), TEXT("원거리 피해 +15%, 이기면 둔화. 턴 끝 지원 포격 6"), TEXT("사거리 +20, 원거리는 지지 않는다. 턴 끝 일제 사격: 적 전체 8") };
	static const TCHAR* Patrovas[] = { TEXT("공격 최댓값 +2"), TEXT("피해 +25%. 턴 시작에 마력 폭주 3"), TEXT("피해 ×3. 턴 시작 마력 폭풍: 적 전체 9") };
	static const TCHAR* Megenberta[] = { TEXT("상점 -15%, 전투 시작 2장 더"), TEXT("카드 보상 +1, 전투 시작 2장 더"), TEXT("상점 거의 공짜, 턴마다 1장 더, 피해 ×2. 턴 끝 황금비: 골드÷15를 적 전체") };
	static const TCHAR* Radiant[] = { TEXT("자해하면 방어도 +3, 이기면 출혈"), TEXT("피해의 30% 흡혈. 자해가 피의 칼날이 된다"), TEXT("자해가 회복으로, 피해 ×2. 자해하면 피의 비: 자해×3을 적 전체") };
	switch (Set)
	{
	case EBWAugmentSet::Astrin: return Astrin[I];
	case EBWAugmentSet::Sylpira: return Sylpira[I];
	case EBWAugmentSet::Soltheris: return Soltheris[I];
	case EBWAugmentSet::Urska: return Urska[I];
	case EBWAugmentSet::Lirin: return Lirin[I];
	case EBWAugmentSet::Lunaris: return Lunaris[I];
	case EBWAugmentSet::Berend: return Berend[I];
	case EBWAugmentSet::Taiga: return Taiga[I];
	case EBWAugmentSet::Geleon: return Geleon[I];
	case EBWAugmentSet::Patrovas: return Patrovas[I];
	case EBWAugmentSet::Megenberta: return Megenberta[I];
	case EBWAugmentSet::Radiant: return Radiant[I];
	default: return FString();
	}
}

void UBWAugmentRules::AddSetBonus(FBWAugmentTotals& T, EBWAugmentSet Set, int32 Count)
{
	using S = EBWAugStat;
	auto Add = [&T](S Stat, float Value)
	{
		float& Slot = T.Stats[(int32)Stat];
		Slot = Stat == S::DamageMult ? Slot * Value : Slot + Value;
	};
	const bool b2 = Count >= 2, b3 = Count >= 3, b4 = Count >= 4;
	switch (Set)
	{
	case EBWAugmentSet::Astrin:
		if (b2) { Add(S::TieWin, 1); Add(S::Rerolls, 2); }
		if (b3) { Add(S::CritChance, 0.1f); Add(S::FirstCritX2, 1); }
		if (b4) Add(S::MaxRoll, 1);
		break;
	case EBWAugmentSet::Sylpira:
		if (b2) Add(S::TwoEffectDraw, 1);
		if (b3) { Add(S::EffectLimit, 1); Add(S::EffectMaxPer, 3); Add(S::EffectThreeMax, 5); }
		if (b4) { Add(S::EffectLimit, 9); Add(S::EffectNoMP, 1); Add(S::EffectMaxPer, 5); Add(S::DamageMult, 2); }
		break;
	case EBWAugmentSet::Soltheris:
		if (b2) { Add(S::HealPct, 0.5f); Add(S::TurnHeal, 1); }
		if (b3) { Add(S::TurnHeal, 2); Add(S::HealToBlock, 1); }
		if (b4) Add(S::HealPct, 0.5f);
		break;
	case EBWAugmentSet::Urska:
		if (b2) Add(S::StartBlock, 8);
		if (b3) { Add(S::TurnEndBlock, 4); Add(S::Block10Max, 5); }
		if (b4) Add(S::BlockNoLoss, 1);
		break;
	case EBWAugmentSet::Lirin:
		if (b2) { Add(S::DebuffShort, 1); Add(S::TurnHeal, 2); }
		if (b3) Add(S::TurnHeal, 5);
		if (b4) { Add(S::DebuffImmune, 1); Add(S::EnemiesConfused, 1); Add(S::TurnHeal, 6); }
		break;
	case EBWAugmentSet::Lunaris:
		if (b2) { Add(S::CritMul, 0.3f); Add(S::KillMove, 2); Add(S::KillDraw, 1); }
		if (b3) { Add(S::CritChance, 0.25f); Add(S::FirstAttackCrit, 1); }
		if (b4) { Add(S::AlwaysCrit, 1); Add(S::CritMul, 1.0f); Add(S::KillRefund, 1); }
		break;
	case EBWAugmentSet::Berend:
		if (b2) { Add(S::BasicMin, 1); Add(S::BasicMax, 2); }
		if (b3) { Add(S::BasicMax, 1); Add(S::MeleeWinBlock, 3); }
		if (b4) Add(S::BasicMul, 2);
		break;
	case EBWAugmentSet::Taiga:
		if (b2) Add(S::MoveLeftMin, 1);
		if (b3) Add(S::AttackMax, 2);
		if (b4) { Add(S::SpeedMax, 20); Add(S::SpeedMin, 20); Add(S::MovedMaxPerTile, 1); }
		break;
	case EBWAugmentSet::Geleon:
		if (b2) Add(S::RangedMax, 3);
		if (b3) { Add(S::RangedDamagePct, 0.15f); Add(S::WinSlow, 1); } // 나머지 힘은 지원 포격으로
		if (b4) { Add(S::ReachUp, 20); Add(S::RangedNeverLose, 1); }
		break;
	case EBWAugmentSet::Patrovas:
		if (b2) Add(S::AttackMax, 2);
		if (b3) Add(S::DamagePct, 0.25f);
		if (b4) Add(S::DamageMult, 3);
		break;
	case EBWAugmentSet::Megenberta:
		if (b2) { Add(S::ShopPct, -0.15f); Add(S::BattleDraw, 2); }
		if (b3) { Add(S::CardChoices, 1); Add(S::BattleDraw, 2); }
		if (b4) { Add(S::ShopPct, -0.85f); Add(S::TurnDraw, 1); Add(S::DamageMult, 2); }
		break;
	case EBWAugmentSet::Radiant:
		if (b2) { Add(S::SelfDamageBlock, 3); Add(S::WinBleed, 2); }
		if (b3) Add(S::Lifesteal, 0.3f);
		if (b4) { Add(S::SelfDamageHeals, 1); Add(S::DamageMult, 2); }
		break;
	case EBWAugmentSet::Lifestyle:
		switch (T.LifestyleClass)
		{
		case EClassType::PHA:
			if (b2) Add(S::WinPerLostHP, 0.02f);
			if (b3) { Add(S::Lifesteal, 0.15f); Add(S::SelfDamageDown, 1); }
			if (b4) Add(S::DamageMult, 2);
			break;
		case EClassType::TEC:
			if (b2) Add(S::CritChance, 0.1f);
			if (b3) { Add(S::CritMul, 0.5f); Add(S::RangedMax, 2); }
			if (b4) Add(S::AlwaysCrit, 1);
			break;
		case EClassType::INT:
			if (b2) { Add(S::StartMP, 3); Add(S::MPPerTurn, 1); }
			if (b3) { Add(S::DamagePct, 0.25f); Add(S::ShieldPerUp, 1); }
			if (b4) { Add(S::DamageMult, 2); Add(S::EffectNoMP, 1); }
			break;
		case EClassType::FTH:
			if (b2) { Add(S::HealPct, 0.25f); Add(S::TurnHeal, 2); }
			if (b3) Add(S::DamagePct, 0.25f);
			if (b4) { Add(S::DamageMult, 2); Add(S::HealToAoE, 1); }
			break;
		case EClassType::HP:
			if (b2) Add(S::StartBlock, 8);
			if (b3) { Add(S::IronBloodUp, 2); Add(S::TurnEndBlock, 3); }
			if (b4) Add(S::BlockNoLoss, 1);
			break;
		default: break;
		}
		break;
	default: break;
	}
}

bool UBWAugmentRules::IsEligible(const FBWAugmentData& Augment, FName RowName, int32 Act, EClassType PlayerClass, const TArray<FName>& Owned)
{
	if (Owned.Contains(RowName)) return false;
	if (Act < Augment.MinAct || Act > Augment.MaxAct) return false;
	return Augment.ClassType == EClassType::Common || Augment.ClassType == PlayerClass;
}

TArray<FName> UBWAugmentRules::PickCandidates(const TArray<FName>& RowNames, const TArray<FBWAugmentData>& Rows, int32 Act, EClassType PlayerClass,
	const TArray<FName>& Owned, const TArray<FName>& Exclude, int32 Count, FRandomStream& Stream)
{
	TArray<FName> Fresh, Repeat;
	for (int32 i = 0; i < FMath::Min(RowNames.Num(), Rows.Num()); ++i)
	{
		if (!IsEligible(Rows[i], RowNames[i], Act, PlayerClass, Owned)) continue;
		(Exclude.Contains(RowNames[i]) ? Repeat : Fresh).Add(RowNames[i]);
	}

	auto Shuffle = [&Stream](TArray<FName>& Names)
	{
		for (int32 i = Names.Num() - 1; i > 0; --i) Names.Swap(i, Stream.RandRange(0, i));
	};
	Shuffle(Fresh);
	Shuffle(Repeat);

	// 리롤하면 새 후보를 우선하고, 모자라면 이전 후보로 채운다
	TArray<FName> Result;
	for (const FName& Name : Fresh) { if (Result.Num() >= Count) break; Result.Add(Name); }
	for (const FName& Name : Repeat) { if (Result.Num() >= Count) break; Result.Add(Name); }
	return Result;
}

TArray<FName> UBWAugmentRules::PickByKind(const TArray<FName>& RowNames, const TArray<FBWAugmentData>& Rows, EClassType PlayerClass,
	const TArray<FName>& Owned, const TArray<FName>& Exclude, FRandomStream& Stream)
{
	const int32 Num = FMath::Min(RowNames.Num(), Rows.Num());
	TArray<int32> SetCounts;
	SetCounts.Init(0, (int32)EBWAugmentSet::Count);
	for (int32 i = 0; i < Num; ++i)
	{
		if (Owned.Contains(RowNames[i]) && Rows[i].Set != EBWAugmentSet::None) ++SetCounts[(int32)Rows[i].Set];
	}

	auto CanOffer = [&](int32 i, EBWAugmentKind Kind)
	{
		// 유리 대포 등: 최대 HP가 공격력인 철혈의 분쇄자에게는 최대 HP를 깎는 증강이 나오지 않는다
		if (PlayerClass == EClassType::HP && Rows[i].Mods.ContainsByPredicate([](const FBWAugmentMod& M) { return M.Stat == EBWAugStat::MaxHPPct && M.Value < 0.0f; })) return false;
		return Rows[i].Kind == Kind && !Owned.Contains(RowNames[i]) && (Rows[i].ClassType == EClassType::Common || Rows[i].ClassType == PlayerClass);
	};

	TArray<int32> Picked;
	for (const EBWAugmentKind Kind : { EBWAugmentKind::Stable, EBWAugmentKind::Change, EBWAugmentKind::Gamble })
	{
		for (const bool bAllowExcluded : { false, true })
		{
			// 공용 전체와 전용 전체가 1:1이 되도록 나눈 뒤 세트 가중치를 곱한다
			int32 CommonCount = 0, ClassCount = 0;
			for (int32 i = 0; i < Num; ++i)
			{
				if (!CanOffer(i, Kind) || (!bAllowExcluded && Exclude.Contains(RowNames[i]))) continue;
				++(Rows[i].ClassType == EClassType::Common ? CommonCount : ClassCount);
			}
			if (CommonCount + ClassCount == 0) continue;

			TArray<int32> Pool;
			TArray<float> Weights;
			float Total = 0.0f;
			for (int32 i = 0; i < Num; ++i)
			{
				if (!CanOffer(i, Kind) || (!bAllowExcluded && Exclude.Contains(RowNames[i]))) continue;
				float W = 1.0f / (Rows[i].ClassType == EClassType::Common ? CommonCount : ClassCount);
				const int32 Have = SetCounts[(int32)Rows[i].Set];
				if (Rows[i].Set != EBWAugmentSet::None && Have >= 3) W *= FullSetWeight;
				else if (Rows[i].Set != EBWAugmentSet::None && Have >= 1) W *= OwnedSetWeight;
				Pool.Add(i);
				Weights.Add(W);
				Total += W;
			}
			float Roll = Stream.FRandRange(0.0f, Total);
			int32 Choice = Pool.Last();
			for (int32 i = 0; i < Pool.Num(); ++i)
			{
				Roll -= Weights[i];
				if (Roll <= 0.0f) { Choice = Pool[i]; break; }
			}
			Picked.Add(Choice);
			break;
		}
	}

	// 세트 보장: 1~2개 가진 세트가 후보에 없으면 한 칸을 그 세트의 같은 종류 증강으로 바꾼다
	TArray<EBWAugmentSet> Wanted;
	for (int32 s = 1; s < (int32)EBWAugmentSet::Count; ++s)
	{
		if (SetCounts[s] >= 1 && SetCounts[s] <= 2) Wanted.Add((EBWAugmentSet)s);
	}
	bool bAlreadyOffered = false;
	for (int32 Index : Picked) bAlreadyOffered |= Wanted.Contains(Rows[Index].Set);
	if (Wanted.Num() > 0 && Picked.Num() > 0 && !bAlreadyOffered && Stream.GetFraction() < SetGuaranteeChance)
	{
		const EBWAugmentSet Target = Wanted[Stream.RandRange(0, Wanted.Num() - 1)];
		const int32 Slot = Stream.RandRange(0, Picked.Num() - 1);
		TArray<int32> Same;
		for (int32 i = 0; i < Num; ++i)
		{
			if (Rows[i].Set == Target && CanOffer(i, Rows[Picked[Slot]].Kind)) Same.Add(i);
		}
		if (Same.Num() > 0) Picked[Slot] = Same[Stream.RandRange(0, Same.Num() - 1)];
	}

	TArray<FName> Result;
	for (int32 Index : Picked) Result.Add(RowNames[Index]);
	return Result;
}

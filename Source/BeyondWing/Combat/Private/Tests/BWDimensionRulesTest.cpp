#include "Misc/AutomationTest.h"
#include "BWDimensionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BWDimensionTest
{
	// 월드가 필요 없는 빠른 순수 함수 테스트라 SmokeFilter로 둔다
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter;

	FBWDuelSide MakeSide(int32 Min, int32 Max)
	{
		FBWDuelSide Side;
		Side.SkillRange = FBWDiceRange(Min, Max);
		return Side;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDimensionTaigaTest, "BeyondWing.Combat.Dimension.Taiga", BWDimensionTest::Flags)
bool FBWDimensionTaigaTest::RunTest(const FString& Parameters)
{
	FBWBattlefieldState State = UBWDimensionRules::MakeBattlefield(EDimensionType::Taiga, false);
	TestEqual(TEXT("3 charges"), State.TaigaChargesLeft, 3);
	TestEqual(TEXT("no enemy charge without overload"), State.TaigaEnemyChargesLeft, 0);

	TestEqual(TEXT("forced crit while charges remain"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 4)).CritChance, 1.0f);
	for (int32 i = 0; i < 3; ++i) TestTrue(TEXT("charge consumed"), UBWDimensionRules::ConsumeTaigaCharge(State));
	TestFalse(TEXT("4th landed attack gets nothing"), UBWDimensionRules::ConsumeTaigaCharge(State));
	TestEqual(TEXT("no forced crit after 3"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 4)).CritChance, 0.0f);

	FBWBattlefieldState Overloaded = UBWDimensionRules::MakeBattlefield(EDimensionType::Taiga, true);
	TestTrue(TEXT("overload: enemy's first attack ignores block"), UBWDimensionRules::ConsumeTaigaEnemyCharge(Overloaded));
	TestFalse(TEXT("only once"), UBWDimensionRules::ConsumeTaigaEnemyCharge(Overloaded));

	FBWBattlefieldState Other = UBWDimensionRules::MakeBattlefield(EDimensionType::Berend, false);
	TestFalse(TEXT("no Taiga charges elsewhere"), UBWDimensionRules::ConsumeTaigaCharge(Other));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDimensionBerendTest, "BeyondWing.Combat.Dimension.BerendWallSlam", BWDimensionTest::Flags)
bool FBWDimensionBerendTest::RunTest(const FString& Parameters)
{
	bool bDamage = false, bStun = false;
	UBWDimensionRules::GetWallSlamRule(UBWDimensionRules::MakeBattlefield(EDimensionType::Berend, false), true, bDamage, bStun);
	TestTrue(TEXT("Berend: my wall slam deals damage and stuns"), bDamage && bStun);

	UBWDimensionRules::GetWallSlamRule(UBWDimensionRules::MakeBattlefield(EDimensionType::Berend, false), false, bDamage, bStun);
	TestTrue(TEXT("Berend: enemy wall slam does nothing extra"), !bDamage && !bStun);

	UBWDimensionRules::GetWallSlamRule(UBWDimensionRules::MakeBattlefield(EDimensionType::Berend, true), false, bDamage, bStun);
	TestTrue(TEXT("Berend overload: enemy wall slam deals damage, no stun"), bDamage && !bStun);

	UBWDimensionRules::GetWallSlamRule(UBWDimensionRules::MakeBattlefield(EDimensionType::Geleon, false), true, bDamage, bStun);
	TestTrue(TEXT("no wall slam bonus outside Berend"), !bDamage && !bStun);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDimensionPatrovasTest, "BeyondWing.Combat.Dimension.Patrovas", BWDimensionTest::Flags)
bool FBWDimensionPatrovasTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(11);
	const int32 Trials = 4000;

	auto GrowRate = [&](bool bOverloaded)
	{
		FBWBattlefieldState State = UBWDimensionRules::MakeBattlefield(EDimensionType::Patrovas, bOverloaded);
		int32 Grew = 0;
		for (int32 i = 0; i < Trials; ++i)
		{
			const float M = UBWDimensionRules::RollPatrovasMultiplier(State, Stream);
			if (M < 0.7f - KINDA_SMALL_NUMBER || M > 1.3f + KINDA_SMALL_NUMBER) { AddError(FString::Printf(TEXT("multiplier out of range: %f"), M)); break; }
			if (M > 1.0f) ++Grew;
		}
		return (float)Grew / Trials;
	};
	const float Normal = GrowRate(false);
	const float Overloaded = GrowRate(true);
	TestTrue(FString::Printf(TEXT("grows about half the time (%.3f)"), Normal), FMath::Abs(Normal - 0.5f) < 0.03f);
	TestTrue(FString::Printf(TEXT("overload grows less often (%.3f)"), Overloaded), FMath::Abs(Overloaded - 0.3f) < 0.03f);

	FBWDiceRange R = UBWDimensionRules::ScaleSkillMax(FBWDiceRange(2, 10), 1.3f);
	TestTrue(TEXT("x1.3: 2~13"), R.Min == 2 && R.Max == 13);
	R = UBWDimensionRules::ScaleSkillMax(FBWDiceRange(5, 6), 0.7f);
	TestTrue(TEXT("x0.7: max 4, min clamped to 4"), R.Min == 4 && R.Max == 4);

	// 파트로바스 배율은 내 쪽 범위에만 적용된다
	FBWBattlefieldState State = UBWDimensionRules::MakeBattlefield(EDimensionType::Patrovas, false);
	State.PatrovasMaxMultiplier = 1.3f;
	TestEqual(TEXT("player side max scaled"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 10)).SkillRange.Max, 13);

	FBWBattlefieldState Elsewhere = UBWDimensionRules::MakeBattlefield(EDimensionType::Taiga, false);
	TestEqual(TEXT("no Patrovas roll elsewhere"), UBWDimensionRules::RollPatrovasMultiplier(Elsewhere, Stream), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDimensionGeleonTest, "BeyondWing.Combat.Dimension.Geleon", BWDimensionTest::Flags)
bool FBWDimensionGeleonTest::RunTest(const FString& Parameters)
{
	FBWBattlefieldState State = UBWDimensionRules::MakeBattlefield(EDimensionType::Geleon, false);
	const FBWDuelSide Side = UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 4));
	TestEqual(TEXT("+50% damage"), Side.DamageMultiplier, 1.5f);
	TestEqual(TEXT("no incoming change without overload"), Side.IncomingDamageMultiplier, 1.0f);

	FBWCardData Strike;   Strike.CardName = TEXT("타격");     Strike.CardKind = ECardKind::Attack;
	FBWCardData Bash;     Bash.CardName = TEXT("방패 밀치기"); Bash.CardKind = ECardKind::Attack;
	FBWCardData Heal;     Heal.CardName = TEXT("응급 처치");   Heal.CardKind = ECardKind::Effect;

	// 효과카드 안 씀: 공격카드는 모두, 효과카드는 못 씀
	TestTrue(TEXT("no-effects: any attack card"), UBWDimensionRules::CanUseCard(State, Strike) && UBWDimensionRules::CanUseCard(State, Bash));
	TestFalse(TEXT("no-effects: effect cards blocked"), UBWDimensionRules::CanUseCard(State, Heal));
	UBWDimensionRules::NoteAttackCardUsed(State, Strike);
	TestTrue(TEXT("no-effects: attack cards stay free after use"), UBWDimensionRules::CanUseCard(State, Bash));

	// 효과카드 사용: 효과카드 + 처음 쓴 공격카드 1종만
	FBWBattlefieldState WithEffects = UBWDimensionRules::MakeBattlefield(EDimensionType::Geleon, false);
	WithEffects.bGeleonUseEffects = true;
	TestTrue(TEXT("effects: effect cards allowed"), UBWDimensionRules::CanUseCard(WithEffects, Heal));
	TestTrue(TEXT("effects: any attack card before the first"), UBWDimensionRules::CanUseCard(WithEffects, Bash));
	UBWDimensionRules::NoteAttackCardUsed(WithEffects, Strike);
	TestTrue(TEXT("effects: same attack card again"), UBWDimensionRules::CanUseCard(WithEffects, Strike));
	TestFalse(TEXT("effects: other attack cards blocked"), UBWDimensionRules::CanUseCard(WithEffects, Bash));
	UBWDimensionRules::NoteAttackCardUsed(WithEffects, Bash);
	TestEqual(TEXT("effects: first card stays the chosen one"), WithEffects.GeleonAttackCardName, FString(TEXT("타격")));

	FBWBattlefieldState Overloaded = UBWDimensionRules::MakeBattlefield(EDimensionType::Geleon, true);
	TestEqual(TEXT("overload: player takes +20%"), UBWDimensionRules::ApplyToPlayerSide(Overloaded, BWDimensionTest::MakeSide(1, 4)).IncomingDamageMultiplier, 1.2f);

	FBWBattlefieldState None = UBWDimensionRules::MakeBattlefield(EDimensionType::None, true);
	TestFalse(TEXT("no dimension, no overload"), None.bOverloaded);
	TestTrue(TEXT("no restriction outside Geleon"), UBWDimensionRules::CanUseCard(None, Heal));
	return true;
}

#endif

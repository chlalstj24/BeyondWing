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
	TestEqual(TEXT("starts without stealth"), State.TaigaChargesLeft, 0);
	State.TaigaChargesLeft = 1;
	TestEqual(TEXT("forced crit while stealthed"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 4)).CritChance, 1.0f);
	TestTrue(TEXT("landing a hit uses stealth"), UBWDimensionRules::ConsumeTaigaCharge(State));
	TestFalse(TEXT("only one hit"), UBWDimensionRules::ConsumeTaigaCharge(State));
	TestEqual(TEXT("no crit afterwards"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 4)).CritChance, 0.0f);

	FBWBattlefieldState Overloaded = UBWDimensionRules::MakeBattlefield(EDimensionType::Taiga, true);
	TestTrue(TEXT("overload: enemy's first attack ignores block"), UBWDimensionRules::ConsumeTaigaEnemyCharge(Overloaded));
	TestFalse(TEXT("only once"), UBWDimensionRules::ConsumeTaigaEnemyCharge(Overloaded));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDimensionBerendTest, "BeyondWing.Combat.Dimension.BerendWallSlam", BWDimensionTest::Flags)
bool FBWDimensionBerendTest::RunTest(const FString& Parameters)
{
	const FBWBattlefieldState Berend = UBWDimensionRules::MakeBattlefield(EDimensionType::Berend, false);
	TestTrue(TEXT("every attack pushes in Berend"), UBWDimensionRules::ApplyToPlayerSide(Berend, BWDimensionTest::MakeSide(1, 4)).bPushesTarget);
	TestTrue(TEXT("enemies push too"), UBWDimensionRules::ApplyToEnemySide(Berend, BWDimensionTest::MakeSide(1, 4), 0, false).bPushesTarget);

	bool bDamage = false, bStun = false;
	UBWDimensionRules::GetWallSlamRule(Berend, true, bDamage, bStun);
	TestTrue(TEXT("my wall slam deals damage and stuns"), bDamage && bStun);
	UBWDimensionRules::GetWallSlamRule(Berend, false, bDamage, bStun);
	TestTrue(TEXT("enemy wall slam deals damage, no stun"), bDamage && !bStun);
	UBWDimensionRules::GetWallSlamRule(UBWDimensionRules::MakeBattlefield(EDimensionType::Berend, true), false, bDamage, bStun);
	TestTrue(TEXT("overload: still no stun on me"), bDamage && !bStun);
	UBWDimensionRules::GetWallSlamRule(UBWDimensionRules::MakeBattlefield(EDimensionType::Geleon, false), true, bDamage, bStun);
	TestTrue(TEXT("nothing outside Berend"), !bDamage && !bStun);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDimensionPatrovasTest, "BeyondWing.Combat.Dimension.Patrovas", BWDimensionTest::Flags)
bool FBWDimensionPatrovasTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(11);
	FBWBattlefieldState State = UBWDimensionRules::MakeBattlefield(EDimensionType::Patrovas, false);
	for (int32 i = 0; i < 200; ++i)
	{
		UBWDimensionRules::RollManaRows(State, 8, Stream);
		TestEqual(TEXT("two different lines"), State.ManaRows.Num(), 2);
	}
	State.ManaRows = { 3, 5 };
	TestTrue(TEXT("on the line"), UBWDimensionRules::IsOnManaLine(State, FIntPoint(3, 7)));
	TestFalse(TEXT("off the line"), UBWDimensionRules::IsOnManaLine(State, FIntPoint(4, 3)));
	TestEqual(TEXT("my max x1.4 on the line"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 10), 0, true).SkillRange.Max, 14);
	TestEqual(TEXT("off the line unchanged"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 10), 0, false).SkillRange.Max, 10);
	TestEqual(TEXT("enemy max x1.4 too"), UBWDimensionRules::ApplyToEnemySide(State, BWDimensionTest::MakeSide(1, 10), 0, true).SkillRange.Max, 14);

	FBWBattlefieldState Elsewhere = UBWDimensionRules::MakeBattlefield(EDimensionType::Taiga, false);
	UBWDimensionRules::RollManaRows(Elsewhere, 8, Stream);
	TestEqual(TEXT("no lines elsewhere"), Elsewhere.ManaRows.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDimensionGeleonTest, "BeyondWing.Combat.Dimension.Geleon", BWDimensionTest::Flags)
bool FBWDimensionGeleonTest::RunTest(const FString& Parameters)
{
	const FBWBattlefieldState State = UBWDimensionRules::MakeBattlefield(EDimensionType::Geleon, false);
	TestEqual(TEXT("no advance, no bonus"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 4), 0).DamageMultiplier, 1.0f);
	TestEqual(TEXT("+15% per tile"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 4), 2).DamageMultiplier, 1.3f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("capped at +60%"), UBWDimensionRules::ApplyToPlayerSide(State, BWDimensionTest::MakeSide(1, 4), 9).DamageMultiplier, 1.6f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("enemy rush counts"), UBWDimensionRules::ApplyToEnemySide(State, BWDimensionTest::MakeSide(1, 4), 2, false).DamageMultiplier, 1.3f, KINDA_SMALL_NUMBER);

	FBWBattlefieldState Overloaded = UBWDimensionRules::MakeBattlefield(EDimensionType::Geleon, true);
	TestEqual(TEXT("overload: player takes +20%"), UBWDimensionRules::ApplyToPlayerSide(Overloaded, BWDimensionTest::MakeSide(1, 4)).IncomingDamageMultiplier, 1.2f);
	FBWBattlefieldState None = UBWDimensionRules::MakeBattlefield(EDimensionType::None, true);
	TestFalse(TEXT("no dimension, no overload"), None.bOverloaded);
	TestEqual(TEXT("no advance bonus elsewhere"), UBWDimensionRules::GetGeleonAdvanceMultiplier(None, 5), 1.0f);
	return true;
}

#endif

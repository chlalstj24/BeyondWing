#include "Misc/AutomationTest.h"
#include "DeckComponent.h"
#include "BWDuelRules.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BWDeckTest
{
	// 월드가 필요 없는 빠른 테스트라 SmokeFilter로 둔다
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter;
}

// 에디터 시작 시 smoke 실행은 엔진 초기화 전(PreInit)이라 액터 컴포넌트를 만들 수 없다. 그때는 건너뛰고 이후 실행에서 검증한다
#define BW_SKIP_IF_ENGINE_NOT_READY() if (!GEngine) { AddInfo(TEXT("Skipped: engine not initialized (startup smoke run)")); return true; }

namespace BWDeckTest
{

	FBWCardData MakeCard(const TCHAR* Name, ECardKind Kind, int32 MPCost = 0)
	{
		FBWCardData Card;
		Card.CardName = Name;
		Card.CardKind = Kind;
		Card.MPCost = MPCost;
		return Card;
	}

	// 공격 3장 + 효과 4장 = 7장
	UDeckComponent* MakeDeck()
	{
		UDeckComponent* Deck = NewObject<UDeckComponent>(GetTransientPackage());
		TArray<FBWCardData> Cards;
		for (int32 i = 0; i < 3; ++i) Cards.Add(MakeCard(TEXT("Attack"), ECardKind::Attack));
		for (int32 i = 0; i < 4; ++i) Cards.Add(MakeCard(TEXT("Effect"), ECardKind::Effect));
		Deck->InitializeCombatDeck(Cards);
		return Deck;
	}

	int32 FindKind(const UDeckComponent* Deck, ECardKind Kind)
	{
		return Deck->Hand.IndexOfByPredicate([Kind](const FBWCardData& C) { return C.CardKind == Kind; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDeckDrawShuffleTest, "BeyondWing.Combat.Deck.DrawAndShuffle", BWDeckTest::Flags)
bool FBWDeckDrawShuffleTest::RunTest(const FString& Parameters)
{
	BW_SKIP_IF_ENGINE_NOT_READY();
	UDeckComponent* Deck = BWDeckTest::MakeDeck();

	Deck->StartTurn();
	TestEqual(TEXT("turn 1 draws the opening hand of 5"), Deck->Hand.Num(), 5);
	TestEqual(TEXT("2 left in the draw pile"), Deck->Deck.Num(), 2);

	// 손패는 버리지 않고 이어진다
	Deck->EndTurn();
	TestEqual(TEXT("end turn keeps the hand"), Deck->Hand.Num(), 5);
	TestEqual(TEXT("nothing discarded at end of turn"), Deck->Discard.Num(), 0);

	// 둘째 턴부터는 1장씩
	Deck->StartTurn();
	TestEqual(TEXT("turn 2 draws one"), Deck->Hand.Num(), 6);
	TestEqual(TEXT("1 left in the draw pile"), Deck->Deck.Num(), 1);

	// 카드를 하나 쓰면 버린 덱으로, 덱이 비면 버린 덱을 섞어 다시 뽑는다
	FBWCardData Used;
	TestTrue(TEXT("use a card"), Deck->UseCard(BWDeckTest::FindKind(Deck, ECardKind::Effect), Used));
	Deck->EndTurn();
	Deck->StartTurn(); // 덱의 마지막 1장
	Deck->EndTurn();
	Deck->StartTurn(); // 덱이 비어 버린 1장을 섞어 뽑는다
	TestEqual(TEXT("discard was shuffled back in and drawn"), Deck->Discard.Num(), 0);
	TestEqual(TEXT("cards are conserved"), Deck->Hand.Num() + Deck->Deck.Num() + Deck->Discard.Num(), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDeckHandLimitTest, "BeyondWing.Combat.Deck.HandLimit", BWDeckTest::Flags)
bool FBWDeckHandLimitTest::RunTest(const FString& Parameters)
{
	BW_SKIP_IF_ENGINE_NOT_READY();
	UDeckComponent* Deck = NewObject<UDeckComponent>(GetTransientPackage());
	TArray<FBWCardData> Cards;
	for (int32 i = 0; i < 14; ++i) Cards.Add(BWDeckTest::MakeCard(TEXT("Effect"), ECardKind::Effect));
	Deck->InitializeCombatDeck(Cards);

	// 5장으로 시작해 턴마다 1장: 6턴째에 10장
	for (int32 Turn = 0; Turn < 6; ++Turn) { Deck->StartTurn(); Deck->EndTurn(); }
	TestEqual(TEXT("hand reaches the limit"), Deck->Hand.Num(), 10);
	TestEqual(TEXT("nothing burned yet"), Deck->Discard.Num(), 0);

	// 가득 찬 채로 뽑으면 그 카드는 바로 버린 덱으로
	Deck->StartTurn();
	TestEqual(TEXT("hand stays at 10"), Deck->Hand.Num(), 10);
	TestEqual(TEXT("drawn card is discarded"), Deck->Discard.Num(), 1);
	Deck->DrawCards(2);
	TestEqual(TEXT("extra draws are discarded too"), Deck->Discard.Num(), 3);
	TestEqual(TEXT("cards are conserved"), Deck->Hand.Num() + Deck->Deck.Num() + Deck->Discard.Num(), 14);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDeckLimitsTest, "BeyondWing.Combat.Deck.TurnLimits", BWDeckTest::Flags)
bool FBWDeckLimitsTest::RunTest(const FString& Parameters)
{
	BW_SKIP_IF_ENGINE_NOT_READY();
	// 공격 2장 + 효과 3장이 확실히 손에 있도록 직접 구성
	UDeckComponent* Deck = NewObject<UDeckComponent>(GetTransientPackage());
	TArray<FBWCardData> Cards;
	for (int32 i = 0; i < 2; ++i) Cards.Add(BWDeckTest::MakeCard(TEXT("Attack"), ECardKind::Attack));
	for (int32 i = 0; i < 3; ++i) Cards.Add(BWDeckTest::MakeCard(TEXT("Effect"), ECardKind::Effect));
	Deck->InitializeCombatDeck(Cards);
	Deck->StartTurn();
	TestTrue(TEXT("attack card in hand"), Deck->HasAttackCardInHand());

	FBWCardData Used;
	TestTrue(TEXT("first attack card is allowed"), Deck->UseCard(BWDeckTest::FindKind(Deck, ECardKind::Attack), Used));
	TestTrue(TEXT("second attack card is blocked"), Deck->CanUseCard(BWDeckTest::FindKind(Deck, ECardKind::Attack), 0) == EBWCardUseResult::AttackLimit);

	TestTrue(TEXT("effect 1"), Deck->UseCard(BWDeckTest::FindKind(Deck, ECardKind::Effect), Used));
	TestTrue(TEXT("effect 2"), Deck->UseCard(BWDeckTest::FindKind(Deck, ECardKind::Effect), Used));
	TestTrue(TEXT("third effect card is blocked"), Deck->CanUseCard(BWDeckTest::FindKind(Deck, ECardKind::Effect), 0) == EBWCardUseResult::EffectLimit);
	TestFalse(TEXT("UseCard refuses past the limit"), Deck->UseCard(BWDeckTest::FindKind(Deck, ECardKind::Effect), Used));
	TestEqual(TEXT("used cards go to the discard pile"), Deck->Discard.Num(), 3);

	// 내가 선공으로 여는 합은 턴당 1회
	TestTrue(TEXT("duel available"), Deck->CanOpenDuel());
	Deck->NotifyDuelOpened();
	TestFalse(TEXT("second duel blocked"), Deck->CanOpenDuel());

	// 다음 턴에는 한도가 초기화된다 (덱이 비어 뽑을 카드가 없어도 괜찮다)
	Deck->EndTurn();
	Deck->StartTurn();
	TestEqual(TEXT("attack counter reset"), Deck->AttackCardsUsedThisTurn, 0);
	TestEqual(TEXT("effect counter reset"), Deck->EffectCardsUsedThisTurn, 0);
	TestTrue(TEXT("duel available again"), Deck->CanOpenDuel());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDeckMPTest, "BeyondWing.Combat.Deck.MPCost", BWDeckTest::Flags)
bool FBWDeckMPTest::RunTest(const FString& Parameters)
{
	BW_SKIP_IF_ENGINE_NOT_READY();
	UDeckComponent* Deck = NewObject<UDeckComponent>(GetTransientPackage());
	Deck->InitializeCombatDeck({ BWDeckTest::MakeCard(TEXT("Fireball"), ECardKind::Attack, 3) });
	Deck->StartTurn();
	TestTrue(TEXT("not enough MP"), Deck->CanUseCard(0, 2) == EBWCardUseResult::NotEnoughMP);
	TestTrue(TEXT("exactly enough MP"), Deck->CanUseCard(0, 3) == EBWCardUseResult::Ok);
	TestTrue(TEXT("bad index"), Deck->CanUseCard(5, 99) == EBWCardUseResult::InvalidIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWBasicStrikeTest, "BeyondWing.Combat.Duel.BasicStrike", BWDeckTest::Flags)
bool FBWBasicStrikeTest::RunTest(const FString& Parameters)
{
	const FBWCardData Strike = UBWDuelRules::MakeBasicStrike(EClassType::PHA);
	TestTrue(TEXT("basic attack is an attack"), Strike.CardKind == ECardKind::Attack);
	TestEqual(TEXT("base min 1"), Strike.SkillMin, 1);
	TestEqual(TEXT("base max 3"), Strike.SkillMax, 3);
	TestEqual(TEXT("no MP cost"), Strike.MPCost, 0);

	// PHA 5 × 0.5 = 2.5 → 최대 3 + 2, 최소 1 + 1
	FBWSkillStats Stats;
	Stats.PHA = 5;
	const FBWDiceRange Range = UBWDuelRules::ComputeCardSkillRange(Strike, Stats);
	TestEqual(TEXT("PHA basic max"), Range.Max, 5);
	TestEqual(TEXT("PHA basic min"), Range.Min, 2);

	// PHA·TEC·HP는 주 스탯 비례 추가 피해, INT·FTH는 스탯 비례 없이 약하다
	TestTrue(TEXT("TEC uses TEC"), UBWDuelRules::MakeBasicStrike(EClassType::TEC).TECScale > 0.0f);
	TestTrue(TEXT("HP uses max HP"), UBWDuelRules::MakeBasicStrike(EClassType::HP).HPScale > 0.0f);
	FBWSkillStats Caster;
	Caster.INT = 8;
	TestEqual(TEXT("INT basic does not scale"), UBWDuelRules::ComputeCardSkillRange(UBWDuelRules::MakeBasicStrike(EClassType::INT), Caster).Max, 3);

	// 대신 카드 강화 배율은 INT·FTH가 크다
	TestTrue(TEXT("INT cards boost more than PHA cards"), UBWDuelRules::MakeClassCardPower(EClassType::INT) > UBWDuelRules::MakeClassCardPower(EClassType::PHA));
	TestTrue(TEXT("FTH cards boost more than TEC cards"), UBWDuelRules::MakeClassCardPower(EClassType::FTH) > UBWDuelRules::MakeClassCardPower(EClassType::TEC));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWCardBoostTest, "BeyondWing.Combat.Duel.CardBoost", BWDeckTest::Flags)
bool FBWCardBoostTest::RunTest(const FString& Parameters)
{
	// 강화한 공격 = 기본 공격 + 카드 범위 × 배율 (각각 내림)
	const FBWDiceRange Boost = UBWDuelRules::ScaleCardBoost(FBWDiceRange(5, 10), 0.5f);
	TestEqual(TEXT("half boost min"), Boost.Min, 2);
	TestEqual(TEXT("half boost max"), Boost.Max, 5);
	const FBWDiceRange Total = UBWDuelRules::AddCardBoost(FBWDiceRange(3, 7), FBWDiceRange(5, 10), 0.5f);
	TestEqual(TEXT("boosted min"), Total.Min, 5);
	TestEqual(TEXT("boosted max"), Total.Max, 12);

	const FBWDiceRange Strong = UBWDuelRules::ScaleCardBoost(FBWDiceRange(5, 10), 1.3f);
	TestEqual(TEXT("1.3x boost min"), Strong.Min, 6);
	TestEqual(TEXT("1.3x boost max"), Strong.Max, 13);

	const FBWDiceRange None = UBWDuelRules::AddCardBoost(FBWDiceRange(3, 7), FBWDiceRange(5, 10), 0.0f);
	TestEqual(TEXT("zero power adds nothing"), None.Max, 7);
	return true;
}

#undef BW_SKIP_IF_ENGINE_NOT_READY

#endif

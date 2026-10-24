#include "DeckComponent.h"
#include "BWCharacter.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/Core/Public/BWMapRules.h"
#include "Kismet/GameplayStatics.h"

UDeckComponent::UDeckComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	TurnCount = 0;
}

void UDeckComponent::BeginPlay()
{
	Super::BeginPlay();
	TurnCount = 0;

	// [수정] 적은 카드를 쓰지 않는다 (적 블루프린트에도 덱 컴포넌트가 붙어 있어 매번 에러가 찍히던 것)
	const ABWCharacter* OwnerChar = Cast<ABWCharacter>(GetOwner());
	if (OwnerChar && OwnerChar->bIsEnemy) return;
	LoadDeckByCharacterClass();
}

void UDeckComponent::LoadDeckByCharacterClass()
{
	if (!CardDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[DeckComponent] 메인 카드 데이터 테이블이 지정되지 않았습니다."));
		return;
	}

	ABWCharacter* OwnerCharacter = Cast<ABWCharacter>(GetOwner());
	if (!OwnerCharacter) return;

	const TArray<FName>* TargetSourceList = nullptr;
	const UBWGameInstance* GI = Cast<UBWGameInstance>(UGameplayStatics::GetGameInstance(this));

	// [추가] 판 덱(보상·강화·삭제가 반영된 덱)이 있으면 그것을 쓴다
	if (GI && GI->RunDeck.Num() > 0 && GI->SelectedClass == OwnerCharacter->CharacterClass)
	{
		Deck.Empty();
		for (const FBWRunCard& Entry : GI->RunDeck)
		{
			const FBWCardData* Found = CardDataTable->FindRow<FBWCardData>(Entry.Row, TEXT("DeckLoadContext"));
			if (!Found) { UE_LOG(LogTemp, Warning, TEXT("[DeckComponent] 테이블에서 카드를 검색할 수 없습니다: %s"), *Entry.Row.ToString()); continue; }
			Deck.Add(Entry.bUpgraded ? UBWMapRules::MakeUpgradedCard(*Found) : *Found);
		}
		for (int32 i = 0; i < Deck.Num() - 1; ++i) Deck.Swap(i, FMath::RandRange(i, Deck.Num() - 1));
		return;
	}

	// [추가] 삶의 방식 테이블(DT_Classes)에 시작 덱이 있으면 그것을 쓰고, 없으면 아래 직업별 배열을 쓴다
	TArray<FName> TableDeck;
	if (GI && GI->GetClassStartingDeck(OwnerCharacter->CharacterClass, TableDeck)) TargetSourceList = &TableDeck;
	else switch (OwnerCharacter->CharacterClass)
	{
	case EClassType::PHA: TargetSourceList = &PHAStartingDeck; break;
	case EClassType::INT: TargetSourceList = &INTStartingDeck; break;
	case EClassType::FTH: TargetSourceList = &FTHStartingDeck; break;
	case EClassType::TEC: TargetSourceList = &TECStartingDeck; break;
	case EClassType::HP:  TargetSourceList = &HPStartingDeck;  break;
	default:              TargetSourceList = &PHAStartingDeck; break;
	}

	if (!TargetSourceList) return;

	Deck.Empty();

	for (const FName& CardName : *TargetSourceList)
	{
		FBWCardData* FoundCard = CardDataTable->FindRow<FBWCardData>(CardName, TEXT("DeckLoadContext"));
		if (FoundCard)
		{
			Deck.Add(*FoundCard);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[DeckComponent] 테이블에서 카드를 검색할 수 없습니다: %s"), *CardName.ToString());
		}
	}

	int32 LastIndex = Deck.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		int32 Index = FMath::RandRange(i, LastIndex);
		if (i != Index)
		{
			Deck.Swap(i, Index);
		}
	}
}

void UDeckComponent::InitializeCombatDeck(const TArray<FBWCardData>& MasterDeck)
{
	Deck = MasterDeck;
	Hand.Empty();
	Discard.Empty();

	int32 LastIndex = Deck.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		int32 Index = FMath::RandRange(i, LastIndex);
		if (i != Index)
		{
			Deck.Swap(i, Index);
		}
	}
}

void UDeckComponent::StartTurn()
{
	TurnCount++;

	// 턴당 한도 초기화 (공격카드 1장, 효과카드 2장, 내가 여는 합 1회)
	AttackCardsUsedThisTurn = 0;
	EffectCardsUsedThisTurn = 0;
	DuelsOpenedThisTurn = 0;

	// [수정] 첫 턴은 InitialHandSize장, 그다음은 턴마다 CardsDrawnPerTurn장 (드로우 덱이 비면 DrawCards가 버린 덱을 섞어 다시 쓴다)
	DrawCards(TurnCount <= 1 ? InitialHandSize : CardsDrawnPerTurn);

	UE_LOG(LogTemp, Warning, TEXT("[턴 시작] %d턴 | 손패: %d장 | 덱: %d장 | 버린 덱: %d장"), TurnCount, Hand.Num(), Deck.Num(), Discard.Num());
}

void UDeckComponent::EndTurn()
{
	// [수정] 손패는 그대로 다음 턴으로 이어진다
}

void UDeckComponent::DrawCards(int32 Count)
{
	UE_LOG(LogTemp, Warning, TEXT("[DeckComponent] Attempting to draw %d cards. Current Deck: %d"), Count, Deck.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		if (Deck.Num() == 0)
		{
			ShuffleDiscardIntoDeck();
		}

		if (Deck.Num() > 0)
		{
			FBWCardData DrawnCard = Deck.Pop();
			if (Hand.Num() >= MaxHandSize)
			{
				// [추가] 손패가 가득 차면 뽑은 카드는 바로 버린 덱으로
				Discard.Add(DrawnCard);
				UE_LOG(LogTemp, Warning, TEXT("[DeckComponent] Hand full (%d): %s was discarded"), MaxHandSize, *DrawnCard.CardName);
				OnCardBurned.Broadcast(DrawnCard);
				continue;
			}
			Hand.Add(DrawnCard);
			UE_LOG(LogTemp, Log, TEXT("[DeckComponent] Card Drawn: %s"), *DrawnCard.CardName);
		}
		else
		{
			// 덱이 손패 크기보다 작으면 정상적으로 생기는 상황이라 Error가 아니다
			UE_LOG(LogTemp, Log, TEXT("[DeckComponent] No more cards to draw: deck and discard are both empty."));
			break; 
		}
	}
	OnHandChanged.Broadcast(Hand);
}

void UDeckComponent::ShuffleDiscardIntoDeck()
{
	if (Discard.Num() == 0) return;

	Deck.Append(Discard);
	Discard.Empty();

	int32 LastIndex = Deck.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		int32 Index = FMath::RandRange(i, LastIndex);
		if (i != Index)
		{
			Deck.Swap(i, Index);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[덱 고갈] 무덤 카드를 수거하여 복구 셔플했습니다."));
}

EBWCardUseResult UDeckComponent::CanUseCard(int32 HandIndex, int32 AvailableMP) const
{
	if (!Hand.IsValidIndex(HandIndex)) return EBWCardUseResult::InvalidIndex;

	const FBWCardData& Card = Hand[HandIndex];
	if (Card.CardKind == ECardKind::Attack && AttackCardsUsedThisTurn >= MaxAttackCardsPerTurn) return EBWCardUseResult::AttackLimit;
	if (Card.CardKind == ECardKind::Effect && EffectCardsUsedThisTurn >= MaxEffectCardsPerTurn) return EBWCardUseResult::EffectLimit;
	if (Card.MPCost > AvailableMP) return EBWCardUseResult::NotEnoughMP;
	return EBWCardUseResult::Ok;
}

bool UDeckComponent::HasAttackCardInHand() const
{
	return Hand.ContainsByPredicate([](const FBWCardData& Card) { return Card.CardKind == ECardKind::Attack; });
}

bool UDeckComponent::UseCard(int32 HandIndex, FBWCardData& OutCardData)
{
	ABWCharacter* OwnerChar = Cast<ABWCharacter>(GetOwner());
	const int32 AvailableMP = OwnerChar ? OwnerChar->CurrentMP : 0;

	const EBWCardUseResult Check = CanUseCard(HandIndex, AvailableMP);
	if (Check != EBWCardUseResult::Ok)
	{
		UE_LOG(LogTemp, Warning, TEXT("[사용 불가] 카드 %d: %s"), HandIndex, *UEnum::GetValueAsString(Check));
		return false;
	}

	OutCardData = Hand[HandIndex];
	if (OutCardData.CardKind == ECardKind::Attack) ++AttackCardsUsedThisTurn;
	else ++EffectCardsUsedThisTurn;
	if (OwnerChar) OwnerChar->CurrentMP -= OutCardData.MPCost;

	// 카드 사용 효과음
	if (GetWorld() && OutCardData.CardSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(GetWorld(), OutCardData.CardSound.Get());
	}

	Discard.Add(OutCardData);
	Hand.RemoveAt(HandIndex);

	OnHandChanged.Broadcast(Hand);
	UE_LOG(LogTemp, Log, TEXT("[카드 사용] %s (공격 %d/%d, 효과 %d/%d)"), *OutCardData.CardName,
		AttackCardsUsedThisTurn, MaxAttackCardsPerTurn, EffectCardsUsedThisTurn, MaxEffectCardsPerTurn);
	return true;
}
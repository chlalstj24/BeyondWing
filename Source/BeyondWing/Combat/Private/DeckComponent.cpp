#include "DeckComponent.h"
#include "BWCharacter.h"

UDeckComponent::UDeckComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CurrentAP = 0;
	MaxAP = 0;
}

void UDeckComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// [정정] 기획 요청 사양: AP 자원은 3으로 시작하며 최대 10까지 증가 가능
	MaxAP = 3;
	CurrentAP = MaxAP;

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

	switch (OwnerCharacter->CharacterClass)
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
		FCardData* FoundCard = CardDataTable->FindRow<FCardData>(CardName, TEXT("DeckLoadContext"));
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

void UDeckComponent::InitializeCombatDeck(const TArray<FCardData>& MasterDeck)
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
	// 1. AP 회복 및 스케일링 (최대 10)
	if (MaxAP < 10)
	{
		MaxAP++;
	}
	CurrentAP = MaxAP;

	// 2. 남은 패 버리기 (슬레이 더 스파이어 규칙)
	for (const FCardData& RemainingCard : Hand)
	{
		Discard.Add(RemainingCard);
	}
	Hand.Empty();

	// 3. 카드 드로우 (CardsToDrawPerTurn 사용)
	DrawCards(CardsToDrawPerTurn);

	// [핵심] 턴 시작 후 최종 패 상태를 UI에 즉시 보고
	OnHandChanged.Broadcast(Hand);
	OnAPChanged.Broadcast(CurrentAP, MaxAP);

	UE_LOG(LogTemp, Warning, TEXT("[턴 시작] AP: %d/%d | 드로우: %d장"), CurrentAP, MaxAP, CardsToDrawPerTurn);
}

void UDeckComponent::EndTurn()
{
	UE_LOG(LogTemp, Warning, TEXT("=== 플레이어 턴 종료 ==="));

	// [상대 턴 행동 처리]
	// 이곳에 적군의 행동 로직(AI)이 들어갈 자리입니다.
	// 예: EnemyManager->ExecuteEnemyTurn();

	UE_LOG(LogTemp, Warning, TEXT("=== 상대 턴 행동 처리 완료 ==="));

	// 다시 플레이어 턴 시작
	StartTurn();
}

void UDeckComponent::DrawCards(int32 Count)
{
	for (int32 i = 0; i < Count; ++i)
	{
		if (Deck.Num() == 0)
		{
			ShuffleDiscardIntoDeck();
		}

		if (Deck.Num() > 0)
		{
			Hand.Add(Deck.Pop());
		}
		else
		{
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

bool UDeckComponent::UseCard(int32 HandIndex, FCardData& OutCardData)
{
	if (!Hand.IsValidIndex(HandIndex)) return false;

	int32 CardCost = Hand[HandIndex].APCost; 

	if (CurrentAP >= CardCost)
	{
		CurrentAP -= CardCost;
		OnAPChanged.Broadcast(CurrentAP, MaxAP);
		OutCardData = Hand[HandIndex];
		
		Discard.Add(OutCardData);
		Hand.RemoveAt(HandIndex);

		OnHandChanged.Broadcast(Hand);
		UE_LOG(LogTemp, Log, TEXT("[카드 사용] %s 카드를 시전했습니다. 잔여 AP: %d"), *OutCardData.CardName, CurrentAP);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[사용 불가] AP 부족. 현재: %d | 요구: %d"), CurrentAP, CardCost);
	return false;
}

bool UDeckComponent::DiscardCardForBasicAction(int32 HandIndex)
{
	if (!Hand.IsValidIndex(HandIndex)) return false;

	int32 ActionCost = 1;

	if (CurrentAP >= ActionCost)
	{
		CurrentAP -= ActionCost;
		
		FCardData DiscardedCard = Hand[HandIndex];
		Discard.Add(DiscardedCard);
		Hand.RemoveAt(HandIndex);

		OnHandChanged.Broadcast(Hand);
		UE_LOG(LogTemp, Log, TEXT("[기본 행동] 카드를 소모했습니다. 잔여 AP: %d"), CurrentAP);
		return true;
	}
	return false;
}
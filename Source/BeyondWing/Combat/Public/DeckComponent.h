#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "Engine/DataTable.h"
#include "DeckComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHandChanged, const TArray<FBWCardData>&, CurrentHand);
// [추가] 손패가 가득 차서 뽑은 카드가 바로 버려졌다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardBurned, const FBWCardData&, BurnedCard);

// [추가] 카드를 지금 쓸 수 있는지, 못 쓴다면 왜인지
UENUM(BlueprintType)
enum class EBWCardUseResult : uint8
{
	Ok,
	InvalidIndex,
	AttackLimit,   // 공격카드는 턴당 1장
	EffectLimit,   // 효과카드는 턴당 2장
	NotEnoughMP    // INT·FTH 마법 카드의 MP 비용
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BEYONDWING_API UDeckComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeckComponent();

	UPROPERTY(BlueprintAssignable, Category = "Combat|Deck")
	FOnHandChanged OnHandChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Deck")
	FOnCardBurned OnCardBurned;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data")
	UDataTable* CardDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data|StartingDecks") TArray<FName> PHAStartingDeck;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data|StartingDecks") TArray<FName> INTStartingDeck;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data|StartingDecks") TArray<FName> FTHStartingDeck;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data|StartingDecks") TArray<FName> TECStartingDeck;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Data|StartingDecks") TArray<FName> HPStartingDeck;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Deck") TArray<FBWCardData> Deck;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Deck") TArray<FBWCardData> Hand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Deck") TArray<FBWCardData> Discard;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Resource") int32 TurnCount;

	// [추가] 턴당 한도 (증강 '연계', '이중 개시'로 늘어난다)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Limits") int32 MaxAttackCardsPerTurn = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Limits") int32 MaxEffectCardsPerTurn = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Limits") int32 MaxDuelsPerTurn = 1; // 내가 선공으로 여는 합

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Limits") int32 AttackCardsUsedThisTurn = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Limits") int32 EffectCardsUsedThisTurn = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Limits") int32 DuelsOpenedThisTurn = 0;

	FORCEINLINE const TArray<FBWCardData>& GetCurrentHand() const { return Hand; }

	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void LoadDeckByCharacterClass();
	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void InitializeCombatDeck(const TArray<FBWCardData>& MasterDeck);

	// 4-1: 턴 한도를 초기화하고 카드를 뽑는다 (첫 턴은 InitialHandSize장, 그다음부터 CardsDrawnPerTurn장)
	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void StartTurn();

	// 4-4: [수정] 손패는 버리지 않고 다음 턴으로 이어진다
	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void EndTurn();

	// [수정] 전투 첫 턴에 뽑는 장수 (기본 5, 증강 '넉넉한 손'으로 +1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Deck")
	int32 InitialHandSize = 5;

	// [추가] 둘째 턴부터 턴마다 뽑는 장수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Deck")
	int32 CardsDrawnPerTurn = 1;

	// [추가] 손패 최대 장수. 가득 찬 채로 뽑은 카드는 바로 버린 덱으로 간다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Deck")
	int32 MaxHandSize = 10;

	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void ShuffleDiscardIntoDeck();
	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void DrawCards(int32 Count);

	UFUNCTION(BlueprintPure, Category = "Combat|Action") EBWCardUseResult CanUseCard(int32 HandIndex, int32 AvailableMP) const;
	UFUNCTION(BlueprintPure, Category = "Combat|Action") bool HasAttackCardInHand() const;
	UFUNCTION(BlueprintPure, Category = "Combat|Action") bool CanOpenDuel() const { return DuelsOpenedThisTurn < MaxDuelsPerTurn; }

	// 한도와 MP를 확인하고 카드를 사용한다 (MP는 소유 캐릭터에서 차감). 사용한 카드는 버린 덱으로
	UFUNCTION(BlueprintCallable, Category = "Combat|Action") bool UseCard(int32 HandIndex, FBWCardData& OutCardData);

	// 합을 열었음을 기록한다 (턴당 MaxDuelsPerTurn회)
	UFUNCTION(BlueprintCallable, Category = "Combat|Action") void NotifyDuelOpened() { ++DuelsOpenedThisTurn; }
};

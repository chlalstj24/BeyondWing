#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GridTile.h"
#include "Engine/DataTable.h"
#include "DeckComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHandChanged, const TArray<FCardData>&, CurrentHand);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAPChanged, int32, CurrentAP, int32, MaxAP);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BEYONDWING_API UDeckComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeckComponent();

	UPROPERTY(BlueprintAssignable, Category = "Combat|Deck")
	FOnHandChanged OnHandChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Resource")
	FOnAPChanged OnAPChanged;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Deck") TArray<FCardData> Deck;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Deck") TArray<FCardData> Hand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Deck") TArray<FCardData> Discard;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Resource") int32 CurrentAP;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Resource") int32 MaxAP;

	FORCEINLINE const TArray<FCardData>& GetCurrentHand() const { return Hand; }

	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void LoadDeckByCharacterClass();
	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void InitializeCombatDeck(const TArray<FCardData>& MasterDeck);
	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void StartTurn();
	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void EndTurn();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Deck")
	int32 CardsToDrawPerTurn = 5;

	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void ShuffleDiscardIntoDeck();
	UFUNCTION(BlueprintCallable, Category = "Combat|Deck") void DrawCards(int32 Count);
	UFUNCTION(BlueprintCallable, Category = "Combat|Action") bool UseCard(int32 HandIndex, FCardData& OutCardData);
	UFUNCTION(BlueprintCallable, Category = "Combat|Action") bool DiscardCardForBasicAction(int32 HandIndex);
};
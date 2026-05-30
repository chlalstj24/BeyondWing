#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GridTile.h"
#include "BWCharacter.generated.h"

class UDeckComponent;

UCLASS()
class BEYONDWING_API ABWCharacter : public APawn
{
	GENERATED_BODY()

public:
	ABWCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Grid") FIntPoint OwnGridCoordinate;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BW Combat") UDeckComponent* DeckComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Character Settings") EClassType CharacterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 PHA;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 INT;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 FTH;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 TEC;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 MaxHP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 CurrentHP;

	UFUNCTION(BlueprintCallable, Category = "BW Combat|Calculations")
	int32 CalculateFinalCardDamage(const FCardData& CardInfo, bool& OutIsCritical);

protected:
	virtual void BeginPlay() override;
};
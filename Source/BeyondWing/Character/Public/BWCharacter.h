#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BeyondWing/Core/Public/BWTypes.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Character Settings") TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 PHA;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 INT;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 FTH;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 TEC;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 ATK; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") float ASP; 

	UFUNCTION(BlueprintPure, Category = "BW Status")
	int32 GetEffectiveATK() const { return PHA + TEC; }

	UFUNCTION(BlueprintPure, Category = "BW Status")
	int32 GetEffectiveASP() const { return 1 + (PHA / 20); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 MaxHP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 CurrentHP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 BaseMovePoints = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 CurrentMovePoints;
	
	// --- 적군 전용 AI 속성 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 BaseAttackPower;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 CurrentAttackBuff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW AI") TArray<FBWEnemyTurnPattern> CombatPatterns;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW AI") int32 CurrentPatternIndex;

	UPROPERTY(BlueprintReadOnly, Category = "BW AI") TArray<FIntPoint> LockedAttackTiles;
	UPROPERTY(BlueprintReadOnly, Category = "BW AI") TArray<FIntPoint> LockedMovePath;
	
	UPROPERTY(BlueprintReadOnly, Category = "BW AI") bool bIntentLocked = false;

	// --- 부드러운 이동 시스템 ---
	UPROPERTY(BlueprintReadOnly, Category = "BW Movement")
	TArray<FVector> VisualMoveQueue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Movement")
	float SmoothMoveSpeed = 15.0f;

	void AddMovementStep(FVector TargetWorldLoc);

	UFUNCTION(BlueprintCallable, Category = "BW AI")
	void UpdateEnemyIntent();

	UFUNCTION(BlueprintCallable, Category = "BW AI")
	void ExecuteNextPattern();

	UFUNCTION(BlueprintCallable, Category = "BW AI")
	void VisualizeEnemyIntent();

	// --- 전투 로직 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	bool bIsEnemy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	FString EnemyDisplayName;

	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void ReceiveDamage(int32 DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "BW Combat|Calculations")
	int32 CalculateFinalCardDamage(const FBWCardData& CardInfo, bool& OutIsCritical);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
};
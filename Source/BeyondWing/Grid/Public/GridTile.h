#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "GridTile.generated.h"

UENUM(BlueprintType)
enum class ETileState : uint8
{
	None,
	Hovered,
	MoveRange,
	AttackRange,
	AllyZone,
	EnemyZone
};

UENUM(BlueprintType)
enum class ESkillShape : uint8
{
	Single,
	Manhattan,
	Cross,
	T_Shape
};

UENUM(BlueprintType)
enum class EClassType : uint8
{
	Common,
	PHA,
	INT,
	FTH,
	TEC,
	HP
};

USTRUCT(BlueprintType)
struct FCardData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") FString CardName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") FString Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") float BaseDamage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") FString SubStatName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") FString SubStatValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") int32 APCost;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") TSoftObjectPtr<UTexture2D> AboveImage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") TSoftObjectPtr<UTexture2D> BodyImage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") TSoftObjectPtr<UTexture2D> BelowImage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") ESkillShape SkillShape;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") int32 Range;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") ETileState DisplayColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") EClassType ClassType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData") TSoftObjectPtr<USoundBase> CardSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData|Combat") EClassType ScalingStat;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardData|Combat") float DamageScale;
};

UCLASS()
class BEYONDWING_API AGridTile : public AActor
{
	GENERATED_BODY()
	
public:	
	AGridTile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid") FIntPoint GridCoordinate;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid") UStaticMeshComponent* TileMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid") class UBoxComponent* CollisionBox;
	UPROPERTY(BlueprintReadWrite, Category = "Grid") ETileState DefaultZoneState;

	UFUNCTION(BlueprintCallable, Category = "Grid") void SetTileState(ETileState NewState);

protected:
	virtual void BeginPlay() override;

public:
	ETileState CurrentState;
	UPROPERTY() UMaterialInstanceDynamic* DynamicMaterial;
};
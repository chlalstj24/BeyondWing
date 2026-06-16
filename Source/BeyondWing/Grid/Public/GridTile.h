#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "GridTile.generated.h"

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
	void StartLandingAnimation(FVector TargetLoc, float Duration);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	bool bIsAnimating = false;
	FVector TargetLocation;
	float AnimationTimer = 0.0f;
	float AnimationDuration = 0.5f;
	FVector StartLocation;

public:
	ETileState CurrentState;
	UPROPERTY() UMaterialInstanceDynamic* DynamicMaterial;
};
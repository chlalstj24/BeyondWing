#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GridTile.h"
#include "BWCharacter.h"
#include "BWPlayerController.generated.h"

class AGridManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStartedSignature);

UCLASS()
class BEYONDWING_API ABWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABWPlayerController();

	// 카드 선택 기능: 블루프린트에서 호출 가능
	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void SelectCard(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void CancelSelection();

	// 핸드 위젯 직접 등록
	void SetHandWidget(class UBWHandWidget* InWidget) { HandWidget = InWidget; }

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	int32 SelectedCardIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	bool bIsSkillTargetingMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	int32 CurrentSkillRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	TSubclassOf<ABWCharacter> CharacterClassToSpawn;

	UPROPERTY(BlueprintAssignable, Category = "BW Combat")
	FOnCombatStartedSignature OnCombatStarted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat|Sounds")
	TSoftObjectPtr<USoundBase> InsufficientAPSound;

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	ABWCharacter* PossessedBWCharacter;

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	AGridTile* GetTileUnderCursor();

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

private:
	void OnLeftClick();
	void OnRightClick();
	void TraceGridUnderCursor();

	UPROPERTY()
	class UBWHandWidget* HandWidget;

	UPROPERTY()
	AGridManager* TargetGridManager;

	UPROPERTY()
	AGridTile* LastHoveredTile;

	UPROPERTY()
	AGridTile* CurrentTileUnderCursor;
};
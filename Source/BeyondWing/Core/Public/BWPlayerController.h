#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GridTile.h"
#include "BWCharacter.h"
#include "BWPlayerController.generated.h"

class AGridManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStartedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetCharacterHovered, ABWCharacter*, TargetChar);

UCLASS()
class BEYONDWING_API ABWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABWPlayerController();

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void SelectCard(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void CancelSelection();

	void SetHandWidget(class UBWHandWidget* InWidget) { HandWidget = InWidget; }
	void SetBattleHUDWidget(class UBWBattleHUDWidget* InWidget) { BattleHUDWidget = InWidget; }

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	int32 SelectedCardIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	bool bIsSkillTargetingMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	bool bIsMovementMode;

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void EnterMovementMode();

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void EnterBasicAttackMode(); // [추가] 기본 공격 모드 진입

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	int32 CurrentSkillRange;

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	bool bIsBasicAttackMode; // [추가] 기본 공격 모드 여부

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	FBWCardData BasicAttackTemplate; // [추가] 기본 공격용 데이터 템플릿

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	TSubclassOf<ABWCharacter> CharacterClassToSpawn;

	// 직업별 고스트 매핑 (C++에서 자동으로 관리)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	TMap<TSubclassOf<ABWCharacter>, TSubclassOf<AActor>> PlacementPreviewMap;

	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void RequestEndTurn();

	// --- 순차적 적 턴 처리 ---
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void ProcessNextEnemyTurn();

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	TArray<ABWCharacter*> EnemyTurnQueue;

	UPROPERTY(BlueprintAssignable, Category = "BW Combat")
	FOnCombatStartedSignature OnCombatStarted;

	UPROPERTY(BlueprintAssignable, Category = "BW Combat")
	FOnTargetCharacterHovered OnTargetCharacterHovered;

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	ABWCharacter* FocusedCharacter;

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
	
	void UpdatePlacementPreview();
	void DestroyPlacementPreview();

	UPROPERTY() class UBWHandWidget* HandWidget;
	UPROPERTY() class UBWBattleHUDWidget* BattleHUDWidget;
	UPROPERTY() AGridManager* TargetGridManager;
	UPROPERTY() AGridTile* LastHoveredTile;
	UPROPERTY() AGridTile* CurrentTileUnderCursor;
	UPROPERTY() AActor* PlacementPreviewActor;
};
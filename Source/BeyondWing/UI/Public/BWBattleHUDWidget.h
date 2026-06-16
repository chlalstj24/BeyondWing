#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWBattleHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UHorizontalBox;
class UButton;
class ABWCharacter;

UCLASS()
class BEYONDWING_API UBWBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBWBattleHUDWidget(const FObjectInitializer& ObjectInitializer);

	// C++에서 안전하게 덱 컴포넌트를 가져오는 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|HUD")
	class UDeckComponent* GetPlayerDeckComponent();

	// 캐릭터 데이터를 받아 HUD 초기화
	UFUNCTION(BlueprintCallable, Category = "Combat|HUD")
	void InitializeBattleHUD(class ABWCharacter* InCharacter);

	// --- 툴팁 제어 ---
	UFUNCTION(BlueprintCallable, Category = "Combat|HUD")
	void ShowTooltip(const FString& Title, const FString& RawDesc, int32 V = 0, int32 D = 0, int32 R = 0);

	UFUNCTION(BlueprintCallable, Category = "Combat|HUD")
	void HideTooltip();

	// --- 에디터 설정용 툴팁 데이터 ---
	UPROPERTY(EditAnywhere, Category = "Combat|Tooltips") FBWTooltipConfig AP_Tooltip;
	UPROPERTY(EditAnywhere, Category = "Combat|Tooltips") FBWTooltipConfig Stat_Tooltip;
	UPROPERTY(EditAnywhere, Category = "Combat|Tooltips") FBWTooltipConfig Gold_Tooltip;
	UPROPERTY(EditAnywhere, Category = "Combat|Tooltips") FBWTooltipConfig Floor_Tooltip;
	UPROPERTY(EditAnywhere, Category = "Combat|Tooltips") FBWTooltipConfig CardCount_Tooltip;
	UPROPERTY(EditAnywhere, Category = "Combat|Tooltips") FBWTooltipConfig AllyZone_Tooltip; // [추가] 아군 배치 영역 툴팁
	UPROPERTY(EditAnywhere, Category = "Combat|Tooltips") FBWTooltipConfig MoveRange_Tooltip; // [추가] 이동 가능 범위 툴팁

public:
	// --- Blueprint Bindings ---
	
	// [수정] UE5.5의 엄격한 UHT 규칙에 따라 BlueprintReadOnly 위젯 바인딩을 public으로 이동
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UBWTooltipWidget* WBP_ToolTipHUD;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UWidget* AP_Group;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UTextBlock* AP_Text;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	class UTextBlock* MP_Text;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* BTN_EndTurn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* BTN_BasicAttack; 

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* PlayerPortrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TXT_Floor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* PB_PlayerHP;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TXT_PlayerHP;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TXT_Gold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TXT_CardCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UProgressBar* PB_Enemy1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Enemy1Name;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Enemy1HP;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* IMG_Enemy1Portrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UProgressBar* PB_Enemy2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Enemy2Name;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Enemy2HP;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* IMG_Enemy2Portrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UProgressBar* PB_Enemy3;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Enemy3Name;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Enemy3HP;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* IMG_Enemy3Portrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_PlayerStats;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// AP가 바뀔 때마다 블루프린트에서 호출될 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|HUD")
	void K2_UpdateAP(int32 CurrentAP, int32 MaxAP);

private:
	UFUNCTION()
	void HandleAPChanged(int32 CurrentAP, int32 MaxAP);

	UFUNCTION()
	void OnEndTurnClicked();

	UFUNCTION()
	void OnBasicAttackClicked(); 

	UFUNCTION()
	void OnTargetHovered(ABWCharacter* TargetChar);

	UPROPERTY()
	ABWCharacter* CachedPlayerChar;

	UPROPERTY()
	ABWCharacter* CurrentHoveredEnemy;
};

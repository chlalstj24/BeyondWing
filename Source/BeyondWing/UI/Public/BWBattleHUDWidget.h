#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "Styling/SlateTypes.h"
#include "BWBattleHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UHorizontalBox;
class UButton;
class UBorder;
class UCanvasPanel;
class UWidget;
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

	// [추가] 화면 위쪽 안내판 (합 결과, 다음에 할 일). Duration이 0 이하면 다음 문구가 올 때까지 유지.
	// Style에 따라 테두리 색·효과가 다르다 (Warning은 붉게 흔들림, Highlight는 청록으로 살짝 커짐)
	UFUNCTION(BlueprintCallable, Category = "Combat|HUD")
	void ShowPrompt(const FString& Message, float Duration = 3.0f, EBWPromptStyle Style = EBWPromptStyle::Info);

	// WBP에 PromptText가 없으면 NativeConstruct에서 안내판(테두리·표시·글)을 만들어 붙인다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* PromptText;

	// [추가] 왼쪽 아래 '기본 공격' 칸: 지금 기본 공격 숫자 범위, 공격카드를 집거나 올리면 강화했을 때의 범위
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TXT_AttackValue;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TXT_AttackDetail;

	// [추가] 덱·버린 카드 장수. WBP에 없으면 왼쪽 아래에 만든다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* DeckText;

	// --- 에디터 설정용 툴팁 데이터 ---
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

	// [추가] 화면 위쪽 적 목록. 이제 적 머리 위에 HP·행동이 떠서 기본은 숨긴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD") bool bShowEnemyList = false;
	// [추가] 위쪽 안내판 글의 최대 폭 (넘으면 두 줄)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD") float PromptMaxWidth = 760.0f;
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

	// [추가] 코드로 다시 짠 HUD: 왼쪽 위 삶의 방식 판(이름·HP·고유 자원·능력치), 오른쪽 위 골드·차원 판,
	// 왼쪽 아래 덱 더미·오른쪽 아래 버린 카드 더미, 오른쪽 버튼 줄. false면 디자이너 화면 그대로
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD") bool bUseCodeLayout = true;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;


private:

	UFUNCTION()
	void OnEndTurnClicked();

	UFUNCTION()
	void OnBasicAttackClicked(); 

	UFUNCTION()
	void OnTargetHovered(ABWCharacter* TargetChar);

	UPROPERTY()
	ABWCharacter* CachedPlayerChar;

	float PromptExpireTime = -1.0f;

	// 안내판 연출 상태
	void BuildPromptBanner(class UCanvasPanel* RootCanvas);
	void BuildAttackPanel(class UCanvasPanel* RootCanvas);
	void UpdatePromptBanner(float DeltaTime);
	void UpdateAttackPanel();
	UPROPERTY() TObjectPtr<UWidget> PromptPanel;     // 안내판 전체 (나타남·흔들림·사라짐)
	UPROPERTY() TObjectPtr<class UBorder> PromptFrame; // 위아래 테두리 색
	UPROPERTY() TObjectPtr<UImage> PromptIcon;        // 왼쪽 마름모
	EBWPromptStyle PromptStyle = EBWPromptStyle::Info;
	float PromptAge = 0.0f;      // 문구가 바뀐 뒤 지난 시간
	bool bPromptSkinned = false;
	float PromptFadeOut = -1.0f; // 사라지는 중이면 0~0.25
	UPROPERTY() TObjectPtr<UWidget> AttackPanel;
	UPROPERTY() TObjectPtr<UImage> AttackAccent;
	// [추가] 왼쪽 아래 '마나' 칸 (MP를 쓰는 삶의 방식만): 보유 MP 숫자 + 칸 막대 + 가드 효율
	void UpdateManaPanel(float DeltaTime);
	UPROPERTY() TObjectPtr<UWidget> ManaPanel;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_ManaTitle;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_ManaValue;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_ManaDetail;

	// [추가] 방어도: HP 바가 푸르게 바뀌고, HP 바 왼쪽 끝에 방패 모양과 수치 (슬레이 더 스파이어식)
	UPROPERTY(EditAnywhere, Category = "BW HUD|Block") FLinearColor BlockBarColor = FLinearColor(0.22f, 0.5f, 1.0f, 1.0f);
	UPROPERTY(EditAnywhere, Category = "BW HUD|Block") float BlockBadgeSize = 54.0f;
	UPROPERTY(EditAnywhere, Category = "BW HUD|Block") TSoftObjectPtr<UTexture2D> BlockShieldTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/HUD/Texture/UI/T_UI_Shield.T_UI_Shield")));
	UPROPERTY() TObjectPtr<UWidget> BlockBadge;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_BlockValue;
	FProgressBarStyle OriginalHPBarStyle;
	FLinearColor OriginalHPFillColor = FLinearColor::White;
	bool bHPBarBlue = false;
	int32 ShownBlock = -1;
	float BlockPulse = 0.0f;
	void BuildBlockBadge();
	void UpdateBlockDisplay(int32 Block, float DeltaTime);
	UPROPERTY() TObjectPtr<UHorizontalBox> ManaCellRow;
	UPROPERTY() TArray<TObjectPtr<UImage>> ManaCells;
	int32 ShownMP = -1;
	float ManaFlash = 0.0f; // MP가 바뀐 직후 숫자가 반짝인다

	UPROPERTY()
	ABWCharacter* CurrentHoveredEnemy;

	// 코드 HUD
	void BuildCodeLayout(UCanvasPanel* RootCanvas);
	void UpdateCodeLayout();
	void StyleButton(UButton* Button, const FLinearColor& Accent, int32 FontSize, const FVector2D& Size, const FVector2D& Position);
	UWidget* MakePile(const FString& Label, bool bRight, UTextBlock*& OutCount);
	UTextBlock* MakeHUDText(int32 Size, const FLinearColor& Color, bool bBold = false);
	FString DescribeBattlefield() const; // 이번 전투 차원의 기믹 한 줄 (차원 없으면 빈 글)
	UPROPERTY() TObjectPtr<UBorder> ClassChip;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_ClassChip;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_ClassName;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_RunInfo;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_LifeLine;
	// [추가] 삶의 방식 전용 '고유 자원' 판: 줄마다 [이름표] 값 + 한 줄 설명 (광기·상처 / 장전·마탄·들림 / 명상·영창 / 어둠·테네브리스 / 방어도·질풍노도)
	UPROPERTY() TObjectPtr<class UVerticalBox> ResourceBox;
	// [추가] 모은 세트: 이름과 눈금만 작게, 마우스를 올리면 단계별 효과
	UPROPERTY() TObjectPtr<class UVerticalBox> SetBox;
	struct FSetRow { TWeakObjectPtr<class UWidget> Widget; FString Title; FString Desc; };
	TArray<FSetRow> SetRows;
	FString ShownSetKey;
	void UpdateSetPanel();
	// [추가] 마우스를 올린 적의 양 옆 카드: 왼쪽 = 내 턴 반격, 오른쪽 = 적 턴 행동
	UPROPERTY() TObjectPtr<class UBorder> EnemyCard[2];
	UPROPERTY() TObjectPtr<class UTextBlock> TXT_EnemyCardKind[2];
	UPROPERTY() TObjectPtr<class UTextBlock> TXT_EnemyCardName[2];
	UPROPERTY() TObjectPtr<class UTextBlock> TXT_EnemyCardValue[2];
	UPROPERTY() TObjectPtr<class UTextBlock> TXT_EnemyCardDesc[2];
	UPROPERTY(EditAnywhere, Category = "BW HUD") float EnemyCardWidth = 220.0f;
	void BuildEnemyCards(class UCanvasPanel* RootCanvas);
	void UpdateEnemyCards();
	FString ShownResourceKey;
	void UpdateResourcePanel(const FLinearColor& ClassColor);
	UPROPERTY() TObjectPtr<UTextBlock> TXT_StatLine;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_GoldValue;
	UPROPERTY() TObjectPtr<UBorder> DimPlate;
	UPROPERTY() TObjectPtr<UBorder> DimDot;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_DimName;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_DimDetail;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_DrawCount;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_DiscardCount;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_HandCount;
};

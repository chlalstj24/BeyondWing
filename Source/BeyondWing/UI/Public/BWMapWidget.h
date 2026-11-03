#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWMapTypes.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWMapWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UVerticalBox;
class UBWMapWidget;

// 알림 창 버튼 하나의 클릭을 지도 화면으로 넘긴다 (버튼마다 번호가 달라 동적 델리게이트용 UObject로 둔다)
UCLASS()
class BEYONDWING_API UBWMapOptionHandler : public UObject
{
	GENERATED_BODY()

public:
	int32 Index = 0; // ToggleUpgradeIndex면 '강화 미리보기' 버튼
	TWeakObjectPtr<UBWMapWidget> Owner;
	static constexpr int32 ToggleUpgradeIndex = -100;

	UFUNCTION() void HandleClicked();
};

// 지도 화면 (지도 기획서). bUseCodeLayout이면 디자이너 화면 대신 코드로 화면을 만든다:
// 4줄(차원) × 15칸 + 보스, 차원문 개폐, 가시거리 2칸, 줄마다 방치 빛·연속 체류, 칸 위에 올리면 도착 시 일어날 일을 미리 보여 준다.
// 칸을 고르면 차원문 통과 → 방치 보너스 → 과부하 → 노드 진입(전투·휴식·상점·보물·이벤트·제단·균열) 순서로 처리하고,
// 전투에서 돌아오면 보상(카드 3장 중 1장 등), 보스를 이기면 다음 막으로 넘어간다.
UCLASS()
class BEYONDWING_API UBWMapWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class UBWAutoPlayer;

public:
	// 옛 디자이너 화면용 (bUseCodeLayout = false일 때만 쓴다)
	UPROPERTY(EditAnywhere, Category = "BW Map")
	TSubclassOf<class UBWMapNodeWidget> NodeWidgetClass;

	UPROPERTY(meta = (BindWidgetOptional))
	class UPanelWidget* MapMainContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	class UScrollBox* MapScrollBox;

	UPROPERTY(EditAnywhere, Category = "BW Map|Design")
	FVector2D NodeSize = FVector2D(180.0f, 180.0f);

	UPROPERTY(EditAnywhere, Category = "BW Map|Design")
	float ColumnSpacing = 250.0f;

	UPROPERTY(EditAnywhere, Category = "BW Map|Design")
	float RowSpacing = 100.0f;

	UPROPERTY(EditAnywhere, Category = "BW Map|Navigation")
	float ScrollSensitivity = 2.0f;

	// --- 코드 화면 ---
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") bool bUseCodeLayout = true;
	// [추가] 보상·상점에서 카드를 보여 줄 카드 위젯 (비면 /Game/HUD/BP/WBP_Card)과 크기
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") TSubclassOf<class UBWCardWidget> CardWidgetClass;
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") FVector2D RewardCardSize = FVector2D(230.0f, 339.0f);
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float MapLeft = 300.0f;      // 1칸 가운데 x
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float ColumnWidth = 100.0f;  // 칸 사이 거리
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float FirstRowY = 410.0f;    // 맨 위 줄 가운데 y
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float RowHeight = 150.0f;    // 줄 사이 거리
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float NodeBoxSize = 66.0f;
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float BossBoxSize = 120.0f;
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float SwipeTime = 0.5f;      // 차원문 스와이프 연출

	UPROPERTY(EditAnywhere, Category = "BW Map|Level") FName BattleLevelName = TEXT("/Game/System/Level/BattleLevel");
	UPROPERTY(EditAnywhere, Category = "BW Map|Level") FName TitleLevelName = TEXT("/Game/System/Level/TitleLevel");

	// 지도 전체 새로고침 및 렌더링
	UFUNCTION(BlueprintCallable, Category = "BW Map")
	void RenderMap();

	// 현재 진행 단계로 스크롤 이동 (옛 화면)
	UFUNCTION(BlueprintCallable, Category = "BW Map")
	void ScrollToCurrentStep();

	// 그 칸으로 이동하고 노드에 들어간다 (갈 수 없으면 false)
	UFUNCTION(BlueprintCallable, Category = "BW Map")
	bool TryMoveTo(int32 Step, int32 Row);

	// 알림 창 (보상·휴식·상점·이벤트 등)
	UFUNCTION(BlueprintCallable, Category = "BW Map") void ChooseModalOption(int32 Index);
	UFUNCTION(BlueprintPure, Category = "BW Map") bool IsModalOpen() const { return bModalOpen; }
	// [추가] Esc: 창을 닫는 선택지(닫기·떠나기·취소 등)가 있으면 그걸 누른다. 꼭 골라야 하는 창은 그대로
	void CloseModalByEscape();
	UFUNCTION(BlueprintPure, Category = "BW Map") FString GetModalTitle() const { return ModalTitleString; }
	UFUNCTION(BlueprintPure, Category = "BW Map") TArray<FString> GetModalOptionLabels() const;
	// [추가] 카드 목록 창의 '강화 미리보기' (켜면 모든 카드를 강화된 모습으로, 다시 누르면 원래대로)
	UFUNCTION(BlueprintCallable, Category = "BW Map") void ToggleUpgradePreview();
	UFUNCTION(BlueprintPure, Category = "BW Map") bool IsUpgradePreviewOn() const { return bPreviewUpgrades; }

	// 덱 보기 + 효과카드의 지도 사용 (HP 회복 카드, 같은 카드는 막당 1회)
	UFUNCTION(BlueprintCallable, Category = "BW Map") void ShowDeck();
	// [추가] 화면 확인용: 콘솔 bw.UI shop|rest|reward|stat|augment|deck
	UFUNCTION(BlueprintCallable, Category = "BW Map") void DebugShowScreen(const FString& Which);

	// 칸 위치 (코드 화면 좌표, 1920×1080 기준)
	FVector2D GetNodeCenter(int32 Step, int32 Row) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	struct FModalOption
	{
		FText Label;
		FText Detail;
		bool bEnabled = true;
		TFunction<void()> OnChosen;
		// [추가] 카드를 고르는 선택지면 글자 대신 카드 그림을 그대로 보여 준다 (슬레이 더 스파이어식 보상)
		bool bShowCard = false;
		FBWCardData Card;
		bool bDimWhenDisabled = true; // 덱 보기처럼 '못 누를 뿐 보기는 하는' 카드는 흐리게 하지 않는다
		// [추가] 상점: 가격표(0 이상이면 설명 대신 동전 + 값), 할인 전 값, 팔린 자리
		int32 Price = -1;
		int32 OriginalPrice = -1;
		bool bSoldOut = false;
	};
	enum class EModalStyle : uint8 { Normal, Shop };
	// [추가] 카드 그림 선택지 하나 만들기 (Caption = 카드 밑 첫 줄: 가격·강화 등)
	static FModalOption MakeCardOption(const FBWCardData& Card, const FString& Caption, bool bEnabled, TFunction<void()> OnChosen);

	struct FNodeView
	{
		int32 Step = 0;
		int32 Row = 0;
		FVector2D Center = FVector2D::ZeroVector;
		float Size = 0.0f;
		UBorder* Box = nullptr;
		FLinearColor Fill = FLinearColor::Black;
		FLinearColor Outline = FLinearColor::Black;
		bool bSelectable = false;
		bool bCurrent = false;
	};

	struct FRowView
	{
		UBorder* Band = nullptr;
		UBorder* Glow = nullptr;
		UTextBlock* Neglect = nullptr;
		int32 GlowLevel = 0; // 0 없음, 1 활성, 2 깊은 방치
	};

	// 화면 만들기
	void BuildCodeLayout();
	void RenderLegacyMap();
	void RebuildMapLayer();
	void RefreshTopBar();
	void UpdateHover(const FVector2D& CanvasPos);
	void ShowInfoFor(int32 Step, int32 Row);
	UBorder* AddBox(UCanvasPanel* Parent, const FVector2D& Center, const FVector2D& Size, const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f);
	UTextBlock* AddText(UCanvasPanel* Parent, const FString& Text, const FVector2D& Pos, int32 FontSize, const FLinearColor& Color, bool bBold = false, const FVector2D& Alignment = FVector2D(0.0f, 0.0f), float WrapAt = 0.0f);
	void AddLine(UCanvasPanel* Parent, const FVector2D& From, const FVector2D& To, float Thickness, const FLinearColor& Color);
	UTextBlock* MakeText(const FString& Text, int32 FontSize, const FLinearColor& Color, bool bBold = false);
	FVector2D CursorToCanvas() const;
	int32 FindNodeAt(const FVector2D& CanvasPos) const;

	// 흐름: 보상 → 카드 삭제 → 막 클리어 → 증강 → 이동 대기
	void ProcessPendingFlow();
	void ShowRewards(const FString& Title);
	// Count장 지우기 (bFromBonus면 타이가 보너스로 남은 장수 PendingCardRemovals를 쓴다). OnDone(실제로 지운 장수)
	void ShowCardRemoval(int32 Count, bool bFromBonus, TFunction<void(int32)> OnDone, int32 RemovedSoFar = 0);
	void ShowCardUpgrade(const FString& Title, TFunction<void()> OnDone, TFunction<void()> OnCancel);
	void ShowArrival();
	void EnterCurrentNode();
	void ShowRest();
	void ShowShop();
	void ShowMystery();
	void ShowStatAllocation();
	void RefreshAugmentStrip();
	void FinishNode();
	void OpenAugmentPick();
	void CheckAugmentClosed();
	void FinishAutoAugment();
	bool bAutoBattlePending = false;  // 측정 봇: 전투 노드에 들어섰다
	bool bAutoAugmentPending = false; // 측정 봇: 증강을 골라야 한다
	void BeginSwipe(EDimensionType Dimension);

	UFUNCTION() void OnDeckButtonClicked();

	void OpenModal(const FString& Title, const FString& Body, TArray<FModalOption> Options, EModalStyle Style = EModalStyle::Normal);
	class UWidget* MakePriceTag(int32 Price, int32 OriginalPrice, bool bAffordable);
	class UWidget* MakeUpgradeToggle();
	// [추가] 카드 선택지 하나 (카드 그림 + 설명, 누르면 그 선택지)
	class UWidget* MakeCardChoice(const FModalOption& Option, int32 Index, const FVector2D& CardSize);
	void CloseModal();
	FString DescribeCard(const FBWCardData& Card) const;

	UPROPERTY() UCanvasPanel* RootCanvas = nullptr;
	UPROPERTY() UCanvasPanel* MapLayer = nullptr;
	UPROPERTY() UTextBlock* ActText = nullptr;
	UPROPERTY() UTextBlock* ProgressText = nullptr;
	UPROPERTY() UTextBlock* StatsText = nullptr;
	UPROPERTY() UTextBlock* BonusText = nullptr;
	UPROPERTY() UTextBlock* InfoTitle = nullptr;
	UPROPERTY() UTextBlock* InfoBody = nullptr;
	UPROPERTY() UBorder* SwipeOverlay = nullptr;
	UPROPERTY() UBorder* ModalLayer = nullptr;
	UPROPERTY() class USizeBox* ModalPanelSize = nullptr;
	UPROPERTY() class USizeBox* ModalScrollSize = nullptr;
	UPROPERTY() UTextBlock* ModalTitle = nullptr;
	UPROPERTY() UBorder* ModalPanel = nullptr;
	UPROPERTY() class UHorizontalBox* ModalHeaderRight = nullptr; // 제목 줄 오른쪽 (강화 미리보기·상점 골드)
	UPROPERTY() UTextBlock* ModalBody = nullptr;
	UPROPERTY() UVerticalBox* ModalOptionBox = nullptr;
	UPROPERTY() TArray<TObjectPtr<UBWMapOptionHandler>> OptionHandlers;
	UPROPERTY() TObjectPtr<class UBWAugmentSelectWidget> AugmentWidget = nullptr;

	TArray<FNodeView> NodeViews;
	TArray<FRowView> RowViews;
	TArray<FModalOption> ModalOptions;
	FString ModalTitleString;
	TArray<FName> ShopCards;
	TArray<bool> ShopSold;      // 산 카드 자리는 '판매 완료'로 남긴다
	int32 ShopSaleIndex = INDEX_NONE; // 반값 카드
	FString ModalBodyString;
	EModalStyle ModalStyle = EModalStyle::Normal;
	bool bPreviewUpgrades = false;
	bool bModalOpen = false;
	float ModalOpenedAt = 0.0f;
	float ModalLockUntil = 0.0f; // 연타로 다음 창까지 눌리지 않게 이 시각까지 입력을 막는다
	UPROPERTY(Transient) TObjectPtr<class UHorizontalBox> AugmentStrip = nullptr;

	// 상단 HUD (층·진행 / 덱 / HP·골드 / 증강·세트·유물 칸 / 받아 둔 효과)
	UPROPERTY(Transient) TObjectPtr<UBorder> FloorPlate = nullptr;
	UPROPERTY(Transient) TObjectPtr<class UButton> DeckButtonWidget = nullptr;
	UPROPERTY(Transient) TObjectPtr<class UHorizontalBox> TopRightBar = nullptr;
	UPROPERTY(Transient) TObjectPtr<class UHorizontalBox> SetStrip = nullptr;
	UPROPERTY(Transient) TObjectPtr<class UHorizontalBox> RelicStrip = nullptr;
	UPROPERTY(Transient) TObjectPtr<class UHorizontalBox> BonusStrip = nullptr;
	// 마우스를 올리면 뜨는 설명 창 (전투 HUD와 같은 WBP_ToolTipHUD, 폭은 글 길이만큼)
	UPROPERTY(Transient) TObjectPtr<class UBWTooltipWidget> MapTooltip = nullptr;
	struct FHoverTip { TWeakObjectPtr<UWidget> Widget; FString Title; FString Desc; };
	TArray<FHoverTip> TopTips;   // 상단 HUD (RefreshTopBar 때 새로)
	TArray<FHoverTip> ModalTips; // 열린 창 안 (OpenModal 때 새로)
	FString NodeTipTitle, NodeTipDesc; // 지금 마우스가 올라간 칸의 설명
	void UpdateTooltip();
	class UWidget* MakeIcon(const FString& Name, float Size, const FString& Fallback = FString(), const FLinearColor& FallbackColor = FLinearColor::White);
	UBorder* MakeChip(UWidget* Content, const FMargin& ChipPadding = FMargin(12.0f, 6.0f));
	UBorder* MakeSlot(UWidget* Content, float Size, const FLinearColor& Edge, float EdgeWidth);
	bool bBusy = false;            // 이동·연출·레벨 전환 중
	int32 HoveredNode = INDEX_NONE;
	float Time = 0.0f;
	float SwipeElapsed = -1.0f;
	FLinearColor SwipeColor = FLinearColor::White;

	// 옛 화면 드래그 스크롤
	bool bIsDraggingMap = false;
	FVector2D LastMousePos;
};

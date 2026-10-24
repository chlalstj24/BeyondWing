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
	int32 Index = 0;
	TWeakObjectPtr<UBWMapWidget> Owner;

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
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float MapLeft = 300.0f;      // 1칸 가운데 x
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float ColumnWidth = 100.0f;  // 칸 사이 거리
	UPROPERTY(EditAnywhere, Category = "BW Map|Layout") float FirstRowY = 290.0f;    // 맨 위 줄 가운데 y
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
	UFUNCTION(BlueprintPure, Category = "BW Map") FString GetModalTitle() const { return ModalTitleString; }
	UFUNCTION(BlueprintPure, Category = "BW Map") TArray<FString> GetModalOptionLabels() const;

	// 덱 보기 + 효과카드의 지도 사용 (HP 회복 카드, 같은 카드는 막당 1회)
	UFUNCTION(BlueprintCallable, Category = "BW Map") void ShowDeck();

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
	};

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
	void FinishNode();
	void OpenAugmentPick();
	void BeginSwipe(EDimensionType Dimension);

	UFUNCTION() void OnDeckButtonClicked();

	void OpenModal(const FString& Title, const FString& Body, TArray<FModalOption> Options);
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
	UPROPERTY() UTextBlock* ModalTitle = nullptr;
	UPROPERTY() UTextBlock* ModalBody = nullptr;
	UPROPERTY() UVerticalBox* ModalOptionBox = nullptr;
	UPROPERTY() TArray<TObjectPtr<UBWMapOptionHandler>> OptionHandlers;
	UPROPERTY() TObjectPtr<class UBWAugmentSelectWidget> AugmentWidget = nullptr;

	TArray<FNodeView> NodeViews;
	TArray<FRowView> RowViews;
	TArray<FModalOption> ModalOptions;
	FString ModalTitleString;
	TArray<FName> ShopCards;
	bool bModalOpen = false;
	bool bBusy = false;            // 이동·연출·레벨 전환 중
	int32 HoveredNode = INDEX_NONE;
	float Time = 0.0f;
	float SwipeElapsed = -1.0f;
	FLinearColor SwipeColor = FLinearColor::White;

	// 옛 화면 드래그 스크롤
	bool bIsDraggingMap = false;
	FVector2D LastMousePos;
};

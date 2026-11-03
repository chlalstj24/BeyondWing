#include "BeyondWing/UI/Public/BWBattleResultWidget.h"
#include "BeyondWing/UI/Public/BWSkin.h"
#include "Blueprint/WidgetTree.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace BWResult
{
	// 0~1 구간 진행도 (Start부터 Duration 동안)
	float Phase(float Age, float Start, float Duration)
	{
		return FMath::Clamp((Age - Start) / FMath::Max(Duration, 0.01f), 0.0f, 1.0f);
	}

	float EaseOut(float T) { return 1.0f - FMath::Square(1.0f - T); }

	constexpr float TitleStart = 0.15f;
	constexpr float LinesStart = 0.8f;
	constexpr float LineFadeTime = 0.22f;
}

void UBWBattleResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!TXT_Title && WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();
}

void UBWBattleResultWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	auto AddFullScreen = [Root](UWidget* Widget)
	{
		if (UCanvasPanelSlot* FullSlot = Root->AddChildToCanvas(Widget))
		{
			FullSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			FullSlot->SetOffsets(FMargin(0.0f));
		}
	};

	// 화면 전체를 어둡게 덮고 클릭을 받는다 (뒤의 손패·칸이 눌리지 않게)
	Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
	AddFullScreen(Dim);

	// 결과를 담는 석판 (큰 판이라 모서리 장식 석판, 폭은 결과 줄 기준으로 고정)
	Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Plate"));
	BWSkin::StylePanel(Plate);
	Plate->SetPadding(FMargin(60.0f, 34.0f, 60.0f, 40.0f));
	Plate->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
	{
		PlateSlot->SetAnchors(FAnchors(0.5f, 0.42f, 0.5f, 0.42f));
		PlateSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PlateSlot->SetAutoSize(true);
	}
	USizeBox* PlateWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PlateWidth"));
	PlateWidth->SetWidthOverride(520.0f);
	Plate->SetContent(PlateWidth);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	PlateWidth->SetContent(Column);

	auto MakeText = [this](const TCHAR* Name, int32 Size) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
		Text->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
		return Text;
	};
	auto AddCentered = [Column](UWidget* Widget, float Top)
	{
		if (UVerticalBoxSlot* CenterSlot = Column->AddChildToVerticalBox(Widget))
		{
			CenterSlot->SetHorizontalAlignment(HAlign_Center);
			CenterSlot->SetPadding(FMargin(0.0f, Top, 0.0f, 0.0f));
		}
	};

	TXT_Title = MakeText(TEXT("TXT_Title"), 76);
	TXT_Title->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	AddCentered(TXT_Title, 0.0f);

	USizeBox* RuleSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RuleSize"));
	RuleSize->SetWidthOverride(460.0f);
	RuleSize->SetHeightOverride(2.0f);
	Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Rule"));
	Rule->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	RuleSize->SetContent(Rule);
	AddCentered(RuleSize, 6.0f);

	TXT_Sub = MakeText(TEXT("TXT_Sub"), 20);
	TXT_Sub->SetColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f));
	TXT_Sub->SetWrapTextAt(520.0f); // 석판 폭 안에서 줄바꿈
	AddCentered(TXT_Sub, 10.0f);

	BOX_Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BOX_Lines"));
	AddCentered(BOX_Lines, 28.0f);

	TXT_Continue = MakeText(TEXT("TXT_Continue"), 16);
	TXT_Continue->SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f));
	AddCentered(TXT_Continue, 34.0f);

	// 넘어갈 때 덮는 검은 막 (클릭은 통과)
	Fade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Fade"));
	Fade->SetBrushColor(FLinearColor::Black);
	Fade->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFullScreen(Fade);
}

void UBWBattleResultWidget::ShowResult(bool bVictory, const FString& Sub, const TArray<TPair<FString, FString>>& Lines, const FString& ContinueText, TFunction<void()> InOnFinished)
{
	Accent = bVictory ? FLinearColor(1.0f, 0.82f, 0.35f) : FLinearColor(0.95f, 0.3f, 0.28f);
	OnFinished = MoveTemp(InOnFinished);

	if (TXT_Title)
	{
		TXT_Title->SetText(FText::FromString(bVictory ? TEXT("승리") : TEXT("패배")));
		TXT_Title->SetColorAndOpacity(Accent);
	}
	if (Rule) Rule->SetBrushColor(Accent);
	if (TXT_Sub)
	{
		TXT_Sub->SetText(FText::FromString(Sub));
		TXT_Sub->SetVisibility(Sub.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (TXT_Continue) TXT_Continue->SetText(FText::FromString(ContinueText));

	LineWidgets.Reset();
	if (BOX_Lines && WidgetTree)
	{
		BOX_Lines->ClearChildren();
		for (const TPair<FString, FString>& Line : Lines)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			auto AddText = [this, Row](const FString& Str, int32 Size, const FLinearColor& Color, float Width, ETextJustify::Type Justify)
			{
				UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				FSlateFontInfo Font = Text->GetFont();
				Font.Size = Size;
				Text->SetFont(Font);
				Text->SetText(FText::FromString(Str));
				Text->SetColorAndOpacity(Color);
				Text->SetJustification(Justify);
				Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
				Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
				USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				if (Width > 0.0f) Cell->SetWidthOverride(Width);
				Cell->SetContent(Text);
				if (UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(Cell)) CellSlot->SetVerticalAlignment(VAlign_Center);
			};
			if (Line.Value.IsEmpty())
			{
				AddText(Line.Key, 18, FLinearColor(0.85f, 0.85f, 0.85f), 460.0f, ETextJustify::Center);
			}
			else
			{
				AddText(Line.Key, 18, FLinearColor(0.7f, 0.7f, 0.7f), 200.0f, ETextJustify::Left);
				AddText(Line.Value, 22, FLinearColor::White, 260.0f, ETextJustify::Right);
			}
			if (UVerticalBoxSlot* RowSlot = BOX_Lines->AddChildToVerticalBox(Row))
			{
				RowSlot->SetHorizontalAlignment(HAlign_Center);
				RowSlot->SetPadding(FMargin(0.0f, 4.0f));
			}
			LineWidgets.Add(Row);
		}
	}

	if (!IsInViewport()) AddToViewport(700);
	Age = 0.0f;
	LeaveAge = -1.0f;
	NativeTick(GetCachedGeometry(), 0.0f);
}

float UBWBattleResultWidget::LinesEndTime() const
{
	return BWResult::LinesStart + LineWidgets.Num() * LineInterval + BWResult::LineFadeTime;
}

void UBWBattleResultWidget::Continue()
{
	if (Age < 0.0f || LeaveAge >= 0.0f) return;
	// 아직 연출 중이면 끝까지 한 번에 보여 준다
	const float ReadyTime = LinesEndTime() + 0.2f;
	if (Age < ReadyTime) { Age = ReadyTime; return; }
	LeaveAge = 0.0f;
}

FReply UBWBattleResultWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Continue();
	return FReply::Handled();
}

void UBWBattleResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Age < 0.0f) return;
	Age += InDeltaTime;
	using namespace BWResult;

	if (Dim) Dim->SetRenderOpacity(EaseOut(Phase(Age, 0.0f, DimTime)));
	// 석판: 제목보다 조금 먼저 살짝 커지며 나타난다
	if (Plate)
	{
		const float T = EaseOut(Phase(Age, 0.0f, TitleStart + 0.2f));
		Plate->SetRenderOpacity(T);
		Plate->SetRenderScale(FVector2D(0.94f + 0.06f * T));
	}

	// 제목: 크게 찍히듯 줄어들며 나타난다
	if (TXT_Title)
	{
		const float T = Phase(Age, TitleStart, TitleTime);
		TXT_Title->SetRenderOpacity(T);
		TXT_Title->SetRenderScale(FVector2D(1.0f + 0.6f * (1.0f - EaseOut(T))));
	}
	if (Rule) Rule->SetRenderScale(FVector2D(EaseOut(Phase(Age, TitleStart + TitleTime * 0.6f, 0.3f)), 1.0f));
	if (TXT_Sub) TXT_Sub->SetRenderOpacity(Phase(Age, TitleStart + TitleTime, 0.25f));

	// 결과 줄이 하나씩 아래에서 떠오른다
	for (int32 i = 0; i < LineWidgets.Num(); ++i)
	{
		if (!LineWidgets[i]) continue;
		const float T = EaseOut(Phase(Age, LinesStart + i * LineInterval, LineFadeTime));
		LineWidgets[i]->SetRenderOpacity(T);
		LineWidgets[i]->SetRenderTranslation(FVector2D(0.0f, 12.0f * (1.0f - T)));
	}

	if (TXT_Continue)
	{
		const float ReadyTime = LinesEndTime() + 0.2f;
		const float Shown = Phase(Age, ReadyTime, 0.25f);
		TXT_Continue->SetRenderOpacity(Shown * (0.6f + 0.4f * FMath::Sin((Age - ReadyTime) * 4.0f)));
	}

	// 넘어가기: 검게 어두워진 뒤 한 번만 부른다
	if (Fade) Fade->SetRenderOpacity(LeaveAge >= 0.0f ? Phase(LeaveAge, 0.0f, FadeOutTime) : 0.0f);
	if (LeaveAge >= 0.0f)
	{
		LeaveAge += InDeltaTime;
		if (LeaveAge >= FadeOutTime && OnFinished)
		{
			TFunction<void()> Finished = MoveTemp(OnFinished);
			OnFinished = nullptr;
			Finished();
		}
	}
}

// 화면 확인용: bw.Result [lose] — 예시 전투 결과 화면 (클릭하면 닫힘)
static FAutoConsoleCommandWithWorldAndArgs GBWResultPreviewCommand(TEXT("bw.Result"), TEXT("화면 확인용 전투 결과 (인자 lose: 패배)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		UBWBattleResultWidget* Widget = PC ? CreateWidget<UBWBattleResultWidget>(PC, UBWBattleResultWidget::StaticClass()) : nullptr;
		if (!Widget) return;
		const bool bWin = !(Args.Num() > 0 && Args[0] == TEXT("lose"));
		TArray<TPair<FString, FString>> Lines;
		Lines.Add({ TEXT("걸린 턴"), TEXT("6턴") });
		Lines.Add({ TEXT("준 피해"), TEXT("128") });
		Lines.Add({ TEXT("받은 피해"), TEXT("34") });
		Lines.Add({ TEXT("골드"), TEXT("+9999") });
		TWeakObjectPtr<UBWBattleResultWidget> Weak = Widget;
		Widget->ShowResult(bWin, bWin ? TEXT("석상 파수꾼을 쓰러뜨렸습니다") : TEXT("탑이 당신을 삼켰습니다"), Lines, TEXT("클릭해서 계속"),
			[Weak]() { if (Weak.IsValid()) Weak->RemoveFromParent(); });
	}));

#include "BeyondWing/UI/Public/BWTurnBannerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UBWTurnBannerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!TXT_Title && WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBWTurnBannerWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Band"));
	Band->SetPadding(FMargin(0.0f, 14.0f));
	Band->SetHorizontalAlignment(HAlign_Center);
	if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
	{
		// 화면 위쪽 1/3 지점을 가로지르는 띠
		BandSlot->SetAnchors(FAnchors(0.0f, 0.32f, 1.0f, 0.32f));
		BandSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BandSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 110.0f));
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Band->SetContent(Column);

	auto MakeText = [this, Column](const TCHAR* Name, int32 Size) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
		Text->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
		if (UVerticalBoxSlot* TextSlot = Column->AddChildToVerticalBox(Text)) TextSlot->SetHorizontalAlignment(HAlign_Center);
		return Text;
	};
	TXT_Title = MakeText(TEXT("TXT_Title"), 46);
	TXT_Sub = MakeText(TEXT("TXT_Sub"), 18);
}

void UBWTurnBannerWidget::ShowBanner(const FString& Title, const FString& Sub, const FLinearColor& Accent)
{
	if (TXT_Title)
	{
		TXT_Title->SetText(FText::FromString(Title));
		TXT_Title->SetColorAndOpacity(Accent);
	}
	if (TXT_Sub)
	{
		TXT_Sub->SetText(FText::FromString(Sub));
		TXT_Sub->SetVisibility(Sub.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Band) Band->SetBrushColor(FLinearColor(Accent.R * 0.15f, Accent.G * 0.15f, Accent.B * 0.15f, 0.82f));
	if (!IsInViewport()) AddToViewport(400);
	Age = 0.0f;
}

void UBWTurnBannerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Age < 0.0f) return;
	Age += InDeltaTime;

	// 왼쪽에서 미끄러져 들어와 잠시 머물고, 오른쪽으로 빠져나가며 흐려진다
	const float ScreenWidth = MyGeometry.GetLocalSize().X > 0.0f ? MyGeometry.GetLocalSize().X : 1920.0f;
	float OffsetX = 0.0f;
	float Opacity = 1.0f;
	if (Age < SlideTime)
	{
		const float T = Age / SlideTime;
		OffsetX = -ScreenWidth * FMath::Square(1.0f - T);
		Opacity = T;
	}
	else if (Age > SlideTime + HoldTime)
	{
		const float T = FMath::Clamp((Age - SlideTime - HoldTime) / SlideTime, 0.0f, 1.0f);
		OffsetX = ScreenWidth * T * T;
		Opacity = 1.0f - T;
		if (T >= 1.0f)
		{
			Age = -1.0f;
			RemoveFromParent();
			return;
		}
	}
	if (Band)
	{
		Band->SetRenderTranslation(FVector2D(OffsetX, 0.0f));
		Band->SetRenderOpacity(Opacity);
	}
}

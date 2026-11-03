#include "BeyondWing/UI/Public/BWMiracleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"

namespace BWMiracle
{
	static constexpr float TotalTime = 1.5f;
	static constexpr float SlamTime = 0.16f;
	static constexpr float FadeTime = 0.4f;
}

void UBWMiracleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget) BuildLayout();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBWMiracleWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	auto FullScreen = [&](const TCHAR* Name) -> UBorder*
	{
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		if (UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(Box))
		{
			BoxSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BoxSlot->SetOffsets(FMargin(0.0f));
		}
		return Box;
	};
	Dim = FullScreen(TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.01f, 0.28f));

	auto MakeBand = [&](const TCHAR* Name, float AnchorY) -> UBorder*
	{
		UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
		{
			BandSlot->SetAnchors(FAnchors(0.0f, AnchorY, 1.0f, AnchorY));
			BandSlot->SetOffsets(FMargin(0.0f, -3.0f, 0.0f, 6.0f));
		}
		return Band;
	};
	BandTop = MakeBand(TEXT("BandTop"), 0.82f);
	BandBottom = MakeBand(TEXT("BandBottom"), 0.82f);

	auto MakeText = [&](const TCHAR* Name, int32 Size, float AnchorY) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 3;
		Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
		if (UCanvasPanelSlot* TextSlot = Root->AddChildToCanvas(Text))
		{
			TextSlot->SetAnchors(FAnchors(0.5f, AnchorY, 0.5f, AnchorY));
			TextSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			TextSlot->SetAutoSize(true);
		}
		return Text;
	};
	TXT_Title = MakeText(TEXT("TXT_Title"), 68, 0.8f);
	TXT_Sub = MakeText(TEXT("TXT_Sub"), 22, 0.885f);

	Flash = FullScreen(TEXT("Flash"));
}

void UBWMiracleWidget::PlayMiracle(const FString& Title, const FString& Sub, const FLinearColor& Accent)
{
	if (!Flash || !TXT_Title) return;
	TXT_Title->SetText(FText::FromString(Title));
	TXT_Title->SetColorAndOpacity(Accent);
	TXT_Sub->SetText(FText::FromString(Sub));
	Flash->SetBrushColor(FLinearColor(Accent.R, Accent.G, Accent.B, 1.0f));
	BandTop->SetBrushColor(Accent);
	BandBottom->SetBrushColor(Accent);
	if (!IsInViewport()) AddToViewport(650);
	StartedAt = FPlatformTime::Seconds();
}

void UBWMiracleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (StartedAt < 0.0) return;
	// 히트스톱으로 시간이 느려져도 연출 길이는 같아야 한다
	const float Age = (float)(FPlatformTime::Seconds() - StartedAt);
	using namespace BWMiracle;
	if (Age >= TotalTime)
	{
		StartedAt = -1.0;
		RemoveFromParent();
		return;
	}

	Flash->SetRenderOpacity(FMath::Clamp(1.0f - Age / 0.18f, 0.0f, 1.0f) * 0.5f);
	const float Slam = FMath::Clamp(Age / SlamTime, 0.0f, 1.0f);
	const float Eased = 1.0f - FMath::Pow(1.0f - Slam, 3.0f);
	const float Fade = FMath::Clamp((TotalTime - Age) / FadeTime, 0.0f, 1.0f);
	Dim->SetRenderOpacity(Fade);

	FWidgetTransform Transform;
	const float Scale = FMath::Lerp(3.2f, 1.0f, Eased);
	Transform.Scale = FVector2D(Scale, Scale);
	TXT_Title->SetRenderTransform(Transform);
	TXT_Title->SetRenderOpacity(Eased * Fade);
	TXT_Sub->SetRenderOpacity(FMath::Clamp((Age - SlamTime) / 0.2f, 0.0f, 1.0f) * Fade);

	// 가운데 금이 위아래로 벌어진다
	const float Spread = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Age / 0.5f, 0.0f, 1.0f), 3.0f);
	const float Offset = 70.0f * Spread;
	BandTop->SetRenderTranslation(FVector2D(0.0f, -Offset));
	BandBottom->SetRenderTranslation(FVector2D(0.0f, Offset));
	BandTop->SetRenderOpacity(Fade);
	BandBottom->SetRenderOpacity(Fade);
}

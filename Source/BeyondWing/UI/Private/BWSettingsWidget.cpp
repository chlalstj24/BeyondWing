#include "BeyondWing/UI/Public/BWSettingsWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"

namespace BWSettings
{
	enum ERow : int32 { WindowMode, Resolution, Quality, VSync, FrameLimit, Volume, Count };

	static const EWindowMode::Type WindowModes[] = { EWindowMode::Windowed, EWindowMode::WindowedFullscreen, EWindowMode::Fullscreen };
	static const float FrameLimits[] = { 30.0f, 60.0f, 120.0f, 144.0f, 0.0f }; // 0 = 제한 없음
	static const FLinearColor Gold(0.95f, 0.82f, 0.5f);

	static FText T(const TCHAR* S) { return FText::FromString(S); }
}

void UBWSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildRows();
	if (WidgetTree) BuildLayout();
}

void UBWSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ReadCurrentSettings();
	for (int32 i = 0; i < Rows.Num(); ++i) RefreshRow(i);
	if (StatusText) StatusText->SetText(FText::GetEmpty());
}

FReply UBWSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape) { Close(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBWSettingsWidget::BuildRows()
{
	using namespace BWSettings;
	Rows.SetNum(Count);
	Rows[WindowMode].Label = T(TEXT("화면 모드"));
	Rows[WindowMode].Choices = { T(TEXT("창 모드")), T(TEXT("테두리 없는 창")), T(TEXT("전체 화면")) };

	// 모니터가 지원하는 해상도 (못 읽으면 흔한 해상도)
	Resolutions.Reset();
	TArray<FIntPoint> Supported;
	if (UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported))
	{
		for (const FIntPoint& R : Supported)
			if (R.X >= 1280 && R.Y >= 720) Resolutions.AddUnique(R);
	}
	if (Resolutions.Num() == 0) Resolutions = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440) };
	Rows[Resolution].Label = T(TEXT("해상도"));
	for (const FIntPoint& R : Resolutions) Rows[Resolution].Choices.Add(FText::FromString(FString::Printf(TEXT("%d × %d"), R.X, R.Y)));

	Rows[Quality].Label = T(TEXT("그래픽 품질"));
	Rows[Quality].Choices = { T(TEXT("낮음")), T(TEXT("중간")), T(TEXT("높음")), T(TEXT("최상")) };
	Rows[VSync].Label = T(TEXT("수직 동기화"));
	Rows[VSync].Choices = { T(TEXT("끔")), T(TEXT("켬")) };
	Rows[FrameLimit].Label = T(TEXT("프레임 제한"));
	Rows[FrameLimit].Choices = { T(TEXT("30")), T(TEXT("60")), T(TEXT("120")), T(TEXT("144")), T(TEXT("제한 없음")) };
	Rows[Volume].Label = T(TEXT("전체 음량"));
	for (int32 v = 0; v <= 10; ++v) Rows[Volume].Choices.Add(FText::FromString(FString::Printf(TEXT("%d%%"), v * 10)));
}

void UBWSettingsWidget::ReadCurrentSettings()
{
	using namespace BWSettings;
	UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (S)
	{
		const EWindowMode::Type Mode = S->GetFullscreenMode();
		for (int32 i = 0; i < UE_ARRAY_COUNT(WindowModes); ++i) if (WindowModes[i] == Mode) Rows[WindowMode].Index = i;

		// 지금 해상도와 가장 가까운 항목
		const FIntPoint Current = S->GetScreenResolution();
		int32 Best = 0, BestDist = MAX_int32;
		for (int32 i = 0; i < Resolutions.Num(); ++i)
		{
			const int32 Dist = FMath::Abs(Resolutions[i].X - Current.X) + FMath::Abs(Resolutions[i].Y - Current.Y);
			if (Dist < BestDist) { BestDist = Dist; Best = i; }
		}
		Rows[Resolution].Index = Best;

		Rows[Quality].Index = FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 3); // -1(사용자 지정)이면 0
		Rows[VSync].Index = S->IsVSyncEnabled() ? 1 : 0;
		const float Limit = S->GetFrameRateLimit();
		Rows[FrameLimit].Index = UE_ARRAY_COUNT(FrameLimits) - 1;
		for (int32 i = 0; i < UE_ARRAY_COUNT(FrameLimits); ++i) if (FMath::IsNearlyEqual(FrameLimits[i], Limit)) Rows[FrameLimit].Index = i;
	}
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	Rows[Volume].Index = FMath::RoundToInt((GI ? GI->GetMasterVolume() : 1.0f) * 10.0f);
}

void UBWSettingsWidget::StepOption(int32 Row, int32 Delta)
{
	if (!Rows.IsValidIndex(Row) || Rows[Row].Choices.Num() == 0) return;
	FOptionRow& R = Rows[Row];
	R.Index = (R.Index + Delta + R.Choices.Num()) % R.Choices.Num();
	RefreshRow(Row);
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("'적용'을 누르면 바뀝니다")));
}

void UBWSettingsWidget::RefreshRow(int32 Row)
{
	if (!Rows.IsValidIndex(Row)) return;
	const FOptionRow& R = Rows[Row];
	if (UTextBlock* Text = R.ValueText.Get())
		Text->SetText(R.Choices.IsValidIndex(R.Index) ? R.Choices[R.Index] : FText::GetEmpty());
}

void UBWSettingsWidget::ApplySettings()
{
	using namespace BWSettings;
	if (UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		S->SetFullscreenMode(WindowModes[FMath::Clamp(Rows[WindowMode].Index, 0, 2)]);
		if (Resolutions.IsValidIndex(Rows[Resolution].Index)) S->SetScreenResolution(Resolutions[Rows[Resolution].Index]);
		S->SetOverallScalabilityLevel(Rows[Quality].Index);
		S->SetVSyncEnabled(Rows[VSync].Index == 1);
		S->SetFrameRateLimit(FrameLimits[FMath::Clamp(Rows[FrameLimit].Index, 0, UE_ARRAY_COUNT(FrameLimits) - 1)]);
		S->ApplySettings(false);
		S->SaveSettings();
	}
	if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance())) GI->SetMasterVolume(Rows[Volume].Index / 10.0f);
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("적용했습니다")));
	UE_LOG(LogTemp, Warning, TEXT("[BWSettings] Applied: mode %d, res %d, quality %d, vsync %d, fps %d, volume %d%%"),
		Rows[WindowMode].Index, Rows[Resolution].Index, Rows[Quality].Index, Rows[VSync].Index, Rows[FrameLimit].Index, Rows[Volume].Index * 10);
}

void UBWSettingsWidget::ResetToDefaults()
{
	using namespace BWSettings;
	// 테두리 없는 창 · 모니터 해상도(가장 큰 항목) · 높음 · 수직 동기화 끔 · 제한 없음 · 100%
	Rows[WindowMode].Index = 1;
	Rows[Resolution].Index = FMath::Max(0, Resolutions.Num() - 1);
	Rows[Quality].Index = 2;
	Rows[VSync].Index = 0;
	Rows[FrameLimit].Index = UE_ARRAY_COUNT(FrameLimits) - 1;
	Rows[Volume].Index = 10;
	for (int32 i = 0; i < Rows.Num(); ++i) RefreshRow(i);
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("기본값으로 바꿨습니다. '적용'을 누르면 바뀝니다")));
}

void UBWSettingsWidget::Close()
{
	RemoveFromParent();
}

void UBWSettingsWidget::OnStepPressed()
{
	// 방금 눌린 버튼(마우스가 올라가 있는 버튼)을 찾는다
	for (int32 i = 0; i < StepButtons.Num(); ++i)
	{
		if (StepButtons[i] && (StepButtons[i]->IsHovered() || StepButtons[i]->IsPressed()))
		{
			StepOption(StepButtonTargets[i].X, StepButtonTargets[i].Y);
			return;
		}
	}
}

void UBWSettingsWidget::BuildLayout()
{
	using namespace BWSettings;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BW_SettingsRoot"));
	WidgetTree->RootWidget = Root;

	// 뒤를 어둡게 덮는다 (클릭이 뒤 화면으로 새지 않게 Visible)
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
	Dim->SetVisibility(ESlateVisibility::Visible);
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	// 금빛 테두리 패널
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(Gold * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f));
	Frame->SetPadding(FMargin(2.0f));
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(FLinearColor(0.035f, 0.03f, 0.06f, 0.97f));
	Panel->SetPadding(FMargin(48.0f, 36.0f));
	Frame->SetContent(Panel);
	if (UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame))
	{
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FrameSlot->SetAutoSize(true);
	}

	auto MakeText = [this](int32 Size, const FLinearColor& Color, ETextJustify::Type Justify = ETextJustify::Left) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetJustification(Justify);
		return Text;
	};
	auto MakeButton = [this, &MakeText](const FText& Label, int32 FontSize, const FLinearColor& Tint) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(Tint);
		UTextBlock* Text = MakeText(FontSize, FLinearColor::White, ETextJustify::Center);
		Text->SetText(Label);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->SetContent(Text))) ContentSlot->SetPadding(FMargin(12.0f, 4.0f));
		return Button;
	};

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);

	UTextBlock* Title = MakeText(32, Gold, ETextJustify::Center);
	Title->SetText(T(TEXT("환경설정")));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));
	}

	for (int32 r = 0; r < Rows.Num(); ++r)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(200.0f);
		UTextBlock* Label = MakeText(18, FLinearColor(0.85f, 0.84f, 0.9f));
		Label->SetText(Rows[r].Label);
		LabelBox->SetContent(Label);
		if (UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(LabelBox)) LabelSlot->SetVerticalAlignment(VAlign_Center);

		for (int32 Dir : { -1, 1 })
		{
			UButton* Step = MakeButton(T(Dir < 0 ? TEXT("◀") : TEXT("▶")), 22, FLinearColor(0.25f, 0.24f, 0.32f));
			Step->OnClicked.AddDynamic(this, &UBWSettingsWidget::OnStepPressed);
			StepButtons.Add(Step);
			StepButtonTargets.Add(FIntPoint(r, Dir));
			if (Dir > 0)
			{
				// 값 칸은 두 버튼 사이에
				USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				ValueBox->SetWidthOverride(240.0f);
				UTextBlock* Value = MakeText(18, FLinearColor::White, ETextJustify::Center);
				ValueBox->SetContent(Value);
				Rows[r].ValueText = Value;
				if (UHorizontalBoxSlot* ValueSlot = Line->AddChildToHorizontalBox(ValueBox)) ValueSlot->SetVerticalAlignment(VAlign_Center);
			}
			if (UHorizontalBoxSlot* StepSlot = Line->AddChildToHorizontalBox(Step)) StepSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* LineSlot = Column->AddChildToVerticalBox(Line)) LineSlot->SetPadding(FMargin(0.0f, 7.0f));
	}

	StatusText = MakeText(14, FLinearColor(0.8f, 0.75f, 0.6f), ETextJustify::Center);
	if (UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
		StatusSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 10.0f));
	}

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Footer)) FooterSlot->SetHorizontalAlignment(HAlign_Center);
	auto AddFooter = [this, Footer, &MakeButton](const TCHAR* Label, const FLinearColor& Tint) -> UButton*
	{
		UButton* Button = MakeButton(FText::FromString(Label), 17, Tint);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetMinDesiredWidth(150.0f);
		Size->SetHeightOverride(46.0f);
		Size->SetContent(Button);
		if (UHorizontalBoxSlot* ButtonSlot = Footer->AddChildToHorizontalBox(Size)) ButtonSlot->SetPadding(FMargin(8.0f, 0.0f));
		return Button;
	};
	AddFooter(TEXT("기본값"), FLinearColor(0.3f, 0.3f, 0.36f))->OnClicked.AddDynamic(this, &UBWSettingsWidget::OnDefaultsClicked);
	AddFooter(TEXT("적용"), FLinearColor(0.95f, 0.7f, 0.25f))->OnClicked.AddDynamic(this, &UBWSettingsWidget::OnApplyClicked);
	AddFooter(TEXT("닫기"), FLinearColor(0.3f, 0.3f, 0.36f))->OnClicked.AddDynamic(this, &UBWSettingsWidget::OnCloseClicked);
}

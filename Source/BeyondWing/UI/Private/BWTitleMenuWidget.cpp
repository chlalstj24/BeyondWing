#include "BeyondWing/UI/Public/BWTitleMenuWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/UI/Public/BWClassSelectWidget.h"
#include "BeyondWing/UI/Public/BWSettingsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"

namespace BWTitle
{
	// 메뉴 순서
	enum EMenu : int32 { Continue, NewGame, Settings, Quit };
	static const FLinearColor MenuIdle(0.85f, 0.84f, 0.88f);
}

UBWTitleMenuWidget::UBWTitleMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TitleText = FText::FromString(TEXT("BEYOND WING"));
	SubtitleText = FText::FromString(TEXT("뒤섞인 차원의 탑을 오르다"));
	ContinueText = FText::FromString(TEXT("이어하기"));
	StartText = FText::FromString(TEXT("새 게임"));
	SettingsText = FText::FromString(TEXT("환경설정"));
	QuitText = FText::FromString(TEXT("게임 종료"));
	FooterText = FText::FromString(TEXT("프로토타입"));
	ConfirmNewGameText = FText::FromString(TEXT("진행 중인 등반이 있습니다.\n새로 시작하면 이전 기록이 사라집니다."));
	ConfirmYesText = FText::FromString(TEXT("새로 시작"));
	ConfirmNoText = FText::FromString(TEXT("취소"));

	BackdropTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/HUD/Texture/UI/T_UI_TitleBackdrop.T_UI_TitleBackdrop")));
	GlowTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/HUD/Texture/UI/T_UI_Glow.T_UI_Glow")));
	VignetteTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/HUD/Texture/UI/T_UI_Vignette.T_UI_Vignette")));
}

void UBWTitleMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (bUseCodeLayout && WidgetTree) BuildDefaultLayout();
}

void UBWTitleMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	OpenTime = 0.0f;
	RefreshContinue();
	ShowConfirm(false);

	// [강제 포커스] 타이틀 화면이 켜지면 무조건 키보드/마우스 포커스를 받도록 설정
	SetIsFocusable(true);
}

void UBWTitleMenuWidget::RefreshContinue()
{
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	const bool bHasSave = GI && GI->HasSavedRun();
	if (MenuEnabled.IsValidIndex(BWTitle::Continue))
	{
		MenuEnabled[BWTitle::Continue] = bHasSave;
		if (MenuButtons[BWTitle::Continue]) MenuButtons[BWTitle::Continue]->SetIsEnabled(bHasSave);
	}
	if (ContinueSummary)
	{
		ContinueSummary->SetText(bHasSave ? GI->GetSavedRunSummary() : FText::GetEmpty());
		ContinueSummary->SetVisibility(bHasSave ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UBWTitleMenuWidget::BuildDefaultLayout()
{
	// 디자이너에 그린 화면 대신 새 화면을 루트로 쓴다 (디자이너 위젯은 남아 있어 블루프린트 그래프가 깨지지 않는다)
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BW_TitleRoot"));
	WidgetTree->RootWidget = Root;
	auto Fill = [](UCanvasPanelSlot* CanvasSlot)
	{
		if (!CanvasSlot) return;
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	};

	UBorder* Black = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Black->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.02f, 1.0f));
	Fill(Root->AddChildToCanvas(Black));

	// 배경 그림: 화면을 꽉 채우고 천천히 커졌다 작아진다. 넘치는 부분은 위쪽(탑 꼭대기)을 남기고 아래를 더 자른다 (NativeTick에서 배치)
	BackdropImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IMG_Backdrop"));
	if (UTexture2D* Backdrop = BackdropTexture.LoadSynchronous())
	{
		BackdropImage->SetBrushFromTexture(Backdrop, true);
		BackdropAspect = Backdrop->GetSizeY() > 0 ? float(Backdrop->GetSizeX()) / Backdrop->GetSizeY() : 16.0f / 9.0f;
	}
	else BackdropImage->SetColorAndOpacity(FLinearColor(0.04f, 0.03f, 0.08f));
	BackdropImage->SetRenderTransformPivot(FVector2D(0.62f, 0.3f));
	Root->AddChildToCanvas(BackdropImage);

	UTexture2D* Glow = GlowTexture.LoadSynchronous();
	for (int32 i = 0; i < NumParticles; ++i)
	{
		UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (Glow) Dot->SetBrushFromTexture(Glow);
		Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* DotSlot = Root->AddChildToCanvas(Dot))
		{
			DotSlot->SetSize(FVector2D(FMath::FRandRange(5.0f, 20.0f)));
			DotSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
		Particles.Add(Dot);
		ParticleState.Add(FVector4f(FMath::FRand(), FMath::FRand(), FMath::FRandRange(14.0f, 45.0f), FMath::FRandRange(0.0f, 6.28f)));
	}
	if (UTexture2D* Vignette = VignetteTexture.LoadSynchronous())
	{
		UImage* VignetteImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		VignetteImage->SetBrushFromTexture(Vignette);
		VignetteImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fill(Root->AddChildToCanvas(VignetteImage));
	}

	auto MakeText = [this](int32 Size, const FLinearColor& Color, int32 LetterSpacing = 0) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.LetterSpacing = LetterSpacing;
		Font.OutlineSettings.OutlineSize = Size >= 30 ? 2 : 0;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.05f, 0.02f, 0.0f, 0.6f);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetShadowOffset(FVector2D(0.0f, 2.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
		Text->SetJustification(ETextJustify::Left);
		return Text;
	};

	// 왼쪽: 제목 + 부제 + 메뉴 (오른쪽은 배경의 탑)
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column))
	{
		ColumnSlot->SetAnchors(FAnchors(0.08f, 0.5f));
		ColumnSlot->SetAlignment(FVector2D(0.0f, 0.5f));
		ColumnSlot->SetAutoSize(true);
	}

	UVerticalBox* TitleColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Title = MakeText(72, AccentColor, 360);
	Title->SetText(TitleText);
	TitleColumn->AddChildToVerticalBox(Title);
	USizeBox* Divider = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Divider->SetWidthOverride(360.0f);
	Divider->SetHeightOverride(2.0f);
	UImage* DividerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	DividerImage->SetColorAndOpacity(AccentColor * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f));
	Divider->SetContent(DividerImage);
	if (UVerticalBoxSlot* DividerSlot = TitleColumn->AddChildToVerticalBox(Divider))
	{
		DividerSlot->SetHorizontalAlignment(HAlign_Left);
		DividerSlot->SetPadding(FMargin(4.0f, 8.0f));
	}
	UTextBlock* Subtitle = MakeText(19, FLinearColor(0.85f, 0.8f, 0.9f, 0.85f), 100);
	Subtitle->SetText(SubtitleText);
	if (UVerticalBoxSlot* SubSlot = TitleColumn->AddChildToVerticalBox(Subtitle)) SubSlot->SetPadding(FMargin(4.0f, 0.0f));
	TitleBlock = TitleColumn;
	if (UVerticalBoxSlot* BlockSlot = Column->AddChildToVerticalBox(TitleColumn)) BlockSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 60.0f));

	// 메뉴 버튼: 배경 없이 글자만, 마우스를 올리면 왼쪽 빛 막대가 켜지고 글자가 오른쪽으로 밀리며 밝아진다
	auto AddMenuButton = [this, Column, &MakeText](const FText& Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		FSlateBrush NoBrush;
		NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(NoBrush);
		Style.SetHovered(NoBrush);
		Style.SetPressed(NoBrush);
		Style.SetDisabled(NoBrush);
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
		Button->SetStyle(Style);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarBox->SetWidthOverride(4.0f);
		BarBox->SetHeightOverride(30.0f);
		UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Bar->SetColorAndOpacity(AccentColor);
		Bar->SetRenderOpacity(0.0f);
		BarBox->SetContent(Bar);
		if (UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarBox))
		{
			BarSlot->SetVerticalAlignment(VAlign_Center);
			BarSlot->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
		}
		UTextBlock* Text = MakeText(26, BWTitle::MenuIdle, 80);
		Text->SetText(Label);
		if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text)) TextSlot->SetVerticalAlignment(VAlign_Center);
		Button->SetContent(Row);

		if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Left);
			ButtonSlot->SetPadding(FMargin(0.0f, 5.0f));
		}
		MenuButtons.Add(Button);
		MenuBars.Add(Bar);
		MenuTexts.Add(Text);
		MenuHover.Add(0.0f);
		MenuEnabled.Add(true);
		return Button;
	};
	AddMenuButton(ContinueText)->OnClicked.AddDynamic(this, &UBWTitleMenuWidget::OnContinueClicked);
	// 이어하기 밑에 저장된 판 한 줄 요약
	ContinueSummary = MakeText(14, FLinearColor(0.75f, 0.72f, 0.8f, 0.8f), 40);
	if (UVerticalBoxSlot* SummarySlot = Column->AddChildToVerticalBox(ContinueSummary)) SummarySlot->SetPadding(FMargin(20.0f, -4.0f, 0.0f, 8.0f));
	AddMenuButton(StartText)->OnClicked.AddDynamic(this, &UBWTitleMenuWidget::OnStartClicked);
	AddMenuButton(SettingsText)->OnClicked.AddDynamic(this, &UBWTitleMenuWidget::OnSettingsClicked);
	AddMenuButton(QuitText)->OnClicked.AddDynamic(this, &UBWTitleMenuWidget::OnQuitClicked);

	if (!FooterText.IsEmpty())
	{
		UTextBlock* Footer = MakeText(13, FLinearColor(1.0f, 1.0f, 1.0f, 0.4f), 60);
		Footer->SetText(FooterText);
		if (UCanvasPanelSlot* FooterSlot = Root->AddChildToCanvas(Footer))
		{
			FooterSlot->SetAnchors(FAnchors(1.0f, 1.0f));
			FooterSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			FooterSlot->SetPosition(FVector2D(-28.0f, -20.0f));
			FooterSlot->SetAutoSize(true);
		}
	}

	// 새 게임 확인 창 (이어할 판이 있을 때만 뜬다)
	UBorder* ConfirmDim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BW_ConfirmDim"));
	ConfirmDim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	ConfirmDim->SetHorizontalAlignment(HAlign_Center);
	ConfirmDim->SetVerticalAlignment(VAlign_Center);
	Fill(Root->AddChildToCanvas(ConfirmDim));
	UBorder* ConfirmFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ConfirmFrame->SetBrushColor(AccentColor * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f));
	ConfirmFrame->SetPadding(FMargin(2.0f));
	UBorder* ConfirmBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ConfirmBox->SetBrushColor(FLinearColor(0.035f, 0.03f, 0.06f, 0.97f));
	ConfirmBox->SetPadding(FMargin(44.0f, 32.0f));
	ConfirmFrame->SetContent(ConfirmBox);
	ConfirmDim->SetContent(ConfirmFrame);
	UVerticalBox* ConfirmColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ConfirmBox->SetContent(ConfirmColumn);
	UTextBlock* ConfirmMessage = MakeText(19, FLinearColor::White);
	ConfirmMessage->SetText(ConfirmNewGameText);
	ConfirmMessage->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* MessageSlot = ConfirmColumn->AddChildToVerticalBox(ConfirmMessage))
	{
		MessageSlot->SetHorizontalAlignment(HAlign_Center);
		MessageSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 26.0f));
	}
	UHorizontalBox* ConfirmButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* ButtonsSlot = ConfirmColumn->AddChildToVerticalBox(ConfirmButtons)) ButtonsSlot->SetHorizontalAlignment(HAlign_Center);
	auto AddConfirmButton = [this, ConfirmButtons, &MakeText](const FText& Label, const FLinearColor& Tint) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(Tint);
		UTextBlock* Text = MakeText(17, FLinearColor::White);
		Text->SetText(Label);
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->SetContent(Text))) LabelSlot->SetPadding(FMargin(26.0f, 8.0f));
		if (UHorizontalBoxSlot* ButtonSlot = ConfirmButtons->AddChildToHorizontalBox(Button)) ButtonSlot->SetPadding(FMargin(8.0f, 0.0f));
		return Button;
	};
	AddConfirmButton(ConfirmYesText, FLinearColor(0.95f, 0.7f, 0.25f))->OnClicked.AddDynamic(this, &UBWTitleMenuWidget::OnConfirmYes);
	AddConfirmButton(ConfirmNoText, FLinearColor(0.3f, 0.3f, 0.36f))->OnClicked.AddDynamic(this, &UBWTitleMenuWidget::OnConfirmNo);
	ConfirmPanel = ConfirmDim;

	// 처음 열릴 때 검은 화면에서 밝아진다
	FadeImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FadeImage->SetColorAndOpacity(FLinearColor::Black);
	FadeImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(Root->AddChildToCanvas(FadeImage));
}

void UBWTitleMenuWidget::ShowConfirm(bool bShow)
{
	if (ConfirmPanel) ConfirmPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UBWTitleMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BackdropImage) return;
	OpenTime += InDeltaTime;
	auto Ease = [](float T) { T = FMath::Clamp(T, 0.0f, 1.0f); return 1.0f - FMath::Pow(1.0f - T, 3.0f); };

	if (FadeImage) FadeImage->SetRenderOpacity(1.0f - Ease(OpenTime / 1.4f));

	// 배경: 천천히 커졌다 작아진다
	const float Cycle = FMath::Max(1.0f, BackdropCycleSeconds);
	const float Zoom = 1.0f + BackdropZoom * (0.5f - 0.5f * FMath::Cos(OpenTime / Cycle * 2.0f * PI));
	BackdropImage->SetRenderScale(FVector2D(Zoom));
	const FVector2D Screen = MyGeometry.GetLocalSize();
	if (UCanvasPanelSlot* BackdropSlot = Cast<UCanvasPanelSlot>(BackdropImage->Slot); BackdropSlot && Screen.X > 1.0f)
	{
		// 화면을 덮는 가장 작은 크기. 남는 세로는 위 15% / 아래 85%로 잘라 탑 꼭대기를 살린다
		FVector2D Size(Screen.X, Screen.X / BackdropAspect);
		if (Size.Y < Screen.Y) Size = FVector2D(Screen.Y * BackdropAspect, Screen.Y);
		BackdropSlot->SetSize(Size);
		BackdropSlot->SetPosition(FVector2D((Screen.X - Size.X) * 0.5f, (Screen.Y - Size.Y) * 0.15f));
	}

	// 제목은 아래에서 떠오르고, 메뉴는 조금 늦게 하나씩 나타난다
	if (TitleBlock)
	{
		const float T = Ease((OpenTime - 0.3f) / 1.0f);
		TitleBlock->SetRenderOpacity(T);
		TitleBlock->SetRenderTranslation(FVector2D(0.0f, (1.0f - T) * 30.0f));
	}
	for (int32 i = 0; i < MenuButtons.Num(); ++i)
	{
		const float Appear = Ease((OpenTime - 0.9f - 0.1f * i) / 0.6f);
		const bool bEnabled = MenuEnabled.IsValidIndex(i) && MenuEnabled[i];
		const bool bHovered = bEnabled && MenuButtons[i]->IsHovered();
		MenuHover[i] = FMath::FInterpTo(MenuHover[i], bHovered ? 1.0f : 0.0f, InDeltaTime, 12.0f);
		const float H = MenuHover[i];
		MenuButtons[i]->SetRenderOpacity(Appear * (bEnabled ? 1.0f : 0.35f));
		MenuBars[i]->SetRenderOpacity(H);
		MenuTexts[i]->SetRenderTranslation(FVector2D(H * 10.0f, 0.0f));
		MenuTexts[i]->SetColorAndOpacity(FMath::Lerp(BWTitle::MenuIdle, AccentColor, H));
	}
	if (ContinueSummary) ContinueSummary->SetRenderOpacity(Ease((OpenTime - 0.9f) / 0.6f));

	// 떠오르는 불티
	const FVector2D ScreenSize = MyGeometry.GetLocalSize();
	if (ScreenSize.X > 1.0f)
	{
		for (int32 i = 0; i < Particles.Num(); ++i)
		{
			FVector4f& P = ParticleState[i];
			if (!bParticlesPlaced) { P.X *= ScreenSize.X; P.Y *= ScreenSize.Y; } // 첫 틱: 비율 → 화면 좌표
			P.Y -= P.Z * InDeltaTime;
			P.W += InDeltaTime;
			if (P.Y < -20.0f) { P.Y = ScreenSize.Y + 20.0f; P.X = FMath::FRand() * ScreenSize.X; }
			if (UCanvasPanelSlot* DotSlot = Cast<UCanvasPanelSlot>(Particles[i]->Slot))
				DotSlot->SetPosition(FVector2D(P.X + FMath::Sin(P.W * 0.8f) * 16.0f, P.Y));
			FLinearColor C = AccentColor;
			C.A = (0.22f + 0.22f * FMath::Sin(P.W * 2.1f)) * FMath::Clamp(P.Y / (ScreenSize.Y * 0.35f), 0.0f, 1.0f);
			Particles[i]->SetColorAndOpacity(C);
		}
		bParticlesPlaced = true;
	}
}

void UBWTitleMenuWidget::OnStartClicked()
{
	// 이어할 판이 있으면 먼저 묻는다
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI && GI->HasSavedRun() && ConfirmPanel) { ShowConfirm(true); return; }
	StartNewGame(NewGameLevel);
}

void UBWTitleMenuWidget::OnConfirmYes()
{
	// 저장은 삶의 방식을 고르고 시작할 때 덮어쓴다 (선택 화면에서 '뒤로'를 누르면 이전 판이 남아 있다)
	ShowConfirm(false);
	StartNewGame(NewGameLevel);
}

void UBWTitleMenuWidget::ContinueGame()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || !GI->LoadRun())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Title] Continue failed: no usable save"));
		RefreshContinue();
		return;
	}
	UGameplayStatics::OpenLevel(GetWorld(), NewGameLevel);
}

void UBWTitleMenuWidget::OpenSettings()
{
	TSubclassOf<UBWSettingsWidget> WidgetClass = SettingsWidgetClass ? SettingsWidgetClass : TSubclassOf<UBWSettingsWidget>(UBWSettingsWidget::StaticClass());
	if (UBWSettingsWidget* Settings = CreateWidget<UBWSettingsWidget>(GetOwningPlayer(), WidgetClass))
	{
		Settings->AddToViewport(200);
		Settings->SetKeyboardFocus();
	}
}

void UBWTitleMenuWidget::StartNewGame(FName LevelName)
{
	// [수정] 바로 지도로 가지 않고 삶의 방식(직업) 선택 화면을 연다. 지도 생성과 레벨 이동은 그 화면에서 고른 뒤에 한다
	// 지정이 없으면 화면 문구를 담은 WBP_ClassSelect → 그것도 없으면 C++ 기본 위젯
	TSubclassOf<UBWClassSelectWidget> WidgetClass = ClassSelectWidgetClass;
	if (!WidgetClass && FPackageName::DoesPackageExist(TEXT("/Game/HUD/BP/WBP_ClassSelect")))
		WidgetClass = LoadClass<UBWClassSelectWidget>(nullptr, TEXT("/Game/HUD/BP/WBP_ClassSelect.WBP_ClassSelect_C"));
	if (!WidgetClass) WidgetClass = UBWClassSelectWidget::StaticClass();
	if (UBWClassSelectWidget* Select = CreateWidget<UBWClassSelectWidget>(GetOwningPlayer(), WidgetClass))
	{
		Select->TargetLevelName = LevelName;
		Select->AddToViewport(100);
		return;
	}

	// 화면을 못 만들면 예전처럼 기본 직업으로 바로 시작
	if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance()))
	{
		GI->BeginNewRun(GI->SelectedClass);
		GI->GenerateWorldMap(15);
		GI->SaveRun();
		UGameplayStatics::OpenLevel(GetWorld(), LevelName);
	}
}

void UBWTitleMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}

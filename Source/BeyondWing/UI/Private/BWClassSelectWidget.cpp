#include "BeyondWing/UI/Public/BWClassSelectWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/DeckComponent.h"
#include "BeyondWing/Combat/Public/BWDuelRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace BWClassSelect
{
	static constexpr int32 MaxCards = 5;
	static constexpr int32 NumRatings = 4;
	static constexpr int32 RatingCells = 5;
	static const TCHAR* FrameGlowPath = TEXT("/Game/HUD/Texture/UI/T_UI_FrameGlow.T_UI_FrameGlow");

	static const TCHAR* StatName(EClassType Class)
	{
		switch (Class)
		{
		case EClassType::PHA: return TEXT("PHA");
		case EClassType::TEC: return TEXT("TEC");
		case EClassType::INT: return TEXT("INT");
		case EClassType::FTH: return TEXT("FTH");
		case EClassType::HP:  return TEXT("HP");
		default:              return TEXT("");
		}
	}

	static int32 MainStatValue(const FBWClassData& Data)
	{
		const FBWClassStartStats& S = Data.StartStats;
		switch (Data.ClassType)
		{
		case EClassType::PHA: return S.PHA;
		case EClassType::TEC: return S.TEC;
		case EClassType::INT: return S.INT;
		case EClassType::FTH: return S.FTH;
		default:              return S.MaxHP;
		}
	}

	static int32 Rating(const FBWClassData& Data, int32 Index)
	{
		const int32 Values[] = { Data.RatingFirepower, Data.RatingSurvival, Data.RatingMobility, Data.RatingGrowth };
		return Values[FMath::Clamp(Index, 0, NumRatings - 1)];
	}

	// DT_Classes가 없을 때만 쓰는 최소한의 카드 (규칙 수치는 C++ 기본값, 문구는 스탯 이름)
	static FBWClassData MakeFallback(EClassType Class, const UBWGameInstance* GI)
	{
		FBWClassData Data;
		Data.ClassType = Class;
		Data.bSelectable = Class != EClassType::FTH;
		Data.Title = FText::FromString(StatName(Class));
		Data.DisplayName = Data.Title;
		Data.Color = FLinearColor(0.5f, 0.5f, 0.55f);
		Data.StartStats = UBWDuelRules::MakeClassStartStats(Class);
		Data.Speed = GI ? GI->GetClassSpeedDice(Class) : FBWDiceRange(3, 6);
		Data.Combat = UBWDuelRules::MakeClassProfile(Class);
		Data.BasicStrike = UBWDuelRules::MakeBasicStrike(Class);
		Data.CardPower = UBWDuelRules::MakeClassCardPower(Class);
		Data.LockedText = FText::FromString(TEXT("준비 중"));
		return Data;
	}
}

UBWClassSelectWidget::UBWClassSelectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerCharacterClass = TSoftClassPtr<ABWCharacter>(FSoftObjectPath(TEXT("/Game/System/Blueprint/BP_BWCharacter.BP_BWCharacter_C")));
	GlowTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/HUD/Texture/UI/T_UI_Glow.T_UI_Glow")));
	VignetteTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/HUD/Texture/UI/T_UI_Vignette.T_UI_Vignette")));

	ScreenTitle = FText::FromString(TEXT("삶의 방식 선택"));
	ScreenSubtitle = FText::FromString(TEXT("이번 여정을 함께할 삶의 방식을 고르세요"));
	PromptText = FText::FromString(TEXT("삶의 방식을 고르세요. 저마다 속도·방어·자원·시작 덱이 다릅니다."));
	SelectedFormat = FText::FromString(TEXT("선택: {Title} · {Name}"));
	StartButtonText = FText::FromString(TEXT("이 삶의 방식으로 시작"));
	BackButtonText = FText::FromString(TEXT("뒤로"));
	StartingDeckLabel = FText::FromString(TEXT("시작 덱"));
	IllustrationPlaceholder = FText::FromString(TEXT("일러스트"));
	RatingNames = { FText::FromString(TEXT("화력")), FText::FromString(TEXT("생존")), FText::FromString(TEXT("기동")), FText::FromString(TEXT("성장")) };
}

void UBWClassSelectWidget::LoadCardData()
{
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	CardData = GI ? GI->GetAllClassData() : TArray<FBWClassData>();
	if (CardData.Num() > BWClassSelect::MaxCards) CardData.SetNum(BWClassSelect::MaxCards);
	if (CardData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClassSelect] DT_Classes를 찾지 못해 기본값으로 카드를 만듭니다."));
		for (EClassType Class : { EClassType::PHA, EClassType::TEC, EClassType::INT, EClassType::HP, EClassType::FTH })
			CardData.Add(BWClassSelect::MakeFallback(Class, GI));
	}
}

const FBWClassData* UBWClassSelectWidget::FindCard(EClassType ClassType) const
{
	return CardData.FindByPredicate([ClassType](const FBWClassData& Data) { return Data.ClassType == ClassType; });
}

bool UBWClassSelectWidget::IsClassAvailable(EClassType ClassType) const
{
	const FBWClassData* Data = FindCard(ClassType);
	return Data && Data->bSelectable;
}

void UBWClassSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	LoadCardData();

	// 블루프린트 디자이너에 카드 버튼(BTN_Class0)이 없으면 기본 화면을 코드로 만든다 (WBP_ClassSelect는 문구만 바꾸는 빈 블루프린트)
	if (!BTN_Class0 && WidgetTree) BuildDefaultLayout();

	if (BTN_Class0) BTN_Class0->OnClicked.AddDynamic(this, &UBWClassSelectWidget::OnClass0Clicked);
	if (BTN_Class1) BTN_Class1->OnClicked.AddDynamic(this, &UBWClassSelectWidget::OnClass1Clicked);
	if (BTN_Class2) BTN_Class2->OnClicked.AddDynamic(this, &UBWClassSelectWidget::OnClass2Clicked);
	if (BTN_Class3) BTN_Class3->OnClicked.AddDynamic(this, &UBWClassSelectWidget::OnClass3Clicked);
	if (BTN_Class4) BTN_Class4->OnClicked.AddDynamic(this, &UBWClassSelectWidget::OnClass4Clicked);
	if (BTN_Start) BTN_Start->OnClicked.AddDynamic(this, &UBWClassSelectWidget::OnStartClicked);
	if (BTN_Back) BTN_Back->OnClicked.AddDynamic(this, &UBWClassSelectWidget::OnBackClicked);
}

void UBWClassSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	OpenTime = 0.0f;
	ApplyIllustrations();
	RefreshCards();
	UpdateCardSizes(0.0f);
}

void UBWClassSelectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateCardSizes(InDeltaTime);
	UpdateLook(MyGeometry, InDeltaTime);
}

void UBWClassSelectWidget::UpdateLook(const FGeometry& MyGeometry, float DeltaTime)
{
	OpenTime += DeltaTime;

	// 화면 색: 고른 직업 색으로 천천히 물든다 (안 골랐으면 금빛)
	FLinearColor Target(0.9f, 0.75f, 0.4f);
	if (const FBWClassData* Chosen = FindCard(Selected)) Target = Chosen->Color;
	ThemeColor = FMath::CInterpTo(ThemeColor, Target, DeltaTime, 4.0f);
	const float Breath = 0.5f + 0.5f * FMath::Sin(OpenTime * 1.6f);
	if (BackGlow)
	{
		FLinearColor C = ThemeColor;
		C.A = 0.16f + 0.06f * Breath;
		BackGlow->SetColorAndOpacity(C);
	}

	// 카드: 처음 열 때 차례로 떠오르고, 마우스를 올리면 살짝 커지고, 고른 카드는 후광이 맥박친다
	UButton* Buttons[] = { BTN_Class0, BTN_Class1, BTN_Class2, BTN_Class3, BTN_Class4 };
	for (int32 i = 0; i < CardBoxes.Num(); ++i)
	{
		if (!CardData.IsValidIndex(i)) break;
		const FBWClassData& Info = CardData[i];
		const bool bSelected = Selected == Info.ClassType;
		const bool bHovered = Buttons[i] && Buttons[i]->IsHovered() && Info.bSelectable;

		const float Enter = FMath::Clamp((OpenTime - 0.08f * i) / 0.4f, 0.0f, 1.0f);
		const float EnterEase = 1.0f - FMath::Pow(1.0f - Enter, 3.0f);
		const float Scale = (bHovered && !bSelected) ? 1.035f : 1.0f;
		FWidgetTransform Transform;
		Transform.Translation = FVector2D(0.0f, (1.0f - EnterEase) * 60.0f);
		Transform.Scale = FVector2D(FMath::Lerp(0.92f, Scale, EnterEase));
		CardBoxes[i]->SetRenderTransform(Transform);
		CardBoxes[i]->SetRenderOpacity(EnterEase);

		if (CardGlows.IsValidIndex(i) && CardGlows[i])
		{
			const float GlowTarget = bSelected ? 0.55f + 0.25f * Breath : (bHovered ? 0.35f : 0.0f);
			const float GlowNow = FMath::FInterpTo(CardGlows[i]->GetRenderOpacity(), GlowTarget, DeltaTime, 8.0f);
			CardGlows[i]->SetRenderOpacity(GlowNow);
			CardGlows[i]->SetColorAndOpacity(Info.Color);
		}
	}

	// 떠오르는 불티
	const FVector2D ScreenSize = MyGeometry.GetLocalSize();
	if (ScreenSize.X > 1.0f)
	{
		for (int32 i = 0; i < Particles.Num(); ++i)
		{
			FVector4f& P = ParticleState[i];
			if (!bParticlesPlaced) { P.X *= ScreenSize.X; P.Y *= ScreenSize.Y; } // 첫 틱: 비율 → 화면 좌표
			P.Y -= P.Z * DeltaTime;
			P.W += DeltaTime;
			if (P.Y < -20.0f) { P.Y = ScreenSize.Y + 20.0f; P.X = FMath::FRand() * ScreenSize.X; }
			if (UCanvasPanelSlot* DotSlot = Cast<UCanvasPanelSlot>(Particles[i]->Slot))
				DotSlot->SetPosition(FVector2D(P.X + FMath::Sin(P.W * 0.9f) * 18.0f, P.Y));
			FLinearColor C = ThemeColor * 0.6f + FLinearColor(0.4f, 0.4f, 0.4f, 0.0f);
			C.A = (0.25f + 0.25f * FMath::Sin(P.W * 2.3f)) * FMath::Clamp(P.Y / (ScreenSize.Y * 0.4f), 0.0f, 1.0f);
			Particles[i]->SetColorAndOpacity(C);
		}
		bParticlesPlaced = true;
	}

	// 시작 버튼: 고를 수 있게 되면 은은하게 숨 쉰다
	if (BTN_Start && BTN_Start->GetIsEnabled())
	{
		const float Pulse = 0.85f + 0.15f * FMath::Sin(OpenTime * 4.0f);
		BTN_Start->SetBackgroundColor(FLinearColor(0.95f * Pulse, 0.7f * Pulse, 0.25f * Pulse));
	}
}

void UBWClassSelectWidget::OnClass0Clicked() { if (CardData.IsValidIndex(0)) SelectClass(CardData[0].ClassType); }
void UBWClassSelectWidget::OnClass1Clicked() { if (CardData.IsValidIndex(1)) SelectClass(CardData[1].ClassType); }
void UBWClassSelectWidget::OnClass2Clicked() { if (CardData.IsValidIndex(2)) SelectClass(CardData[2].ClassType); }
void UBWClassSelectWidget::OnClass3Clicked() { if (CardData.IsValidIndex(3)) SelectClass(CardData[3].ClassType); }
void UBWClassSelectWidget::OnClass4Clicked() { if (CardData.IsValidIndex(4)) SelectClass(CardData[4].ClassType); }

void UBWClassSelectWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BW_ClassSelectRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.025f, 1.0f));
	if (UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(Background))
	{
		BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BgSlot->SetOffsets(FMargin(0.0f));
	}

	// 배경 꾸밈: 가운데 큰 빛 번짐(고른 직업 색으로 물듦) → 떠오르는 불티 → 가장자리 어둠
	UTexture2D* Glow = GlowTexture.LoadSynchronous();
	UTexture2D* FrameGlow = LoadObject<UTexture2D>(nullptr, BWClassSelect::FrameGlowPath);
	BackGlow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IMG_BackGlow"));
	if (Glow) BackGlow->SetBrushFromTexture(Glow);
	BackGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* GlowSlot = Root->AddChildToCanvas(BackGlow))
	{
		GlowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		GlowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		GlowSlot->SetSize(FVector2D(2200.0f, 1300.0f));
	}
	for (int32 i = 0; i < NumParticles; ++i)
	{
		UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (Glow) Dot->SetBrushFromTexture(Glow);
		Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* DotSlot = Root->AddChildToCanvas(Dot))
		{
			const float DotSize = FMath::FRandRange(6.0f, 22.0f);
			DotSlot->SetSize(FVector2D(DotSize));
			DotSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
		Particles.Add(Dot);
		// 화면 크기를 모르는 첫 틱 전에는 비율(0~1)로 두고, 첫 틱에 화면 좌표로 바꾼다
		ParticleState.Add(FVector4f(FMath::FRand(), FMath::FRand(), FMath::FRandRange(18.0f, 60.0f), FMath::FRandRange(0.0f, 6.28f)));
	}
	if (UTexture2D* Vignette = VignetteTexture.LoadSynchronous())
	{
		UImage* VignetteImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IMG_Vignette"));
		VignetteImage->SetBrushFromTexture(Vignette);
		VignetteImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* VignetteSlot = Root->AddChildToCanvas(VignetteImage))
		{
			VignetteSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			VignetteSlot->SetOffsets(FMargin(0.0f));
		}
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	if (UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column))
	{
		ColumnSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ColumnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ColumnSlot->SetAutoSize(true);
	}

	auto MakeText = [this](const TCHAR* Name, int32 Size, const FLinearColor& Color, ETextJustify::Type Justify) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = Size >= 20 ? 1 : 0;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		// 밝은 카드(체력 회백색)에서도 흰 글씨가 보이도록 그림자
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		Text->SetJustification(Justify);
		Text->SetAutoWrapText(true);
		return Text;
	};

	// 제목: ── 삶의 방식 선택 ── + 작은 안내 문구
	const FLinearColor Gold(0.95f, 0.82f, 0.5f);
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));
	auto AddTitleLine = [this, TitleRow, &Gold]()
	{
		USizeBox* Line = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Line->SetWidthOverride(180.0f);
		Line->SetHeightOverride(2.0f);
		UImage* LineImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		LineImage->SetColorAndOpacity(Gold * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f));
		Line->SetContent(LineImage);
		if (UHorizontalBoxSlot* LineSlot = TitleRow->AddChildToHorizontalBox(Line))
		{
			LineSlot->SetVerticalAlignment(VAlign_Center);
			LineSlot->SetPadding(FMargin(22.0f, 0.0f));
		}
	};
	AddTitleLine();
	UTextBlock* Title = MakeText(TEXT("TXT_Title"), 40, Gold, ETextJustify::Center);
	Title->SetText(ScreenTitle);
	Title->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* TitleSlot = TitleRow->AddChildToHorizontalBox(Title)) TitleSlot->SetVerticalAlignment(VAlign_Center);
	AddTitleLine();
	if (UVerticalBoxSlot* TitleRowSlot = Column->AddChildToVerticalBox(TitleRow)) TitleRowSlot->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Subtitle = MakeText(TEXT("TXT_Subtitle"), 15, FLinearColor(0.75f, 0.72f, 0.65f, 0.85f), ETextJustify::Center);
	Subtitle->SetText(ScreenSubtitle);
	if (UVerticalBoxSlot* SubSlot = Column->AddChildToVerticalBox(Subtitle))
	{
		SubSlot->SetHorizontalAlignment(HAlign_Center);
		SubSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 16.0f));
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ClassRow"));
	if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row)) RowSlot->SetHorizontalAlignment(HAlign_Center);

	UButton** Buttons[] = { &BTN_Class0, &BTN_Class1, &BTN_Class2, &BTN_Class3, &BTN_Class4 };
	UTextBlock** Texts[] = { &TXT_Class0, &TXT_Class1, &TXT_Class2, &TXT_Class3, &TXT_Class4 };
	for (int32 i = 0; i < CardData.Num(); ++i)
	{
		const FBWClassData& Info = CardData[i];
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("BOX_Class%d"), i));
		Size->SetWidthOverride(CardWidthNormal);
		Size->SetHeightOverride(CardHeight);
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("BTN_Class%d"), i));
		// 카드 내용이 넘치면 카드 안에서 자른다 (후광은 카드 밖으로 번져야 하므로 크기 상자가 아니라 버튼에서 자름)
		Button->SetClipping(EWidgetClipping::ClipToBounds);

		// 카드 안: 스탯 이름 + 한 줄 소개 + 일러스트 + 수치 + (고르면) 긴 설명
		UVerticalBox* CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* StatText = MakeText(*FString::Printf(TEXT("TXT_ClassName%d"), i), 40, FLinearColor::White, ETextJustify::Center);
		StatText->SetText(Info.Title);
		if (UVerticalBoxSlot* NameSlot = CardColumn->AddChildToVerticalBox(StatText)) NameSlot->SetHorizontalAlignment(HAlign_Fill);
		UTextBlock* Tagline = MakeText(*FString::Printf(TEXT("TXT_ClassTag%d"), i), 14, FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), ETextJustify::Center);
		Tagline->SetText(Info.Tagline.IsEmpty() ? Info.DisplayName : FText::Format(FText::FromString(TEXT("{0} · {1}")), Info.DisplayName, Info.Tagline));
		if (UVerticalBoxSlot* TagSlot = CardColumn->AddChildToVerticalBox(Tagline))
		{
			TagSlot->SetHorizontalAlignment(HAlign_Fill);
			TagSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));
		}

		// 일러스트 칸: 그림은 칸을 꽉 채우고 넘치는 부분은 잘라 낸다. 그림이 없으면 직업 색 빈 칸 + 안내 글자
		USizeBox* ArtBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("BOX_Art%d"), i));
		ArtBox->SetHeightOverride(ArtHeightNormal);
		ArtBox->SetClipping(EWidgetClipping::ClipToBounds);
		UOverlay* ArtOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UScaleBox* ArtScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		ArtScale->SetStretch(EStretch::ScaleToFill);
		UImage* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("IMG_Class%d"), i));
		ArtScale->SetContent(Art);
		if (UOverlaySlot* ScaleSlot = ArtOverlay->AddChildToOverlay(ArtScale))
		{
			ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
			ScaleSlot->SetVerticalAlignment(VAlign_Fill);
		}
		UTextBlock* Placeholder = MakeText(*FString::Printf(TEXT("TXT_ArtPlaceholder%d"), i), 14, FLinearColor(1.0f, 1.0f, 1.0f, 0.35f), ETextJustify::Center);
		Placeholder->SetText(IllustrationPlaceholder);
		if (UOverlaySlot* PlaceholderSlot = ArtOverlay->AddChildToOverlay(Placeholder))
		{
			PlaceholderSlot->SetHorizontalAlignment(HAlign_Center);
			PlaceholderSlot->SetVerticalAlignment(VAlign_Center);
		}
		ArtBox->SetContent(ArtOverlay);
		if (UVerticalBoxSlot* ArtSlot = CardColumn->AddChildToVerticalBox(ArtBox))
		{
			ArtSlot->SetHorizontalAlignment(HAlign_Fill);
			ArtSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}

		// 일러스트 아래 정보 줄: [왼쪽: 능력 막대 + 수치] [오른쪽: 긴 설명]
		// 평소에는 긴 설명이 접혀 있어 왼쪽 칸이 카드 폭을 다 쓰고 가운데 정렬, 펼치면 왼쪽은 필요한 만큼만 쓰고 오른쪽에 설명이 붙는다
		UHorizontalBox* InfoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("BOX_Info%d"), i));
		UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// 능력 막대: 화력·생존·기동·성장을 5칸으로 (펼치면 칸과 글씨가 커진다)
		UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("BOX_Bars%d"), i));
		for (int32 r = 0; r < BWClassSelect::NumRatings; ++r)
		{
			UHorizontalBox* BarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LabelBox->SetWidthOverride(BarLabelWidthNormal);
			UTextBlock* BarLabel = MakeText(*FString::Printf(TEXT("TXT_Bar%d_%d"), i, r), BarFontNormal, FLinearColor::White, ETextJustify::Left);
			BarLabel->SetAutoWrapText(false);
			BarLabel->SetText(RatingNames.IsValidIndex(r) ? RatingNames[r] : FText::GetEmpty());
			LabelBox->SetContent(BarLabel);
			if (UHorizontalBoxSlot* LabelSlot = BarRow->AddChildToHorizontalBox(LabelBox)) LabelSlot->SetVerticalAlignment(VAlign_Center);
			for (int32 c = 0; c < BWClassSelect::RatingCells; ++c)
			{
				USizeBox* CellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				CellBox->SetWidthOverride(BarCellSizeNormal.X);
				CellBox->SetHeightOverride(BarCellSizeNormal.Y);
				UImage* Cell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				CellBox->SetContent(Cell);
				if (UHorizontalBoxSlot* CellSlot = BarRow->AddChildToHorizontalBox(CellBox))
				{
					CellSlot->SetVerticalAlignment(VAlign_Center);
					CellSlot->SetPadding(FMargin(1.5f, 0.0f));
				}
				BarCells.Add(Cell);
				BarCellBoxes.Add(CellBox);
			}
			BarLabels.Add(BarLabel);
			BarLabelBoxes.Add(LabelBox);
			if (UVerticalBoxSlot* BarSlot = Bars->AddChildToVerticalBox(BarRow)) BarSlot->SetPadding(FMargin(0.0f, 1.5f));
		}
		if (Info.RatingFirepower + Info.RatingSurvival + Info.RatingMobility + Info.RatingGrowth == 0) Bars->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* BarsSlot = LeftColumn->AddChildToVerticalBox(Bars))
		{
			BarsSlot->SetHorizontalAlignment(HAlign_Center);
			BarsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}

		UTextBlock* Label = MakeText(*FString::Printf(TEXT("TXT_Class%d"), i), 14, FLinearColor::White, ETextJustify::Center);
		if (UVerticalBoxSlot* BodySlot = LeftColumn->AddChildToVerticalBox(Label)) BodySlot->SetHorizontalAlignment(HAlign_Fill);

		if (UHorizontalBoxSlot* LeftSlot = InfoRow->AddChildToHorizontalBox(LeftColumn))
		{
			LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LeftSlot->SetVerticalAlignment(VAlign_Top);
			InfoLeftSlots.Add(LeftSlot);
		}

		UTextBlock* LongText = MakeText(*FString::Printf(TEXT("TXT_ClassLong%d"), i), LongTextFontSize, FLinearColor::White, ETextJustify::Left);
		LongText->SetVisibility(ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* LongSlot = InfoRow->AddChildToHorizontalBox(LongText))
		{
			LongSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LongSlot->SetVerticalAlignment(VAlign_Top);
			LongSlot->SetPadding(FMargin(22.0f, 0.0f, 10.0f, 0.0f));
		}
		if (UVerticalBoxSlot* InfoSlot = CardColumn->AddChildToVerticalBox(InfoRow)) InfoSlot->SetHorizontalAlignment(HAlign_Fill);

		// 버튼은 기본으로 내용을 가운데에 작게 놓으므로, 카드 폭을 꽉 채우고 위에서부터 쌓이게 한다
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->SetContent(CardColumn)))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetVerticalAlignment(VAlign_Top);
			ContentSlot->SetPadding(FMargin(10.0f, 12.0f));
		}
		// 카드 뒤 후광 (고르거나 마우스를 올리면 빛남)
		UOverlay* CardStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* CardGlow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("IMG_CardGlow%d"), i));
		// 테두리 후광은 9분할(Box)로 그려 카드 폭이 바뀌어도 모서리 모양이 유지되게 한다
		if (FrameGlow)
		{
			FSlateBrush GlowBrush;
			GlowBrush.SetResourceObject(FrameGlow);
			GlowBrush.ImageSize = FVector2D(128.0f);
			GlowBrush.DrawAs = ESlateBrushDrawType::Box;
			GlowBrush.Margin = FMargin(0.45f);
			CardGlow->SetBrush(GlowBrush);
		}
		CardGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
		CardGlow->SetRenderOpacity(0.0f);
		if (UOverlaySlot* GlowSlot = CardStack->AddChildToOverlay(CardGlow))
		{
			GlowSlot->SetHorizontalAlignment(HAlign_Fill);
			GlowSlot->SetVerticalAlignment(VAlign_Fill);
			GlowSlot->SetPadding(FMargin(-38.0f));
		}
		if (UOverlaySlot* ButtonSlot = CardStack->AddChildToOverlay(Button))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Size->SetContent(CardStack);
		Size->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		CardGlows.Add(CardGlow);
		if (UHorizontalBoxSlot* CardSlot = Row->AddChildToHorizontalBox(Size))
		{
			CardSlot->SetPadding(FMargin(8.0f, 0.0f));
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}
		*Buttons[i] = Button;
		*Texts[i] = Label;

		CardBoxes.Add(Size);
		ArtBoxes.Add(ArtBox);
		ArtImages.Add(Art);
		ArtPlaceholders.Add(Placeholder);
		TitleTexts.Add(StatText);
		TagTexts.Add(Tagline);
		LongTexts.Add(LongText);
		CardWidths.Add(CardWidthNormal);
		ArtHeights.Add(ArtHeightNormal);
	}

	USizeBox* DetailWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DetailWidth->SetWidthOverride(1220.0f);
	DetailWidth->SetMinDesiredHeight(30.0f);
	TXT_Detail = MakeText(TEXT("TXT_Detail"), 15, FLinearColor(0.82f, 0.82f, 0.86f), ETextJustify::Center);
	DetailWidth->SetContent(TXT_Detail);
	if (UVerticalBoxSlot* DetailSlot = Column->AddChildToVerticalBox(DetailWidth))
	{
		DetailSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 14.0f));
		DetailSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Footer"));
	if (UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Footer)) FooterSlot->SetHorizontalAlignment(HAlign_Center);

	auto MakeFooterButton = [this, Footer, &MakeText](const TCHAR* ButtonName, const FText& Label, const FLinearColor& Tint) -> UButton*
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetMinDesiredWidth(220.0f); // 문구가 길어지면 버튼이 늘어난다
		Size->SetHeightOverride(54.0f);
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		Button->SetBackgroundColor(Tint);
		UTextBlock* Text = MakeText(*FString::Printf(TEXT("%s_Label"), ButtonName), 18, FLinearColor::White, ETextJustify::Center);
		Text->SetText(Label);
		Text->SetAutoWrapText(false);
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->SetContent(Text))) LabelSlot->SetPadding(FMargin(24.0f, 0.0f));
		Size->SetContent(Button);
		if (UHorizontalBoxSlot* ButtonSlot = Footer->AddChildToHorizontalBox(Size)) ButtonSlot->SetPadding(FMargin(12.0f, 0.0f));
		return Button;
	};
	BTN_Back = MakeFooterButton(TEXT("BTN_Back"), BackButtonText, FLinearColor(0.35f, 0.35f, 0.4f));
	BTN_Start = MakeFooterButton(TEXT("BTN_Start"), StartButtonText, FLinearColor(0.95f, 0.7f, 0.25f));
}

void UBWClassSelectWidget::ApplyIllustrations()
{
	for (int32 i = 0; i < ArtImages.Num() && CardData.IsValidIndex(i); ++i)
	{
		const FBWClassData& Info = CardData[i];
		TSoftObjectPtr<UTexture2D> Soft = Info.Illustration;
		if (Soft.IsNull())
		{
			// 정해 둔 경로에 그림을 넣기만 해도 쓰이도록
			const TCHAR* Stat = BWClassSelect::StatName(Info.ClassType);
			const FString Package = FString::Printf(TEXT("/Game/HUD/Texture/ClassArt/T_ClassArt_%s"), Stat);
			if (FPackageName::DoesPackageExist(Package))
				Soft = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%s.T_ClassArt_%s"), *Package, Stat)));
		}

		UTexture2D* Texture = Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
		if (Texture)
		{
			ArtImages[i]->SetBrushFromTexture(Texture, true);
			ArtImages[i]->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			ArtImages[i]->SetColorAndOpacity(Info.Color * 0.35f + FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
		}
		if (ArtPlaceholders[i]) ArtPlaceholders[i]->SetVisibility(Texture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UBWClassSelectWidget::UpdateCardSizes(float DeltaTime)
{
	const bool bAnySelected = Selected != EClassType::Common;
	for (int32 i = 0; i < CardBoxes.Num() && CardData.IsValidIndex(i); ++i)
	{
		const bool bSelected = Selected == CardData[i].ClassType;
		const float TargetWidth = !bAnySelected ? CardWidthNormal : (bSelected ? CardWidthExpanded : CardWidthShrunk);
		const float TargetArt = !bAnySelected ? ArtHeightNormal : (bSelected ? ArtHeightExpanded : ArtHeightShrunk);
		CardWidths[i] = DeltaTime > 0.0f ? FMath::FInterpTo(CardWidths[i], TargetWidth, DeltaTime, ExpandSpeed) : TargetWidth;
		ArtHeights[i] = DeltaTime > 0.0f ? FMath::FInterpTo(ArtHeights[i], TargetArt, DeltaTime, ExpandSpeed) : TargetArt;
		CardBoxes[i]->SetWidthOverride(CardWidths[i]);
		ArtBoxes[i]->SetHeightOverride(ArtHeights[i]);

		// 긴 설명은 카드가 거의 다 펼쳐진 뒤에 서서히 나타난다 (좁을 때 글자가 마구 줄바꿈되는 것을 피함)
		if (UTextBlock* LongText = LongTexts[i])
		{
			const float Open = FMath::Clamp((CardWidths[i] - CardWidthNormal) / FMath::Max(1.0f, CardWidthExpanded - CardWidthNormal), 0.0f, 1.0f);
			const bool bShow = bSelected && Open > 0.75f;
			LongText->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			LongText->SetRenderOpacity(bShow ? (Open - 0.75f) / 0.25f : 0.0f);
			// 설명이 보이면 정보 줄 왼쪽(막대·수치)은 필요한 폭만, 아니면 카드 폭을 다 써서 가운데 정렬
			if (InfoLeftSlots.IsValidIndex(i) && InfoLeftSlots[i])
				InfoLeftSlots[i]->SetSize(FSlateChildSize(bShow ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill));

			// 능력 막대는 펼치는 만큼 커진다 (고른 카드만)
			const float Grow = bSelected ? Open : 0.0f;
			const FVector2D CellSize = FMath::Lerp(BarCellSizeNormal, BarCellSizeExpanded, Grow);
			for (int32 c = 0; c < BWClassSelect::NumRatings * BWClassSelect::RatingCells; ++c)
			{
				const int32 Index = i * BWClassSelect::NumRatings * BWClassSelect::RatingCells + c;
				if (!BarCellBoxes.IsValidIndex(Index) || !BarCellBoxes[Index]) continue;
				BarCellBoxes[Index]->SetWidthOverride(CellSize.X);
				BarCellBoxes[Index]->SetHeightOverride(CellSize.Y);
			}
			const int32 FontSize = Grow > 0.5f ? BarFontExpanded : BarFontNormal;
			for (int32 r = 0; r < BWClassSelect::NumRatings; ++r)
			{
				const int32 Index = i * BWClassSelect::NumRatings + r;
				if (BarLabelBoxes.IsValidIndex(Index) && BarLabelBoxes[Index])
					BarLabelBoxes[Index]->SetWidthOverride(FMath::Lerp(BarLabelWidthNormal, BarLabelWidthExpanded, Grow));
				UTextBlock* BarLabel = BarLabels.IsValidIndex(Index) ? BarLabels[Index].Get() : nullptr;
				if (BarLabel && BarLabel->GetFont().Size != FontSize)
				{
					FSlateFontInfo Font = BarLabel->GetFont();
					Font.Size = FontSize;
					BarLabel->SetFont(Font);
				}
			}
		}
	}
}

void UBWClassSelectWidget::SelectClass(EClassType ClassType)
{
	if (!IsClassAvailable(ClassType)) return;
	Selected = ClassType;
	RefreshCards();
}

void UBWClassSelectWidget::RefreshCards()
{
	UButton* Buttons[] = { BTN_Class0, BTN_Class1, BTN_Class2, BTN_Class3, BTN_Class4 };
	UTextBlock* Texts[] = { TXT_Class0, TXT_Class1, TXT_Class2, TXT_Class3, TXT_Class4 };

	for (int32 i = 0; i < BWClassSelect::MaxCards; ++i)
	{
		if (!CardData.IsValidIndex(i))
		{
			// 테이블 행이 5개보다 적으면 남는 블루프린트 버튼은 숨긴다
			if (Buttons[i]) Buttons[i]->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FBWClassData& Info = CardData[i];
		const bool bAvailable = Info.bSelectable;
		const bool bSelected = Selected == Info.ClassType;

		if (Buttons[i])
		{
			Buttons[i]->SetIsEnabled(bAvailable);
			// 고른 직업은 밝게, 나머지는 어둡게 (아직 안 골랐으면 모두 보통)
			const float Strength = !bAvailable ? 0.25f : (bSelected ? 1.0f : (Selected == EClassType::Common ? 0.6f : 0.35f));
			Buttons[i]->SetBackgroundColor(Info.Color * Strength + FLinearColor(0.0f, 0.0f, 0.0f, 1.0f - Strength));
		}
		if (!Texts[i]) continue;

		// 수치 줄은 테이블의 규칙 수치로 자동으로 만든다 (MainStatText를 적으면 주 능력치 줄을 그 글로)
		const FBWClassStartStats& Stats = Info.StartStats;
		const FString MainPart = !Info.MainStatText.IsEmpty() ? Info.MainStatText.ToString()
			: FString::Printf(TEXT("%s %d"), BWClassSelect::StatName(Info.ClassType), BWClassSelect::MainStatValue(Info));
		FString Label = FString::Printf(TEXT("HP %d\n속도 %d~%d\n%s\n\n%s\n%s"),
			Stats.MaxHP, Info.Speed.Min, Info.Speed.Max, *MainPart, *Info.DefenseText.ToString(), *Info.ResourceText.ToString());
		if (!bAvailable) Label = Info.LockedText.ToString();
		Texts[i]->SetText(FText::FromString(Label));

		UTextBlock* LongText = LongTexts.IsValidIndex(i) ? LongTexts[i].Get() : nullptr;
		if (LongText && bSelected)
		{
			// 수치는 왼쪽 칸에 그대로 보이므로 오른쪽은 기본 공격 한 줄 + 설명 + 시작 덱
			FBWSkillStats StartSkill;
			StartSkill.PHA = Stats.PHA; StartSkill.TEC = Stats.TEC; StartSkill.INT = Stats.INT; StartSkill.FTH = Stats.FTH; StartSkill.MaxHP = Stats.MaxHP;
			const FBWDiceRange Basic = UBWDuelRules::ComputeCardSkillRange(Info.BasicStrike, StartSkill);
			const FString AttackLine = FString::Printf(TEXT("기본 공격 %d~%d · 공격카드 강화 %d%%"), Basic.Min, Basic.Max, FMath::RoundToInt(Info.CardPower * 100.0f));
			LongText->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s\n\n%s — %s"),
				*AttackLine, *Info.Description.ToString(), *StartingDeckLabel.ToString(), *DescribeStartingDeck(Info.ClassType))));
		}

		// 밝게 켜진 회백색 카드는 검은 글씨, 나머지는 흰 글씨
		const bool bDark = bAvailable && bSelected && Info.bDarkText;
		const FLinearColor Ink = !bAvailable ? FLinearColor(0.55f, 0.55f, 0.6f) : (bDark ? FLinearColor(0.08f, 0.08f, 0.1f) : FLinearColor::White);
		const FLinearColor Shadow(0.0f, 0.0f, 0.0f, bDark ? 0.0f : 0.85f);
		UTextBlock* TagText = TagTexts.IsValidIndex(i) ? TagTexts[i].Get() : nullptr;
		UTextBlock* CardTexts[] = { Texts[i], TitleTexts.IsValidIndex(i) ? TitleTexts[i].Get() : nullptr, TagText, LongText };
		for (UTextBlock* Text : CardTexts)
		{
			if (!Text) continue;
			FLinearColor Color = Ink;
			if (Text == TagText) Color.A = 0.8f; // 한 줄 소개는 조금 흐리게
			Text->SetColorAndOpacity(Color);
			Text->SetShadowColorAndOpacity(Shadow);
		}

		// 능력 막대 색: 찬 칸은 직업 색을 밝게(밝은 카드면 어둡게), 빈 칸은 반투명
		for (int32 r = 0; r < BWClassSelect::NumRatings; ++r)
		{
			const int32 LabelIndex = i * BWClassSelect::NumRatings + r;
			if (BarLabels.IsValidIndex(LabelIndex) && BarLabels[LabelIndex])
			{
				BarLabels[LabelIndex]->SetColorAndOpacity(Ink);
				BarLabels[LabelIndex]->SetShadowColorAndOpacity(Shadow);
			}
			for (int32 c = 0; c < BWClassSelect::RatingCells; ++c)
			{
				const int32 CellIndex = LabelIndex * BWClassSelect::RatingCells + c;
				if (!BarCells.IsValidIndex(CellIndex) || !BarCells[CellIndex]) continue;
				const bool bFilled = c < BWClassSelect::Rating(Info, r);
				const FLinearColor Filled = bDark ? FLinearColor(0.12f, 0.12f, 0.14f) : Info.Color * 0.5f + FLinearColor(0.5f, 0.5f, 0.5f, 0.5f);
				const FLinearColor Empty = bDark ? FLinearColor(1.0f, 1.0f, 1.0f, 0.35f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.35f);
				BarCells[CellIndex]->SetColorAndOpacity(bFilled ? Filled : Empty);
			}
		}
	}

	if (BTN_Start) BTN_Start->SetIsEnabled(Selected != EClassType::Common);
	if (TXT_Detail)
	{
		FText Detail = PromptText;
		if (const FBWClassData* Info = FindCard(Selected))
		{
			// 기본 화면은 시작 덱을 펼친 카드 안에 보여 준다. 블루프린트 디자인이면 여기에 덱을 적는다
			FFormatNamedArguments Args;
			Args.Add(TEXT("Title"), Info->Title);
			Args.Add(TEXT("Name"), Info->DisplayName);
			Detail = LongTexts.Num() > 0 ? FText::Format(SelectedFormat, Args)
				: FText::FromString(FString::Printf(TEXT("%s %s — %s"), *Info->DisplayName.ToString(), *StartingDeckLabel.ToString(), *DescribeStartingDeck(Selected)));
		}
		TXT_Detail->SetText(Detail);
	}
}

FString UBWClassSelectWidget::DescribeStartingDeck(EClassType ClassType) const
{
	UClass* CharClass = PlayerCharacterClass.LoadSynchronous();
	const ABWCharacter* CDO = CharClass ? CharClass->GetDefaultObject<ABWCharacter>() : nullptr;
	const UDeckComponent* Deck = CDO ? CDO->DeckComponent : nullptr;

	// 게임과 같은 순서로 찾는다: DT_Classes의 StartingDeck → 없으면 말 블루프린트의 직업별 배열
	TArray<FName> Names;
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || !GI->GetClassStartingDeck(ClassType, Names))
	{
		if (!Deck) return TEXT("(덱 정보를 찾지 못했습니다)");
		switch (ClassType)
		{
		case EClassType::PHA: Names = Deck->PHAStartingDeck; break;
		case EClassType::TEC: Names = Deck->TECStartingDeck; break;
		case EClassType::INT: Names = Deck->INTStartingDeck; break;
		case EClassType::FTH: Names = Deck->FTHStartingDeck; break;
		case EClassType::HP:  Names = Deck->HPStartingDeck;  break;
		default: return FString();
		}
	}

	// 같은 카드는 묶어서 '타격 ×3'처럼 보여 준다 (덱에 적힌 순서 유지)
	const UDataTable* CardTable = Deck ? Deck->CardDataTable : nullptr;
	TArray<FString> Order;
	TMap<FString, int32> Counts;
	int32 Attacks = 0, Effects = 0;
	for (const FName& Row : Names)
	{
		const FBWCardData* Card = CardTable ? CardTable->FindRow<FBWCardData>(Row, TEXT("ClassSelect")) : nullptr;
		const FString Name = Card ? Card->CardName : Row.ToString();
		if (Card) ++(Card->CardKind == ECardKind::Effect ? Effects : Attacks);
		if (!Counts.Contains(Name)) Order.Add(Name);
		++Counts.FindOrAdd(Name);
	}

	TArray<FString> Parts;
	for (const FString& Name : Order)
	{
		const int32 Count = Counts[Name];
		Parts.Add(Count > 1 ? FString::Printf(TEXT("%s ×%d"), *Name, Count) : Name);
	}
	return FString::Printf(TEXT("공격 %d · 효과 %d장\n%s"), Attacks, Effects, *FString::Join(Parts, TEXT(", ")));
}

void UBWClassSelectWidget::StartRun()
{
	if (Selected == EClassType::Common) return;
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	GI->BeginNewRun(Selected);
	GI->GenerateWorldMap(15);
	GI->SaveRun(); // 이전 판 저장은 여기서 덮어쓴다
	UE_LOG(LogTemp, Warning, TEXT("[Title] Class chosen, map generated, moving to %s"), *TargetLevelName.ToString());
	UGameplayStatics::OpenLevel(GetWorld(), TargetLevelName);
}

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"
#include "Components/Button.h"
#include "Components/Border.h"

// HUD 틀(B 석판) 조각: /Game/HUD/Texture/UI/Skin/T_UI_<이름>
// 9분할(Box)로 늘려 쓰고, 텍스처가 없으면 false를 돌려 호출한 쪽이 옛 모양을 쓴다.
namespace BWSkin
{
	struct FPiece { const TCHAR* Name; FMargin Margin; float Scale; };

	inline UTexture2D* LoadPiece(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/HUD/Texture/UI/Skin/T_UI_%s.T_UI_%s"), Name, Name);
		return Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad());
	}

	// Margin은 텍스처 크기에 대한 비율, Scale은 화면에 그릴 때 텍스처를 몇 배로 볼지(모서리 크기)
	inline bool MakeBox(const TCHAR* Name, const FMargin& Margin, float Scale, FSlateBrush& Out, const FLinearColor& Tint = FLinearColor::White)
	{
		UTexture2D* Tex = LoadPiece(Name);
		if (!Tex) return false;
		Out = FSlateBrush();
		Out.SetResourceObject(Tex);
		Out.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY()) * Scale;
		Out.DrawAs = ESlateBrushDrawType::Box;
		Out.Margin = Margin;
		Out.TintColor = FSlateColor(Tint);
		return true;
	}

	inline bool MakeImage(const TCHAR* Name, const FVector2D& Size, FSlateBrush& Out, const FLinearColor& Tint = FLinearColor::White)
	{
		UTexture2D* Tex = LoadPiece(Name);
		if (!Tex) return false;
		Out = FSlateBrush();
		Out.SetResourceObject(Tex);
		Out.ImageSize = Size;
		Out.DrawAs = ESlateBrushDrawType::Image;
		Out.TintColor = FSlateColor(Tint);
		return true;
	}

	// 자주 쓰는 조각 (가장자리 두께는 조각 그림에서 잰 값)
	inline bool Panel(FSlateBrush& Out)   { return MakeBox(TEXT("panel_large"), FMargin(0.075f, 0.12f), 0.5f, Out); }
	inline bool Small(FSlateBrush& Out)   { return MakeBox(TEXT("panel_small"), FMargin(0.13f), 0.4f, Out); }
	inline bool Tooltip(FSlateBrush& Out) { return MakeBox(TEXT("panel_tooltip"), FMargin(0.055f, 0.095f), 0.5f, Out); }
	// 작은 칸용: 모서리 장식이 작은 얇은 틀 (큰 판의 삼각형을 작은 칸에 쓰지 않는다)
	inline bool Thin(FSlateBrush& Out)    { return MakeBox(TEXT("panel_tooltip"), FMargin(0.055f, 0.095f), 0.26f, Out); }
	inline bool Banner(FSlateBrush& Out)  { return MakeBox(TEXT("banner"), FMargin(0.06f, 0.32f), 0.45f, Out); }
	inline bool BarFrame(FSlateBrush& Out){ return MakeBox(TEXT("bar_frame"), FMargin(0.03f, 0.3f), 0.35f, Out); }
	// bRed: 붉은 강조 버튼, State 0 평소 / 1 올림 / 2 누름
	inline bool Button(bool bRed, int32 State, FSlateBrush& Out)
	{
		static const TCHAR* Names[2][3] = { { TEXT("btn_sage_normal"), TEXT("btn_sage_hover"), TEXT("btn_sage_pressed") },
		                                    { TEXT("btn_red_normal"), TEXT("btn_red_hover"), TEXT("btn_red_pressed") } };
		return MakeBox(Names[bRed ? 1 : 0][FMath::Clamp(State, 0, 2)], FMargin(0.11f, 0.33f), 0.38f, Out);
	}

	// 창 안의 작은 버튼: 장식 없이 석판색 + 은빛 테두리 (모서리 장식은 전투 HUD 큰 버튼에만)
	inline FSlateBrush Plain(const FLinearColor& Fill, const FLinearColor& Edge, float Width)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings.CornerRadii = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.Color = FSlateColor(Edge);
		B.OutlineSettings.Width = Width;
		return B;
	}
	inline bool StyleButton(UButton* Target, bool bRed = false)
	{
		if (!Target) return false;
		const FLinearColor Fill = bRed ? FLinearColor(0.36f, 0.13f, 0.12f, 1.0f) : FLinearColor(0.17f, 0.21f, 0.25f, 1.0f);
		FButtonStyle Style = Target->GetStyle();
		Style.SetNormal(Plain(Fill, FLinearColor(0.68f, 0.7f, 0.74f, 0.85f), 1.5f));
		Style.SetHovered(Plain(Fill * 1.35f, FLinearColor(0.92f, 0.94f, 0.97f, 1.0f), 2.0f));
		Style.SetPressed(Plain(Fill * 0.75f, FLinearColor(0.92f, 0.94f, 0.97f, 1.0f), 2.0f));
		Style.SetDisabled(Plain(FLinearColor(0.1f, 0.11f, 0.13f, 0.9f), FLinearColor(1, 1, 1, 0.12f), 1.0f));
		Style.SetNormalPadding(FMargin(20.0f, 8.0f));
		Style.SetPressedPadding(FMargin(20.0f, 9.0f, 20.0f, 7.0f));
		Target->SetStyle(Style);
		Target->SetBackgroundColor(FLinearColor::White);
		return true;
	}

	// 증강 아이콘: 표의 Icon → 없으면 /Game/HUD/Texture/AugmentIcons/T_AugIcon_<행 이름> → 둘 다 없으면 nullptr
	inline UTexture2D* LoadAugmentIcon(FName Row, const TSoftObjectPtr<UTexture2D>& TableIcon)
	{
		if (UTexture2D* Tex = TableIcon.LoadSynchronous()) return Tex;
		const FString Name = Row.ToString();
		return Cast<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/HUD/Texture/AugmentIcons/T_AugIcon_%s.T_AugIcon_%s"), *Name, *Name)).TryLoad());
	}
	// 지도·HUD 아이콘: /Game/HUD/Texture/MapIcons/T_Icon_<이름> (Node_Battle, HUD_Gold, Set_Astrin, Mark_Bonus …)
	inline UTexture2D* LoadIcon(const FString& Name)
	{
		return Cast<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/HUD/Texture/MapIcons/T_Icon_%s.T_Icon_%s"), *Name, *Name)).TryLoad());
	}
	// 세트 아이콘 이름 (EBWAugmentSet 순서)
	inline FString SetIconName(uint8 Set)
	{
		static const TCHAR* Names[] = { TEXT(""), TEXT("Astrin"), TEXT("Sylpira"), TEXT("Soltheris"), TEXT("Urska"), TEXT("Lirin"), TEXT("Lunaris"),
			TEXT("Berend"), TEXT("Taiga"), TEXT("Geleon"), TEXT("Patrovas"), TEXT("Megenberta"), TEXT("Radiant"), TEXT("Lifestyle") };
		return Set > 0 && Set < UE_ARRAY_COUNT(Names) ? FString(TEXT("Set_")) + Names[Set] : FString();
	}

	// 증강 종류 색: 안정 파랑, 변화 금빛, 도박 붉음 (Kind: 0 안정 / 1 변화 / 2 도박)
	inline FLinearColor AugmentAccent(uint8 Kind)
	{
		return Kind == 2 ? FLinearColor(1.0f, 0.3f, 0.25f) : Kind == 1 ? FLinearColor(1.0f, 0.78f, 0.3f) : FLinearColor(0.35f, 0.65f, 1.0f);
	}

	// 큰 판(창·모달)을 석판으로
	inline bool StylePanel(UBorder* Target, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Skin;
		if (!Target || !Panel(Skin)) return false;
		Skin.TintColor = FSlateColor(Tint);
		Target->SetBrush(Skin);
		Target->SetBrushColor(FLinearColor::White); // 예전 어두운 덧칠(BrushColor)이 석판을 덮지 않게
		return true;
	}
}

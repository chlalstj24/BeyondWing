#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWPauseWidget.generated.h"

class UButton;

// [추가] Esc 메뉴: 계속하기, 환경설정, 진행 저장 후 타이틀로. 화면은 코드로 만든다
UCLASS()
class BEYONDWING_API UBWPauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Pause") FName TitleLevelName = TEXT("/Game/System/Level/TitleLevel");

	void Close();

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void OnResumeClicked() { Close(); }
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnSaveQuitClicked();
	UButton* AddButton(class UVerticalBox* Column, const FString& Text);
};

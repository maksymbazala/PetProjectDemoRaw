// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include "VEN_Types.h"

#include "VEN_CursorManager.generated.h"

class AVEN_GameMode;

UCLASS()
class GAME_4_24_API UVEN_CursorManager : public UObject
{
	GENERATED_BODY()

public:

	UVEN_CursorManager();

public:

	void Initialize();
	void RequestCursorChange(vendetta::CURSOR_TYPE new_cursor);

private:

	AVEN_GameMode* GetGameMode() const;

private:

	vendetta::CURSOR_TYPE m_current_cursor;
};

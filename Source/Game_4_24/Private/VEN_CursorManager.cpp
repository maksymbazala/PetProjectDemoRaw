// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_CursorManager.h"
#include "VEN_GameMode.h"
#include "VEN_FreeFunctions.h"

using namespace vendetta;

UVEN_CursorManager::UVEN_CursorManager()
	: m_current_cursor(CURSOR_TYPE::COMMON)
{

}

void UVEN_CursorManager::Initialize()
{
	m_current_cursor = CURSOR_TYPE::COMMON;
	GetGameMode()->OnCursorChange(m_current_cursor);
}

void UVEN_CursorManager::RequestCursorChange(CURSOR_TYPE new_cursor)
{
	if (m_current_cursor != new_cursor)
	{
		m_current_cursor = new_cursor;
		GetGameMode()->OnCursorChange(m_current_cursor);
	}
}

AVEN_GameMode* UVEN_CursorManager::GetGameMode() const
{
	auto game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	//if (!game_mode)
		//debug_log("UVEN_QuestsManager::GetGameMode. Game Mode not found!", FColor::Red);

	return game_mode;
}
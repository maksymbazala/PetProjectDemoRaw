#pragma once

#include "Engine/EngineTypes.h"
#include "CoreMinimal.h"
#include "VEN_Types.generated.h"

namespace vendetta
{
	const FString QUEST_PLAYER_UNIT_MENTION_REPLACER = "#####";

	//directions
	enum DIRECTION
	{
		BACK,
		FRONT,
		LEFT,
		RIGHT
	};

	//quests related
	enum QUEST_ID
	{
		NONE = -1,
		WELCOME = 0,
		HEALING_HERBS,
		SAVE_SISTER_PART_1,
		SAVE_SISTER_PART_2,
		GOOD_BYE
	};
	enum QUEST_STATUS
	{
		UNALLOWED,
		ALLOWED,
		IN_PROGRESS,
		CAN_BE_COMPLETED,
		COMPLETED
	};

	//quests notifications related
	enum QUEST_UPDATE
	{
		UNALLOW,
		ALLOW,
		START_PROGRESS,
		INCREMENT_COUNTER,
		FINISH_PROGRESS,
		COMPLETE
	};

	//quests manager related
	enum QUESTS_MANAGER_UPDATE
	{
		ALLOW_QUEST,
		UNALLOW_QUEST,
		INTERACT_WITH_NPC,
		INTERACT_WITH_ITEM,
		ENEMY_UNIT_KILLED,
	};

	//actors related
	enum ACTOR_UPDATE
	{
		SPAWN,
		INTERACT,
		REMOVE
	};

	//battle related
	enum class ATTACK_TYPE
	{
		NONE,
		MELEE,
		RANGE
	};

	enum class IN_BATTLE_UNIT_ACTION
	{
		MOVE,
		ATTACK,
		MOVE_AND_ATTACK
	};

	//animations
	enum class ANIMATION_ACTION
	{
		IDLE,
		PICK_ITEM,
		ATTACK_1,
		ATTACK_2,
		RECEIVE_DAMAGE_BACK,
		RECEIVE_DAMAGE_FRONT,
		RECEIVE_DAMAGE_LEFT,
		RECEIVE_DAMAGE_RIGHT,
		STUN,
		DIE
	};

	enum class ANIMATION_UPDATE
	{
		START_BATTLE,
		FINISH_BATTLE,
		FINISH_STUN,
		START_NEXT_ACTION,
		START_MOVEMENT
	};

	enum class ANIMATION_NOTIFICATION
	{
		ITEM_PICK_ANIMATION_UPDATED,
		ITEM_PICK_ANIMATION_FINISHED,
		ATTACK_ANIMATION_UPDATED,
		ATTACK_ANIMATION_FINISHED,
		RECEIVE_DAMAGE_ANIMATION_FINISHED,
		STUN_ANIMATION_FINISHED,
		DEATH_ANIMATION_FINISHED
	};

	enum class CURSOR_TYPE
	{
		COMMON,
		CAMERA,
		MOVE,
		ERROR,
		ATTACK_MELEE,
		ATTACK_RANGE,
		TALK,
		COLLECT
	};

	enum class CAMERA_PP_MATERIAL_TYPE
	{
		DEFAULT,
		ENEMY_UNIT
	};
}


/* ##### UENUMs ##### */


UENUM(BlueprintType)
enum class EEnemyUnitType : uint8
{
	MERCENARY_COMMON UMETA(DisplayName = "Mercenary Common"),
	MERCENARY_LEADER UMETA(DisplayName = "Mercenary Leader")
};

UENUM(BlueprintType)
enum class EPlayerUnitType : uint8
{
	BROTHER UMETA(DisplayName = "Brother"),
	SISTER UMETA(DisplayName = "Sister")
};

UENUM(BlueprintType)
enum class EBattleQueueUnitIconType : uint8
{
	PLAYER_UNIT_BROTHER UMETA(DisplayName = "Player Unit Brother"),
	PLAYER_UNIT_SISTER UMETA(DisplayName = "Player Unit Sister"),
	ENEMY_UNIT_1 UMETA(DisplayName = "Enemy Unit 1"),
	ENEMY_UNIT_2 UMETA(DisplayName = "Enemy Unit 2"),
	ENEMY_UNIT_3 UMETA(DisplayName = "Enemy Unit 3")
};

UENUM(BlueprintType)
enum class EWidgetButtonType : uint8
{
	FINISH_TURN UMETA(DisplayName = "Finish Turn"),
	FOLLOW_MY_LEAD UMETA(DisplayName = "Follow My Lead")
};

UENUM(BlueprintType)
enum class EWidgetButtonState : uint8
{
	ACTIVE UMETA(DisplayName = "Active"),
	INACTIVE UMETA(DisplayName = "Inctive"),
};

UENUM(BlueprintType)
enum class EWidgetQuestWindowButtonsType : uint8
{
	NONE UMETA(DisplayName = "None"),
	OK UMETA(DisplayName = "Ok"),
	ACCEPT_DECLINE UMETA(DisplayName = "Accept and Decline"),
};

UENUM(BlueprintType)
enum class EWidgetQuestWindowPlayerResponse : uint8
{
	OK UMETA(DisplayName = "Ok"),
	ACCEPT UMETA(DisplayName = "Accept"),
	DECLINE UMETA(DisplayName = "Decline"),
};

UENUM(BlueprintType)
enum class EWidgetBattleRequestType : uint8
{
	START_BATTLE UMETA(DisplayName = "Start Battle"),
	VICTORY UMETA(DisplayName = "Victory"),
	DEFEAT UMETA(DisplayName = "Defeat"),
};

UENUM(BlueprintType)
enum class EWidgetBattleResponseType : uint8
{
	START_BATTLE_SHOWN UMETA(DisplayName = "Start Battle Popup Is Shown"),
	VICTORY_SHOWN UMETA(DisplayName = "Victory Popup Is Shown"),
	DEFEAT_SHOWN UMETA(DisplayName = "Defeat Popup Is Shown"),
};

UENUM(BlueprintType)
enum class EWidgetFlyingDataType : uint8
{
	POSITIVE_HERBS UMETA(DisplayName = "Positive Herbs"),
	NEGATIVE_DAMAGE UMETA(DisplayName = "Negative Damage"),
};

UENUM(BlueprintType)
enum class EAmbientMusicType : uint8
{
	COMMON_AMBIENT UMETA(DisplayName = "Common Ambient"),
	INBATTLE_AMBIENT UMETA(DisplayName = "Inbattle Ambient"),
	MENU_AMBIENT UMETA(DisplayName = "Menu Ambient")
};

UENUM(BlueprintType)
enum class ECurrentLevel : uint8
{
	PERSISTENT UMETA(DisplayName = "Persistent"),
	MENU UMETA(DisplayName = "Menu"),
	GAMEPLAY UMETA(DisplayName = "Gameplay"),
};


/* ##### USTRUCTs ##### */


USTRUCT(Blueprintable)
struct FEnemyUnitInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector location;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEnemyUnitType type;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_hovered;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool in_main_area;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool in_minor_area;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int hp_current;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int hp_total;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int attack_power_min;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int attack_power_max;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float turn_points;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float attack_price;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float owner_attack_price;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float owner_movement_price;
};

USTRUCT(Blueprintable)
struct FPlayerUnitInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_active;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int hp_current;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int hp_total;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int tps_current;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int tps_total;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int xp_current;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int xp_total;
};

USTRUCT(Blueprintable)
struct FBattleQueueUnitInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBattleQueueUnitIconType icon_type;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float hp_percent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float tps_percent;
};

USTRUCT(Blueprintable)
struct FAdvancedCursorInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool show;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D coords;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString action_cost_str;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool action_is_allowed;
};

USTRUCT(Blueprintable)
struct FBattleTurnPlayerUnitInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool show;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString damage_str;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString range_str;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString cost_str;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString pts_str;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float pts_percent;
};

USTRUCT(Blueprintable)
struct FWidgetQuestWindowInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString title_text;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> description_text;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int reward_xp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int reward_money;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWidgetQuestWindowButtonsType buttons_type;
};

USTRUCT(Blueprintable)
struct FQuestsListInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool show;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool can_be_completed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString quest_title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString quest_aim;
};
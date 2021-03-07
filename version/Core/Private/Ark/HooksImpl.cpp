#include "HooksImpl.h"

#include "ApiUtils.h"
#include "../Commands.h"
#include "../Hooks.h"
#include "../PluginManager/PluginManager.h"
#include "../IBaseApi.h"

#include <Logger/Logger.h>

namespace ArkApi
{
	// Hooks declaration
	DECLARE_HOOK(UEngine_Init, void, DWORD64, DWORD64);
	DECLARE_HOOK(UWorld_InitWorld, void, UWorld*, DWORD64);
	DECLARE_HOOK(UWorld_Tick, void, UWorld*, ELevelTick, float);
	DECLARE_HOOK(AShooterGameMode_InitGame, void, AShooterGameMode*, FString*, FString*, FString*);
	DECLARE_HOOK(AShooterPlayerController_ServerSendChatMessage_Impl, void, AShooterPlayerController*, FString*,
	EChatSendMode::Type);
	DECLARE_HOOK(APlayerController_ConsoleCommand, FString*, APlayerController*, FString*, FString*, bool);
	DECLARE_HOOK(RCONClientConnection_ProcessRCONPacket, void, RCONClientConnection*, RCONPacket*, UWorld*);
	DECLARE_HOOK(AGameState_DefaultTimer, void, AGameState*);
	DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);

	void InitHooks()
	{
		auto& hooks = API::game_api->GetHooks();

		hooks->SetHook("UEngine.Init", &Hook_UEngine_Init, &UEngine_Init_original);
		hooks->SetHook("UWorld.InitWorld", &Hook_UWorld_InitWorld, &UWorld_InitWorld_original);
		hooks->SetHook("UWorld.Tick", &Hook_UWorld_Tick, &UWorld_Tick_original);
		hooks->SetHook("AShooterGameMode.InitGame", &Hook_AShooterGameMode_InitGame,
			&AShooterGameMode_InitGame_original);
		hooks->SetHook("AShooterPlayerController.ServerSendChatMessage_Implementation",
			&Hook_AShooterPlayerController_ServerSendChatMessage_Impl,
			&AShooterPlayerController_ServerSendChatMessage_Impl_original);
		hooks->SetHook("APlayerController.ConsoleCommand", &Hook_APlayerController_ConsoleCommand,
			&APlayerController_ConsoleCommand_original);
		hooks->SetHook("RCONClientConnection.ProcessRCONPacket", &Hook_RCONClientConnection_ProcessRCONPacket,
			&RCONClientConnection_ProcessRCONPacket_original);
		hooks->SetHook("AGameState.DefaultTimer", &Hook_AGameState_DefaultTimer, &AGameState_DefaultTimer_original);
		hooks->SetHook("AShooterGameMode.BeginPlay", &Hook_AShooterGameMode_BeginPlay,
			&AShooterGameMode_BeginPlay_original);

		Log::GetLog()->info("Initialized hooks\n");
	}

	// Hooks

	void Hook_UEngine_Init(DWORD64 engine, DWORD64 in_engine_loop)
	{
		UEngine_Init_original(engine, in_engine_loop);

		Log::GetLog()->info("UGameEngine::Init was called");
		Log::GetLog()->info("Loading plugins..\n");

		API::PluginManager::Get().LoadAllPlugins();

		dynamic_cast<API::IBaseApi&>(*API::game_api).RegisterCommands();
	}

	void Hook_UWorld_InitWorld(UWorld* world, DWORD64 initialization_values)
	{
		Log::GetLog()->info("UWorld::InitWorld was called");

		dynamic_cast<ApiUtils&>(*API::game_api->GetApiUtils()).SetWorld(world);

		UWorld_InitWorld_original(world, initialization_values);
	}

	void Hook_AShooterGameMode_InitGame(AShooterGameMode* shooter_game_mode, FString* map_name, FString* options,
		FString* error_message)
	{
		dynamic_cast<ApiUtils&>(*API::game_api->GetApiUtils()).SetShooterGameMode(shooter_game_mode);

		AShooterGameMode_InitGame_original(shooter_game_mode, map_name, options, error_message);
	}

	void Hook_AShooterPlayerController_ServerSendChatMessage_Impl(
		AShooterPlayerController* player_controller, FString* message, EChatSendMode::Type mode)
	{
		const long double last_chat_time = player_controller->LastChatMessageTimeField();
		const long double now_time = ArkApi::GetApiUtils().GetWorld()->TimeSecondsField();

		const auto spam_check = now_time - last_chat_time < 1.0;
		if (last_chat_time > 0 && spam_check) {
			return;
		}

		player_controller->LastChatMessageTimeField() = now_time;

		const auto command_executed = dynamic_cast<ArkApi::Commands&>(*API::game_api->GetCommands()).
			CheckChatCommands(player_controller, message, mode);

		const auto prevent_default = dynamic_cast<ArkApi::Commands&>(*API::game_api->GetCommands()).
			CheckOnChatMessageCallbacks(player_controller, message, mode, spam_check, command_executed);

		if (command_executed || prevent_default) {
			return;
		}

		AShooterPlayerController_ServerSendChatMessage_Impl_original(player_controller, message, mode);
	}

	FString* Hook_APlayerController_ConsoleCommand(APlayerController* player_controller, FString* result,
		FString* cmd, bool write_to_log)
	{
		dynamic_cast<Commands&>(*API::game_api->GetCommands()).CheckConsoleCommands(
			player_controller, cmd, write_to_log);

		return APlayerController_ConsoleCommand_original(player_controller, result, cmd, write_to_log);
	}

	void Hook_RCONClientConnection_ProcessRCONPacket(RCONClientConnection* connection, RCONPacket* packet,
		UWorld* in_world)
	{
		if (connection->IsAuthenticatedField()) {
			dynamic_cast<Commands&>(*API::game_api->GetCommands()).CheckRconCommands(connection, packet, in_world);
		}

		RCONClientConnection_ProcessRCONPacket_original(connection, packet, in_world);
	}

	//void __fastcall UWorld::Tick(UWorld *this, ELevelTick TickType, float DeltaSeconds)
	void Hook_UWorld_Tick(UWorld* world, ELevelTick tick_type, float delta_seconds)
	{
		//dynamic_cast<Commands&>(*API::game_api->GetCommands()).CheckOnTickCallbacks(delta_seconds);
		Commands* command = dynamic_cast<Commands*>(API::game_api->GetCommands().get());
		if (command)
		{
			command->CheckOnTickCallbacks(delta_seconds);
		}
		
		UWorld_Tick_original(world, tick_type, delta_seconds);
	}

	void Hook_AGameState_DefaultTimer(AGameState* game_state)
	{
		//dynamic_cast<Commands&>(*API::game_api->GetCommands()).CheckOnTimerCallbacks();
		Commands* command = dynamic_cast<Commands*>(API::game_api->GetCommands().get());
		if (command)
		{
			command->CheckOnTimerCallbacks();
		}

		AGameState_DefaultTimer_original(game_state);
	}

	void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* shooter_game_mode)
	{
		AShooterGameMode_BeginPlay_original(shooter_game_mode);

		dynamic_cast<ApiUtils&>(*API::game_api->GetApiUtils()).SetStatus(ServerStatus::Ready);
	}
} // namespace ArkApi

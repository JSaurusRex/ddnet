/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include "DDRace.h"

#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#define GAME_TYPE_NAME "COTD"
#define TEST_TYPE_NAME "COTD"

CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;
	m_GameFlags = protocol7::GAMEFLAG_RACE;
}

CGameControllerDDRace::~CGameControllerDDRace() = default;

CScore *CGameControllerDDRace::Score()
{
	return GameServer()->Score();
}

void CGameControllerDDRace::KO_Start()
{
	bool finished = (GameServer()->ko_player_count <= 1);
	if(finished)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameServer()->PlayerExists(i))
				continue;
			
			if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
				continue;

			char aBuf[256];
			if(GameServer()->m_apPlayers[i]->m_player_eliminated)
			{
				str_format(aBuf, sizeof(aBuf), "you survived until round %i!", GameServer()->m_apPlayers[i]->m_ko_round);
				GameServer()->SendChatTarget(i, aBuf);
				continue;
			}

			str_format(aBuf, sizeof(aBuf), "%s wins!", Server()->ClientName(i));
			GameServer()->SendBroadcast(aBuf, -1, true);
		}
		m_Warmup = 10 * Server()->TickSpeed();
		GameServer()->ko_game = false;
		GameServer()->ko_round = 0;
		return;
	}

	GameServer()->ko_round++;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "starting round %i!", GameServer()->ko_round);
	GameServer()->SendChat(-1, TEAM_ALL, aBuf);
	
	GameServer()->ko_player_count = 0;
	GameServer()->ko_players_finished = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameServer()->PlayerExists(i))
			continue;
		
		if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
			continue;
				
		if(GameServer()->GetPlayerChar(i))
		{
			GameServer()->m_apPlayers[i]->KillCharacter();
		}

		if(GameServer()->m_apPlayers[i]->m_player_eliminated)
		{
			continue;
		}

		GameServer()->m_apPlayers[i]->TryRespawn();
		
		CCharacter * pChar = GameServer()->GetPlayerChar(i);

		if(!pChar)
			continue; //spawning failed
		
		GameServer()->ko_player_count++;
		

		pChar->SetSolo(true);
		pChar->ResetPickups();
		pChar->ResetInput();
		

		pChar->m_StartTime = Server()->Tick();
		pChar->m_DDRaceState = DDRACE_STARTED;

		pChar->m_LastTimeCp = -1;
		pChar->m_LastTimeCpBroadcasted = -1;
		for(float &CurrentTimeCp : pChar->m_aCurrentTimeCp)
		{
			CurrentTimeCp = 0.0f;
		}
	}

	if(GameServer()->ko_player_count <= 1)
		KO_Start();
	
	GameServer()->ko_players_tobe_eliminated = g_Config.m_SvKoEliminations;

	// if(GameServer()->ko_player_count > 20)
	// 	GameServer()->ko_players_tobe_eliminated = 8;
	
	// if(GameServer()->ko_player_count > 10)
	// 	GameServer()->ko_players_tobe_eliminated = 4;
	
	// if(GameServer()->ko_player_count > 5)
	// 	GameServer()->ko_players_tobe_eliminated = 2;

	m_Timer = m_Time;
}

void CGameControllerDDRace::HandleCharacterTiles(CCharacter *pChr, int MapIndex)
{
	CPlayer *pPlayer = pChr->GetPlayer();
	const int ClientId = pPlayer->GetCid();

	int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	//Sensitivity
	int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	int FTile1 = GameServer()->Collision()->GetFTileIndex(S1);
	int FTile2 = GameServer()->Collision()->GetFTileIndex(S2);
	int FTile3 = GameServer()->Collision()->GetFTileIndex(S3);
	int FTile4 = GameServer()->Collision()->GetFTileIndex(S4);

	const int PlayerDDRaceState = pChr->m_DDRaceState;
	bool IsOnStartTile = (TileIndex == TILE_START) || (TileFIndex == TILE_START) || FTile1 == TILE_START || FTile2 == TILE_START || FTile3 == TILE_START || FTile4 == TILE_START || Tile1 == TILE_START || Tile2 == TILE_START || Tile3 == TILE_START || Tile4 == TILE_START;
	// start
	if(IsOnStartTile && PlayerDDRaceState != DDRACE_CHEAT)
	{
		const int Team = GameServer()->GetDDRaceTeam(ClientId);
		if(Teams().GetSaving(Team))
		{
			GameServer()->SendStartWarning(ClientId, "You can't start while loading/saving of team is in progress");
			pChr->Die(ClientId, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam == SV_TEAM_MANDATORY && (Team == TEAM_FLOCK || Teams().Count(Team) <= 1))
		{
			GameServer()->SendStartWarning(ClientId, "You have to be in a team with other tees to start");
			pChr->Die(ClientId, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team > TEAM_FLOCK && Team < TEAM_SUPER && Teams().Count(Team) < g_Config.m_SvMinTeamSize && !Teams().TeamFlock(Team))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Your team has fewer than %d players, so your team rank won't count", g_Config.m_SvMinTeamSize);
			GameServer()->SendStartWarning(ClientId, aBuf);
		}
		if(g_Config.m_SvResetPickups)
		{
			pChr->ResetPickups();
		}

		Teams().OnCharacterStart(ClientId);
		pChr->m_LastTimeCp = -1;
		pChr->m_LastTimeCpBroadcasted = -1;
		for(float &CurrentTimeCp : pChr->m_aCurrentTimeCp)
		{
			CurrentTimeCp = 0.0f;
		}
	}

	// finish
	if(((TileIndex == TILE_FINISH) || (TileFIndex == TILE_FINISH) || FTile1 == TILE_FINISH || FTile2 == TILE_FINISH || FTile3 == TILE_FINISH || FTile4 == TILE_FINISH || Tile1 == TILE_FINISH || Tile2 == TILE_FINISH || Tile3 == TILE_FINISH || Tile4 == TILE_FINISH) && PlayerDDRaceState == DDRACE_STARTED)
		Teams().OnCharacterFinish(ClientId);
	
	if(((TileIndex == TILE_FINISH) || (TileFIndex == TILE_FINISH) || FTile1 == TILE_FINISH || FTile2 == TILE_FINISH || FTile3 == TILE_FINISH || FTile4 == TILE_FINISH || Tile1 == TILE_FINISH || Tile2 == TILE_FINISH || Tile3 == TILE_FINISH || Tile4 == TILE_FINISH) && GameServer()->ko_game)
	{
		char aBuf[256];
		GameServer()->ko_players_finished++;
		if(GameServer()->ko_players_finished == GameServer()->ko_player_count)
		{
			pPlayer->m_player_eliminated = true;
			// str_format(aBuf, sizeof(aBuf), "%s is eliminated!", Server()->ClientName(pPlayer->GetCid()));
			// GameServer()->SendBroadcast(aBuf, -1, true);
			// GameServer()->SendChat(-1, TEAM_ALL, aBuf);
		}

		pPlayer->KillCharacter(WEAPON_GAME);

		if(GameServer()->ko_player_count <= 2 && GameServer()->ko_players_finished == 1)
		{			
			str_format(aBuf, sizeof(aBuf), "%s Wins!", Server()->ClientName(pPlayer->GetCid()));
			GameServer()->SendBroadcast(aBuf, -1, true);
			GameServer()->SendChat(-1, TEAM_ALL, aBuf);

			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(!GameServer()->PlayerExists(i) || i == pPlayer->GetCid())
				{
					continue;
				}
				
				GameServer()->m_apPlayers[i]->KillCharacter(WEAPON_WORLD);
				if(GameServer()->m_apPlayers[i]->m_player_eliminated)
				{
					str_format(aBuf, sizeof(aBuf), "you survived until round %i!", GameServer()->m_apPlayers[i]->m_ko_round);
					GameServer()->SendChatTarget(i, aBuf);
					continue;
				}
			}

			GameServer()->ko_game = false;
		}
	}

	// unlock team
	else if(((TileIndex == TILE_UNLOCK_TEAM) || (TileFIndex == TILE_UNLOCK_TEAM)) && Teams().TeamLocked(GameServer()->GetDDRaceTeam(ClientId)))
	{
		Teams().SetTeamLock(GameServer()->GetDDRaceTeam(ClientId), false);
		GameServer()->SendChatTeam(GameServer()->GetDDRaceTeam(ClientId), "Your team was unlocked by an unlock team tile");
	}

	// solo part
	// if(((TileIndex == TILE_SOLO_ENABLE) || (TileFIndex == TILE_SOLO_ENABLE)) && !Teams().m_Core.GetSolo(ClientId))
	// {
	// 	GameServer()->SendChatTarget(ClientId, "You are now in a solo part");
	// 	pChr->SetSolo(true);
	// }
	// else if(((TileIndex == TILE_SOLO_DISABLE) || (TileFIndex == TILE_SOLO_DISABLE)) && Teams().m_Core.GetSolo(ClientId))
	// {
	// 	GameServer()->SendChatTarget(ClientId, "You are now out of the solo part");
	// 	pChr->SetSolo(false);
	// }
}

void CGameControllerDDRace::SetArmorProgress(CCharacter *pCharacer, int Progress)
{
	pCharacer->SetArmor(clamp(10 - (Progress / 15), 0, 10));
}

void CGameControllerDDRace::OnPlayerConnect(CPlayer *pPlayer)
{
	IGameController::OnPlayerConnect(pPlayer);
	int ClientId = pPlayer->GetCid();

	// init the player
	Score()->PlayerData(ClientId)->Reset();

	// Can't set score here as LoadScore() is threaded, run it in
	// LoadScoreThreaded() instead
	Score()->LoadPlayerData(ClientId);

	if(!Server()->ClientPrevIngame(ClientId))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientId), GetTeamName(pPlayer->GetTeam()));
		GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1, CGameContext::FLAG_SIX);

		GameServer()->SendChatTarget(ClientId, "DDraceNetwork Mod. Version: " GAME_VERSION);
		GameServer()->SendChatTarget(ClientId, "please visit DDNet.org or say /info and make sure to read our /rules");
	}
}

void CGameControllerDDRace::OnPlayerDisconnect(CPlayer *pPlayer, const char *pReason)
{
	int ClientId = pPlayer->GetCid();
	bool WasModerator = pPlayer->m_Moderating && Server()->ClientIngame(ClientId);

	IGameController::OnPlayerDisconnect(pPlayer, pReason);

	if(!GameServer()->PlayerModerating() && WasModerator)
		GameServer()->SendChat(-1, TEAM_ALL, "Server kick/spec votes are no longer actively moderated.");

	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO)
		Teams().SetForceCharacterTeam(ClientId, TEAM_FLOCK);

	for(int Team = TEAM_FLOCK + 1; Team < TEAM_SUPER; Team++)
		if(Teams().IsInvited(Team, ClientId))
			Teams().SetClientInvited(Team, ClientId, false);
}

void CGameControllerDDRace::OnReset()
{
	IGameController::OnReset();
	Teams().Reset();
}

void CGameControllerDDRace::Tick()
{
	if(m_Warmup <= 0 && GameServer()->ko_game)
	{
		m_Timer--;
		if(m_Timer < 10*Server()->TickSpeed() && m_Timer % Server()->TickSpeed() == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%i", m_Timer/Server()->TickSpeed());
			GameServer()->SendChat(-1, TEAM_ALL, aBuf);
			GameServer()->SendBroadcast(aBuf, -1);
		}
	}
	
	int playersIn = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameServer()->PlayerExists(i) || GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
			continue;

		if(GameServer()->m_apPlayers[i]->m_player_eliminated || !GameServer()->m_apPlayers[i]->GetCharacter())
		{
			continue;
		}

		playersIn++;
	}

	if(playersIn <= GameServer()->ko_players_tobe_eliminated && m_Warmup <= 0 && GameServer()->ko_game)
	{
		printf("everybody alive gets eliminated\n");
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameServer()->PlayerExists(i) || GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
				continue;

			if(GameServer()->m_apPlayers[i]->m_player_eliminated || !GameServer()->m_apPlayers[i]->GetCharacter())
			{
				continue;
			}

			GameServer()->m_apPlayers[i]->m_player_eliminated = true;
		}

		if(playersIn < 1)
		{
			m_Warmup = 10 * Server()->TickSpeed();
			m_Timer = 1;
		}
	}
	
	if(m_Timer == 0 && GameServer()->ko_game)
	{
		//times up
		GameServer()->SendChat(-1, TEAM_ALL, "Time is up");
		GameServer()->SendBroadcast("Time is up!", -1);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameServer()->PlayerExists(i) || !GameServer()->GetPlayerChar(i) || GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
				continue;
			
			GameServer()->m_apPlayers[i]->m_player_eliminated = true;
			GameServer()->m_apPlayers[i]->KillCharacter();
		}

		m_Timer = -1;
	}

	IGameController::Tick();
	Teams().ProcessSaveTeam();
	Teams().Tick();
}

void CGameControllerDDRace::DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	CCharacter *pCharacter = pPlayer->GetCharacter();

	if(Team == TEAM_SPECTATORS)
	{
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && pCharacter)
		{
			// Joining spectators should not kill a locked team, but should still
			// check if the team finished by you leaving it.
			int DDRTeam = pCharacter->Team();
			Teams().SetForceCharacterTeam(pPlayer->GetCid(), TEAM_FLOCK);
			Teams().CheckTeamFinished(DDRTeam);
		}
	}

	IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);
}

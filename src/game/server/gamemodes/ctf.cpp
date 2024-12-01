#include "ctf.h"

#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

CGameControllerCTF::CGameControllerCTF(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;
	m_GameFlags_v7 = protocol7::GAMEFLAG_TEAMS | protocol7::GAMEFLAG_FLAGS;

	m_apFlags[0] = 0;
	m_apFlags[1] = 0;

	m_pGameType = "CTF+";
}

CGameControllerCTF::~CGameControllerCTF() = default;

void CGameControllerCTF::Tick()
{
	CGameControllerInstagib::Tick();

	FlagTick(); // ddnet-insta
}

void CGameControllerCTF::OnCharacterSpawn(class CCharacter *pChr)
{
	OnCharacterConstruct(pChr);

	pChr->SetTeams(&Teams());
	Teams().OnCharacterSpawn(pChr->GetPlayer()->GetCid());

	// default health
	pChr->IncreaseHealth(10);

	pChr->ResetPickups();

	pChr->GiveWeapon(WEAPON_GUN, false, 10);
	pChr->GiveWeapon(WEAPON_HAMMER);
	pChr->SetActiveWeapon(WEAPON_GUN);
}

bool CGameControllerCTF::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	if(Character.m_IsGodmode)
	{
		Dmg = 0;
		return false;
	}
	// TODO: ddnet-insta cfg team damage
	// if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCid(), From) && !g_Config.m_SvTeamdamage)
	if(GameServer()->m_pController->IsFriendlyFire(Character.GetPlayer()->GetCid(), From))
		return false;

	if(g_Config.m_SvOnlyHookKills && From >= 0 && From <= MAX_CLIENTS)
	{
		CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
		if(!pChr || pChr->GetCore().HookedPlayer() != Character.GetPlayer()->GetCid())
			Dmg = 0;
	}

	if(WEAPON_LASER == Weapon)
	{
		Dmg = 5;
	}

	int damage = Dmg;

	if(From == Character.GetPlayer()->GetCid())
		damage = std::max(1, damage/2);
	
	Character.m_DamageTaken++;

	// create healthmod indicator
	if(Server()->Tick() < Character.m_DamageTakenTick+25)
	{
		// make sure that the damage indicators doesn't group together
		GameServer()->CreateDamageInd(Character.m_Pos, Character.m_DamageTaken*0.25f, Dmg);
	}
	else
	{
		Character.m_DamageTaken = 0;
		GameServer()->CreateDamageInd(Character.m_Pos, 0, damage);
	}
	
	if(damage)
	{
		if(Character.m_Armor)
		{
			if(damage > 1)
			{
				Character.m_Health--;
				damage--;
			}

			if(damage > Character.m_Armor)
			{
				damage -= Character.m_Armor;
				Character.m_Armor = 0;
			}
			else
			{
				Character.m_Armor -= damage;
				damage = 0;
			}
		}

		Character.m_Health -= damage;
	}

	Character.m_DamageTakenTick = Server()->Tick();

	if(From >= 0 && From != Character.GetPlayer()->GetCid() && GameServer()->m_apPlayers[From])
	{
		// do damage Hit sound
		CClientMask Mask = CClientMask().set(From);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorId == From)
				Mask.set(i);
			
			// if(gaem)
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
	}

	// check for death
	if(Character.m_Health <= 0)
	{
		Character.Die(From, Weapon);

		if(From >= 0 && From != Character.GetPlayer()->GetCid() && GameServer()->m_apPlayers[From])
		{
			CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
			if(pChr)
			{
				// set attacker's face to happy (taunt!)
				pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());

				// refill nades
				int RefillNades = 0;
				if(g_Config.m_SvGrenadeAmmoRegenOnKill == 1)
					RefillNades = 1;
				else if(g_Config.m_SvGrenadeAmmoRegenOnKill == 2)
					RefillNades = g_Config.m_SvGrenadeAmmoRegenNum;
				if(RefillNades && g_Config.m_SvGrenadeAmmoRegen && Weapon == WEAPON_GRENADE)
				{
					pChr->SetWeaponAmmo(WEAPON_GRENADE, minimum(pChr->GetCore().m_aWeapons[WEAPON_GRENADE].m_Ammo + RefillNades, g_Config.m_SvGrenadeAmmoRegenNum));
				}
			}

			// do damage Hit sound
			CClientMask Mask = CClientMask().set(From);
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorId == From)
					Mask.set(i);
			}
			GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
		}
		return false;
	}

	if (damage > 2)
		GameServer()->CreateSound(Character.GetPos(), SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(Character.GetPos(), SOUND_PLAYER_PAIN_SHORT);

	if(damage)
	{
		Character.SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);
	}

	return false;
}

int CGameControllerCTF::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponId);
	int HadFlag = 0;

	if(WeaponId == WEAPON_SELF)
	{
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * 3.0f;
	}
	else
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * 0.5f;

	// drop flags
	for(auto &F : m_apFlags)
	{
		if(F && pKiller && pKiller->GetCharacter() && F->GetCarrier() == pKiller->GetCharacter())
			HadFlag |= 2;
		if(F && F->GetCarrier() == pVictim)
		{
			GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
			GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_DROP, -1);
			/*pVictim->GetPlayer()->m_Rainbow = false;
			pVictim->GetPlayer()->m_TeeInfos.m_ColorBody = pVictim->GetPlayer()->m_ColorBodyOld;
			pVictim->GetPlayer()->m_TeeInfos.m_ColorFeet = pVictim->GetPlayer()->m_ColorFeetOld;*/
			F->m_DropTick = Server()->Tick();
			F->SetCarrier(0);
			F->m_Vel = vec2(0, 0);

			if(pKiller)
				pKiller->IncrementScore();

			HadFlag |= 1;
		}
		if(F && F->GetCarrier() == pVictim)
			F->SetCarrier(0);
	}

	return HadFlag;
}

bool CGameControllerCTF::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerInstagib::OnEntity(Index, x, y, Layer, Flags, Initial, Number);

	const vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
	int Team = -1;
	if(Index == ENTITY_FLAGSTAND_RED)
		Team = TEAM_RED;
	if(Index == ENTITY_FLAGSTAND_BLUE)
		Team = TEAM_BLUE;
	if(Team == -1 || m_apFlags[Team])
		return false;

	CFlag *F = new CFlag(&GameServer()->m_World, Team);
	F->m_StandPos = Pos;
	F->m_Pos = Pos;
	m_apFlags[Team] = F;
	GameServer()->m_World.InsertEntity(F);
	return true;
}

void CGameControllerCTF::OnFlagReturn(CFlag *pFlag)
{
	CGameControllerInstagib::OnFlagReturn(pFlag);

	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
	GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_RETURN, -1);
	GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
}

void CGameControllerCTF::OnFlagGrab(class CFlag *pFlag)
{
	if(!g_Config.m_SvFastcap)
		return;
	if(!pFlag)
		return;
	if(!pFlag->IsAtStand())
		return;
	if(!pFlag->m_pCarrier)
		return;

	Teams().OnCharacterStart(pFlag->m_pCarrier->GetPlayer()->GetCid());
}

void CGameControllerCTF::OnFlagCapture(class CFlag *pFlag, float Time)
{
	if(!g_Config.m_SvFastcap)
		return;
	if(!pFlag)
		return;
	if(!pFlag->m_pCarrier)
		return;

	Teams().OnCharacterFinish(pFlag->m_pCarrier->GetPlayer()->GetCid());
}

void CGameControllerCTF::FlagTick()
{
	if(GameServer()->m_World.m_ResetRequested || GameServer()->m_World.m_Paused)
		return;

	for(int Fi = 0; Fi < 2; Fi++)
	{
		CFlag *F = m_apFlags[Fi];

		if(!F)
			continue;

		//
		if(F->GetCarrier())
		{
			if(m_apFlags[Fi ^ 1] && m_apFlags[Fi ^ 1]->IsAtStand())
			{
				if(distance(F->GetPos(), m_apFlags[Fi ^ 1]->GetPos()) < CFlag::ms_PhysSize + CCharacterCore::PhysicalSize())
				{
					// CAPTURE! \o/
					m_aTeamscore[Fi ^ 1] += 100;
					F->GetCarrier()->GetPlayer()->AddScore(5);
					float Diff = Server()->Tick() - F->GetGrabTick();

					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "flag_capture player='%d:%s' team=%d time=%.2f",
						F->GetCarrier()->GetPlayer()->GetCid(),
						Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCid()),
						F->GetCarrier()->GetPlayer()->GetTeam(),
						Diff / (float)Server()->TickSpeed());
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

					float CaptureTime = Diff / (float)Server()->TickSpeed();
					if(CaptureTime <= 60)
						str_format(aBuf,
							sizeof(aBuf),
							"The %s flag was captured by '%s' (%d.%s%d seconds)", Fi ? "blue" : "red",
							Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCid()), (int)CaptureTime % 60, ((int)(CaptureTime * 100) % 100) < 10 ? "0" : "", (int)(CaptureTime * 100) % 100);
					else
						str_format(
							aBuf,
							sizeof(aBuf),
							"The %s flag was captured by '%s'", Fi ? "blue" : "red",
							Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCid()));
					for(auto &pPlayer : GameServer()->m_apPlayers)
					{
						if(!pPlayer)
							continue;
						if(Server()->IsSixup(pPlayer->GetCid()))
							continue;

						GameServer()->SendChatTarget(pPlayer->GetCid(), aBuf);
					}
					GameServer()->m_pController->OnFlagCapture(F, Diff);
					GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_CAPTURE, Fi, F->GetCarrier()->GetPlayer()->GetCid(), Diff, -1);
					GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
					for(int i = 0; i < 2; i++)
						for(auto &pFlag : m_apFlags)
							pFlag->Reset();
					// do a win check(capture could trigger win condition)
					if(DoWincheckMatch())
						return;
				}
			}
		}
		else
		{
			CCharacter *apCloseCCharacters[MAX_CLIENTS];
			int Num = GameServer()->m_World.FindEntities(F->GetPos(), CFlag::ms_PhysSize, (CEntity **)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Num; i++)
			{
				if(!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->GetPos(), apCloseCCharacters[i]->GetPos(), NULL, NULL))
					continue;

				if(apCloseCCharacters[i]->GetPlayer()->GetTeam() == F->GetTeam())
				{
					// return the flag
					if(!F->IsAtStand())
					{
						CCharacter *pChr = apCloseCCharacters[i];
						pChr->GetPlayer()->IncrementScore();

						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "flag_return player='%d:%s' team=%d",
							pChr->GetPlayer()->GetCid(),
							Server()->ClientName(pChr->GetPlayer()->GetCid()),
							pChr->GetPlayer()->GetTeam());
						GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
						GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_RETURN, -1);
						GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
						F->Reset();
					}
				}
				else
				{
					// take the flag
					if(F->IsAtStand())
						m_aTeamscore[Fi ^ 1]++;

					F->Grab(apCloseCCharacters[i]);

					F->GetCarrier()->GetPlayer()->IncrementScore();

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "flag_grab player='%d:%s' team=%d",
						F->GetCarrier()->GetPlayer()->GetCid(),
						Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCid()),
						F->GetCarrier()->GetPlayer()->GetTeam());
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
					GameServer()->SendGameMsg(protocol7::GAMEMSG_CTF_GRAB, Fi, -1);
					break;
				}
			}
		}
	}
	DoWincheckMatch();
}

void CGameControllerCTF::Snap(int SnappingClient)
{
	CGameControllerInstagib::Snap(SnappingClient);

	int FlagCarrierRed = FLAG_MISSING;
	if(m_apFlags[TEAM_RED])
	{
		if(m_apFlags[TEAM_RED]->m_AtStand)
			FlagCarrierRed = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
			FlagCarrierRed = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCid();
		else
			FlagCarrierRed = FLAG_TAKEN;
	}

	int FlagCarrierBlue = FLAG_MISSING;
	if(m_apFlags[TEAM_BLUE])
	{
		if(m_apFlags[TEAM_BLUE]->m_AtStand)
			FlagCarrierBlue = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
			FlagCarrierBlue = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCid();
		else
			FlagCarrierBlue = FLAG_TAKEN;
	}

	if(Server()->IsSixup(SnappingClient))
	{
		protocol7::CNetObj_GameDataFlag *pGameDataObj = Server()->SnapNewItem<protocol7::CNetObj_GameDataFlag>(0);
		if(!pGameDataObj)
			return;

		pGameDataObj->m_FlagCarrierRed = FlagCarrierRed;
		pGameDataObj->m_FlagCarrierBlue = FlagCarrierBlue;
	}
	else
	{
		CNetObj_GameData *pGameDataObj = Server()->SnapNewItem<CNetObj_GameData>(0);
		if(!pGameDataObj)
			return;

		pGameDataObj->m_FlagCarrierRed = FlagCarrierRed;
		pGameDataObj->m_FlagCarrierBlue = FlagCarrierBlue;

		pGameDataObj->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
		pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];
	}
}

bool CGameControllerCTF::DoWincheckMatch()
{
	CGameControllerInstagib::DoWincheckMatch();

	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		if(m_SuddenDeath)
		{
			if(m_aTeamscore[TEAM_RED] / 100 != m_aTeamscore[TEAM_BLUE] / 100)
			{
				EndMatch();
				return true;
			}
		}
		else
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE])
			{
				EndMatch();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	return false;
}

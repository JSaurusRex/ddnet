#include <game/server/entities/character.h>
#include <game/server/player.h>

#include "dm.h"

CGameControllerDM::CGameControllerDM(class CGameContext *pGameServer) :
	CGameControllerCTF(pGameServer)
{
	m_GameFlags = 0;
	m_GameFlags_v7 = 0;

	m_pGameType = "DM";
}

CGameControllerDM::~CGameControllerDM() = default;

void CGameControllerDM::Tick()
{
	CGameControllerInstagib::Tick();
}

void CGameControllerDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerCTF::OnCharacterSpawn(pChr);
}

bool CGameControllerDM::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	return CGameControllerInstagib::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
}

bool CGameControllerDM::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	return CGameControllerCTF::OnCharacterTakeDamage(Force, Dmg, From, Weapon, Character);
}


int CGameControllerDM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponId);

	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && pKiller->m_Score.value_or(0) >= m_GameInfo.m_ScoreLimit) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		EndMatch();
		return true;
	}
	return false;
}

#include <gtest/gtest.h>

#include <base/detect.h>
#include <engine/external/json-parser/json.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/demo.h>
#include <engine/shared/snapshot.h>
#include <game/gamecore.h>

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <engine/serverbrowser.h>
#include <game/client/gameclient.h>
#include <engine/client.h>
#include <engine/shared/map.h>
#include <engine/kernel.h>

#include <vector>

#include <cstdlib>

void RegisterGameUuids(CUuidManager *pManager);

void print_CharacterCore(CNetObj_CharacterCore * obj)
{
	// printf("msg_Id: %i\n", obj->ms_MsgId);
	printf("X: %i\n", obj->m_X);
	printf("Y: %i\n", obj->m_Y);
	printf("VelX: %i\n", obj->m_VelX);
	printf("VelY: %i\n", obj->m_VelY);
	printf("Tick: %i\n", obj->m_Tick);
	// printf("Angle: %i\n", obj->m_Angle);
	// printf("Direction: %i\n", obj->m_Direction);
	// printf("hookDx: %i\n", obj->m_HookDx);
	// printf("hookDy: %i\n", obj->m_HookDy);
	printf("HookedPlayer: %i\n", obj->m_HookedPlayer);
	printf("HookState: %i\n", obj->m_HookState);
	// printf("HookX: %i\n", obj->m_HookX);
	// printf("HookY: %i\n", obj->m_HookY);
	// printf("Jumped: %i\n", obj->m_Jumped);
	printf("hookTick: %i\n", obj->m_HookTick);
}

bool compareCNet_CharacterCore(CNetObj_CharacterCore * a, CNetObj_CharacterCore * b)
{
	if(a->m_X != b->m_X)
		return false;
	if(a->m_Y != b->m_Y)
		return false;
	if(a->m_VelX != b->m_VelX)
		return false;
	if(a->m_VelY != b->m_VelY)
		return false;
	
	return true;
}

class PhysicsChecker : public ::testing::Test, public CDemoPlayer::IListener
{
public:
	CSnapshotDelta m_SnapshotDelta;
	CDemoPlayer m_DemoPlayer;
	CConfig m_Config;
	CTuningParams m_Tuning;

	CGameWorld m_GameWorld;
	CGameInfo m_GameInfo;
	CMap m_Map;

	CLayers m_Layers;
	CCollision m_Collision;
	CTuningParams m_aTuningList[256];

	IKernel *m_pKernel;

	int m_Tick = 0;

	bool fail = false;

	CCharacter * players[MAX_CLIENTS] = {0};

	PhysicsChecker() : m_SnapshotDelta(), m_DemoPlayer(&m_SnapshotDelta, false), m_Tuning(), m_GameWorld()
	{
	}

	bool Start(const char * file)
	{
		printf("eyoooo\n");

		m_pKernel = IKernel::Create();
		fail = false;

		printf("eyoo 1.1\n");
		// m_demoplayer.
		m_DemoPlayer.SetListener(this);
		printf("scooby scooby doo\n");
		const char * argv = "./";
		IStorage * pStorage = CreateStorage(IStorage::STORAGETYPE_BASIC, 1, &argv);
		m_pKernel->RegisterInterface(pStorage);
		printf("roogy roogy\n");
		int error = m_DemoPlayer.Load(pStorage, nullptr, file, IStorage::TYPE_ALL_OR_ABSOLUTE);

		if(error < 0)
		{
			printf("demoplayer error %i\n", error);
			return false;
		}

		printf("eyoo 1.2\n");
		// load map
		{
			const CMapInfo *pMapInfo = m_DemoPlayer.GetMapInfo();
			// int Crc = pMapInfo->m_Crc;
			// SHA256_DIGEST Sha = pMapInfo->m_Sha256;

			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapInfo->m_aName);

			printf("%s\n", aBuf);

			m_pKernel->RegisterInterface(static_cast<IMap *>(&m_Map), false);
			
			if(!m_Map.Load(aBuf))
			{
				printf("no map\n");
				return false;
			}
		}

		printf("eyoo 2\n");

		//load collision & layers
		{
			//load layers
			m_Layers.Init(m_pKernel);

			m_Collision.Init(&m_Layers);

			m_GameWorld.m_pCollision = &m_Collision;
			m_GameWorld.m_pTuningList = m_aTuningList;

			m_GameWorld.m_Core.InitSwitchers(m_Collision.m_HighestSwitchNumber);
		}

		//todo load map settings

		printf("eyoo 3\n");

		CNetBase::Init();
		m_DemoPlayer.Play();

		printf("m_DemoPlayer.IsPlaying() %i\n", m_DemoPlayer.IsPlaying());
		const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();


		while(m_DemoPlayer.IsPlaying())
		{
			m_DemoPlayer.Update(false);
			if(pInfo->m_Info.m_Paused)
				break;
		}

		printf("error demo: %s\n", m_DemoPlayer.ErrorMessage());

		m_DemoPlayer.Stop();

		printf("Fail %i\n", fail);
		return !fail;
	}

	bool maybeAddNewPlayer(int id, CNetObj_Character * newCNet, CNetObj_DDNetCharacter * newDDNet)
	{
		if(players[id])
			return false;
		
		CNetObj_Character tmp = {0};
		if(!newCNet)
		{
			newCNet = &tmp;
		}
		
		printf("new player %i\n", id);
		//spawn in new player
		CCharacter * pChar = new CCharacter(&m_GameWorld, id, newCNet, newDDNet);
		
		m_GameWorld.InsertEntity(pChar);
		players[id] = pChar;

		return true;
	}

	bool firstTime = true;

	void OnDemoPlayerSnapshot(void *pData, int Size) override
	{
		CSnapshot * newSnapshot = (CSnapshot*)pData;

		int num = newSnapshot->NumItems();

		int prevTick = m_Tick;
		m_Tick = m_DemoPlayer.Info()->m_Info.m_CurrentTick;

		printf("\n\nnew snapshot: %i\n", m_Tick);

		if(!firstTime)
		{
			//run simulation
			for(int tick = prevTick; tick < m_Tick; tick++)
			{
				m_GameWorld.m_GameTick = tick;
				printf("\tworld tick %i m_Tick %i\n", tick, m_Tick);
				m_GameWorld.m_Core.m_aTuning[0] = m_Tuning;
				m_GameWorld.Tick();
			}
		}else
		{
			m_GameWorld.m_GameTick = m_Tick;
		}
		firstTime = false;


		for(int i = 0; i < num; i++)
		{
			const CSnapshotItem * snapItem = newSnapshot->GetItem(i);
			int size = newSnapshot->GetItemSize(i);
			const int type = newSnapshot->GetItemType(i);

			printf("snapshot item: type %i size %i id %i\n", type, size, snapItem->Id());

			if(type == NETOBJTYPE_PLAYERINPUT)
			{
				printf("\n\n\n\n\nholy shit its player input\n\n\n\n");

				CNetObj_PlayerInput * input = (CNetObj_PlayerInput*) snapItem->Data();
				int id = snapItem->Id();
				maybeAddNewPlayer(id, nullptr, nullptr);
				players[id]->SetInput(input);
				players[id]->OnDirectInput(input);
				players[id]->OnPredictedInput(input);
			}

			if(type == NETOBJTYPE_CHARACTERCORE)
			{
				printf("\n\n charactercore!!!\n");
				int id = snapItem->Id();
				CNetObj_CharacterCore * cNetCore = (CNetObj_CharacterCore*) snapItem->Data();
				maybeAddNewPlayer(id, nullptr, nullptr);
				players[id]->m_Core.Read(cNetCore);
			}

			if(type == NETOBJTYPE_DDNETCHARACTER)
			{
				if(snapItem->Id() < MAX_CLIENTS)
				{
					int id = snapItem->Id();
					CNetObj_DDNetCharacter * newCNet = (CNetObj_DDNetCharacter *) snapItem->Data();
					maybeAddNewPlayer(id, nullptr, newCNet);
					players[id]->m_Core.ReadDDNet(newCNet);

					printf("NETOBJTYPE_DDNETCHARACTER\n");
				}
			}

			if(type == NETOBJTYPE_CHARACTER)
			{
				if(snapItem->Id() < MAX_CLIENTS)
				{
					CNetObj_Character * newCNet = (CNetObj_Character *) snapItem->Data();

					printf("weapon %i\n", newCNet->m_Weapon);

					int id = snapItem->Id();

					//does player already exist
					if(maybeAddNewPlayer(id, newCNet, nullptr))
					{
						printf("demo: \n");
						print_CharacterCore((CNetObj_CharacterCore*)newCNet);
					}else
					{

						printf("compare memory\n");

						CNetObj_Character oldCNet; 
						players[id]->m_Core.Write(&oldCNet);

						// oldCNet.m_Tick = 0;
						// newCNet->m_Tick = 0;


						printf("simulated: \n");
						print_CharacterCore(&oldCNet);
						printf("\n");
						printf("demo: \n");
						print_CharacterCore((CNetObj_CharacterCore*)newCNet);

						if(!compareCNet_CharacterCore(&oldCNet, newCNet))
						{
							//not same
							printf("\033[31m---------different-------\n");
							// m_DemoPlayer.Stop("error, physics check failed");
							fail = true;
							// return;
						}else
							printf("\033[32m++++++++++++SAME++++++++++++\n");
						
						printf("\033[0m");
					}

					//sync player
					players[id]->Read(newCNet, nullptr, false);
				}
			}

			// if(type == NETOBJTYPE_GAMEINFOEX)
			// {
			// 	printf("\nNETOBJTYPE_GAMEINFOEX\n");
			// 	//leave server info blank
			// 	CServerInfo ServerInfo;
			// 	m_GameInfo = GetGameInfo((const CNetObj_GameInfoEx *)pData, newSnapshot->GetItemSize(i), &ServerInfo);
			// 	m_GameWorld.m_WorldConfig.m_IsVanilla = m_GameInfo.m_PredictVanilla;
			// 	m_GameWorld.m_WorldConfig.m_IsDDRace = m_GameInfo.m_PredictDDRace;
			// 	m_GameWorld.m_WorldConfig.m_IsFNG = m_GameInfo.m_PredictFNG;
			// 	m_GameWorld.m_WorldConfig.m_PredictDDRace = m_GameInfo.m_PredictDDRace;
			// 	m_GameWorld.m_WorldConfig.m_PredictTiles = m_GameInfo.m_PredictDDRace && m_GameInfo.m_PredictDDRaceTiles;
			// 	m_GameWorld.m_WorldConfig.m_UseTuneZones = m_GameInfo.m_PredictDDRaceTiles;
			// 	m_GameWorld.m_WorldConfig.m_PredictFreeze = g_Config.m_ClPredictFreeze;
			// 	m_GameWorld.m_WorldConfig.m_PredictWeapons = true;
			// 	m_GameWorld.m_WorldConfig.m_BugDDRaceInput = m_GameInfo.m_BugDDRaceInput;
			// 	m_GameWorld.m_WorldConfig.m_NoWeakHookAndBounce = m_GameInfo.m_NoWeakHookAndBounce;
			// }
		}

		printf("end of function\n");
	}

	void OnDemoPlayerMessage(void *pData, int Size) override
	{
		return;
	}
};

TEST_F(PhysicsChecker, Demo)
{
	ASSERT_TRUE(Start("demo.demo"));
}
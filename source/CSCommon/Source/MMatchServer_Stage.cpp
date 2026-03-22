#include "stdafx.h"
#include "MMatrix.h"
#include "MMatchServer.h"
#include "MSharedCommandTable.h"
#include "MErrorTable.h"
#include "MBlobArray.h"
#include "MObject.h"
#include "MMatchObject.h"
#include "MMatchItem.h"
#include "MAgentObject.h"
#include "MMatchNotify.h"
#include "Msg.h"
#include "MMatchObjCache.h"
#include "MMatchStage.h"
#include "MMatchTransDataType.h"
#include "MMatchFormula.h"
#include "MMatchConfig.h"
#include "MCommandCommunicator.h"
#include "MMatchShop.h"
#include "MMatchTransDataType.h"
#include "MDebug.h"
#include "MMatchAuth.h"
#include "MMatchStatus.h"
#include "MAsyncDBJob.h"
#include "MVoteDiscussImpl.h"
#include "MUtil.h"
#include "MMatchGameType.h"
#include "MMatchRuleBaseQuest.h"
#include "MMatchRuleQuest.h"
#include "MMatchRuleBerserker.h"
#include "MMatchRuleDuel.h"
#include "MCrashDump.h"

#include "MAsyncDBJob_InsertGamePlayerLog.h"

static bool StageShowInfo(MMatchServer* pServer, const MUID& uidPlayer, const MUID& uidStage, char* pszChat);


MMatchStage* MMatchServer::FindStage(const MUID& uidStage)
{
	MMatchStageMap::iterator i = m_StageMap.find(uidStage);
	if(i==m_StageMap.end()) return NULL;

	MMatchStage* pStage = (*i).second;
	return pStage;
}

bool MMatchServer::StageAdd(MMatchChannel* pChannel, const char* pszStageName, bool bPrivate, const char* pszStagePassword, MUID* pAllocUID, bool bIsAllowNullChannel)
{
	// 클랜전은 pChannel이 NULL이다.

	MUID uidStage = m_StageMap.UseUID();
	
	MMatchStage* pStage= new MMatchStage;
	if (pChannel && !pChannel->AddStage(pStage)) {
		delete pStage;
		return false;
	}


	MMATCH_GAMETYPE GameType = MMATCH_GAMETYPE_DEFAULT;
	bool bIsCheckTicket = false;
	DWORD dwTicketID = 0;

	if ( (NULL != pChannel) && MGetServerConfig()->IsUseTicket()) {
		bIsCheckTicket = (pChannel != 0) && pChannel->IsUseTicket() && pChannel->IsTicketChannel();
		dwTicketID = pChannel->GetTicketItemID();

		// 티켓 서버에서 사설 채널은 무조건 티켓 검사 - 티켓은 클랜전 티켓과 동일하다.
		if ( pChannel->GetChannelType() == MCHANNEL_TYPE_USER) {
			bIsCheckTicket = true;
			dwTicketID = GetChannelMap()->GetClanChannelTicketInfo().m_dwTicketItemID;
		}
	}

	if (!pStage->Create( uidStage, pszStageName, bPrivate, pszStagePassword, bIsAllowNullChannel, GameType, bIsCheckTicket, dwTicketID) ) {
		if (pChannel) {
			pChannel->RemoveStage(pStage);
		}

		delete pStage;
		return false;
	}

	m_StageMap.Insert(uidStage, pStage);

	*pAllocUID = uidStage;

	return true;
}


bool MMatchServer::StageRemove(const MUID& uidStage, MMatchStageMap::iterator* pNextItor)
{
	MMatchStageMap::iterator i = m_StageMap.find(uidStage);
	if(i==m_StageMap.end()) {
		return false;
	}

	MMatchStage* pStage = (*i).second;

	MMatchChannel* pChannel = FindChannel(pStage->GetOwnerChannel());
	if (pChannel) {
		pChannel->RemoveStage(pStage);
	}

	pStage->Destroy();
	delete pStage;

	MMatchStageMap::iterator itorTemp = m_StageMap.erase(i);
	if (pNextItor) *pNextItor = itorTemp;

	return true;
}


bool MMatchServer::StageJoin(const MUID& uidPlayer, const MUID& uidStage)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (!IsEnabledObject(pObj)) return false;

	if (pObj->GetStageUID() != MUID(0,0))
		StageLeave(pObj->GetUID());//, pObj->GetStageUID());

	MMatchChannel* pChannel = FindChannel(pObj->GetChannelUID());
	if (pChannel == NULL) return false;
	if (pChannel->GetChannelType() == MCHANNEL_TYPE_DUELTOURNAMENT) return false;

	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return false;

	int ret = ValidateStageJoin(uidPlayer, uidStage);
	if (ret != MOK) {
		RouteResponseToListener(pObj, MC_MATCH_RESPONSE_STAGE_JOIN, ret);
		return false;
	}
	pObj->OnStageJoin();

	// Cache Add
	MMatchObjectCacheBuilder CacheBuilder;
	CacheBuilder.AddObject(pObj);
	MCommand* pCmdCacheAdd = CacheBuilder.GetResultCmd(MATCHCACHEMODE_ADD, this);
	RouteToStage(pStage->GetUID(), pCmdCacheAdd);

	// Join
	pStage->AddObject(uidPlayer, pObj);
		// 임시코드... 잘못된 클랜ID 온다면 체크하여 잡기위함...20090224 by kammir
	if(pObj->GetCharInfo()->m_ClanInfo.GetClanID() >= 9000000)
		LOG(LOG_FILE, "[UpdateCharClanInfo()] %s's ClanID:%d.", pObj->GetAccountName(), pObj->GetCharInfo()->m_ClanInfo.GetClanID());

	pObj->SetStageUID(uidStage);
	pObj->SetStageState(MOSS_NONREADY);
	pObj->SetTeam(pStage->GetRecommandedTeam());

	// Cast Join
	MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_JOIN), MUID(0,0), m_This);
	pNew->AddParameter(new MCommandParameterUID(uidPlayer));
	pNew->AddParameter(new MCommandParameterUID(pStage->GetUID()));
	pNew->AddParameter(new MCommandParameterUInt(pStage->GetIndex()+1));
	pNew->AddParameter(new MCommandParameterString((char*)pStage->GetName()));
	
	if (pStage->GetState() == STAGE_STATE_STANDBY)  RouteToStage(pStage->GetUID(), pNew);
	else											RouteToListener(pObj, pNew);


	// Cache Update
	CacheBuilder.Reset();
	for (MUIDRefCache::iterator i=pStage->GetObjBegin(); i!=pStage->GetObjEnd(); i++) {
		MUID uidObj = (MUID)(*i).first;
		MMatchObject* pScanObj = (MMatchObject*)GetObject(uidObj);
		if (pScanObj) {
			CacheBuilder.AddObject(pScanObj);
		} else {
			LOG(LOG_PROG, "MMatchServer::StageJoin - Invalid ObjectMUID(%u:%u) exist in Stage(%s)\n",
				uidObj.High, uidObj.Low, pStage->GetName());
			pStage->RemoveObject(uidObj);
			return false;
		}
	}
    MCommand* pCmdCacheUpdate = CacheBuilder.GetResultCmd(MATCHCACHEMODE_UPDATE, this);
	RouteToListener(pObj, pCmdCacheUpdate);


	// Cast Master(방장)
	MUID uidMaster = pStage->GetMasterUID();
	MCommand* pMasterCmd = CreateCommand(MC_MATCH_STAGE_MASTER, MUID(0,0));
	pMasterCmd->AddParameter(new MCommandParameterUID(uidStage));
	pMasterCmd->AddParameter(new MCommandParameterUID(uidMaster));
	RouteToListener(pObj, pMasterCmd);


#ifdef _QUEST_ITEM
	if (MGetServerConfig()->GetServerMode() == MSM_QUEST)
	{
		const MSTAGE_SETTING_NODE* pNode = pStage->GetStageSetting()->GetStageSetting();
		if( 0 == pNode )
		{
			mlog( "MMatchServer::StageJoin - 스테이지 셋팅 노드 찾기 실패.\n" );
			return false;
		}

		if (MGetGameTypeMgr()->IsQuestDerived(pNode->nGameType))
		{
			MMatchRuleBaseQuest* pRuleQuest = reinterpret_cast< MMatchRuleBaseQuest* >( pStage->GetRule() );
			if( 0 == pRuleQuest )
			{
				mlog( "MMatchServer::StageJoin - 포인터 형변환 실패.\n" );
				return false;
			}

			pRuleQuest->OnChangeCondition();
			//pRuleQuest->OnResponseQL_ToStage( pObj->GetStageUID() );
			// 동환씨께서 처음 스테이지 조인시는 이전에 설정이 퀘스트로 되있어도 
			//  처음 조인한 유저는 퀘스트 타입인지 알수가 없기에,
			//	클라이언트가 스테이지 타입이 퀘스트인지를 인식하는 시점에서
			//  이 정보를 요청을 하는 방향으로 수정함. - 05/04/14 by 추교성.
			// pStage->GetRule()->OnResponseSacrificeSlotInfoToStage( uidPlayer );
		}
	}
#endif


	// Cast Character Setting
	StageTeam(uidPlayer, uidStage, pObj->GetTeam());
	StagePlayerState(uidPlayer, uidStage, pObj->GetStageState());


	// 방송 관계자면 방장권한을 자동으로 빼앗는다. - 온게임넷 비비빅 요청
	//We can enable this if needed. It will automaticly change the stage master to UID if their grade is 252
	//if (MMUG_GM == pObj->GetAccountInfo()->m_nUGrade) {
	//	OnEventChangeMaster(pObj->GetUID());
	//}

	return true;
}

bool MMatchServer::StageLeave(const MUID& uidPlayer)//, const MUID& uidStage)
{
	MMatchObject* pObj = GetObject( uidPlayer );
	if( !IsEnabledObject(pObj) ) return false;
	// MMatchStage* pStage = FindStage(uidStage);

	//if(pObj->GetStageUID()!=uidStage)
	//	mlog(" stage leave hack %s (%d, %d) ignore\n", pObj->GetName(), uidPlayer.High, uidPlayer.Low);

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return false;

	bool bLeaverMaster = false;
	if (uidPlayer == pStage->GetMasterUID()) bLeaverMaster = true;

#ifdef _QUEST_ITEM
	if (MGetServerConfig()->GetServerMode() == MSM_QUEST)
	{
		const MSTAGE_SETTING_NODE* pNode = pStage->GetStageSetting()->GetStageSetting();
		if( 0 != pNode )
		{
			if (MGetGameTypeMgr()->IsQuestDerived(pNode->nGameType))
			{
				MMatchRuleBaseQuest* pRuleQuest = reinterpret_cast< MMatchRuleBaseQuest* >( pStage->GetRule() );
				if(pRuleQuest)
				{
					pRuleQuest->PreProcessLeaveStage( uidPlayer );
				} else {
					LOG(LOG_PROG, "StageLeave:: MMatchRule to MMatchRuleBaseQuest FAILED \n");
				}
			}
		}
	}
#endif

	MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_LEAVE), MUID(0,0), m_This);
	pNew->AddParameter(new MCommandParameterUID(uidPlayer));
	// pNew->AddParameter(new MCommandParameterUID(pStage->GetUID()));
	RouteToStage(pStage->GetUID(), pNew);

	pStage->RemoveObject(uidPlayer);

	//MMatchObject* pObj = GetObject(uidPlayer);
	//if (pObj)
	{
		MMatchObjectCacheBuilder CacheBuilder;
		CacheBuilder.AddObject(pObj);
		MCommand* pCmdCache = CacheBuilder.GetResultCmd(MATCHCACHEMODE_REMOVE, this);
		RouteToStage(pStage->GetUID(), pCmdCache);
	}

	// cast Master
	if (bLeaverMaster) StageMaster(pStage->GetUID());

#ifdef _QUEST_ITEM
	// 유저가 스테이지에서 나간후에 QL을 다시 계산해 줘야 함.
	if (MGetServerConfig()->GetServerMode() == MSM_QUEST)
	{
		const MSTAGE_SETTING_NODE* pNode = pStage->GetStageSetting()->GetStageSetting();
		if( 0 == pNode )
		{
			mlog( "MMatchServer::StageLeave - 스테이지 셋팅 노드 찾기 실패.\n" );
			return false;
		}

		if (MGetGameTypeMgr()->IsQuestDerived(pNode->nGameType))
		{
			MMatchRuleBaseQuest* pRuleQuest = reinterpret_cast< MMatchRuleBaseQuest* >( pStage->GetRule() );
			if( 0 == pRuleQuest )
			{
				mlog( "MMatchServer::StageLeave - 포인터 형변환 실패.\n" );
				return false;
			}

			if( STAGE_STATE_STANDBY == pStage->GetState() )
				pRuleQuest->OnChangeCondition();
				//pRuleQuest->OnResponseQL_ToStage( uidStage );
		}
	}
#endif


	return true;
}



DWORD StageEnterBattleExceptionHandler( PEXCEPTION_POINTERS ExceptionInfo )
{
	char szStageDumpFileName[ _MAX_DIR ]= {0,};
	SYSTEMTIME SystemTime;
	GetLocalTime( &SystemTime );
	sprintf( szStageDumpFileName, "Log/StageDump_%d-%d-%d_%d-%d-%d.dmp"
		, SystemTime.wYear
		, SystemTime.wMonth
		, SystemTime.wDay
		, SystemTime.wHour
		, SystemTime.wMinute
		, SystemTime.wSecond );

	return CrashExceptionDump( ExceptionInfo, szStageDumpFileName, true );
}



bool ExceptionTraceStageEnterBattle( MMatchObject* pObj, MMatchStage* pStage )
{
	if( NULL == pObj )
	{
		return false;
	}

	if( NULL == pStage )
	{
		return false;
	}

	__try
	{
		pStage->EnterBattle(pObj);
	}
	__except( StageEnterBattleExceptionHandler(GetExceptionInformation()) )
	{
	/*	mlog( "\nexception : stage enter battle =====================\n" );


		MMatchObject* pMaster = MGetMatchServer()->GetObject( pStage->GetMasterUID() );
		if( NULL != pMaster )  
		{
			if( NULL != pMaster->GetCharInfo() )
			{
				mlog( "stage master cid : %d\n", pMaster->GetCharInfo()->m_nCID );
			}
		}
		else
		{
			mlog( "stage master hass problem.\n" );				
		}
		
		
		if( NULL != pObj->GetCharInfo() )
		{
			mlog( "cmd sender cid : %d\n", pObj->GetCharInfo()->m_nCID );
		}
		else
		{
			mlog( "cmd send char info null point.\n" );
		}

		
		MMatchStageSetting*	pStageSetting = pStage->GetStageSetting();
		if( NULL != pStageSetting )
		{
			mlog( "stage state : %d\n", pStage->GetStageSetting()->GetStageState() );

			const MSTAGE_SETTING_NODE* pExStageSettingNode = pStageSetting->GetStageSetting();
			if( NULL != pExStageSettingNode )
			{
				mlog( "stage name : %s\n", pExStageSettingNode->szMapName );
				mlog( "stage game type : %d\n", pExStageSettingNode->nGameType );
				mlog( "stage max player : %d\n", pExStageSettingNode->nMaxPlayers );
				mlog( "stage current player : %d\n", pStage->GetPlayers() );
				mlog( "stage force entry enable : %d\n", pExStageSettingNode->bForcedEntryEnabled );
				mlog( "stage rule pointer : %x\n", pStage->GetRule() );
			}
		}

		MUIDRefCache::iterator itStage, endStage;
		endStage = pStage->GetObjEnd();
		itStage = pStage->GetObjBegin();
		MMatchObject* pObj = NULL;
		for( ; endStage != itStage; ++itStage )
		{
			pObj = MGetMatchServer()->GetObject( itStage->first );
			if( NULL == pObj )
			{
				mlog( "!!!!stage can't find player!!!!\n" );
				continue;
			}

			mlog( "stage player name : %s\n", pObj->GetName() );
		}

		mlog( "=====================\n\n" );
		*/
		return false;
	}

	return true;
}



bool MMatchServer::StageEnterBattle(const MUID& uidPlayer, const MUID& uidStage)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (!IsEnabledObject(pObj)) return false;
	// MMatchStage* pStage = FindStage(uidStage);
	
	if(pObj->GetStageUID()!=uidStage)
		mlog(" stage enter battle hack %s (%d, %d) ignore\n", pObj->GetName(), uidPlayer.High, uidPlayer.Low);

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return false;

	pObj->SetPlace(MMP_BATTLE);

	MCommand* pNew = CreateCommand(MC_MATCH_STAGE_ENTERBATTLE, MUID(0,0));
	//pNew->AddParameter(new MCommandParameterUID(uidPlayer));
	//pNew->AddParameter(new MCommandParameterUID(uidStage));

	unsigned char nParam = MCEP_NORMAL;
	if (pObj->IsForcedEntried()) nParam = MCEP_FORCED;
	pNew->AddParameter(new MCommandParameterUChar(nParam));

	void* pPeerArray = MMakeBlobArray(sizeof(MTD_PeerListNode), 1);
	MTD_PeerListNode* pNode = (MTD_PeerListNode*)MGetBlobArrayElement(pPeerArray, 0);
	memset(pNode, 0, sizeof(MTD_PeerListNode));
	
	pNode->uidChar	= pObj->GetUID();
	pNode->dwIP		= pObj->GetIP();
	pNode->nPort	= pObj->GetPort();

	CopyCharInfoForTrans(&pNode->CharInfo, pObj->GetCharInfo(), pObj);
	//버프정보임시주석 	CopyCharBuffInfoForTrans(&pNode->CharBuffInfo, pObj->GetCharInfo(), pObj);

	pNode->ExtendInfo.nPlayerFlags = pObj->GetPlayerFlags();
	if (pStage->GetStageSetting()->IsTeamPlay())	pNode->ExtendInfo.nTeam = (char)pObj->GetTeam();
	else											pNode->ExtendInfo.nTeam = 0;	

	pNew->AddParameter(new MCommandParameterBlob(pPeerArray, MGetBlobArraySize(pPeerArray)));
	MEraseBlobArray(pPeerArray);

	RouteToStage(uidStage, pNew);

	// 배틀 시작시간 세팅
	pObj->GetCharInfo()->m_nBattleStartTime = MMatchServer::GetInstance()->GetGlobalClockCount();
	pObj->GetCharInfo()->m_nBattleStartXP = pObj->GetCharInfo()->m_nXP;

	// 라우팅 후에 넣어야 한다.
	return ExceptionTraceStageEnterBattle( pObj, pStage );
}

bool MMatchServer::StageLeaveBattle(const MUID& uidPlayer, bool bGameFinishLeaveBattle, bool bForcedLeave)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (!IsEnabledObject(pObj)) return false;
	if (pObj->GetPlace() != MMP_BATTLE) { return false; }

	// MMatchStage* pStage = FindStage(uidStage);

	//if(pObj->GetStageUID()!=uidStage)
	//	mlog(" stage leave battle hack %s (%d, %d) ignore\n", pObj->GetName(), uidPlayer.High, uidPlayer.Low);

	const MUID uidStage = pObj->GetStageUID();

	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL)
	{	// 클랜전시 한족이 다나가면 스테이지가 없어지므로 여기서 agent를 끊어준다. 
		if (pObj->GetRelayPeer()) {
			MAgentObject* pAgent = GetAgent(pObj->GetAgentUID());
			if (pAgent) {
				MCommand* pCmd = CreateCommand(MC_AGENT_PEER_UNBIND, pAgent->GetCommListener());
				pCmd->AddParameter(new MCmdParamUID(uidPlayer));
				Post(pCmd);
			}
		}

		UpdateCharDBCachingData(pObj);		///< XP, BP, KillCount, DeathCount 캐슁 업데이트
		UpdateCharItemDBCachingData(pObj);	///< Character Item에서 업데이트가 필요한 것들 업데이트
		//CheckSpendableItemCounts(pObj);		///< 항상 UpdateCharItemDBCachingData 뒤에 있어야 합니다.
		
		ProcessCharPlayInfo(pObj);			///< 캐릭터 플레이한 정보 업데이트 
		return false;
	}
	else
	{
		// 플레이 직후 대기실에서 경험치, 승/패, 승률, 바운티가 반영되지 않습니다. - by kammir 2008.09.19
		// LeaveBattle가 되면서 캐릭터 데이터를 업데이트 해준다.
		MMatchObjectCacheBuilder CacheBuilder;
		CacheBuilder.Reset();
		for (MUIDRefCache::iterator i=pStage->GetObjBegin(); i!=pStage->GetObjEnd(); i++) {
			MMatchObject* pScanObj = (MMatchObject*)(*i).second;
			CacheBuilder.AddObject(pScanObj);
		}
		MCommand* pCmdCacheUpdate = CacheBuilder.GetResultCmd(MATCHCACHEMODE_UPDATE, this);
		RouteToListener(pObj, pCmdCacheUpdate);
	}

	pStage->LeaveBattle(pObj);
	pObj->SetPlace(MMP_STAGE);


	// 레벨에 안맞는 장비아이템 체크
#define LEGAL_ITEMLEVEL_DIFF		3
	bool bIsCorrect = true;
	for (int i = 0; i < MMCIP_END; i++) {
		if (CorrectEquipmentByLevel(pObj, MMatchCharItemParts(i), LEGAL_ITEMLEVEL_DIFF)) {
			bIsCorrect = false;
		}
	}

	if (!bIsCorrect) {
		MCommand* pNewCmd = CreateCommand(MC_MATCH_RESPONSE_RESULT, MUID(0,0));
		pNewCmd->AddParameter(new MCommandParameterInt(MERR_TAKEOFF_ITEM_BY_LEVELDOWN));
		RouteToListener(pObj, pNewCmd);
	}
	
	CheckExpiredItems(pObj);		//< 기간 만료 아이템이 있는지 체크

	if (pObj->GetRelayPeer()) {
		MAgentObject* pAgent = GetAgent(pObj->GetAgentUID());
		if (pAgent) {
			MCommand* pCmd = CreateCommand(MC_AGENT_PEER_UNBIND, pAgent->GetCommListener());
			pCmd->AddParameter(new MCmdParamUID(uidPlayer));
			Post(pCmd);
		}
	}	

	// 캐릭터 플레이한 정보 업데이트 
	ProcessCharPlayInfo(pObj);

	//=======================================================================================================================================
	
	bool bIsLeaveAllBattle = true;
	
	for (MUIDRefCache::iterator i=pStage->GetObjBegin(); i!=pStage->GetObjEnd(); i++) {
		MUID uidObj = (MUID)(*i).first;
		MMatchObject* pAllObj = (MMatchObject*)GetObject(uidObj);
		if(NULL == pAllObj) continue;
		if(MMP_STAGE != pAllObj->GetPlace()) { 
			bIsLeaveAllBattle = false; 
			break; 
		}
	}


	if(pStage->IsRelayMap())
	{
		if(bGameFinishLeaveBattle)
		{	// 릴레이맵, 배틀 종료로 스테이지로 나왔을때
			if(!pStage->m_bIsLastRelayMap)
			{	// 다음맵이 있다면 바로 다음 맵시작 처리		

				if( !bForcedLeave ) 
				{
					pObj->SetStageState(MOSS_READY);
				}

				if( bIsLeaveAllBattle ) 
				{					
					OnStageRelayStart(uidStage);
				} 

				MCommand* pNew = CreateCommand(MC_MATCH_STAGE_LEAVEBATTLE_TO_CLIENT, MUID(0,0));
				pNew->AddParameter(new MCommandParameterUID(uidPlayer));
				pNew->AddParameter(new MCommandParameterBool(true));
				RouteToStage(uidStage, pNew);
			}
		}
		else
		{	///< 메인 메뉴로 스테이지에 나옴		
			MCommand* pNew = CreateCommand(MC_MATCH_STAGE_LEAVEBATTLE_TO_CLIENT, MUID(0,0));
			pNew->AddParameter(new MCommandParameterUID(uidPlayer));
			pNew->AddParameter(new MCommandParameterBool(false));
			RouteToStage(uidStage, pNew);			

			if(bIsLeaveAllBattle) 
			{	///< 모두 스테이지에 있다면 릴레이맵 세팅을 다시 해준다.
				pStage->m_bIsLastRelayMap = true;//릴레이맵을 끝낸다
				pStage->GetStageSetting()->SetMapName(MMATCH_MAPNAME_RELAYMAP);
				pStage->SetRelayMapCurrList(pStage->GetRelayMapList());
				pStage->m_RelayMapRepeatCountRemained = pStage->GetRelayMapRepeatCount();
			}
		}
	} 
	else 
	{
		MCommand* pNew = CreateCommand(MC_MATCH_STAGE_LEAVEBATTLE_TO_CLIENT, MUID(0,0));
		pNew->AddParameter(new MCommandParameterUID(uidPlayer));
		pNew->AddParameter(new MCommandParameterBool(false));
		RouteToStage(uidStage, pNew);
	}

	//=======================================================================================================================================

	// 방에서 나가면 noready상태로 변경된다. 
	// 변경된 정보를 스테이지의 모든 클라이언트로 보내줌. - by SungE 2007-06-04
	StagePlayerState( uidPlayer, pStage->GetUID(), pObj->GetStageState() );	
	
	UpdateCharDBCachingData(pObj);		///< XP, BP, KillCount, DeathCount 캐슁 업데이트
	UpdateCharItemDBCachingData(pObj);	///< Character Item에서 업데이트가 필요한 것들 업데이트
	//CheckSpendableItemCounts(pObj);		///< 항상 UpdateCharItemDBCachingData 뒤에 있어야 합니다.

	return true;
}

bool MMatchServer::StageChat(const MUID& uidPlayer, const MUID& uidStage, char* pszChat)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL)	return false;
	MMatchObject* pObj = (MMatchObject*)GetObject(uidPlayer);
	if ((pObj == NULL) || (pObj->GetCharInfo() == NULL)) return false;

	if (pObj->GetAccountInfo()->m_nUGrade == MMUG_CHAT_LIMITED) return false;

//	InsertChatDBLog(uidPlayer, pszChat);

	///< 홍기주(2009.08.04)
	///< 현재 해당 사용자가 있는 Stage와 보내온 Stage의 UID가 다를 경우!
	///< 다른 Stage들에게도 Msg를 보낼 수 있는 문제가 있음 (해킹 프로그램 사용시)
	if( uidStage != pObj->GetStageUID() )
	{
		//LOG(LOG_FILE,"MMatchServer::StageChat - Different Stage(S:%d, P:%d)", uidStage, pObj->GetStageUID());
		return false;
	}


	MCommand* pCmd = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_CHAT), MUID(0,0), m_This);
	pCmd->AddParameter(new MCommandParameterUID(uidPlayer));
	pCmd->AddParameter(new MCommandParameterUID(uidStage));
	pCmd->AddParameter(new MCommandParameterString(pszChat));
	RouteToStage(uidStage, pCmd);
	return true;
}

bool MMatchServer::StageTeam(const MUID& uidPlayer, const MUID& uidStage, MMatchTeam nTeam)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return false;

	pStage->PlayerTeam(uidPlayer, nTeam);

	MCommand* pCmd = CreateCommand(MC_MATCH_STAGE_TEAM, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterUID(uidPlayer));
	pCmd->AddParameter(new MCommandParameterUID(uidStage));
	pCmd->AddParameter(new MCommandParameterUInt(nTeam));

	RouteToStageWaitRoom(uidStage, pCmd);
	return true;
}

bool MMatchServer::StagePlayerState(const MUID& uidPlayer, const MUID& uidStage, MMatchObjectStageState nStageState)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (!IsEnabledObject(pObj)) return false;
	// MMatchStage* pStage = FindStage(uidStage);
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return false;

	pStage->PlayerState(uidPlayer, nStageState);
	
	MCommand* pCmd = CreateCommand(MC_MATCH_STAGE_PLAYER_STATE, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterUID(uidPlayer));
	pCmd->AddParameter(new MCommandParameterUID(uidStage));
	pCmd->AddParameter(new MCommandParameterInt(nStageState));
	RouteToStage(uidStage, pCmd);
	return true;
}

bool MMatchServer::StageMaster(const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return false;

	MUID uidMaster = pStage->GetMasterUID();

	MCommand* pCmd = CreateCommand(MC_MATCH_STAGE_MASTER, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterUID(uidStage));
	pCmd->AddParameter(new MCommandParameterUID(uidMaster));
	RouteToStage(uidStage, pCmd);

	return true;
}

void MMatchServer::StageLaunch(const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;

	ReserveAgent(pStage);

	MCommand* pCmd = CreateCommand(MC_MATCH_STAGE_LAUNCH, MUID(0,0));
	pCmd->AddParameter(new MCmdParamUID(uidStage));
	pCmd->AddParameter(new MCmdParamStr( const_cast<char*>(pStage->GetMapName()) ));
	RouteToStage(uidStage, pCmd);
}

void MMatchServer::StageRelayLaunch(const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;

	ReserveAgent(pStage);

	for (MUIDRefCache::iterator i=pStage->GetObjBegin(); i!=pStage->GetObjEnd(); i++) {
		MUID uidObj = (MUID)(*i).first;
		MMatchObject* pObj = (MMatchObject*)GetObject(uidObj);
		if (pObj) {
			if( pObj->GetStageState() == MOSS_READY) {
				MCommand* pCmd = CreateCommand(MC_MATCH_STAGE_RELAY_LAUNCH, MUID(0,0));
				pCmd->AddParameter(new MCmdParamUID(uidStage));
				pCmd->AddParameter(new MCmdParamStr(const_cast<char*>(pStage->GetMapName())));
				pCmd->AddParameter(new MCmdParamBool(false));
				RouteToListener(pObj, pCmd);
			} else {
				MCommand* pCmd = CreateCommand(MC_MATCH_STAGE_RELAY_LAUNCH, MUID(0,0));
				pCmd->AddParameter(new MCmdParamUID(uidStage));
				pCmd->AddParameter(new MCmdParamStr(const_cast<char*>(pStage->GetMapName())));
				pCmd->AddParameter(new MCmdParamBool(true));
				RouteToListener(pObj, pCmd);
			}
		} else {
			LOG(LOG_PROG, "WARNING(StageRelayLaunch) : Not Existing Obj(%u:%u)\n", uidObj.High, uidObj.Low);
			i=pStage->RemoveObject(uidObj);
			LogObjectCommandHistory(uidObj);
		}
	}
}

void MMatchServer::StageFinishGame(const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;

	bool bIsRelayMapUnFinish = true;

	if(pStage->IsRelayMap())
	{ // 릴레이 맵일때에는 배틀을 다시 시작해준다. 
		if((int)pStage->m_vecRelayMapsRemained.size() <= 0)
		{	// 남은 맵이 없을때
			int nRepeatCount = (int)pStage->m_RelayMapRepeatCountRemained - 1;
			if(nRepeatCount < 0)
			{
				bIsRelayMapUnFinish = false;

				pStage->m_bIsLastRelayMap = true;//릴레이맵을 끝낸다				
				nRepeatCount = 0;
				pStage->GetStageSetting()->SetMapName(MMATCH_MAPNAME_RELAYMAP);	//"RelayMap" 세팅
			}
			pStage->m_RelayMapRepeatCountRemained = (RELAY_MAP_REPEAT_COUNT)nRepeatCount;
			pStage->SetRelayMapCurrList(pStage->GetRelayMapList());
		}

		if(!pStage->m_bIsLastRelayMap) {
			// 처음 시작시, Flag를 On시켜준다. 
			if( pStage->IsStartRelayMap() == false ) {
				pStage->SetIsStartRelayMap(true);
			}			

			if((int)pStage->m_vecRelayMapsRemained.size() > 0) { // 다음맵이 있다면
				int nRelayMapIndex = 0;

				if(pStage->GetRelayMapType() == RELAY_MAP_TURN) {	//< 남은 것중에서 첫 번째부터 시작(가독성)
					nRelayMapIndex = 0; 
				} else if(pStage->GetRelayMapType() == RELAY_MAP_RANDOM) {
					nRelayMapIndex = rand() % (int)pStage->m_vecRelayMapsRemained.size();
				}

				if(nRelayMapIndex >= MAX_RELAYMAP_LIST_COUNT) { //< 맵 구성이 20개
					mlog("StageFinishGame RelayMap Fail RelayMapList MIsCorrect MaxCount[%d] \n", (int)nRelayMapIndex);
					return;
				}

				char* szMapName = (char*)MGetMapDescMgr()->GetMapName(pStage->m_vecRelayMapsRemained[nRelayMapIndex].nMapID);
				if (!szMapName)
				{
					mlog("RelayMapBattleStart Fail MapID[%d] \n", (int)pStage->m_vecRelayMapsRemained[nRelayMapIndex].nMapID);
					return;
				}

				pStage->GetStageSetting()->SetMapName(szMapName);

				// 실행한 릴레이맵은 삭제해준다.
				vector<RelayMap>::iterator itor = pStage->m_vecRelayMapsRemained.begin();
				for(int i=0 ; nRelayMapIndex > i ; ++itor, ++i);// 해당 인덱스까지 이동
				pStage->m_vecRelayMapsRemained.erase(itor);
			} 
			else {
				mlog("MMatchServer::StageFinishGame::IsRelayMap() - m_vecRelayMapsRemained.size() == 0\n");
			}
		} else {
			pStage->SetIsStartRelayMap(false);
			bIsRelayMapUnFinish = false; // 릴레이맵 진행이 끝났음
		}
	}

	MCommand* pCmd = CreateCommand(MC_MATCH_STAGE_FINISH_GAME, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterUID(uidStage));
	pCmd->AddParameter(new MCommandParameterBool(bIsRelayMapUnFinish));
	RouteToStage(uidStage, pCmd);

	return;
}

MCommand* MMatchServer::CreateCmdResponseStageSetting(const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return NULL;

	MCommand* pCmd = CreateCommand(MC_MATCH_RESPONSE_STAGESETTING, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterUID(pStage->GetUID()));

	MMatchStageSetting* pSetting = pStage->GetStageSetting();

	// Param 1 : Stage Settings
	void* pStageSettingArray = MMakeBlobArray(sizeof(MSTAGE_SETTING_NODE), 1);
	MSTAGE_SETTING_NODE* pNode = (MSTAGE_SETTING_NODE*)MGetBlobArrayElement(pStageSettingArray, 0);
	CopyMemory(pNode, pSetting->GetStageSetting(), sizeof(MSTAGE_SETTING_NODE));
	pCmd->AddParameter(new MCommandParameterBlob(pStageSettingArray, MGetBlobArraySize(pStageSettingArray)));
	MEraseBlobArray(pStageSettingArray);

	// Param 2 : Char Settings
	int nCharCount = (int)pStage->GetObjCount();
	void* pCharArray = MMakeBlobArray(sizeof(MSTAGE_CHAR_SETTING_NODE), nCharCount);
	int nIndex=0;
	for (MUIDRefCache::iterator itor=pStage->GetObjBegin(); itor!=pStage->GetObjEnd(); itor++) {
		MSTAGE_CHAR_SETTING_NODE* pCharNode = (MSTAGE_CHAR_SETTING_NODE*)MGetBlobArrayElement(pCharArray, nIndex++);
		MMatchObject* pObj = (MMatchObject*)(*itor).second;
		pCharNode->uidChar = pObj->GetUID();
		pCharNode->nTeam = pObj->GetTeam();
		pCharNode->nState = pObj->GetStageState();
	}
	pCmd->AddParameter(new MCommandParameterBlob(pCharArray, MGetBlobArraySize(pCharArray)));
	MEraseBlobArray(pCharArray);

	// Param 3 : Stage State
	pCmd->AddParameter(new MCommandParameterInt((int)pStage->GetState()));

	// Param 4 : Stage Master
	pCmd->AddParameter(new MCommandParameterUID(pStage->GetMasterUID()));

	return pCmd;
}



void MMatchServer::OnStageCreate(const MUID& uidChar, char* pszStageName, bool bPrivate, char* pszStagePassword)
{
	MMatchObject* pObj = GetObject(uidChar);
	if (pObj == NULL) return;

	MMatchChannel* pChannel = FindChannel(pObj->GetChannelUID());
	if (pChannel == NULL) return;

	if ((MGetServerConfig()->GetServerMode() == MSM_CLAN) && (pChannel->GetChannelType() == MCHANNEL_TYPE_CLAN)
		&& (pChannel->GetChannelType() == MCHANNEL_TYPE_DUELTOURNAMENT)) {
		return;
	}
	
	MUID uidStage;

	if (!StageAdd(pChannel, pszStageName, bPrivate, pszStagePassword, &uidStage))
	{
		RouteResponseToListener(pObj, MC_MATCH_RESPONSE_STAGE_CREATE, MERR_CANNOT_CREATE_STAGE);
		return;
	}
	StageJoin(uidChar, uidStage);

	MMatchStage* pStage = FindStage(uidStage);
	if (pStage)
		pStage->SetFirstMasterName(pObj->GetCharInfo()->m_szName);
}


//void MMatchServer::OnStageJoin(const MUID& uidChar, const MUID& uidStage)
//{
//	MMatchObject* pObj = GetObject(uidChar);
//	if (pObj == NULL) return;
//
//	MMatchStage* pStage = NULL;
//
//	if (uidStage == MUID(0,0)) {
//		return;
//	} else {
//		pStage = FindStage(uidStage);
//	}
//
//	if (pStage == NULL) {
//		RouteResponseToListener(pObj, MC_MATCH_RESPONSE_STAGE_JOIN, MERR_STAGE_NOT_EXIST);
//		return;
//	}
//
//	if ((IsAdminGrade(pObj) == false) && pStage->IsPrivate())
//	{
//		RouteResponseToListener(pObj, MC_MATCH_RESPONSE_STAGE_JOIN, MERR_CANNOT_JOIN_STAGE_BY_PASSWORD);
//		return;
//	}
//
//	StageJoin(uidChar, pStage->GetUID());
//}

void MMatchServer::OnPrivateStageJoin(const MUID& uidPlayer, const MUID& uidStage, char* pszPassword)
{
	if (strlen(pszPassword) > STAGEPASSWD_LENGTH) return;

	MMatchStage* pStage = NULL;

	if (uidStage == MUID(0,0)) 
	{
		return;
	} 
	else 
	{
		pStage = FindStage(uidStage);
	}

	if (pStage == NULL) 
	{
		MMatchObject* pObj = GetObject(uidPlayer);
		if (pObj != NULL)
		{
			RouteResponseToListener(pObj, MC_MATCH_RESPONSE_STAGE_JOIN, MERR_STAGE_NOT_EXIST);
		}

		return;
	}

	// 영자나 개발자면 무시..

	bool bSkipPassword = false;

	MMatchObject* pObj = GetObject(uidPlayer);

	if ((pObj == NULL) || (pObj->GetCharInfo() == NULL)) 
		return;

	MMatchUserGradeID ugid = pObj->GetAccountInfo()->m_nUGrade;

	if (ugid == MMUG_DEVELOPER || ugid == MMUG_ADMIN || ugid == MMUG_GM) 
		bSkipPassword = true;

	// 비밀방이 아니거나 패스워드가 틀리면 패스워드가 틀렸다고 응답한다.
	if(bSkipPassword==false) {
		if ((!pStage->IsPrivate()) || (strcmp(pStage->GetPassword(), pszPassword)))
		{
			MMatchObject* pObj = GetObject(uidPlayer);
			if (pObj != NULL)
			{
				RouteResponseToListener(pObj, MC_MATCH_RESPONSE_STAGE_JOIN, MERR_CANNOT_JOIN_STAGE_BY_PASSWORD);
			}

			return;
		}
	}

	StageJoin(uidPlayer, pStage->GetUID());
}

void MMatchServer::OnStageFollow(const MUID& uidPlayer, const char* pszTargetName)
{
	MMatchObject* pPlayerObj = GetObject(uidPlayer);
	if (pPlayerObj == NULL) return;

	MMatchObject* pTargetObj = GetPlayerByName(pszTargetName);
	if (pTargetObj == NULL) return;

	// 자기 자신을 따라 가려고 했을경우 검사.
	if (pPlayerObj->GetUID() == pTargetObj->GetUID()) return;

	// 스테이트가 잘못되어 있는지 검사.
	if (!pPlayerObj->CheckEnableAction(MMatchObject::MMOA_STAGE_FOLLOW)) return;


	// 서로 다른 채널인지 검사.
	if (pPlayerObj->GetChannelUID() != pTargetObj->GetChannelUID()) {

#ifdef _VOTESETTING
		RouteResponseToListener( pPlayerObj, MC_MATCH_RESPONSE_STAGE_FOLLOW, MERR_CANNOT_FOLLOW );
#endif
		return;
	}

	if ((IsAdminGrade(pTargetObj) == true)) {
		NotifyMessage(pPlayerObj->GetUID(), MATCHNOTIFY_GENERAL_USER_NOTFOUND);
		return;
	}

	MMatchStage* pStage = FindStage(pTargetObj->GetStageUID());
	if (pStage == NULL) return;

	// 클랜전게임은 따라갈 수 없음
	if (pStage->GetStageType() != MST_NORMAL) return;

	if (pStage->IsPrivate()==false) {
		if ((pStage->GetStageSetting()->GetForcedEntry()==false) && pStage->GetState() != STAGE_STATE_STANDBY) {
			// Deny Join

#ifdef _VOTESETTING
			RouteResponseToListener( pPlayerObj, MC_MATCH_RESPONSE_STAGE_FOLLOW, MERR_CANNOT_FOLLOW );
#endif
		} else {
			StageJoin(uidPlayer, pTargetObj->GetStageUID());
		}
	}
	else {
		// 따라가려는 방이 비밀번호를 필요로 할경우는 따라갈수 없음.
		//RouteResponseToListener( pPlayerObj, MC_MATCH_RESPONSE_STAGE_FOLLOW, MERR_CANNOT_FOLLOW_BY_PASSWORD );

		// 해당방이 비밀방이면 비밀번호를 요구한다.
		MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_REQUIRE_PASSWORD), MUID(0,0), m_This);
		pNew->AddParameter(new MCommandParameterUID(pStage->GetUID()));
		pNew->AddParameter(new MCommandParameterString((char*)pStage->GetName()));
		RouteToListener(pPlayerObj, pNew);
	}
}

void MMatchServer::OnStageLeave(const MUID& uidPlayer)//, const MUID& uidStage)
{
	MMatchObject* pObj = GetObject( uidPlayer );
	if( !IsEnabledObject(pObj) ) return;
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	if( !IsEnabledObject(GetObject(uidPlayer)) )
	{
		return;
	}

	StageLeave(uidPlayer);// , uidStage);
}

void MMatchServer::OnStageRequestPlayerList(const MUID& uidPlayer, const MUID& uidStage)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;

	// MMatchStage* pStage = FindStage(uidStage);
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	// 방인원 목록
	MMatchObjectCacheBuilder CacheBuilder;
	CacheBuilder.Reset();
	for (MUIDRefCache::iterator i=pStage->GetObjBegin(); i!=pStage->GetObjEnd(); i++) {
		MMatchObject* pScanObj = (MMatchObject*)(*i).second;
		CacheBuilder.AddObject(pScanObj);
	}
    MCommand* pCmdCacheUpdate = CacheBuilder.GetResultCmd(MATCHCACHEMODE_UPDATE, this);
	RouteToListener(pObj, pCmdCacheUpdate);

	// Cast Master(방장)
	MUID uidMaster = pStage->GetMasterUID();
	MCommand* pMasterCmd = CreateCommand(MC_MATCH_STAGE_MASTER, MUID(0,0));
	pMasterCmd->AddParameter(new MCommandParameterUID(uidStage));
	pMasterCmd->AddParameter(new MCommandParameterUID(uidMaster));
	RouteToListener(pObj, pMasterCmd);

	// Cast Character Setting
	StageTeam(uidPlayer, uidStage, pObj->GetTeam());
	StagePlayerState(uidPlayer, uidStage, pObj->GetStageState());
}

void MMatchServer::OnStageEnterBattle(const MUID& uidPlayer, const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;

	StageEnterBattle(uidPlayer, uidStage);
}

void MMatchServer::OnStageLeaveBattle(const MUID& uidPlayer, bool bGameFinishLeaveBattle)//, const MUID& uidStage)
{
	if( !IsEnabledObject(GetObject(uidPlayer)) )
	{
		return;
	}

	StageLeaveBattle(uidPlayer, bGameFinishLeaveBattle, false);//, uidStage);
}


#include "CMLexicalAnalyzer.h"
// 강퇴 임시코드
bool StageKick(MMatchServer* pServer, const MUID& uidPlayer, const MUID& uidStage, char* pszChat)
{
	MMatchObject* pChar = pServer->GetObject(uidPlayer);
	if (pChar == NULL)	return false;
	MMatchStage* pStage = pServer->FindStage(uidStage);
	if (pStage == NULL) return false;
	if (uidPlayer != pStage->GetMasterUID()) return false;

	bool bResult = false;
	CMLexicalAnalyzer lex;
	lex.Create(pszChat);

	if (lex.GetCount() >= 1) {
		char* pszCmd = lex.GetByStr(0);
		if (pszCmd) {
			if (stricmp(pszCmd, "/kick") == 0) {
				if (lex.GetCount() >= 2) {
					char* pszTarget = lex.GetByStr(1);
					if (pszTarget) {
						for (MUIDRefCache::iterator itor = pStage->GetObjBegin(); 
							itor != pStage->GetObjEnd(); ++itor)
						{
							MMatchObject* pTarget = (MMatchObject*)((*itor).second);
							if (stricmp(pszTarget, pTarget->GetName()) == 0) {
								if (pTarget->GetPlace() != MMP_BATTLE) {
									pServer->StageLeave(pTarget->GetUID());//, uidStage);
									bResult = true;
								}
								break;
							}
						}
					}
				}
			}	// Kick
		}
	}

	lex.Destroy();
	return bResult;
}

// 방장확인 임시코드
bool StageShowInfo(MMatchServer* pServer, const MUID& uidPlayer, const MUID& uidStage, char* pszChat)
{
	MMatchObject* pChar = pServer->GetObject(uidPlayer);
	if (pChar == NULL)	return false;
	MMatchStage* pStage = pServer->FindStage(uidStage);
	if (pStage == NULL) return false;
	if (uidPlayer != pStage->GetMasterUID()) return false;

	bool bResult = false;
	CMLexicalAnalyzer lex;
	lex.Create(pszChat);

	if (lex.GetCount() >= 1) {
		char* pszCmd = lex.GetByStr(0);
		if (pszCmd) {
			if (stricmp(pszCmd, "/showinfo") == 0) {
				char szMsg[256]="";
				sprintf(szMsg, "FirstMaster : (%s)", pStage->GetFirstMasterName());
				pServer->Announce(pChar, szMsg);
				bResult = true;
			}	// ShowInfo
		}
	}

	lex.Destroy();
	return bResult;
}
void MMatchServer::OnStageChat(const MUID& uidPlayer, const MUID& uidStage, char* pszChat)
{
	// RAONHAJE : 강퇴 임시코드
	if (pszChat[0] == '/') {
		if (StageKick(this, uidPlayer, uidStage, pszChat))
			return;
		if (StageShowInfo(this, uidPlayer, uidStage, pszChat))
			return;
	}

	StageChat(uidPlayer, uidStage, pszChat);
}

void MMatchServer::OnStageStart(const MUID& uidPlayer, const MUID& uidStage, int nCountdown)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;
	if (pStage->GetMasterUID() != uidPlayer) return;

	if (pStage->StartGame(MGetServerConfig()->IsUseResourceCRC32CacheCheck()) == true) {
		StageRelayMapBattleStart(uidPlayer, uidStage);

		MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_START), MUID(0,0), m_This);
		pNew->AddParameter(new MCommandParameterUID(uidPlayer));
		pNew->AddParameter(new MCommandParameterUID(uidStage));
		pNew->AddParameter(new MCommandParameterInt(min(nCountdown,3)));
		RouteToStage(uidStage, pNew);

		// 디비에 로그를 남긴다.
		SaveGameLog(uidStage);
	}
}

void MMatchServer::OnStageRelayStart(const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;
	
	if (pStage->StartRelayGame(MGetServerConfig()->IsUseResourceCRC32CacheCheck()) == true) {
		// 디비에 로그를 남긴다.
		SaveGameLog(uidStage);
	}
}

void MMatchServer::OnStartStageList(const MUID& uidComm)
{
	MMatchObject* pObj = GetPlayerByCommUID(uidComm);
	if (pObj == NULL) return;

	pObj->SetStageListTransfer(true);
}

void MMatchServer::OnStopStageList(const MUID& uidComm)
{
	MMatchObject* pObj = GetPlayerByCommUID(uidComm);
	if (pObj == NULL) return;

	pObj->SetStageListTransfer(false);
}

void MMatchServer::OnStagePlayerState(const MUID& uidPlayer, const MUID& uidStage, MMatchObjectStageState nStageState)
{
	StagePlayerState(uidPlayer, uidStage, nStageState);
}


void MMatchServer::OnStageTeam(const MUID& uidPlayer, const MUID& uidStage, MMatchTeam nTeam)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;

	MMatchObject* pChar = GetObject(uidPlayer);
	if (pChar == NULL) return;

	StageTeam(uidPlayer, uidStage, nTeam);
}

void MMatchServer::OnStageMap(const MUID& uidStage, char* pszMapName)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;
	if (pStage->GetState() != STAGE_STATE_STANDBY) return;	// 대기상태에서만 바꿀수 있다
	if (strlen(pszMapName) < 2) return;

	pStage->SetMapName( pszMapName );
	pStage->SetIsRelayMap(strcmp(MMATCH_MAPNAME_RELAYMAP, pszMapName) == 0);
	
	MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_MAP), MUID(0,0), m_This);
	pNew->AddParameter(new MCommandParameterUID(uidStage));
	pNew->AddParameter(new MCommandParameterString(pStage->GetMapName()));

	if ( MGetGameTypeMgr()->IsQuestDerived( pStage->GetStageSetting()->GetGameType()))
	{
		MMatchRuleBaseQuest* pQuest = reinterpret_cast< MMatchRuleBaseQuest* >( pStage->GetRule() );
		pQuest->RefreshStageGameInfo();
	}

    RouteToStage(uidStage, pNew);
}

void MMatchServer::StageRelayMapBattleStart(const MUID& uidPlayer, const MUID& uidStage)
{// 릴레이맵 선택하고 게임 시작 버튼 누르면 다음을 수행한다
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;
	if (pStage->GetMasterUID() != uidPlayer) return;
	if(!pStage->IsRelayMap()) return;
	
	// 게임 처음시작시 초기화 해주기
	pStage->InitCurrRelayMap();

	if (pStage->m_vecRelayMapsRemained.empty()) return;

	if((int)pStage->m_vecRelayMapsRemained.size() > MAX_RELAYMAP_LIST_COUNT)
	{// 맵 구성이 20개 초과하면 에러
		mlog("RelayMapBattleStart Fail RelayMapList MIsCorrect OverCount[%d] \n", (int)pStage->m_vecRelayMapsRemained.size());
		return;
	}

	if (pStage->m_vecRelayMapsRemained.size() != pStage->GetRelayMapListCount())
	{
		mlog("m_vecRelayMapsRemained[%d] != GetRelayMapListCount[%d]\n", (int)pStage->m_vecRelayMapsRemained.size(), pStage->GetRelayMapListCount());
		return;
	}

	// 처음 실행할 맵을 정한다
	int nRelayMapIndex = 0;
	if(pStage->GetRelayMapType() == RELAY_MAP_TURN )
		nRelayMapIndex = 0; // 남은것중에서 처음번째 부터 시작(가독성)
	else if(pStage->GetRelayMapType() == RELAY_MAP_RANDOM)
		nRelayMapIndex = rand() % int(pStage->m_vecRelayMapsRemained.size());

	if(MMATCH_MAP_RELAYMAP == pStage->m_vecRelayMapsRemained[nRelayMapIndex].nMapID)
	{
		mlog("RelayMapBattleStart Fail Type[%d], RoundCount[Curr:%d][%d], ListCount[Curr:%d][%d] \n",  
			pStage->GetRelayMapType(), pStage->m_RelayMapRepeatCountRemained, pStage->GetRelayMapRepeatCount(), (int)pStage->m_vecRelayMapsRemained.size(), pStage->GetRelayMapListCount());
		return;
	}

	char* szMapName = (char*)MGetMapDescMgr()->GetMapName(pStage->m_vecRelayMapsRemained[nRelayMapIndex].nMapID);
	if (!szMapName)
	{
		mlog("RelayMapBattleStart Fail MapID[%d] \n", (int)pStage->m_vecRelayMapsRemained[nRelayMapIndex].nMapID);
		return;
	}

	pStage->GetStageSetting()->SetMapName(szMapName);

	// 실행한 릴레이맵은 삭제해준다.
	vector<RelayMap>::iterator itor = pStage->m_vecRelayMapsRemained.begin();
	for(int i=0 ; nRelayMapIndex > i ; ++itor, ++i);// 해당 인덱스까지 이동
	pStage->m_vecRelayMapsRemained.erase(itor);
}

void MMatchServer::OnStageRelayMapElementUpdate(const MUID& uidStage, int nType, int nRepeatCount)
{
	MMatchStage* pStage = FindStage(uidStage);
	pStage->SetRelayMapType((RELAY_MAP_TYPE)nType);
	pStage->SetRelayMapRepeatCount((RELAY_MAP_REPEAT_COUNT)nRepeatCount);

	MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_RELAY_MAP_ELEMENT_UPDATE), MUID(0,0), m_This);
	pNew->AddParameter(new MCommandParameterUID(uidStage));
	pNew->AddParameter(new MCommandParameterInt((int)pStage->GetRelayMapType()));
	pNew->AddParameter(new MCommandParameterInt((int)pStage->GetRelayMapRepeatCount()));
	RouteToStage(uidStage, pNew);
}

void MMatchServer::OnStageRelayMapListUpdate(const MUID& uidStage, int nRelayMapType, int nRelayMapRepeatCount, void* pRelayMapListBlob)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;
	if(!pStage->IsRelayMap()) return;
	if (pStage->GetState() != STAGE_STATE_STANDBY) return;	// 대기상태에서만 바꿀수 있다

	// 릴레이맵 정보를 서버쪽 스테이지를 갱신
	RelayMap relayMapList[MAX_RELAYMAP_LIST_COUNT];
	for (int i = 0; i < MAX_RELAYMAP_LIST_COUNT; i++)
		relayMapList[i].nMapID = -1;
	int nRelayMapListCount = MGetBlobArrayCount(pRelayMapListBlob);
	if(nRelayMapListCount > MAX_RELAYMAP_LIST_COUNT)
		nRelayMapListCount = MAX_RELAYMAP_LIST_COUNT;
	for (int i = 0; i < nRelayMapListCount; i++)
	{
		MTD_RelayMap* pRelayMap = (MTD_RelayMap*)MGetBlobArrayElement(pRelayMapListBlob, i);
		if(!MGetMapDescMgr()->MIsCorrectMap(pRelayMap->nMapID))
		{
			mlog("OnStageRelayMapListUpdate Fail MIsCorrectMap ID[%d] \n", (int)pRelayMap->nMapID);
			break;
		}
		relayMapList[i].nMapID = pRelayMap->nMapID;
	}

	pStage->SetRelayMapType((RELAY_MAP_TYPE)nRelayMapType);
	pStage->SetRelayMapRepeatCount((RELAY_MAP_REPEAT_COUNT)nRelayMapRepeatCount);
	pStage->SetRelayMapList(relayMapList);
	pStage->InitCurrRelayMap();


	// 블럭 만들기, 맵리스트 세팅
	void* pRelayMapListBlob = MMakeBlobArray(sizeof(MTD_RelayMap), pStage->GetRelayMapListCount());
	RelayMap RelayMapList[MAX_RELAYMAP_LIST_COUNT];
	memcpy(RelayMapList, pStage->GetRelayMapList(), sizeof(RelayMap)*MAX_RELAYMAP_LIST_COUNT);
	for (int i = 0; i < pStage->GetRelayMapListCount(); i++)
	{
		MTD_RelayMap* pRelayMapList = (MTD_RelayMap*)MGetBlobArrayElement(pRelayMapListBlob, i);
		pRelayMapList->nMapID = RelayMapList[i].nMapID;
	}

	// 방장이 보낸 릴레이맵 정보를 방원들에게 보냄
	MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_RELAY_MAP_INFO_UPDATE), MUID(0,0), m_This);
	pNew->AddParameter(new MCommandParameterUID(uidStage));
	pNew->AddParameter(new MCommandParameterInt((int)pStage->GetRelayMapType()));
	pNew->AddParameter(new MCommandParameterInt((int)pStage->GetRelayMapRepeatCount()));
	pNew->AddParameter(new MCommandParameterBlob(pRelayMapListBlob, MGetBlobArraySize(pRelayMapListBlob)));
	RouteToStage(uidStage, pNew);
}
void MMatchServer::OnStageRelayMapListInfo(const MUID& uidStage, const MUID& uidChar)
{
	MMatchStage* pStage = FindStage(uidStage);
	if(pStage == NULL) return;
	if(!pStage->IsRelayMap()) return;
	MMatchObject* pObj = GetObject(uidChar);
	if (pObj == NULL) return;
	// 대기상태일때 방장은 처리 안해줌(릴레이맵 작성중일수도 있음)
	if(pStage->GetState() == STAGE_STATE_STANDBY && pStage->GetMasterUID() == uidChar) return;	

	// 블럭 만들기, 맵리스트 세팅
	void* pRelayMapListBlob = MMakeBlobArray(sizeof(MTD_RelayMap), pStage->GetRelayMapListCount());
	RelayMap RelayMapList[MAX_RELAYMAP_LIST_COUNT];
	memcpy(RelayMapList, pStage->GetRelayMapList(), sizeof(RelayMap)*MAX_RELAYMAP_LIST_COUNT);
	for (int i = 0; i < pStage->GetRelayMapListCount(); i++)
	{
		MTD_RelayMap* pRelayMapList = (MTD_RelayMap*)MGetBlobArrayElement(pRelayMapListBlob, i);
		pRelayMapList->nMapID = RelayMapList[i].nMapID;
	}
	MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_RELAY_MAP_INFO_UPDATE), MUID(0,0), m_This);
	pNew->AddParameter(new MCommandParameterUID(uidStage));
	pNew->AddParameter(new MCommandParameterInt((int)pStage->GetRelayMapType()));
	pNew->AddParameter(new MCommandParameterInt((int)pStage->GetRelayMapRepeatCount()));
	pNew->AddParameter(new MCommandParameterBlob(pRelayMapListBlob, MGetBlobArraySize(pRelayMapListBlob)));
	MEraseBlobArray(pRelayMapListBlob);

	RouteToListener(pObj, pNew); // 방장이 릴레이맵 설정중에 업데이트된 설정으로 변경 될수가 있음
}

void MMatchServer::OnStageSetting(const MUID& uidPlayer, const MUID& uidStage, void* pStageBlob, int nStageCount)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;
	if (pStage->GetState() != STAGE_STATE_STANDBY) return;	// 대기상태에서만 바꿀수 있다
	if (nStageCount <= 0) return;

	// validate
	MMatchObject* pObj = GetObject(uidPlayer);
	if (!IsEnabledObject(pObj)) {
		mlog(" stage setting invalid object (%d, %d) ignore\n", uidPlayer.High, uidPlayer.Low);
		return;
	}

	if( pObj->GetStageUID()!=uidStage ||  nStageCount!=1 ||
		MGetBlobArraySize(pStageBlob) != (sizeof(MSTAGE_SETTING_NODE)+sizeof(int)*2) )
	{
		mlog(" stage setting hack %s (%d, %d) ignore\n", pObj->GetName(), uidPlayer.High, uidPlayer.Low);
		LogObjectCommandHistory( uidPlayer );
		return;
	}

	// 방장이거나 운영자가 아닌데 세팅을 바꾸면 그냥 리턴
	if (pStage->GetMasterUID() != uidPlayer)
	{
		MMatchObject* pObjMaster = GetObject(uidPlayer);
		if (!IsAdminGrade(pObjMaster)) return;
	}


	MSTAGE_SETTING_NODE* pNode = (MSTAGE_SETTING_NODE*)MGetBlobArrayElement(pStageBlob, 0);

	// let's refactor
	if( (pNode->nGameType < MMATCH_GAMETYPE_DEATHMATCH_SOLO) || (pNode->nGameType >= MMATCH_GAMETYPE_MAX)) {
		mlog(" stage setting game mode hack %s (%d, %d) ignore\n", pObj->GetName(), uidPlayer.High, uidPlayer.Low);
		LogObjectCommandHistory( uidPlayer );

		// 디비에 남기자.
//		pObj->SetInvalidStageSettingDisconnectWaitInfo();
		pObj->DisconnectHacker( MMHT_INVALIDSTAGESETTING );

		return;
	}

	// 서바이벌이 비활성 세팅인데 서바이벌 요청시
	if( MGetServerConfig()->IsEnabledSurvivalMode()==false && pNode->nGameType==MMATCH_GAMETYPE_SURVIVAL) {
		mlog(" stage setting game mode hack %s (%d, %d) ignore\n", pObj->GetName(), uidPlayer.High, uidPlayer.Low);
		LogObjectCommandHistory( uidPlayer );
		pObj->DisconnectHacker( MMHT_INVALIDSTAGESETTING );
		return;
	}

	// 기본적으로 최대 인원이 STAGE_BASIC_MAX_PLAYERCOUNT이 넘으면 STAGE_BASIC_MAX_PLAYERCOUNT로 맞춰줌.
	// 남은 작업을 진행하면은 각 게임에 맞는 인원으로 셋팅을 함. - by SungE 2007-05-14
	if( STAGE_MAX_PLAYERCOUNT < pNode->nMaxPlayers )
		pNode->nMaxPlayers = STAGE_MAX_PLAYERCOUNT;

	// 이 이상의 라운드 셋팅은 불가능 하다. 무조건 보정한다. - By SungE 2007-11-07
	if( STAGE__MAX_ROUND < pNode->nRoundMax )
		pNode->nRoundMax = STAGE__MAX_ROUND;

	MMatchStageSetting* pSetting = pStage->GetStageSetting();
	MMatchChannel* pChannel = FindChannel(pStage->GetOwnerChannel());

	bool bCheckChannelRule = true;

	if (QuestTestServer())
	{
		if (MGetGameTypeMgr()->IsQuestDerived(pNode->nGameType))
		{
			bCheckChannelRule = false;
		}
	}

	if ((pChannel) && (bCheckChannelRule))
	{
		// 세팅할 수 있는 맵, 게임타입인지 체크
		MChannelRule* pRule = MGetChannelRuleMgr()->GetRule(pChannel->GetRuleType());
		if (pRule)
		{
			if (!pRule->CheckGameType(pNode->nGameType))
			{
				pNode->nGameType = MMATCH_GAMETYPE_DEATHMATCH_SOLO;
			}

			bool bDuelMode = false;
			bool bCTFMode = false;
			if ( pNode->nGameType == MMATCH_GAMETYPE_DUEL)
				bDuelMode = true;

			if ( pNode->nGameType == MMATCH_GAMETYPE_CTF)
				bCTFMode = true;


			if (!pRule->CheckCTFMap(pNode->nMapIndex) && bCTFMode)
			{
				strcpy(pNode->szMapName, MGetMapDescMgr()->GetMapName(MMATCH_MAP_MANSION));
				pNode->nMapIndex = 0;
			}
			if (!pRule->CheckMap(pNode->nMapIndex, bDuelMode) && !bCTFMode)
			{
				strcpy(pNode->szMapName, MGetMapDescMgr()->GetMapName(MMATCH_MAP_MANSION));
				pNode->nMapIndex = 0;
			}
			else if(!bCTFMode)
			{
				strcpy(pNode->szMapName, pSetting->GetMapName());
				pNode->nMapIndex = pSetting->GetMapIndex();
			}
		}
	}

	MMATCH_GAMETYPE nLastGameType = pSetting->GetGameType();

	// 퀘스트 모드이면 무조건 난입불가, 최대인원 4명으로 세팅한다.
	if (MGetGameTypeMgr()->IsQuestDerived(pNode->nGameType))
	{
		if (pNode->bForcedEntryEnabled == true) pNode->bForcedEntryEnabled = false;
		pNode->nMaxPlayers = STAGE_QUEST_MAX_PLAYER;
		pNode->nLimitTime = STAGESETTING_LIMITTIME_UNLIMITED;


		// 퀘스트 서버가 아닌데 퀘스트 게임이면 솔로데스매치로 바꾼다.
		if (!QuestTestServer())
		{
			pNode->nGameType = MMATCH_GAMETYPE_DEATHMATCH_SOLO;
		}
	}

	// 퀘스트 모드였다가 다른 모드가 되면 '난입불가'를 허용으로 변경
	if (MGetGameTypeMgr()->IsQuestDerived( nLastGameType ) == true &&
		MGetGameTypeMgr()->IsQuestDerived( pNode->nGameType ) == false)
		pNode->bForcedEntryEnabled = true;

	if (!MGetGameTypeMgr()->IsTeamGame(pNode->nGameType))
	{
		pNode->bAutoTeamBalancing = true;
	}

	// 릴레이맵 세팅
	pStage->SetIsRelayMap(strcmp(MMATCH_MAPNAME_RELAYMAP, pNode->szMapName) == 0);
	pStage->SetIsStartRelayMap(false);

	if(!pStage->IsRelayMap())
	{	// 릴레이맵이 아니면 기본으로 초기화 해준다.
		pNode->bIsRelayMap = pStage->IsRelayMap();
		pNode->bIsStartRelayMap = pStage->IsStartRelayMap();
		for (int i=0; i<MAX_RELAYMAP_LIST_COUNT; ++i)
			pNode->MapList[i].nMapID = -1;
		pNode->nRelayMapListCount = 0;
		pNode->nRelayMapType = RELAY_MAP_TURN;
		pNode->nRelayMapRepeatCount = RELAY_MAP_3REPEAT;
	}

	if(pNode->bAntiLead) {
		pSetting->SetAntiLead(true);
	} else {
		pSetting->SetAntiLead(true);
	}


	pSetting->UpdateStageSetting(pNode);
	pStage->ChangeRule(pNode->nGameType);


	MCommand* pCmd = CreateCmdResponseStageSetting(uidStage);
	RouteToStage(uidStage, pCmd);


	// 게임 모드가 변경되었을경우
	if (nLastGameType != pSetting->GetGameType())
	{
		char szNewMap[ MAPNAME_LENGTH ] = {0};

		if (MGetGameTypeMgr()->IsQuestDerived( nLastGameType ) == false &&
			MGetGameTypeMgr()->IsQuestDerived( pSetting->GetGameType() ) == true)
		{
//			OnStageMap(uidStage, GetQuest()->GetSurvivalMapInfo(MSURVIVAL_MAP(0))->szName);
//			OnStageMap(uidStage, pSetting->GetMapName());
			OnStageMap(uidStage, MMATCH_DEFAULT_STAGESETTING_MAPNAME);

			MMatchRuleBaseQuest* pQuest = reinterpret_cast< MMatchRuleBaseQuest* >( pStage->GetRule());
			pQuest->RefreshStageGameInfo();
		}
		else if ( (nLastGameType != MMATCH_GAMETYPE_DUEL) && ( pSetting->GetGameType() == MMATCH_GAMETYPE_DUEL))
		{
			strcpy( szNewMap, MGetMapDescMgr()->GetMapName( MMATCH_MAP_HALL));
			OnStageMap(uidStage, szNewMap);
		}
		else if ( ((nLastGameType == MMATCH_GAMETYPE_QUEST) || (nLastGameType == MMATCH_GAMETYPE_SURVIVAL) || (nLastGameType == MMATCH_GAMETYPE_DUEL)) &&
			      ((pSetting->GetGameType() != MMATCH_GAMETYPE_QUEST) && (pSetting->GetGameType() != MMATCH_GAMETYPE_SURVIVAL) && ( pSetting->GetGameType() != MMATCH_GAMETYPE_DUEL)))
		{
			strcpy( szNewMap, MGetMapDescMgr()->GetMapName( MMATCH_MAP_MANSION));
			OnStageMap(uidStage, szNewMap);
		}
	}
}

void MMatchServer::OnRequestStageSetting(const MUID& uidComm, const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;

	MCommand* pCmd = CreateCmdResponseStageSetting(uidStage);
	pCmd->m_Receiver = uidComm;
	Post(pCmd);

	// 맵 선택이 릴레이맵이면 처리해준다.
	OnStageRelayMapListInfo(uidStage, uidComm);

	MMatchObject* pChar = GetObject(uidComm);
	if (pChar && (MMUG_GM == pChar->GetAccountInfo()->m_nUGrade) ||
		pChar && (MMUG_DEVELOPER == pChar->GetAccountInfo()->m_nUGrade) || 
		pChar && (MMUG_ADMIN == pChar->GetAccountInfo()->m_nUGrade)
		) 	{
		// 이벤트 마스터에게 처음 방만들었던 사람을 알려준다
		StageShowInfo(this, uidComm, uidStage, "/showinfo");
	}
}

void MMatchServer::OnRequestPeerList(const MUID& uidChar, const MUID& uidStage)
{
	ResponsePeerList(uidChar, uidStage);
}

void MMatchServer::OnRequestGameInfo(const MUID& uidChar, const MUID& uidStage)
{
	ResponseGameInfo(uidChar, uidStage);
}

void MMatchServer::ResponseGameInfo(const MUID& uidChar, const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage); if (pStage == NULL) return;
	MMatchObject* pObj = GetObject(uidChar); if (pObj == NULL) return;
	if (pStage->GetRule() == NULL) return;

	MCommand* pNew = CreateCommand(MC_MATCH_RESPONSE_GAME_INFO, MUID(0,0));
	pNew->AddParameter(new MCommandParameterUID(pStage->GetUID()));

	// 게임정보
	void* pGameInfoArray = MMakeBlobArray(sizeof(MTD_GameInfo), 1);
	MTD_GameInfo* pGameItem = (MTD_GameInfo*)MGetBlobArrayElement(pGameInfoArray, 0);
	memset(pGameItem, 0, sizeof(MTD_GameInfo));
	
	if (pStage->GetStageSetting()->IsTeamPlay())
	{
		pGameItem->nRedTeamScore = static_cast<char>(pStage->GetTeamScore(MMT_RED));
		pGameItem->nBlueTeamScore = static_cast<char>(pStage->GetTeamScore(MMT_BLUE));

		pGameItem->nRedTeamKills = static_cast<short>(pStage->GetTeamKills(MMT_RED));
		pGameItem->nBlueTeamKills = static_cast<short>(pStage->GetTeamKills(MMT_BLUE));
	}

	pNew->AddParameter(new MCommandParameterBlob(pGameInfoArray, MGetBlobArraySize(pGameInfoArray)));
	MEraseBlobArray(pGameInfoArray);

	// 룰정보
	void* pRuleInfoArray = NULL;
	if (pStage->GetRule())
		pRuleInfoArray = pStage->GetRule()->CreateRuleInfoBlob();
	if (pRuleInfoArray == NULL)
		pRuleInfoArray = MMakeBlobArray(0, 0);
	pNew->AddParameter(new MCommandParameterBlob(pRuleInfoArray, MGetBlobArraySize(pRuleInfoArray)));
	MEraseBlobArray(pRuleInfoArray);

	// Battle에 들어간 사람만 List를 만든다.
	int nPlayerCount = pStage->GetObjInBattleCount();

	void* pPlayerItemArray = MMakeBlobArray(sizeof(MTD_GameInfoPlayerItem), nPlayerCount);
	int nIndex=0;
	for (MUIDRefCache::iterator itor=pStage->GetObjBegin(); itor!=pStage->GetObjEnd(); itor++) 
	{
		MMatchObject* pObj = (MMatchObject*)(*itor).second;
		if (pObj->GetEnterBattle() == false) continue;

		MTD_GameInfoPlayerItem* pPlayerItem = (MTD_GameInfoPlayerItem*)MGetBlobArrayElement(pPlayerItemArray, nIndex++);
		pPlayerItem->uidPlayer = pObj->GetUID();
		pPlayerItem->bAlive = pObj->CheckAlive();
		pPlayerItem->nKillCount = pObj->GetAllRoundKillCount();
		pPlayerItem->nDeathCount = pObj->GetAllRoundDeathCount();
	}
	pNew->AddParameter(new MCommandParameterBlob(pPlayerItemArray, MGetBlobArraySize(pPlayerItemArray)));
	MEraseBlobArray(pPlayerItemArray);

	RouteToListener(pObj, pNew);
}

void MMatchServer::OnMatchLoadingComplete(const MUID& uidPlayer, int nPercent)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;

	MCommand* pCmd = CreateCommand(MC_MATCH_LOADING_COMPLETE, MUID(0,0));
	pCmd->AddParameter(new MCmdParamUID(uidPlayer));
	pCmd->AddParameter(new MCmdParamInt(nPercent));
	RouteToStage(pObj->GetStageUID(), pCmd);	
}


void MMatchServer::OnGameRoundState(const MUID& uidStage, int nState, int nRound)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;

	pStage->RoundStateFromClient(uidStage, nState, nRound);
}


void MMatchServer::OnDuelSetObserver(const MUID& uidChar)
{
	MMatchObject* pObj = GetObject(uidChar);
	if (pObj == NULL) return;

	MCommand* pCmd = CreateCommand(MC_MATCH_SET_OBSERVER, MUID(0,0));
	pCmd->AddParameter(new MCmdParamUID(uidChar));
	RouteToBattle(pObj->GetStageUID(), pCmd);
}

void MMatchServer::OnRequestSpawn(const MUID& uidChar, const MVector& pos, const MVector& dir)
{
	MMatchObject* pObj = GetObject(uidChar);
	if (pObj == NULL) return;

	// Do Not Spawn when AdminHiding
	if (IsAdminGrade(pObj) && pObj->CheckPlayerFlags(MTD_PlayerFlags_AdminHide)) return;


	// 마지막 죽었던 시간과 새로 리스폰을 요청한 시간 사이에 2초 이상의 시간이 있었는지 검사한다.
	DWORD dwTime = timeGetTime() - pObj->GetLastSpawnTime();	
	if ( dwTime < RESPAWN_DELAYTIME_AFTER_DYING_MIN) return;
	pObj->SetLastSpawnTime(timeGetTime());

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;
	if ( (pStage->GetRule()->GetRoundState() != MMATCH_ROUNDSTATE_PREPARE) && (!pObj->IsEnabledRespawnDeathTime(GetTickTime())) )
		 return;

	MMatchRule* pRule = pStage->GetRule();					// 이런 식의 코드는 마음에 안들지만 -_-; 게임타입 보고 예외처리.
	MMATCH_GAMETYPE gameType = pRule->GetGameType();		// 다른 방법 있나요.
	if (gameType == MMATCH_GAMETYPE_DUEL)
	{
		MMatchRuleDuel* pDuel = (MMatchRuleDuel*)pRule;		// RTTI 안써서 dynamic cast는 패스.. 예외처리도 짜증나고 -,.-
		if (uidChar != pDuel->uidChampion && uidChar != pDuel->uidChallenger)
		{
			OnDuelSetObserver(uidChar);
			return;
		}
	}

	pObj->ResetCustomItemUseCount();
	pObj->SetAlive(true);

	MCommand* pCmd = CreateCommand(MC_MATCH_GAME_RESPONSE_SPAWN, MUID(0,0));
	pCmd->AddParameter(new MCmdParamUID(uidChar));
	pCmd->AddParameter(new MCmdParamShortVector(pos.x, pos.y, pos.z));
	pCmd->AddParameter(new MCmdParamShortVector(DirElementToShort(dir.x), DirElementToShort(dir.y), DirElementToShort(dir.z)));
	RouteToBattle(pObj->GetStageUID(), pCmd);
}

void MMatchServer::OnGameRequestTimeSync(const MUID& uidComm, unsigned long nLocalTimeStamp)
{
	MMatchObject* pObj = GetPlayerByCommUID(uidComm);
	if (pObj == NULL) return;

	MMatchTimeSyncInfo* pSync = pObj->GetSyncInfo();
	pSync->Update(GetGlobalClockCount());

	MCommand* pCmd = CreateCommand(MC_MATCH_GAME_RESPONSE_TIMESYNC, MUID(0,0));
	pCmd->AddParameter(new MCmdParamUInt(nLocalTimeStamp));
	pCmd->AddParameter(new MCmdParamUInt(GetGlobalClockCount()));
	RouteToListener(pObj, pCmd);
}

void MMatchServer::OnGameReportTimeSync(const MUID& uidComm, unsigned long nLocalTimeStamp, unsigned int nDataChecksum)
{
	MMatchObject* pObj = GetPlayerByCommUID(uidComm);
	if (pObj == NULL) return;

	pObj->UpdateTickLastPacketRecved();	// Last Packet Timestamp

	if (pObj->GetEnterBattle() == false)
		return;

	//// SpeedHack Test ////
	MMatchTimeSyncInfo* pSync = pObj->GetSyncInfo();
	int nSyncDiff = nLocalTimeStamp - pSync->GetLastSyncClock();
	float fError = static_cast<float>(nSyncDiff) / static_cast<float>(MATCH_CYCLE_CHECK_SPEEDHACK);
	if (fError > 2.f) {	
		pSync->AddFoulCount();
		if (pSync->GetFoulCount() >= 3) {	// 3연속 스피드핵 검출 - 3진아웃

			#ifndef _DEBUG		// 디버그할때는 빼놓았음
				NotifyMessage(pObj->GetUID(), MATCHNOTIFY_GAME_SPEEDHACK);
				StageLeave(pObj->GetUID());//, pObj->GetStageUID());
				Disconnect(pObj->GetUID());
			#endif
			mlog("SPEEDHACK : User='%s', SyncRatio=%f (TimeDiff=%d) \n", 
				pObj->GetName(), fError, nSyncDiff);
			pSync->ResetFoulCount();
		}
	} else {
		pSync->ResetFoulCount();
	}
	pSync->Update(GetGlobalClockCount());

	//// MemoryHack Test ////
	if (nDataChecksum > 0) {	// 서버가 Client MemoryChecksum 모르므로 일단 클라이언트가 신고하는의미로 사용한다.
		NotifyMessage(pObj->GetUID(), MATCHNOTIFY_GAME_MEMORYHACK);
		StageLeave(pObj->GetUID());//, pObj->GetStageUID());
		Disconnect(pObj->GetUID());
		mlog("MEMORYHACK : User='%s', MemoryChecksum=%u \n", pObj->GetName(), nDataChecksum);
	}
}

void MMatchServer::OnUpdateFinishedRound(const MUID& uidStage, const MUID& uidChar, 
						   void* pPeerInfo, void* pKillInfo)
{

}

void MMatchServer::OnRequestForcedEntry(const MUID& uidStage, const MUID& uidChar)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;
	MMatchObject* pObj = GetObject(uidChar);	
	if (pObj == NULL) return;

	pObj->SetForcedEntry(true);

	RouteResponseToListener(pObj, MC_MATCH_STAGE_RESPONSE_FORCED_ENTRY, MOK);
}

void MMatchServer::OnRequestSuicide(const MUID& uidPlayer)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	pStage->ReserveSuicide( uidPlayer, MGetMatchServer()->GetGlobalClockCount() );

	// OnGameKill(uidPlayer, uidPlayer);

	//MCommand* pNew = CreateCommand(MC_MATCH_RESPONSE_SUICIDE, MUID(0,0));
	//int nResult = MOK;
	//pNew->AddParameter(new MCommandParameterInt(nResult));
	//pNew->AddParameter(new MCommandParameterUID(uidPlayer));
	//RouteToBattle(pObj->GetStageUID(), pNew);
}

void MMatchServer::OnRequestObtainWorldItem(const MUID& uidPlayer, const int nItemUID)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	pStage->ObtainWorldItem(pObj, nItemUID);
}

void MMatchServer::OnRequestFlagCap(const MUID& uidPlayer, const int nItemID)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	if(pStage->GetStageSetting())
	{
		if(pStage->GetStageSetting()->GetGameType() == MMATCH_GAMETYPE_CTF)
		{
			MMatchRuleTeamCTF* pRule = (MMatchRuleTeamCTF*)pStage->GetRule();
			if(pRule)
			{
			pRule->OnObtainWorldItem( pObj, nItemID, NULL );
			}
		}
	}
}

void MMatchServer::OnRequestSpawnWorldItem(const MUID& uidPlayer, const int nItemID, const float x, const float y, const float z, float fDropDelayTime)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	if( !pObj->IsHaveCustomItem() )
		return;

	/*
	착용하고 있는 쪽을 알 수 없으므로 두쪽의 합으로 최대 사용할 수 있는 수량을 구한후
	그 한도 안에서 사용할 수 있도록 한다.
	*/
	if( pObj->IncreaseCustomItemUseCount() )
	{
		pStage->RequestSpawnWorldItem(pObj, nItemID, x, y, z, fDropDelayTime);
	}
}

void MMatchServer::OnNotifyThrowTrapItem(const MUID& uidPlayer, const int nItemID)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	if (!pObj->IsEquipCustomItem(nItemID))
		return;

	pStage->OnNotifyThrowTrapItem(uidPlayer, nItemID);
}

void MMatchServer::OnNotifyActivatedTrapItem(const MUID& uidPlayer, const int nItemID, const MVector3& pos)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;
	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	pStage->OnNotifyActivatedTrapItem(uidPlayer, nItemID, pos);
}

float MMatchServer::GetDuelVictoryMultiflier(int nVictorty)
{
	return 1.0f;
}

float MMatchServer::GetDuelPlayersMultiflier(int nPlayerCount)
{
	return 1.0f;
}

void MMatchServer::CalcExpOnGameKill(MMatchStage* pStage, MMatchObject* pAttacker, MMatchObject* pVictim, 
					   int* poutAttackerExp, int* poutVictimExp)
{
	bool bSuicide = false;		// 자살
	if (pAttacker == pVictim) bSuicide = true;		

	MMATCH_GAMETYPE nGameType = pStage->GetStageSetting()->GetGameType();
	float fGameExpRatio = MGetGameTypeMgr()->GetInfo(nGameType)->fGameExpRatio;

	// 게임타입이 Training이면 바로 0리턴
	if (nGameType == MMATCH_GAMETYPE_TRAINING)
	{
		*poutAttackerExp = 0;
		*poutVictimExp = 0;
		return;
	}
	// 게임타입이 버서커일 경우
	else if (nGameType == MMATCH_GAMETYPE_BERSERKER)
	{
		MMatchRuleBerserker* pRuleBerserker = (MMatchRuleBerserker*)pStage->GetRule();

		if (pRuleBerserker->GetBerserker() == pAttacker->GetUID())
		{
			if (pAttacker != pVictim)
			{
				// 버서커는 경험치를 80%만 획득한다.
				fGameExpRatio = fGameExpRatio * 0.8f;
			}
			else
			{
				// 버서커는 자살 또는 피가 줄어 죽는경우 손실 경험치는 없도록 한다.
				fGameExpRatio = 0.0f;
			}
		}
	}
	else if (nGameType == MMATCH_GAMETYPE_DUEL)
	{
		MMatchRuleDuel* pRuleDuel = (MMatchRuleDuel*)pStage->GetRule();
		if (pVictim->GetUID() == pRuleDuel->uidChallenger)
		{
			fGameExpRatio *= GetDuelVictoryMultiflier(pRuleDuel->GetVictory());
		}
		else
		{
			fGameExpRatio *= GetDuelVictoryMultiflier(pRuleDuel->GetVictory()) * GetDuelPlayersMultiflier(pStage->GetPlayers());

		}
//		if (pRuleDuel->GetVictory() <= 1)
//		{
//			fGameExpRatio = fGameExpRatio * GetDuelPlayersMultiflier(pStage->GetPlayers()) * GetDuelVictoryMultiflier()
//		}
	}
	else if (nGameType == MMATCH_GAMETYPE_CTF)
	{
			MMatchRuleTeamCTF* pRuleCTF = (MMatchRuleTeamCTF*)pStage->GetRule();
			if (pAttacker != pVictim)
			{
				// 버서커는 경험치를 80%만 획득한다.
				if(!(pAttacker->GetTeam() == pVictim->GetTeam()) && (pVictim->GetUID() == pRuleCTF->GetBlueCarrier() || pVictim->GetUID() == pRuleCTF->GetRedCarrier()))
				fGameExpRatio = fGameExpRatio * 2;
			}
			else
			{
				// 버서커는 자살 또는 피가 줄어 죽는경우 손실 경험치는 없도록 한다.
				fGameExpRatio = 0.0f;
			}
	}

	// 맵, 게임타입에 대한 경험치 비율 적용
	int nMapIndex = pStage->GetStageSetting()->GetMapIndex();
	if ((nMapIndex >=0) && (nMapIndex < MMATCH_MAP_COUNT))
	{
		float fMapExpRatio = MGetMapDescMgr()->GetExpRatio(nMapIndex);
		fGameExpRatio = fGameExpRatio * fMapExpRatio;
	}

	int nAttackerLevel = pAttacker->GetCharInfo()->m_nLevel;
	int nVictimLevel = pVictim->GetCharInfo()->m_nLevel;

	// 경험치 계산
	int nAttackerExp = (int)(MMatchFormula::GetGettingExp(nAttackerLevel, nVictimLevel) * fGameExpRatio);
	int nVictimExp = (int)(MMatchFormula::CalcPanaltyEXP(nAttackerLevel, nVictimLevel) * fGameExpRatio);


	// 클랜전일 경우는 획득 경험치가 1.5배, 손실경험치 없음
	if ((MGetServerConfig()->GetServerMode() == MSM_CLAN) && (pStage->GetStageType() == MST_LADDER))
	{
		nAttackerExp = (int)((float)nAttackerExp * 1.5f);
		nVictimExp = 0;
	}

	// 고수채널, 초고수채널일 경우에는 경치다운 없음(자살제외)
	MMatchChannel* pOwnerChannel = FindChannel(pStage->GetOwnerChannel());
	if ((pOwnerChannel) && (!bSuicide))
	{
		if ((pOwnerChannel->GetRuleType() == MCHANNEL_RULE_MASTERY) || 
			(pOwnerChannel->GetRuleType() == MCHANNEL_RULE_ELITE) ||
			(pOwnerChannel->GetRuleType() == MCHANNEL_RULE_CHAMPION))
		{
			nVictimExp=0;
		}
	}

	// 죽은사람이 운영자, 개발자일 경우 경험치 두배
	if ((pVictim->GetAccountInfo()->m_nUGrade == MMUG_ADMIN) || 
		(pVictim->GetAccountInfo()->m_nUGrade == MMUG_DEVELOPER))
	{
		nAttackerExp = nAttackerExp * 2;
	}
	// 죽인사람이 운영자, 개발자일 경우 경치다운 없음
	if ((!bSuicide) &&
		((pAttacker->GetAccountInfo()->m_nUGrade == MMUG_ADMIN) || 
		(pAttacker->GetAccountInfo()->m_nUGrade == MMUG_DEVELOPER)))
	{
		nVictimExp = 0;
	}

	// 자살일 경우 경험치 손실이 두배
	if (bSuicide) 
	{
		nVictimExp = (int)(MMatchFormula::GetSuicidePanaltyEXP(nVictimLevel) * fGameExpRatio);
		nAttackerExp = 0;
	}

	// 팀킬인경우 경험치 제로
	if ((pStage->GetStageSetting()->IsTeamPlay()) && (pAttacker->GetTeam() == pVictim->GetTeam()))
	{
		nAttackerExp = 0;
	}


	// 팀전일 경우 경험치 배분
	if (pStage->IsApplyTeamBonus())
	{
		int nTeamBonus = 0;
		if (pStage->GetRule() != NULL)
		{
			int nNewAttackerExp = nAttackerExp;
			pStage->GetRule()->CalcTeamBonus(pAttacker, pVictim, nAttackerExp, &nNewAttackerExp, &nTeamBonus);
			nAttackerExp = nNewAttackerExp;
		}

		// 팀 경험치 적립
		pStage->AddTeamBonus(nTeamBonus, MMatchTeam(pAttacker->GetTeam()));
	}

	// xp 보너스 적용(넷마블 PC방, 경험치 반지)
	int nAttackerExpBonus = 0;
	if (nAttackerExp != 0)
	{
		//const float ta = float(atoi("15")) / 100.0f;
		//mlog( "test float : %f\n", ta * 100.0f );

		//MMatchItemBonusType nBonusType			= GetStageBonusType(pStage->GetStageSetting());
		//const double		dAttackerExp		= static_cast< double >( nAttackerExp );
		//const double		dXPBonusRatio		= static_cast< double >( MMatchFormula::CalcXPBonusRatio(pAttacker, nBonusType) );
		//const double		dAttackerExpBouns	= dAttackerExp * dXPBonusRatio;
		//const double		dSumAttackerExp		= dAttackerExp + dAttackerExpBouns;
		//
		//
		//nAttackerExpBonus = static_cast< int >( dAttackerExpBouns + 0.00001); 

		MMatchItemBonusType nBonusType = GetStageBonusType(pStage->GetStageSetting());
		const float fAttackerExpBonusRatio = MMatchFormula::CalcXPBonusRatio(pAttacker, nBonusType);
		 //부동소수점 오차때문에 계산에 영향을 주지 않는 범위에서 보정을 해준다.
		// 만약 이부분에서 다시 문제가 발생한다면 보정이 아닌 베이스부터 수정 작업을 해 줘야 한다.
		 nAttackerExpBonus = (int)(nAttackerExp * (fAttackerExpBonusRatio + 0.00001f));
	}

	*poutAttackerExp = nAttackerExp + nAttackerExpBonus;

	*poutVictimExp = nVictimExp;
}


const int MMatchServer::CalcBPonGameKill( MMatchStage* pStage, MMatchObject* pAttacker, const int nAttackerLevel, const int nVictimLevel )
{
	if( (0 == pStage) || (0 == pAttacker) ) 
		return -1;

	const int	nAddedBP		= static_cast< int >( MMatchFormula::GetGettingBounty(nAttackerLevel, nVictimLevel) );
	const float fBPBonusRatio	= MMatchFormula::CalcBPBounsRatio( pAttacker, GetStageBonusType(pStage->GetStageSetting()) );
	const int	nBPBonus		= static_cast< int >( nAddedBP * fBPBonusRatio );

	return nAddedBP + nBPBonus;
}




void MMatchServer::ProcessPlayerXPBP(MMatchStage* pStage, MMatchObject* pPlayer, int nAddedXP, int nAddedBP)
{
	if (pStage == NULL) return;
	if (!IsEnabledObject(pPlayer)) return;

	/*
		경험치 계산
		캐릭터에 경험치 적용
		레벨 계산
		DB캐싱 업데이트
		레벨업,다운 메세지 전송
	*/

	MUID uidStage = pPlayer->GetStageUID();
	int nPlayerLevel = pPlayer->GetCharInfo()->m_nLevel;

	// 캐릭터 XP 업데이트
	pPlayer->GetCharInfo()->IncXP(nAddedXP);

	// 레벨 계산
	int nNewPlayerLevel = -1;
	if ((pPlayer->GetCharInfo()->m_nLevel < MAX_LEVEL) &&
		(pPlayer->GetCharInfo()->m_nXP >= MMatchFormula::GetNeedExp(nPlayerLevel)))
	{
		nNewPlayerLevel = MMatchFormula::GetLevelFromExp(pPlayer->GetCharInfo()->m_nXP);
		if (nNewPlayerLevel != pPlayer->GetCharInfo()->m_nLevel) pPlayer->GetCharInfo()->m_nLevel = nNewPlayerLevel;
	}

	// 바운티 추가해준다
	pPlayer->GetCharInfo()->IncBP(nAddedBP);


	// DB 캐슁 업데이트
	if (pPlayer->GetCharInfo()->GetDBCachingData()->IsRequestUpdate()) {
		UpdateCharDBCachingData(pPlayer);		///< XP, BP, KillCount, DeathCount 캐슁 업데이트
	}

	// 만약 레벨이 바뀌면 따로 레벨업한다.
	if ((nNewPlayerLevel >= 0) && (nNewPlayerLevel != nPlayerLevel))
	{
		// 레벨이 바뀌면 바로 캐슁 업데이트한다
		UpdateCharDBCachingData(pPlayer);

		pPlayer->GetCharInfo()->m_nLevel = nNewPlayerLevel;
		if (!m_MatchDBMgr.UpdateCharLevel(pPlayer->GetCharInfo()->m_nCID, 
										  nNewPlayerLevel, 
										  pPlayer->GetCharInfo()->m_nBP,
										  pPlayer->GetCharInfo()->m_nTotalKillCount, 
										  pPlayer->GetCharInfo()->m_nTotalDeathCount,
										  pPlayer->GetCharInfo()->m_nTotalPlayTimeSec,
										  true))
		{
			mlog("DB UpdateCharLevel Error : %s\n", pPlayer->GetCharInfo()->m_szName);
		}
	}

	// 레벨업, 레벨 다운 메세지 보내기
	if (nNewPlayerLevel > 0)
	{
		if (nNewPlayerLevel > nPlayerLevel)
		{
			MCommand* pCmd = CreateCommand(MC_MATCH_GAME_LEVEL_UP, MUID(0,0));
			pCmd->AddParameter(new MCommandParameterUID(pPlayer->GetUID()));
			pCmd->AddParameter(new MCommandParameterInt(nNewPlayerLevel));
			RouteToBattle(uidStage, pCmd);	
		}
		else if (nNewPlayerLevel < nPlayerLevel)
		{
			MCommand* pCmd = CreateCommand(MC_MATCH_GAME_LEVEL_DOWN, MUID(0,0));
			pCmd->AddParameter(new MCommandParameterUID(pPlayer->GetUID()));
			pCmd->AddParameter(new MCommandParameterInt(nNewPlayerLevel));
			RouteToBattle(uidStage, pCmd);	
		}
	}
}

// 팀 보너스 적용
void MMatchServer::ApplyObjectTeamBonus(MMatchObject* pObject, int nAddedExp)
{
	if (!IsEnabledObject(pObject)) return;
	if (nAddedExp <= 0)
	{
		//_ASSERT(0);
		return;
	}
	
	bool bIsLevelUp = false;

	// 보너스 적용
	if (nAddedExp != 0)
	{
		int nExpBonus = (int)(nAddedExp * MMatchFormula::CalcXPBonusRatio(pObject, MIBT_TEAM));
		nAddedExp += nExpBonus;
	}




	// 캐릭터 XP 업데이트
	pObject->GetCharInfo()->IncXP(nAddedExp);

	// 레벨 계산
	int nNewLevel = -1;
	int nCurrLevel = pObject->GetCharInfo()->m_nLevel;

	if (nNewLevel > nCurrLevel) bIsLevelUp = true;

	if ((pObject->GetCharInfo()->m_nLevel < MAX_LEVEL) &&
		(pObject->GetCharInfo()->m_nXP >= MMatchFormula::GetNeedExp(nCurrLevel)))
	{
		nNewLevel = MMatchFormula::GetLevelFromExp(pObject->GetCharInfo()->m_nXP);
		if (nNewLevel != nCurrLevel) pObject->GetCharInfo()->m_nLevel = nNewLevel;
	}

	// DB 캐슁 업데이트
	if (pObject->GetCharInfo()->GetDBCachingData()->IsRequestUpdate())
	{
		UpdateCharDBCachingData(pObject);
	}

	// 만약 레벨이 바뀌면 바로 레벨업한다.
	if ((nNewLevel >= 0) && (nNewLevel != nCurrLevel))
	{
		// 레벨이 바뀌면 바로 캐슁 업데이트한다
		UpdateCharDBCachingData(pObject);

		pObject->GetCharInfo()->m_nLevel = nNewLevel;
		nCurrLevel = nNewLevel;

		if (!m_MatchDBMgr.UpdateCharLevel(pObject->GetCharInfo()->m_nCID, 
			                              nNewLevel,
										  pObject->GetCharInfo()->m_nBP,
										  pObject->GetCharInfo()->m_nTotalKillCount,
										  pObject->GetCharInfo()->m_nTotalDeathCount,
										  pObject->GetCharInfo()->m_nTotalPlayTimeSec,
										  bIsLevelUp
										  ))
		{
			mlog("DB UpdateCharLevel Error : %s\n", pObject->GetCharInfo()->m_szName);
		}
	}


	MUID uidStage = pObject->GetStageUID();

	unsigned long int nExpArg;
	unsigned long int nChrExp;
	int nPercent;

	nChrExp = pObject->GetCharInfo()->m_nXP;
	nPercent = MMatchFormula::GetLevelPercent(nChrExp, nCurrLevel);
	// 상위 2바이트는 경험치, 하위 2바이트는 경험치의 퍼센트이다.
	nExpArg = MakeExpTransData(nAddedExp, nPercent);


	MCommand* pCmd = CreateCommand(MC_MATCH_GAME_TEAMBONUS, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterUID(pObject->GetUID()));
	pCmd->AddParameter(new MCommandParameterUInt(nExpArg));
	RouteToBattle(uidStage, pCmd);	


	// 레벨업 메세지 보내기
	if ((nNewLevel >= 0) && (nNewLevel > nCurrLevel))
	{
		MCommand* pCmd = CreateCommand(MC_MATCH_GAME_LEVEL_UP, MUID(0,0));
		pCmd->AddParameter(new MCommandParameterUID(pObject->GetUID()));
		pCmd->AddParameter(new MCommandParameterInt(nNewLevel));
		RouteToBattle(uidStage, pCmd);	
	}
}

// 플레이 중 캐릭터 정보 업데이트
void MMatchServer::ProcessCharPlayInfo(MMatchObject* pPlayer)
{
	if (!IsEnabledObject(pPlayer)) return;

	/*
	원할때마다 캐릭터 정보를 업데이트 적용
	경험치 계산
	캐릭터에 경험치 적용
	레벨 계산
	레벨업,다운 메세지 전송
	바운티 추가해준다
	접속시간, 게임 진행시간, 플레이 시간
	*/

	MUID uidStage = pPlayer->GetStageUID();
	int nPlayerLevel = pPlayer->GetCharInfo()->m_nLevel;

	// 레벨 계산
	int nNewPlayerLevel = -1;
	if ((pPlayer->GetCharInfo()->m_nLevel < MAX_LEVEL) &&
		(pPlayer->GetCharInfo()->m_nXP >= MMatchFormula::GetNeedExp(nPlayerLevel)))
	{
		nNewPlayerLevel = MMatchFormula::GetLevelFromExp(pPlayer->GetCharInfo()->m_nXP);
		if (nNewPlayerLevel != pPlayer->GetCharInfo()->m_nLevel) pPlayer->GetCharInfo()->m_nLevel = nNewPlayerLevel;
	}
	// 만약 레벨이 바뀌면 따로 레벨업한다.
	if ((nNewPlayerLevel >= 0) && (nNewPlayerLevel != nPlayerLevel))
		pPlayer->GetCharInfo()->m_nLevel = nNewPlayerLevel;

	// 접속시간, 게임 진행시간, 플레이 시간
	unsigned long int nNowTime = MMatchServer::GetInstance()->GetGlobalClockCount();
	unsigned long int nBattlePlayingTimeSec = 0;
	if(pPlayer->GetCharInfo()->m_nBattleStartTime != 0)
	{
		nBattlePlayingTimeSec = MGetTimeDistance(pPlayer->GetCharInfo()->m_nBattleStartTime, nNowTime) / 1000;	// 배틀을 진행한 시간
		
		/*
		// 아무 처리도 하지 않는데, 왜 로그는 남기는건가요? 일단 주석 처리합니다. - carrot318
		if(nBattlePlayingTimeSec > 60*60)
		{// 이상적으로 값이 세팅돼면 로그를 남긴다.
			CTime theTime = CTime::GetCurrentTime();
			CString szTime = theTime.Format( "[%c] " );

			// 게임 모드
			char buf[64]={0,};
			MMatchStage* pStage = FindStage(uidStage);

			if( pStage != NULL )
			{
				switch((int)pStage->GetStageSetting()->GetGameType())
				{
				case MMATCH_GAMETYPE_DEATHMATCH_SOLO:	{sprintf(buf, "DEATHMATCH_SOLO");	} break;		///< 개인 데쓰매치
				case MMATCH_GAMETYPE_DEATHMATCH_TEAM:	{sprintf(buf, "DEATHMATCH_TEAM");	} break;		///< 팀 데쓰매치
				case MMATCH_GAMETYPE_GLADIATOR_SOLO:	{sprintf(buf, "GLADIATOR_SOLO");	} break;		///< 개인 글래디에이터
				case MMATCH_GAMETYPE_GLADIATOR_TEAM:	{sprintf(buf, "GLADIATOR_TEAM");	} break;		///< 팀 글래디에이터
				case MMATCH_GAMETYPE_ASSASSINATE:		{sprintf(buf, "ASSASSINATE");		} break;		///< 보스전
				case MMATCH_GAMETYPE_TRAINING:			{sprintf(buf, "TRAINING");			} break;		///< 연습

				case MMATCH_GAMETYPE_SURVIVAL:			{sprintf(buf, "SURVIVAL");			} break;		///< 서바이벌
				case MMATCH_GAMETYPE_QUEST:				{sprintf(buf, "QUEST");				} break;		///< 퀘스트

				case MMATCH_GAMETYPE_BERSERKER:			{sprintf(buf, "BERSERKER");			} break;		
				case MMATCH_GAMETYPE_DEATHMATCH_TEAM2:	{sprintf(buf, "DEATHMATCH_TEAM2");	} break;		
				case MMATCH_GAMETYPE_DUEL:				{sprintf(buf, "DUEL");				} break;	
				default:								{sprintf(buf, "don't know");		} break;
				}
				mlog("%s BattlePlayT Error GameMode:%s, CID:%d, Name:%s, ServerCurrT:%u, BattleStartT:%u, PlayT:%d, PlayerConnectT:%u \n"
					, szTime, buf, pPlayer->GetCharInfo()->m_nCID, pPlayer->GetCharInfo()->m_szName, nNowTime, pPlayer->GetCharInfo()->m_nBattleStartTime, nBattlePlayingTimeSec, pPlayer->GetCharInfo()->m_nConnTime);
			}
		}
		*/
		//pPlayer->GetCharInfo()->m_nBattleStartTime = 0;
	}
	unsigned long int nLoginTotalTimeSec = MGetTimeDistance(pPlayer->GetCharInfo()->m_nConnTime, nNowTime) / 1000;	// 게임을 진행한 시간

	// 이상적으로 경험치가 획득했으면 로그를 남겨준다.
	// 아무 처리도 하지 않는데, 왜 로그는 남기는건가요? 일단 주석 처리합니다. - carrot318
	/*
	long int nBattleEXPGained = pPlayer->GetCharInfo()->m_nXP - pPlayer->GetCharInfo()->m_nBattleStartXP;
	if(nBattleEXPGained < -150000 || 150000 < nBattleEXPGained)
	{
		CTime theTime = CTime::GetCurrentTime();
		CString szTime = theTime.Format( "[%c] " );
		mlog("%s BattleXPGained Error CID:%d, Name:%s, StartXP:%d, EXPGained:%d \n", szTime, pPlayer->GetCharInfo()->m_nCID, pPlayer->GetCharInfo()->m_szName, pPlayer->GetCharInfo()->m_nBattleStartXP, nBattleEXPGained);
	}
	*/

#ifdef LOCALE_NHNUSA
	if (!m_MatchDBMgr.UpdateCharPlayInfo(pPlayer->GetAccountInfo()->m_nAID
										, pPlayer->GetCharInfo()->m_nCID
										, pPlayer->GetCharInfo()->m_nXP
										, pPlayer->GetCharInfo()->m_nLevel
										, nBattlePlayingTimeSec										// 배틀 시간
										, nLoginTotalTimeSec										// 총게임을 진행한 시간
										, pPlayer->GetCharInfo()->m_nTotalKillCount
										, pPlayer->GetCharInfo()->m_nTotalDeathCount
										, pPlayer->GetCharInfo()->m_nBP
										, false))
	{
		mlog("DB UpdateCharPlayInfo Error : %s\n", pPlayer->GetCharInfo()->m_szName);
	}
#endif

}

void MMatchServer::PostGameDeadOnGameKill(MUID& uidStage, MMatchObject* pAttacker, MMatchObject* pVictim,
									int nAddedAttackerExp, int nSubedVictimExp)
{
	unsigned long int nAttackerArg = 0;
	unsigned long int nVictimArg =0;

	int nRealAttackerLevel = pAttacker->GetCharInfo()->m_nLevel;
	int nRealVictimLevel = pVictim->GetCharInfo()->m_nLevel;

	unsigned long int nChrExp;
	int nPercent;

	nChrExp = pAttacker->GetCharInfo()->m_nXP;
	nPercent = MMatchFormula::GetLevelPercent(nChrExp, nRealAttackerLevel);
	nAttackerArg = MakeExpTransData(nAddedAttackerExp, nPercent);

	nChrExp = pVictim->GetCharInfo()->m_nXP;
	nPercent = MMatchFormula::GetLevelPercent(nChrExp, nRealVictimLevel);
	nVictimArg = MakeExpTransData(nSubedVictimExp, nPercent);

	MCommand* pCmd = CreateCommand(MC_MATCH_GAME_DEAD, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterUID(pAttacker->GetUID()));
	pCmd->AddParameter(new MCommandParameterUInt(nAttackerArg));
	pCmd->AddParameter(new MCommandParameterUID(pVictim->GetUID()));
	pCmd->AddParameter(new MCommandParameterUInt(nVictimArg));
	RouteToBattle(uidStage, pCmd);	
}

void MMatchServer::StageList(const MUID& uidPlayer, int nStageStartIndex, bool bCacheUpdate)
{
	MMatchObject* pChar = GetObject(uidPlayer);
	if (pChar == NULL) return;
	MMatchChannel* pChannel = FindChannel(pChar->GetChannelUID());
	if (pChannel == NULL) return;

	// 클랜서버인데 클랜채널일 경우에는 방 리스트대신 대기중 클랜 리스트를 보낸다.
	if ((MGetServerConfig()->GetServerMode() == MSM_CLAN) && (pChannel->GetChannelType() == MCHANNEL_TYPE_CLAN))
	{
		StandbyClanList(uidPlayer, nStageStartIndex, bCacheUpdate);
		return;
	}


	MCommand* pNew = new MCommand(m_CommandManager.GetCommandDescByID(MC_MATCH_STAGE_LIST), MUID(0,0), m_This);

	int nPrevStageCount = -1, nNextStageCount = -1;
	int nNextStageIndex = pChannel->GetMaxPlayers()-1;


	// 2008.09.16 
	int nRealStageStartIndex = nStageStartIndex;
	int nStageCount = 0;
	for(int i = 0; i < pChannel->GetMaxPlayers(); i++)
	{
		// 방이 비워잇으면 처리안한다
		if (pChannel->IsEmptyStage(i)) continue;
		// 방이 있으면 처리
		if(nStageCount < nStageStartIndex) // 앞에 탭에 처리된 방들 < 현재 탭에서 시작할 방 Index
			nStageCount++;
		else
		{
			nRealStageStartIndex = i;
			break;
		}
	}

	int nRealStageCount = 0;
	for (int i = /*nStageStartIndex*/nRealStageStartIndex; i < pChannel->GetMaxPlayers(); i++)
	{
		if (pChannel->IsEmptyStage(i)) continue;

		MMatchStage* pStage = pChannel->GetStage(i);
		if ((pStage == NULL) || (pStage->GetState() == STAGE_STATE_CLOSE)) continue;

		nRealStageCount++;
		if (nRealStageCount >= TRANS_STAGELIST_NODE_COUNT) 
		{
			nNextStageIndex = i;
			break;
		}
	}

	if (!bCacheUpdate)
	{
		nPrevStageCount = pChannel->GetPrevStageCount(nStageStartIndex);
		nNextStageCount = pChannel->GetNextStageCount(nNextStageIndex);
	}

	pNew->AddParameter(new MCommandParameterChar((char)nPrevStageCount));
	pNew->AddParameter(new MCommandParameterChar((char)nNextStageCount));


	void* pStageArray = MMakeBlobArray(sizeof(MTD_StageListNode), nRealStageCount);
	int nArrayIndex=0;

	for (int i = /*nStageStartIndex*/nRealStageStartIndex; i < pChannel->GetMaxPlayers(); i++)
	{
		if (pChannel->IsEmptyStage(i)) continue;
		MMatchStage* pStage = pChannel->GetStage(i);
		if ((pStage == NULL) || (pStage->GetState() == STAGE_STATE_CLOSE)) continue;
		
		if( pStage->GetState() < STAGE_STATE_STANDBY || pStage->GetState() > STAGE_STATE_COUNT )
		{
			/* 크래쉬로 인해 방어코드 추가. 나중에 pChannel->m_pStages 배열이 아닌 맵이나 다른걸로 리펙토링필요*/
			LOG(LOG_FILE, "there is unavailable stages in %s channel. No:%d \n", pChannel->GetName(), i);
			continue;
		}


		if (nArrayIndex >= nRealStageCount) break;

		MTD_StageListNode* pNode = (MTD_StageListNode*)MGetBlobArrayElement(pStageArray, nArrayIndex++);
		pNode->uidStage = pStage->GetUID();
		strcpy(pNode->szStageName, pStage->GetName());
		pNode->nNo = (unsigned char)(pStage->GetIndex() + 1);	// 사용자에게 보여주는 인덱스는 1부터 시작한다
		pNode->nPlayers = (char)pStage->GetPlayers();
		pNode->nMaxPlayers = pStage->GetStageSetting()->GetMaxPlayers();
		pNode->nState = pStage->GetState();
		pNode->nGameType = pStage->GetStageSetting()->GetGameType();
		
		// 릴레이면 로비 방리스트 배너를 릴레이맵으로 유지해준다.
		if(pStage->IsRelayMap()) pNode->nMapIndex = MMATCH_MAP_RELAYMAP;
		else		 			 pNode->nMapIndex = pStage->GetStageSetting()->GetMapIndex();
		
		pNode->nSettingFlag = 0;
		// 난입
		if (pStage->GetStageSetting()->GetForcedEntry())
		{
			pNode->nSettingFlag |= MSTAGENODE_FLAG_FORCEDENTRY_ENABLED;
		}
		// 비밀방
		if (pStage->IsPrivate())
		{
			pNode->nSettingFlag |= MSTAGENODE_FLAG_PRIVATE;
		}
		// 레벨제한
		pNode->nLimitLevel = pStage->GetStageSetting()->GetLimitLevel();
		pNode->nMasterLevel = 0;

		if (pNode->nLimitLevel != 0)
		{
			pNode->nSettingFlag |= MSTAGENODE_FLAG_LIMITLEVEL;

			;
			MMatchObject* pMaster = GetObject(pStage->GetMasterUID());
			if (pMaster)
			{
				if (pMaster->GetCharInfo())
				{
					pNode->nMasterLevel = pMaster->GetCharInfo()->m_nLevel;
				}
			}
		}
	}

	pNew->AddParameter(new MCommandParameterBlob(pStageArray, MGetBlobArraySize(pStageArray)));
	MEraseBlobArray(pStageArray);

	RouteToListener(pChar, pNew);	
}


void MMatchServer::OnStageRequestStageList(const MUID& uidPlayer, const MUID& uidChannel, const int nStageCursor)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;
	MMatchChannel* pChannel = FindChannel(uidChannel);
	if (pChannel == NULL) return;

	pObj->SetStageCursor(nStageCursor);
	StageList(pObj->GetUID(), nStageCursor, false);
}


void MMatchServer::OnRequestQuickJoin(const MUID& uidPlayer, void* pQuickJoinBlob)
{
	MTD_QuickJoinParam* pNode = (MTD_QuickJoinParam*)MGetBlobArrayElement(pQuickJoinBlob, 0);
	ResponseQuickJoin(uidPlayer, pNode);
}

void MMatchServer::ResponseQuickJoin(const MUID& uidPlayer, MTD_QuickJoinParam* pQuickJoinParam)
{
	if (pQuickJoinParam == NULL) return;

	MMatchObject* pObj = GetObject(uidPlayer);
	if (!IsEnabledObject(pObj)) return;
	MMatchChannel* pChannel = FindChannel(pObj->GetChannelUID());
	if (pChannel == NULL) return;

	list<MUID>	recommended_stage_list;
	MUID uidRecommendedStage = MUID(0,0);
	int nQuickJoinResult = MOK;


	for (int i = 0; i < pChannel->GetMaxStages(); i++)
	{
		if (pChannel->IsEmptyStage(i)) continue;
		MMatchStage* pStage = pChannel->GetStage(i);
		if ((pStage == NULL) || (pStage->GetState() == STAGE_STATE_CLOSE)) continue;

		int ret = ValidateStageJoin(pObj->GetUID(), pStage->GetUID());
		if (ret == MOK)
		{
			if (pStage->IsPrivate()) continue;

			int nMapIndex = pStage->GetStageSetting()->GetMapIndex();
			int nGameType = pStage->GetStageSetting()->GetGameType();

			if (!CheckBitSet(pQuickJoinParam->nMapEnum, nMapIndex)) continue;
			if (!CheckBitSet(pQuickJoinParam->nModeEnum, nGameType)) continue;

			//if (((1 << nMapIndex) & (pQuickJoinParam->nMapEnum)) == 0) continue;
			//if (((1 << nGameType) & (pQuickJoinParam->nModeEnum)) == 0) continue;

			recommended_stage_list.push_back(pStage->GetUID());
		}
	}

	if (!recommended_stage_list.empty())
	{
		int nSize=(int)recommended_stage_list.size();
		int nIndex = rand() % nSize;

		int nCnt = 0;
		for (list<MUID>::iterator itor = recommended_stage_list.begin(); itor != recommended_stage_list.end(); ++itor)
		{
			if (nIndex == nCnt)
			{
				uidRecommendedStage = (*itor);
				break;
			}
			nCnt++;
		}
	}
	else
	{
		nQuickJoinResult = MERR_CANNOT_NO_STAGE;
	}

	MCommand* pCmd = CreateCommand(MC_MATCH_STAGE_RESPONSE_QUICKJOIN, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterInt(nQuickJoinResult));
	pCmd->AddParameter(new MCommandParameterUID(uidRecommendedStage));
	RouteToListener(pObj, pCmd);	
}

static int __cdecl _int_sortfunc(const void* a, const void* b)
{
	return *((int*)a) - *((int*)b);
}


int MMatchServer::GetLadderTeamIDFromDB(const int nTeamTableIndex, const int* pnMemberCIDArray, const int nMemberCount)
{
	if ((nMemberCount <= 0) || (nTeamTableIndex != nMemberCount))
	{
		//_ASSERT(0);
		return 0;
	}

	// cid 오름차순으로 소팅 - db상에 소팅되어 들어가있다. 
	int* pnSortedCIDs = new int[nMemberCount];
	for (int i = 0; i < nMemberCount; i++)
	{
		pnSortedCIDs[i] = pnMemberCIDArray[i];
	}
	qsort(pnSortedCIDs, nMemberCount, sizeof(int), _int_sortfunc);

	int nTID = 0;
	if (pnSortedCIDs[0] != 0)
	{
		if (!m_MatchDBMgr.GetLadderTeamID(nTeamTableIndex, pnSortedCIDs, nMemberCount, &nTID))
		{
			nTID = 0;
		}
	}
	

	delete[] pnSortedCIDs;

	return nTID;
}

void MMatchServer::SaveLadderTeamPointToDB(const int nTeamTableIndex, const int nWinnerTeamID, const int nLoserTeamID, const bool bIsDrawGame)
{
	// 포인트 계산 - 액션리그 전용
	int nWinnerPoint = 0, nLoserPoint = 0, nDrawPoint = 0;

	nLoserPoint = -1;
	switch (nTeamTableIndex)
	{
	case 2:	// 2대2
		{
			nWinnerPoint = 4;
			nDrawPoint = 1;
		}
		break;
	case 3:
		{
			nWinnerPoint = 6;
			nDrawPoint = 1;
		}
		break;
	case 4:
		{
			nWinnerPoint = 10;
			nDrawPoint = 2;
		}
		break;
	}

	if (!m_MatchDBMgr.LadderTeamWinTheGame(nTeamTableIndex, nWinnerTeamID, nLoserTeamID, bIsDrawGame,
		                                   nWinnerPoint, nLoserPoint, nDrawPoint))
	{
		mlog("DB Query(SaveLadderTeamPointToDB) Failed\n");
	}
}


void MMatchServer::OnVoteCallVote(const MUID& uidPlayer, const char* pszDiscuss, const char* pszArg)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;

	// 운영자가 강퇴투표하면 강제로 강퇴
	if (IsAdminGrade(pObj)) {
		MMatchStage* pStage = FindStage(pObj->GetStageUID());
		if (pStage)
			pStage->KickBanPlayer(pszArg, false);
		return;
	}

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	char szMsg[256];
	// 운영자가 같이 게임중이면 투표 불가능
	for (MUIDRefCache::iterator itor = pStage->GetObjBegin(); itor != pStage->GetObjEnd(); itor++) {
		MUID uidObj = (MUID)(*itor).first;
		MMatchObject* pPlayer = (MMatchObject*)GetObject(uidObj);
		if ((pPlayer) && (IsAdminGrade(pPlayer)))
		{
			sprintf(szMsg, "%s%d", MTOK_ANNOUNCE_PARAMSTR, MERR_CANNOT_VOTE);
			Announce(uidPlayer, szMsg);

			return;
		}
	}


	if( pObj->WasCallVote() )
	{
		sprintf(szMsg, "%s%d", MTOK_ANNOUNCE_PARAMSTR, MERR_CANNOT_VOTE);
		Announce(uidPlayer, szMsg);

		return;
	}

	// 투표를 했다는걸 표시해놓음.
	pObj->SetVoteState( true );

	if (pStage->GetStageType() == MST_LADDER)
	{
		sprintf(szMsg, "%s%d", MTOK_ANNOUNCE_PARAMSTR, MERR_CANNOT_VOTE_LADERGAME);
		Announce(uidPlayer, szMsg);

		return;
	}

	if (pStage->GetRule() && pStage->GetRule()->GetGameType() == MMATCH_GAMETYPE_DUELTOURNAMENT)
	{
		sprintf(szMsg, "%s%d", MTOK_ANNOUNCE_PARAMSTR, MERR_CANNOT_VOTE);
		Announce(uidPlayer, szMsg);

		return;
	}
#ifdef _VOTESETTING
	// 방 설정중 투표기능을 검사함.
	if( !pStage->GetStageSetting()->bVoteEnabled ) {
		VoteAbort( uidPlayer );
		return;
	}

	// 이번 게임에서 투표를 건의했는지 검사.
	if( pStage->WasCallVote() ) {
		VoteAbort( uidPlayer );
		return;
	}
	else {
		pStage->SetVoteState( true );
	}
#endif

	if (pStage->GetVoteMgr()->GetDiscuss())
	{
		sprintf(szMsg, "%s%d", MTOK_ANNOUNCE_PARAMSTR, MERR_VOTE_ALREADY_START);
		Announce(uidPlayer, szMsg);

		return;
	}

	MVoteDiscuss* pDiscuss = MVoteDiscussBuilder::Build(uidPlayer, pStage->GetUID(), pszDiscuss, pszArg);
	if (pDiscuss == NULL) return;

	if (pStage->GetVoteMgr()->CallVote(pDiscuss)) {
		pDiscuss->Vote(uidPlayer, MVOTE_YES);	// 발의자 무조건 찬성

		MCommand* pCmd = CreateCommand(MC_MATCH_NOTIFY_CALLVOTE, MUID(0,0));
		pCmd->AddParameter(new MCmdParamStr(pszDiscuss));
		pCmd->AddParameter(new MCmdParamStr(pszArg));
		RouteToStage(pStage->GetUID(), pCmd);
		return;
	}
	else
	{
		sprintf(szMsg, "%s%d", MTOK_ANNOUNCE_PARAMSTR, MERR_VOTE_FAILED);
		Announce(uidPlayer, szMsg);

		return;
	}
}

void MMatchServer::OnVoteYes(const MUID& uidPlayer)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	MVoteDiscuss* pDiscuss = pStage->GetVoteMgr()->GetDiscuss();
    if (pDiscuss == NULL) return;

	pDiscuss->Vote(uidPlayer, MVOTE_YES);
}

void MMatchServer::OnVoteNo(const MUID& uidPlayer)
{
	MMatchObject* pObj = GetObject(uidPlayer);
	if (pObj == NULL) return;

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	MVoteDiscuss* pDiscuss = pStage->GetVoteMgr()->GetDiscuss();
    if (pDiscuss == NULL) return;

	pDiscuss->Vote(uidPlayer, MVOTE_NO);
}

void MMatchServer::VoteAbort( const MUID& uidPlayer )
{
#ifndef MERR_CANNOT_VOTE
#define MERR_CANNOT_VOTE 120000
#endif

	MMatchObject* pObj = GetObject( uidPlayer );
	if( 0 == pObj )
		return;

	MCommand* pCmd = CreateCommand( MC_MATCH_VOTE_RESPONSE, MUID(0, 0) );
	if( 0 == pCmd )
		return;

	pCmd->AddParameter( new MCommandParameterInt(MERR_CANNOT_VOTE) );
	RouteToListener( pObj, pCmd );
}



void MMatchServer::OnEventChangeMaster(const MUID& uidAdmin)
{
	MMatchObject* pObj = GetObject(uidAdmin);
	if( 0 == pObj )
		return;

	if (!IsEnabledObject(pObj)) return;

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	// 관리자 권한을 가진 사람이 아니면 연결을 끊는다.
	if (!IsAdminGrade(pObj))
	{
//		DisconnectObject(uidAdmin);		
		return;
	}

	if (pStage->GetMasterUID() == uidAdmin)
		return;

	pStage->SetMasterUID(uidAdmin);
	StageMaster(pStage->GetUID());
}

void MMatchServer::OnEventChangePassword(const MUID& uidAdmin, const char* pszPassword)
{
	MMatchObject* pObj = GetObject(uidAdmin);
	if( 0 == pObj ) 
		return;

	if (!IsEnabledObject(pObj)) return;

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	// 관리자 권한을 가진 사람이 아니면 연결을 끊는다.
	if (!IsAdminGrade(pObj))
	{
//		DisconnectObject(uidAdmin);		
		return;
	}

	pStage->SetPassword(pszPassword);
	pStage->SetPrivate(true);
}

void MMatchServer::OnEventRequestJjang(const MUID& uidAdmin, const char* pszTargetName)
{
	MMatchObject* pObj = GetObject(uidAdmin);
	if( 0 == pObj )
		return;

	if (!IsEnabledObject(pObj)) return;

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	// 관리자 권한을 가진 사람이 아니면 무시
	if (!IsAdminGrade(pObj))
	{
		return;
	}

	MMatchObject* pTargetObj = GetPlayerByName(pszTargetName);
	if (pTargetObj == NULL) return;
	if (IsAdminGrade(pTargetObj)) return;		// 어드민 대상으로 짱불가
	if (MMUG_STAR == pTargetObj->GetAccountInfo()->m_nUGrade) return;	// 이미 짱

	pTargetObj->GetAccountInfo()->m_nUGrade = MMUG_STAR;

	if (m_MatchDBMgr.EventJjangUpdate(pTargetObj->GetAccountInfo()->m_nAID, true)) {
		MMatchObjectCacheBuilder CacheBuilder;
		CacheBuilder.AddObject(pTargetObj);
		MCommand* pCmdCacheUpdate = CacheBuilder.GetResultCmd(MATCHCACHEMODE_REPLACE, this);
		RouteToStage(pStage->GetUID(), pCmdCacheUpdate);

		MCommand* pCmdUIUpdate = CreateCommand(MC_EVENT_UPDATE_JJANG, MUID(0,0));
		pCmdUIUpdate->AddParameter(new MCommandParameterUID(pTargetObj->GetUID()));
		pCmdUIUpdate->AddParameter(new MCommandParameterBool(true));
		RouteToStage(pStage->GetUID(), pCmdUIUpdate);
	}
}

void MMatchServer::OnEventRemoveJjang(const MUID& uidAdmin, const char* pszTargetName)
{
	MMatchObject* pObj = GetObject(uidAdmin);
	if( 0 == pObj )
		return;

	if (!IsEnabledObject(pObj)) return;

	MMatchStage* pStage = FindStage(pObj->GetStageUID());
	if (pStage == NULL) return;

	// 관리자 권한을 가진 사람이 아니면 연결을 끊는다.
	if (!IsAdminGrade(pObj))
	{
		return;
	}
	
	MMatchObject* pTargetObj = GetPlayerByName(pszTargetName);
	if (pTargetObj == NULL) return;			// 어드민 대상으로 짱불가

	pTargetObj->GetAccountInfo()->m_nUGrade = MMUG_FREE;

	if (m_MatchDBMgr.EventJjangUpdate(pTargetObj->GetAccountInfo()->m_nAID, false)) {
		MMatchObjectCacheBuilder CacheBuilder;
		CacheBuilder.AddObject(pTargetObj);
		MCommand* pCmdCacheUpdate = CacheBuilder.GetResultCmd(MATCHCACHEMODE_REPLACE, this);
		RouteToStage(pStage->GetUID(), pCmdCacheUpdate);

		MCommand* pCmdUIUpdate = CreateCommand(MC_EVENT_UPDATE_JJANG, MUID(0,0));
		pCmdUIUpdate->AddParameter(new MCommandParameterUID(pTargetObj->GetUID()));
		pCmdUIUpdate->AddParameter(new MCommandParameterBool(false));
		RouteToStage(pStage->GetUID(), pCmdUIUpdate);
	}
}


void MMatchServer::OnStageGo(const MUID& uidPlayer, unsigned int nRoomNo)
{
	MMatchObject* pChar = GetObject(uidPlayer);
	if( 0 == pChar ) return;
	if (!IsEnabledObject(pChar)) return;
	if (pChar->GetPlace() != MMP_LOBBY) return;
	MMatchChannel* pChannel = FindChannel(pChar->GetChannelUID());
	if (pChannel == NULL) return;

	MMatchStage* pStage = pChannel->GetStage(nRoomNo-1);
	if (pStage) {
		MCommand* pNew = CreateCommand(MC_MATCH_REQUEST_STAGE_JOIN, GetUID());
		pNew->SetSenderUID(uidPlayer);	// 플레이어가 보낸 메시지인 것처럼 위장
		pNew->AddParameter(new MCommandParameterUID(uidPlayer));
		pNew->AddParameter(new MCommandParameterUID(pStage->GetUID()));
		Post(pNew);
	}
}



void MMatchServer::OnDuelQueueInfo(const MUID& uidStage, const MTD_DuelQueueInfo& QueueInfo)
{
	MCommand* pCmd = CreateCommand(MC_MATCH_DUEL_QUEUEINFO, MUID(0,0));
	pCmd->AddParameter(new MCmdParamBlob(&QueueInfo, sizeof(MTD_DuelQueueInfo)));
	RouteToBattle(uidStage, pCmd);
}


void MMatchServer::OnQuestSendPing(const MUID& uidStage, unsigned long int t)
{
	MCommand* pCmd = CreateCommand(MC_QUEST_PING, MUID(0,0));
	pCmd->AddParameter(new MCommandParameterUInt(t));
	RouteToBattle(uidStage, pCmd);
}

void MMatchServer::SaveGameLog(const MUID& uidStage)
{
	MMatchStage* pStage = FindStage(uidStage);
	if (pStage == NULL) return;

	int nMapID		= pStage->GetStageSetting()->GetMapIndex();
	int nGameType	= (int)pStage->GetStageSetting()->GetGameType();
	
	

	// test 맵등은 로그 남기지 않는다.
	if ( (MGetMapDescMgr()->MIsCorrectMap(nMapID)) && (MGetGameTypeMgr()->IsCorrectGameType(nGameType)) )
	{
		if (pStage->GetStageType() != MST_LADDER)
		{
			MMatchObject* pMaster = GetObject(pStage->GetMasterUID());

			MAsyncDBJob_InsertGameLog* pJob = new MAsyncDBJob_InsertGameLog(uidStage);
			pJob->Input(pMaster == NULL ? 0 : pMaster->GetCharInfo()->m_nCID,
				MGetMapDescMgr()->GetMapName(nMapID), 
				MGetGameTypeMgr()->GetInfo(MMATCH_GAMETYPE(nGameType))->szGameTypeStr);
			PostAsyncJob(pJob);
		}
	}

}

void MMatchServer::SaveGamePlayerLog(MMatchObject* pObj, unsigned int nStageID)
{	
	if( pObj == NULL ) return;
	if( nStageID == 0 ) return;
	if( pObj->GetCharInfo() == NULL ) return;

	MAsyncDBJob_InsertGamePlayerLog* pJob = new MAsyncDBJob_InsertGamePlayerLog;
	pJob->Input(nStageID, pObj->GetCharInfo()->m_nCID,
		(GetGlobalClockCount() - pObj->GetCharInfo()->m_nBattleStartTime) / 1000,
		pObj->GetCharInfo()->GetCharGamePlayInfo()->nKillCount,
		pObj->GetCharInfo()->GetCharGamePlayInfo()->nDeathCount,
		pObj->GetCharInfo()->GetCharGamePlayInfo()->nXP,
		pObj->GetCharInfo()->GetCharGamePlayInfo()->nBP);
	PostAsyncJob(pJob);
}
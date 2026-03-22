#include "stdafx.h"
#include "MBMatchServer.h"
#include "MatchServerDoc.h"
#include "OutputView.h"
#include <atltime.h>
#include "MatchServer.h"
#include "MMap.h"
#include "MErrorTable.h"
#include "CommandLogView.h"
#include "MDebug.h"
#include "MMatchRule.h"
#include "MMatchRuleAssassinate.h"
#include "MBMatchAuth.h"
#include "MDebug.h"
#include "MMatchStatus.h"
#include "MMatchSchedule.h"
#include "MSharedCommandTable.h"
#include "MMatchConfig.h"
#include "MBlobArray.h"
#include "MUtil.h"
#include "RTypes.h"
#include "MTypes.h"
#include "MCommandParameter.h"
#include "MMatchUtil.h"
#include "MMatchShop.h"

#ifdef LOCALE_NHNUSA
#include "MBMatchNHNAuth.h"
#endif


bool MBMatchServer::OnCommand(MCommand* pCommand)
{
	// 최소한 하나의 커맨드가 무엇인지는 알기위해 로컬변수에 저장
	int nCommandID = pCommand->GetID();

	CheckMemoryCorruption();

	if( MMatchServer::OnCommand(pCommand) )
	{
		CheckMemoryCorruption();
		return true;
	}

	switch( pCommand->GetID() )
	{	
	case MC_MATCH_REQUEST_EQUIP_ITEM:
		{
			MUID uidPlayer, uidItem;
			uidPlayer = pCommand->GetSenderUID();
			unsigned long int nEquipmentSlot = 0;

			//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
			pCommand->GetParameter(&uidItem, 1, MPT_UID);
			pCommand->GetParameter(&nEquipmentSlot, 2, MPT_UINT);

			OnRequestEquipItem(uidPlayer, uidItem, nEquipmentSlot);
		}
		break;

	case MC_MATCH_REQUEST_TAKEOFF_ITEM:
		{
			MUID uidPlayer = pCommand->GetSenderUID();
			unsigned long int nEquipmentSlot = 0;
			//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
			pCommand->GetParameter(&nEquipmentSlot, 1, MPT_UINT);

			OnRequestTakeoffItem(uidPlayer, nEquipmentSlot);
		}
		break;

	case MC_MATCH_LOGIN_NETMARBLE:
		{
#ifdef LOCALE_KOREA
			char szAuthCookie[4096];
			char szDataCookie[4096];
			char szCPCookie[4096];
			char szSpareParam[4096];
			int nCommandVersion = 0;
			unsigned long nChecksumPack = 0;
			if (pCommand->GetParameter(szAuthCookie, 0, MPT_STR, sizeof(szAuthCookie) )==false) break;
			if (pCommand->GetParameter(szDataCookie, 1, MPT_STR, sizeof(szDataCookie) )==false) break;
			if (pCommand->GetParameter(szCPCookie, 2, MPT_STR, sizeof(szCPCookie) )==false) break;
			if (pCommand->GetParameter(szSpareParam, 3, MPT_STR, sizeof(szSpareParam) )==false) break;
			if (pCommand->GetParameter(&nCommandVersion, 4, MPT_INT)==false) break;
			if (pCommand->GetParameter(&nChecksumPack, 5, MPT_UINT)==false) break;

			//todok mark
			OnRequestLoginNetmarble(pCommand->GetSenderUID(), szAuthCookie, szDataCookie, szCPCookie, szSpareParam, nCommandVersion, nChecksumPack);
			//OnRequestLoginNetmarble(pCommand->GetSenderUID(), szCPCookie, szSpareParam, nCommandVersion, nChecksumPack);
#endif
		}
		break;

	case MC_MATCH_LOGIN_NHNUSA :  
		{
#ifdef LOCALE_NHNUSA
			char szUserID[ MAX_USERID_STRING_LEN ]	= {0,};
			char szAuthStr[ NHN_AUTH_LENGTH ]		= {0,};
			int  nCommandVersion = 0;
			int  nCheckSumPack = 0;

			pCommand->GetParameter( szUserID, 0, MPT_STR, MAX_USERID_STRING_LEN );
			pCommand->GetParameter( szAuthStr, 1, MPT_STR, NHN_AUTH_LENGTH );
			pCommand->GetParameter( &nCommandVersion, 2, MPT_INT );
			pCommand->GetParameter( &nCheckSumPack, 3, MPT_UINT );
			MCommandParameter* pLoginParam = pCommand->GetParameter(4);
			if (pLoginParam->GetType() != MPT_BLOB) break;
			void *pLoginBlob = pLoginParam->GetPointer();
			if( NULL == pLoginBlob )
				break;

			char *szEncryptMD5Value = (char *)MGetBlobArrayElement(pLoginBlob, 0);

			OnRequestLoginNHNUSA( pCommand->GetSenderUID(), szUserID, szAuthStr, nCommandVersion, nCheckSumPack, szEncryptMD5Value);
#endif
		}
		break;

		case MC_MATCH_LOGIN_GAMEON_JP :  
		{
#ifdef LOCALE_JAPAN
			char szString[ 512 ] = {0,};
			char szStatIndex[ 512 ] = {0,};
			int  nCommandVersion = 0;
			int  nCheckSumPack = 0;

			pCommand->GetParameter( szString, 0, MPT_STR, 512 );
			pCommand->GetParameter( szStatIndex, 1, MPT_STR, 512 );
			pCommand->GetParameter( &nCommandVersion, 2, MPT_INT );
			pCommand->GetParameter( &nCheckSumPack, 3, MPT_UINT );
			MCommandParameter* pLoginParam = pCommand->GetParameter(4);
			if (pLoginParam->GetType() != MPT_BLOB) break;
			void *pLoginBlob = pLoginParam->GetPointer();
			if( NULL == pLoginBlob )
				break;

			char *szEncryptMD5Value = (char *)MGetBlobArrayElement(pLoginBlob, 0);

			OnRequestLoginGameOn( pCommand->GetSenderUID(), szString, szStatIndex, nCommandVersion, nCheckSumPack, szEncryptMD5Value);
#endif
		}
		break;

	case MC_MATCH_SCHEDULE_ANNOUNCE_MAKE :
		{
			// 서버 스케쥴러에 등록된 공지를 클라이언트로 보냄.

			char szAnnounce[ ANNOUNCE_STRING_LEN ];

			if( MUID(0, 0) != pCommand->GetSenderUID() )
				break;

			pCommand->GetParameter( szAnnounce, 0, MPT_STR, ANNOUNCE_STRING_LEN );

			OnScheduleAnnounce( szAnnounce );
		}
		break;

	case MC_MATCH_SCHEDULE_CLAN_SERVER_SWITCH_DOWN : 
		{
			// 클랜 서버일 경우 클랜전을 할수 없게 설정합니다.

			// bool bClanEnabled;
			// pCommand->GetParameter( &bClanEnabled, 0, MPT_BOOL );

			OnScheduleClanServerSwitchDown();		
		}
		break;

	case MC_MATCH_SCHEDULE_CLAN_SERVER_SWITCH_ON :
		{
			OnScheduleClanServerSwitchUp();
		}
		break;

	case MC_REQUEST_KEEPER_CONNECT_MATCHSERVER :
		{
			OnRequestConnectMatchServer( pCommand->GetSenderUID() );
		}
		break;

	case MC_REQUEST_MATCHSERVER_STATUS :
		{
			OnResponseServerStatus( pCommand->GetSenderUID() );
		}
		break;

	case MC_REQUEST_SERVER_HEARBEAT :
		{
			OnRequestServerHearbeat( pCommand->GetSenderUID() );
		}
		break;

	case MC_REQUEST_KEEPER_ANNOUNCE :
		{
			char szAnnounce[ ANNOUNCE_STRING_LEN ];

			pCommand->GetParameter( szAnnounce, 0, MPT_STR, ANNOUNCE_STRING_LEN );

			OnRequestKeeperAnnounce( pCommand->GetSenderUID(), szAnnounce );
		}
		break;

	case MC_REQUEST_ANNOUNCE_STOP_SERVER :
		{
			OnRequestStopServerWithAnnounce( pCommand->GetSenderUID() );
		}
		break;

	case MC_REQUEST_KEEPER_MANAGER_SCHEDULE :
		{
			int nType;
			int nYear;
			int nMonth;
			int nDay;
			int nHour;
			int nMin;
			int nCount;
			int nCommand;
			char szAnnounce[ ANNOUNCE_STRING_LEN ] = {0,};

			pCommand->GetParameter( &nType, 0, MPT_INT );
			pCommand->GetParameter( &nYear, 1, MPT_INT );
			pCommand->GetParameter( &nMonth, 2, MPT_INT );
			pCommand->GetParameter( &nDay, 3, MPT_INT );
			pCommand->GetParameter( &nHour, 4, MPT_INT );
			pCommand->GetParameter( &nMin, 5, MPT_INT );
			pCommand->GetParameter( &nCount, 6, MPT_INT );
			pCommand->GetParameter( &nCommand, 7, MPT_INT );
			pCommand->GetParameter( szAnnounce, 8, MPT_STR, ANNOUNCE_STRING_LEN );

			OnRequestSchedule( pCommand->GetSenderUID(), 
				nType, 
				nYear, 
				nMonth, 
				nDay, 
				nHour, 
				nMin, 
				nCount, 
				nCommand, 
				szAnnounce );
		}
		break;

	case MC_MATCH_SCHEDULE_STOP_SERVER :
		{
			char szAnnounce[ ANNOUNCE_STRING_LEN ] = { 0, };

			pCommand->GetParameter( szAnnounce, 0, MPT_STR, ANNOUNCE_STRING_LEN );

			OnRequestKeeperStopServerSchedule( pCommand->GetSenderUID(), szAnnounce );
		}
		break;

	case MC_LOCAL_UPDATE_USE_COUNTRY_FILTER :
		{
			OnLocalUpdateUseCountryFilter();
		}
		break;

	case MC_LOCAL_GET_DB_IP_TO_COUNTRY :
		{
			OnLocalGetDBIPtoCountry();
		}
		break;

	case MC_LOCAL_GET_DB_BLOCK_COUNTRY_CODE : 
		{
			OnLocalGetDBBlockCountryCode();
		}
		break;

	case MC_LOCAL_GET_DB_CUSTOM_IP :
		{
			OnLocalGetDBCustomIP();
		}
		break;

	case MC_LOCAL_UPDATE_IP_TO_COUNTRY :
		{
			OnLocalUpdateIPtoCountry();
		}
		break;

	case MC_LOCAL_UPDATE_BLOCK_COUTRYCODE :
		{
			OnLocalUpdateBlockCountryCode();
		}
		break;

	case MC_LOCAL_UPDATE_CUSTOM_IP :
		{
			OnLocalUpdateCustomIP();
		}
		break;

	case MC_LOCAL_UPDATE_ACCEPT_INVALID_IP :
		{
			OnLocalUpdateAcceptInvaildIP();
		}
		break;

	case MC_REQUEST_RELOAD_CONFIG :
		{
			char szFileList[ 1024 ] = {0,};

			pCommand->GetParameter( szFileList, 0, MPT_STR, 1024 );

			OnRequestReloadServerConfig( pCommand->GetSenderUID(), szFileList );
		}
		break;

	case MC_REQUEST_ADD_HASHMAP :
		{
	//		char szNewHashValue[ 128 ] = {0,};
	//		pCommand->GetParameter( szNewHashValue, 0, MPT_STR, 128 );
	//		OnRequestAddHashMap( pCommand->GetSenderUID(), szNewHashValue );
		}
		break;

	case MC_ADMIN_RELOAD_CLIENT_HASH:
		{
		}
		break;

	case MC_MATCH_REQUEST_ACCOUNT_CHARLIST:
		{
			MUID uidPlayer = pCommand->GetSenderUID();
			OnRequestAccountCharList(uidPlayer);
		}
		break;
	case MC_MATCH_REQUEST_SHOP_ITEMLIST:
		{
			MUID uidPlayer = pCommand->GetSenderUID();
			int nFirstItemIndex = 0, nItemCount = 0;

			pCommand->GetParameter(&nFirstItemIndex, 1, MPT_INT);
			pCommand->GetParameter(&nItemCount, 2, MPT_INT);

			OnRequestShopItemList(uidPlayer, nFirstItemIndex, nItemCount);
		}
		break;
	case MC_MATCH_REQUEST_GAMBLE:
		{
			MUID uidPlayer = pCommand->GetSenderUID();
			MUID uidItem;
			pCommand->GetParameter(&uidItem, 0, MPT_UID);

			OnRequestGamble(uidPlayer, uidItem);
		}
		break;

	case MC_MATCH_REQUEST_BUY_ITEM:
		{
			MUID uidPlayer = pCommand->GetSenderUID();
			unsigned long int nItemID;
			int	nItemCount;

			pCommand->GetParameter(&nItemID, 1, MPT_UINT);
			pCommand->GetParameter(&nItemCount, 2, MPT_UINT);

			OnRequestBuyItem(uidPlayer, nItemID, nItemCount);
		}
		break;

	case MC_MATCH_REQUEST_CHARACTER_ITEMLIST:
		{
			MUID uidPlayer = pCommand->GetSenderUID();

			OnRequestCharacterItemList(uidPlayer);
		}
		break;

	case MC_MATCH_REQUEST_CHARACTER_ITEMLIST_FORCE:
		{
			MUID uidPlayer = pCommand->GetSenderUID();

			OnRequestCharacterItemListForce(uidPlayer);
		}
		break;

	case MC_MATCH_REQUEST_SELL_ITEM:
		{
			MUID uidPlayer, uidItem;
			int nItemCount = 0;
			uidPlayer = pCommand->GetSenderUID();			

			//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
			pCommand->GetParameter(&uidItem, 1, MPT_UID);
			pCommand->GetParameter(&nItemCount, 2, MPT_UINT);

			OnRequestSellItem(uidPlayer, uidItem, nItemCount);
		}
		break;

	case MC_MATCH_REQUEST_BRING_ACCOUNTITEM:
		{
			unsigned int nAIID, nItemID, nItemCnt;
			MUID uidPlayer = pCommand->GetSenderUID();

			pCommand->GetParameter(&nAIID, 1, MPT_INT);
			pCommand->GetParameter(&nItemID, 2, MPT_INT);
			pCommand->GetParameter(&nItemCnt, 3, MPT_INT);

			OnRequestBringAccountItem(uidPlayer, nAIID, nItemID, nItemCnt);
		}
		break;

	case MC_MATCH_REQUEST_BRING_BACK_ACCOUNTITEM:
		{
			int nItemCnt;
			MUID uidPlayer, uidItem;

			uidPlayer = pCommand->GetSenderUID();

			pCommand->GetParameter(&uidItem,  1, MPT_UID);
			pCommand->GetParameter(&nItemCnt, 2, MPT_UINT);

			OnRequestBringBackAccountItem(uidPlayer, uidItem, nItemCnt);
		}
		break;

	case MC_ADMIN_RELOAD_GAMBLEITEM :
		{
			MMatchObject* pObj = GetObject( pCommand->GetSenderUID() );
			if( NULL == pObj )
				break;

			if( IsAdminGrade(pObj) )
				InitGambleMachine();
			break;
		}

	case MC_ADMIN_DUMP_GAMBLEITEM_LOG :
		{
			MMatchObject* pObj = GetObject( pCommand->GetSenderUID() );
			if( NULL == pObj )
				break;

			if( IsAdminGrade(pObj) )
			{
				GetGambleMachine().WriteGambleItemInfoToLog();
			}
		}
		break;
	
	case MC_ADMIN_ASSASIN :
		{
			MMatchObject* pObj = GetObject( pCommand->GetSenderUID() );
			if( NULL == pObj )
				break;

			if( IsAdminGrade(pObj) )
			{
				MMatchStage* pStage = FindStage(pObj->GetStageUID());
				if (pStage == NULL)			break;
				if (pStage->GetStageSetting()->GetGameType() != MMATCH_GAMETYPE_ASSASSINATE ) break;

				MMatchRuleAssassinate* pRule = (MMatchRuleAssassinate*)pStage->GetRule();
				pRule->ChooseAdminAsCommander();
			}
		}
		break;

	case MC_MATCH_GAME_KILL:
		{
			MUID uidAttacker, uidVictim;
			pCommand->GetParameter(&uidAttacker, 0, MPT_UID);
			uidVictim = pCommand->GetSenderUID();

			OnGameKill(uidAttacker, uidVictim);
		}
		break;

	case MC_MATCH_REQUEST_STAGE_JOIN:
		{
			MUID uidPlayer, uidStage;

			uidPlayer = pCommand->GetSenderUID();

			//pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
			pCommand->GetParameter(&uidStage, 1, MPT_UID);

			OnStageJoin(uidPlayer, uidStage);
		}
		break;

	case MC_REQUEST_GIVE_ONESELF_UP :
		{
			MMatchObject* pObj = GetObject( pCommand->GetSenderUID() );
			if( NULL == pObj )
			{
				break;
			}

//			pObj->SetDllInjectionDisconnectWaitInfo();
			pObj->DisconnectHacker( MMHT_GIVE_ONESELF_UP_DLLINJECTION);
		}
		break;

	default :
		{
			// 정의되지 않은 커맨드.
		}
		CheckMemoryCorruption();
		return false;
	}

	CheckMemoryCorruption();
	return true;
}
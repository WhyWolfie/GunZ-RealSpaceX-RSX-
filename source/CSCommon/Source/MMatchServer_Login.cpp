#include "stdafx.h"
#include "MMatchServer.h"
#include "MSharedCommandTable.h"
#include "MErrorTable.h"
#include "MBlobArray.h"
#include "MObject.h"
#include "MMatchObject.h"
#include "Msg.h"
#include "MMatchConfig.h"
#include "MCommandCommunicator.h"
#include "MDebug.h"
#include "MMatchAuth.h"
#include "MAsyncDBJob.h"
#include "MAsyncDBJob_GetLoginInfo.h"
#include "MAsyncDBJob_InsertConnLog.h"
#include "RTypes.h"
#include "MMatchUtil.h"
#include <winbase.h>
#include "MMatchPremiumIPCache.h"
#include "MCommandBuilder.h"
#include "MMatchStatus.h"
#include "MMatchLocale.h"

#include "../../sdk/sha512/sha512.h"

bool MMatchServer::CheckOnLoginPre(const MUID& CommUID, int nCmdVersion, bool& outbFreeIP, string& strCountryCode3)
{
	MCommObject* pCommObj = (MCommObject*)m_CommRefCache.GetRef(CommUID);
	if (pCommObj == NULL) return false;

	// 프로토콜 버전 체크
	if (nCmdVersion != MCOMMAND_VERSION)
	{
		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_COMMAND_INVALID_VERSION);
		Post(pCmd);	
		return false;
	}

	// free login ip를 검사하기전에 debug서버와 debug ip를 검사한다.
	// 서버가 debug타입인지 검사.
	if( MGetServerConfig()->IsDebugServer() && MGetServerConfig()->IsDebugLoginIPList(pCommObj->GetIPString()) )
	{
		outbFreeIP = true;
		return true;
	}

	// 최대인원 체크
	bool bFreeLoginIP = false;
	if (MGetServerConfig()->CheckFreeLoginIPList(pCommObj->GetIPString()) == true) {
		bFreeLoginIP = true;
		outbFreeIP = true;
		return true;
	} else {
		outbFreeIP = false;

		if ((int)m_Objects.size() >= MGetServerConfig()->GetMaxUser())
		{
			MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_CLIENT_FULL_PLAYERS);
			Post(pCmd);	
			return false;
		}
	}

	// 접속을 막아놓은 지역의 IP인가
	if( CheckIsValidIP(CommUID, pCommObj->GetIPString(), strCountryCode3, MGetServerConfig()->IsUseFilter()) )
		IncreaseNonBlockCount();
	else
	{
		IncreaseBlockCount();

		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_FAILED_BLOCK_IP);
		Post(pCmd);	
		return false;
	}

	return true;
}

void MMatchServer::OnMatchLogin(MUID CommUID, const char* szUserID, const char* szPassword, int nCommandVersion, unsigned long nChecksumPack, char *szEncryptMd5Value)
{
//	MCommObject* pCommObj = (MCommObject*)m_CommRefCache.GetRef(CommUID);
//	if (pCommObj == NULL) return;

	// 초기 위치의 노드는 검색해서 얻어낸다.
	int nMapID = 0;

	unsigned int nAID = 0;
	char szDBPassword[129] = "";
	string strCountryCode3;

	bool bFreeLoginIP = false;

	// 프로토콜, 최대인원 체크
	if (!CheckOnLoginPre(CommUID, nCommandVersion, bFreeLoginIP, strCountryCode3)) return;

	// 원래 계정은 넷마블에 있으므로 해당 계정이 없으면 새로 생성한다. 
	if (!m_MatchDBMgr.GetLoginInfo(szUserID, &nAID, szDBPassword))
	{
#ifdef _DEBUG
		m_MatchDBMgr.CreateAccount(szUserID, szPassword, 0, szUserID, 20, 1);
		strcpy(szDBPassword, szPassword);

		m_MatchDBMgr.GetLoginInfo(szUserID, &nAID, szDBPassword);
#endif

		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_CLIENT_WRONG_PASSWORD);
		Post(pCmd);	

		return;
	}
    szDBPassword[128] = 0;

	MCommObject* pCommObj = (MCommObject*)m_CommRefCache.GetRef(CommUID);
	if (pCommObj)
	{
		// 디비에 최종 접속시간을 업데이트 한다.
		if (!m_MatchDBMgr.UpdateLastConnDate(szUserID, pCommObj->GetIPString()))
		{	
			mlog("DB Query(OnMatchLogin > UpdateLastConnDate) Failed");
		}

	}


	// 패스워드가 틀렸을 경우 처리
	//if (strcmp(szDBPassword, szPassword))
    if (!sha512_match(szPassword, szDBPassword))
	{
		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_CLIENT_WRONG_PASSWORD);
		Post(pCmd);	

		return;
	}

	MMatchAccountInfo accountInfo;
	if (!m_MatchDBMgr.GetAccountInfo(nAID, &accountInfo, MGetServerConfig()->GetServerID()))
	{
		// Notify Message 필요 -> 로그인 관련 - 해결(Login Fail 메세지 이용)
		// Disconnect(CommUID);
		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_FAILED_GETACCOUNTINFO);
		Post(pCmd);	
	}

	MMatchAccountPenaltyInfo accountpenaltyInfo;
	if( !m_MatchDBMgr.GetAccountPenaltyInfo(nAID, &accountpenaltyInfo) ) 
	{
		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_FAILED_GETACCOUNTINFO);
		Post(pCmd);	
	}

#ifndef _DEBUG
	// 중복 로그인이면 이전에 있던 사람을 끊어버린다.
	MMatchObject* pCopyObj = GetPlayerByAID(accountInfo.m_nAID);
 	if (pCopyObj != NULL) 
	{
		// 내가 로그인일때 이미 로그인 돼있는 클라이언트가 있으면 이미 로그인 클라이언트에 
		// 중복 로그인이란 메세지 보내고 접속을 끊음. - by kammir 2008.09.30
		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(pCopyObj->GetUID(), MERR_MULTIPLE_LOGIN);
		Post(pCmd);	
		//Disconnect(pCopyObj->GetUID());
	}
#endif

	// 사용정지 계정인지 확인한다.
	if ((accountInfo.m_nUGrade == MMUG_BLOCKED) || (accountInfo.m_nUGrade == MMUG_PENALTY))
	{
		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_CLIENT_MMUG_BLOCKED);
		Post(pCmd);	
		return;
	}

#ifndef _DEBUG // debug에선 상관없다. 테스트가 필요하면 따로 설정을 해야 함. - by SungE 2007-05-03
	// gunz.exe 실행파일의 무결성을 확인한다. (암호화 되어 있다)
	// server.ini 파일에서 설정된 값에 따라 사용하지 않으면 검사하지 않는다.
	if (MGetServerConfig()->IsUseMD5())				
	{
		unsigned char szMD5Value[ MAX_MD5LENGH ] = {0, };
		pCommObj->GetCrypter()->Decrypt(szEncryptMd5Value, MAX_MD5LENGH, (MPacketCrypterKey*)pCommObj->GetCrypter()->GetKey());
		memcpy( szMD5Value, szEncryptMd5Value, MAX_MD5LENGH );

		if ((memcmp(m_szMD5Value, szMD5Value, MAX_MD5LENGH)) != 0)
		{
			// "정상적인 실행파일이 아닙니다." 이런 오류 패킷이 없어서 전송 생략
			LOG(LOG_PROG, "MD5 error : AID(%u).\n \n", accountInfo.m_nAID);
			// 접속 끊어버리자
//			Disconnect(CommUID);
			return;
		}
	}
#endif

	// 로그인성공하여 오브젝트(MMatchObject) 생성
	AddObjectOnMatchLogin(CommUID, &accountInfo, &accountpenaltyInfo, bFreeLoginIP, strCountryCode3, nChecksumPack);

/*
	MUID AllocUID = CommUID;
	int nErrCode = ObjectAdd(CommUID);
	if(nErrCode!=MOK){
		LOG(LOG_DEBUG, MErrStr(nErrCode) );
	}

	MMatchObject* pObj = GetObject(AllocUID);
	pObj->AddCommListener(CommUID);
	pObj->SetObjectType(MOT_PC);
	memcpy(pObj->GetAccountInfo(), &accountInfo, sizeof(MMatchAccountInfo));
	pObj->SetFreeLoginIP(bFreeLoginIP);
	pObj->SetCountryCode3( strCountryCode3 );
	pObj->UpdateTickLastPacketRecved();

	if (pCommObj != NULL)
	{
		pObj->SetPeerAddr(pCommObj->GetIP(), pCommObj->GetIPString(), pCommObj->GetPort());
	}
	
	SetClientClockSynchronize(CommUID);


	// 프리미엄 IP를 체크한다.
	if (MGetServerConfig()->CheckPremiumIP())
	{
		if (pCommObj)
		{
			bool bIsPremiumIP = false;
			bool bExistPremiumIPCache = false;
			
			bExistPremiumIPCache = MPremiumIPCache()->CheckPremiumIP(pCommObj->GetIP(), bIsPremiumIP);

			// 만약 캐쉬에 없으면 직접 DB에서 찾도록 한다.
			if (!bExistPremiumIPCache)
			{
				if (m_MatchDBMgr.CheckPremiumIP(pCommObj->GetIPString(), bIsPremiumIP))
				{
					// 결과를 캐쉬에 저장
					MPremiumIPCache()->AddIP(pCommObj->GetIP(), bIsPremiumIP);
				}
				else
				{
					MPremiumIPCache()->OnDBFailed();
				}

			}

			if (bIsPremiumIP) pObj->GetAccountInfo()->m_nPGrade = MMPG_PREMIUM_IP;
		}		
	}


	MCommand* pCmd = CreateCmdMatchResponseLoginOK(CommUID, 
												   AllocUID, 
												   pObj->GetAccountInfo()->m_szUserID,
												   pObj->GetAccountInfo()->m_nUGrade,
                                                   pObj->GetAccountInfo()->m_nPGrade);
	Post(pCmd);	

	// 접속 로그를 남긴다.
	m_MatchDBMgr.InsertConnLog(pObj->GetAccountInfo()->m_nAID, pObj->GetIPString(), pObj->GetCountryCode3() );

#ifndef _DEBUG
	// Client DataFile Checksum을 검사한다.
	unsigned long nChecksum = nChecksumPack ^ CommUID.High ^ CommUID.Low;
	if (nChecksum != GetItemFileChecksum()) {
		LOG(LOG_PROG, "Invalid ZItemChecksum(%u) , UserID(%s) ", nChecksum, pObj->GetAccountInfo()->m_szUserID);
		Disconnect(CommUID);
	}
#endif

*/
}

/*
void MMatchServer::OnMatchLoginFromNetmarble(const MUID& CommUID, const char* szCPCookie, const char* szSpareData, int nCmdVersion, unsigned long nChecksumPack)
{
	MCommObject* pCommObj = (MCommObject*)m_CommRefCache.GetRef(CommUID);
	if (pCommObj == NULL) return;

	bool bFreeLoginIP = false;
	string strCountryCode3;

	// 프로토콜, 최대인원 체크
	if (!CheckOnLoginPre(CommUID, nCmdVersion, bFreeLoginIP, strCountryCode3)) return;


	MMatchAuthBuilder* pAuthBuilder = GetAuthBuilder();
	if (pAuthBuilder == NULL) {
		LOG(LOG_PROG, "Critical Error : MatchAuthBuilder is not assigned.\n");
		return;
	}
	MMatchAuthInfo* pAuthInfo = NULL;
	if (pAuthBuilder->ParseAuthInfo(szCPCookie, &pAuthInfo) == false) 
	{
		MGetServerStatusSingleton()->SetRunStatus(5);

		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_CLIENT_WRONG_PASSWORD);
		Post(pCmd);	

		LOG(LOG_PROG, "Netmarble Certification Failed\n");
		return;
	}

	const char* pUserID = pAuthInfo->GetUserID();
	const char* pUniqueID = pAuthInfo->GetUniqueID();
	const char* pCertificate = pAuthInfo->GetCertificate();
	const char* pName = pAuthInfo->GetName();
	int nAge = pAuthInfo->GetAge();
	int nSex = pAuthInfo->GetSex();
	bool bCheckPremiumIP = MGetServerConfig()->CheckPremiumIP();
	const char* szIP = pCommObj->GetIPString();
	DWORD dwIP = pCommObj->GetIP();

	// Async DB
	MAsyncDBJob_GetLoginInfo* pNewJob = new MAsyncDBJob_GetLoginInfo(CommUID);
	pNewJob->Input(new MMatchAccountInfo(), 
					pUserID, 
					pUniqueID, 
					pCertificate, 
					pName, 
					nAge, 
					nSex, 
					bFreeLoginIP, 
					nChecksumPack,
					bCheckPremiumIP,
					szIP,
					dwIP,
					strCountryCode3);
	PostAsyncJob(pNewJob);

	if (pAuthInfo)
	{
		delete pAuthInfo; pAuthInfo = NULL;
	}
}
*/

void MMatchServer::OnMatchLoginFromNetmarbleJP(const MUID& CommUID, const char* szLoginID, const char* szLoginPW, int nCmdVersion, unsigned long nChecksumPack)
{
	bool bFreeLoginIP = false;
	string strCountryCode3;

	// 프로토콜, 최대인원 체크
	if (!CheckOnLoginPre(CommUID, nCmdVersion, bFreeLoginIP, strCountryCode3)) return;

	// DBAgent에 먼저 보내고 응답을 받으면 로그인 프로세스를 진행한다.
	if (!MGetLocale()->PostLoginInfoToDBAgent(CommUID, szLoginID, szLoginPW, bFreeLoginIP, nChecksumPack, GetClientCount()))
	{
		mlog( "Server user full(DB agent error).\n" );
		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, MERR_CLIENT_FULL_PLAYERS);
		Post(pCmd);
		return;
	}
}

void MMatchServer::OnMatchLoginFromDBAgent(const MUID& CommUID, const char* szLoginID, const char* szName, int nSex, bool bFreeLoginIP, unsigned long nChecksumPack)
{
#ifndef LOCALE_NHNUSA
	MCommObject* pCommObj = (MCommObject*)m_CommRefCache.GetRef(CommUID);
	if (pCommObj == NULL) return;

	string strCountryCode3;
	CheckIsValidIP( CommUID, pCommObj->GetIPString(), strCountryCode3, false );

	const char* pUserID = szLoginID;
	char szPassword[16] = "";			// 패스워드는 없다
	char szCertificate[16] = "";
	const char* pName = szName;
	int nAge = 20;

	bool bCheckPremiumIP = MGetServerConfig()->CheckPremiumIP();
	const char* szIP = pCommObj->GetIPString();
	DWORD dwIP = pCommObj->GetIP();

	// Async DB
	MAsyncDBJob_GetLoginInfo* pNewJob = new MAsyncDBJob_GetLoginInfo(CommUID);
	pNewJob->Input(new MMatchAccountInfo,
					new MMatchAccountPenaltyInfo,
					pUserID, 
					szPassword, 
					szCertificate, 
					pName, 
					nAge, 
					nSex, 
					bFreeLoginIP, 
					nChecksumPack,
					bCheckPremiumIP,
					szIP,
					dwIP,
					strCountryCode3);
	PostAsyncJob(pNewJob);
#endif
}

void MMatchServer::OnMatchLoginFailedFromDBAgent(const MUID& CommUID, int nResult)
{
#ifndef LOCALE_NHNUSA
	// 프로토콜 버전 체크
	MCommand* pCmd = CreateCmdMatchResponseLoginFailed(CommUID, nResult);
	Post(pCmd);	
#endif
}

MCommand* MMatchServer::CreateCmdMatchResponseLoginOK(const MUID& uidComm, 
													  MUID& uidPlayer, 
													  const char* szUserID, 
													  MMatchUserGradeID nUGradeID, 
													  MMatchPremiumGradeID nPGradeID)
{
	MCommand* pCmd = CreateCommand(MC_MATCH_RESPONSE_LOGIN, uidComm);
	pCmd->AddParameter(new MCommandParameterInt(MOK));
	pCmd->AddParameter(new MCommandParameterString(MGetServerConfig()->GetServerName()));
	pCmd->AddParameter(new MCommandParameterChar((char)MGetServerConfig()->GetServerMode()));
	pCmd->AddParameter(new MCommandParameterString(szUserID));
	pCmd->AddParameter(new MCommandParameterUChar((unsigned char)nUGradeID));
	pCmd->AddParameter(new MCommandParameterUChar((unsigned char)nPGradeID));
	pCmd->AddParameter(new MCommandParameterUID(uidPlayer));
	pCmd->AddParameter(new MCommandParameterBool((bool)MGetServerConfig()->IsEnabledSurvivalMode()));
	pCmd->AddParameter(new MCommandParameterBool((bool)MGetServerConfig()->IsEnabledDuelTournament()));

	return pCmd;
}

MCommand* MMatchServer::CreateCmdMatchResponseLoginFailed(const MUID& uidComm, const int nResult)
{
	MCommand* pCmd = CreateCommand(MC_MATCH_RESPONSE_LOGIN, uidComm);
	pCmd->AddParameter(new MCommandParameterInt(nResult));
	pCmd->AddParameter(new MCommandParameterString(MGetServerConfig()->GetServerName()));
	pCmd->AddParameter(new MCommandParameterChar((char)MGetServerConfig()->GetServerMode()));
	pCmd->AddParameter(new MCommandParameterString("Ana"));
	pCmd->AddParameter(new MCommandParameterUChar((unsigned char)MMUG_FREE));
	pCmd->AddParameter(new MCommandParameterUChar((unsigned char)MMPG_FREE));
	pCmd->AddParameter(new MCommandParameterUID(MUID(0,0)));
	pCmd->AddParameter(new MCommandParameterBool((bool)MGetServerConfig()->IsEnabledSurvivalMode()));
	pCmd->AddParameter(new MCommandParameterBool((bool)MGetServerConfig()->IsEnabledDuelTournament()));

	return pCmd;
}


bool MMatchServer::AddObjectOnMatchLogin(const MUID& uidComm, 
										const MMatchAccountInfo* pSrcAccountInfo,
										const MMatchAccountPenaltyInfo* pSrcAccountPenaltyInfo,
										bool bFreeLoginIP, string strCountryCode3, unsigned long nChecksumPack)
{
	MCommObject* pCommObj = (MCommObject*)m_CommRefCache.GetRef(uidComm);
	if (pCommObj == NULL) return false;

	MUID AllocUID = uidComm;
	int nErrCode = ObjectAdd(uidComm);
	if(nErrCode!=MOK) {
		LOG(LOG_DEBUG, MErrStr(nErrCode) );
	}

	MMatchObject* pObj = GetObject(AllocUID);
	if (pObj == NULL) {
		// Notify Message 필요 -> 로그인 관련 - 해결(Login Fail 메세지 이용)
		// Disconnect(uidComm);
		MCommand* pCmd = CreateCmdMatchResponseLoginFailed(AllocUID, MERR_FAILED_LOGIN_RETRY);
		Post(pCmd);	
		return false;
	}

	pObj->AddCommListener(uidComm);
	pObj->SetObjectType(MOT_PC);

	memcpy(pObj->GetAccountInfo(), pSrcAccountInfo, sizeof(MMatchAccountInfo));
	memcpy(pObj->GetAccountPenaltyInfo(), pSrcAccountPenaltyInfo, sizeof(MMatchAccountPenaltyInfo));
		
	pObj->SetFreeLoginIP(bFreeLoginIP);
	pObj->SetCountryCode3( strCountryCode3 );
	pObj->UpdateTickLastPacketRecved();

	if (pCommObj != NULL)
	{
		pObj->SetPeerAddr(pCommObj->GetIP(), pCommObj->GetIPString(), pCommObj->GetPort());
	}
	
	SetClientClockSynchronize(uidComm);

	// 프리미엄 IP를 체크한다.
	if (MGetServerConfig()->CheckPremiumIP())
	{
		if (pCommObj)
		{
			bool bIsPremiumIP = false;
			bool bExistPremiumIPCache = false;
			
			bExistPremiumIPCache = MPremiumIPCache()->CheckPremiumIP(pCommObj->GetIP(), bIsPremiumIP);

			// 만약 캐쉬에 없으면 직접 DB에서 찾도록 한다.
			if (!bExistPremiumIPCache)
			{
				if (m_MatchDBMgr.CheckPremiumIP(pCommObj->GetIPString(), bIsPremiumIP))
				{
					// 결과를 캐쉬에 저장
					MPremiumIPCache()->AddIP(pCommObj->GetIP(), bIsPremiumIP);
				}
				else
				{
					MPremiumIPCache()->OnDBFailed();
				}

			}

			if (bIsPremiumIP) pObj->GetAccountInfo()->m_nPGrade = MMPG_PREMIUM_IP;
		}		
	}

	MCommand* pCmd = CreateCmdMatchResponseLoginOK(uidComm, 
												   AllocUID, 
												   pObj->GetAccountInfo()->m_szUserID,
												   pObj->GetAccountInfo()->m_nUGrade,
                                                   pObj->GetAccountInfo()->m_nPGrade);
	Post(pCmd);	

	// 접속 로그를 남긴다.
	//m_MatchDBMgr.InsertConnLog(pObj->GetAccountInfo()->m_nAID, pObj->GetIPString(), pObj->GetCountryCode3() );

	// 접속 로그
	MAsyncDBJob_InsertConnLog* pNewJob = new MAsyncDBJob_InsertConnLog(uidComm);
	pNewJob->Input(pObj->GetAccountInfo()->m_nAID, pObj->GetIPString(), pObj->GetCountryCode3() );
	PostAsyncJob(pNewJob);

	// Client DataFile Checksum을 검사한다.
	// 2006.2.20 dubble. filelist checksum으로 변경
	unsigned long nChecksum = nChecksumPack ^ uidComm.High ^ uidComm.Low;
	if( MGetServerConfig()->IsUseFileCrc() && !MMatchAntiHack::CheckClientFileListCRC(nChecksum, pObj->GetUID()) && 
		!MGetServerConfig()->IsDebugLoginIPList(pObj->GetIPString()) )
	{
		LOG(LOG_PROG, "Invalid filelist crc (%u) , UserID(%s)\n ", nChecksum, pObj->GetAccountInfo()->m_szUserID);
//		pObj->SetBadFileCRCDisconnectWaitInfo();
		pObj->DisconnectHacker( MMHT_BADFILECRC);
	}
	/*
	if (nChecksum != GetItemFileChecksum()) {
		LOG(LOG_PROG, "Invalid ZItemChecksum(%u) , UserID(%s) ", nChecksum, pObj->GetAccountInfo()->m_szUserID);
		Disconnect(uidComm);
		return false;
	}
	*/

	pObj->LoginCompleted();

	return true;
}

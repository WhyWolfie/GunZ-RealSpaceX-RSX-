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
#include "MBMatchAuth.h"
#include "MDebug.h"
#include "MMatchStatus.h"
#include "MMatchSchedule.h"
#include "MSharedCommandTable.h"
#include "MMatchConfig.h"
#include "MMatchEventFactory.h"
#include "MMatchLocale.h"
#include "MBlobArray.h"
#include "MMatchCheckLoopTime.h"

#ifdef LOCALE_NHNUSA
#include "MBMatchNHNAuth.h"
#include "MBMatchNHNRTA.h"
#endif

#ifdef LOCALE_JAPAN
#include "MBMatchGameOnAuth.h"
#endif


void __cdecl MBRCPLog(const char *pFormat,...)
{
	char szBuf[256];

	va_list args;

	va_start(args,pFormat);
	vsprintf(szBuf, pFormat, args);
	va_end(args);

	int nEnd = (int)(strlen(szBuf)-1);
	if ((nEnd >= 0) && (szBuf[nEnd] == '\n')) {
		szBuf[nEnd] = NULL;
		strcat(szBuf, "\n");
	}
	mlog(szBuf);
}

MBMatchServer::~MBMatchServer() 
{
	OnDestroy();
}

bool MBMatchServer::OnCreate(void)
{
	SetupRCPLog(MBRCPLog);

	CMatchServerApp* pApp = (CMatchServerApp*)AfxGetApp();

	// UDP초기화보다 먼저 실행이 되어야 함.
	// 모니터링 관련 패킷이 모니터링 초기화 보다 먼저 도착하면 NULL포인터 참조가 일어남.
#ifdef _MONITORING
	if( !m_Monitor.Init() )
	{
		mlog( "Init monitor failn" );
		return false;
	}
#endif

	if( !MMatchServer::OnCreate() )
		return false;

	if( !m_ConfigReloader.Create() )
		return false;
	
	if( !LoadFileMD5HashValue() )
		return false; // - by SungE 2007-05-28
	

	if( m_KillTracker.Init(&(MGetServerConfig()->GetKillTrackerConfig())) )
		return false;


	WriteServerInfoLog();

	MGetCheckLoopTimeInstance()->SetInit();

#ifdef _DEBUG
	//m_Tester.Init( this );
	//m_Tester.DoTest( GetGlobalClockCount() );
#endif
	return true;
}


bool MBMatchServer::LoadFileMD5HashValue()
{
#ifndef _DEBUG
	char curDirectory[MAX_PATH] = {0};
	GetCurrentDirectory(MAX_PATH, curDirectory);

	char filePath[MAX_PATH] = {0};
	sprintf(filePath, "%s\\MD5HashValue.txt", curDirectory);

	FILE *ReadFp = fopen(filePath, "rb");
	if( NULL == ReadFp )
	{
		mlog( "Load MD5 fail.\n" );
		return false;
	}

	int readNum = 0;
	for (int i = 0; i < 16; i++)
	{
		fscanf(ReadFp, "%x", &readNum);
		m_szMD5Value[i] = (unsigned char)readNum;
	}
	
	fclose(ReadFp);


	mlog("FILE PATH : %s\nMD5 Hash Value => ", filePath);
	for( int i=0; i<16; i++)
	{
		mlog("%02x ", m_szMD5Value[i]);
	}

	mlog("\n");
#endif

	return true;
}


void MBMatchServer::ReleaseSecurity()
{
	m_Security.Release();
}


void MBMatchServer::OnDestroy(void)
{
	ReleaseSecurity();

#ifdef _MONITORING
		m_Monitor.Release();
#endif

#ifdef _DEBUG
	m_Tester.Release();
#endif
}

void MBMatchServer::OnPrepareCommand(MCommand* pCommand)
{
	// 커맨드 로그 남기기
	if(m_pCmdLogView==NULL) return;

	CMatchServerApp* pApp = (CMatchServerApp*)AfxGetApp();
	if (pApp->CheckOutputLog() == false) return;


	CCommandLogView::CCommandType t;
	if(pCommand->m_pCommandDesc->IsFlag(MCDT_LOCAL)==true) t = CCommandLogView::CCT_LOCAL;
	else if(pCommand->m_Sender==m_This) t = CCommandLogView::CCT_SEND;
	else if(pCommand->m_Receiver==m_This) t = CCommandLogView::CCT_RECEIVE;
	else _ASSERT(FALSE);

	m_pCmdLogView->AddCommand(GetGlobalClockCount(), t, pCommand);
/*
#ifdef _DEBUG
#ifndef _DEBUG_PUBLISH
	// 커맨드 로그 남기기
	if(m_pCmdLogView==NULL) return;

	CMatchServerApp* pApp = (CMatchServerApp*)AfxGetApp();
	if (pApp->CheckOutputLog() == false) return;


	CCommandLogView::CCommandType t;
	if(pCommand->m_pCommandDesc->IsFlag(MCDT_LOCAL)==true) t = CCommandLogView::CCT_LOCAL;
	else if(pCommand->m_Sender==m_This) t = CCommandLogView::CCT_SEND;
	else if(pCommand->m_Receiver==m_This) t = CCommandLogView::CCT_RECEIVE;
	else _ASSERT(FALSE);
	
	m_pCmdLogView->AddCommand(GetGlobalClockCount(), t, pCommand);
#endif
#endif
*/
}



MBMatchServer::MBMatchServer(COutputView* pView)
{
	m_pView = pView;
	m_pCmdLogView = NULL;
	
	SetKeeperUID( MUID(0, 0) );
}

void MBMatchServer::Shutdown()
{
	MMatchServer::Shutdown();
	AfxGetMainWnd()->PostMessage(WM_DESTROY);
//	AfxGetMainWnd()->DestroyWindow();
}

void MBMatchServer::Log(unsigned int nLogLevel, const char* szLog)
{
	MMatchServer::Log(nLogLevel, szLog);	

	if ((nLogLevel & LOG_PROG) == LOG_PROG)
	{
		if(m_pView==NULL) return;

		CTime theTime = CTime::GetCurrentTime();
		CString szTime = theTime.Format( "[%c] " );

		m_pView->AddString(szTime, TIME_COLOR, false);
		m_pView->AddString(szLog, RGB(0,0,0));
	}
}

bool MBMatchServer::InitSubTaskSchedule()
{
	/*  등록방법을 결정해야 함.
		우선은 코드 속에다가. */

	// TODO: 버그가 있는 것 같아 주석처리 해놓았습니다. 클랜전신청 스위치가 꺼지지 않아야 할 때 꺼집니다. 확인 요망 -bird
	// 수정 했음 - SungE.

	// clan서버일 경우만.
	if( MSM_CLAN == MGetServerConfig()->GetServerMode() ){
		if( !AddClanServerSwitchDownSchedule() )
			return false;

		if( !AddClanServerAnnounceSchedule() )
		return false;
	}

	return true;
}

// 서버 시작시 클랜전 서버일경우 등록되는 스케쥴.
bool MBMatchServer::AddClanServerSwitchDownSchedule()
{
	int a = 0;

	MCommand* pCmd = CreateCommand( MC_MATCH_SCHEDULE_CLAN_SERVER_SWITCH_DOWN, MUID(0, 0) );
	if( 0 == pCmd )
		return false;

	tm* ptm = MMatchGetLocalTime();

	// 이것을 현제는 한번만 실행을 하지만, 매달 시작을 하는것으로 수정을 해야함.
	// 커맨드가 실행을 하는 시점에서 커맨드를 업데이트해서 다시 스케쥴러에 등록을 하는쪽으로 방향을 잡음.
	// OnCommnad계열에서 클랜 서버 다운 커맨드가 실행이 될시에 서버다운 커맨드를 다음달로 재 설정을 하고,
	//  다음달 1일에 클랜 서버가 다시 동작하는 커맨드를 생성하여 추가하는 방식으로 함.
	// 클랜서버 다운 커맨드 실행 -> 다음달 1일 클랜 서버 다시 실행 커맨드 생성, 등록 -> 다음달 클랜서버 다운 커맨드 생성, 등록.
	// 만약 클랜전다운 시간중에 여기까지 오면(서버 재시작등) 클랜전다운 시간이 이미 지났지만 이 시간으로 세팅하여
	// 곧바로 클랜전다운을 시켜주고 클랜전 시작시간을 생성, 등록해준다.

	MMatchScheduleData* pScheduleData = m_pScheduler->MakeOnceScheduleData( ptm->tm_year - 100, ptm->tm_mon + 1, GetMaxDay(), 23, 50, pCmd );
	if( 0 == pScheduleData ){
		delete pCmd;
		return false;
	}

	if( !m_pScheduler->AddDynamicSchedule(pScheduleData) ){
		delete pCmd;
		delete pScheduleData;
		return false;
	}

	mlog( "MBMatchServer::AddClanServerSwitchDownSchedule - make close clan mode schedule success. next close clan mode time is %d-%d-%d %d:%d\n",
		pScheduleData->GetYear(), pScheduleData->GetMonth(), pScheduleData->GetDay(),
		pScheduleData->GetHour(), pScheduleData->GetMin() );

	return true;
}


bool MBMatchServer::AddClanServerSwitchUpSchedule()
{
	// 다음달 1일 아침 10시에 클랜서버 클랜전 활성화.
	MCommand* pCmd = CreateCommand( MC_MATCH_SCHEDULE_CLAN_SERVER_SWITCH_ON, MUID(0, 0) );
	if( 0 == pCmd )
	{
		mlog( "MBMatchServer::AddClanServerSwitchUpSchedule - make clan mode open command fail.\n" );
		return false;
	}

	tm* ptm = MMatchGetLocalTime();

	unsigned char cYear;
	unsigned char cMon;

	// 다음달이 내년으로 넘어갔는지 검사함.
	if( 12 >= (ptm->tm_mon + 2) )
	{
		cYear = ptm->tm_year - 100;
		cMon  = ptm->tm_mon + 2;
	}
	else
	{
		// 다음달이 내년 1월일 경우.
		cYear = ptm->tm_year - 99;
		cMon  = 1;
	}

	MMatchScheduleData* pScheduleData = m_pScheduler->MakeOnceScheduleData( cYear, cMon, 1, 9, 0, pCmd );
	if( 0 == pScheduleData )
	{
		delete pCmd;
		return false;
	}

	if( !m_pScheduler->AddDynamicSchedule(pScheduleData) )
	{
		delete pCmd;
		delete pScheduleData;
		return false;
	}

	mlog( "MBMatchServer::AddClanServerSwitchUpSchedule - make open clan mode schedule success. next open time is %d-%d-%d %d:%d\n",
		pScheduleData->GetYear(), pScheduleData->GetMonth(), pScheduleData->GetDay(),
		pScheduleData->GetHour(), pScheduleData->GetMin() );

	return true;
}


// 서버 시작시 등록되어야 하는 공지사항 스케쥴.
bool MBMatchServer::AddClanServerAnnounceSchedule()
{
	char szTest[] = "clan mode is closed on 23:50";

	MCommand* pCmd = CreateCommand( MC_MATCH_SCHEDULE_ANNOUNCE_MAKE, MUID(0, 0) );
	if( 0 == pCmd )
		return false;

	MCommandParameterString* pCmdPrmStr = new MCommandParameterString( szTest );
	if( 0 == pCmdPrmStr ){
		delete pCmd;
		return false;
	}
	
	if( !pCmd->AddParameter(pCmdPrmStr) ){
		delete pCmd;
		return false;
	}

	tm* ptm = MMatchGetLocalTime();

	MMatchScheduleData* pScheduleData = m_pScheduler->MakeOnceScheduleData( ptm->tm_year - 100, ptm->tm_mon + 1, GetMaxDay(), 23, 40, pCmd );
	if( 0 == pScheduleData ){
		delete pCmd;
		return false;
	}

	if( !m_pScheduler->AddDynamicSchedule(pScheduleData) ){
		pScheduleData->Release();
		delete pScheduleData;
		return false;
	}
	
	return true;
}



char log_buffer[65535];

void AddStr(const char* pFormat,...)
{
	va_list args;
	char temp[1024];

	va_start(args, pFormat);
	vsprintf(temp, pFormat, args);

	strcat(log_buffer, temp);
	va_end(args);
}

void MBMatchServer::OnViewServerStatus()
{
	MGetServerStatusSingleton()->SaveToLogFile();
}

bool MBMatchServer::IsKeeper( const MUID& uidKeeper )
{
	MMatchObject* pObj = GetObject( uidKeeper );
	if( 0 == pObj )
		return false;

	if( !MGetServerConfig()->IsKeeperIP(pObj->GetIPString()) )
	{
		mlog( "Keeper hacking. " );
		if( 0 != pObj->GetIPString() )
			mlog( "IP:%s, ", pObj->GetIPString() );

		if( (0 != pObj->GetCharInfo()) && (0 != pObj->GetCharInfo()->m_szName) )
			mlog( "Name:%s", pObj->GetCharInfo()->m_szName );

		mlog( "\n" );

		return false;
	}

	return true;
}


void MBMatchServer::WriteServerInfoLog()
{
	mlog( "\n" );
	mlog( "================================== Server configure info ==================================\n" );

	char szTemp[256];
	sprintf(szTemp, "Release Date : %s", __DATE__);
	Log(LOG_PROG, szTemp);



#ifndef _DEBUG
	// MD5 확인 하는지 여부 로그로 남긴다.
	if (MGetServerConfig()->IsUseMD5())
	{
		LOG( LOG_PROG, "MD5 Check On" );
	}
	else
	{
		LOG( LOG_PROG, "MD5 Check Off" );
	}
#endif

	if (MGetServerConfig()->IsUseLoopLog())
	{
		LOG(LOG_PROG, "Loop Log Save On - Time Gap : (%u)", MGetServerConfig()->GetLoopTimeGap());
	}
	else
	{
		LOG(LOG_PROG, "Loop Log Save Off");
	}

	if( MC_KOREA == MGetLocale()->GetCountry() )
		LOG( LOG_PROG, "Server Country : KOREA" );
	else if( MC_US == MGetLocale()->GetCountry() )
		LOG( LOG_PROG, "Server Country : US" );
	else if( MC_JAPAN == MGetLocale()->GetCountry() )
		LOG( LOG_PROG, "Server Country : JAPAN" );
	else if( MC_BRAZIL == MGetLocale()->GetCountry() )
		LOG( LOG_PROG, "Server Country : BRAZIL" );
	else if( MC_INDIA == MGetLocale()->GetCountry() )
		LOG( LOG_PROG, "Server Country : INDIA" );
	else if( MC_NHNUSA == MGetLocale()->GetCountry() )
		LOG( LOG_PROG, "Server Country : NHNUSA" );
	else
	{
		ASSERT( 0 && "국가 설정을 해주세요." );
		LOG( LOG_PROG, "!!!!!!!!!Not setted country code!!!!!!!!!!!!!" );
	}
	
	LOG( LOG_PROG, "Command version : (%u)", MCOMMAND_VERSION );
	LOG( LOG_PROG, "Event usable state : (%s)", MGetServerConfig()->IsUseEvent() ? "true" : "false" );
	LOG( LOG_PROG, "Load event size : (%u)", MMatchEventFactoryManager::GetInstance().GetLoadEventSize() );
	LOG( LOG_PROG, "FileCRCCheckSum usable state : (%s)", MGetServerConfig()->IsUseFileCrc() ? "true" : "false" );
	LOG( LOG_PROG, "FileCRC size : (%u)", MMatchAntiHack::GetFielCRCSize() );
	LOG( LOG_PROG, "Country Code Filter usalbe state : (%s)", MGetServerConfig()->IsUseFilter() ? "true" : "false" );
	LOG( LOG_PROG, "Accept Invalied IP state : (%s)", MGetServerConfig()->IsAcceptInvalidIP() ? "true" : "false" );
	LOG( LOG_PROG, "Keeper IP : (%s)", MGetServerConfig()->GetKeeperIP().c_str() );
	in_addr ar;
	ar.S_un.S_addr = MGetServerConfig()->GetMonitorUDPIP();
	LOG( LOG_PROG, "Monitor IP : (%s)", inet_ntoa(ar) );
	LOG( LOG_PROG, "Ticket use : %d\n", MGetServerConfig()->IsUseTicket() );

	mlog( "===========================================================================================\n" );
	mlog( "\n" );
}

#include "MMatchBuff.h"

void MBMatchServer::OnRun(void)
{
	MMatchServer::OnRun();

	const DWORD dwGlobalClock = GetGlobalClockCount();

	MGetCheckLoopTimeInstance()->SetNHNUSAAuthTick();
#ifdef LOCALE_NHNUSA
	if( MGetServerConfig()->IsUseNHNUSAAuth() )
	{
		if( GetNHNRTA().IsElapsed(dwGlobalClock) )
		{
			GetNHNRTA().RTA( GetClientCount(), dwGlobalClock );
		}
	}
#endif

	MGetServerStatusSingleton()->SetRunStatus(113);

	MGetServerStatusSingleton()->SetRunStatus(114);

	MGetCheckLoopTimeInstance()->SetMonitorTick();
#ifdef _MONITORING
	m_Monitor.Run( dwGlobalClock );
#endif

	MGetCheckLoopTimeInstance()->SetKillTrackerTick();
	GetKillTracker().Update( dwGlobalClock );
	
	MGetServerStatusSingleton()->SetRunStatus(115);
}

void MBMatchServer::OnNetClear( const MUID& CommUID )
{
	// MMatchServer::OnNetClear( CommUID );

    MMatchObject* pObj = GetObject(CommUID);
	if (pObj)
		OnCharClear(pObj->GetUID());

	MAgentObject* pAgent = GetAgent(CommUID);
	if (pAgent)
		AgentRemove(pAgent->GetUID(), NULL);
	
	MServer::OnNetClear(CommUID);
}

#ifdef NEW_AUTH_MODULE
bool MBMatchServer::InitNetmarble()
{
#ifdef LOCALE_KOREA
	if ( MGetNetmarbleModule().InitModule() == false)
	{
		mlog( "init Netmarble failed.\n");
		return false;
	}

	mlog( "init Netmarble success.\n");
#endif

	return true;
}
#endif


void MBMatchServer::SafePushMonitorUDP( const DWORD dwIP, const WORD wPort, const char* pData, const DWORD dwDataSize )
{
#ifdef _MONITORING
	if( NULL == pData )
		return;

	m_Monitor.SafePushRecvUDP( dwIP, wPort, pData, dwDataSize );
#endif
}

bool MBMatchServer::SendMonitorUDP(const DWORD dwIP, const USHORT nPort, const string& strMonitorCommand)
{
	char* szMonitorCommand;
	szMonitorCommand = new char[ strMonitorCommand.length() ];
	strncpy( szMonitorCommand, strMonitorCommand.c_str(), strMonitorCommand.length() );
	return m_SafeUDP.Send(dwIP, nPort, szMonitorCommand, static_cast<DWORD>(strMonitorCommand.length()));
}
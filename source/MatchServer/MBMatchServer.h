#ifndef MBMATCHSERVER_H
#define MBMATCHSERVER_H

#include "MMatchServer.h"
#include "MBMatchServerConfigReloader.h"
#include "MBMatchSecurity.h"
#include "MBMatchMonitor.h"
#include "MBMatchKillTracker.h"
#include "MBMatchTest.h"

class COutputView;
class CCommandLogView;
class MAsyncJob;

class MBMatchServer : public MMatchServer
{
private :
	friend class MBMatchTest;

private :
	MBMatchServerConfigReloader m_ConfigReloader;

public:
	COutputView*		m_pView;
	CCommandLogView*	m_pCmdLogView;

	MUID m_uidKeeper;

	MBMatchSecurity		m_Security;
//	MBMatchMonitor		m_Monitor;
	MBMatchKillTracker	m_KillTracker;
	MBMatchTest			m_Tester;


protected:
	/// Create()호출시에 불리는 함수
	virtual bool OnCreate(void);
	/// Destroy()호출시에 불리는 함수
	virtual void OnDestroy(void);
	/// 커맨드를 처리하기 전에
	virtual void OnPrepareCommand(MCommand* pCommand);
	/// 사용자 커맨드 처리
	virtual bool OnCommand(MCommand* pCommand);

	virtual void OnNetClear(const MUID& CommUID);

public:
	MBMatchServer(COutputView* pView=NULL);
	~MBMatchServer();
	virtual void Shutdown();
	virtual void Log(unsigned int nLogLevel, const char* szLog);
	void OnViewServerStatus();

private :
	// 서버의 시작과 함께 등록되는 스케쥴.
	bool InitSubTaskSchedule();
	bool AddClanServerAnnounceSchedule();
	bool AddClanServerSwitchDownSchedule();
	bool AddClanServerSwitchUpSchedule();

	// 스케쥴 처리 함수.
	// 공지사항.
	void OnScheduleAnnounce( const char* pszAnnounce );
	// 클랜서버 
	void OnScheduleClanServerSwitchDown();
	void OnScheduleClanServerSwitchUp();

	const MUID GetKeeperUID() const { return m_uidKeeper; }
	void SetKeeperUID( const MUID& uidKeeper ) { m_uidKeeper = uidKeeper; }

	// MD5 Hash Value File Load
	bool LoadFileMD5HashValue();

	void WriteServerInfoLog();

protected :
	// Keeper관련.
	bool IsKeeper( const MUID& uidKeeper );

	void OnResponseServerStatus( const MUID& uidSender );
	void OnRequestServerHearbeat( const MUID& uidSender );
	void OnResponseServerHeartbeat( const MUID& uidSender );
	void OnRequestConnectMatchServer( const MUID& uidSender );
	void OnResponseConnectMatchServer( const MUID& uidSender );
	void OnRequestKeeperAnnounce( const MUID& uidSender, const char* pszAnnounce );
	void OnRequestStopServerWithAnnounce( const MUID& uidSender );
	void OnResponseStopServerWithAnnounce( const MUID& uidSender );
	void OnRequestSchedule( const MUID& uidSender, 
		const int nType, 
		const int nYear, 
		const int nMonth, 
		const int nDay, 
		const int nHour, 
		const int nMin,
		const int nCount,
		const int nCommand,
		const char* pszAnnounce );
	void OnResponseSchedule( const MUID& uidSender, 
		const int nType, 
		const int nYear, 
		const int nMonth, 
		const int nDay, 
		const int nHour, 
		const int nMin,
		const int nCount,
		const int nCommand,
		const char* pszAnnounce );
	void OnRequestKeeperStopServerSchedule( const MUID& uidSender, const char* pszAnnounce );
	void OnResponseKeeperStopServerSchedule( const MUID& uidSender, const char* pszAnnounce );
	void OnRequestDisconnectServerFromKeeper( const MUID& uidSender );
	void OnRequestReloadServerConfig( const MUID& uidSender, const string& strFileList );
	void OnResponseReloadServerConfig( const MUID& uidSender, const string& strFileList );

	// command
	void OnRequestAccountCharList(const MUID& uidPlayer);
	void OnGameKill(const MUID& uidAttacker, const MUID& uidVictim);
	void ProcessOnGameKill(MMatchStage* pStage, MMatchObject* pAttacker, MMatchObject* pVictim);
	void OnStageJoin(const MUID& uidPlayer, const MUID& uidStage);
	void OnRequestGamble(const MUID& uidPlayer, const MUID& uidItem);
	MCommandParameterBlob* MakeShopGambleItemListBlob() const;
	MCommandParameterBlob* MakeShopItemListBlob( const int nFirstItemIndex, const int nItemCount ) const;
	void OnRequestBuyItem(const MUID& uidPlayer, const unsigned long int nItemID, int nItemCount);
	bool ResponseBuyItem( MMatchObject* pObj
		, const unsigned long int nItemID
		, const int nItemCount
		, const bool bIsGambleItem
		, const DWORD dwPrice
		, const WORD dwRentHourPeriod
		, const bool bIsSpendableItem);
	const bool CheckUserCanBuyItem( MMatchObject* pObj, const int nItemID, const int nItemCnt, const DWORD dwPrice );
	bool CheckUserCanDistributeRewardItem( MMatchObject* pObj);
	void OnRequestCharacterItemList(const MUID& uidPlayer);
	void OnRequestCharacterItemListForce(const MUID& uidPlayer);
	bool ResponseCharacterItemList(const MUID& uidPlayer);
	void OnRequestSellItem(const MUID& uidPlayer, const MUID& uidItem, int nItemCount);
	
	bool IsValidSellItem(const MUID& uidPlayer, const MUID& uidItem, int nItemCount = 1);
	bool ResponseSellItem(const MUID& uidPlayer, const MUID& uidItem);
	bool ResponseSellSpendableItem(const MUID& uidPlayer, const MUID& uidItem, int nItemCount);

	bool ResponseSellGItem( const MUID& uidPlayer, const MUID& uidItem, int nItemCount);


	void OnRequestBringAccountItem(const MUID& uidPlayer, unsigned int nAIID, unsigned int nItemID, unsigned int nItemCnt);	
	void OnRequestBringBackAccountItem(const MUID& uidPlayer, const MUID& uidItem, unsigned int nItemCount);

	const bool SendGambleItemList( const MMatchObject* pObj );
	void OnRequestEquipItem(const MUID& uidPlayer, const MUID& uidItem, const long int nEquipmentSlot);
	void ResponseEquipItem(const MUID& uidPlayer, const MUID& uidItem, const MMatchCharItemParts parts);
	void OnRequestTakeoffItem(const MUID& uidPlayer, const unsigned long int nEquipmentSlot);
	void ResponseTakeoffItem(const MUID& uidPlayer, const MMatchCharItemParts parts);

	int ValidateStageJoin(const MUID& uidPlayer, const MUID& uidStage);

	// filter
	void OnLocalUpdateUseCountryFilter();
	void OnLocalGetDBIPtoCountry();
	void OnLocalGetDBBlockCountryCode();
	void OnLocalGetDBCustomIP();
	void OnLocalUpdateIPtoCountry();
	void OnLocalUpdateBlockCountryCode();
	void OnLocalUpdateCustomIP();
	void OnLocalUpdateAcceptInvaildIP();

	// 
	void OnRequestLoginNetmarble(const MUID& CommUID, const char* szAuthCookie, const char* szDataCookie, const char* szCPCookie, const char* szSpareData, int nCmdVersion, unsigned long nChecksumPack);
	void OnRequestLoginNHNUSA( const MUID& CommUID, const char* pszUserID, const char* pszAuthStr, const int nCommandVersion, const int nCheckSumPack, char* szEncryptMD5Value );
	void OnRequestLoginGameOn( const MUID& CommUID, const char* szString, const char* szStatIndex, int nCommandVersion, int nCheckSumPack, char* szEncryptMD5Value);

	// async
	void OnProcessAsyncJob( MAsyncJob* pJob );
	void OnAsyncNetmarbleLogin(MAsyncJob* pJobInput);
	void OnAsyncNHNLogin( MAsyncJob* pJob );
	void OnAsyncGameOnLogin( MAsyncJob* pJob );
	void OnAsyncGetAccountCharInfo(MAsyncJob* pJobResult);
	void OnAsyncGetCharInfo(MAsyncJob* pJobResult);
	void OnAsyncGetCpuUsage(MAsyncJob* pJobResult);	
	void OnAsyncBuyBountyItem( MAsyncJob* pJobResult );
	
	void OnAsyncSelling_Item_ToBounty( MAsyncJob* pJobResult );
	void OnAsyncSelling_SpendableItem_ToBounty( MAsyncJob* pJobResult );

	void OnAsyncGambleItem( MAsyncJob* pJobResult );
	void OnAsyncGambleQuestItem( MAsyncJob* pJobResult );		
	void OnUpdateEquipItem( MAsyncJob* pJobResult );
	void OnTakeoffItem( MAsyncJob* pJobResult );

	void OnAsyncBringAccountItem(MAsyncJob* pJobResult);
	void OnAsyncBringAccountItemStackable(MAsyncJob* pJobResult);

	void OnBringBackAccountItem( MAsyncJob* pJobResult );
	void OnBringBackAccountItemStackable( MAsyncJob* pJobResult );

	/// 사용자 루프
	virtual void OnRun(void);
	virtual void SafePushMonitorUDP( const DWORD dwIP, const WORD wPort, const char* pData, const DWORD dwDataSize );
	virtual bool SendMonitorUDP(const DWORD dwIP, const USHORT nPort, const string& strMonitorCommand);

private :
#ifdef NEW_AUTH_MODULE
	bool InitNetmarble();
#endif

public:
	void OnRequestShopItemList(const MUID& uidPlayer, const int nFirstItemIndex, const int nItemCount);
	void ResponseShopItemList(const MUID& uidPlayer, const int nFirstItemIndex, const int nItemCount);

	// ------------------------- security helper ----------------------------------

private :
	MBMatchKillTracker& GetKillTracker() { return m_KillTracker; }
	bool				IsPowerLevelingHacker( MMatchObject* pObj );
	bool				IncreaseAttackerKillCount( MMatchObject* pAttacker, MMatchObject* pVictim, const DWORD dwCurTime = timeGetTime() );

private :
	void ReleaseSecurity();
};


#endif
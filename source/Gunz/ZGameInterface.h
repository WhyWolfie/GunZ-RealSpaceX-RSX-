#ifndef _ZGAMEINTERFACE_H
#define _ZGAMEINTERFACE_H

#include "ZPrerequisites.h"
#include "ZInterface.h"
#include "ZCamera.h"
#include "ZChat.h"
#include "ZQuest.h"
#include "ZSurvival.h"
#include "ZGameType.h"
#include "ZTips.h"
#include "ZScreenDebugger.h"
#include "ZCombatMenu.h"
#include "ZMyCharacter.h"
#include "ZBandiCapturer.h" // 동영상 캡쳐
#include "ZBitmapManager.h"
/*
#include "ZGame.h"
#include "ZCharacter.h"
#include "ZCombatInterface.h"
#include "ZObserver.h"
#include "ZLoading.h"
#include "ZGameInput.h"
#include "ZMyItemList.h"
#include "ZMonsterBookInterface.h"
#include "ZInitialLoading.h"
*/

// 여기에 #include 를 달기전에 꼭 그래야만하는지 다시한번 생각해보세요 +_+  - dubble


#define LOGINSTATE_FADEIN				0
#define LOGINSTATE_SHOWLOGINFRAME		1
#define LOGINSTATE_STANDBY				2
#define LOGINSTATE_LOGINCOMPLETE		3
#define LOGINSTATE_FADEOUT				4

#if defined(_DEBUG) || defined(_RELEASE)
	#define _DUELTOURNAMENT_LOG_ENABLE_		// 듀얼토너먼트 로그(테스트 작업용)
#endif

#if defined(_DEBUG) || defined(_RELEASE)
//#define _LOG_ENABLE_CLIENT_COMMAND_			// 클라이언트 커맨드 로그(테스트 작업용)
//#define _LOG_ENABLE_OBSERVER_COMMAND_BUSH_		// 옵저버 커맨드 푸쉬 로그(테스트 작업용)
//#define _LOG_ENABLE_RELAY_COMMAND_BUSH_			// 리플레이 커맨드 푸쉬 로그(테스트 작업용)
//#define _LOG_ENABLE_OBSERVER_COMMAND_DELETE_	// 옵저버 실행된 로그(테스트 작업용)
//#define _LOG_ENABLE_REPLAY_COMMAND_DELETE_		// 리플레이 실행된 로그(테스트 작업용)
#define _LOG_ENABLE_ROUNDSTATE_					// 라운드 상태 로그(테스트 작업용)
#endif

#if defined(_DEBUG) || defined(_RELEASE)
#define _CHATOUTPUT_ENABLE_CHAR_DAMAGE_INFO_				// 각 캐릭터 데미지 로그(테스트 작업용)
#endif

class ZLocatorList;
class ZGameInput;
class ZMonsterBookInterface;
class ZShopEquipInterface;

enum ZChangeWeaponType;

class MUserDataListItem : public MDefaultListItem{
	int m_nUserData;
public:
	MUserDataListItem(const char* szText, int nUserData)
		: MDefaultListItem(szText){
			m_nUserData=nUserData;
		}

	int GetUserData() { return m_nUserData; }
};

class ZGameInterface : public ZInterface {
public:
	GunzState			m_nInitialState;
	bool				m_bTeenVersion;
	bool				m_bViewUI;
	bool				m_bTeamPlay;

	bool				m_bLoginTimeout;
	DWORD				m_dwLoginTimeout;

//	int					m_nSelectedCharacter;

	MTextAreaLook		m_textAreaLookItemDesc;	// 상점에서 아이템 설명용 TextArea의 Look
	
protected:
	ZScreenEffectManager *m_pScreenEffectManager;
	ZEffectManager*		m_pEffectManager;

	GunzState			m_nPreviousState;

	ZCombatInterface*	m_pCombatInterface;
	ZShopEquipInterface* m_pShopEquipInterface;
	ZGameInput*			m_pGameInput;
	ZLoading*			m_pLoadingInterface;
	ZPlayerMenu*		m_pPlayerMenu;

	static ZGameClient*	m_spGameClient;
	ZGame*				m_pGame;
	ZCamera				m_Camera;
	ZChat				m_Chat;
	ZQuest				m_Quest;					///< 퀘스트 관련들	(이건 로직 객체인데 왜 인터페이스에 들어있는건가..)
	ZSurvival			m_Survival;					// 서바이벌
	ZGameTypeManager	m_GameTypeManager;			///< 게임타입 관련들
//	ZClan				m_Clan;
	ZMiniMap			*m_pMiniMap;
	ZTips				m_Tips;

	ZBandiCapturer*		m_Capture;					
	
	ZScreenDebugger		m_ScreenDebugger;			///< 화면에 나오는 디버그 화면
	ZCombatMenu			m_CombatMenu;				///< 게임중 메뉴

	ZMyCharacter*		m_pMyCharacter;

	ZMonsterBookInterface* m_pMonsterBookInterface;


	
	bool				m_bShowInterface;

	bool				m_bCursor;					///< 커서를 사용할 수 있는 상태
	LPDIRECT3DSURFACE9	m_pCursorSurface;

	DWORD				m_dwFrameMoveClock;

	ZIDLResource		m_IDLResource;

	GunzState			m_nState;			///< 현재 상태
	bool				m_bLogin;			///< Login 되었는가?

	bool				m_bLoading;
	bool				m_bWaitingArrangedGame;

	MBitmap				*m_pMapThumbnail;///< 맵 썸네일

	ZMsgBox*				m_pMsgBox;
	ZMsgBox*				m_pConfirmMsgBox;
	ZInterfaceBackground*	m_pBackground;
	ZCharacterSelectView*	m_pCharacterSelectView;

	bool				m_bOnEndOfReplay;		// 리플레이 보구나면 플레이어의 Level Percent가 바뀌기 때문에 리플레이 시작 전에
	int					m_nLevelPercentCache;	// m_bOnEndOfReplay를 true로 셋한 다음 m_nLevelPercentCache에 현재 LevelPercent
												// 값을 저장해 놓구서 끝나면 다시 복원한다. 좀 안좋은 구조... 방법이 없음. -_-;

	unsigned long int	m_nDrawCount;

	bool			m_bReservedWeapon;
	ZChangeWeaponType m_ReservedWeapon;

	bool			m_bLeaveBattleReserved;
	bool			m_bLeaveStageReserved;
	DWORD			m_dwLeaveBattleTime;


	int				m_nLoginState;
	DWORD			m_dwLoginTimer;
	DWORD			m_dwRefreshTime;
	int				m_nLocServ;

	MBitmapR2*		m_pRoomListFrame;							// 게임방 리스트 프레임 이미지
	MBitmapR2*		m_pDuelTournamentLobbyFrame;				// 듀얼토너먼트 채널일때의 프레임 이미지
	MBitmapR2*		m_pBottomFrame;								// 하단 정보창 프레임 이미지
	MBitmapR2*		m_pClanInfoBg;								// 클랜 정보 배경 이미지
	MBitmapR2*		m_pDuelTournamentInfoBg;					// 듀얼토너먼트 전적 정보 배경 이미지
	MBitmapR2*		m_pDuelTournamentRankingLabel;				// 듀얼토너먼트 랭킹 리스트 상단 레이블 이미지
	MBitmapR2*		m_pLoginBG;									// 로그인 배경 이미지
	MBitmapR2*		m_pLoginPanel;								// 로그인 패널 이미지

	ZBitmapManager<int> m_ItemThumbnailMgr;							// 상점/장비창/듀얼토너먼트 게임중에 보여질 아이템 썸네일 매니저
	//↑적절한 시기마다 모두 언로드해서 메모리를 낭비하지 않도록 해야 합니다. (상점 나갈때, 장비창 나갈때, 듀얼토너먼트 게임 나갈때)

	ZLocatorList*	m_pLocatorList;
	ZLocatorList*	m_pTLocatorList;

	DWORD			m_dwTimeCount;								// 게임 경과시간 카운트. 청소년 자율규제 적용안 쓰불...
	DWORD			m_dwHourCount;								// 게임 경과시간(hour) 카운트. 청소년 자율규제 적용안 쓰불...

	DWORD			m_dwVoiceTime;								// 현재 출력중인 보이스 사운드 시간
	char			m_szCurrVoice[ 256];						// 현재 출력하는 보이스 파일 이름
	char			m_szNextVoice[ 256];						// 다음에 출력할 보이스 파일 이름
	DWORD			m_dwNextVoiceTime;							// 다음에 출력할 보이스 사운드 시간

	int				m_nRetryCount;

	bool			m_bReservedQuit;
	DWORD			m_dwReservedQuitTimer;

	bool			m_bReserveResetApp;							// for changing language

	static bool		m_bSkipGlobalEvent;

	DWORD			m_MyPort;

	DWORD			m_dErrMaxPalyerDelayTime;
	DWORD			m_bErrMaxPalyer;

	bool			m_bGameFinishLeaveBattle;		// 배틀에서 나갈때 스테이지 종료 여부

	list<MCommand*>	m_listDelayedGameCmd;

// _DUELTOURNAMENT
	vector<DTPlayerInfo> m_vecDTPlayerInfo;
	MDUELTOURNAMENTTYPE m_eDuelTournamentType;

protected:
	static bool		OnGlobalEvent(MEvent* pEvent);
	virtual bool	OnEvent(MEvent* pEvent, MListener* pListener);
	bool			OnDebugEvent(MEvent* pEvent, MListener* pListener);
	virtual bool	OnCommand(MWidget* pWidget, const char* szMessage);
	static bool		OnCommand(MCommand* pCommand);

	bool ResizeWidget(const char* szName, int w, int h);
	bool ResizeWidgetRecursive( MWidget* pWidget, int w, int h);
	void SetListenerWidget(const char* szName, MListener* pListener);

	void UpdateCursorEnable();
	void UpdateDuelTournamentWaitMsgDots();

//	void LoadCustomBitmap();
	bool InitInterface(const char* szSkinName,ZLoadingProgress *pLoadingProgress = NULL);
	bool InitInterfaceListener();
	void FinalInterface();

	void LoadBitmaps(const char* szDir, const char* szSubDir, ZLoadingProgress *pLoadingProgress);

	void LeaveBattle();

	void OnGreeterCreate(void);
	void OnGreeterDestroy(void);

	void OnLoginCreate(void);
	void OnLoginDestroy(void);

	void OnDirectLoginCreate(void);
	void OnDirectLoginDestroy(void);

	void OnNetmarbleLoginCreate(void);
	void OnNetmarbleLoginDestroy(void);

	void OnGameOnLoginCreate(void);
	void OnGameOnLoginDestroy(void);

	void OnLobbyCreate(void);
	void OnLobbyDestroy(void);

	void OnStageCreate(void);
	void OnStageDestroy(void);

	void OnCharSelectionCreate(void);
	void OnCharSelectionDestroy(void);

	void OnCharCreationCreate(void);
	void OnCharCreationDestroy(void);

	void OnShutdownState();

#ifdef _BIRDTEST
	void OnBirdTestCreate();
	void OnBirdTestDestroy();
	void OnBirdTestUpdate();
	void OnBirdTestDraw();
	void OnBirdTestCommand(MCommand* pCmd);
#endif

	void OnUpdateGameMessage(void);

	void HideAllWidgets();

	void OnResponseShopItemList( const vector< MTD_ShopItemInfo*> &vShopItemList  , const vector<MTD_GambleItemNode*>& vGItemList );
	void OnResponseCharacterItemList(MUID* puidEquipItem
		, MTD_ItemNode* pItemNodes
		, int nItemCount
		, MTD_GambleItemNode* pGItemNodes
		, int nGItemCount );

	void OnSendGambleItemList( void* pGItemArray, const DWORD dwCount );

	void OnDrawStateGame(MDrawContext* pDC);
	void OnDrawStateLogin(MDrawContext* pDC);
	void OnDrawStateLobbyNStage(MDrawContext* pDC);
	void OnDrawStateCharSelection(MDrawContext* pDC);

#ifdef _QUEST_ITEM
	void OnResponseCharacterItemList_QuestItem( MTD_QuestItemNode* pQuestItemNode, int nQuestItemCount );
	void OnResponseBuyQuestItem( const int nResult, const int nBP );
	void OnResponseSellQuestItem( const int nResult, const int nBP );
#endif

	void OnResponseServerStatusInfoList( const int nListCount, void* pBlob );
	void OnResponseBlockCountryCodeIP( const char* pszBlockCountryCode, const char* pszRoutingURL );

	// locator관련.
	void RequestServerStatusListInfo();

/*
	GUNZ_NA = 0,
	GUNZ_GAME = 1,
	GUNZ_LOGIN = 2,
	GUNZ_NETMARBLELOGIN = 3,
	GUNZ_LOBBY = 4,
	GUNZ_STAGE = 5,
	GUNZ_GREETER = 6,
	GUNZ_CHARSELECTION = 7,
	GUNZ_CHARCREATION = 8,
	GUNZ_PREVIOUS = 10,
	GUNZ_SHUTDOWN = 11,
	GUNZ_BIRDTEST
*/
public:
	ZGameInterface(const char* szName=NULL, MWidget* pParent=NULL, MListener* pListener=NULL);
	~ZGameInterface();

	static bool m_sbRemainClientConnectionForResetApp;	// 언어를 바꾸어서 리소스를 다시 로딩해야 할때 이걸 true로 해줘야 함

	bool OnCreate(ZLoadingProgress *pLoadingProgress);
	void OnDestroy();

	void OnInvalidate();
	void OnRestore();

	bool Update(float fElapsed);
	void OnDraw(MDrawContext *pDC);

	void SetCursorEnable(bool bEnable);
	void OnResetCursor();
	bool IsCursorEnable() { return m_bCursor; }

	bool SetState(GunzState nState);
	GunzState GetState(void){ return m_nState; }
	
	void UpdateBlueRedTeam(void);		// 동환이가 추가

	void ChangeToCharSelection(void);	///< 캐릭터 선택으로 이동

	bool ChangeInterfaceSkin(const char* szNewSkinName);

	/// 해당하는 이름을 아이템으로 가진 위젯의 Visible상태를 바꾼다.
	bool ShowWidget(const char* szName, bool bVisible, bool bModal=false);
	void SetTextWidget(const char* szName, const char* szText);
	void EnableWidget(const char* szName, bool bEnable);

	// 나중에 지울것..우선 당장 테스트를 위해
	void TestChangeParts(int mode);
	void TestChangePartsAll();
	void TestChangeWeapon(RVisualMesh* pVMesh = NULL);
	void TestToggleCharacter();

	void ChangeParts(int mode);
	void ChangeWeapon(ZChangeWeaponType nType);
	
	void Reload();

	void RespawnMyCharacter();	// 혼자테스트할때 클릭하면 되살아난다.

	void ReserveLeaveStage();	// 스테이지에서 나갈때 일정시간 흐른뒤 나간다
	void ReserveLeaveBattle();	// 대기방으로 나갈떄 일정시간 흐른뒤 나간다
	void FinishGame(void);
	bool IsLeaveBattleReserved() { return m_bLeaveBattleReserved; }

	void ReserveResetApp(bool b)	{ m_bReserveResetApp = b; }
	bool IsReservedResetApp()		{ return m_bReserveResetApp; }

	void SaveScreenShot();

	void ShowMessage(const char* szText, MListener* pCustomListenter=NULL, int nMessageID=0);
	void ShowConfirmMessage(const char* szText, MListener* pCustomListenter=NULL);
	void ShowMessage(int nMessageID);
	void ShowErrorMessage(int nErrorID);
	void ShowErrorMessage(const char* szErrorMsg, int nErrorID);
	
	void ShowInterface(bool bShowInterface);
	bool IsShowInterface() { return m_bShowInterface; }

	void SetTeenVersion(bool bt) { m_bTeenVersion = bt; }
	bool GetTeenVersion() { return m_bTeenVersion; }

	void OnCharSelect(void);


	// GunzState에 따른 Create/Destroy 핸들러
	bool OnGameCreate(void);
	void OnGameDestroy(void);
	void OnGameUpdate(float fElapsed);


	// 로비 UI 설정
//	void SetupPlayerListButton(int index=-1);
//	void SetupPlayerListTab();
	void OnArrangedTeamGameUI(bool bFinding);
	void OnDuelTournamentGameUI(bool bWaiting);

	void InitLobbyUIByChannelType();

	void InitLadderUI(bool bLadderEnable);
	void InitClanLobbyUI(bool bClanBattleEnable);
	void InitDuelTournamentLobbyUI(bool bEnableDuelTournamentUI);
	void InitChannelFrame(MCHANNEL_TYPE nChannelType);

//	bool InitLocatorList( MZFileSystem* pFileSystem, const char* pszLocatorList );

	// 스테이지 UI 설정
	void SetMapThumbnail(const char* szMapName);
	void ClearMapThumbnail();
	void SerializeStageInterface();

	void EnableLobbyInterface(bool bEnable);
	void EnableStageInterface(bool bEnable);
	void ShowPrivateStageJoinFrame(const char* szStageName);

	void SetRoomNoLight( int d );


	// 상점및 장비
	void ShowEquipmentDialog(bool bShow=true);
	void ShowShopDialog(bool bShow=true);

	// 캐릭터 선택
	void ChangeSelectedChar( int nNum);


	// 리플레이
	void ShowReplayDialog( bool bShow);
	void ViewReplay( void);


	void ShowMenu(bool bEnable);
	void Show112Dialog(bool bShow);
	bool IsMenuVisible();

	bool OpenMiniMap();
	bool IsMiniMapEnable();

	void RequestQuickJoin();

	void EnableCharSelectionInterface(bool bEnable);

public:


	// 클랜생성이나 래더팀게임초대를 받을수 있는 상태인가 ?
	bool IsReadyToPropose();

	// 리플레이
	void OnReplay();

	void OnDisconnectMsg( const DWORD dwMsgID );
	void ShowDisconnectMsg( DWORD errStrID, DWORD delayTime );

	void OnAnnounceDeleteClan( const string& strAnnounce );

	// 퀘스트 아이템 아이콘 비트맵 얻기(쓰는덴 많은데 마땅히 둘데가 없어서... -_-;)
	MBitmap* GetQuestItemIcon( int nItemID, bool bSmallIcon);

	// ZActionKey 입력중 GlobalEvent 무력화
	static bool CheckSkipGlobalEvent() { return m_bSkipGlobalEvent; }
	void SetSkipGlobalEvent(bool bSkip) { m_bSkipGlobalEvent = bSkip; }

	// 보이스 사운드 출력
	void OnVoiceSound();
	void PlayVoiceSound( char* pszSoundName, DWORD time=0);

	void SetAgentPing(DWORD nIP, DWORD nTimeStamp);

	void SetErrMaxPlayerDelayTime(DWORD dDelayTime) { m_dErrMaxPalyerDelayTime = dDelayTime; }
	DWORD GetErrMaxPlayerDelayTime() { return m_dErrMaxPalyerDelayTime; }
	void SetErrMaxPlayer(bool bErrMaxPalyer) { m_bErrMaxPalyer = bErrMaxPalyer; }
	bool IsErrMaxPlayer() { return m_bErrMaxPalyer == 0 ? false : true; }

	virtual void MultiplySize(float byIDLWidth, float byIDLHeight, float byCurrWidth, float byCurrHeight);

// _DUELTOURNAMENT
	void SetDuelTournamentCharacterList(MDUELTOURNAMENTTYPE nType, const vector<DTPlayerInfo>& vecDTPlayerInfo);
	const vector<DTPlayerInfo>& GetVectorDTPlayerInfo()	{ return m_vecDTPlayerInfo; }
	void SetDuelTournamantType(MDUELTOURNAMENTTYPE eType)		{ m_eDuelTournamentType = eType; }
	MDUELTOURNAMENTTYPE GetDuelTournamentType()		{ return m_eDuelTournamentType; }

	void UpdateDuelTournamantMyCharInfoUI();
	void UpdateDuelTournamantMyCharInfoPreviousUI();

// 릴레이맵
	bool GetIsGameFinishLeaveBattle()			{ return m_bGameFinishLeaveBattle; }

	// 바깥에서 얻을만한 인터페이스들 (__forceinline 는 dll-injection 핵 방해를 위한 것)
	__forceinline ZGameClient* GetGameClient(void)			{ return m_spGameClient; }
	__forceinline ZGame* GetGame(void)						{ return m_pGame; }
	__forceinline ZCombatInterface* GetCombatInterface(void) { return m_pCombatInterface; }
	__forceinline ZShopEquipInterface* GetShopEquipInterface(void) { return m_pShopEquipInterface; }
	__forceinline ZCamera* GetCamera()						{ return &m_Camera; }
	__forceinline ZCharacter*	GetMyCharacter()			{ return (ZCharacter*)m_pMyCharacter; }
	__forceinline ZBaseQuest* GetQuest();					// 퀘스트/서바이벌 중 현재 모드에 맞는 걸 반환
	__forceinline ZQuest* GetQuestExactly()					{ return &m_Quest; }		// 퀘스트 객체를 반환
	__forceinline ZSurvival* GetSurvivalExactly()			{ return &m_Survival; }		// 서바이벌 객체를 반환
	__forceinline ZChat* GetChat()							{ return &m_Chat; }
	__forceinline ZGameTypeManager* GetGameTypeManager()		{ return &m_GameTypeManager; }


	ZScreenEffectManager* GetScreenEffectManager() { return m_pScreenEffectManager; }
	ZEffectManager* GetEffectManager()			{ return m_pEffectManager; }
	void SetGameClient(ZGameClient* pGameClient){ m_spGameClient = pGameClient; }
	
	ZCharacterSelectView* GetCharacterSelectView() { return m_pCharacterSelectView; }
	ZIDLResource* GetIDLResource(void)			{ return &m_IDLResource; }
	ZPlayerMenu* GetPlayerMenu()				{ return m_pPlayerMenu; }
	ZMiniMap*	GetMiniMap()					{ return m_pMiniMap; }
	
	ZTips* GetTips()							{ return &m_Tips; }
	ZBandiCapturer*	GetBandiCapturer()			{ return m_Capture; }					///< 동영상 캡쳐...by kammir 2008.10.02
	ZScreenDebugger* GetScreenDebugger()		{ return &m_ScreenDebugger; }
	ZCombatMenu*	 GetCombatMenu()			{ return &m_CombatMenu; }
	ZMonsterBookInterface* GetMonsterBookInterface()	{ return m_pMonsterBookInterface; }

	ZBitmapManager<int>* GetItemThumbnailMgr()		{ return &m_ItemThumbnailMgr; }
};

__forceinline ZBaseQuest* ZGameInterface::GetQuest()
{
	if (m_pGame && m_pGame->GetMatch())
	{
		MMATCH_GAMETYPE gameType = ZGetGame()->GetMatch()->GetMatchType();

		if (m_GameTypeManager.IsQuestOnly( gameType ))
			return static_cast<ZBaseQuest*>(&m_Quest);
		else if (m_GameTypeManager.IsSurvivalOnly( gameType ))
			return static_cast<ZBaseQuest*>(&m_Survival);
	}
	
	//ASSERT(0);
	return static_cast<ZBaseQuest*>(&m_Quest);
}


#define BEGIN_WIDGETLIST(_ITEM, _IDLRESPTR, _CLASS, _INSTANCE)								\
{																							\
	MWidgetList WidgetList;																	\
	(_IDLRESPTR)->FindWidgets(WidgetList, _ITEM);											\
	for (MWidgetList::iterator itor = WidgetList.begin(); itor != WidgetList.end(); ++itor) \
{																							\
	if ((*itor) != NULL)																	\
{																							\
	_CLASS _INSTANCE = ((_CLASS)(*itor));

#define END_WIDGETLIST()		}}}


#define DEFAULT_INTERFACE_SKIN "Default"


#define WM_CHANGE_GAMESTATE		(WM_USER + 25)
void ZChangeGameState(GunzState state);		/// 쓰레드에 안전하기 위해서는 만듦


//void ZLoadBitmap(const char* szDir, const char* szFilter, bool bAddDirToAliasName = false);

inline void GetDuelTournamentGradeIconFileName(char* out_sz, int grade)
{
	sprintf(out_sz, "dt_grade%d.png", grade);
}

char* GetItemSlotName( const char* szName, int nItem);
bool SetWidgetToolTipText(char* szWidget,const char* szToolTipText, MAlignmentMode mam=MAM_LEFT|MAM_TOP);

#endif
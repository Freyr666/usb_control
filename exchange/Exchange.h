#pragma once

//CyApi
#include "CyAPI.h"
#include "GlobalVars.h"
#include "DSPMessages.h"

/*error buffer
[0] - video loss
[1] - video freeze
[2] - video low motion
[3] - video black
[4] - video almost black
[5] - video blockiness_1
[6] - video blockiness_2
[7]	- reserved for future use
[8]	- reserved for future use
[9]	- reserved for future use

[10] - audio loss
[11] - audio high
[12] - audio almost high
[13] - audio low
[14] - audio almost low
[15] - reserved for future use
[16] - reserved for future use
[17] - reserved for future use
[18] - reserved for future use
[19] - reserved for future use
*/

class CExchange
{
private:
	static bool got_errors;
	static HWND mainWnd;
	
	static USER_STREAM_INFO fullSI;
	static USER_STREAM_INFO userSI;
	static SAT_STATUS dvbStatus;
	static UAT_INFO dvbInfo;

	static BYTE settings_version;
	static BYTE dvb_status_version;
	static BYTE dvb_settings_version;
	static BYTE status_version;

	static UINT idOutTimer;
	static UINT idInTimer;
	static PCHAR pLastErrorMsg;

	static BOOL SendData(PUCHAR data, LONG xfer, BOOL com_sopr = FALSE);
	static BOOL ReceiveData(PUCHAR data, PLONG xfer);

	static void SetLastErrorMsg(PCHAR pErrMsg);
	static void SetMsgNoError();

	static BOOL SendTypeVersion();
	static BOOL SendExitReceive();
	static BOOL SendStatus();
	static BOOL SendError();
	static BOOL SendIPAnswer();
	static BOOL SendAnalisVideoAnswer();
	static BOOL SendAnalisAudioAnswer();
	static BOOL SendDVBStatusAnswer();
	static BOOL SendDVBSettingsAnswer();
	static BOOL ProceedData(WORD*, long);
	static BOOL SendPrgList();	// added by TeddyJack
	static BOOL SendIPSettings(IP_SETTINGS_ONLY*);
	static BOOL SendLEDs();
	static BOOL ChangeReceiveFIFO(BYTE);
	static BOOL SendIPSettRdReq();
	static BOOL ProceedDtmData(WORD*, long, IP_SETTINGS_ONLY*);
	static void MessageDisassembler(WORD* buffer, USER_STREAM_INFO* pSI);	// added by TeddyJack

	static VOID CALLBACK ErrorTimer(HWND, UINT, UINT_PTR, DWORD);
	static VOID CALLBACK InTimer(HWND, UINT, UINT_PTR, DWORD);

	static void FillErrorBufs(ERR_INFO*, int num, int chainNum);
protected:
	static WORD ip_mode_req_id		;
	static WORD dec_vid_req_id		;
	static WORD dec_aud_req_id		;
	static WORD analis_vid_req_id	;
	static WORD analis_aud_req_id	;
	static WORD prg_list_req_id		;			// Added by TeddyJack
	static WORD dvb_status_req_id	;
	static WORD dvb_set_req_id		;
	static WORD ip_mode_clnt_id		;
	static WORD dec_vid_clnt_id		;
	static WORD dec_aud_clnt_id		;
	static WORD analis_vid_clnt_id	;
	static WORD analis_aud_clnt_id	;
	static WORD prg_list_clnt_id	;			// Added by TeddyJack
	static WORD dvb_status_clnt_id	;
	static WORD dvb_set_clnt_id		;
public:
	CExchange(void) { };
	~CExchange(void) { CloseDevice(); };
	
	static BOOL SetStatus(BYTE*, BYTE*, BYTE);
	static BOOL SetData(WORD errCnt, WORD chainNum, WORD errNum);
	static HRESULT InitializeDevice();
	static HRESULT CloseDevice();
	static BOOL SetSettings(EXCHANGE_SETTINGS*);
	static PCHAR GetLastErrorMsg();
	static void IncrementDVBStatus();
	static void IncrementDVBSettings();
	static void IncrementSettings();
	static void SetFullStreamInfo(USER_STREAM_INFO* pSI);
	static void SetUserStreamInfo(USER_STREAM_INFO* pSI);
	static void SetDVBStatus(SAT_STATUS* pStat);
	static void SetDVBInfo(UAT_INFO* pInfo);
	static BOOL CheckDTM3200Settings();
	static void SetMainWnd(HWND hWnd = NULL);
};
#include "stdafx.h"
#include "Exchange.h"
#include "Strings.h"
#include "DSPipeline.h"
#include "Rtp_settingsDlg.h"
#include "AnalysisSetDlg.h"
#include "TunerSetDlg.h"
#include <time.h>

#define MIN_PERIOD		1
#define dev_ok			0x0000
#define no_dev			-1
#define dev_uncomp		-2
#define dev_many		-3

#define USB_BUF_SIZE	64
#define MAX_MSG_SIZE	256*2	//256

#define CUR_VERSION		0x3
#define CUR_TYPE		0x2

#define B sizeof(USER_PIDS_INFO) / 2																			// кол-во слов, которое занимает структура user_pids_info
#define A sizeof(USER_PROG_INFO) / 2 - STREAM_INFO_PIDS_PER_PROGRAM*B											// кол-во слов, которое занимает структура user_prog_info без части user_pids_info
#define USER_STREAM_INFO_MAIN_SIZE (sizeof(USER_STREAM_INFO) - sizeof(USER_PROG_INFO)*MAX_ANALYZED_PROG_NUM)/2	// кол-во слов в user stream info до начала массива структур user prog info
#define USER_PROG_INFO_MAIN_SIZE (sizeof(USER_PROG_INFO) - 2*MAX_PROG_NAME - 2*sizeof(WORD) - sizeof(USER_PIDS_INFO)*STREAM_INFO_PIDS_PER_PROGRAM)/2
#define MAX_PRG_LIST_WORDS  USER_STREAM_INFO_MAIN_SIZE + (A + B * 2) * MAX_ANALYZED_PROG_NUM					// кол-во слов если 10 программ по 2 пида в каждой

static CCyUSBDevice *USBDevice = NULL;

HWND CExchange::mainWnd = NULL;
WORD CExchange::ip_mode_req_id = 0;
WORD CExchange::dec_vid_req_id = 0;
WORD CExchange::dec_aud_req_id = 0;
WORD CExchange::analis_vid_req_id = 0;
WORD CExchange::analis_aud_req_id = 0;
WORD CExchange::prg_list_req_id = 0;			// Added by TeddyJack
WORD CExchange::dvb_status_req_id = 0;
WORD CExchange::dvb_set_req_id = 0;
WORD CExchange::ip_mode_clnt_id = 0;
WORD CExchange::dec_vid_clnt_id = 0;
WORD CExchange::dec_aud_clnt_id = 0;
WORD CExchange::analis_vid_clnt_id = 0;
WORD CExchange::analis_aud_clnt_id = 0;
WORD CExchange::prg_list_clnt_id = 0;			// Added by TeddyJack
WORD CExchange::dvb_status_clnt_id = 0;
WORD CExchange::dvb_set_clnt_id = 0;

BYTE CExchange::settings_version = 0;
BYTE CExchange::status_version = 0;
BYTE CExchange::dvb_settings_version = 0;
BYTE CExchange::dvb_status_version = 0;
UINT CExchange::idInTimer = 0;
UINT CExchange::idOutTimer = 0;
PCHAR CExchange::pLastErrorMsg = NULL;
USER_STREAM_INFO CExchange::fullSI = 0;
USER_STREAM_INFO CExchange::userSI = 0;
SAT_STATUS CExchange::dvbStatus;
UAT_INFO CExchange::dvbInfo;
bool CExchange::got_errors = false;


WORD state				= 0;
WORD msg_cnt			= 0;
WORD client_id			= 0;
WORD app_conn_cnt		= 0;

LONG length						= USB_BUF_SIZE;
bool		initialize_flag		= FALSE;
static BOOL incomplete_msg_flag = FALSE;

STATUS_MSG stat_msg;				//сообщение статуса

WORD err_cnt[MAX_ANALYZED_PROG_NUM][ERR_NUM];

WORD prg_list_buffer[MAX_PRG_LIST_WORDS];
BOOL first_msg_flag = TRUE;							// флаг что Лёня передаёт первую программу
BOOL last_msg_flag = FALSE;
WORD curr_prog_index = 0;
WORD num_of_progs = 0;

void CExchange::SetMainWnd(HWND hWnd)
{
	mainWnd = hWnd;
}

BOOL CExchange::SetData(WORD errCnt, WORD chainNum, WORD errNum)
{
	got_errors = true;
	err_cnt[chainNum][errNum] += errCnt;

	return TRUE;
}

BOOL CExchange::SetStatus(BYTE* status_data, BYTE* loudness_levels, BYTE stream_flags)
{
	stat_msg.flags						= stream_flags;
	for(int i = 0; i < MAX_ANALYZED_PROG_NUM; i++)
	{
		stat_msg.err_flags[i]			= status_data[i];
		stat_msg.loudness_levels[i]		= loudness_levels[i];
	}
	return TRUE;
}

BOOL CExchange::SetSettings(EXCHANGE_SETTINGS* user_set)
{
	if(user_set->send_period >= MIN_PERIOD)
		user_set->send_period	= user_set->send_period;				//?????
	else
	{
		SetLastErrorMsg(short_send_period_txt[language]);
		return FALSE;
	}

	if(user_set->send_enable)
	{
		if(idOutTimer) KillTimer(NULL, idOutTimer);
		idOutTimer = SetTimer(NULL, NULL, user_set->send_period*mSEC_in_SEC, &CExchange::ErrorTimer);
		if(!idOutTimer)
		{
			SetLastErrorMsg(failed_start_send_timer_txt[language]);
			return FALSE;
		}

		if(idInTimer) KillTimer(NULL, idInTimer);
		idInTimer = SetTimer(NULL, NULL, UINT(mSEC_in_SEC)/4, &CExchange::InTimer);
		if(!idInTimer)
		{
			if(idOutTimer) KillTimer(NULL, idOutTimer);									//на случай, если удалось запустить первый таймер и не удалось запустить второй
			SetLastErrorMsg(failed_start_receive_timer_txt[language]);
			return FALSE;
		}
	}
	else
	{
		if(idOutTimer)
			KillTimer(NULL, idOutTimer);
		if(idInTimer) 
			KillTimer(NULL, idInTimer);
	}

	SetLastErrorMsg(settings_applied_txt[language]);

	return TRUE;
}

BOOL CExchange::SendData(PUCHAR data, LONG xfer, BOOL com_sopr)
{
	if (!USBDevice) return FALSE;
	LONG msg_size = 0;
	BYTE buffer[USB_BUF_SIZE];
	LONG in_buf_cnt = 0;
	
	if(com_sopr == FALSE)
	{
		buffer[0] = 0x69;
		buffer[1] = 0x96;
		
		while(xfer > 62)
		{
			msg_size = 64;
			for(int i = 0; i < 62; i++, in_buf_cnt++)
			{
				*(buffer+2+i) = *(data+in_buf_cnt);
			}
			if(!USBDevice->BulkOutEndPt->XferData(buffer, msg_size))
				return FALSE;
			xfer -=62;
		}
		msg_size = xfer + 2;
		for(int i = 0; i < xfer; i++, in_buf_cnt++)
		{
			*(buffer+2+i) = *(data+in_buf_cnt);
		}
		if(USBDevice->BulkOutEndPt)
			if(USBDevice->BulkOutEndPt->XferData(buffer, msg_size))
				return TRUE;
	}
	else
	{
		buffer[0] = 0x96;
		buffer[1] = 0x69;
		msg_size = xfer + 2;
		for(int i = 0; i < xfer; i++, in_buf_cnt++)
		{
			*(buffer+2+i) = *(data+in_buf_cnt);
		}
		if(USBDevice->BulkOutEndPt)
			if(USBDevice->BulkOutEndPt->XferData(buffer, msg_size))
				return TRUE;
	}
	return FALSE;
}

void CExchange::SetLastErrorMsg(PCHAR pErrMsg)
{
	pLastErrorMsg = pErrMsg;
}

PCHAR CExchange::GetLastErrorMsg()
{
	return pLastErrorMsg;
}

void CExchange::SetMsgNoError()
{
	pLastErrorMsg = success[language];
}

HRESULT CExchange::InitializeDevice()
{
	initialize_flag = FALSE;
	if(!USBDevice)
		USBDevice = new CCyUSBDevice;
	else
		USBDevice->Close();

	if (USBDevice->DeviceCount() == 1)
	{
		if(!USBDevice->IsOpen())
			USBDevice->Open(0);
		//Get config descriptor
		USB_CONFIGURATION_DESCRIPTOR ConfDesc;
		USBDevice->GetConfigDescriptor(&ConfDesc);	
		if(ConfDesc.bNumInterfaces==1)
		{
			USB_INTERFACE_DESCRIPTOR IntfDesc;
			USBDevice->GetIntfcDescriptor(&IntfDesc);
			if(IntfDesc.bAlternateSetting==0)
			{
				if(IntfDesc.bNumEndpoints==2)
				{
					SetMsgNoError();
					return dev_ok;
				}
				else
				{
					SetLastErrorMsg(dev_found_uncomp[language]);
					return dev_uncomp;
				}
			}
			else
			{
				SetLastErrorMsg(dev_found_uncomp[language]);	
				return dev_uncomp;
			}
		}
		else
		{
			SetLastErrorMsg(dev_found_uncomp[language]);
			return dev_uncomp;
		}
	}
	else if(USBDevice->DeviceCount() > 1)
	{
		SetLastErrorMsg(many_dev_found[language]);
		return dev_many;
	}
	else
	{
		SetLastErrorMsg(dev_not_found[language]);
		return no_dev;
	}
}

HRESULT CExchange::CloseDevice()
{
	initialize_flag = FALSE;

	if(idOutTimer)	KillTimer(NULL, idOutTimer);
	if(idInTimer)	KillTimer(NULL, idInTimer);

	if(USBDevice)
	{
		delete USBDevice;
		USBDevice = NULL;
	}
	return S_OK;
}

VOID CALLBACK CExchange::ErrorTimer(HWND hwnd,UINT uMsg,UINT_PTR idEvent,DWORD dwTime)		//функция, вызывающаяся по таймеру для отправки ошибок по USB
{
	if(!initialize_flag)
	{
		SendTypeVersion();
		return;
	}
	//SendError();
	SendStatus();
	SendExitReceive();

	SendLEDs();
}

VOID CALLBACK CExchange::InTimer(HWND hwnd,UINT uMsg,UINT_PTR idEvent,DWORD dwTime)
{
	PUCHAR data = new UCHAR[USB_BUF_SIZE];
	ZeroMemory(data, USB_BUF_SIZE);
	length = 64;
	WORD word = 0;

	if(ReceiveData(data, &length))
	{
		if(length)
			ProceedData(((WORD*)data), length/2);
	}

	delete [] data;
}

BOOL CExchange::ReceiveData(PUCHAR data, PLONG xfer)
{
	if(USBDevice->BulkInEndPt)
	{
		USBDevice->BulkInEndPt->TimeOut = 1000;
		if(USBDevice->BulkInEndPt->XferData(data, *xfer))
			return TRUE;
		else
			return FALSE;
	}
	*xfer = 0;
	return FALSE;
}

BOOL CExchange::ProceedData(WORD* buf, long len)
{
	static ANALYSIS_PARAMS ap;
	static IP_SETTINGS ips;
	static ALL_DVB_TUNER_SETTINGS ts;
	static int length_data = 0;
	static WORD offset = 0;	// смещение для записи в буфер списка программ
	DWORD temp = 0;

	for(int i = 0; i < len; i ++)
	{
		if(*(buf+i) == PREFIX)
		{
			state	= PREFIX;
			msg_cnt = 0;
			continue;
		}

		if((len == USB_BUF_SIZE/2) && (i == len - 1))
			incomplete_msg_flag = TRUE;

		switch(state)
		{
		case PREFIX:
			//в данном случае обрабатываются сообщения типа 1.1, которые не несут полезной нагрузки
			if(*(buf+i) == GET_BOARD_INFO)		//нужно послать информацию о типе и версии программы
			{
				initialize_flag = FALSE;		//обнулили флаг инициализации, теперь программа будет заново посылать сообщение о типе и версии устройства
			}
			else if(*(buf+i) == GET_BOARD_MODE)	//нужно послать информацию о режиме работы платы
			{

			}
			else
				state = *(buf+i);
			break;
		case SET_BOARD_MODE:	//4 параметра
			//нужно установить полученный режим работы
			if(msg_cnt == 0)
			{
				initialize_flag = TRUE;
				msg_cnt++;
			}
			break;
		case MEGABUF_STATUS:	//1 параметр
			//получаем статус мегабуфера
			break;
		case MEGABUF_RESULT:	//1 параметр
			//получаем ответ об ошибках приема данных мегабуфера
			break;
		case CLOSE_CLIENT:		//2 параметра
			//получаем информацию об id клиента, который отключился, и количество соединений
			if(msg_cnt == 0)
			{
				client_id = *(buf+i);
				msg_cnt++;
			}
			else if(msg_cnt == 1)
			{
				app_conn_cnt = *(buf+i);
				::SendMessage(mainWnd, WM_ONREMOTECLIENT,(WPARAM)(app_conn_cnt), NULL);
				msg_cnt++;
			}
			break;
		case OPEN_CLIENT:		//2 параметра
			//получаем информацию об id клиента, который подключился, и количество соединений
			if(msg_cnt == 0)
			{
				client_id = *(buf+i);
				msg_cnt++;
			}
			else if(msg_cnt == 1)
			{
				app_conn_cnt = *(buf+i);
				::SendMessage(mainWnd, WM_ONREMOTECLIENT,(WPARAM)(app_conn_cnt), NULL);
				msg_cnt++;
			}	
			break;
		case SET_BOARD_MODE_EXT: //64 параметра (128 байт параметров)
			//нужно установить полученный расширенный режим работы
			break;
		case ATS_Set_IP_mode:	
			switch(msg_cnt)
			{
			case 0: msg_cnt++; break;							//client id
			case 1: length_data = *(buf+i); msg_cnt++; break;	//data len	
			case 2: msg_cnt++; break;							//request id
			case 3: temp = *(buf+i); msg_cnt++; break;			//ip addr, 1st and 2nd bytes
			case 4:												//ip addr, 3rd and 4th bytes
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ips.ip_addr = temp;
				msg_cnt++; 
				break;									
			case 5: temp = *(buf+i); msg_cnt++; break;			//mask, 1st and 2nd bytes
			case 6: 											//mask, 3rd and 4th bytes
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ips.subnet_mask = temp;
				msg_cnt++; 
				break;										
			case 7: temp = *(buf+i); msg_cnt++; break;			//gateway, 1st and 2nd bytes
			case 8: 											//gateway, 3rd and 4th bytes
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ips.gateway = temp;
				msg_cnt++; 
				break;										
			case 9: temp = *(buf+i); msg_cnt++; break;			//multicast, 1st and 2nd bytes
			case 10:											//multicast, 3rd and 4th bytes
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ips.multicast_addr = temp;
				msg_cnt++; 
				break;											
			case 11:											//udp port
				ips.udp_port = *(buf+i);
				msg_cnt++; 
				break;							
			case 12:											//local port
				ips.local_port = 3399;//*(buf+i);
				msg_cnt++; 
				break;							
			case 13: msg_cnt++; break;							//flags
			case 14:											//reserved
					::SendMessage(mainWnd, WM_ONRTPSET, (WPARAM)SET_CHANGE_REMOTE, (LPARAM)&ips);
					msg_cnt++; 
					break;	
			};
			break;
		case ATS_Set_Dec_Video:	//deprecated
			break;
		case ATS_Set_Dec_Audio:	//deprecated
			break;
		case ATS_Set_Analis_Video:
			switch(msg_cnt)
			{
			case 0:	 msg_cnt++; break;							//client id
			case 1:	 length_data = *(buf+i); msg_cnt++; break;	//data len
			case 2:	 msg_cnt++; break;							//request id
			case 3:	 temp = *(buf+i); msg_cnt++; break;			//black level 1
			case 4:												//black level 2	 
				CAnalysisSetDlg::ReadRegistry(&ap);				//считываем значения из реестра для заполнения тех параметров, которые не будут приняты
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.black_frame.norm = *((float*)(&temp));
				msg_cnt++; 
				break;		
			case 5:	 temp = *(buf+i); msg_cnt++; break;			//black level warn 1
			case 6:												//black level warn 2
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.black_frame.fail = *((float*)(&temp));
				msg_cnt++; 
				break;		
			case 7:	 temp = *(buf+i); msg_cnt++; break;			//ident level 1
			case 8:												//ident level 2
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.freeze_frame.norm = *((float*)(&temp));
				msg_cnt++; 
				break;		
			case 9:	 temp = *(buf+i); msg_cnt++; break;			//ident level warn 1
			case 10:											//ident level warn 2
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.freeze_frame.fail = *((float*)(&temp));
				msg_cnt++; 
				break;		
			case 11: temp = *(buf+i); msg_cnt++; break;			//mot_level 1
			case 12:											//mot_level 2
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.motion_level.norm = *((float*)(&temp));
				msg_cnt++; 
				break;		
			case 13:											//time to video loss / average luma level	
				ap.time_to_video_loss = *((UCHAR*)(buf+i))*1000;	
				ap.luma_warn_level = *(((UCHAR*)(buf+i))+1);
				msg_cnt++; 
				break;							
			case 14:											//frames to black / black pixel level
				ap.frames_to_black = *((UCHAR*)(buf+i));	
				ap.black_pixel_val = *(((UCHAR*)(buf+i))+1);
				msg_cnt++; 
				break;							
			case 15:											//max pixel difference	/ frames to freeze
				ap.pixel_difference = *((UCHAR*)(buf+i));	
				ap.frames_to_freeze = *(((UCHAR*)(buf+i))+1);
				msg_cnt++; 
				break;							
			case 16:											//reserved
				msg_cnt++; 
				break;						
			case 17:											//reserved
				msg_cnt++; 
				break;		
			case 18:											//reserved
				::SendMessage(mainWnd, WM_ONANALYSISSET, (WPARAM)SET_CHANGE_REMOTE, (LPARAM)&ap);
				msg_cnt++; 
				break;		
			};
			break;
		case ATS_Set_Analis_Audio:
			switch(msg_cnt)
			{
			case 0:	msg_cnt++; break;							//client id
			case 1:	length_data = *(buf+i); msg_cnt++; break;	//data len
			case 2:	msg_cnt++; break;							//request id
			case 3:	temp = *(buf+i); msg_cnt++; break;			//audio loudness 1
			case 4:												//audio loudness 2
				CAnalysisSetDlg::ReadRegistry(&ap);				//считываем значения из реестра для заполнения тех параметров, которые не будут приняты
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.audio_loudness.norm = *((float*)(&temp));
				msg_cnt++; 
				break;	
			case 5:	temp = *(buf+i); msg_cnt++; break;			//audio loudness warn 1
			case 6:												//audio loudness warn 2
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.audio_loudness.fail = *((float*)(&temp));
				msg_cnt++; 
				break;								
			case 7:	temp = *(buf+i); msg_cnt++; break;			//audio silence 1
			case 8:												//audio silence 2
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.audio_silence.norm = *((float*)(&temp));
				msg_cnt++; 
				break;											
			case 9:	temp = *(buf+i); msg_cnt++; break;			//audio silence warn 1
			case 10:											//audio silence warn 2
				temp |= ((DWORD)(*(buf+i)))<<16; 
				ap.audio_silence.fail = *((float*)(&temp));
				msg_cnt++; 
				break;								
			case 11:
				ap.time_to_audio_loss = *((UCHAR*)(buf+i))*1000;
				msg_cnt++; 
				break;							//res 1.1
			case 12:msg_cnt++; break;							//res 1.2
			case 13:msg_cnt++; break;							//res 2.1
			case 14:											//res 2.2
				::SendMessage(mainWnd, WM_ONANALYSISSET, (WPARAM)SET_CHANGE_REMOTE, (LPARAM)&ap);
				msg_cnt++; 
				break;									
			};
			break;
		case ATS_Set_Prg_List:			// Added by TeddyJack
			switch(msg_cnt)
			{
			//message header
			case 0:		msg_cnt++;	break;				//client_id
			case 1:		length_data = *(buf + i);		//data len
						msg_cnt++;	break;
			case 2:		msg_cnt++;	break;				//request id
			//data - user stream info
			case 3:		if(first_msg_flag)										//first word of wParam (dword)
							*(prg_list_buffer + msg_cnt - 3 + offset) = *(buf + i);
						msg_cnt++;	break;
			case 4:		if(first_msg_flag)										//second word of wParam (dword)
							*(prg_list_buffer + msg_cnt - 3 + offset) = *(buf + i);
						msg_cnt++;	break;
			case 5:		if (first_msg_flag)										//streamsNum (word)
							*(prg_list_buffer + msg_cnt - 3 + offset) = *(buf + i);
						msg_cnt++; break;
			case 6:		if(first_msg_flag)										//progNum (word)
							*(prg_list_buffer + msg_cnt - 3 + offset) = *(buf + i);
						num_of_progs = *(buf + i);								
						msg_cnt++;	break;
			//data - first word of user prog info
			case 7:		curr_prog_index = *(buf+i);								//first word of wParam (interpreted as prog index)
						//формируем флаги "первая/последняя программа"
						if(curr_prog_index == 0)	first_msg_flag = TRUE;
						else						first_msg_flag = FALSE;
						if(curr_prog_index == num_of_progs-1)	last_msg_flag = TRUE;
						else									last_msg_flag = FALSE;

						*(prg_list_buffer + msg_cnt - (curr_prog_index + 1)*3 + offset) = *(buf + i);
						msg_cnt++;	break;
			//data - последовательно пишем байты в буффер prg_list_buffer
			default:	*(prg_list_buffer + msg_cnt - (curr_prog_index + 1)*3 + offset) = *(buf + i);
						msg_cnt++;	break;
			}

			if(msg_cnt == length_data + 1)		//+1 - для учета client id
			{
				offset += length_data - 2;	//смещение для записи следующей части структуры программ (-1 для учета того, что не записывали length data, еще -1 так как считаем от 0) 

				if(last_msg_flag == TRUE)
				{
					USER_STREAM_INFO* pSI = new USER_STREAM_INFO;
					MessageDisassembler(prg_list_buffer, pSI);
					offset = 0;
					::SendMessage(mainWnd, WM_ONUDPSET, SET_CHANGE_REMOTE, (LPARAM)pSI);
					last_msg_flag = FALSE;
				}
			}
			break;
		case ATS_Set_Control_DVBT2:
			switch(msg_cnt)
			{
			case 0:		msg_cnt++; break;														//client_id
			case 1:		length_data = *(buf+i); msg_cnt++; break;								//data len
			case 2:		msg_cnt++; break;														//request id
			case 3:		msg_cnt++; break;														//control + dataLength
			case 4:		msg_cnt++; break;														//reserve
			case 5:		msg_cnt++; break;														//reserve
			case 6:		msg_cnt++; break;														//reserve
			case 7:		msg_cnt++; break;														//reserve
			case 8:		ts.device = (BYTE)(*(buf+i) & 0x00ff); msg_cnt++; break;				//device (мл) + reserve (ст)
			case 9:		msg_cnt++; break;														//reserve
			case 10:	ts.c_freq = ((DWORD)*(buf+i)); msg_cnt++; break;						//dvbc freq
			case 11:	ts.c_freq |= (DWORD)(*(buf+i))<<16; msg_cnt++; break;					//dvbc freq
			case 12:	ts.t_freq = ((DWORD)*(buf+i)); msg_cnt++; break;						//dvbt freq
			case 13:	ts.t_freq |= (DWORD)(*(buf+i))<<16; msg_cnt++; break;					//dvbt freq
			case 14:	ts.t_band = *(buf+i); msg_cnt++; break;									//dvbt band
			case 15:	msg_cnt++; break;														//reserve
			case 16:	ts.t2_freq = ((DWORD)*(buf+i)); msg_cnt++; break;						//dvbt2 freq
			case 17:	ts.t2_freq |= (DWORD)(*(buf+i))<<16; msg_cnt++; break;					//dvbt2 freq
			case 18:	ts.t2_band = *(buf+i); msg_cnt++; break;								//dvbt2 band
			case 19:	ts.t2_plp_id = (BYTE)(*(buf+i) & 0x00ff); msg_cnt++; 					//dvbt2 plp (мл) + reserve (ст)
						::SendMessage(mainWnd, WM_ONTUNERSET, (WPARAM)SET_CHANGE_REMOTE, (LPARAM)&ts);
						break;
			default:	msg_cnt++; break;
			};
			break;
		case ATS_Get_IP_mode:
			switch(msg_cnt)
			{
			case 0:	ip_mode_clnt_id = *(buf+i); msg_cnt++; break;
			case 1:	length_data = *(buf+i); msg_cnt++; break;	
			case 2:	ip_mode_req_id = *(buf+i); SendIPAnswer(); msg_cnt++; break;							//request id
			};
			break;
		case ATS_Get_Dec_Video:	//deprecated
			break;
		case ATS_Get_Dec_Audio:	//deprecated
			break;
		case ATS_Get_Analis_Video:
			switch(msg_cnt)
			{
			case 0: analis_vid_clnt_id = *(buf+i); msg_cnt++; break;
			case 1: length_data = *(buf+i); msg_cnt++; break;
			case 2: analis_vid_req_id = *(buf+i); SendAnalisVideoAnswer(); msg_cnt++; break;
			};
			break;
		case ATS_Get_Analis_Audio:
			switch(msg_cnt)
			{
			case 0: analis_aud_clnt_id = *(buf+i); msg_cnt++; break;
			case 1: length_data = *(buf+i); msg_cnt++; break;
			case 2: analis_aud_req_id = *(buf+i); SendAnalisAudioAnswer(); msg_cnt++; break;
			};
			break;
		case ATS_Get_Prg_List:													// Added by TeddyJack
			switch(msg_cnt)														//
			{
			case 0: prg_list_clnt_id = *(buf+i); msg_cnt++; break;				//
			case 1: length_data = *(buf+i); msg_cnt++; break;					//
			case 2: prg_list_req_id = *(buf+i); SendPrgList(); msg_cnt++; break;//
			}
			break;																// Added later by TeddyJack
		case Reset_board:
			switch(msg_cnt)
			{
			case 0:
				::SendMessage(mainWnd, WM_ONRESETANALYSIS, (WPARAM)SET_CHANGE_REMOTE, NULL);
				msg_cnt++; 
				break;
			};
			break;
		case ATS_Get_Control_DVBT2:
			switch(msg_cnt)
			{
			case 0: dvb_set_clnt_id = *(buf+i); msg_cnt++; break;
			case 1: length_data = *(buf+i); msg_cnt++; break;
			case 2: dvb_set_req_id = *(buf+i); SendDVBSettingsAnswer(); msg_cnt++; break;
			};
			break;
		case ATS_Get_Status_DVBT2:
			switch(msg_cnt)
			{
			case 0: dvb_status_clnt_id = *(buf+i); msg_cnt++; break;
			case 1: length_data = *(buf+i); msg_cnt++; break;
			case 2: dvb_status_req_id = *(buf+i); SendDVBStatusAnswer(); msg_cnt++; break;
			};
			break;
		case ATS_Power_Off:
			switch(msg_cnt)
			{
			case 0: msg_cnt++; break;
			};
			break;
		default:
			//получено неизвестное сообщение
			break;
		}
	}
	return TRUE;
}

BOOL CExchange::ProceedDtmData(WORD* buf, long len, IP_SETTINGS_ONLY* pIS)
{
	if ( ((IP_SETTINGS_ONLY*)(buf + 3)) )
		*pIS = *((IP_SETTINGS_ONLY*)(buf + 3));
	else return FALSE;
	return TRUE;
}

// сообщение-запрос о типе и версии программы
//перенести в другую функцию
//

BOOL CExchange::SendTypeVersion()
{
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];
	BOOL succeeded = FALSE;
	MSG_1_2	type_ver_msg;

	type_ver_msg.header.prefix		= PREFIX;
	type_ver_msg.header.cod_comand	= VERSION | START_MSG | STOP_MSG | EXIT_RECEIVE;
	type_ver_msg.version			= CUR_VERSION;
	type_ver_msg.type				= CUR_TYPE;
	type_ver_msg.reserved			= 0;
	
	*((MSG_1_2*)message)			= type_ver_msg;
	
	succeeded = SendData(message, MSG_1_2_LEN);

	delete [] message;

	return succeeded;
}

BOOL CExchange::SendExitReceive()
{
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];
	BOOL succeeded = FALSE;
	MSG_1_1	exit_msg;

	exit_msg.header.prefix = PREFIX;
	exit_msg.header.cod_comand = EXIT_MSG;

	*((MSG_1_1*)message) = exit_msg;

	succeeded = SendData(message, MSG_1_1_LEN);

	delete[] message;

	return succeeded;
}

void CExchange::FillErrorBufs(ERR_INFO* err_inf_buf, int i, int chainNum)
{
	if (chainNum < MAX_ANALYZED_PROG_NUM)
	{
		switch (i)
		{
		case video_loss_err:
			err_inf_buf[i].count = err_cnt[chainNum][video_loss_err];
			err_inf_buf[i].err_code = 0x11;
			err_inf_buf[i].index = 0;
			break;
		case video_freeze_err:
			err_inf_buf[i].count = err_cnt[chainNum][video_freeze_err];
			err_inf_buf[i].err_code = 0x12;
			err_inf_buf[i].index = 1;
			break;
		case video_low_motion_err:
			err_inf_buf[i].count = err_cnt[chainNum][video_low_motion_err];
			err_inf_buf[i].err_code = 0x13;
			err_inf_buf[i].index = 2;
			break;
		case video_black_err:
			err_inf_buf[i].count = err_cnt[chainNum][video_black_err];
			err_inf_buf[i].err_code = 0x14;
			err_inf_buf[i].index = 3;
			break;
		case video_almost_black_err:
			err_inf_buf[i].count = err_cnt[chainNum][video_almost_black_err];
			err_inf_buf[i].err_code = 0x15;
			err_inf_buf[i].index = 4;
			break;
		case video_blockiness_err:
			err_inf_buf[i].count = err_cnt[chainNum][video_blockiness_err];
			err_inf_buf[i].err_code = 0x16;
			err_inf_buf[i].index = 5;
			break;
		case video_blockiness_warn_err:
			err_inf_buf[i].count = err_cnt[chainNum][video_blockiness_warn_err];
			err_inf_buf[i].err_code = 0x17;
			err_inf_buf[i].index = 6;
			break;
		case audio_loss_err:
			err_inf_buf[i].count = err_cnt[chainNum][audio_loss_err];
			err_inf_buf[i].err_code = 0x21;
			err_inf_buf[i].index = 7;
			break;
		case audio_high_err:
			err_inf_buf[i].count = err_cnt[chainNum][audio_high_err];
			err_inf_buf[i].err_code = 0x22;
			err_inf_buf[i].index = 8;
			break;
		case audio_almost_high_err:
			err_inf_buf[i].count = err_cnt[chainNum][audio_almost_high_err];
			err_inf_buf[i].err_code = 0x23;
			err_inf_buf[i].index = 9;
			break;
		case audio_low_err:
			err_inf_buf[i].count = err_cnt[chainNum][audio_low_err];
			err_inf_buf[i].err_code = 0x24;
			err_inf_buf[i].index = 10;
			break;
		case audio_almost_low_err:
			err_inf_buf[i].count = err_cnt[chainNum][audio_almost_low_err];
			err_inf_buf[i].err_code = 0x25;
			err_inf_buf[i].index = 11;
			break;
		default: break;
		};

		err_inf_buf[i].err_ext = 0x0000;			//разновидность ошибки
		if (i < audio_loss_err)
		{
			for (int k = 0; k < 2; k++)
			{
				if ((userSI.prog_info[chainNum].prog_pids[k].type == typeVideo))
				{
					err_inf_buf[i].PID = (userSI.prog_info[chainNum].prog_pids[k].PID) & 0x1fff;			//PID, в котором произошла ошибка (на самом деле тут еще есть multy pid и reserved)
				}
			}
		}
		else
		{
			for (int k = 0; k < 2; k++)
			{
				if ((userSI.prog_info[chainNum].prog_pids[k].type == typeAudio))
				{
					err_inf_buf[i].PID = (userSI.prog_info[chainNum].prog_pids[k].PID) & 0x1fff;			//PID, в котором произошла ошибка (на самом деле тут еще есть multy pid и reserved)
				}
			}
		}

		err_inf_buf[i].packet = 0x00000000;		//номер пакета с ошибкой (по факту не используется)
		err_inf_buf[i].param1 = 0x00000000;		//доп. инфо об ошибке
		err_inf_buf[i].param2 = 0x00000000;		//доп. инфо об ошибке
	}
}

BOOL CExchange::SendError()
{
	if (!got_errors) return FALSE;
	int index = 0;
	ERR_INFO err_inf[ERR_NUM];		//массив описаний произошедших ошибок
	DWORD to_transmit[ERR_NUM];		//массив, где хранятся номера тех ошибок, которые надо передать
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];
	long length = 0;
	BOOL succeeded = FALSE;
	ERR_MSG	err_msg;				//сообщение об ошибке

	err_msg.header.prefix = PREFIX;
	err_msg.header.cod_comand = TRA_MPEG | /*START_MSG |*/ STOP_MSG;
#ifdef PROTOCOL_VER3
	err_msg.TS_index = 0;
	err_msg.TS_Num = 0;
	err_msg.reserve = 0;
#endif
	for(int i = 0; i < MAX_ANALYZED_PROG_NUM; i++)
	{
		for(int j = 0; j < ERR_NUM; j++)										//заполняем массив ошибок теми ошибками, которые произошли за измерительный интервал
		{
			if(err_cnt[i][j] > 0)
			{
				//			  buf       buf index   chain_num
				FillErrorBufs(err_inf,	j,			i);
				to_transmit[index] = j;
				index++;
			}
		}																		//теперь у нас есть массив, в котором содержаться те номера ошибок, количество которых не равно 0
		
		err_msg.length				= (sizeof(ERR_MSG) - sizeof(HEAD) - sizeof(WORD))/2 + 10*index;
		err_msg.err_num				= index;
		*((ERR_MSG*)(message))		= err_msg;									//записываем шапку в пересылаемое сообщение

		for(int k = 0; k < index; k++)											//записываем информацию об ошибках в пересылаемое сообщение
			*( (ERR_INFO*)(message + sizeof(ERR_MSG) + 10*sizeof(WORD)*k) ) = err_inf[to_transmit[k]];

		length = sizeof(ERR_MSG) + 10*sizeof(WORD)*index;										//сколько байт нужно переслать по USB (длина одного сообщения - не более 256 слов)
																				//256 слов - это:
																				//заголовок: 4 слова; 1 ошибка: 10 слов; 256 - 4 = 252 (осталось на ошибки); 252/10 = 25 ошибок за сообщени
		WORD mes[MAX_MSG_SIZE];
		memcpy(mes, message, MAX_MSG_SIZE);
		succeeded = SendData(message, length);
		index = 0;

		if(!succeeded)
		{
			delete [] message;
			return false;
		}
	
	}
	//if(succeeded)
	{
		for(int i = 0; i < MAX_ANALYZED_PROG_NUM; i++)
			for(int j = 0; j < ERR_NUM; j++)	//почему то zeromemory не сработало(
				err_cnt[i][j] = 0;
	}

	delete [] message;

	return succeeded;
}

void CExchange::IncrementDVBStatus()
{
	dvb_status_version++;
}
void CExchange::IncrementDVBSettings()
{
	dvb_settings_version++;
}
void CExchange::IncrementSettings()
{
	settings_version++;
}

BOOL CExchange::SendStatus()			
{
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MSG_STAT_LEN];
	stat_msg.header.prefix				= PREFIX;
	stat_msg.header.cod_comand			= STATUS | START_MSG | STOP_MSG | EXIT_RECEIVE;
	stat_msg.reserved					= 0;
#ifdef PROTOCOL_VER3
	stat_msg.TS_Count					= 1;
	stat_msg.reserved_2					= 0;
#endif
	stat_msg.stat_version				= status_version;
	stat_msg.set_version				= settings_version;
	stat_msg.video_load					= 0;
	stat_msg.audio_load					= 0;
	stat_msg.dvbt2_opt_status_version	= dvb_status_version;
	stat_msg.dvbt2_opt_control_version	= dvb_settings_version;

	*((STATUS_MSG*)(message)) = stat_msg;

	succeeded = SendData(message, MSG_STAT_LEN);
	if(succeeded) status_version++;
	
	delete [] message;
	return succeeded;
}

BOOL CExchange::SendIPAnswer()
{
	BOOL succeeded = FALSE;
	GET_IP_MODE_ANSWER		ip_mode_answer;			//сообщение ответа на запрос настроек ip
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];

	IP_SETTINGS is; CRtp_settingsDlg::ReadRegistry(&is);

	ip_mode_answer.header.prefix		= PREFIX;
	ip_mode_answer.header.cod_comand	= SEND_PERS_BUF | EXIT_RECEIVE;
	ip_mode_answer.length_data			= IP_ANSWER_LEN_DATA/2;
	ip_mode_answer.client_id			= ip_mode_clnt_id;
	ip_mode_answer.mess_cod				= IP_ANSWER_CODE;
	ip_mode_answer.request_id			= ip_mode_req_id;
	ip_mode_answer.length_mess			= IP_ANSWER_LEN_MSG/2;
	ip_mode_answer.IP_addr				= is.ip_addr;
	ip_mode_answer.IP_mask				= is.subnet_mask;
	ip_mode_answer.IP_gateway			= is.gateway;
	ip_mode_answer.IP_group				= is.multicast_addr;
	ip_mode_answer.UDP_port				= is.udp_port;
	ip_mode_answer.Internal_port		= is.local_port;
	ip_mode_answer.flags				= is.flags;
	ip_mode_answer.reserve				= 0;

	*((GET_IP_MODE_ANSWER*)(message)) = ip_mode_answer;

	succeeded = SendData(message, sizeof(GET_IP_MODE_ANSWER));

	delete [] message;
	return succeeded;
}

BOOL CExchange::SendAnalisVideoAnswer()
{
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];
	GET_ANALIS_VIDEO_ANSWER	analis_video_answer;	//сообщение ответа на запрос настроек анализа видео
	
	ANALYSIS_PARAMS ap;	CAnalysisSetDlg::ReadRegistry(&ap);

	//заполнить структуру
	analis_video_answer.header.prefix		= PREFIX;
	analis_video_answer.header.cod_comand	= SEND_PERS_BUF | EXIT_RECEIVE;
	analis_video_answer.length_data			= ANALIS_VID_ANSWER_LEN_DATA/2;
	analis_video_answer.client_id			= analis_vid_clnt_id;
	analis_video_answer.mess_cod			= ANALIS_VID_ANSWER_CODE;
	analis_video_answer.request_id			= analis_vid_req_id;
	analis_video_answer.length_mess			= ANALIS_VID_ANSWER_LEN_MSG/2;
	analis_video_answer.level_black			= (float)ap.black_frame.fail;
	analis_video_answer.level_black_user	= (float)ap.black_frame.norm;
	analis_video_answer.level_ident			= (float)ap.freeze_frame.fail;
	analis_video_answer.level_ident_user	= (float)ap.freeze_frame.norm;
	analis_video_answer.level_run			= (float)ap.motion_level.norm;
	analis_video_answer.time_stop_video		= (BYTE)(ap.time_to_video_loss * 0.001);
	analis_video_answer.mean_y_level		= (BYTE)ap.luma_warn_level;
	analis_video_answer.num_black_frames	= (BYTE)ap.frames_to_black;
	analis_video_answer.black_level			= (BYTE)ap.black_pixel_val;
	analis_video_answer.difference_pixel	= (BYTE)ap.pixel_difference;
	analis_video_answer.num_freeze_frames	= (BYTE)ap.frames_to_freeze;
	analis_video_answer.reserve1			= 0;
	analis_video_answer.reserve2			= 0;

	*((GET_ANALIS_VIDEO_ANSWER*)(message)) = analis_video_answer;
	succeeded = SendData(message, sizeof(GET_ANALIS_VIDEO_ANSWER));
	delete [] message;
	return succeeded;
}

BOOL CExchange::SendAnalisAudioAnswer()
{
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];
	GET_ANALIS_AUDIO_ANSWER	analis_audio_answer;	//сообщение ответа на запрос настроек анализа аудио

	ANALYSIS_PARAMS ap;	CAnalysisSetDlg::ReadRegistry(&ap);

	//заполнить структуру
	analis_audio_answer.header.prefix		= PREFIX;
	analis_audio_answer.header.cod_comand	= SEND_PERS_BUF | EXIT_RECEIVE;
	analis_audio_answer.length_data			= ANALIS_AUD_ANSWER_LEN_DATA/2;
	analis_audio_answer.client_id			= analis_aud_clnt_id;
	analis_audio_answer.mess_cod			= ANALIS_AUD_ANSWER_CODE;
	analis_audio_answer.request_id			= analis_aud_req_id;
	analis_audio_answer.length_mess			= ANALIS_AUD_ANSWER_LEN_MSG/2;
	analis_audio_answer.level_over			= (float)ap.audio_loudness.fail;
	analis_audio_answer.level_over_user		= (float)ap.audio_loudness.norm;
	analis_audio_answer.level_under			= (float)ap.audio_silence.fail;
	analis_audio_answer.level_under_user	= (float)ap.audio_silence.norm;
	analis_audio_answer.time_stop_audio		= (BYTE)(ap.time_to_audio_loss * 0.001);
	analis_audio_answer.reserved1			= 0;
	analis_audio_answer.reserved2			= 0;
	analis_audio_answer.reserved3			= 0;
	*((GET_ANALIS_AUDIO_ANSWER*)(message)) = analis_audio_answer;
	succeeded = SendData(message, sizeof(GET_ANALIS_AUDIO_ANSWER));
	delete [] message;
	return succeeded;
}

void CExchange::SetFullStreamInfo(USER_STREAM_INFO* pSI)
{
	fullSI = *pSI;
}
void CExchange::SetUserStreamInfo(USER_STREAM_INFO* pSI)
{
	userSI = *pSI;
}

void CExchange::SetDVBStatus(SAT_STATUS* pStatus)
{
	dvbStatus = *pStatus;
}

void CExchange::SetDVBInfo(UAT_INFO* pInfo)
{
	dvbInfo = *pInfo;
}

// TEDDYJACK's code starts
BOOL CExchange::SendPrgList()
{
	GET_PROGRAMS_LIST prg_list;				//сообщение, содержащее список программ в потоке
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];

	USER_STREAM_INFO* p_prog_info = new USER_STREAM_INFO[2];

	p_prog_info[0] = fullSI;
	p_prog_info[1] = userSI;

	prg_list.header.prefix		= PREFIX;
	prg_list.header.cod_comand	= SEND_PERS_BUF;
	prg_list.client_id			= prg_list_clnt_id;
	prg_list.request_id			= prg_list_req_id;

	WORD b = sizeof(USER_PIDS_INFO)/2;										// кол-во слов, которое занимает структура user_pids_info
	WORD a = sizeof(USER_PROG_INFO)/2-STREAM_INFO_PIDS_PER_PROGRAM*b;		// кол-во слов, которое занимает структура user_prog_info без части user_pids_info
	
	WORD length_mess = 0;
	for(int d = 0; d < 2; d++)
	{
		length_mess += (sizeof(USER_STREAM_INFO) - sizeof(USER_PROG_INFO)*MAX_PROG_NUM)/2;													// "3" - кол-во вступительных слов в структуре user_stream_info
		for(int i = 0; i < p_prog_info[d].prog_num; i++)						// вычисление кол-ва слов в передаваемом Лёне сообщении
			length_mess += (a+b*p_prog_info[d].prog_info[i].pids_num);
	}
	WORD* lp_word = new WORD[length_mess];					// lp_word - указатель на нулевую ячейку размера word
	
	WORD n = 0;											// n - это смещение относительно нулевой ячейки
	for(int d = 0; d < 2; d++)
	{
#ifdef PROTOCOL_VER3
		*((DWORD*)(lp_word + n)) = p_prog_info[d].wParam;		// перейти к ячейке 0 и записать туда dword
		n += 2;													// перейти на 2 ячейки дальше, так как записали dword
#else
		*((WORD*)(lp_word + n)) = p_prog_info[d].wParam;		// перейти к ячейке 0 и записать туда word
		n++;													// перейти на 1 ячейку дальше, так как записали word
#endif
		*((WORD*)(lp_word + n)) = p_prog_info[d].streamsNum;	// перейти к ячейке 2, записать туда word
		n++;													// перейти на 1 ячейку дальше, так как записали word
		lp_word[n] = p_prog_info[d].prog_num;			n++;

		for(int i=0; i<p_prog_info[d].prog_num; i++)
		{
#ifdef PROTOCOL_VER3
			*((DWORD*)(lp_word + n)) = p_prog_info[d].prog_info[i].wParam;	// перейти к ячейке lp_word+n
			n += 2;
#else
			*((WORD*)(lp_word + n)) = p_prog_info[d].prog_info[i].wParam;	// перейти к ячейке lp_word+n
			n++;
#endif
			*((WORD*)(lp_word + n)) = p_prog_info[d].prog_info[i].streamID;	// перейти к ячейке lp_word+n
			n++;
			memcpy_s((BYTE*)(lp_word+n),MAX_PROG_NAME,p_prog_info[d].prog_info[i].prog_name,MAX_PROG_NAME);		// Саня, проверь
			n += MAX_PROG_NAME/2;
			memcpy_s((BYTE*)(lp_word+n),MAX_PROG_NAME,p_prog_info[d].prog_info[i].prov_name,MAX_PROG_NAME);
			n += MAX_PROG_NAME/2;


			lp_word[n] = p_prog_info[d].prog_info[i].prog_type;				n++;
			lp_word[n] = p_prog_info[d].prog_info[i].pids_num;				n++;
			for(int j = 0; j < p_prog_info[d].prog_info[i].pids_num; j++)
			{
				*((USER_PIDS_INFO*)(lp_word+n)) = p_prog_info[d].prog_info[i].prog_pids[j];
				n += sizeof(USER_PIDS_INFO)/2;
			}

		}
	}

	int n_of_msgs = (int)(length_mess/MAX_DATA_SIZE)+1;

	for(int i=1; i<=n_of_msgs; i++)
	{
		if(i==1)
		{
			prg_list.mess_cod		= PRG_LIST_ANSWER_CODE;
			prg_list.length_mess	= length_mess;
		}
		else
		{
			prg_list.mess_cod		= PRG_LIST_ANSWER_CODE & 0x7FFF;
			prg_list.length_mess	= (i-1)*MAX_DATA_SIZE;
		}
		if(i==n_of_msgs)
		{
			prg_list.length_data	= length_mess-(n_of_msgs-1)*MAX_DATA_SIZE+4;
			prg_list.header.cod_comand = SEND_PERS_BUF | EXIT_RECEIVE;
		}
		else
		{
			prg_list.length_data	= MAX_DATA_SIZE+4;
			prg_list.header.cod_comand = SEND_PERS_BUF;
		}

		BOOL flag = FALSE;
		if(p_prog_info->prog_num == 0)
			flag = TRUE;
		
		memcpy_s(prg_list.data, sizeof(prg_list.data), lp_word+(i-1)*MAX_DATA_SIZE, 2*(prg_list.length_data-4));
		*((GET_PROGRAMS_LIST*)(message)) = prg_list;
		succeeded = SendData(message, 2*(prg_list.length_data+3));
	}
	
		//CFile file;	// для отладки, посмотреть что пишется в память
		//file.Open(_T("memory.hex"), CFile::modeCreate | CFile::modeReadWrite);
		//file.Write(lp_word, length_mess*2);
		//file.Flush();
		//file.Close();

	delete [] message;
	delete [] p_prog_info;
	delete [] lp_word;
	return succeeded;
}
// end of TEDDYJACK's code

BOOL CExchange::SendDVBStatusAnswer()
{
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];

	GET_STATUS_DVBT2 dvbt2_status_answer;	//сообщение ответа на запрос статуса ТВ тюнера
	GET_STATUS_DVBT dvbt_status_answer;		//сообщение ответа на запрос статуса ТВ тюнера
	GET_STATUS_DVBC	dvbc_status_answer;		//сообщение ответа на запрос статуса ТВ тюнера

	switch(dvbStatus.device)
	{
	case DVBT2:	
		dvbt2_status_answer.header.prefix		= PREFIX;
		dvbt2_status_answer.header.cod_comand	= SEND_PERS_BUF | EXIT_RECEIVE;
		dvbt2_status_answer.length_data			= DVBT2_STATUS_ANSWER_LEN_DATA/2;
		dvbt2_status_answer.client_id			= dvb_status_clnt_id;
		dvbt2_status_answer.mess_cod			= DVB_STAT_ANSWER_CODE;
		dvbt2_status_answer.request_id			= dvb_status_req_id;
		dvbt2_status_answer.length_mess			= DVBT2_STATUS_ANSWER_LEN_MSG/2;
		dvbt2_status_answer.status_ver			= dvb_status_version;
		dvbt2_status_answer.dataLength			= sizeof(GET_STATUS_DVBT2_DATA);
		dvbt2_status_answer.opt_type			= 9;
		dvbt2_status_answer.opt_ver				= 2;
		dvbt2_status_answer.reserve1			= 0;
		dvbt2_status_answer.reserve2			= 0;
		dvbt2_status_answer.data.device			= dvbStatus.device;
		dvbt2_status_answer.data.reserve3		= 0;
		dvbt2_status_answer.data.status			= dvbStatus.status;
		dvbt2_status_answer.data.frequency		= dvbStatus.frequency;
		dvbt2_status_answer.data.snr			= dvbStatus.snr;
		dvbt2_status_answer.data.ber			= dvbStatus.ber;
		dvbt2_status_answer.data.reserve4		= 0;
		dvbt2_status_answer.data.T2_stream_type	= dvbInfo.t2.main.T2_stream_type;
		dvbt2_status_answer.data.pilot_pattern	= dvbInfo.t2.main.pilot_pattern;
		dvbt2_status_answer.data.L1_FEC			= dvbInfo.t2.main.L1_FEC;
		dvbt2_status_answer.data.L1_code_rate	= dvbInfo.t2.main.L1_code_rate;
		dvbt2_status_answer.data.PAPR_indicator	= dvbInfo.t2.main.PAPR_indicator;
		dvbt2_status_answer.data.L1_modulation	= dvbInfo.t2.main.L1_modulation;
		dvbt2_status_answer.data.FFT_mode		= dvbInfo.t2.main.FFT_mode;
		dvbt2_status_answer.data.guard_interval	= dvbInfo.t2.main.guard_interval;
		dvbt2_status_answer.data.cellID			= dvbInfo.t2.main.cellId;
		dvbt2_status_answer.data.networkID		= dvbInfo.t2.main.networkId;
		dvbt2_status_answer.data.systemID		= dvbInfo.t2.main.systemId;
		dvbt2_status_answer.data.reserve5		= 0;
		dvbt2_status_answer.data.basic_type		= dvbInfo.t2.main.basic_type;
		dvbt2_status_answer.data.num_plp		= dvbInfo.t2.main.num_PLPs;
		for(int i = 0; i < 64; i++)
			dvbt2_status_answer.data.PLP_IDs[i]	= dvbInfo.t2.PLPs_ID[i];
		dvbt2_status_answer.data.PLP_ID			= dvbInfo.t2.PLP_info.ID;
		dvbt2_status_answer.data.PLP_type		= dvbInfo.t2.PLP_info.PLP_type;
		dvbt2_status_answer.data.PLP_payload	= dvbInfo.t2.PLP_info.PLP_payload;
		dvbt2_status_answer.data.PLP_constel	= dvbInfo.t2.PLP_info.PLP_constel;
		dvbt2_status_answer.data.PLP_code_rate	= dvbInfo.t2.PLP_info.PLP_code_rate;
		dvbt2_status_answer.data.PLP_FEC		= dvbInfo.t2.PLP_info.PLP_FEC;

		*((GET_STATUS_DVBT2*)(message)) = dvbt2_status_answer;
		succeeded = SendData(message, sizeof(GET_STATUS_DVBT2));
		break;
	case DVBT: 	
		dvbt_status_answer.header.prefix			= PREFIX;;
		dvbt_status_answer.header.cod_comand		= SEND_PERS_BUF | EXIT_RECEIVE;
		dvbt_status_answer.length_data				= DVBT_STATUS_ANSWER_LEN_DATA/2;
		dvbt_status_answer.client_id				= dvb_status_clnt_id;
		dvbt_status_answer.mess_cod					= DVB_STAT_ANSWER_CODE;
		dvbt_status_answer.request_id				= dvb_status_req_id;
		dvbt_status_answer.length_mess				= DVBT_STATUS_ANSWER_LEN_MSG/2;
		dvbt_status_answer.status_ver				= dvb_status_version;
		dvbt_status_answer.dataLength				= sizeof(GET_STATUS_DVBT_DATA);
		dvbt_status_answer.opt_type					= 9;
		dvbt_status_answer.opt_ver					= 2;
		dvbt_status_answer.reserve1					= 0;
		dvbt_status_answer.reserve2					= 0;
		dvbt_status_answer.data.device				= dvbStatus.device;
		dvbt_status_answer.data.reserve3			= 0;
		dvbt_status_answer.data.status				= dvbStatus.status;
		dvbt_status_answer.data.frequency			= dvbStatus.frequency;
		dvbt_status_answer.data.snr					= dvbStatus.snr;
		dvbt_status_answer.data.ber					= dvbStatus.ber;
		dvbt_status_answer.data.reserve4			= 0;
		dvbt_status_answer.data.modulation			= dvbInfo.t.modulation;
		dvbt_status_answer.data.transmit_mode		= dvbInfo.t.transmit_mode;
		dvbt_status_answer.data.guard_interval		= dvbInfo.t.guard_interval;
		dvbt_status_answer.data.hierarchy			= dvbInfo.t.hierarchy;
		dvbt_status_answer.data.HP_fec_code_rate	= dvbInfo.t.HP_fec_code_rate;
		dvbt_status_answer.data.LP_fec_code_rate	= dvbInfo.t.LP_fec_code_rate;

		*((GET_STATUS_DVBT*)(message))	= dvbt_status_answer;
		succeeded = SendData(message, sizeof(GET_STATUS_DVBT));
		break;
	case DVBC:	
		dvbc_status_answer.header.prefix			= PREFIX;;
		dvbc_status_answer.header.cod_comand		= SEND_PERS_BUF | EXIT_RECEIVE;
		dvbc_status_answer.length_data				= DVBC_STATUS_ANSWER_LEN_DATA/2;
		dvbc_status_answer.client_id				= dvb_status_clnt_id;
		dvbc_status_answer.mess_cod					= DVB_STAT_ANSWER_CODE;
		dvbc_status_answer.request_id				= dvb_status_req_id;
		dvbc_status_answer.length_mess				= DVBC_STATUS_ANSWER_LEN_MSG/2;
		dvbc_status_answer.status_ver				= dvb_status_version;
		dvbc_status_answer.dataLength				= sizeof(GET_STATUS_DVBC_DATA);
		dvbc_status_answer.opt_type					= 9;
		dvbc_status_answer.opt_ver					= 2;
		dvbc_status_answer.reserve1					= 0;
		dvbc_status_answer.reserve2					= 0;
		dvbc_status_answer.data.device				= dvbStatus.device;
		dvbc_status_answer.data.reserve3			= 0;
		dvbc_status_answer.data.status				= dvbStatus.status;
		dvbc_status_answer.data.frequency			= dvbStatus.frequency;
		dvbc_status_answer.data.snr					= dvbStatus.snr;
		dvbc_status_answer.data.ber					= dvbStatus.ber;
		dvbc_status_answer.data.reserve4			= 0;
		dvbc_status_answer.data.modulation			= dvbInfo.c.modulation;
		dvbc_status_answer.data.sym_rate			= dvbInfo.c.sym_rate;

		*((GET_STATUS_DVBC*)(message)) = dvbc_status_answer;
		succeeded = SendData(message, sizeof(GET_STATUS_DVBC));
		break;
	};
	
	delete [] message;

	return succeeded;
}
BOOL CExchange::SendDVBSettingsAnswer()
{
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];
	GET_CONTROL_DVBT2		dvb_settings_answer;	//сообщение ответа на запрос настроек ТВ тюнера

	ALL_DVB_TUNER_SETTINGS ts; CTunerSetDlg::ReadRegistry(&ts);

	//заполнить структуру
	dvb_settings_answer.header.prefix		= PREFIX;
	dvb_settings_answer.header.cod_comand	= SEND_PERS_BUF | EXIT_RECEIVE;
	dvb_settings_answer.length_data			= DVB_SET_ANSWER_LEN_DATA/2;
	dvb_settings_answer.client_id			= dvb_set_clnt_id;
	dvb_settings_answer.mess_cod			= DVB_SET_ANSWER_CODE;
	dvb_settings_answer.request_id			= dvb_set_req_id;
	dvb_settings_answer.length_mess			= DVB_SET_ANSWER_LEN_MSG/2;
	dvb_settings_answer.control				= dvb_settings_version & 0xF;
	dvb_settings_answer.dataLength			= sizeof(dvb_settings_answer.data);
	dvb_settings_answer.reserve1			= 0;
	dvb_settings_answer.reserve2			= 0;
	dvb_settings_answer.data.device			= ts.device;
	dvb_settings_answer.data.reserve3		= 0;
	dvb_settings_answer.data.reserve4		= 0;
	dvb_settings_answer.data.dvbc_freq		= ts.c_freq;
	dvb_settings_answer.data.dvbt_freq		= ts.t_freq;
	dvb_settings_answer.data.dvbt_band		= ts.t_band;
	dvb_settings_answer.data.reserve5		= 0;
	dvb_settings_answer.data.dvbt2_freq		= ts.t2_freq;
	dvb_settings_answer.data.dvbt2_band		= ts.t2_band;
	dvb_settings_answer.data.dvbt2_plp		= ts.t2_plp_id;

	*((GET_CONTROL_DVBT2*)(message)) = dvb_settings_answer;
	succeeded = SendData(message, sizeof(GET_CONTROL_DVBT2));
	delete [] message;
	return succeeded;
}

BOOL CExchange::SendLEDs()
{
	SET_LEDS led_status;				//сообщение, содержащее значение светодиодов
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];

	BYTE leds = 0;
	if(stat_msg.flags & (TS_OK | IP_OK))
	{
		leds |= 0x01;
		if(stat_msg.flags & AUD_OK)
		{
			if((stat_msg.err_flags[0] & 0x38))
				leds |= 0x50;
			else
				leds |= 0x10;
		}
		else
			leds |= 0x40;
		if(stat_msg.flags & VID_OK)
		{
			if((stat_msg.err_flags[0] & 0x07))
				leds |= 0x0A;
			else
				leds |= 0x02;
		}
		else
			leds |= 0x08;
	}

	led_status.header.prefix = PREFIX;
	led_status.header.cod_comand = 0x0888;
	led_status.leds = leds;


	*((SET_LEDS*)(message)) = led_status;

	succeeded = SendData(message, sizeof(SET_LEDS), TRUE);
	
	delete [] message;
	return succeeded;
}

BOOL CExchange::SendIPSettings(IP_SETTINGS_ONLY* pIS)
{
	if (!pIS) return FALSE;

	IP_SETTINGS_ONLY is;
	SET_IP_SETTINGS isFull;

	is.volatile_mem = 0x00;				// all (> byte) values are inverted because of big-endian
	is.ip_addr = 0xe06fa8c0;				// 192.168.111.3
	is.subn_mask = 0x00ffffff;			// 255.255.255.0
	is.gateway = 0x016fa8c0;				// 192.168.111.1
	is.dhcp_en = 0x00;
	is.ip_transmit_en = 0x01;
	is.fec_en = 0x00;
	is.fec_col = 0x00;
	is.fec_rows = 0x00;
	is.dest_ip = 0x020201e0;				// 224.1.2.2
	is.udp_port = 0xd204;					// 1234
	is.tp_per_ip = 7;
	is.protocol = 0x00;					// UDP
	is.ttl = 0xff;
	is.packet_size = 0x00;				// = 188 bytes
	is.input_select = 0x01;				// 1 = SPI input
	is.mode = 0x00;						// 0 = ASItoIP; 1 = IPtoASI
	is.application = 0x01;				// 0 = failsafe, 1 = normal

	if (memcmp(pIS, &is, sizeof(IP_SETTINGS_ONLY)) != 0)
	{
		BOOL succeeded = FALSE;
		PUCHAR message = new UCHAR[MAX_MSG_SIZE];

		isFull.length_data = sizeof(SET_IP_SETTINGS) - sizeof(HEAD) - sizeof(WORD);
		isFull.header.prefix = PREFIX;
		isFull.header.cod_comand = 0x0777;
		isFull.ip_settings = is;
		isFull.reboot = 0x01;				// reboot after sending all settings

		*((SET_IP_SETTINGS*)(message)) = isFull;

		succeeded = SendData(message, sizeof(SET_IP_SETTINGS), TRUE);

		delete[] message;
		return succeeded;
	}
	return TRUE;
}

BOOL CExchange::CheckDTM3200Settings()
{
	IP_SETTINGS_ONLY is;
	if (!SendIPSettRdReq()) return FALSE;
	if (!ChangeReceiveFIFO(1)) return FALSE;
	PUCHAR data = new UCHAR[64];
	ZeroMemory(data, 64);
	LONG length = 64;
	long totalLen = 0;
	WORD word = 0;

	time_t start_time = 0;
	time_t cur_time = 0;
	time(&start_time);
	time(&cur_time);

	while (difftime(cur_time, start_time) < DTM3200_READ_SETTINGS_TIMEOUT)
	{
		length = 64;
		ReceiveData(data, &length);
		totalLen += length;
		time(&cur_time);
	};

	if (totalLen != sizeof(SET_IP_SETTINGS)) return FALSE;

	if (!CExchange::ProceedDtmData(((WORD*)data), length / 2, &is)) return FALSE;
	
	if (!SendIPSettings(&is)) return FALSE;
	if (!ChangeReceiveFIFO(0)) return FALSE;

	delete[] data;
	return TRUE;
}

BOOL CExchange::SendIPSettRdReq()
{
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];
	IP_SET_REQ ipSetReq;

	ipSetReq.header.prefix = PREFIX;
	ipSetReq.header.cod_comand = 0x0778;

	*((IP_SET_REQ*)(message)) = ipSetReq;

	succeeded = SendData(message, sizeof(IP_SET_REQ), TRUE);

	delete[] message;
	return succeeded;
}

BOOL CExchange::ChangeReceiveFIFO(BYTE fifo_num)
{
	BOOL succeeded = FALSE;
	PUCHAR message = new UCHAR[MAX_MSG_SIZE];
	CHANGE_FIFO cf;

	cf.header.prefix = PREFIX;
	cf.header.cod_comand = 0x0779;
	cf.which_fifo = fifo_num;

	*((CHANGE_FIFO*)(message)) = cf;

	succeeded = SendData(message, sizeof(CHANGE_FIFO), TRUE);

	delete[] message;
	return succeeded;
}

void CExchange::MessageDisassembler(WORD* buffer, USER_STREAM_INFO* pSI)
{
	if (!pSI) return;

	ZeroMemory(pSI, sizeof(USER_STREAM_INFO));

	WORD n = 0;

	pSI->wParam		= *((DWORD*)(buffer+n));	n += sizeof(DWORD) / 2;
#ifdef PROTOCOL_VER3
	pSI->streamsNum = *((WORD*)(buffer + n));	n += sizeof(WORD) / 2;
#endif
	pSI->prog_num	= *((WORD*)(buffer+n));		n += sizeof(WORD) / 2;

	for(auto i = 0; i < pSI->prog_num; i++)
	{
		pSI->prog_info[i].wParam	= *((DWORD*)(buffer + n));	n += sizeof(DWORD) / 2;
#ifdef PROTOCOL_VER3
		pSI->prog_info[i].streamID	= *((WORD*)(buffer + n));	n += sizeof(WORD) / 2;
#endif
		memcpy_s(pSI->prog_info[i].prog_name,MAX_PROG_NAME,(BYTE*)(buffer+n),MAX_PROG_NAME);	n += MAX_PROG_NAME/2;
		memcpy_s(pSI->prog_info[i].prov_name,MAX_PROG_NAME,(BYTE*)(buffer+n),MAX_PROG_NAME);	n += MAX_PROG_NAME/2;
		pSI->prog_info[i].prog_type = *((WORD*)(buffer+n));		n += sizeof(WORD) / 2;
		pSI->prog_info[i].pids_num	= *((WORD*)(buffer+n));		n += sizeof(WORD) / 2;

		for(auto j = 0; j < pSI->prog_info[i].pids_num; j++)
		{
			pSI->prog_info[i].prog_pids[j] = *((USER_PIDS_INFO*)(buffer + n));
			n += sizeof(USER_PIDS_INFO) / 2;
		}
	}

	//BYTE buf_char[MAX_PRG_LIST_WORDS * 2];
	//memcpy_s(buf_char, MAX_PRG_LIST_WORDS * 2, prg_list_buffer, MAX_PRG_LIST_WORDS * 2);

	ZeroMemory(prg_list_buffer, MAX_PRG_LIST_WORDS*2);
	first_msg_flag	= TRUE;		// флаг что Лёня передаёт первую программу
	last_msg_flag	= FALSE;
	curr_prog_index = 0;
	num_of_progs	= 0;
}
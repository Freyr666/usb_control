#pragma once

#define PREFIX						0x55aa
#define MSG_1_1_LEN					4		//в байтах
#define MSG_1_2_LEN					8
#define MSG_STAT_LEN				sizeof(STATUS_MSG)

#define IP_ANSWER_LEN_DATA				32
#define IP_ANSWER_LEN_MSG				IP_ANSWER_LEN_DATA-8
#define DEC_VID_ANSWER_LEN_DATA			20
#define DEC_VID_ANSWER_LEN_MSG			(DEC_VID_ANSWER_LEN_DATA-8)
#define DEC_AUD_ANSWER_LEN_DATA			20
#define DEC_AUD_ANSWER_LEN_MSG			(DEC_AUD_ANSWER_LEN_DATA-8)
#define ANALIS_VID_ANSWER_LEN_DATA		40
#define ANALIS_VID_ANSWER_LEN_MSG		(ANALIS_VID_ANSWER_LEN_DATA-8)
#define ANALIS_AUD_ANSWER_LEN_DATA		32
#define ANALIS_AUD_ANSWER_LEN_MSG		(ANALIS_AUD_ANSWER_LEN_DATA-8)
#define DVB_SET_ANSWER_LEN_DATA			136
#define DVB_SET_ANSWER_LEN_MSG			(DVB_SET_ANSWER_LEN_DATA-8)
#define DVBT2_STATUS_ANSWER_LEN_DATA	126
#define DVBT2_STATUS_ANSWER_LEN_MSG		(DVBT2_STATUS_ANSWER_LEN_DATA-8)
#define DVBT_STATUS_ANSWER_LEN_DATA		62
#define DVBT_STATUS_ANSWER_LEN_MSG		(DVBT_STATUS_ANSWER_LEN_DATA-8)
#define DVBC_STATUS_ANSWER_LEN_DATA		46
#define DVBC_STATUS_ANSWER_LEN_MSG		(DVBC_STATUS_ANSWER_LEN_DATA-8)

//ATS MESSAGE CODES
#define IP_ANSWER_CODE			0xc511
#define DEC_VID_ANSWER_CODE		0xc512
#define DEC_AUD_ANSWER_CODE		0xc513
#define ANALIS_VID_ANSWER_CODE	0xc514
#define ANALIS_AUD_ANSWER_CODE	0xc515
#define PRG_LIST_ANSWER_CODE	0xc516	// Added by TeddyJack
#define DVB_SET_ANSWER_CODE		0xc518
#define DVB_STAT_ANSWER_CODE	0xc519

//cod command
#define EXIT_RECEIVE			0x40
#define START_MSG				0x10
#define STOP_MSG				0x20
#define MSG_VAR_LEN				0x01
#define MSG_CRC					0x02

#define VERSION					0x0100	//1.2
#define STATUS					0x0300	//2.2
#define STA_MPEG				0x0411	//2.0	
#define TRA_MPEG				0x0401	//2.0
#define STO_MPEG				0x0421	//2.0
#define STA_STRU				0x0613	//1.4
#define TRA_STRU				0x0603	//1.4
#define STO_STRU				0x0623	//1.4
#define STASTO_STRU				0x0633	//1.4	//структура потока умещается в 1 пакет
#define STA_SPEED				0x0713	//1.4
#define TRA_SPEED				0x0703	//1.4
#define STO_SPEED				0x0723	//1.4
#define STASTO_SPEED			0x0733	//1.4
#define MEGABUF_STAT			0x0810	//1.1
#define MEGABUF_STA				0x0813	//1.4
#define MEGABUF_TRA				0x0803	//1.4
#define MEGABUF_TRPAUSE			0x0843	//1.3
#define MEGABUF_TRACONN			0x0820	//1.2
#define	INDBUF_TR_WAIT			0x0903	//1.5
#define	INDBUF_TR				0x0943	//1.5
#define	EXIT_MSG				0xFF00	//1.1
#define SEND_PERS_BUF			0x0901	//
//ATS
#define ATS_Set_IP_mode			0x0501	//3.1
#define ATS_Set_Dec_Video		0x0502	//3.2
#define ATS_Set_Dec_Audio		0x0503	//3.3
#define ATS_Set_Analis_Video	0x0504	//3.4
#define ATS_Set_Analis_Audio	0x0505	//3.5
#define ATS_Set_Prg_List		0x0506
#define ATS_Get_IP_mode			0x0511	//3.6
#define ATS_Get_Dec_Video		0x0512	//3.8
#define ATS_Get_Dec_Audio		0x0513	//3.10
#define ATS_Get_Analis_Video	0x0514	//3.12
#define ATS_Get_Analis_Audio	0x0515	//3.14
#define ATS_Get_Prg_List		0x0516	//3.18	Added by TeddyJack
#define ATS_Set_Control_DVBT2	0x0517	//
#define ATS_Get_Control_DVBT2	0x0518	//
#define ATS_Get_Status_DVBT2	0x0519
#define ATS_Power_Off			0x0800	//
#define Reset_board				0x0111	//
#define ATS_Get_Dtm_Settings	0x0780	//
//

//ETHERNET MESSAGES
#define GET_BOARD_INFO			0x0080	//1.1
#define GET_BOARD_MODE			0x0081	//1.1
#define SET_BOARD_MODE			0x0082	//1.2
#define MEGABUF_STATUS			0x0083	//1.2
#define MEGABUF_RESULT			0x0084	//1.2
#define CLOSE_CLIENT			0x0085	//1.2
#define OPEN_CLIENT				0x0086	//1.2
#define SET_BOARD_MODE_EXT		0x0087	//1.2

#define MAX_DATA_SIZE			243		// 243 words

#pragma pack(1)
struct HEAD
{
	WORD prefix;
	WORD cod_comand;
};

struct MSG_1_1		//пустое сообщение
{
	HEAD header;
};

struct MSG_1_2		//тип/версия
{
	HEAD header;
	BYTE type;
	BYTE version;
	WORD reserved;
};

struct ERR_MSG			//2.0
{
	HEAD header;
	WORD length;
#ifdef PROTOCOL_VER3
	WORD TS_index;
	DWORD TS_Num;
	WORD reserve;
#endif
	WORD err_num; 
};

struct ERR_INFO
{
	WORD index;
	WORD count;
	BYTE err_code;	//
	BYTE err_ext;	//order of this bytes changed specially
	WORD PID;
	DWORD packet;
	DWORD param1;
	DWORD param2;
};

struct STATUS_MSG
{
	HEAD header;
#ifdef PROTOCOL_VER3
	DWORD reserved;
	//word 0
	WORD TS_Count;
	//word 1
	BYTE reserved_2;
#else
	BYTE reserved;
#endif
	BYTE flags;
	//word 2
	BYTE stat_version;
	BYTE set_version;
	//word 3
	BYTE video_load;
	BYTE audio_load;
	//word 4
	BYTE dvbt2_opt_status_version;
	BYTE dvbt2_opt_control_version;
	//word 5-14
#ifdef PROTOCOL_VER3
	BYTE err_flags[MAX_ANALYZED_PROG_NUM];
	//word 15-24
	BYTE loudness_levels[MAX_ANALYZED_PROG_NUM];
#else
	BYTE err_flags[12];
	BYTE loudness_levels[12];
#endif
#ifdef PROTOCOL_VER3
	WORD reserved_arr[225];
#endif
};

struct GET_IP_MODE_ANSWER
{
	HEAD header;
	WORD length_data;
	WORD client_id;
	WORD mess_cod;
	WORD request_id;
	WORD length_mess;
	DWORD IP_addr;
	DWORD IP_mask;
	DWORD IP_gateway;
	DWORD IP_group;
	WORD UDP_port;
	WORD Internal_port;
	WORD flags;
	WORD reserve;
};

struct GET_ANALIS_VIDEO_ANSWER
{
	HEAD header;
	WORD length_data;
	WORD client_id;
	WORD mess_cod;
	WORD request_id;
	WORD length_mess;
	float level_black_user;
	float level_black;
	float level_ident_user;
	float level_ident;
	float level_run;
	BYTE time_stop_video;
	BYTE mean_y_level;
	BYTE num_black_frames;
	BYTE black_level;
	BYTE difference_pixel;
	BYTE num_freeze_frames;
	WORD reserve1;
	DWORD reserve2;
};

struct GET_ANALIS_AUDIO_ANSWER
{
	HEAD header;
	WORD length_data;			
	WORD client_id;				//2
	WORD mess_cod;				//4
	WORD request_id;			//6
	WORD length_mess;			//8
	float level_over_user;		//12		//4
	float level_over;			//16		//8
	float level_under_user;		//20		//12
	float level_under;			//24		//16
	BYTE time_stop_audio;		//25		//17
	BYTE reserved1;				//26		//18
	WORD reserved2;				//28		//20
	DWORD reserved3;			//32		//24
};


struct GET_PROGRAMS_LIST			// Added by TeddyJack
{
	HEAD header;
	WORD length_data;
	WORD client_id;
	WORD mess_cod;
	WORD request_id;
	WORD length_mess;
	WORD data [MAX_DATA_SIZE];
};

struct GET_CONTROL_DVBT2_DATA
{
	BYTE device;			//1
	BYTE reserve3;			//2
	WORD reserve4;			//4
	DWORD dvbc_freq;		//8
	DWORD dvbt_freq;		//12
	WORD dvbt_band;			//14
	WORD reserve5;			//16
	DWORD dvbt2_freq;		//20
	WORD dvbt2_band;		//22
	BYTE dvbt2_plp;			//23
	BYTE reserve_arr[95];	//23+95 = 118
};

struct GET_CONTROL_DVBT2
{
	HEAD header;
	WORD length_data;
	WORD client_id;			//2
	WORD mess_cod;			//4
	WORD request_id;		//6
	WORD length_mess;		//8
	BYTE control;			//9		//1		
	BYTE dataLength;		//10	//2
	DWORD reserve1;			//14	//6
	DWORD reserve2;			//18	//10			
	GET_CONTROL_DVBT2_DATA data;	//sizeofdata (118)			//length_data = 118 + 18 = 136
																//length_mess = 118 + 10 = 128
																//dataLength = 118
};

struct GET_STATUS_DVBT2_DATA
{
	BYTE device;			//19
	BYTE reserve3;			//20
	WORD status;			//22
	DWORD frequency;		//26
	DWORD snr;				//30
	DWORD ber;				//34
	DWORD reserve4;			//38
	BYTE T2_stream_type;	//39
	BYTE pilot_pattern;		//40
	BYTE L1_FEC;			//41
	BYTE L1_code_rate;		//42
	BYTE PAPR_indicator;	//43
	BYTE L1_modulation;		//44
	BYTE FFT_mode;			//45
	BYTE guard_interval;	//46
	WORD cellID;			//48
	WORD networkID;			//50
	WORD systemID;			//52
	WORD reserve5;			//54
	BYTE basic_type;		//55
	BYTE num_plp;
	BYTE PLP_IDs[64];		//55+64 = 119
	BYTE PLP_ID;			//120
	BYTE PLP_type;			//121
	BYTE PLP_payload;		//122
	BYTE PLP_constel;		//123
	BYTE PLP_code_rate;		//124
	BYTE PLP_FEC;			//125
};

struct GET_STATUS_DVBT2			//dataLength = 107
{
	HEAD header;			
	WORD length_data;
	WORD client_id;			//2
	WORD mess_cod;			//4
	WORD request_id;		//6
	WORD length_mess;		//8
	BYTE status_ver;		//9		
	BYTE dataLength;		//10	
	BYTE opt_type;			//11	
	BYTE opt_ver;			//12
	DWORD reserve1;			//16
	WORD reserve2;			//18
	GET_STATUS_DVBT2_DATA data;	//125
};

struct GET_STATUS_DVBT_DATA
{
	BYTE device;
	BYTE reserve3;
	WORD status;
	DWORD frequency;
	DWORD snr;
	DWORD ber;
	DWORD reserve4;
	DWORD modulation;
	DWORD transmit_mode;
	DWORD guard_interval;
	DWORD hierarchy;
	DWORD HP_fec_code_rate;
	DWORD LP_fec_code_rate;
};


struct GET_STATUS_DVBT			//dataLength = 44
{
	HEAD header;
	WORD length_data;
	WORD client_id;				//
	WORD mess_cod;				//
	WORD request_id;			//
	WORD length_mess;			//
	BYTE status_ver;			//
	BYTE dataLength;			//
	BYTE opt_type;				//
	BYTE opt_ver;				//
	DWORD reserve1;				//
	WORD reserve2;				//18
	GET_STATUS_DVBT_DATA data;	//44+18 = 62
};

struct GET_STATUS_DVBC_DATA
{
	BYTE device;
	BYTE reserve3;
	WORD status;
	DWORD frequency;
	DWORD snr;
	DWORD ber;
	DWORD reserve4;
	DWORD modulation;
	DWORD sym_rate;
};

struct GET_STATUS_DVBC			//dataLength = 28
{
	HEAD header;
	WORD length_data;
	WORD client_id;		//2
	WORD mess_cod;		//4
	WORD request_id;	//6
	WORD length_mess;	//8
	BYTE status_ver;	//9
	BYTE dataLength;	//10
	BYTE opt_type;		//11
	BYTE opt_ver;		//12
	DWORD reserve1;		//16
	WORD reserve2;		//18
	GET_STATUS_DVBC_DATA data;		//28+18 = 46
};

struct IP_SETTINGS_ONLY
{
	BYTE volatile_mem;
	DWORD ip_addr;
	DWORD subn_mask;
	DWORD gateway;
	BYTE dhcp_en;
	BYTE ip_transmit_en;
	BYTE fec_en;
	WORD fec_col;
	WORD fec_rows;
	DWORD dest_ip;
	WORD udp_port;
	BYTE tp_per_ip;
	BYTE protocol;
	BYTE ttl;
	BYTE packet_size;
	BYTE input_select;
	BYTE mode;
	BYTE application;
};

struct SET_IP_SETTINGS
{
	HEAD header;
	WORD length_data;
	IP_SETTINGS_ONLY ip_settings;
	BYTE reboot;
};

struct IP_SET_REQ
{
	HEAD header;
};

struct CHANGE_FIFO
{
	HEAD header;
	BYTE which_fifo;
};

struct SET_LEDS
{
	HEAD header;
	BYTE leds;
};

#pragma pack()
#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#endif

#include "ice_ipcsdk.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef USEICONV
#include "./include_iconv/iconv.h"
#else
#ifdef COMPILEUTF8
#include <iconv.h>
#include <arpa/inet.h>
#endif
#endif
//#include <pthread.h>
#include <stdbool.h>
#include "ice_vdc_result.h"
#include <stdint.h>
#include "thread.h"

#define OFFLINE 0
#define ONLINE  1
#define MAX_OSD_TEXT 64

#define MAX_MSTAR_OSD_TEXT	(30)
#define MAX_MSTAR_OSD_LINE	(4)


static int g_run = 1;
static ICE_HANDLE hSDK = NULL;
static int g_nTransPortTime = 0;
static int g_nTransPortTime_rs232 = 0;
static int g_nOpenGateTime = 0;
static int g_nEnableLogRs485 = 0;
static int g_nEnableLogRs232 = 0;
static int g_nCapture = 0;
static char g_ip[32] = {0};
// char cIp[20];
typedef void (*evtp_callback_t)(void *param, 
	uint32_t type, void *data, int len);


typedef struct evtp_data
{
	uint32_t type;
	void *data;
	int len;

	struct evtp_data *next;

} evtp_data_t;

#define BUF_SIZE (2 * 1024 * 1024)

typedef struct
{
	pthread_t thread, thread_callback, thread_timer, thread_delfile;
	int exit;
	
	char ip[32];
	uint16_t port;
	evtp_callback_t callback;
	void *param;

	uint8_t encrypt;
	char passwd[128];
	int sockfd;
	int loss_count, timer;
	char buf[BUF_SIZE];
	int connected;

	evtp_data_t *head, *tail;
	pthread_mutex_t mutex;

	void* hLog;

} evtp_param_t;


#ifdef COMPILEUTF8
//编码转换
int code_convert(const char *from_charset,const char *to_charset,char *inbuf,size_t inlen,char *outbuf,size_t outlen)
{
  iconv_t cd;
  char **pin = &inbuf;
  char **pout = &outbuf;

  cd = iconv_open(to_charset,from_charset);
  if (cd==0)
       return -1;
  
  memset(outbuf,0,outlen);
  if (iconv(cd,pin,&inlen,pout,&outlen)==-1)
       return -1;
  
  iconv_close(cd);
  return 0;
}

//GB2312码转为UNICODE码

int g2u(char *inbuf,size_t inlen,char *outbuf,size_t outlen)
{
	return code_convert("gb2312", "utf-8", inbuf, inlen, outbuf, outlen);
}

#endif

//创建文件夹
int   CreatePath(const char *sPathName)  
{  
  char   DirName[256];  
  strcpy(DirName, sPathName);  
  int   i,len = strlen(DirName);  
  if(DirName[len-1]!='/')  
  strcat(DirName,   "/");  
   
  len = strlen(DirName);  
   
  for(i=1; i<len; i++)  
  {  
  	if(DirName[i]=='/')  
  	{  
  		DirName[i] = 0;  
  		if(access(DirName, R_OK)!=0)  
  		{  
    		if(mkdir(DirName, 0755)==-1)  
     		{   
        		perror("mkdir   error");   
            	return   -1;   
      		}  
  		}  
 	 DirName[i]   =   '/';  
	}  
}  
   
  return   0;  
}

//记录日志，demo中用来记录收到的rs485和rs232串口数据
static void logFile(char* filename,char* str,char* str2)
{
	time_t t;
	FILE *fp;
	char now_str[100];
	struct tm* tp;
	time(&t);
	tp = localtime(&t);
	strftime(now_str, 100, "%Y-%m-%d %H:%M:%S", tp);

	fp = fopen(filename, "a");
	if (fp == NULL) {  
		return;
	}

	fprintf(fp, "%s :%s,%s\r\n",now_str, str, str2);
	fclose(fp);
}

//存储车辆特征码
static void StoreResFeature(ICE_VBR_RESULT_S *vbr, char* filePath, char* picName, char* plateNum)
{
	if (NULL == vbr)
		return;
	int i = 0;

	if (CreatePath(filePath) != 0 )
	{
		return;
	}
	char fileName[256];
	memset(fileName, 0, sizeof(fileName));
	sprintf(fileName, "%s/vbr_record.txt", filePath);

	FILE *fp = fopen(fileName, "a");
	if (NULL == fp)
		return;

	if (NULL != vbr)
	{
		char szFeature[1024] = {0};
		float feature_tmp[VBR_RES_TOPN * 2];
		memset(feature_tmp, 0, sizeof(feature_tmp));
		memcpy(feature_tmp, vbr->fResFeature, sizeof(feature_tmp));
		for (i = 0; i < VBR_RES_TOPN * 2; i++)
		{
			char szFloat[32];
			memset(szFloat, 0, sizeof(szFloat));
			//sprintf(szFloat, "%f", vbr->fResFeature[i]);//转为float类型
			sprintf(szFloat, "%f", feature_tmp[i]);

			if (0 != i)
				strcat(szFeature, " ");

			strcat(szFeature, szFloat);
		}

		//printf("%s\n", szFeature);

		fprintf(fp, "%s %s %s\n", picName, plateNum, szFeature);//存储	
	}

	fclose(fp);
}

//存储mac列表
static void StorePhoneMacList(ICE_Phone_Mac_List *list, char* filePath, char* picName, char* plateNum)
{
	if (NULL == list)
		return;
	int i = 0;

	if (CreatePath(filePath) != 0 )
	{
		return;
	}
	char fileName[256];
	memset(fileName, 0, sizeof(fileName));
	sprintf(fileName, "%s/macList_record.txt", filePath);

	FILE *fp = fopen(fileName, "a");
	if (NULL == fp)
		return;

	if (NULL != list)
		fprintf(fp, "%s %s,总数：%d\n%s\n", picName, plateNum, list->mac_list_size, list->phone_mac_list);
		
	fclose(fp);
}

//保存图片
static void savePic(const char* pcIp, const char* pcNumber, void* pvPicData, 
	long nPicLen, int bPlate, long capTime, char* pcLogName)
{
	if (NULL == pcIp || NULL == pcNumber || NULL == pvPicData)
		return;
	char path[256];
	char name[512];
	char name1[512];
	FILE *fp;
	char filePath[256];
	char *tmpPath;
	static char headPath[256] = {'\0'};
	static bool nflag = 0;

	memset(path, 0, sizeof(path));
	memset(name, 0, sizeof(name));
	memset(name1, 0, sizeof(name1));
	memset(filePath, 0, sizeof(filePath));

	if (!nflag)
	{
		memset(filePath, 0, sizeof(filePath));
		fp = fopen("./fileConfig", "r");
		if (NULL != fp)
		{
			fread(filePath, sizeof(char), sizeof(filePath), fp);
			tmpPath = strtok(filePath, "\n");//get the file path
			strcpy(headPath, tmpPath);
			nflag = 1;
			fclose(fp);
		}
		printf("%s\n", headPath);		
	}

	struct tm *timenow;
	if (0 == capTime)
	{
		time_t now;	
		time(&now);
		timenow = localtime(&now);
	}
	else
	{
		timenow = localtime(&capTime);
	}
	

	sprintf(path,"%s/zhuapai/%s/%4d%02d%02d", headPath, pcIp, timenow->tm_year+1900, timenow->tm_mon+1, timenow->tm_mday);
	//printf("SavePic:%s\r\n", path);
	if (CreatePath(path) != 0 )
	{
		return;
	}
	
	if (NULL == pcLogName)//如果有主品牌，则主品牌也要加到文件名中
		sprintf(name1, "%4d%02d%02d%02d%02d%02d_%s", timenow->tm_year+1900, timenow->tm_mon+1, timenow->tm_mday, 
			timenow->tm_hour,timenow->tm_min,timenow->tm_sec, pcNumber);
	else
		sprintf(name1, "%4d%02d%02d%02d%02d%02d_%s_%s", timenow->tm_year+1900, timenow->tm_mon+1, timenow->tm_mday, 
			timenow->tm_hour,timenow->tm_min,timenow->tm_sec, pcLogName, pcNumber);
	if (bPlate == 0)//全景图
	{
		sprintf(name, "%s/%s.jpg",path, name1);
	}
	else if(bPlate == 1)//车牌图
	{
		sprintf(name, "%s/%s_Plate.jpg",path, name1);
	}
	else if(bPlate == 2)//软触发接口获得的图
	{
		sprintf(name, "%s/%s_Trigger.jpg",path, name1);
	}
	else if (bPlate == 3 || bPlate == 4)//3为车辆特征码，4为mac列表
	{
		sprintf(name, "%s.jpg", name1);
		if (bPlate == 3)
		{
			ICE_VBR_RESULT_S *vbr = (ICE_VBR_RESULT_S *)pvPicData;
			StoreResFeature(vbr, path, name, (char*)pcNumber);
		}
		else if (bPlate == 4)
		{
			ICE_Phone_Mac_List *list = (ICE_Phone_Mac_List *)pvPicData;
			StorePhoneMacList(list, path, name, (char*)pcNumber);
		}
		
		return;
	}
	
	//printf("PicName:%s\r\n", name);
	//存图
	fp = fopen(name, "wb");
	if (NULL != fp)
	{
		fwrite(pvPicData, 1, nPicLen, fp);
		fclose(fp);
	}
	else
	{
		printf("open file failed!\r\n");
	}	
}

static const char* g_szVehicleColor[] = 
{
	"未知", 
	"黑色",
	"蓝色",
	"灰色",
	"棕色",
	"绿色",
	"夜间深色",
	"紫色",
	"红色",
	"白色",
	"黄色"
};

static const char* g_szAlarmType[] = 
{
	"实时_硬触发+临时车辆",
	"实时_视频触发+临时车辆",
	"实时_软触发+临时车辆",
	"实时_硬触发+白名单",
	"实时_视频触发+白名单",
	"实时_软触发+白名单",
	"实时_硬触发+黑名单",
	"实时_视频触发+黑名单",
	"实时_软触发+黑名单",
	"脱机_硬触发+临时车辆",
	"脱机_视频触发+临时车辆",
	"脱机_软触发+临时车辆",
	"脱机_硬触发+白名单",
	"脱机_视频触发+白名单",
	"脱机_软触发+白名单",
	"脱机_硬触发+黑名单",
	"脱机_视频触发+黑名单",
	"脱机_软触发+黑名单",
	"实时_硬触发+过期白名单",
	"实时_视频触发+过期白名单",
	"实时_软触发+过期白名单",
	"脱机_硬触发+过期白名单",
	"脱机_视频触发+过期白名单",
	"脱机_软触发+过期白名单"
};

static const char* g_szVehicleDir[] =
{
	"车头方向",
	"车尾方向",
	"车头和车尾方向"
};

static const char* g_szVehicleType[] = 
{
	"未知",
	"轿车",
	"面包车",
	"大型客车",
	"中型客车",
	"皮卡",
	"非机动车",
	"SUV",
	"MPV",
	"微型货车",
	"轻型货车",
	"中型货车",
	"重型货车"
};

//实时抓拍数据
void OnPlate(void *pvParam, 
	const char* pcIP, const char* pcNumber, const char* pcColor, 
	void* pvPicData, long nPicLen, void* pvPlatePicData, long nPlatePicLen,
	long nPlatePosLeft, long nPlatePosTop, long nPlatePosRight, long nPlatePosBottom, 
	float fPlateConfidence, long nVehicleColor, 
	long nPlateType, long nVehicleDir, long nAlarmType, long nFalsePlate,
	long nCapTime, long nVehicleType, long nResultHigh, long nResultLow)
{
	static int nPlateNum = 0;
	char number[32] = {'\0'};
	char color[32] = {'\0'};
	char now_str[64];
	memset(now_str, 0 ,sizeof(now_str));
	struct tm* tp;
	tp = localtime(&nCapTime);
	strftime(now_str, sizeof(now_str), "%Y-%m-%d %H:%M:%S", tp);

//64位指针为8位，需使用高地址位和低地址位
 #ifdef X64
 	ICE_VDC_PICTRUE_INFO_S *info = (ICE_VDC_PICTRUE_INFO_S *)((uint64_t)(nResultHigh<<32) + (uint64_t)nResultLow);
 #else
 	ICE_VDC_PICTRUE_INFO_S *info = (ICE_VDC_PICTRUE_INFO_S *)nResultLow;
 #endif

#ifdef COMPILEUTF8//编译宏，表示要做转码，车牌号和车牌颜色需要由gb2312转为utf8
	/*转换编码，gb2312 to unicode,若不需要，则不调用*/
	g2u((char*)pcNumber,strlen(pcNumber),number,sizeof(number));
	g2u((char*)pcColor,strlen(pcColor),color,sizeof(color));

	//需要显示在界面的信息
	printf("Plate:%d %s,%s,%s,%s,%s,%s,%s,%f", ++nPlateNum, now_str, number, color, g_szVehicleColor[nVehicleColor+1],
	 g_szAlarmType[nAlarmType], g_szVehicleDir[nVehicleDir], g_szVehicleType[nVehicleType], fPlateConfidence);
	if(nFalsePlate == 1)
		printf(",疑似虚假车牌");
	if (NULL != info)
	{
		printf(",图片ID: %d", info->u32PictureHashID);
		if (NULL != info->pstVbrResult)
		{
			if (0 == strlen(info->pstVbrResult->szLogName))
			strcpy(info->pstVbrResult->szLogName, "未知");
			printf(",主品牌:%s", info->pstVbrResult->szLogName);
		}		
	}
		
	printf("\n");
	//存车牌图
	if (NULL != pvPicData && nPicLen > 0)
	{
		if (NULL != info && NULL != info->pstVbrResult)
		{
			savePic(pcIP, number, pvPicData, nPicLen, 0, 0, info->pstVbrResult->szLogName);
			savePic(pcIP, number, info->pstVbrResult, 0, 3, 0, info->pstVbrResult->szLogName);//存车辆特征码
			if (NULL != info && NULL != info->pPhoneMacList)
				savePic(pcIP, number, info->pPhoneMacList, 0, 4, 0, info->pstVbrResult->szLogName);//存mac列表
		}
		
		else
		{
			savePic(pcIP, number, pvPicData, nPicLen, 0, 0, NULL);
			if (NULL != info && NULL != info->pPhoneMacList)
				savePic(pcIP, number, info->pPhoneMacList, 0, 4, 0, NULL);
		}			
	}
	
	//存车牌图
	if (NULL != pvPlatePicData && nPlatePicLen > 0)
	{
		if (NULL != info && NULL != info->pstVbrResult)
			savePic(pcIP, number, pvPlatePicData, nPlatePicLen, 1, 0, info->pstVbrResult->szLogName);
		else
			savePic(pcIP, number, pvPlatePicData, nPlatePicLen, 1, 0, NULL);
	}
#else//不做转码
	printf("Plate:%d %s,%s,%s,%s,%s,%s,%s", ++nPlateNum, now_str, pcNumber, pcColor, g_szVehicleColor[nVehicleColor+1],
	 g_szAlarmType[nAlarmType], g_szVehicleDir[nVehicleDir], g_szVehicleType[nVehicleType]);
	if(nFalsePlate == 1)
		printf(",疑似虚假车牌");
	if (NULL != info && NULL != info->pstVbrResult)
		printf(",主品牌:%s", info->pstVbrResult->szLogName);
	printf("\n");

	if (NULL != pvPicData)
	{
		if (NULL != info && NULL != info->pstVbrResult)
		{
			savePic(pcIP, pcNumber, pvPicData, nPicLen, 0, 0, info->pstVbrResult->szLogName);
			savePic(pcIP, pcNumber, info->pstVbrResult, 0, 3, 0, info->pstVbrResult->szLogName);
			if (NULL != info && NULL != info->pPhoneMacList)
				savePic(pcIP, pcNumber, info->pPhoneMacList, 0, 4, 0, info->pstVbrResult->szLogName);
		}
		
		else
		{
			savePic(pcIP, pcNumber, pvPicData, nPicLen, 0, 0, NULL);
			if (NULL != info && NULL != info->pPhoneMacList)
				savePic(pcIP, pcNumber, info->pPhoneMacList, 0, 4, 0, NULL);
		}		
	}
	
	if (NULL != pvPlatePicData && nPlatePicLen > 0)
	{
		if (NULL != info && NULL != info->pstVbrResult)
			savePic(pcIP, pcNumber, pvPlatePicData, nPlatePicLen, 1, 0, info->pstVbrResult->szLogName);
		else
			savePic(pcIP, pcNumber, pvPlatePicData, nPlatePicLen, 1, 0, NULL);
	}
#endif
}

//断网抓拍数据
void OnPastPlate(void *pvParam, const char *pcIP, 
	long nCapTime, const char *pcNumber, const char *pcColor, 
	void *pvPicData, long nPicLen, void *pvPlatePicData, long nPlatePicLen, 
	long nPlatePosLeft, long nPlatePosTop, long nPlatePosRight, long nPlatePosBottom, 
	float fPlateConfidence, long nVehicleColor, long nPlateType, long nVehicleDir, 
	long nAlarmType, long nReserved1, long nReserved2, long nReserved3, long nReserved4)
{
	static int nPastPlateNum = 0;
	char number[32] = {'\0'};
	char color[32] = {'\0'};
	char now_str[64];
	memset(now_str, 0 ,sizeof(now_str));
	struct tm* tp;
	tp = localtime(&nCapTime);	
	strftime(now_str, sizeof(now_str), "%Y-%m-%d %H:%M:%S", tp);


#ifdef COMPILEUTF8//编译宏，表示要做转码，车牌号和车牌颜色需要由gb2312转为utf8
	/*转换编码，gb2312 to unicode,若不需要，则不调用*/
	g2u((char*)pcNumber,strlen(pcNumber),number,sizeof(number));
	g2u((char*)pcColor,strlen(pcColor),color,sizeof(color));

	printf("Past Plate:%d %s,%s,%s,%s,%s,%s\n", ++nPastPlateNum, now_str, number, color, g_szVehicleColor[nVehicleColor+1],
	 g_szAlarmType[nAlarmType], g_szVehicleDir[nVehicleDir]);
	
	if (NULL != pvPicData && nPicLen > 0)
	{
		savePic(pcIP, number, pvPicData, nPicLen, 0, nCapTime, NULL);//存全景图
	}
	
	if (NULL != pvPlatePicData && nPlatePicLen > 0)
	{
		savePic(pcIP, number, pvPlatePicData, nPlatePicLen, 1, nCapTime, NULL);//存车牌图
	}
#else//不做转码
	printf("Past Plate:%d %s,%s,%s,%s,%s,%s\n", ++nPastPlateNum, now_str, pcNumber, pcColor, g_szVehicleColor[nVehicleColor+1],
	 g_szAlarmType[nAlarmType], g_szVehicleDir[nVehicleDir]);

	if (NULL != pvPicData && nPicLen > 0)
	{
		savePic(pcIP, pcNumber, pvPicData, nPicLen, 0, nCapTime, NULL);
	}
	
	if (NULL != pvPlatePicData && nPlatePicLen > 0)
	{
		savePic(pcIP, pcNumber, pvPlatePicData, nPlatePicLen, 1, nCapTime, NULL);
	}
#endif
}

//接收到的rs485串口数据
void OnSerialPort(void *pvParam, 
	const char *pcIP, char *pcData, long u32Len)
{
	// char temp[u32Len+1];
	// memset(temp, 0, sizeof(temp));
	// strncpy(temp, pcData, u32Len);
	static long nRecvPortData = 0;
	int i = 0;
	char buf[1024];
	memset(buf, 0, sizeof(buf));

	printf("%ld: %s: recv rs485 SerialPort data Number:%ld\nrecv rs485 data: ", ++nRecvPortData, pcIP, u32Len);
	
	for (; i < u32Len; ++i)
	{
		printf("%02hhx ", pcData[i]);//显示
		char temp[16];
		memset(temp, 0, sizeof(temp));
		sprintf(temp, "%02hhX ", pcData[i]);
		if (1024 - strlen(buf) <= strlen(temp))
			break;
		strcat(buf, temp);
	}

	printf("\n");

	char szNum[32];
	memset(szNum, 0, sizeof(szNum));
	sprintf(szNum, "%ld", nRecvPortData);

	if (1 == g_nEnableLogRs485)
		logFile("log_rs485", szNum, buf);//存储	
}

//接收到的rs232数据
void OnSerialPort_rs232(void *pvParam, 
	const char *pcIP, char *pcData, long u32Len)
{
	static long nRecvPortData_rs232 = 0;
	int i = 0;
	char buf[1024];
	memset(buf, 0, sizeof(buf));

	printf("%ld: %s: recv rs232 SerialPort data Number:%ld\nrecv rs232 data: ", ++nRecvPortData_rs232, pcIP, u32Len);

	for (; i < u32Len; ++i)
	{
		printf("%02hhx ", pcData[i]);//显示
		char temp[16];
		memset(temp, 0, sizeof(temp));
		sprintf(temp, "%02hhX ", pcData[i]);
		if (1024 - strlen(buf) <= strlen(temp))
			break;
		strcat(buf, temp);
	}

	printf("\n");

	char szNum[32];
	memset(szNum, 0, sizeof(szNum));
	sprintf(szNum, "%ld", nRecvPortData_rs232);

	if (1 == g_nEnableLogRs232)
		logFile("log_rs232", szNum, buf);	//存储
}

//相机事件处理，现在包括IO状态改变事件和相机连接状态事件
void OnDeviceEvent(void *pvParam, const char *pcIP, long u32EventType, 
	long u32EventData1, long u32EventData2, long u32EventData3, long u32EventData4)
{
	if (u32EventType == 2)
		printf("IO状态改变，值为：%ld%ld%ld%ld\n", u32EventData1,u32EventData2,u32EventData3,u32EventData4);
	else
	{
		if (1 == u32EventType)
			printf("%s当前状态:在线\n", pcIP);
		else if (0 == u32EventType)
			printf("%s当前状态:离线\n", pcIP);
	}
}

void OnStateEvent(void *pvParam,  const char *pcIP, char *pcData, long u32Len)
{
	ICE_NET_CONN_STAT_S *state = (ICE_NET_CONN_STAT_S*)pcData;
	if(ICE_NET_CONN_STAT_PC_SDK_ONLINE == state->eNetStat)
	{
		printf("_____offline event _PC_ipc sdk PC连接上板子____\n");
	}
	else if(ICE_NET_CONN_STAT_PC_SDK_OFFLINE == state->eNetStat)
	{
		printf("_____offline event _PC_ipc sdk PC端脱机事件发生____\n");		
	}
}


#ifdef _WIN32
BOOL WINAPI handle_sig(DWORD msgType)
{
	if ((CTRL_C_EVENT == msgType) || (CTRL_CLOSE_EVENT == msgType))
    {
        g_run = 0;
		Sleep(300);
        return TRUE;
    }

    return FALSE;
}
#else
void handle_sig(int signo)
{
    if (SIGINT == signo)
    	g_run = 0;
}
#endif

//开闸线程
void openGate()
{
	static int nOpenGateNum = 0;
	int success = 0;

	while (g_run)
	{
		success = ICE_IPCSDK_OpenGate(hSDK);
		if (success)
		{
			printf("openGate:%d\r\n", ++nOpenGateNum);
		}
		usleep(g_nOpenGateTime*1000);
	}
}

//获取颜色值
long getColor(long pos)
{
	long lColor = 0;
	switch(pos)
	{
	case 0:
		lColor = 0x000000;
		break;
	case 1:
		lColor = 0xFFFFFF;
		break;
	case 2:
		lColor = 0x0000FF;//红
		break;
	case 3:
		lColor = 0xFF0000;//蓝
		break;
	case 4:
		lColor = 0x00FF00;//绿
		break;
	case 5:
		lColor = 0x00FFFF;//黄
		break;
	case 6:
		lColor = 0xFF00FF;//紫
		break;
	default:
		lColor = 0x000000;
		break;
	}
	return lColor;
}

//判断相机ip是否不合理
int is_valid_ip(const char *ip, int index) 
{ 
    int section = 0; //每一节的十进制值 
    int dot = 0; //几个点分隔符 
    int last = -1; //每一节中上一个字符 
    while(*ip)
    { 
        if(*ip == '.')
        { 
        	if (*(ip+1))
        	{
        		if (*(ip+1) == '.')
        			return 0;
        	}
        	else
        		return 0;
            dot++; 
            if(dot > 3)
            { 
                return 0; 
            } 
            if(section >= 0 && section <=255)
            { 
                section = 0; 
            }else{ 
                return 0; 
            } 
        }else if(*ip >= '0' && *ip <= '9')
        { 
            section = section * 10 + *ip - '0'; 
        }else{ 
            return 0; 
        } 
        last = *ip; 
        ip++; 
    }

    if(section >= 0 && section <=255)
    { 
        if(3 == dot)
        {
        	if ((section == 0 || section == 255) && (0 == index))
        	{
        		return 0;
        	}
        return 1;
      }
    } 
    return 0; 
}

//获取识别区域坐标值
int  getLoopParam(char *data, long *param)
{
	if (NULL == data)
		return 0;
	char szData[4096] = {0};
	strcpy(szData, data);
	int i = 0;
	char *pch = strtok(szData, ";\n,\t ");
	while (pch != NULL)
	{
		if (i < 9)
		{
			param[i] = atol(pch);
			pch = strtok(NULL, ";\n,\t ");
			i++;
		}		
	}

	if ((param[5] == 0) || (param[6] == 0) || (i != 9))
		return 0;
	
	if ((param[1]>=param[3])||(param[7]>param[5])||(param[2]>=param[8])||(param[4]>=param[6]))
		return -1;
	if (param[1] >= 176 ||param[1] <= 0)
		return -1;
	if (param[2] >= 144 ||param[1] <= 0)
		return -1;
	if (param[3] >= 176 ||param[1] <= 0)
		return -1;
	if (param[4] >= 144 ||param[1] <= 0)
		return -1;
	if (param[5] >= 176 ||param[1] <= 0)
		return -1;
	if (param[6] >= 144 ||param[1] <= 0)
		return -1;
	if (param[7] >= 176 ||param[1] <= 0)
		return -1;
	if (param[8] >= 144 ||param[1] <= 0)
		return -1;

	return 1;
}

//获取是否记录收到的串口数据的配置
int getPortLog(char *data)
{
	if (NULL == data)
		return 0;
	char szData[4096] = {0};
	strcpy(szData, data);
	char *pch = strtok(szData, ";\n,\t ");
	if (pch != NULL)
	{
		g_nEnableLogRs485 = atoi(pch);
		pch = strtok(NULL, ";\n,\t");
		if (pch != NULL)
		{
			g_nEnableLogRs232 = atoi(pch);
			return 1;
		}
	}
	return 0;
}

//获取网络参数配置
int getIpSetParam(const char *data, char *pcIp, char *pcMask, char *pcGateWay, int *nEnableSet)
{
	if (NULL == data)
		return 0;
	char szData[4096] = {0};
	strcpy(szData, data);
	char *pch = strtok(szData, ";\n,\t ");
	if (pch != NULL)
	{
		*nEnableSet = atoi(pch);
		pch = strtok(NULL, ";\n,\t");
		if (pch != NULL)
		{
			strcpy(pcIp, pch);
			pch = strtok(NULL, ";\n,\t");
			if (pch != NULL)
			{
				strcpy(pcMask, pch);
				pch = strtok(NULL, ";\n,\t");
				if (pch != NULL)
				{
					strcpy(pcGateWay, pch);
					return 1;
				}
			}
		}
	}
	return 0;
}

//发送RS485串口数据线程
void thread_port()
{
	static long nPortSendNum = 0;
	while(g_run)
	{
		char sendData[128];
		memset(sendData, 0, sizeof(sendData));
		strcpy(sendData, "Send rs485 port data to camera.");
		int success = ICE_IPCSDK_TransSerialPort(hSDK, sendData, strlen(sendData)+1);
		if(0 != success)
			printf("%ld: rs485 port send\n", ++nPortSendNum);
		else
			printf("rs485 port send failed\n");
		usleep(g_nTransPortTime*1000);
	}
}

//发送RS232串口数据线程
void thread_port_rs232()
{
	static long nPortSendNum_rs232 = 0;
	while(g_run)
	{
		char sendData[128];
		memset(sendData, 0, sizeof(sendData));
		strcpy(sendData, "Send rs232 port data to camera.");
		int success = ICE_IPCSDK_TransSerialPort_RS232(hSDK, sendData, strlen(sendData)+1);
		if(0 != success)
			printf("%ld: rs232 port send success\n", ++nPortSendNum_rs232);
		else
			printf("rs232 port send failed\n");
		usleep(g_nTransPortTime_rs232*1000);
	}
}

//手动抓拍线程
void thread_capture()
{
	char pdata[1048576];
	static int nOnlyCapture = 0;
	int success = 0;
	long len = 0;
	while(g_run)
	{
		memset(pdata, 0, sizeof(pdata));
		success = ICE_IPCSDK_Capture(hSDK, (void*)pdata, 1048576, &len);
		if (success)
		{
			savePic(g_ip,"Capture",pdata, len, 0, 0, NULL);
			printf("Capture：%d\n", ++nOnlyCapture);
			usleep(g_nCapture*1000);
		}
	}

}

int main(int argc, char* argv[])
{
	printf("TEEST1234");
	if(argc < 3)
	{
		printf("请输入正确的参数!\n");
		return -1;
	}
	
	int mode = 1;

	char *passwd = (char*)malloc(64 * sizeof(char));
	if(NULL == passwd)
		return 0;

	memset(passwd, 0, 64 * sizeof(char));
	

	if(0 == strcmp("0", argv[2]))
	{
		mode = 0;
	}
	else if(0 == strcmp("1", argv[2]))
	{
		if(argc <= 3)
		{
			printf("请输入密码");
			free(passwd);
			return 0;
		}
		memcpy(passwd, argv[3], strlen(argv[3]));
		mode = 1;
	}
	else 
	{
		free(passwd);
		printf("请输入正确的参数!\n");
		return -1;
	}

    static int nCamStatus = OFFLINE;
    int nCamCurrentStatus = OFFLINE;

    int res = 0;
   // pthread_t a_thread;
    struct itimerval tick;

    char number[32];
    char color[32];
    long len = 0;
    int success = 0;
    static int nTriggerNum = 0;    
    char pdata[1048576];

	char tmpNum[32] = {'\0'};
	char tmpColor[32] = {'\0'};

    int nTriggerTime = 0;
    char filePath[2048] = {'\0'};

	char bstrCustomVideo[6*MAX_OSD_TEXT];
	char bstrCustomJpeg[6*MAX_OSD_TEXT];
	int line = 0;
	int nstrLen = 0;
	memset(bstrCustomVideo, 0, sizeof(bstrCustomVideo));
	memset(bstrCustomJpeg, 0, sizeof(bstrCustomJpeg));
	long lOsdParam[10] = {0};
	long tmpPos;
	int nParamPos = 0;
	long colorVideo = 0;
	long colorJpeg = 0;

	long loopParam[9] = {0};

	pthread_t id;
	pthread_t id_rs232;
	pthread_t id_opengate;
	pthread_t id_capture;
	int ret = -1;
	int ret_rs232 = -1;
	int ret_opengate = -1;
	int ret_capture = -1;
	char szLoop[128];
	memset(szLoop, 0, sizeof(szLoop));
	char szSearchDev[128];
	memset(szSearchDev, 0, sizeof(szSearchDev));
	char szLog[32];
	memset(szLog, 0, sizeof(szLog));

	int nGetFlag = 0;
    ICE_OSDAttr_S pstOSDAttr;

    FILE* fp;
    // memset(cIp, 0, sizeof(cIp));
  
//	if (2 != argc)
//		return 0;

	if (is_valid_ip(argv[1], 0) != 1)
	{
		printf("Ip is invalid!\n" );
		free(passwd);
		return 0;
	}
		

	 memcpy(g_ip, argv[1], strlen(argv[1]));
	
#ifdef _WIN32
	SetConsoleCtrlHandler(handle_sig, TRUE);
#else
	signal(SIGINT, handle_sig);
#endif	
	ICE_IPCSDK_SetDeviceEventCallBack(OnDeviceEvent, NULL, 0);//è®¾ç½®ç›¸æœºäº‹ä»¶å›žè°ƒ

	ICE_IPCSDK_SetStateEventCallBack(OnStateEvent, NULL, 0);//test  offline
	/*****************************èŽ·å–é…ç½®******************************/
	memset(filePath, 0, sizeof(filePath));
	fp = fopen("./fileConfig", "r");
	if (NULL != fp)
	{
		fread(filePath, sizeof(char), sizeof(filePath), fp);
		char *s = strtok(filePath, "\n");
		s = strtok(NULL, "\n");
		g_nOpenGateTime = atoi(s);
		s = strtok(NULL, "\n");
		nTriggerTime = atoi(s);

		s = strtok(NULL, "\n");
		if (strcmp(s, "[capture]") == 0)
		{
			s = strtok(NULL, "\n");
			g_nCapture = atoi(s);
		}

		s = strtok(NULL, "\n");
		if (strcmp(s, "[rs485]") == 0)
		{
			s = strtok(NULL, "\n");
			g_nTransPortTime = atoi(s);
		}		

		s = strtok(NULL, "\n");
		if (strcmp(s, "[rs232]") == 0)
		{
			s = strtok(NULL, "\n");
			g_nTransPortTime_rs232 = atoi(s);
		}

		s = strtok(NULL, "\n");
		if (strcmp(s, "[loop]") == 0)
		{
			s = strtok(NULL, "\n");
			strcpy(szLoop, s);
		}
		//printf("____%s___\n", szLoop);

		s = strtok(NULL, "\n");
		if (strcmp(s, "[portLog]") == 0)
		{
			s = strtok(NULL, "\n");
			strcpy(szLog, s);
		}

		s = strtok(NULL, "\n");
		while(strcmp(s, "[customVideoBuf]") != 0)
		{
			s = strtok(NULL, "\n");
			if(0 == nParamPos)
				tmpPos =  atol(s);
			lOsdParam[nParamPos] = atol(s);
			s = strtok(NULL, "\n");
			nParamPos++;
		}

		char pVideo[512]= {0};
		if (strcmp(s, "[customVideoBuf]") == 0)
		{
			s = strtok(NULL, "\n");
			if (NULL != s){
				while((strcmp(s, "[customJpegBuf]") != 0) && line<4)
				{	
				    strcat(pVideo, s);
					if(line != 3)
						strcat(pVideo, "\r\n");
					s = strtok(NULL, "\n");
					line++;
					if (NULL == s){
						break;
					}else if (NULL != s){
						if(strcmp(s,"[customJpegBuf]") == 0)
							break;
					}
				}
			}
		}
		strcpy(bstrCustomVideo, pVideo);
		printf("----input----bstrCustomVideo = \n%s\n", bstrCustomVideo);
		
		if (NULL != s){
			while(strcmp(s,"[customJpegBuf]") !=0 )
			{
				s = strtok(NULL, "\n");
			}

			
			char pJpeg[512]= {0};
			if (strcmp(s,"[customJpegBuf]") == 0)
			{
				line = 0;
				s = strtok(NULL, "\n");
				while((s!=NULL) && line<MAX_MSTAR_OSD_LINE)
				{
				    strcat(pJpeg, s);
				    if(line != 3)
						strcat(pJpeg, "\r\n");
	
					s = strtok(NULL, "\n");
					line++;
					if (NULL == s){
						break;
					}
				}
			}
			strcpy(bstrCustomJpeg, pJpeg);
		}
		fclose(fp);
	}
	printf("----input----bstrCustomJpeg = \n%s\n", bstrCustomJpeg);

	/*****************************获取配置结束******************************/
	getPortLog(szLog);//是否记录接收到串口数据日志

	//连接相机
	if(mode == 0)
	{
		hSDK = ICE_IPCSDK_Open(argv[1], OnPlate, NULL);
	}
	else
	{
		hSDK = ICE_IPCSDK_Open_Passwd(argv[1], passwd, OnPlate, NULL);
	}
	
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		goto exit;
	}
	//调用配置抓拍规则函数，第二个参数传入1表示抓拍全景图, 第二个参数传入1表示有车牌图
	ICE_ICPSDK_SetSnapCfg(hSDK, 1, 1);

	/**********************************时间同步****************************************/
	struct tm *timenow;

	time_t now;	
	time(&now);
	timenow = localtime(&now);

	int success1 = ICE_IPCSDK_SyncTimeExt(hSDK, (short)timenow->tm_year+1900, timenow->tm_mon+1, timenow->tm_mday, 
		timenow->tm_hour, timenow->tm_min, timenow->tm_sec);
	if (1 == success1)
		printf("时间同步成功\n");
	else
		printf("时间同步失败\n");
	/**********************************时间同步结束****************************************/

	/**********************************设置osd开始***************************************/	

	colorVideo = getColor(lOsdParam[1]);
	colorJpeg = getColor(lOsdParam[6]);
    memset(&pstOSDAttr, 0, sizeof(pstOSDAttr));
    ICE_IPCSDK_GetOSDCfg(hSDK, &pstOSDAttr);
	lOsdParam[0] = tmpPos;
	printf("__________pos=%d_______________\n",lOsdParam[0]);
	pstOSDAttr.u32OSDLocationVideo = lOsdParam[0];
	pstOSDAttr.u32ColorVideo = colorVideo;
	pstOSDAttr.u32DateVideo = lOsdParam[2];
	pstOSDAttr.u32License = lOsdParam[3];
	pstOSDAttr.u32CustomVideo = lOsdParam[4];
	strcpy(pstOSDAttr.szCustomVideo6, bstrCustomVideo);
	printf("___OsdInfo=pos=%d,color=%d,date=%d,\n infoVideo=%s____\n",lOsdParam[0],colorVideo,lOsdParam[2],&bstrCustomVideo[0]);
	pstOSDAttr.u32OSDLocationJpeg = lOsdParam[5];
	pstOSDAttr.u32ColorJpeg = colorJpeg;
	pstOSDAttr.u32DateJpeg = lOsdParam[7];
	pstOSDAttr.u32Algo = lOsdParam[8];
	pstOSDAttr.u32CustomJpeg = lOsdParam[9];
	strcpy(pstOSDAttr.szCustomJpeg6, bstrCustomJpeg);
	printf("___OsdInfo=pos=%d,color=%d,date=%d,\ninfoJpg=%s____\n",lOsdParam[5],colorJpeg,lOsdParam[7],&bstrCustomJpeg[0]);

	//int setosd = ICE_IPCSDK_SetOsdCfg(hSDK, lOsdParam[0],colorVideo,lOsdParam[2],lOsdParam[3],lOsdParam[4],&bstrCustomVideo[0],lOsdParam[5],
		//colorJpeg,lOsdParam[7],lOsdParam[8],lOsdParam[9],&bstrCustomJpeg[0]);//设置osd叠加
	
	int setosd = ICE_IPCSDK_SetOsdCfg_new(hSDK, &pstOSDAttr);
	if (setosd == 1)
		printf("setosd : success__\n");
	else
		printf("setosd failed\n");
	
	/**********************************设置osd结束***************************************/	


	ICE_IPCSDK_SetSerialPortCallBack(hSDK, OnSerialPort, NULL);//设置RS485串口数据回调
	ICE_IPCSDK_SetSerialPortCallBack_RS232(hSDK, OnSerialPort_rs232, NULL);//设置RS232串口数据回调
	//设置断网续传回调
	ICE_IPCSDK_SetPastPlateCallBack(hSDK, OnPastPlate, NULL);

	printf("Time:%d, %d\r\n", g_nOpenGateTime, nTriggerTime);

	//以下线程用于测试相机及sdk稳定性而设计的
	if (g_nTransPortTime > 0)//如果定时发送RS485串口数据时间>0，则创建线程
	{
		ret = pthread_create(&id, NULL, (void*)thread_port, NULL);
		if (ret != 0)
		{
			printf("Create thread_port_rs485 error\n");
		}
	}

	if (g_nTransPortTime_rs232 > 0)//如果定时发送RS232串口数据时间>0，则创建线程
	{
		ret_rs232 = pthread_create(&id_rs232, NULL, (void*)thread_port_rs232, NULL);
		if (ret_rs232 != 0)
		{
			printf("Create thread_port_rs232 error\n");
		}
	}

  	if (g_nOpenGateTime > 0)//如果定时打开道闸时间>0，则创建线程
	{
		ret_opengate = pthread_create(&id_opengate, NULL, (void*)openGate, NULL);
		if (ret_opengate != 0)
		{
			printf("Create thread_opengate error\n");
		}
	}

	if(g_nCapture > 0)//如果定时手动触发时间>0，则创建线程
	{
		ret_capture = pthread_create(&id_capture, NULL, (void*)thread_capture, NULL);
		if(ret_capture != 0)
			printf("Create thread_capture error\n");
	}	
   
	while (g_run)
	{
		//调用ICE_IPCSDK_GetStatus获取相机状态
			    
		nCamCurrentStatus = ICE_IPCSDK_GetStatus(hSDK);

		// printf("status:%d\n", nCamCurrentStatus);
		if (nCamCurrentStatus != nCamStatus)
		{
			nCamStatus = nCamCurrentStatus;
			if (OFFLINE == nCamStatus)
				printf("offline\r\n");
			else
			{
				char devID[32];
				char sendData[128];
				char recvData[128];
				memset(devID, 0, sizeof(devID));
				memset(sendData, 0, sizeof(sendData));
				memset(recvData, 0, sizeof(recvData));

				ICE_IPCSDK_GetDevID(hSDK, devID);
				printf("online, mac addr:%s\r\n", devID);
				
				sprintf(sendData, "Camera send and recv data,ip:%s,mac:%s", argv[1], devID);
				ICE_IPCSDK_WriteUserData(hSDK, sendData);
				// ICE_IPCSDK_ReadUserData(hSDK, recvData);
				printf("%s\n", recvData);

				//ICE_IPCSDK_TransSerialPort(hSDK, sendData, strlen(sendData));
			}		
		}

		if (nTriggerTime != 0)//软触发
		{
			memset(pdata, 0, sizeof(pdata));
			memset(number, 0, sizeof(number));
			memset(color, 0, sizeof(color));
			//调用软触发接口
			success = ICE_IPCSDK_Trigger(hSDK, number, color, (void*)pdata, 1048576, &len);
			if (success)
			{
				#ifdef COMPILEUTF8
				/*转换编码，若不需要，则不调用*/
				g2u(number, strlen(number), tmpNum, 32);
				g2u(color, strlen(color), tmpColor, 32);
				savePic(argv[1], tmpNum, pdata, len, 2, 0, NULL);
				#else
				savePic(argv[1], number, pdata, len, 2, 0, NULL);
				#endif

				// printf("sdk Trigger %d:%s  %s\n", ++nTriggerNum, tmpNum, tmpColor);
				printf("sdk Trigger : %d\n", ++nTriggerNum);
			}

			//只做触发，不做识别
			// success = ICE_IPCSDK_TriggerByIp(argv[1]);
			// if (success)
			// {
			// 	printf("success Trigger: %d\r\n", ++nTriggerNum);
			// }

			// memset(pdata, 0, sizeof(pdata));
			// success = ICE_IPCSDK_Capture(hSDK, (void*)pdata, 1048576, &len);
			// if (success)
			// {
			// 	savePic(argv[1],"Capture",pdata, len, 0);
			// 	printf("Capture：%d\n", ++nOnlyCapture);
			// }
			usleep(nTriggerTime*1000);
		}
		
		else
		{
			struct timeval delay;
			delay.tv_sec = 0;
			delay.tv_usec = 1000 * 1000; 
			select(0, NULL, NULL, NULL, &delay);
		}
		
	}
	
exit:
	
	if (0 == ret)
		pthread_join(id, NULL);
	if (0 == ret_rs232)
		pthread_join(id_rs232, NULL);
	if (0 == ret_opengate)
		pthread_join(id_opengate, NULL);
	if (0 == ret_capture)
		pthread_join(id_capture, NULL);
	if (NULL != hSDK)
	{
		//断开相机
		ICE_IPCSDK_Close(hSDK);
		hSDK = NULL;
	}
	
	free(passwd);

	return 0;
}



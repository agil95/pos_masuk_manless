#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <time.h>
#include <string.h>

#include "ice_ipcsdk.h"
#include "ice_com_type.h"
#include "ice_vlpr_result.h"
#include <stdbool.h>
#include <unistd.h>
#include "config.h"

static void WhiteListFindItem_Test(char *ip, char *plate);
static void WhiteListGetAllItem_Test(char *ip, int type);
static int listDataCheck(char* pcDateBegin, char* pcDateEnd, 
	char* pcTimeBegin, char* pcTimeEnd, char *pcType);
static void syncTime(char *ip);
static void controlAlarmOut(char *ip, int nIndex);
static void modifyDevIP(char *ip,  char* szEthName);
static bool is_str_utf8(const char* str);

static void GetUARTCfg(char *ip);
static void SetUARTCfg(char *ip);

static void SetTriggerMode(char *ip, int nTriggerMode);
static void GetTriggerMode(char *ip);

static void ModifyEncPwd(char *ip, char *pcOldPwd, char *pcNewPwd);
static void EnableEnc(char *ip, int nEncId, char *pcPwd);

static void GetIPAddr(char *ip);
static void GetTime(char *pcIp);

static void SetAlarmOutConfig(char *ip, int nIndex, int nIdleState, int nDelayTime);
static void GetAlarmOutConfig(char *ip, int nIndex);

static void SetIPAddr(char *ip, char *pcDstIp, char *pcDstMask, char *pcDstGateway);

static int inputUartParam(int *nResult, int nMin, int nMax);

static void GetUID(const char *ip);
static void GetDevID(const char *ip);

static void WriteUserData(const char *ip);
static void ReadUserData(const char *ip);

static void SetTransPortData1(const char *ip);
static void SetTransPortData(const char *ip);

static void GetNewWhiteListParam(const char *ip);
static void SetNewWhiteListParam(const char *ip);



int main(int argc, char *argv[])
{
	if (argc < 2)
		return 0;
	
	if (0 == strcmp(argv[1], "ICE_IPCSDK_SearchDev"))
	{
		if (argc < 4)
		{
			printf("Parameter error");
			return 0;
		}
		char szDevs[4096];
		
		ICE_IPCSDK_SearchDev(argv[2], szDevs, 4096, atoi(argv[3]));
		printf("%s\n", szDevs);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_DefaultParam"))
	{
		if (argc < 3)
			return 0;
		
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;
		
		if (1 == ICE_IPCSDK_DefaultParam(hSDK))
			printf("success\n");
		else
			printf("fail\n");
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetIOState"))
	{
		if (argc < 3)
			return 0;
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;

		int i = 0;
		int success = 0;
		long nIOState = 0;
		for (; i < 4; i++)
		{
			success = ICE_IPCSDK_GetIOState(hSDK, i, &nIOState, 0, 0);
			if (success == 1)
				printf("IO%d status:%ld  ", i+1, nIOState);
			else
				printf("IO%d get status failed!", i+1);
		}
		printf("\n");
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetNewWhiteListParam"))
	{
		if (argc < 3)
			return 0;
		GetNewWhiteListParam(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_SetNewWhiteListParam"))
	{
		if (argc < 3)
			return 0;
		SetNewWhiteListParam(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WhiteListEditItem"))
	{
		if (argc < 9)
		{
			printf("Parameter error");
			return 0;
		}
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;

		if (0 == listDataCheck(argv[4], argv[5], argv[6], argv[7], argv[8]))
		{
			printf("Edit list:%s failed\n", argv[3]);
			return 0;
		}	
		
		char gbPlate[128];
		char gbRemark[128];
		memset(gbPlate, 0, sizeof(gbPlate));
		memset(gbRemark, 0, sizeof(gbRemark));

		memcpy(gbPlate, argv[3], strlen(argv[3]));
		if (argc > 9)
			memcpy(gbRemark, argv[9], strlen(argv[9]) > 128 ? 128 : strlen(argv[9]));

		if (1 == ICE_IPCSDK_WhiteListEditItem_ByNumber(hSDK, gbPlate, argv[4], 
			argv[5], argv[6], argv[7], argv[8], gbRemark, 0, 0, 0))
			printf("Edit list:%s successfullt\n", argv[3]);
		else
			printf("Edit list:%s failed\n", argv[3]);
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WhiteListInsertItem"))
	{
		if (argc < 9)
		{
			printf("Parameter error");
			return 0;
		}

		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;

		if (0 == listDataCheck(argv[4], argv[5], argv[6], argv[7], argv[8]))
		{
			printf("Add list:%s failed\n", argv[3]);
			return 0;
		}
		char gbPlate[128];
		char gbRemark[128];
		memset(gbPlate, 0, sizeof(gbPlate));
		memset(gbRemark, 0, sizeof(gbRemark));

		memcpy(gbPlate, argv[3], strlen(argv[3]));
		if (argc > 9)
			memcpy(gbRemark, argv[9], strlen(argv[9]) > 128 ? 128 : strlen(argv[9]));
		
		if (1 == ICE_IPCSDK_WhiteListInsertItem_ByNumber(hSDK, gbPlate, argv[4], 
			argv[5], argv[6], argv[7], argv[8], gbRemark, 0, 0, 0))
			printf("Add list:%s successfully\n", argv[3]);
		else
			printf("Add list:%s failed\n", argv[3]);
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WhiteListInsertCoverItem"))
	{
		if (argc < 9)
		{
			printf("Parameter error");
			return 0;
		}

		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;

		if (0 == listDataCheck(argv[4], argv[5], argv[6], argv[7], argv[8]))
		{
			printf("Add list:%s failed\n", argv[3]);
			return 0;
		}
		char gbPlate[128];
		char gbRemark[128];
		memset(gbPlate, 0, sizeof(gbPlate));
		memset(gbRemark, 0, sizeof(gbRemark));

		memcpy(gbPlate, argv[3], strlen(argv[3]));
		if (argc > 9)
			memcpy(gbRemark, argv[9], strlen(argv[9]) > 128 ? 128 : strlen(argv[9]));
		
		if (1 == ICE_IPCSDK_WhiteListInsertCoverItem_ByNumber(hSDK, gbPlate, argv[4], 
			argv[5], argv[6], argv[7], argv[8], gbRemark, 0, 0, 0))
			printf("Add list:%s successfully\n", argv[3]);
		else
			printf("Add list:%s failed\n", argv[3]);
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WhiteListDeleteItem"))
	{
		if (argc < 4)
			return 0;
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;
		char gbPlate[128];
		memset(gbPlate, 0, sizeof(gbPlate));

		memcpy(gbPlate, argv[3], strlen(argv[3]));

		if (1 == ICE_IPCSDK_WhiteListDeleteItem_ByNumber(hSDK, gbPlate))
			printf("Delete list:%s successfully\n", argv[3]);
		else
			printf("Delete list:%s falied\n", argv[3]);
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WhiteListDelAllItems"))
	{
		if (argc < 3)
			return 0;
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;
		
		if (1 == ICE_IPCSDK_WhiteListDelAllItems_ByNumber(hSDK))
			printf("Delete all black and white lists successfully\n");
		else
			printf("Delete all black and white lists failed\n");
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_DelAllWhiteItems"))
	{
		if (argc < 3)
			return 0;
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;
		
		if (1 == ICE_IPCSDK_DelAllWhiteItems(hSDK))
			printf("Delete all white lists successfully\n");
		else
			printf("Delete all white lists failed\n");
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_DelAllBlackItems"))
	{
		if (argc < 3)
			return 0;
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;
		
		if (1 == ICE_IPCSDK_DelAllBlackItems(hSDK))
			printf("Delete all black lists successfully\n");
		else
			printf("Delete all black lists failed\n");
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WhiteListFindItem"))
	{
		if (argc < 4)
			return 0;
		WhiteListFindItem_Test(argv[2], argv[3]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WhiteListGetCount"))
	{
		if (argc < 3)
			return 0;
		long count = 0;

		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;
		if (1 == ICE_IPCSDK_WhiteListGetCount(hSDK, &count))
			printf("Total number of black and white lists：%ld\n", count);
		else
			printf("Get the total number of black and white lists falied\n");

		if (1 == ICE_IPCSDK_GetWhiteCount(hSDK, &count))
			printf("Total number of white lists：%ld\n", count);
		else
			printf("Get the total number of white lists falied\n");

		if (1 == ICE_IPCSDK_GetBlackCount(hSDK, &count))
			printf("Total number of black lists：%ld\n", count);
		else
			printf("Get the total number of black lists falied\n");

		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WhiteListGetAllItem"))
	{
		if (argc < 3)
			return 0;
		WhiteListGetAllItem_Test(argv[2], 0);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetAllWhiteItem"))
	{
		if (argc < 3)
			return 0;
		WhiteListGetAllItem_Test(argv[2], 1);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetAllBlackItem"))
	{
		if (argc < 3)
			return 0;
		WhiteListGetAllItem_Test(argv[2], 2);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_Broadcast"))
	{
		if (argc < 4)
			return 0;
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;

		if (1 == ICE_IPCSDK_Broadcast(hSDK, atoi(argv[3])))
			printf("Voice broadcast successfully\n");
		else
			printf("Voice broadcast failed\n");
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_Broadcast_ByName"))
	{
		if (argc < 4)
			return 0;
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;

		char *pch = strtok(argv[3], ";\n,\t ");

		if (1 == ICE_IPCSDK_Broadcast_ByName(hSDK, pch))
			printf("Voice broadcast successfully\n");
		else
			printf("Voice broadcast failed\n");
		
		ICE_IPCSDK_Close(hSDK);
	}
	else if(0 == strcmp(argv[1], "ICE_IPCSDK_BroadcastGroup_ByName"))
	{
		if (argc < 3)
			return 0;
		ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(argv[2]);
		if (NULL == hSDK)
			return 0;
		char fileName[1024];
		memset(fileName, 0, sizeof(fileName));
		printf("Enter a voice file name:\n");
		fgets(fileName, sizeof(fileName), stdin);
		if (1 == ICE_IPCSDK_BroadcastGroup_ByName(hSDK, fileName))
			printf("Voice broadcast successfully\n");
		else
			printf("Voice broadcast failed\n");

		ICE_IPCSDK_Close(hSDK);
	}

	else if (0 == strcmp(argv[1], "ICE_IPCSDK_SyncTime"))//sync time
	{
		if (argc < 3)
			return 0;
		syncTime(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_ControlAlarmOut"))
	{
		if (argc < 4)
			return 0;
		controlAlarmOut(argv[2], atoi(argv[3]));
	}
	else if(0 == strcmp(argv[1], "ICE_IPCSDK_GetSDKVersion"))
	{
		if (argc < 2)
			return 0;
		char szSDKVersion[32];
		memset(szSDKVersion, 0, sizeof(szSDKVersion));
		int success = ICE_IPCSDK_GetSDKVersion(szSDKVersion);
		if (1 == success)
			printf("SDK Version: %s\n", szSDKVersion);
		else
			printf("Get sdk version failed!\n");
	}
	else if(0 == strcmp(argv[1], "ICE_IPCSDK_ModifyDevIP"))
	{
		if (argc < 4)
			return 0;
		modifyDevIP(argv[2], argv[3]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetUARTCfg"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		GetUARTCfg(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_SetUARTCfg"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		SetUARTCfg(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_EnableEnc"))
	{
		if (argc < 5)
		{
			printf("Parameter error\n");
			return 0;
		}
		
		if ((strcmp(argv[3], "0") == 0) || (strcmp(argv[3], "1") == 0) )
		{
			EnableEnc(argv[2], atoi(argv[3]), argv[4]);
		}
		else
		{
			printf("Parameter error\n");
		}	
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_ModifyEncPwd"))
	{
		if (argc < 5)
		{
			printf("Parameter error\n");
			return 0;
		}
		ModifyEncPwd(argv[2], argv[3], argv[4]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetTriggerMode"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		GetTriggerMode(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_SetTriggerMode"))
	{
		if (argc < 4)
		{
			printf("Parameter error\n");
			return 0;
		}
		if ((strcmp(argv[3], "0") == 0) || (strcmp(argv[3], "1") == 0) 
			|| (strcmp(argv[3], "2") == 0))
		{
			SetTriggerMode(argv[2], atoi(argv[3]));
		}
		else
		{
			printf("Parameter error\n");
		}	
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetIPAddr"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		GetIPAddr(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetSyncTime"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		GetTime(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetAlarmOutConfig"))
	{
		if (argc < 4)
		{
			printf("Parameter error\n");
			return 0;
		}
		GetAlarmOutConfig(argv[2], atoi(argv[3]));
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_SetAlarmOutConfig"))
	{
		if (argc < 6)
		{
			printf("Parameter error\n");
			return 0;
		}
		SetAlarmOutConfig(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_SetIPAddr"))
	{
		if (argc < 6)
		{
			printf("Parameter error\n");
			return 0;
		}
		SetIPAddr(argv[2], argv[3], argv[4], argv[5]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetUID"))//UID
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		GetUID(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_GetDevID"))//MAC
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		GetDevID(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_WriteUserData"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		WriteUserData(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_ReadUserData"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		ReadUserData(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_TransSerialPort"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		SetTransPortData(argv[2]);
	}
	else if (0 == strcmp(argv[1], "ICE_IPCSDK_TransSerialPort_RS232"))
	{
		if (argc < 3)
		{
			printf("Parameter error\n");
			return 0;
		}
		SetTransPortData1(argv[2]);
	}

	return 0;
}

static int string2hex(const char *src, unsigned char *dst, int dstSize, int *outlen)
{
	if (NULL == src || NULL == dst || 0 == dstSize || NULL == outlen)
		return 0;

	char *p = (char*)src;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	int size = tmplen / 2 > dstSize ? dstSize : tmplen / 2;
	while(cnt < size)
	{
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		dst[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p ++;
		cnt ++;
	}
	if ((tmplen / 2 < dstSize) && (tmplen % 2 != 0))
	{
		dst[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		cnt++;	
	}


	*outlen = cnt;
	return cnt;
}

static void SetNewWhiteListParam(const char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	WhiteList_Param param;
	memset(&param, 0, sizeof(param));
	int ret = ICE_IPCSDK_GetNewWhiteListParam(hSDK, &param);
	if (0 == ret)
	{
		printf("Get failed!\n");
		return;
	}

	if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "whitelistMode", &param.mode))
	{
		printf("Get whitelistMode error\n");
		return;
	}

	if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "blacklistMode", &param.black_mode))
	{
		printf("Get blacklistMode error\n");
		return;
	}

	if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "TempCarMode", &param.temp_mode))
	{
		printf("Get TempCarMode error\n");
		return;
	}

	if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "PoliceCarMode", &param.Jing_mode))
	{
		printf("Get PoliceCarMode error\n");
		return;
	}

	if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "ArmyVehicleMode", &param.Army_mode))
	{
		printf("Get ArmyVehicleMode error\n");
		return;
	}

	int nExact = 0;
	if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "ExactMatch", &nExact))
	{
		printf("Get ExactMatch error\n");
		return;
	}
	
	if (1 == nExact)
	{
		param.allow_unmatch_chars_cnt = 0;
		param.ignoreHZ_flag = 0;
	}
	else
	{
		int nFuzzy = 0;
		if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "FuzzyMatch", &nFuzzy))
		{
			printf("Get FuzzyMatch error\n");
			return;
		}

		if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "AllowUnmatchrs", &param.allow_unmatch_chars_cnt))
		{
			printf("Get AllowUnmatchrs error\n");
			return;
		}

		if (GetConfigIntValue("./testCfg.ini", "WhitelistParams", "ignoreChinese", &param.ignoreHZ_flag))
		{
			printf("Get ignoreChinese error\n");
			return;
		}

		if (0==nFuzzy && 0==param.ignoreHZ_flag)
			param.allow_unmatch_chars_cnt = 0;
	}

	param.new_version = 1;

	ret = ICE_IPCSDK_SetNewWhiteListParam(hSDK, &param);
	if (0 == ret)
	{
		printf("Set failed!\n");
	}
	else
	{
		printf("Set successfully!\n");
	}

}

static void GetNewWhiteListParam(const char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	WhiteList_Param param;
	memset(&param, 0, sizeof(param));

	int ret = ICE_IPCSDK_GetNewWhiteListParam(hSDK, &param);
	if (0 == ret)
	{
		printf("Get failed!\n");
	}
	else
	{
		const char* pcWMode[] = {"Associated opening after disconnection", "Real-time associated opening", "Not associated opening"};
		const char* pcOtherMode[] = {"Not associated opening", "Real-time associated opening"};
		printf("whitelist mode: %s\n", (param.mode >= 0 && param.mode <= 2) ? pcWMode[param.mode] : "Associated opening after disconnection");
		printf("Blacklist mode:%s\n", (param.black_mode >=0 && param.black_mode <= 1) ? pcOtherMode[param.black_mode] : "Not associated opening");
		printf("Temporary car mode::%s\n", (param.temp_mode >=0 && param.temp_mode <= 1) ? pcOtherMode[param.temp_mode] : "Not associated opening");
		printf("Police car mode::%s\n", (param.Jing_mode >=0 && param.Jing_mode <= 1) ? pcOtherMode[param.Jing_mode] : "Not associated opening");
		printf("Army vehicle mode:%s\n", (param.Army_mode >=0 && param.Army_mode <= 1) ? pcOtherMode[param.Army_mode] : "Not associated opening");

		int nFirPrecise = 0;
		int nSecNormal = 0;
		int nThrIgnore = 0;
		int nAllowNumber = 1;

		if (0 == param.allow_unmatch_chars_cnt)
			nSecNormal = 0;
		else
			nSecNormal = 1;
		nThrIgnore = param.ignoreHZ_flag;

		if (0 == param.allow_unmatch_chars_cnt)
			nAllowNumber = 1;
		else
			nAllowNumber = param.allow_unmatch_chars_cnt;

		if (0 == nSecNormal && 0 == nThrIgnore)
		{
			printf("Exact match\n");
		}
		else
		{
			printf("Fuzzy matching of ordinary characters (letters, numbers):%s\n", (0 == nSecNormal) ? "disable" : "enable");
			printf("The number of allowed characters does not match:%d\n", nAllowNumber);
			printf("Ignore Chinese characters:%s\n", (0 == nThrIgnore) ? "disable" : "enable");
		}
	}
}


static void SetTransPortData1(const char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	char data[4096] = {0}, portData[4096] = {0};
	
	if (GetConfigStringValue("./testCfg.ini", "RS485-1", "data", data))
	{
		printf("Get fileName parameters error\n");
		return;
	}

	int len = 0;
	string2hex(data, (unsigned char*)portData, sizeof(portData), &len);
	int ret = ICE_IPCSDK_TransSerialPort_RS232(hSDK, portData, len);
	if (0 == ret)
	{
		printf("Set RS485-1 data failed!\n");
	}
	else
	{
		printf("Set RS485-1 data successfully!\n");
	}
}


static void SetTransPortData(const char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	char data[4096] = {0}, portData[4096] = {0};
	
	if (GetConfigStringValue("./testCfg.ini", "RS485", "data", data))
	{
		printf("Get fileName parameters error\n");
		return;
	}

	int len = 0;
	string2hex(data, (unsigned char*)portData, sizeof(portData), &len);
	int ret = ICE_IPCSDK_TransSerialPort(hSDK, portData, len);
	if (0 == ret)
	{
		printf("Set RS485 data failed!\n");
	}
	else
	{
		printf("Set RS485 data successfully!\n");
	}
}

static void WriteUserData(const char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	char data[4096] = {0}, userData[4096] = {0};
	
	if (GetConfigStringValue("./testCfg.ini", "UserData", "data", data))
	{
		printf("Get fileName parameters error\n");
		return;
	}

	int len = 0;
	string2hex(data, (unsigned char*)userData, sizeof(userData), &len);
	int ret = ICE_IPCSDK_WriteUserData(hSDK, userData);
	if (0 == ret)
	{
		printf("Write user data failed!\n");
	}
	else
	{
		printf("Write user data successfully!\n");
	}
}


static void ReadUserData(const char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}

	char data[4096] = {0};
	int len = 0;
	int ret = ICE_IPCSDK_ReadUserData(hSDK, data, sizeof(data), &len);
	if (0 == ret)
	{
		printf("Read user data failed!\n");
	}
	else
	{
		int i = 0;
		for (; i < len; i++)
		{
			printf("%02x ", (unsigned char)data[i]);
		}
		printf("\n");
	}

}

static void GetDevID(const char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	
	char szDevID[64] = {0};
	int ret = ICE_IPCSDK_GetDevID(hSDK, szDevID);
	if (1 != ret)
	{
		printf("Get failed!\n");
	}
	else
	{
		printf("MAC:%s\n", szDevID);
	}
}


static void GetUID(const char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	
	UID_PARAM param;
	memset(&param, 0, sizeof(UID_PARAM));
	int ret = ICE_IPCSDK_GetUID(hSDK, &param);
	if (1 != ret)
	{
		printf("Get failed!\n");
	}
	else
	{
		printf("UID:%s\n", param.ucUID);
	}
}

int is_valid_ip(const char *ip, int index) 
{ 
    int section = 0; 
    int dot = 0; 
    int last = -1; 
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


static void SetIPAddr(char *ip, char *pcDstIp, char *pcDstMask, char *pcDstGateway)
{
	if((is_valid_ip(pcDstIp, 0) != 1) || (is_valid_ip(pcDstMask, 1) != 1) || (is_valid_ip(pcDstGateway, 1) != 1))
	{
		printf("Parameter is invalid\n");
		return;
	}
	
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}	

	int ret = ICE_IPCSDK_SetIPAddr(hSDK, pcDstIp, pcDstMask, pcDstGateway);
	if (0 == ret)
		printf("Set failed\n");
	else
		printf("Set successfully\n");
		
	ICE_IPCSDK_Close(hSDK);
}

static void SetAlarmOutConfig(char *ip, int nIndex, int nIdleState, int nDelayTime)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	printf("Parameters are:IO:%d, status:%d, delay time:%d\n", (nIndex+1), nIdleState, nDelayTime);

	int ret = ICE_IPCSDK_SetAlarmOutConfig(hSDK, nIndex, nIdleState, nDelayTime, 0);
	if (0 == ret)
		printf("Set failed\n");
	else
		printf("Set successfully\n");
		
	ICE_IPCSDK_Close(hSDK);
}

static void GetAlarmOutConfig(char *ip, int nIndex)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	unsigned int nIdleState = 0;
	unsigned int nDelayTime = 0;
	unsigned int nResv = 0;
	int ret = ICE_IPCSDK_GetAlarmOutConfig(hSDK, nIndex, &nIdleState, &nDelayTime, &nResv);
	if (0 == ret)
		printf("Get failed\n");
	else
		printf("IO:%d，status:%s, delay time:%d\n", (nIndex+1), nIdleState==0 ? "Normally open":"Normally closed", nDelayTime);
		
	ICE_IPCSDK_Close(hSDK);
}

static void GetTime(char *pcIp)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(pcIp);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	struct tm timenow;
	
	int ret = ICE_IPCSDK_GetSyncTime(hSDK, &timenow);
	if (0 == ret)
		printf("Get failed\n");
	else
		printf("Camera time:%04d-%02d-%02d %02d:%02d:%02d\n", timenow.tm_year, timenow.tm_mon, timenow.tm_mday, 
		timenow.tm_hour, timenow.tm_min, timenow.tm_sec);
	
	ICE_IPCSDK_Close(hSDK);
}

static void GetIPAddr(char *pcIp)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(pcIp);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	char ip[32], mask[32], gateway[32];
	memset(ip, 0, sizeof(ip));
	memset(mask, 0, sizeof(ip));
	memset(gateway, 0, sizeof(gateway));
	
	int ret = ICE_IPCSDK_GetIPAddr(hSDK, ip, mask, gateway);
	if (0 == ret)
		printf("Get failed\n");
	else
		printf("Camera ip:%s, mask:%s, gateway:%s\n", ip, mask, gateway);
	
	ICE_IPCSDK_Close(hSDK);
}

static void ModifyEncPwd(char *ip, char *pcOldPwd, char *pcNewPwd)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	int nRet = ICE_IPCSDK_ModifyEncPwd(hSDK, pcOldPwd, pcNewPwd);
	
	if (1 != nRet)
		printf("Set failed\n");
	else
	{
		printf("Set successfully\n");
	}
	

	ICE_IPCSDK_Close(hSDK);
}

static void EnableEnc(char *ip, int nEncId, char *pcPwd)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	int nRet = ICE_IPCSDK_EnableEnc(hSDK, nEncId, pcPwd);
	
	if (1 != nRet)
		printf("Set failed\n");
	else
	{
		printf("Set successfully\n");
	}
	

	ICE_IPCSDK_Close(hSDK);
}

static void SetTriggerMode(char *ip, int nTriggerMode)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	int nRet = ICE_IPCSDK_SetTriggerMode(hSDK, nTriggerMode);
	
	if (1 != nRet)
		printf("Set failed\n");
	else
	{
		printf("Set successfully\n");
	}
	

	ICE_IPCSDK_Close(hSDK);
}

static void GetTriggerMode(char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}
	unsigned int nTriggerMode = 0;
	int nRet = ICE_IPCSDK_GetTriggerMode(hSDK, &nTriggerMode);
	
	if (1 != nRet)
		printf("Get failed\n");
	else
	{
		printf("Trigger mode:");
		if (1 == nTriggerMode)
			printf("video\n");
		else if (2 == nTriggerMode)
			printf("mix\n");
		else
			printf("IO coil\n");
	}
	

	ICE_IPCSDK_Close(hSDK);
}

static int getInput(int *cInput)
{
	char ch;
	int i = 0;
	while((ch = getchar()) != '\n')
	{
		if (0 == i)
			*cInput = ch - '0';
		i++;
	}

	return (1==i);
}

static int inputUartParam(int *nResult, int nMin, int nMax)
{
	int nInput = 0;
	int nFlag = 0;
	int nFin = 0;
	int i = 0;
	do{
		printf("please input:");
		if (0 == getInput(&nInput))
		{
			printf("The input value is not within the range, enter 1 to continue, and enter other numbers to exit the setting:");
			if (0 == getInput(&nFlag))
				break;
			continue;
		}

		if ((nInput < nMin) || (nInput > nMax))
		{
			printf("The input value is not within the range, enter 1 to continue, and enter other numbers to exit the setting:");
			if (0 == getInput(&nFlag))
				nFlag = 0;
		}
		else 
		{
			nFin = 1;
			*nResult = nInput;
			nFlag = 0;
		}
		
	}while(1 == nFlag);

	return nFin;
}

int inputUart(ICE_UART_PARAM *stParam, int pos)
{

		printf("RS485-%d\n", pos+1);
		printf("Serial port enabl, 0 means not enabled, and 1 means enabled ");
		if (0 == inputUartParam(&stParam->uart_param[pos].uartEn, 0, 1))
		return 0;
		
		printf("Serial port working mode: 1 indicates screen display control, 2 indicates transparent transmission control ");
		int screen_mode = 0;
		if (0 == inputUartParam(&screen_mode, 1, 2))
			return 0;
		stParam->uart_param[pos].screen_mode = screen_mode;

		printf("Baud rate: 0 means 1200, 1 means 2400, 2 means 4800, 3 means 9600, 4 means 19200, 5 means 38400, 6 means 115200");
		if (0 == inputUartParam(&stParam->uart_param[pos].baudRate, 0, 6))
			return 0;

		printf("Check bit: 0 indicates none, 1 indicates odd check, 2 indicates even check, 3 indicates mark, and 4 indicates space ");
		if (0 == inputUartParam(&stParam->uart_param[pos].parity, 0, 4))
			return 0;

		printf("Stop bits,0 means 1,1 means 2 ");
		if (0 == inputUartParam(&stParam->uart_param[pos].stopBits, 0, 1))
			return 0;

		printf("Flow control mode, 0 means none, 1 means hardware, 2 means Xon, 3 means XOFF ");
		if (0 == inputUartParam(&stParam->uart_param[pos].flowControl, 0, 3))
			return 0;
	
		printf("Number of retransmissions: 0-3 ");
		if (0 == inputUartParam(&stParam->uart_param[pos].u32UartProcOneReSendCnt, 0, 3))
			return 0;		

	return 1;
}

static void SetUARTCfg(char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}

	int ret = 0, i=0;

	ICE_UART_PARAM stParam;
	memset(&stParam, 0, sizeof(ICE_UART_PARAM));
	if (0 == ICE_IPCSDK_GetUARTCfg(hSDK, &stParam))
	{
		printf("Get serial port parameters failed\n");
		ICE_IPCSDK_Close(hSDK);
		return;
	}

	int nUartNum = 0;
	printf("%s","Prompt: 0 means to set rs485-1 serial port, 1 means to set rs485-2 serial port, and 2 means to set the two together");
	if (0 == inputUartParam(&nUartNum, 0, 2))
		return;

	int nSet = 0;
	
	if ((0 == nUartNum) || (2 == nUartNum))
	{
		if ( 1 == inputUart(&stParam, 0))
			nSet = 1;
		else
		{
			nSet = 0;
			ICE_IPCSDK_Close(hSDK);
			return;
		}
	}

	if ((1 == nUartNum) || (2 == nUartNum))
	{
		if ( 1 == inputUart(&stParam, 1))
			nSet = 1;
		else 
			nSet = 0;
	}
		
	if (1 == nSet)
	{
		if (0 == ICE_IPCSDK_SetUARTCfg(hSDK, &stParam))
		{
			printf("Set serial port parameters failed\n");
		}
		else
			printf("Set serial port parameters successfully\n");
	}
	

	ICE_IPCSDK_Close(hSDK);
}

static void GetUARTCfg(char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
	{
		printf("connect failed!\n");
		return;
	}

	int ret = 0, i=0;

	ICE_UART_PARAM stParam;
	memset(&stParam, 0, sizeof(ICE_UART_PARAM));
	if (0 == ICE_IPCSDK_GetUARTCfg(hSDK, &stParam))
	{
		printf("Get serial port parameters failed\n");
		ICE_IPCSDK_Close(hSDK);
		return;
	}

	int nShowLED = 0;

	int nBaudRate[] = {1200, 2400, 4800, 9600, 19200, 38400, 115200};
	char* strParity[] = {"none", "odd", "even", "mark", "space"};
	char* strFlowControl[] = {"none", "hardware", "Xon", "Xoff"};

	for(i=0; i<2; i++)
	{
		if ((stParam.uart_param[i].baudRate < 0) || (stParam.uart_param[i].baudRate > 6))
			stParam.uart_param[i].baudRate = 0;
		if ((stParam.uart_param[i].parity < 0) || (stParam.uart_param[i].parity > 4))
			stParam.uart_param[i].parity = 0;
		if ((stParam.uart_param[i].flowControl < 0) || (stParam.uart_param[i].flowControl > 3))
			stParam.uart_param[i].flowControl = 0;

		printf("RS485-%d:\n", i+1);
		printf("serial port enable: %s\n", stParam.uart_param[i].uartEn == 0 ? "disable" : "enable");
		printf("work mode: %s\n", stParam.uart_param[i].screen_mode == 2 ? "Transparent transmission control" : "Screen display control");
		printf("baudrate: %d\n", nBaudRate[stParam.uart_param[i].baudRate]);
		printf("databits: 8\n");
		printf("parity: %s\n", strParity[stParam.uart_param[i].parity]);
		printf("stopbits：%s\n", stParam.uart_param[i].stopBits == 1 ? "2" : "1");
		printf("flowControl：%s\n", strFlowControl[stParam.uart_param[i].flowControl]);
		printf("Number of retransmissions: %d\n", stParam.uart_param[i].u32UartProcOneReSendCnt);		
	}

	ICE_IPCSDK_Close(hSDK);
}

static bool is_str_utf8(const char* str)
{
	unsigned int nBytes = 0;
	unsigned char chr = *str;
	bool bAllAscii = true;
	unsigned int i = 0;
 
	for (i = 0; str[i] != '\0'; ++i){
		chr = *(str + i);
		
		if (nBytes == 0 && (chr & 0x80) != 0){
			bAllAscii = false;
		}
 
		if (nBytes == 0) {
			
			if (chr >= 0x80) {
 
				if (chr >= 0xFC && chr <= 0xFD){
					nBytes = 6;
				}
				else if (chr >= 0xF8){
					nBytes = 5;
				}
				else if (chr >= 0xF0){
					nBytes = 4;
				}
				else if (chr >= 0xE0){
					nBytes = 3;
				}
				else if (chr >= 0xC0){
					nBytes = 2;
				}
				else{
					return false;
				}
 
				nBytes--;
			}
		}
		else{
			
			if ((chr & 0xC0) != 0x80){
				return false;
			}
			
			nBytes--;
		}
	}
 
	
	if (nBytes != 0)  {
		return false;
	}
 
	if (bAllAscii){ 
		return true;
	}
 
	return true;
}


int lastIndexOf(char *str1,char *str2)  
{  
    char *p=str1;  
    int i=0,len=strlen(str2);  
    p=strstr(str1,str2);  
    if(p==NULL)return -1;  
    while(p!=NULL)  
    {  
        for(;str1!=p;str1++)i++;  
        p=p+len;  
        p=strstr(p,str2);  
    }  
    return i;  
}  
  
void substring(char *dest,char *src,int start,int end)  
{  
    int i=start;  
    if(start>strlen(src))
		return;  
    if(end>strlen(src))  
        end=strlen(src);  
    while(i<end)  
    {     
        dest[i-start]=src[i];  
        i++;  
    }  
    dest[i-start]='\0';  
    return;  
}  

static void modifyDevIP(char *ip,  char* szEthName)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if(NULL == hSDK)
	{
		printf("Open device error\n");
		return;
	}

	printf("In automatic test, please wait for running results...\n");
	ICE_U32 result = 0;
	char szDevIDSrc[16] = {'\0'};
	char szDevID[32] = {'\0'};
	
	result = ICE_IPCSDK_GetDevID(hSDK, szDevIDSrc);
	if (1 != result)
	{
		printf("Get mac failed\n");
		return;
	}
	
	sprintf(szDevID, "%c%c-%c%c-%c%c-%c%c-%c%c-%c%c", szDevIDSrc[0], szDevIDSrc[1], 
			szDevIDSrc[2], szDevIDSrc[3], szDevIDSrc[4], szDevIDSrc[5], szDevIDSrc[6], 
			szDevIDSrc[7], szDevIDSrc[8], szDevIDSrc[9], szDevIDSrc[10], szDevIDSrc[11]);

	printf("mac:%s\n", szDevID);

	srand(time(NULL));
					
	char ip_new[16] = {'\0'};
	int ip_1 = rand() % 255;
	int ip_2 = rand() %253 + 2;
	sprintf(ip_new, "192.168.%d.%d", ip_1, ip_2);
	char gateway[16] = {'\0'};
	sprintf(gateway, "192.168.%d.1", ip_1);
					

	char szDev[4096];
	memset(szDev, 0, sizeof(szDev));
	ICE_IPCSDK_ModifyDevIP(szEthName, szDevID, ip_new, "255.255.255.0", gateway);
					
	usleep(2000 * 1000);

	ICE_IPCSDK_SearchDev(szEthName, szDev, 4096, 10000);	

	printf("List of cameras searched after modifying IP:\n%s", szDev);
		
	char szTmpDevID[32];			

	char *substr = strtok(szDev, " ");
	while( NULL != substr)
	{
		memset(szTmpDevID, 0, sizeof(szTmpDevID));
		char *tmp = strstr(substr, "\n");
		if (NULL != tmp)
		{
			strcpy(szTmpDevID, tmp + 1);
		}
		else
			strcpy(szTmpDevID, substr);

		if (strcasecmp(szTmpDevID, szDevID) == 0)
		{
			substr = strtok(NULL, " ");
			if (strcmp(substr, ip_new) != 0)
			{
				printf("Failed, the modified IP is not effective\n");
				ICE_IPCSDK_Close(hSDK);
				return;
			}
			else
			{
				int pos = lastIndexOf(ip, ".");
				char szGateway[16] = {'\0'};
				substring(szGateway, ip, 0, pos + 1);
				strcat(szGateway, "1");
				ICE_IPCSDK_ModifyDevIP(szEthName, szDevID, ip, "255.255.255.0", gateway);
				printf("Run successfully\n");
				ICE_IPCSDK_Close(hSDK);
				return;
			}
			break;
		}
		

		substr = strtok(NULL, "\n");
		if (NULL == substr)
		{
			printf("Failed, no device found1\n");
			ICE_IPCSDK_Close(hSDK);
			return;
		}
		substr = strtok(NULL, " ");
	}	
			
	ICE_IPCSDK_Close(hSDK);		
	printf("Failed, no device found2\n");
}

static void controlAlarmOut(char *ip, int nIndex)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK || nIndex < 1)
		return;

	int success = ICE_IPCSDK_ControlAlarmOut(hSDK, (nIndex-1));
	if (1 == success)
		printf("ControlAlarmOut successfully\n");
	else
		printf("ControlAlarmOut failed\n");

	ICE_IPCSDK_Close(hSDK);
}

static void syncTime(char *ip)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
		return;

	struct tm *timenow;

	time_t now;	
	time(&now);
	timenow = gmtime(&now);

	int success = ICE_IPCSDK_SyncTime(hSDK, (short)timenow->tm_year+1900, timenow->tm_mon+1, timenow->tm_mday, 
		timenow->tm_hour, timenow->tm_min, timenow->tm_sec);
	if (1 == success)
		printf("Sync time successfully\n");
	else
		printf("Sync time failed\n");

	ICE_IPCSDK_Close(hSDK);
}


static void WhiteListFindItem_Test(char *ip, char *plate)
{
	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
		return;
	bool bUTF8 = false;
	char gbPlate[128];
	char sdate[11];
	char edate[11];
	char stime[11];
	char etime[11];
	char type[11];
	char resvr1[64], resvr2[16], resvr3[16], resvr4[16], gbRemark[128];

	memset(gbPlate, 0, sizeof(gbPlate));
	memset(sdate, 0, sizeof(sdate));
	memset(edate, 0, sizeof(edate));
	memset(stime, 0, sizeof(stime));
	memset(etime, 0, sizeof(etime));
	memset(type, 0, sizeof(type));
	memset(resvr1, 0, sizeof(resvr1));
	memset(resvr2, 0, sizeof(resvr2));
	memset(resvr3, 0, sizeof(resvr3));
	memset(resvr4, 0, sizeof(resvr4));
	memset(gbRemark, 0, sizeof(gbRemark));

	memcpy(gbPlate, plate, strlen(plate));
		
	if (1 == ICE_IPCSDK_WhiteListFindItem_ByNumber(hSDK, gbPlate, sdate, edate, stime, etime, type,
		resvr1, resvr2, resvr3, resvr4))
	{
		memcpy(gbRemark, resvr1, strlen(resvr1));
		printf("List details:%s %s %s %s %s %s %s\n", plate, sdate, edate, stime, etime, type, gbRemark);
	}
		
	else
		printf("Find list: %s failed\n", plate);
		
	ICE_IPCSDK_Close(hSDK);
}


static void WhiteListGetAllItem_Test(char *ip, int type)
{
	long count = 0;
	int i = 0;
	char szNumber[128], szBeginDate[16], szEndDate[16], szBeginTime[16], szEndTime[16], szType[11],
		 szRerv2[16], szRerv3[16], szRerv4[16], utfRemark[128];
	memset(szNumber, 0, sizeof(szNumber));
	memset(szBeginDate, 0, sizeof(szBeginDate));
	memset(szEndDate, 0, sizeof(szEndDate));
	memset(szBeginTime, 0, sizeof(szBeginTime));
	memset(szEndTime, 0, sizeof(szEndTime));
	memset(szRerv2, 0, sizeof(szRerv2));
	memset(szRerv3, 0, sizeof(szRerv3));
	memset(szRerv4, 0, sizeof(szRerv4));
	memset(utfRemark, 0, sizeof(utfRemark));

	ICE_HANDLE hSDK = ICE_IPCSDK_OpenDevice(ip);
	if (NULL == hSDK)
		return;
	if (0 == type)
	{
		if (1 == ICE_IPCSDK_WhiteListGetCount(hSDK, &count))
		{
			for (i = 0; i < count; ++i)
			{
				memset(szNumber, 0, sizeof(szNumber));
				memset(szBeginDate, 0, sizeof(szBeginDate));
				memset(szEndDate, 0, sizeof(szEndDate));
				memset(szBeginTime, 0, sizeof(szBeginTime));
				memset(szEndTime, 0, sizeof(szEndTime));
				memset(utfRemark, 0, sizeof(utfRemark));
				memset(szRerv2, 0, sizeof(szRerv2));
				memset(szRerv3, 0, sizeof(szRerv3));
				memset(szRerv4, 0, sizeof(szRerv4));
				if (1 == ICE_IPCSDK_WhiteListGetItem(hSDK, i, szNumber, szBeginDate,
					szEndDate, szBeginTime, szEndTime, szType, utfRemark, szRerv2, szRerv3, szRerv4))
				{
					printf("%d %s %s %s %s %s %s %s\n", i+1, szNumber, szBeginDate,
						szEndDate, szBeginTime, szEndTime, szType, utfRemark);
				}
				else
					printf("Failed to get the list\n");
			}
		}
		else
			printf("Failed to get the list\n");
	}
	else if (1 == type)
	{
		if (1 == ICE_IPCSDK_GetWhiteCount(hSDK, &count))
		{
			for (i = 0; i < count; ++i)
			{
				memset(szNumber, 0, sizeof(szNumber));
				memset(szBeginDate, 0, sizeof(szBeginDate));
				memset(szEndDate, 0, sizeof(szEndDate));
				memset(szBeginTime, 0, sizeof(szBeginTime));
				memset(szEndTime, 0, sizeof(szEndTime));
				memset(utfRemark, 0, sizeof(utfRemark));
				memset(szRerv2, 0, sizeof(szRerv2));
				memset(szRerv3, 0, sizeof(szRerv3));
				memset(szRerv4, 0, sizeof(szRerv4));
				if (1 == ICE_IPCSDK_GetWhiteItem(hSDK, i, szNumber, szBeginDate,
					szEndDate, szBeginTime, szEndTime, szType, utfRemark, szRerv2, szRerv3, szRerv4))
				{
					printf("%d %s %s %s %s %s %s %s\n", i+1, szNumber, szBeginDate,
						szEndDate, szBeginTime, szEndTime, szType, utfRemark);
				}
				else
					printf("Failed to get whitelist\n");
			}
		}
		else
			printf("Failed to get whitelist\n");
	}
	else if (2 == type)
	{
		if (1 == ICE_IPCSDK_GetBlackCount(hSDK, &count))
		{
			for (i = 0; i < count; ++i)
			{
				memset(szNumber, 0, sizeof(szNumber));
				memset(szBeginDate, 0, sizeof(szBeginDate));
				memset(szEndDate, 0, sizeof(szEndDate));
				memset(szBeginTime, 0, sizeof(szBeginTime));
				memset(szEndTime, 0, sizeof(szEndTime));
				memset(utfRemark, 0, sizeof(utfRemark));
				memset(szRerv2, 0, sizeof(szRerv2));
				memset(szRerv3, 0, sizeof(szRerv3));
				memset(szRerv4, 0, sizeof(szRerv4));
				if (1 == ICE_IPCSDK_GetBlackItem(hSDK, i, szNumber, szBeginDate,
					szEndDate, szBeginTime, szEndTime, szType, utfRemark, szRerv2, szRerv3, szRerv4))
				{
					printf("%d %s %s %s %s %s %s %s\n", i+1, szNumber, szBeginDate,
						szEndDate, szBeginTime, szEndTime, szType, utfRemark);
				}
				else
					printf("Failed to get blacklist\n");
			}
		}
		else
			printf("Failed to get blacklist\n");
	}
	

	ICE_IPCSDK_Close(hSDK);
}

static int getDate(char* strSrc, char* strSplit, int *data)
{
	if (NULL == strSrc || NULL == strSplit || NULL == data)
		return 0;

	int i = 0;
	char szSrc[64];
	memset(data, 0, sizeof(data));
	strcpy(szSrc, strSrc);
	char *pch = strtok(szSrc, strSplit);
	while(NULL != pch)
	{
		if (i >= 3)
			break;
		data[i] = atoi(pch);
		i++;
		pch = strtok(NULL, strSplit);
	}
	return 1;
}


static int dateCompare(int startA, int startM, int startL, int endA, int endM, int endL)
{
	if (endA < startA)
		return -1;
	else if (endA == startA)
	{
		if (endM < startM)
			return -1;
		else if (endM == startM)
		{
			if (endL < startL)
			{
				return -1;
			}
			else if (endL == startL)
			{
				return 0;
			}
		}
	}

	return 1;
}


static int dateFormatCheck(char *strDate, const char *pattern)
{
	if (NULL == strDate || NULL == pattern)
		return 0;
	int flag = 0;
	regex_t reg;
	int status = 0;
	regmatch_t pmatch[1];
	const size_t nmatch = 1;
	int cflages = REG_EXTENDED;

	regcomp(&reg, pattern, cflages);
	status = regexec(&reg, strDate, nmatch, pmatch, 0);
	if (status == REG_NOMATCH)
		flag = 0;
	else if (status == 0)
		flag = 1;

	regfree(&reg);

	return flag;
}


static int listDataCheck(char* pcDateBegin, char* pcDateEnd, 
	char* pcTimeBegin, char* pcTimeEnd, char *pcType)
{
	if (NULL == pcDateBegin || NULL == pcDateEnd || NULL == pcTimeBegin 
		|| NULL == pcTimeEnd || NULL == pcType)
	{
		printf("Parameter error\n");
		return 0;
	}

	const char *pattern = "^(([0-9]{3}[1-9]|[0-9]{2}[1-9][0-9]{1}|[0-9]{1}[1-9][0-9]{2}|[1-9][0-9]{3})/(((0[13578]|1[02])/(0[1-9]|[12][0-9]|3[01]))|((0[469]|11)/(0[1-9]|[12][0-9]|30))|(02/(0[1-9]|[1][0-9]|2[0-8]))))|((([0-9]{2})(0[48]|[2468][048]|[13579][26])|((0[48]|[2468][048]|[3579][26])00))/02/29)$";
	const char *patternTime = "^(0[0-9]|1[0-9]|2[0-3]):[0-5][0-9]:([0-5][0-9])$";

	if (0 == dateFormatCheck(pcDateBegin, pattern))
	{
		printf("Start date parameter error\n"); 
		return 0;
	}		
	else if (0 == dateFormatCheck(pcDateEnd, pattern))
	{
		printf("End date parameter error\n");
		return 0;
	}
	else if (0 == dateFormatCheck(pcTimeBegin, patternTime))
	{
		printf("Start time parameter error\n");
		return 0;
	} 
	else if (0 == dateFormatCheck(pcTimeEnd, patternTime))
	{
		printf("End time parameter error\n"); 
		return 0;
	}
	else if(strcmp(pcType, "B") != 0 && strcmp(pcType, "W") != 0)
	{
		printf("Type error\n");
		return 0;
	}

	int nDateBegin[3], nDateEnd[3], nTimeBegin[3], nTimeEnd[3];
	memset(nDateBegin, 0, sizeof(nDateBegin));
	memset(nDateEnd, 0, sizeof(nDateEnd));
	memset(nTimeBegin, 0, sizeof(nTimeBegin));
	memset(nTimeEnd, 0, sizeof(nTimeEnd));

	if ((0 == getDate(pcDateBegin, "/", nDateBegin)) || (0 == getDate(pcDateEnd, "/", nDateEnd)) ||
		(0 == getDate(pcTimeBegin, ":", nTimeBegin)) || (0 == getDate(pcTimeEnd, ":", nTimeEnd)))
		return 0;
	if (dateCompare(nTimeBegin[0], nTimeBegin[1],nTimeBegin[2],
					nTimeEnd[0], nTimeEnd[1],nTimeEnd[2]) != 1)
	{
		printf("The end time is earlier than the start time.");
		return 0;
	}
	else if (dateCompare(nDateBegin[0], nDateBegin[1],nDateBegin[2], 
						nDateEnd[0], nDateEnd[1],nDateEnd[2]) != 1)
	{
		printf("The end date is earlier than the start date.");
		return 0;
	}

	return 1;
}

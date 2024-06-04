#ifndef _ICE_IPCSDK__H_
#define _ICE_IPCSDK__H_

#include "ice_base_type.h"
#include "ice_com_type.h"
#include "ice_vdc_result.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAYLOAD_TYPE_H264  96	
/**
* @brief  
*/
typedef enum
{
	ICE_DEVICE_OFFLINE,		    
	ICE_DEVICE_ONLINE,		    
	ICE_DEVICE_IO_CHANGED		
} ICE_DEVICE_EVENT_TYPE;

/**
*  @brief  Plate recognition data callback interface
*  @param  [OUT] pvParam                callback context
*  @param  [OUT] pcIP                   camera ip
*  @param  [OUT] ptLprResult			lpr result
*  @param  [OUT] pFullPicData			full picture data
*  @param  [OUT] fullPicLen				full picture length
*  @param  [OUT] pPlatePicData			plate picture data
*  @param  [OUT] platePiclen			plate picture length
*  @param  [OUT] u32Reserved1			reserve parameter
*  @param  [OUT] u32Reserved2			reserve parameter
*/
typedef void (*ICE_IPCSDK_OnPlate)(void *pvParam, const char *pcIp, T_LprResult *ptLprResult, const char *pFullPicData, int fullPicLen,
											  const char *pPlatePicData, int platePiclen, ICE_U32 u32Reserved1, ICE_U32 u32Reserved2);

 /**
 *  @brief  Transparent serial port receives events (rs485)
 *  @param  [OUT] pvParam   callback context
 *  @param  [OUT] pcIP      camera ip
 *  @param  [OUT] pcData    serial data
 *  @param  [OUT] u32Len    length of serial data
 *  @return void
 */
typedef void (*ICE_IPCSDK_OnSerialPort)(void *pvParam, 
	const char *pcIP, char *pcData, long u32Len);

/**
 *  @brief  Transparent serial port receives events(rs485-1)
 *  @param  [OUT] pvParam   callback context
 *  @param  [OUT] pcIP      camera ip
 *  @param  [OUT] pcData    serial data
 *  @param  [OUT] u32Len    length of serial data
 *  @return void
 */
typedef void (*ICE_IPCSDK_OnSerialPort_RS232)(void *pvParam, 
	const char *pcIP, char *pcData, long u32Len);

/**
*  @brief  device event callback
*  @param  [OUT] pvParam             callback context
*  @param  [OUT] pvReserved			 reserve parameter
*  @param  [OUT] pcIP                camera ip
*  @param  [OUT] u32EventType        Event type 0: offline 1: online 
*  @param  [OUT] u32Reserved1        reserve parameter
*  @param  [OUT] u32Reserved2        reserve parameter
*  @param  [OUT] u32Reserved3        reserve parameter
*  @param  [OUT] u32Reserved4        reserve parameter
*/
typedef void(*ICE_IPCSDK_OnDeviceEvent)(void *pvParam, void *pvReserved, const char *pcIP, long u32EventType, 
	long u32Reserved1, long u32Reserved2, long u32Reserved3, long u32Reserved4);

/**
*  @brief  IO state change event callback
*  @param  [OUT] pvParam 		callback context
*  @param  [OUT] pcIP			camera ip
*  @param  [OUT] u32EventType 	event type 0:IO change
*  @param  [OUT] u32IOData1		When the event type is 0, it means IO1’s state
*  @param  [OUT] u32IOData2		When the event type is 0, it means IO2’s state
*  @param  [OUT] u32IOData3		When the event type is 0, it means IO3’s state
*  @param  [OUT] u32IOData4		When the event type is 0, it means IO4’s state
*/
typedef void (*ICE_IPCSDK_OnIOEvent)(void *pvParam, const ICE_CHAR *pcIP, ICE_U32 u32EventType, 
	ICE_U32 u32IOData1, ICE_U32 u32IOData2, ICE_U32 u32IOData3, ICE_U32 u32IOData4);


/**
*  @brief  Stream callback interface, SDK will callback this interface when there is data on the network 
*  @param  [OUT] pvParam			callback context
*  @param  [OUT] u8PayloadType		Payload of the type defined above
*  @param  [OUT] u32Timestamp		timestamp
*  @param  [OUT] pvData				Address of data received
*  @param  [OUT] s32Len				Length of data received
*/
typedef void (*ICE_IPCSDK_OnStream)(void *pvParam, 
	ICE_U8 u8PayloadType, ICE_U32 u32Timestamp, 
	void *pvData, ICE_S32 s32Len);

/**
*  @brief  Connection without video streaming
*  @param  [IN] pcIP      ip address
*  @return SDK handle(If the connection is unsuccessful, the return value is null)
*/
ICE_HANDLE ICE_IPCSDK_OpenDevice(const char* pcIP);

/**
*  @brief  Connection without video streaming
*  @param  [IN] pcIP      ip address
*  @param  [IN] pcPasswd  password
*  @return SDK handle(If the connection is unsuccessful, the return value is null)
*/
ICE_HANDLE ICE_IPCSDK_OpenDevice_Passwd(const char* pcIP, const ICE_CHAR *passwd);

/**
*  @brief  disconnect
*  @param  [IN] hSDK    SDK handle
*  return void
*/
void ICE_IPCSDK_Close(ICE_HANDLE hSDK);

/**
*  @brief  Register lpr result event
*  @param  [IN] hSDK                   sdk handle
*  @param  [IN] iNeedPic               Is need picture
*  @param  [IN] iReserve			   reserve
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_RegLprResult(ICE_HANDLE hSDK, int iNeedPic, int iReserve);

/**
*  @brief  Unregister lpr result event
*  @param  [IN] hSDK                   sdk handle
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_UnregLprResult(ICE_HANDLE hSDK);

/**
*  @brief  Set the plate recognition event callback
*  @param  [IN] hSDK            SDK handle
*  @param  [IN] pfOnPlate       the plate recognition event callback
*  @param  [IN] pvPlateParam    callback context
*  @return void
*/
void ICE_IPCSDK_SetPlateCallBack(ICE_HANDLE hSDK, 
	ICE_IPCSDK_OnPlate pfOnPlate, void *pvPlateParam);

/**
*  @brief  Set device event callback
*  @param  [IN] pfOnDeviceEvent              device event callback
*  @param  [IN] pvDeviceEventParam           callback context
*  @param  [IN] nReserved1                   reserve1
*/
void ICE_IPCSDK_SetDeviceEventCallBack(ICE_IPCSDK_OnDeviceEvent pfOnDeviceEvent, 
	void *pvDeviceEventParam, long nReserved1);

/**
*  @brief  Register IO event
*  @param  [IN] hSDK                   sdk handle
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_RegIOEvent(ICE_HANDLE hSDK);

/**
*  @brief  Unregister IO event
*  @param  [IN] hSDK                   sdk handle
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_UnregIOEvent(ICE_HANDLE hSDK);

/**
*  @brief  Set the IO state change event callback
*  @param  [IN] hSDK                     SDK handle
*  @param  [IN] pfOnIOEvent              IO state change event callback
*  @param  [IN] pvIOEventParam           callback context
*/
void ICE_IPCSDK_SetIOEventCallBack(ICE_HANDLE hSDK, 
	ICE_IPCSDK_OnIOEvent pfOnIOEvent, void *pvIOEventParam);

/**
*  @brief  Set stream data callback
*  @param  [IN] hSDK			SDK handle
*  @param  [IN] pfOnStream		Stream callback interface, SDK will callback this interface when there is data on the network
*  @param  [IN] pvParam			callback context
*  @return void
*/
void ICE_IPCSDK_SetDataCallback(ICE_HANDLE hSDK, 
	ICE_IPCSDK_OnStream pfOnStream, void *pvParam);
	
/**
*  @brief  Software Trigger extension
*  @param  [IN] hSDK          SDK handle 
*  @return 0:fail 1 success 
*/
int ICE_IPCSDK_TriggerExt(ICE_HANDLE hSDK);

 /**
*  @brief  Open gate
*  @param  [IN] hSDK  SDK handle
*  @return 1 success 0 fail
 */
int ICE_IPCSDK_OpenGate(ICE_HANDLE hSDK);

/**
*  @brief  get device status
*  @param  [IN] hSDK      sdk handle
*  @return 0 offline 1 online
 */
int ICE_IPCSDK_GetStatus(ICE_HANDLE hSDK);

/**
*  @brief  Set osd stack configuration
*  @param  [IN] hSDK             SDK handle
*  @param  [IN] pstOSDAttr       OSD structure(ICE_OSDAttr_S)
*  @return 1 success 0 fail
*/
int ICE_IPCSDK_SetOsdCfg(ICE_HANDLE hSDK, ICE_OSDAttr_S *pstOSDAttr);

/**
*  @brief  Get osd stack configuration
*  @param  [IN] hSDK             SDK handle
*  @param  [IN] pstOSDAttr       OSD structure(ICE_OSDAttr_S)
*  @return 1 success 0 fail
*/
int ICE_IPCSDK_GetOSDCfg(ICE_HANDLE hSDK, ICE_OSDAttr_S *pstOSDAttr);

 /**
 *  @brief  Capture image
 *  @param  [IN]  hSDK          sdk handle
 *  @param  [OUT] pcPicData     The buffer address of the captured image
 *  @param  [OUT] nPicSize      The buffer size of the captured image
 *  @param  [OUT] nPicLen       The real size of the captured image
 *  @return 0 fail, 1 success
 */
int ICE_IPCSDK_Capture(ICE_HANDLE hSDK, char* pcPicData, long nPicSize, long* nPicLen);

/**
 *  @brief  Reboot
 *  @param  [IN] hSDK   sdk handle
 *  @return 0 fail, 1 success
 */
int ICE_IPCSDK_Reboot(ICE_HANDLE hSDK);

/**
*  @brief  Voice broadcast by index number (unicast)
*  @param  [IN] hSDK       SDK handle
*  @param  [IN] nIndex     voice file index
*  @return 1 success, 0 fail
 */
int ICE_IPCSDK_Broadcast(ICE_HANDLE hSDK, short nIndex);

/**
*  @brief  broadcast by filename (unicast)
*  @param  [IN] hSDK        SDK handle
*  @param  [IN] pcName      file name of the voice broadcast
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_Broadcast_ByName(ICE_HANDLE hSDK, const char* pcName);

/**
*  @brief  Voice multicast by filename
*  @param  [IN] hSDK        SDK handle
*  @param  [IN] pcName      A string containing the name of the voice file
							Note: the middle can be used; \t \n or space separate; Such as: 1 2 3 4 OR 1,2,3,4
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_BroadcastGroup_ByName(ICE_HANDLE hSDK, const char* pcName);

/**
*  @brief  Get the camera MAC address
*  @param  [IN] hSDK                  SDK handle
*  @param  [OUT] szDevID              MAC address
*  @return 0 fail 1 success
 */
int ICE_IPCSDK_GetDevID(ICE_HANDLE hSDK, char* szDevID);

/**
 *  @brief  Write user data
 *  @param  [IN] hSDK       SDK handle
 *  @parame [IN] szData     data that needs to be written
 *  @return 1 success,0 fail
 */
int ICE_IPCSDK_WriteUserData(ICE_HANDLE hSDK, const char* szData);

/**
 *  @brief  Read user data
 *  @param  [IN] hSDK       SDK handle
 *  @param  [OUT] szData    user data of read out 
 *  @param  [IN]  nSize     The maximum length of data read out, i.e. the size of user data allocation
 *  @param  [OUT] outSize	The real size of user data
 *  @return 1 success, 0 fail
 */
int ICE_IPCSDK_ReadUserData(ICE_HANDLE hSDK, char* szData, int nSize, int *outSize);

/**
 *  @brief  Send transparent serial port data(rs485)
 *  @param  [IN] hSDK      sdk handle
 *  @param  [IN] nData     Serial data
 *  @param  [IN] nLen      length of serial data
 *  @return 0 fail, 1 success
 */
int ICE_IPCSDK_TransSerialPort(ICE_HANDLE hSDK, char* nData, long nLen);

/**
 *  @brief  Send transparent serial port data(rs485-1)
 *  @param  [IN] hSDK      sdk handle
 *  @param  [IN] nData     Serial data
 *  @param  [IN] nLen      length of serial data
 *  @return 0 fail, 1 success
 */
int ICE_IPCSDK_TransSerialPort_RS232(ICE_HANDLE hSDK, char* nData, long nLen);

/**
*  @brief  Register serial port event
*  @param  [IN] hSDK                   sdk handle
*  @param  [IN] iChannel				1:RS485-1, 2:RS485-2, 3: RS485-1 + RS485-2
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_RegSerialPortEvent(ICE_HANDLE hSDK, ICE_U32 uiChannel);

/**
*  @brief  Unregister serial port event
*  @param  [IN] hSDK                   sdk handle
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_UnregSerialPortEvent(ICE_HANDLE hSDK);

 /**
*  @brief  Set transparent serial port receives events(rs485)
*  @param  [IN] hSDK                   SDK handle
*  @param  [IN] pfOnSerialPort         callback event
*  @param  [IN] pvSerialPortParam      callback context
 *  @return void
 */
void ICE_IPCSDK_SetSerialPortCallBack(ICE_HANDLE hSDK, 
	ICE_IPCSDK_OnSerialPort pfOnSerialPort, void *pvSerialPortParam);

/**
*  @brief  Set transparent serial port receives events(rs485-1)
*  @param  [IN] hSDK                   SDK handle
*  @param  [IN] pfOnSerialPort         callback event
*  @param  [IN] pvSerialPortParam      callback context
 *  @return void
 */
void ICE_IPCSDK_SetSerialPortCallBack_RS232(ICE_HANDLE hSDK, 
	ICE_IPCSDK_OnSerialPort_RS232 pfOnSerialPort, void *pvSerialPortParam);

/**
 *  @brief  Search device
 *  @param  [IN]  szIfName Interface name, such as eth0, can be viewed through ifconfig
 *  @param  [OUT] szDevs   String of device MAC address and IP address
*							   String of device MAC address and IP address,format is:mac ip\r\n  such as:
*							   00-00-00-00-00-00 192.168.55.150\r\n
 *  @param  [IN] nSize		The length of szDevs
 *  @param  [IN] nTimeoutMs Timeout of receiving device, in MS. That is, the total reception time. 
 *  @return void
 */
void ICE_IPCSDK_SearchDev(const char *szIfName, char *szDevs, int nSize, int nTimeoutMs);

/**
 *  @brief  Set the camera network configuration
 *  @param  [IN] hSDK        sdk handle
 *  @parame [IN] pcIP        camera IP address
 *  @param  [IN] pcMask      mask
 *  @param  [IN] pcGateway   gateway
 *  @return 0 fail, 1 success
 */
int ICE_IPCSDK_SetIPAddr(ICE_HANDLE hSDK, const char* pcIP, 
	const char* pcMask, const char* pcGateway);
	
/**
 *  @brief  Get the camera network configuration
 *  @param  [IN] hSDK        sdk handle
 *  @parame [OUT] pcIP        camera IP address
 *  @param  [OUT] pcMask      mask
 *  @param  [OUT] pcGateway   gateway
 *  @return 0 fail, 1 success
 */
int ICE_IPCSDK_GetIPAddr(ICE_HANDLE hSDK, char* pcIP, char* pcMask, char* pcGateway);

/**
*  @brief  Restore default parameters
*  @param  [IN] hSDK        sdk handle
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_DefaultParam(ICE_HANDLE hSDK);

/**
*  @brief  Get IO status
*  @param  [IN] hSDK             sdk handle
*  @param  [OUT] u32Index        IO index（0：IO1 1:IO2 2:IO3 3:IO4）
*  @param  [OUT] pu32IOState     IO status（0: data available 1: no data available）
*  @param  [OUT] pu32Reserve1    reserve1
*  @param  [OUT] pu32Reserve2    reserve2
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_GetIOState(ICE_HANDLE hSDK, long u32Index, long *pu32IOState, 
							long *pu32Reserve1, long *pu32Reserve2);

/*  @brief  Set the black and white list parameters 
*   @Note:
*Off-line association: when the camera is off the network, the front-end camera can only match the white-list association and open the gate.
*This mode is mainly used for whitelist which is done by platform in the case of network connection and by camera in the case of offline.
*Real-time work: no judgment network or network state, both by the front camera white list of association matching and open gate work.
*  @param  [IN]  hSDK        SDK handle
*  @param  [IN] whiteList   black and white list parameters
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_SetNewWhiteListParam(ICE_HANDLE hSDK, WhiteList_Param* whiteList);

/*  @brief  Get the black and white list parameters
*   @Note:
*Off-line association: when the camera is off the network, the front-end camera can only match the white-list association and open the gate.
*This mode is mainly used for whitelist which is done by platform in the case of network connection and by camera in the case of offline.
*Real-time work: no judgment network or network state, both by the front camera white list of association matching and open gate work.
*  @param  [IN]  hSDK        SDK handle
*  @param  [OUT] whiteList   black and white list parameters
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_GetNewWhiteListParam(ICE_HANDLE hSDK,  WhiteList_Param* whiteList);

/**
*  @brief  Edit B & W list item
*  @param  [IN]  hSDK				SDK handle
*  @param  [IN] pcNumber			plate number
*  @param  [IN] pcDateBegin			begin data(format is yyyy/MM/dd)
*  @param  [IN] pcDateEnd			end date(format is yyyy/MM/dd)
*  @param  [IN] pcTimeBegin			begin time(format is HH:mm:ss)
*  @param  [IN] pcTimeEnd			end time(format is HH:mm:ss)
*  @param  [IN] pcType				type('W':whitelist/'B':blacklist)
*  @param  [IN] pcRmark				remark
*  @param  [IN] pcRsrv2				reserve
*  @param  [IN] pcRsrv3				reserve
*  @param  [IN] pcRsrv4				reserve
*  @return 1 success,0 fail
*/
int ICE_IPCSDK_WhiteListEditItem_ByNumber(ICE_HANDLE hSDK, const char* pcNumber, 
	const char* pcDateBegin, const char* pcDateEnd, 
	const char* pcTimeBegin, const char* pcTimeEnd, const char* pcType, 
	const char* pcRemark, const char* pcRsrv2, const char* pcRsrv3, const char* pcRsrv4);

/**
*  @brief  Add B & W list item
*  @param  [IN]  hSDK				SDK handle
*  @param  [IN] s32Index			index of list which need to get
*  @param  [IN] pcNumber			plate number
*  @param  [IN] pcDateBegin			begin data(format is yyyy/MM/dd)
*  @param  [IN] pcDateEnd			end date(format is yyyy/MM/dd)
*  @param  [IN] pcTimeBegin			begin time(format is HH:mm:ss)
*  @param  [IN] pcTimeEnd			end time(format is HH:mm:ss)
*  @param  [IN] pcType				type('W':whitelist/'B':blacklist)
*  @param  [IN] pcRmark				remark
*  @param  [IN] pcRsrv2				reserve
*  @param  [IN] pcRsrv3				reserve
*  @param  [IN] pcRsrv4				reserve
*  @return 1 success,0 fail
*/
int ICE_IPCSDK_WhiteListInsertItem_ByNumber(ICE_HANDLE hSDK, const char* pcNumber, 
	const char* pcDateBegin, const char* pcDateEnd, const char* 
	pcTimeBegin, const char* pcTimeEnd, const char* pcType, 
	const char* pcRemark, const char* pcRsrv2, const char* pcRsrv3, const char* pcRsrv4);

/**
*  @brief  Add B & W list item By cover
*  @param  [IN]  hSDK				SDK handle
*  @param  [IN] s32Index			index of list which need to get
*  @param  [IN] pcNumber			plate number
*  @param  [IN] pcDateBegin			begin data(format is yyyy/MM/dd)
*  @param  [IN] pcDateEnd			end date(format is yyyy/MM/dd)
*  @param  [IN] pcTimeBegin			begin time(format is HH:mm:ss)
*  @param  [IN] pcTimeEnd			end time(format is HH:mm:ss)
*  @param  [IN] pcType				type('W':whitelist/'B':blacklist)
*  @param  [IN] pcRmark				remark
*  @param  [IN] pcRsrv2				reserve
*  @param  [IN] pcRsrv3				reserve
*  @param  [IN] pcRsrv4				reserve
*  @return 1 success,0 fail
*/
int ICE_IPCSDK_WhiteListInsertCoverItem_ByNumber(ICE_HANDLE hSDK, const char* pcNumber, 
	const char* pcDateBegin, const char* pcDateEnd, const char* pcTimeBegin, const char* pcTimeEnd, const char* pcType, 
	const char* pcRemark, const char* pcRsrv2, const char* pcRsrv3, const char* pcRsrv4);

/**
*  @brief  Delete a B & W list item
*  @param  [IN] hSDK              SDk handle
*  @param  [IN] pcNumber          plate number which need to delete
*  @return 1 success, 0 fail
*/
int ICE_IPCSDK_WhiteListDeleteItem_ByNumber(ICE_HANDLE hSDK, const char* pcNumber);

/**
*  @brief  Delete all B & W list item
*  @param  [IN] hSDK              SDk handle
*  @return 1 success, 0 fail
*/
int ICE_IPCSDK_WhiteListDelAllItems_ByNumber(ICE_HANDLE hSDK);

/**
*  @brief  Delete all whitelist items (excluding blacklist)
*  @param  [IN] hSDK             SDK handle
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_DelAllWhiteItems(ICE_HANDLE hSDK);

/**
*  @brief  Delete all blacklist items (excluding whitelist)
*  @param  [IN] hSDK             SDK handle
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_DelAllBlackItems(ICE_HANDLE hSDK);

/**
*  @brief  Find a black and white list item
*  @param  [IN] hSDK             SDK handle
*  @param  [IN] s32Index         plate number of list which need to find
*  @param  [OUT] pcNumber        plate number
*  @param  [OUT] pcDateBegin     begin data(format is yyyy/MM/dd)
*  @param  [OUT] pcDateEnd       end data(format is yyyy/MM/dd)
*  @param  [OUT] pcTimeBegin     begin time(format is HH:mm:ss)
*  @param  [OUT] pcTimeEnd       end time(format is HH:mm:ss)
*  @param  [OUT] pcType          type('W' white list /'B' black list)
*  @param  [OUT] pcRemark        remark
*  @param  [OUT] pcRsrv2         reserve
*  @param  [OUT] pcRsrv3         reserve
*  @param  [OUT] pcRsrv4         reserve
*  @return 1 success, 0 fail
*/
int ICE_IPCSDK_WhiteListFindItem_ByNumber(ICE_HANDLE hSDK, const char* pcNumber, 
	char* pcDateBegin, char* pcDateEnd, char* pcTimeBegin, char* pcTimeEnd, char* pcType, 
	char* pcRemark, char* pcRsrv2, char* pcRsrv3, char* pcRsrv4);

/**
*  @brief  Get the number of black and white lists
*          Note: this interface is called to allocate memory before the whitelist item is retrieved.
*  @param  [IN]  hSDK         SDK handle
*  @param  [OUT] pu32Count    Total number of list items
*  @return 1 success 0 fail
*/
int ICE_IPCSDK_WhiteListGetCount(ICE_HANDLE hSDK, long *pu32Count);

/**
*  @brief  Get total number of white list (excluding blacklist)
*  @param  [IN] hSDK              SDK handle
*  @param  [OUT] pu32Count        total number of white list
*  @param 1 success, 0 fail
*/
int ICE_IPCSDK_GetWhiteCount(ICE_HANDLE hSDK, long *pu32Count);

/**
*  @brief get total number of black list
*  @param  [IN]  hSDK            SDK handle
*  @param  [OUT] pu32Count       total number of black list
*  @return 1 success, 0 fail
*/
int ICE_IPCSDK_GetBlackCount(ICE_HANDLE hSDK, long *pu32Count);

/**
*  @brief  获取白名单项(旧模式,以序列号为索引)
*  @param  [IN] hSDK             sdk handle
*  @param  [IN] s32Index         需要获取的白名单的索引
*  @param  [OUT] pcNumber        白名单项中的车牌号
*  @param  [OUT] pcDateBegin     白名单项中的开始日期(格式为yyyy/MM/dd)
*  @param  [OUT] pcDateEnd       白名单项中的结束日期(格式为yyyy/MM/dd)
*  @param  [OUT] pcTimeBegin     白名单项中的开始时间(格式为HH:mm:ss)
*  @param  [OUT] pcTimeEnd       白名单项中的结束时间(格式为HH:mm:ss)
*  @param  [OUT] pcType          白名单项类型(白名单"W",黑名单"B")
*  @param  [OUT] pcRemark        车牌备注
*  @param  [OUT] pcRsrv2         预留字段2
*  @param  [OUT] pcRsrv3         预留字段3
*  @param  [OUT] pcRsrv4         预留字段4
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_WhiteListGetItem(ICE_HANDLE hSDK, long s32Index, char* pcNumber, 
	char* pcDateBegin, char* pcDateEnd, char* pcTimeBegin, char* pcTimeEnd, char *pcType,
	char* pcRemark, char* pcRsrv2, char* pcRsrv3, char* pcRsrv4);

/**
*  @brief  Gets the black and white list item
*  @param  [IN] hSDK             SDK handle
*  @param  [IN] s32Index         index of list which need to get
*  @param  [OUT] pcNumber        plate number
*  @param  [OUT] pcDateBegin     begin data(format is yyyy/MM/dd)
*  @param  [OUT] pcDateEnd       end data(format is yyyy/MM/dd)
*  @param  [OUT] pcTimeBegin     begin time(format is HH:mm:ss)
*  @param  [OUT] pcTimeEnd       end time(format is HH:mm:ss)
*  @param  [OUT] pcType          type('W' white list /'B' black list)
*  @param  [OUT] pcRsrv2         remark
*  @param  [OUT] pcRsrv3         reserve
*  @param  [OUT] pcRsrv4         reserve
*  @return 1 success, 0 fail
*/
int ICE_IPCSDK_GetWhiteItem(ICE_HANDLE hSDK, long s32Index, char* pcNumber, 
	char* pcDateBegin, char* pcDateEnd, char* pcTimeBegin, char* pcTimeEnd, char *pcType,
	char* pcRemark, char* pcRsrv2, char* pcRsrv3, char* pcRsrv4);

/**
*  @brief  Get black list items (excluding whitelist)
*  @param  [IN]  hSDK				SDK handle
*  @param  [IN] s32Index			index of list which need to get
*  @param  [OUT] pcNumber			plate number
*  @param  [OUT] pcDateBegin		begin data(format is yyyy/MM/dd)
*  @param  [OUT] pcDateEnd			end date(format is yyyy/MM/dd)
*  @param  [OUT] pcTimeBegin		begin time(format is HH:mm:ss)
*  @param  [OUT] pcTimeEnd			end time(format is HH:mm:ss)
*  @param  [OUT] pcRmark			remark
*  @param  [OUT] pcRsrv2				reserve
*  @param  [OUT] pcRsrv3			reserve
*  @param  [OUT] pcRsrv4			reserve
*  @return 1 success,0 fail
*/
int ICE_IPCSDK_GetBlackItem(ICE_HANDLE hSDK, long s32Index, char* pcNumber, 
	char* pcDateBegin, char* pcDateEnd, char* pcTimeBegin, char* pcTimeEnd, char *pcType,
	char* pcRemark, char* pcRsrv2, char* pcRsrv3, char* pcRsrv4);

/**
*  @brief  synchronization time
*  @param  [IN] hSDK		SDK handle
*  @param  [IN] u16Year		year
*  @param  [IN] u8Month		month
*  @param  [IN] u8Day		day
*  @param  [IN] u8Hour		hour
*  @param  [IN] u8Min		minute
*  @param  [IN] u8Sec		second
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_SyncTime(ICE_HANDLE hSDK, short u16Year, char u8Month, char u8Day, 
							char u8Hour, char u8Min, char u8Sec);
							
/**
*  @brief  Get camera time
*  @param  [IN] hSDK			SDK handle
*  @param  [OUT] devtime		time structure
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_GetSyncTime(ICE_HANDLE hSDK, struct tm* devtime);

/**
*  @brief  Control gate(alarmout)
*  @param  [IN] hSDK            SDK handle
*  @param  [IN] u32Index        u32Index IO index(0: IO1: IO2: IO3: IO4)
*  @return 1 success 0 fail
*/
int ICE_IPCSDK_ControlAlarmOut(ICE_HANDLE hSDK, int u32Index);

/**
*  @brief  Get SDK version
*  @param  [OUT] szSDKVersion   sdk version
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_GetSDKVersion(char* szSDKVersion);

/**
*  @brief  Get alarm out configuration
*  @param  [IN]   hSDK				SDK handle
*  @param  [IN] u32Index			IO index(0::IO1, 1:IO2, 2:IO3, 3:IO4)
*  @param  [OUT] pu32IdleState		IO state(0:normally open, 1:normally closed)
*  @param  [OUT] pu32DelayTime		Time of switching state (-1 represents non-recovery.unit: s, value: -1,1-10)
*  @param  [OUT] pu32Reserve		reserve
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_GetAlarmOutConfig(ICE_HANDLE hSDK, ICE_U32 u32Index, ICE_U32 *pu32IdleState, ICE_U32 *pu32DelayTime, ICE_U32 *pu32Reserve);

/**
*  @brief  Set alarm out configuration
*  @param  [IN]  hSDK				SDK handle
*  @param  [IN] u32Index	IO 		index(0::IO1, 1:IO2, 2:IO3, 3:IO4)
*  @param  [IN] u32IdleState		IO state(0:normally open, 1:normally closed)
*  @param  [IN] u32DelayTime		Time of switching state (-1 represents non-recovery.unit: s, value: -1,1-10)
*  @param  [IN] u32Reserve			reserve
*  @return 0 fail 1 success
*/	
int ICE_IPCSDK_SetAlarmOutConfig(ICE_HANDLE hSDK, ICE_U32 u32Index, ICE_U32 u32IdleState, ICE_U32 u32DelayTime, ICE_U32 u32Reserve);

/**
*  @brief  Modifying IP addresses across network segments
*  @param  [IN] szIfName       Interface name, such as eth0, can be viewed through ifconfig
*  @param  [IN] szMac          camera's mac
*  @param  [IN] szIP           camera's ip
*  @param  [IN] szMask         camera's mask
*  @param  [IN] szGateway      camera's gateway
*  @return void
*/

void ICE_IPCSDK_ModifyDevIP(const char *szIfName, const ICE_CHAR* szMac, const ICE_CHAR *szIP, 
										   const ICE_CHAR *szMask, const ICE_CHAR* szGateway);

/**
*  @brief  Get serial port configuration
*  @param  [IN]  hSDK             SDK handle
*  @param  [OUT] pstUARTCfg       serial port configuration(ICE_UART_PARAM)
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_GetUARTCfg(ICE_HANDLE hSDK, ICE_UART_PARAM *pstUARTCfg);

/**
*  @brief  Set serial port configuration
*  @param  [IN]  hSDK            SDK handle
*  @param  [IN] pstUARTCfg       serial port configuration(ICE_UART_PARAM)
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_SetUARTCfg(ICE_HANDLE hSDK, const ICE_UART_PARAM *pstUARTCfg);

/**
*  @brief  set encryption on and off
*  @param  [IN] hSDK        SDK handle
*  @param  [IN] ucEncId     Whether encrypted (unencrypted if 0, encrypted if others)
*  @param  [IN] szPwd       camera’s password
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_EnableEnc(ICE_HANDLE hSDK, ICE_U32 u32EncId, const char *szPwd);

/**
*  @brief  Modify camera’s password
*  @param  [IN] hSDK        SDK handle 
*  @param  [IN] szOldPwd    Camera’s old password
*  @param  [IN] szNewPwd    Camera’s new password
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_ModifyEncPwd(ICE_HANDLE hSDK, const char *szOldPwd, const char *szNewPwd);

/**
*  @brief  Set camera’s password
*  @param  [IN] hSDK        SDK handle
*  @param  [IN] szPwd       camera’s password`
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_SetDecPwd(ICE_HANDLE hSDK, const char *szPwd);

/**
*  @brief  start video
*  @param  [IN] hSDK          SDK handle
*  @param  [IN] u8MainStream  whether is main stream.1:main stream 0:subStream
*  @param  [IN] u32Reserve    reserve
*  @return 0 fail 1 success
*/
int ICE_IPCSDK_StartStream(ICE_HANDLE hSDK, ICE_U8 u8MainStream, ICE_U32 u32Reserve);

/**
*  @brief  stop video
*  @param  [IN] hSDK          sdk handle
*  @return void
*/
int ICE_IPCSDK_StopStream(ICE_HANDLE hSDK);

/*
*  @brief  Get additional alarm input parameters
*  @param  [IN] hSDK           SDK handle
*  @param  [OUT] pstParam      alarm input parameters 
*/
int ICE_IPCSDK_GetAlarmInExt(ICE_HANDLE hSDK, ALARM_IN_EXT *pstParam);

/*
*  @brief  Set additional alarm input parameters
*  @param  [IN] hSDK           SDK handle
*  @param  [IN] pstParam       alarm input parameters  
*/
int ICE_IPCSDK_SetAlarmInExt(ICE_HANDLE hSDK, ALARM_IN_EXT *pstParam);

/**
*  @brief  Get trigger mode configuration
*  @param  [IN] hSDK			SDK handle
*  @param  [OUT] u32TriggerMode trigger mode(0: coil trigger 1: video trigger 2: mixed trigger)
*  @return  1 success, 0 fail
*/
int ICE_IPCSDK_GetTriggerMode(ICE_HANDLE hSDK, ICE_U32 *pu32TriggerMode);

/**
*  @brief  Set trigger mode configuration
*  @param  [IN] hSDK			SDK handle
*  @param  [IN] u32TriggerMode	trigger mode(0: coil trigger 1: video trigger 2: mixed trigger)
*  @return  1 success, 0 fail
*/
int ICE_IPCSDK_SetTriggerMode(ICE_HANDLE hSDK, ICE_U32 u32TriggerMode);

/**
*  @brief  Get camera UID information
*  @param  [IN]   hSDK                     SDK handle
*  @param  [OUT]  pstParam				   UID information structure
*  @return 0 fail, 1 success
*/
int ICE_IPCSDK_GetUID(ICE_HANDLE hSDK, UID_PARAM *pstParam);

#ifdef __cplusplus
}
#endif

#endif
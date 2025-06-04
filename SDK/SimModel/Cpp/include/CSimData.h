#pragma once


#include "SimBasicTypes.h"
#include "CSimTypes.h"

#include <string.h>

#define EventTypeLen (64)

/**
* 仿真数据类定义
* 用于描述实体之间的交互信息（对应RTI的对象类）
*/
class CSimData
{
public:
	int64 time;					///< 仿真时间（ms）
	int64 sender;				///< 发送者（实体id）
    int64 receiver;				///< 接收者（实体id）
    int64 componentId;   ///< 发送者组件id
    char topic[EventTypeLen];	///< 交互类主题
    CDataFormat dataFormat;     ///数据的格式
	const void *data;			///< 数据载荷（用户定义的事件信息）
	uint32 length;				///< 数据长度

public:
	CSimData()
		: time(-1)
		, sender(-1)
        , receiver(-1)
        , componentId(-1)
        , dataFormat(STRUCT)
		, data(nullptr)
		, length(0)
	{
		memset(topic,0,EventTypeLen);
	}

	CSimData(int64 paramTime, int64 paramSender, int64 paramComponentId, int64 paramReceiver, const char *paramTopic, const void *paramData, uint32 paramLength)
		: time(paramTime)
		, sender(paramSender)
        , receiver(paramReceiver)
        , componentId(paramComponentId)
        , dataFormat(STRUCT)
		, data(paramData)
		, length(0)
	{
		(void)paramLength;
        memset(topic,0,EventTypeLen);
		if (paramTopic) {
            size_t typeLen = strlen(paramTopic);
            typeLen = typeLen < EventTypeLen ? typeLen : EventTypeLen;
			memcpy(topic, paramTopic, typeLen);
		}
	}


	~CSimData() {
	}
};

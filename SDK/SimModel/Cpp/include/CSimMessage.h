#ifndef SimMessage_h__
#define SimMessage_h__
#include "SimBasicTypes.h"
#include "CSimTypes.h"

#include <string.h>

#define EventTypeLen (64)

/**
* 仿真事件类定义
* 用于描述实体之间的交互信息（对应RTI的交互类）
*/
class CSimMessage
{
public:
	int64 time;					///< 仿真时间（ms）
	int64 sender;				///< 发送者（实体id）
	int64 senderComponentId;	///< 发送者组件id
    int64 receiver;				///< 接收者（实体id）
    CDataFormat dataFormat;     ///数据的格式
	char topic[EventTypeLen];	///< 交互类主题
	const void *data;			///< 数据载荷（用户定义的事件信息）
	uint32 length;				///< 数据长度

public:
	CSimMessage()
		: time(-1)
		, sender(-1)
		, senderComponentId(-1)
		, receiver(-1)
        , dataFormat(STRUCT)
		, data(nullptr)
		, length(0)
	{
		memset(topic,0,EventTypeLen);
	}

	CSimMessage(int64 paramTime, int64 paramSender, int64 paramComponentSender, int64 paramReceiver, const char *paramTopic, const void *paramData, uint32 paramLength)
		: time(paramTime)
		, sender(paramSender)
		, senderComponentId(paramComponentSender)
		, receiver(paramReceiver)
        , dataFormat(STRUCT)
		, data(paramData)
		, length(paramLength)
	{
		memset(topic,0,EventTypeLen);
		if (paramTopic) {
            size_t typeLen = strlen(paramTopic);
            typeLen = typeLen < EventTypeLen ? typeLen : EventTypeLen;
			memcpy(topic, paramTopic, typeLen);
		}
	}

	~CSimMessage() {
	}
};

#endif // SimMessage_h__

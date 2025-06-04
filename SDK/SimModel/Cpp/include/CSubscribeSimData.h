#pragma once


#include "SimBasicTypes.h"

#define EventTypeLen (64)

class CSubscribeSimData
{
public:
        ///本平台实体id
	int64 platformId;			 // 平台实体id
        ///目标id
        int64 targetId;              // 目标id,目标平台id或者阵营id
	char topic[EventTypeLen];	 // 主题
};

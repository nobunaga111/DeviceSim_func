#pragma once

#include "CSimModelAgentBase.h"
#include "CSimTypes.h"
#include "CSimComponentAttribute.h"


class SIM_SDK_API CSimComponentBase
{
public:
    virtual ~CSimComponentBase() = 0;

	/**
	* 初始化
	*/
	virtual bool init(CSimModelAgentBase* simModelAgent, CSimComponentAttribute* attr) = 0;

    /**
    * 开始
    */
    virtual void start() = 0;

    /**
    * 停止
    */
    virtual void stop() = 0;

	/**
	* 销毁
	*/
	virtual void destroy() = 0;

	/**
	* 消息处理
	*/
	virtual void onMessage(CSimMessage* simMessage) = 0;

	/**
	* 仿真步进接口 
	* curTime:当期仿真时间（ms）
	* step:步长（ms）
	*/
	virtual void step(int64 curTime, int32 step) = 0;

	/**
	*	返回该模型版本，格式是VX.X.X.20240326
	*/
    virtual const char* getVersion() = 0;

    /**
     * 序列化
     * @brief serialize
     * @return 返回json串
     */
    virtual std::string serialize();

    /**
     * 反序列化
     * @brief unSerialize
     * @param data   json串
     * @return  0:正常
     */
    virtual int unSerialize(std::string data);
};

#pragma once


#include "CSimMessage.h"
#include "CSimData.h"
#include "CSubscribeSimData.h"
#include "CSimTypes.h"
#include "CSimComponentAttribute.h"
#include "CSimIntegrationLogger.h"

#include <vector>


class SIM_SDK_API CSimModelAgentBase
{
public:
	virtual ~CSimModelAgentBase();
	/**
	* 发布数据信息
	*/
	virtual void publishSimData(CSimData* simData);

	/**
	* 获取已订阅的仿真数据
	*/
    virtual CSimData* getSubscribeSimData(const char* topic, int64 platformId);

    /**
    * 获取已订阅的仿真数据
    */
    virtual CSimData* getSubscribeSimData(const char* topic, int64 platformId, int64 componentId);

	/**
	* 订阅数据类主题
	*/
	virtual void subscribeSimData(CSubscribeSimData* subscribeSimData);

	/**
	* 订阅阵营数据类主题
	*/
	virtual void subscribeCampSimData(CSubscribeSimData* subscribeSimData);

	/**
	* 取消订阅数据类主题
	*/
	virtual void unsubscribSimData(CSubscribeSimData* subscribeSimData);

	/**
	* 取消订阅阵营数据类主题
	*/
	virtual void unsubscribCampSimData(CSubscribeSimData* subscribeSimData);

	/**
	* 发送交互信息
	* @param msg
	*/
	virtual void sendMessage(CSimMessage* simMessage);

	/**
	* 订阅事件类主题
	*/
    virtual void subscribeMessage(const char* topic);

	/**
	* 取消订阅事件类主题
	*/
    virtual void unsubscribeMessage(const char* topic);

    /**
    * 公布关键事件
    */
    virtual void publishCriticalEvent(CCriticalEvent* criticalEvent);

	/**
	* 获取实体信息（本组件装配到的实体）
	*/
	virtual CSimPlatformEntity* getPlatformEntity();

	/**
	* 获取当前组件属性
	*/
	virtual CSimComponentAttribute* getComponentAttribute();

	/**
	* 获取阵营id集合
	*/
	virtual std::vector<int32> getCamp();

	/**
	* 获取阵营所有实体集合
	*/
	virtual std::vector<CSimPlatformEntity*> getPlatformEntiysByCampId(int32 campId);

	/**
	* 获取当前组件推进累积时长
	*/
	virtual int64 getComponentElapseTime();

	/**
	* 获取当前引擎推进累积时长
	*/
	virtual int64 getEngineElapseTime();

	/**
	* 获取步长/间隔
	*/
	virtual int32 getStep();

	/**
	* 调整步长
	*/
	virtual void setStep(int32 step);

	/**
	* 获取想定的开始时间
	*
	* @return 想定开始时间
	*/
	virtual int64 getStartTime();

	/**
	* 获取想定的结束时间
	*
	* @return 想定结束时间
	*/
	virtual int64 getEndTime();

    /**
     * @brief getLogger  获取写日志功能类
     * @return
     */
    virtual CSimIntegrationLogger* getLogger();
};

#ifndef DEVICEMODELAGENT_H
#define DEVICEMODELAGENT_H

#include "CSimModelAgentBase.h"
#include <map>
#include <string>
#include <memory>
#include "../../DeviceModel/src/common/DMLogger.h"

class DeviceModelAgent : public CSimModelAgentBase
{
public:
    DeviceModelAgent();
    ~DeviceModelAgent();

    /**
    * 发布数据信息
    */
    void publishSimData(CSimData* simData) override;

    /**
    * 获取已订阅的仿真数据
    */
    CSimData* getSubscribeSimData(const char* topic, int64 platformId) override;

    /**
    * 获取已订阅的仿真数据
    */
    CSimData* getSubscribeSimData(const char* topic, int64 platformId, int64 componentId) override;

    /**
    * 订阅数据类主题
    */
    void subscribeSimData(CSubscribeSimData* subscribeSimData) override;

    /**
    * 订阅阵营数据类主题
    */
    void subscribeCampSimData(CSubscribeSimData* subscribeSimData) override;

    /**
    * 取消订阅数据类主题
    */
    void unsubscribSimData(CSubscribeSimData* subscribeSimData) override;

    /**
    * 取消订阅阵营数据类主题
    */
    void unsubscribCampSimData(CSubscribeSimData* subscribeSimData) override;

    /**
    * 发送交互信息
    * @param msg
    */
    void sendMessage(CSimMessage* simMessage) override;

    /**
    * 订阅事件类主题
    */
    void subscribeMessage(const char* topic) override;

    /**
    * 取消订阅事件类主题
    */
    void unsubscribeMessage(const char* topic) override;

    /**
    * 公布关键事件
    */
    void publishCriticalEvent(CCriticalEvent* criticalEvent) override;

    /**
    * 获取实体信息（本组件装配到的实体）
    */
    CSimPlatformEntity* getPlatformEntity() override;

    /**
    * 获取当前组件属性
    */
    CSimComponentAttribute* getComponentAttribute() override;

    /**
    * 获取阵营id集合
    */
    std::vector<int32> getCamp() override;

    /**
    * 获取阵营所有实体集合
    */
    std::vector<CSimPlatformEntity*> getPlatformEntiysByCampId(int32 campId) override;

    /**
    * 获取当前组件推进累积时长
    */
    int64 getComponentElapseTime() override;

    /**
    * 获取当前引擎推进累积时长
    */
    int64 getEngineElapseTime() override;

    /**
    * 获取步长/间隔
    */
    int32 getStep() override;

    /**
    * 调整步长
    */
    void setStep(int32 step) override;

    /**
    * 获取想定的开始时间
    *
    * @return 想定开始时间
    */
    int64 getStartTime() override;

    /**
    * 获取想定的结束时间
    *
    * @return 想定结束时间
    */
    int64 getEndTime() override;

    /**
     * @brief getLogger  获取写日志功能类
     * @return
     */
    CSimIntegrationLogger* getLogger() override;

    /**
    * 添加订阅数据 - 接口方法
    * @param topic 主题
    * @param platformId 平台ID
    * @param data 数据指针
    */
    void addSubscribedData(const char* topic, int64 platformId, CSimData* data);

    /**
    * 添加订阅数据 - 重载方法，支持组件ID
    * @param topic 主题
    * @param platformId 平台ID
    * @param componentId 组件ID
    * @param data 数据指针
    */
    void addSubscribedData(const char* topic, int64 platformId, int64 componentId, CSimData* data);

    /**
    * 清除过期的订阅数据
    * @param maxAge 最大年龄（毫秒）
    */
    void clearExpiredData(int64 maxAge = 10000);

    /**
    * 获取订阅数据的统计信息
    * @return 数据统计字符串
    */
    std::string getDataStatistics() const;

    /**
    * 检查数据是否有效
    * @param topic 主题
    * @param platformId 平台ID
    * @param maxAge 最大年龄（毫秒）
    * @return true if data is valid and not too old
    */
    bool isDataValid(const char* topic, int64 platformId, int64 maxAge = 5000) const;

private:
    /**
    * 生成数据存储的键值
    * @param topic 主题
    * @param platformId 平台ID
    * @param componentId 组件ID（可选）
    * @return 键值字符串
    */
    std::string generateDataKey(const char* topic, int64 platformId, int64 componentId = -1) const;

    /**
    * 安全删除数据指针
    * @param data 要删除的数据指针
    */
    void safeDeleteData(CSimData* data);

    /**
    * 打印调试信息
    * @param message 调试消息
    */
    void debugLog(const std::string& message) const;

private:
    CSimPlatformEntity m_platform;
    CSimComponentAttribute m_attribute;

    // 存储订阅的各种数据 - 使用智能指针管理内存
    std::map<std::string, std::unique_ptr<CSimData>> m_subscribedDataMap;

    // 数据访问统计
    mutable std::map<std::string, int> m_dataAccessCount;
    mutable std::map<std::string, int64> m_lastAccessTime;

    // 调试开关
    bool m_enableDebugOutput;

    // 数据清理相关
    int64 m_lastCleanupTime;
    static const int64 CLEANUP_INTERVAL_MS = 30000; // 30秒清理一次
    static const int64 DEFAULT_DATA_MAX_AGE_MS = 10000; // 默认数据有效期10秒
};

#endif // DEVICEMODELAGENT_H

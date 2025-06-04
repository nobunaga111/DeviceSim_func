#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include "CSimComponentBase.h"
#include "DeviceTestInOut.h"
#include <vector>
#include <map>
#include <fstream>
#include "common/DMLogger.h"

class DeviceModel : public CSimComponentBase
{
public:
    DeviceModel();
    ~DeviceModel();

    /**
    * 初始化
    */
    bool init(CSimModelAgentBase* simModelAgent, CSimComponentAttribute* attr) override;

    /**
    * 开始
    */
    void start() override;

    /**
    * 停止
    */
    void stop() override;

    /**
    * 销毁
    */
    void destroy() override;

    /**
    * 消息处理
    */
    void onMessage(CSimMessage* simMessage) override;

    /**
    * 仿真步进接口
    * curTime:当期仿真时间（ms）
    * step:步长（ms）
    */
    void step(int64 curTime, int32 step) override;

    /**
    * 返回该模型版本，格式是VX.X.X.20240326
    */
    const char* getVersion() override;

private:
    /**
     * @brief 处理声纳控制命令
     * @param simMessage 接收到的命令消息
     */
    void handleSonarControlOrder(CSimMessage* simMessage);

    /**
     * @brief 处理声纳初始化信息
     * @param simMessage 接收到的初始化消息
     */
    void handleSonarInitialization(CSimMessage* simMessage);

    /**
     * @brief 处理平台自噪声数据
     * @param simData 接收到的平台自噪声数据
     */
    void handlePlatformSelfSound(CSimData* simData);

    /**
     * @brief 处理传播声音数据
     * @param simMessage 接收到的传播声音消息
     */
    void handlePropagatedSound(CSimMessage* simMessage);

    /**
     * @brief 处理平台机动信息
     * @param simData 接收到的平台机动信息
     */
    void handleMotionData(CSimData* simData);

    /**
     * @brief 更新声纳状态
     */
    void updateSonarState();

    void initDetectionTrack();
private:
    CSimModelAgentBase* m_agent;               // 代理对象

    // 声纳状态信息
    bool m_initialized;                       // 初始化标志
    int64 curTime;
    std::map<int, CData_SonarState> m_sonarStates;  // 各声纳阵列状态

    // 声纳配置信息
    std::map<int, CAttr_PassiveSonarComponent> m_passiveSonarConfigs;  // 被动声纳配置

    // 平台信息
    CData_Motion m_platformMotion;              // 平台机动信息

    // 检测跟踪结果
    struct DetectionTrackData {
        std::vector<C_PassiveSonarDetectionResult> passiveDetections;
        std::vector<C_PassiveSonarTrackingResult> passiveTracks;
        std::vector<C_ActiveSonarDetectionResult> activeDetections;
        std::vector<C_ActiveSonarTrackingResult> activeTracks;
        std::vector<C_ScoutingSonarDetectionResult> scoutingDetections;
        int lastPassivePublishTime;
        int lastActivePublishTime;
        int lastScoutingPublishTime;
    };

    std::map<int, DetectionTrackData> m_detectionData;  // 各声纳阵列的探测数据

    // 环境数据
    float m_environmentNoiseLevel;              // 环境噪声级别

    // 处理周期控制
    int m_passiveProcessInterval;               // 被动处理周期（ms）
    int m_activeProcessInterval;                // 主动处理周期（ms）
    int m_scoutingProcessInterval;              // 侦察处理周期（ms）
///侦擦声纳上次处理时间
    int64 m_lastProcessTime;                    // 上次处理时间
    int m_nextTrackId;                          // 下一个跟踪ID

    std::ofstream log;
    bool ifOutput = true;
};

#endif // DEVICEMODEL_H

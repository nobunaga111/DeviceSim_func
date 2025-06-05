// 完整的 devicemodel.h 文件，包含所有声纳方程计算相关的声明

#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include "CSimComponentBase.h"
#include "DeviceTestInOut.h"
#include <vector>
#include <map>
#include <fstream>
#include <random>
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

    // *** 声纳方程计算相关的公共接口 ***
    /**
     * @brief 获取指定声纳的声纳方程计算结果
     * @param sonarID 声纳ID (0-3)
     * @return 声纳方程计算结果X值，如果没有有效结果返回0.0
     */
    double getSonarEquationResult(int sonarID);

    /**
     * @brief 获取所有声纳的声纳方程计算结果
     * @return 包含所有声纳计算结果的map (sonarID -> X值)
     */
    std::map<int, double> getAllSonarEquationResults();

    /**
     * @brief 设置指定声纳的DI计算参数
     * @param sonarID 声纳ID
     * @param frequency_khz 频率(kHz)
     * @param offset 偏移量
     */
    void setDIParameters(int sonarID, double frequency_khz, double offset);

    /**
     * @brief 检查指定声纳的数据是否有效且在时效内
     * @param sonarID 声纳ID
     * @return true if data is valid and recent
     */
    bool isEquationDataValid(int sonarID);

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

    // *** 声纳方程计算相关的私有方法 ***

    // 声纳方程计算相关的数据结构
    struct SonarEquationCache {
        // 传播后连续声数据缓存 (按声纳ID分别存储)
        std::map<int, std::vector<float>> propagatedContinuousSpectrum;

        // 平台区噪声数据缓存 (按声纳ID分别存储)
        std::map<int, std::vector<float>> platformSelfSoundSpectrum;

        // 海洋环境噪声数据缓存 (按声纳ID分别存储)
        std::map<int, std::vector<float>> environmentNoiseSpectrum;

        // 数据更新时间戳 (用于检查数据时效性)
        int64 lastPropagatedSoundTime;
        int64 lastPlatformSoundTime;
        int64 lastEnvironmentNoiseTime;

        // 声纳方程计算结果缓存
        std::map<int, double> equationResults;  // sonarID -> X值

        SonarEquationCache() {
            lastPropagatedSoundTime = 0;
            lastPlatformSoundTime = 0;
            lastEnvironmentNoiseTime = 0;
        }
    };

    // 各声纳位置的DI计算参数
    struct DIParameters {
        double frequency_khz;  // 频率参数(kHz)
        double offset;         // 偏移量参数
    };

    /**
     * @brief 更新传播后连续声数据缓存
     * @param simMessage 接收到的传播声消息
     */
    void updatePropagatedSoundCache(CSimMessage* simMessage);

    /**
     * @brief 更新平台区噪声数据缓存
     * @param simData 平台自噪声数据
     */
    void updatePlatformSelfSoundCache(CSimData* simData);

    /**
     * @brief 更新海洋环境噪声数据缓存
     * @param simMessage 环境噪声消息
     */
    void updateEnvironmentNoiseCache(CSimMessage* simMessage);

    /**
     * @brief 计算频谱数据的累加求和
     * @param spectrum 频谱数据
     * @return 累加求和值
     */
    double calculateSpectrumSum(const std::vector<float>& spectrum);

    /**
     * @brief 计算指定声纳的DI值
     * @param sonarID 声纳ID (0:艏端, 1:舷侧, 2:粗拖, 3:细拖)
     * @return DI值
     */
    double calculateDI(int sonarID);

    /**
     * @brief 计算单个声纳的声纳方程 SL-TL-NL+DI=X
     * @param sonarID 声纳ID
     * @return X值
     */
    double calculateSonarEquation(int sonarID);

    /**
     * @brief 执行所有声纳的声纳方程计算 (在step中调用)
     */
    void performSonarEquationCalculation();

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
    int64 m_lastProcessTime;                    // 上次处理时间
    int m_nextTrackId;                          // 下一个跟踪ID

    std::ofstream log;
    bool ifOutput = true;

    // *** 声纳方程计算相关的成员变量 ***
    SonarEquationCache m_equationCache;  // 声纳方程数据缓存

    // 声纳方程计算常量
    static const int SPECTRUM_DATA_SIZE = 5296;           // 频谱数据大小
    static const int DATA_UPDATE_INTERVAL_MS = 50000;     // 数据更新间隔(ms)
    static constexpr const double MAX_FREQUENCY_KHZ = 5.0;         // DI计算的最大频率(kHz)

    // 为4个声纳位置预设DI参数 (可通过setDIParameters修改)
    std::map<int, DIParameters> m_diParameters = {
        {0, {3.0, 9.5}},  // 艏端声纳: f=3kHz, offset=9.5
        {1, {3.2, 9.6}},  // 舷侧声纳: f=3.2kHz, offset=9.6
        {2, {2.8, 9.4}},  // 粗拖声纳: f=2.8kHz, offset=9.4
        {3, {3.5, 9.7}}   // 细拖声纳: f=3.5kHz, offset=9.7
    };
};

#endif // DEVICEMODEL_H

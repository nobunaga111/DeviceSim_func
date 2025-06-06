// 完整的 devicemodel.h 文件，支持多目标探测的声纳方程计算

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

    // *** 多目标声纳方程计算相关的公共接口 ***

    /**
     * @brief 单个目标的声纳方程计算结果
     */
    struct TargetEquationResult {
        int targetId;           // 目标ID
        double equationResult;  // 声纳方程计算结果X值
        float targetDistance;   // 目标距离
        float targetBearing;    // 目标方位角
        bool isValid;          // 结果是否有效

        TargetEquationResult() : targetId(-1), equationResult(0.0),
                               targetDistance(0.0), targetBearing(0.0), isValid(false) {}
    };



    /**
     * @brief 获取所有声纳的所有目标计算结果
     * @return 包含所有声纳所有目标计算结果的map (sonarID -> 目标结果列表)
     */
    std::map<int, std::vector<TargetEquationResult>> getAllSonarTargetsResults();


    void setFileLogEnabled(bool enabled);



    /**
     * @brief 设置全局探测阈值（所有声纳使用相同阈值）
     * @param threshold 探测阈值
     */
    void setGlobalDetectionThreshold(double threshold);

    /**
     * @brief 设置单个声纳的探测阈值
     * @param sonarID 声纳ID (0-3)
     * @param threshold 探测阈值
     */
    void setSonarDetectionThreshold(int sonarID, double threshold);

    /**
     * @brief 获取指定声纳的探测阈值
     * @param sonarID 声纳ID (0-3)
     * @return 探测阈值
     */
    double getSonarDetectionThreshold(int sonarID) const;

    /**
     * @brief 设置是否使用全局阈值
     * @param useGlobal true=使用全局阈值，false=使用各声纳独立阈值
     */
    void setUseGlobalThreshold(bool useGlobal);

    /**
     * @brief 获取是否使用全局阈值
     */
    bool isUsingGlobalThreshold() const;

    /**
     * @brief 获取全局探测阈值
     */
    double getGlobalDetectionThreshold() const;

    /**
     * @brief 保存阈值配置到文件
     * @param filename 配置文件路径
     */
    void saveThresholdConfig(const std::string& filename) const;

    /**
     * @brief 从文件加载阈值配置
     * @param filename 配置文件路径
     */
    void loadThresholdConfig(const std::string& filename);


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

//    /**
//     * @brief 处理平台自噪声数据
//     * @param simData 接收到的平台自噪声数据
//     */
//    void handlePlatformSelfSound(CSimData* simData);

//    /**
//     * @brief 处理传播声音数据
//     * @param simMessage 接收到的传播声音消息
//     */
//    void handlePropagatedSound(CSimMessage* simMessage);

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

     /**
      * @brief 获取声纳的相对探测角度范围（相对于本艇艏向）
      * @param sonarID 声纳ID
      * @return 相对角度范围 (起始角度, 结束角度)
      */
     std::pair<float, float> getRelativeSonarAngleRange(int sonarID);

    // *** 多目标声纳方程计算相关的私有方法 ***

    // 单个目标的数据结构
    struct TargetData {
        int targetId;                           // 目标ID
        std::vector<float> propagatedSpectrum;  // 传播后连续声频谱
        float targetDistance;                   // 目标距离
        float targetBearing;                    // 目标方位角
        int64 lastUpdateTime;                   // 最后更新时间
        bool isValid;                          // 数据是否有效

        TargetData() : targetId(-1), targetDistance(0.0), targetBearing(0.0),
                      lastUpdateTime(0), isValid(false) {}
    };

    // 多目标声纳方程计算的数据缓存结构
    struct MultiTargetSonarEquationCache {
        // 每个声纳可探测的目标数据 (声纳ID -> 目标数据列表，最多8个目标)
        std::map<int, std::vector<TargetData>> sonarTargetsData;

        // 平台区噪声数据缓存 (按声纳ID分别存储)
        std::map<int, std::vector<float>> platformSelfSoundSpectrum;

        // 海洋环境噪声数据缓存 (按声纳ID分别存储)
        std::map<int, std::vector<float>> environmentNoiseSpectrum;

        // 数据更新时间戳
        int64 lastPlatformSoundTime;
        int64 lastEnvironmentNoiseTime;

        // 多目标声纳方程计算结果缓存 (声纳ID -> 目标结果列表)
        std::map<int, std::vector<TargetEquationResult>> multiTargetEquationResults;

        MultiTargetSonarEquationCache() {
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
     * @brief 更新传播后连续声数据缓存（多目标版本）
     * @param simMessage 接收到的传播声消息
     */
    void updateMultiTargetPropagatedSoundCache(CSimMessage* simMessage);

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
     * @brief 计算单个目标的声纳方程 SL-TL-NL+DI=X
     * @param sonarID 声纳ID
     * @param targetData 目标数据
     * @return X值
     */
    double calculateTargetSonarEquation(int sonarID, const TargetData& targetData);

    /**
     * @brief 执行所有声纳所有目标的声纳方程计算 (在step中调用)
     */
    void performMultiTargetSonarEquationCalculation();

    /**
     * @brief 判断目标是否在声纳的探测范围内
     * @param sonarID 声纳ID
     * @param targetBearing 目标方位角
     * @param targetDistance 目标距离
     * @return true if target is in detection range
     */
    bool isTargetInSonarRange(int sonarID, float targetBearing, float targetDistance);

    /**
     * @brief 获取指定声纳的有效阈值（考虑全局/独立阈值设置）
     * @param sonarID 声纳ID
     * @return 实际使用的阈值
     */
    double getEffectiveThreshold(int sonarID) const;


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

    // *** 多目标声纳方程计算相关的成员变量 ***
    MultiTargetSonarEquationCache m_multiTargetCache;  // 多目标声纳方程数据缓存

    // 声纳方程计算常量
    static const int SPECTRUM_DATA_SIZE = 5296;           // 频谱数据大小
    static const int DATA_UPDATE_INTERVAL_MS = 5000;     // 数据更新间隔(ms)
    static const int MAX_TARGETS_PER_SONAR = 8;          // 每个声纳最大目标数
    static const int MAX_DETECTION_RANGE = 30000;        // 最大探测距离(米)
    static constexpr const double MAX_FREQUENCY_KHZ = 5.0;         // DI计算的最大频率(kHz)

    // 为4个声纳位置预设DI参数 (可通过setDIParameters修改)
    std::map<int, DIParameters> m_diParameters = {
        {0, {5.0, 17.5}},  // 艏端声纳: f=5kHz, offset=17.5
        {1, {3.0, 23.6}},  // 舷侧声纳: f=3kHz, offset=23.6
        {2, {0.5, 24}},    // 粗拖声纳: f=0.5kHz, offset=24
        {3, {0.3, 25}}     // 细拖声纳: f=0.3kHz, offset=25
    };



    int64 m_lastPropagatedSoundLogTime = 0;  // 上次打印传播声日志的时间
    int64 m_lastEnvironmentNoiseLogTime = 0;  // 上次打印环境噪声日志的时间  // 新增
    int64 m_lastPlatformSelfSoundLogTime = 0; // 上次打印平台自噪声日志的时间  // 新增
    static const int64 PROPAGATED_SOUND_LOG_INTERVAL = 5000; // 5秒打印间隔




    // *** 可配置的探测阈值相关 ***
    // 每个声纳的探测阈值配置
    std::map<int, double> m_detectionThresholds = {
       {0, 33.0},  // 艏端声纳默认阈值
       {1, 33.0},  // 舷侧声纳默认阈值
       {2, 43.0},  // 粗拖声纳默认阈值
       {3, 43.0}   // 细拖声纳默认阈值
    };

    // 全局阈值（所有声纳使用相同阈值）
    bool m_useGlobalThreshold = true;
    double m_globalDetectionThreshold = 33.0;
};

#endif // DEVICEMODEL_H

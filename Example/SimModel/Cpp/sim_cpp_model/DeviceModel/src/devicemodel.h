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


// 前向声明
//class QMap;
//template<typename T> class QMap;

//namespace MainWindow {
//    struct SonarRangeConfig;
//}

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
     * @brief 保存阈值配置到文件
     * @param filename 配置文件路径
     */
    void saveThresholdConfig(const std::string& filename) const;

    /**
     * @brief 从文件加载阈值配置
     * @param filename 配置文件路径
     */
    void loadThresholdConfig(const std::string& filename);


   /**
    * @brief 声纳角度范围配置结构体（DeviceModel独立定义）
    */
   struct SonarAngleConfig {
       float startAngle1;        // 第一段起始角度
       float endAngle1;          // 第一段结束角度
       float startAngle2;        // 第二段起始角度（仅舷侧声纳使用）
       float endAngle2;          // 第二段结束角度（仅舷侧声纳使用）
       bool hasTwoSegments;      // 是否有两个角度段

       SonarAngleConfig() : startAngle1(0.0f), endAngle1(0.0f),
                          startAngle2(0.0f), endAngle2(0.0f),
                          hasTwoSegments(false) {}

       SonarAngleConfig(float start1, float end1, float start2 = 0.0f, float end2 = 0.0f, bool twoSeg = false)
           : startAngle1(start1), endAngle1(end1), startAngle2(start2), endAngle2(end2), hasTwoSegments(twoSeg) {}
   };

   /**
    * @brief 声纳范围配置结构体（DeviceModel独立定义，仅用于保存配置文件）
    */
   struct SonarRangeConfig {
       int sonarId;
       float maxRange;           // 最大探测距离（米）
       float startAngle1;        // 第一段起始角度
       float endAngle1;          // 第一段结束角度
       float startAngle2;        // 第二段起始角度（仅舷侧声纳使用）
       float endAngle2;          // 第二段结束角度（仅舷侧声纳使用）
       bool hasTwoSegments;      // 是否有两个角度段

       SonarRangeConfig() : sonarId(-1), maxRange(30000.0f),
                          startAngle1(0.0f), endAngle1(0.0f),
                          startAngle2(0.0f), endAngle2(0.0f),
                          hasTwoSegments(false) {}
   };

   /**
    * @brief 保存扩展配置到文件（独立接口，不依赖外部类型）
    * @param filename 配置文件路径
    * @param sonarRangeMap 声纳范围配置映射 (sonarID -> SonarRangeConfig)
    */
   void saveExtendedConfig(const std::string& filename, const std::map<int, SonarRangeConfig>& sonarRangeMap) const;

   /**
    * @brief 从文件加载扩展配置
    * @param filename 配置文件路径
    */
   void loadExtendedConfig(const std::string& filename);

   /**
    * @brief 设置声纳角度范围配置
    * @param sonarID 声纳ID
    * @param config 角度配置
    */
   void setSonarAngleConfig(int sonarID, const SonarAngleConfig& config);

   /**
    * @brief 获取声纳角度范围配置
    * @param sonarID 声纳ID
    * @return 角度配置
    */
   SonarAngleConfig getSonarAngleConfig(int sonarID) const;

   /**
    * @brief 获取所有声纳的角度配置（用于外部同步显示）
    * @return 所有声纳的角度配置映射
    */
   std::map<int, SonarAngleConfig> getAllSonarAngleConfigs() const;

   /**
    * @brief 设置声纳最大显示距离（仅用于保存配置，不影响探测逻辑）
    * @param sonarID 声纳ID
    * @param maxRange 最大显示距离
    */
   void setSonarMaxDisplayRange(int sonarID, float maxRange);

   /**
    * @brief 获取声纳最大显示距离
    * @param sonarID 声纳ID
    * @return 最大显示距离
    */
   float getSonarMaxDisplayRange(int sonarID) const;

   /**
    * @brief 获取所有声纳的显示距离配置
    * @return 所有声纳的显示距离映射
    */
   std::map<int, float> getAllSonarMaxDisplayRanges() const;



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
        std::map<int, std::vector<float>> platformSelfSoundSpectrumMap;

        // 海洋环境噪声数据缓存 (按声纳ID分别存储)
        std::map<int, std::vector<float>> environmentNoiseSpectrumMap;

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


    // *** 声纳角度范围配置相关 ***
   std::map<int, SonarAngleConfig> m_sonarAngleConfigs = {
      {0, SonarAngleConfig(-45.0f, 45.0f)},                                    // 艏端声纳: -45°到45°
      {1, SonarAngleConfig(45.0f, 135.0f, -135.0f, -45.0f, true)},           // 舷侧声纳: 两个分段
      {2, SonarAngleConfig(135.0f, 225.0f)},                                  // 粗拖声纳: 135°到225°
      {3, SonarAngleConfig(120.0f, 240.0f)}                                   // 细拖声纳: 120°到240°
   };

   // *** 声纳最大显示距离配置（仅用于界面显示，不影响探测逻辑）***
   std::map<int, float> m_sonarMaxDisplayRanges = {
      {0, 30000.0f},  // 艏端声纳默认30km
      {1, 25000.0f},  // 舷侧声纳默认25km
      {2, 35000.0f},  // 粗拖声纳默认35km
      {3, 40000.0f}   // 细拖声纳默认40km
   };

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
//    float m_environmentNoiseLevel;              // 环境噪声级别

    // 处理周期控制
//    int m_passiveProcessInterval;               // 被动处理周期（ms）
//    int m_activeProcessInterval;                // 主动处理周期（ms）
//    int m_scoutingProcessInterval;              // 侦察处理周期（ms）
//    int64 m_lastProcessTime;                    // 上次处理时间
//    int m_nextTrackId;                          // 下一个跟踪ID

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

};

#endif // DEVICEMODEL_H

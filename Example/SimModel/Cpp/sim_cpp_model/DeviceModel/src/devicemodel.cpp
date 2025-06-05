#include "devicemodel.h"

#include <iostream>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <thread>
#include <iomanip>
#include <cstring>
#include <random>
#include "common/define.h"
#include <QString>
#include <QDateTime>

constexpr const double DeviceModel::MAX_FREQUENCY_KHZ;

DeviceModel::DeviceModel()
{
    m_agent = nullptr; // CSimModelAgentBase 代理对象
    m_initialized = false; // 声纳状态信息 初始化标志
    m_environmentNoiseLevel = 60.0f; // 环境噪声级别
    m_passiveProcessInterval = 1000; // 被动处理周期（ms）
    m_activeProcessInterval = 2000; // 主动处理周期（ms）
    m_scoutingProcessInterval = 3000; // 侦察处理周期（ms）
    m_lastProcessTime = 0; // 上次处理时间
    m_nextTrackId = 1; // 下一个跟踪ID

    // 初始化多目标缓存
    m_multiTargetCache = MultiTargetSonarEquationCache();
    // 初始化兼容性缓存
    m_equationCache = SonarEquationCache();

    LOG_INFO("Sonar model created with multi-target equation calculation capability");

    // 初始化平台机动信息
    m_platformMotion = CData_Motion();  // 调用默认构造函数
    m_platformMotion.action = true; //实体行动状态 false:异常   true：正常
    m_platformMotion.isPending = false;

    log.open("device_model.log", std::ios::out | std::ios::app);
    if (!log.is_open()) {
        std::cerr << "Failed to open log file" << std::endl;
    }
}

DeviceModel::~DeviceModel()
{
    LOG_INFO("Sonar model destroyed");

    if (log.is_open()) {
        log.close();
    }
}

void DeviceModel::initDetectionTrack()
{
    // 初始化跟踪状态
    DetectionTrackData emptyData;
    emptyData.lastPassivePublishTime = 0;
    emptyData.lastActivePublishTime = 0;
    emptyData.lastScoutingPublishTime = 0;
    // 为每个可能的声纳ID预初始化数据结构
    for (int i = 1; i <= 8; i++) {
        m_detectionData[i] = emptyData;

        // 初始化声纳状态
        CData_SonarState state;
        state.sonarID = i;
        m_sonarStates[i] = state;
    }

    // 为4个主要声纳初始化多目标数据结构
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        m_multiTargetCache.sonarTargetsData[sonarID] = std::vector<TargetData>();
        m_multiTargetCache.sonarTargetsData[sonarID].reserve(MAX_TARGETS_PER_SONAR);
    }
}

bool DeviceModel::init(CSimModelAgentBase* simModelAgent, CSimComponentAttribute* attr)
{
    LOG_INFO("Multi-target sonar model init");
    (void)attr;

    if (!simModelAgent)
    {
        LOG_WARN("simModelAgent is null!");
        return false;
    }
    m_agent = simModelAgent;

    initDetectionTrack();

    m_initialized = true;

    return true;
}

void DeviceModel::start()
{
    LOG_INFO("Multi-target sonar model started");

    if (m_agent) {
        // 订阅声纳需要处理的消息主题
        m_agent->subscribeMessage(MSG_SonarCommandControlOrder);
        m_agent->subscribeMessage(MSG_PropagatedContinuousSound);
        m_agent->subscribeMessage(MSG_PropagatedActivePulseSound);
        m_agent->subscribeMessage(MSG_PropagatedCommPulseSound);
        m_agent->subscribeMessage(MSG_EnvironmentNoiseToSonar);
        m_agent->subscribeMessage(MSG_PropagatedInstantSound);

        // 订阅声纳需要处理的数据主题
        CSubscribeSimData motion;
        memset(&motion, 0, sizeof(motion));
        // 订阅平台机动信息
        memcpy(motion.topic, Data_Motion, strlen(Data_Motion) + 1);
        motion.platformId = m_agent->getPlatformEntity()->id;//本平台id
        motion.targetId = m_agent->getPlatformEntity()->id;
        m_agent->subscribeSimData(&motion);

        CSubscribeSimData selfSound;
        memset(&selfSound, 0, sizeof(selfSound));
        // 订阅平台自噪声
        memcpy(selfSound.topic, Data_PlatformSelfSound, strlen(Data_PlatformSelfSound) + 1);
        selfSound.platformId = m_agent->getPlatformEntity()->id;//本平台id
        selfSound.targetId = m_agent->getPlatformEntity()->id;
        m_agent->subscribeSimData(&selfSound);
    }

    // 初始化4个声纳状态（ID：0,1,2,3）
    for (int i = 0; i < 4; i++) {
        CData_SonarState& state = m_sonarStates[i];

        // 更新工作状态
        state.sonarID = i;
        state.arrayWorkingState = 1;
        state.activeWorkingState = 1;
        state.passiveWorkingState = 1;
        state.scoutingWorkingState = 1;
        state.multiSendWorkingState = 1;
        state.multiReceiveWorkingState = 1;
        state.activeTransmitWorkingState = 1;

        LOG_INFOF("Updated sonar %d state: array=%d active=%d passive=%d scouting=%d",
                  i, state.arrayWorkingState, state.activeWorkingState,
                  state.passiveWorkingState, state.scoutingWorkingState);

        // 初始化检测跟踪数据
        DetectionTrackData emptyData;
        emptyData.lastPassivePublishTime = 0;
        emptyData.lastActivePublishTime = 0;
        emptyData.lastScoutingPublishTime = 0;
        m_detectionData[i] = emptyData;
    }

    // 发布更新后的声纳状态
    updateSonarState();
}

void DeviceModel::stop()
{
    LOG_INFO("Multi-target sonar model stopped");
}

void DeviceModel::destroy()
{
    LOG_INFO("Multi-target sonar model destroyed");
}

void DeviceModel::onMessage(CSimMessage* simMessage)
{
    if (!simMessage || !m_agent) {
        std::cout << __FUNCTION__ << ":" << __LINE__ << " simMessage or m_agent is null!" << std::endl;
        return;
    }

    // 根据消息主题进行相应处理
    std::string topic = simMessage->topic;
    log<<"onMessage!!!Topic:"<<topic<<"\n";
    LOG_INFOF("Received message with topic: %s", topic.c_str());

    // 声纳指控指令
    if (topic == MSG_SonarCommandControlOrder) {
        // 处理声纳控制命令
        handleSonarControlOrder(simMessage);
    }
    else if (topic == ATTR_PassiveSonarComponent) {
        // 处理被动声纳初始化信息
        handleSonarInitialization(simMessage);
    }
    else if (topic == MSG_PropagatedContinuousSound ||
             topic == MSG_PropagatedActivePulseSound ||
             topic == MSG_PropagatedCommPulseSound ||
             topic == MSG_PropagatedInstantSound) {
        // 处理传播声音数据
        handlePropagatedSound(simMessage);
    }
    else if (topic == MSG_EnvironmentNoiseToSonar) {
        // 处理环境噪声数据
        updateEnvironmentNoiseCache(simMessage);
    }
    else {
        std::cout << __FUNCTION__ << ":" << __LINE__ << " Unknown message topic: " << topic << std::endl;
    }
}

void DeviceModel::handleSonarControlOrder(CSimMessage* simMessage)
{
    const CMsg_SonarCommandControlOrder* order =
        reinterpret_cast<const CMsg_SonarCommandControlOrder*>(simMessage->data);

    // 检查声纳ID是否在有效范围内（0-3）
    if (order->sonarID < 0 || order->sonarID >= 4) {
        LOG_WARNF("Invalid sonar ID: %d, should be 0-3", order->sonarID);
        return;
    }

    // 找到对应的声纳ID并更新状态
    if (m_sonarStates.find(order->sonarID) != m_sonarStates.end()) {
        CData_SonarState& state = m_sonarStates[order->sonarID];

        // 更新工作状态
        state.arrayWorkingState = order->arrayWorkingOrder;
        state.activeWorkingState = order->activeWorkingOrder;
        state.passiveWorkingState = order->passiveWorkingOrder;
        state.scoutingWorkingState = order->scoutingWorkingOrder;
        state.multiSendWorkingState = order->multiSendWorkingOrder;
        state.multiReceiveWorkingState = order->multiReceiveWorkingOrder;
        state.activeTransmitWorkingState = order->activeTransmitWorkingOrder;

        if(ifOutput){
            log << __FUNCTION__ << ":" << __LINE__
                      << " Updated sonar " << order->sonarID << " state: "
                      << " array=" << state.arrayWorkingState
                      << " active=" << state.activeWorkingState
                      << " passive=" << state.passiveWorkingState
                      << " scouting=" << state.scoutingWorkingState
                      << std::endl;
            log.flush();
        }

        // 发布更新后的声纳状态
        updateSonarState();
    }
}

// 处理传播声音数据
void DeviceModel::handlePropagatedSound(CSimMessage* simMessage)
{
    std::string topic = simMessage->topic;

    // 使用新的多目标处理方法
    if (topic == MSG_PropagatedContinuousSound) {
        updateMultiTargetPropagatedSoundCache(simMessage);
    }
    // 继续支持原有方法用于兼容性
    else if (topic == MSG_PropagatedActivePulseSound) {
        const CMsg_PropagatedActivePulseSoundListStruct* sounds =
            reinterpret_cast<const CMsg_PropagatedActivePulseSoundListStruct*>(simMessage->data);

        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Received active pulse sound data, count: "
                  << sounds->propagateActivePulseList.size() << std::endl;
    }
    else if (topic == MSG_PropagatedCommPulseSound) {
        const CMsg_PropagatedCommPulseSoundListStruct* sounds =
            reinterpret_cast<const CMsg_PropagatedCommPulseSoundListStruct*>(simMessage->data);

        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Received comm pulse sound data, count: "
                  << sounds->propagatedCommList.size() << std::endl;
    }
}

// 平台自噪声数据
void DeviceModel::handlePlatformSelfSound(CSimData* simData)
{
    if (!simData->data)
    {
        return;
    }

    updatePlatformSelfSoundCache(simData);
}

void DeviceModel::step(int64 curTime, int32 step)
{
    LOG_INFO("Multi-target sonar model step");
    (void)step;

    if (!m_agent || !m_initialized)
    {
        return;
    }
    this->curTime = curTime;

    CMsg_SonarWorkState sta;
    sta.platformId = m_agent->getPlatformEntity()->id;
    sta.maxDetectRange = MAX_DETECTION_RANGE;
    sta.sonarOnOff = true;
    CSimMessage cSimMessage;
    cSimMessage.sender = m_agent->getPlatformEntity()->id;
    memset(cSimMessage.topic,0,EventTypeLen);
    memcpy(cSimMessage.topic,Msg_SonarWorkState,sizeof(Msg_SonarWorkState));
    cSimMessage.data=&sta;
    m_agent->sendMessage(&cSimMessage);

    // 获取平台机动信息
    CSimData* motionData = m_agent->getSubscribeSimData(Data_Motion, m_agent->getPlatformEntity()->id);
    if (motionData) {
        handleMotionData(motionData);
    }

    // 获取平台自噪声数据
    int64 platformId = m_agent->getPlatformEntity()->id;
    LOG_INFOF("Attempting to get platform self sound data for platformId: %lld", platformId);

    CSimData* selfSoundData = m_agent->getSubscribeSimData(Data_PlatformSelfSound, platformId);
    if (selfSoundData) {
        LOG_INFOF("Retrieved platform self sound data with timestamp: %lld", selfSoundData->time);
        handlePlatformSelfSound(selfSoundData);
    } else {
        LOG_WARN("Failed to retrieve platform self sound data in step()");
        LOG_WARNF("Topic: %s, PlatformId: %lld", Data_PlatformSelfSound, platformId);
    }

    // 执行多目标声纳方程计算
    performMultiTargetSonarEquationCalculation();
}

void DeviceModel::handleMotionData(CSimData* simData)
{
    if (!simData->data) return;

    const CData_Motion* motionData =
        reinterpret_cast<const CData_Motion*>(simData->data);

    // 更新平台机动信息
    m_platformMotion = *motionData;

    std::cout << __FUNCTION__ << ":" << __LINE__
              << std::fixed << std::setprecision(6)
              << " Platform motion updated: "
              << " lon=" << m_platformMotion.x
              << " lat=" << m_platformMotion.y
              << " alt=" << m_platformMotion.z
              << " heading=" << m_platformMotion.rotation
              << " speed=" << m_platformMotion.curSpeed
              << std::endl;
}

const char* DeviceModel::getVersion()
{
    return "Multi-Target Sonar Model V2.0.0.20240605";
}

void DeviceModel::updateSonarState()
{
    if (!m_agent)
    {
        return;
    }

    // 为每个声纳阵列发布状态（ID：0-3）
    for (const auto& entry : m_sonarStates) {
        CData_SonarState state = entry.second;

        // 发布声纳状态
        CSimData simData;
        simData.dataFormat = STRUCT;
        simData.sender = m_agent->getPlatformEntity()->id;
        memcpy(simData.topic, Data_SonarState_Topic, strlen(Data_SonarState_Topic) + 1);
        simData.data = &state;
        simData.length = sizeof(state);

        m_agent->publishSimData(&simData);
    }
}

void DeviceModel::handleSonarInitialization(CSimMessage* simMessage)
{
    const CAttr_PassiveSonarComponent* config =
        reinterpret_cast<const CAttr_PassiveSonarComponent*>(simMessage->data);

    // 检查声纳ID是否在有效范围内（0-3）
    if (config->sonarID < 0 || config->sonarID >= 4) {
        LOG_WARNF("Invalid sonar ID: %d, should be 0-3", config->sonarID);
        return;
    }

    // 保存被动声纳配置
    m_passiveSonarConfigs[config->sonarID] = *config;

    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Initialized passive sonar " << config->sonarID
              << ", mode=" << config->sonarMod
              << ", array elements=" << config->arrayNumber
              << std::endl;
}

// devicemodel.cpp  - 多目标声纳方程计算实现

// *** 多目标传播声数据缓存更新 ***
void DeviceModel::updateMultiTargetPropagatedSoundCache(CSimMessage* simMessage)
{
    if (!simMessage || !simMessage->data) {
        LOG_WARN("Invalid propagated sound message");
        return;
    }

    const CMsg_PropagatedContinuousSoundListStruct* soundListStruct =
        reinterpret_cast<const CMsg_PropagatedContinuousSoundListStruct*>(simMessage->data);

    LOG_INFOF("Updating multi-target propagated sound cache, sound count: %zu",
              soundListStruct->propagatedContinuousList.size());

    // 为每个声纳清空过期的目标数据
    int64 currentTime = simMessage->time;
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];

        // 移除过期数据（超过5秒未更新）
        targetsData.erase(
            std::remove_if(targetsData.begin(), targetsData.end(),
                [currentTime](const TargetData& target) {
                    return (currentTime - target.lastUpdateTime) > DATA_UPDATE_INTERVAL_MS;
                }),
            targetsData.end()
        );
    }

    // 处理新的目标数据
    int targetIndex = 0;
    for (const auto& soundData : soundListStruct->propagatedContinuousList) {
        // 为每个目标分配一个唯一ID
        int targetId = targetIndex + 1000; // 从1000开始编号避免与其他ID冲突

        // 计算目标相对于本艇的位置
        float targetBearing = soundData.arrivalSideAngle;
        float targetDistance = soundData.targetDistance;

        LOG_INFOF("Processing target %d: bearing=%.1f°, distance=%.1fm",
                  targetId, targetBearing, targetDistance);

        // 为每个声纳判断该目标是否在探测范围内
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            // 检查声纳是否启用
            auto stateIt = m_sonarStates.find(sonarID);
            if (stateIt == m_sonarStates.end() ||
                !stateIt->second.arrayWorkingState ||
                !stateIt->second.passiveWorkingState) {
                continue; // 声纳未启用，跳过
            }

            // 检查目标是否在该声纳的探测范围内
            if (!isTargetInSonarRange(sonarID, targetBearing, targetDistance)) {
                continue; // 不在探测范围内，跳过
            }

            auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];

            // 检查是否已达到最大目标数限制
            if (targetsData.size() >= MAX_TARGETS_PER_SONAR) {
                // 如果新目标距离更近，则替换最远的目标
                auto farthestIt = std::max_element(targetsData.begin(), targetsData.end(),
                    [](const TargetData& a, const TargetData& b) {
                        return a.targetDistance < b.targetDistance;
                    });

                if (farthestIt != targetsData.end() && targetDistance < farthestIt->targetDistance) {
                    LOG_INFOF("Replacing farthest target (distance=%.1f) with closer target (distance=%.1f) for sonar %d",
                              farthestIt->targetDistance, targetDistance, sonarID);
                    targetsData.erase(farthestIt);
                } else {
                    LOG_INFOF("Sonar %d target limit reached, skipping target %d", sonarID, targetId);
                    continue; // 新目标不够近，跳过
                }
            }

            // 查找是否已存在相同目标ID的数据
            auto existingIt = std::find_if(targetsData.begin(), targetsData.end(),
                [targetId](const TargetData& target) {
                    return target.targetId == targetId;
                });

            TargetData targetData;
            targetData.targetId = targetId;
            targetData.targetDistance = targetDistance;
            targetData.targetBearing = targetBearing;
            targetData.lastUpdateTime = currentTime;
            targetData.isValid = true;

            // 提取频谱数据
            targetData.propagatedSpectrum.reserve(SPECTRUM_DATA_SIZE);
            for (int i = 0; i < SPECTRUM_DATA_SIZE; i++) {
                targetData.propagatedSpectrum.push_back(soundData.spectrumData[i]);
            }

            if (existingIt != targetsData.end()) {
                // 更新现有目标数据
                *existingIt = targetData;
                LOG_INFOF("Updated existing target %d for sonar %d", targetId, sonarID);
            } else {
                // 添加新目标数据
                targetsData.push_back(targetData);
                LOG_INFOF("Added new target %d for sonar %d (total targets: %zu)",
                          targetId, sonarID, targetsData.size());
            }
        }

        targetIndex++;
    }

    // 输出每个声纳的目标统计
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        const auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];
        LOG_INFOF("Sonar %d now tracking %zu targets", sonarID, targetsData.size());
    }
}

void DeviceModel::updatePlatformSelfSoundCache(CSimData* simData)
{
    if (!simData || !simData->data) {
        LOG_WARN("Invalid platform self sound data");
        return;
    }

    const CData_PlatformSelfSound* selfSound =
        reinterpret_cast<const CData_PlatformSelfSound*>(simData->data);

    LOG_INFOF("Updating platform self sound cache, spectrum count: %zu",
              selfSound->selfSoundSpectrumList.size());

    // 清空旧的平台噪声数据
    m_multiTargetCache.platformSelfSoundSpectrum.clear();

    // 处理平台自噪声数据列表
    for (const auto& spectrumStruct : selfSound->selfSoundSpectrumList) {
        int sonarID = spectrumStruct.sonarID;

        // 验证声纳ID范围 (0-3对应艏端、舷侧、粗拖、细拖)
        if (sonarID < 0 || sonarID >= 4) {
            LOG_WARNF("Invalid sonar ID in platform self sound: %d", sonarID);
            continue;
        }

        // 提取频谱数据
        std::vector<float> spectrum;
        spectrum.reserve(SPECTRUM_DATA_SIZE);
        for (int i = 0; i < SPECTRUM_DATA_SIZE; i++) {
            spectrum.push_back(spectrumStruct.spectumData[i]);
        }

        // 存储到对应声纳的缓存中
        m_multiTargetCache.platformSelfSoundSpectrum[sonarID] = spectrum;

        LOG_INFOF("Updated platform self sound cache for sonar %d", sonarID);
    }

    m_multiTargetCache.lastPlatformSoundTime = simData->time;
    LOG_INFOF("Platform self sound time updated to: %lld", simData->time);
}

void DeviceModel::updateEnvironmentNoiseCache(CSimMessage* simMessage)
{
    if (!simMessage || !simMessage->data) {
        LOG_WARN("Invalid environment noise message");
        return;
    }

    const CMsg_EnvironmentNoiseToSonarStruct* noiseData =
        reinterpret_cast<const CMsg_EnvironmentNoiseToSonarStruct*>(simMessage->data);

    LOG_INFO("Updating environment noise cache");

    // 提取环境噪声频谱数据
    std::vector<float> spectrum;
    spectrum.reserve(SPECTRUM_DATA_SIZE);
    for (int i = 0; i < SPECTRUM_DATA_SIZE; i++) {
        spectrum.push_back(noiseData->spectrumData[i]);
    }

    // 环境噪声对所有声纳位置都是相同的，为每个声纳ID都存储一份
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        m_multiTargetCache.environmentNoiseSpectrum[sonarID] = spectrum;
    }

    LOG_INFOF("Updated environment noise cache for all sonars, spectrum size: %zu",
              spectrum.size());

    m_multiTargetCache.lastEnvironmentNoiseTime = simMessage->time;
    LOG_INFOF("Environment noise time updated to: %lld", simMessage->time);
}

double DeviceModel::calculateSpectrumSum(const std::vector<float>& spectrum)
{
    if (spectrum.empty()) {
        return 0.0;
    }

    double sum = 0.0;
    for (const float& value : spectrum) {
        sum += static_cast<double>(value);
    }

    return sum;
}

double DeviceModel::calculateDI(int sonarID)
{
    // 检查声纳ID是否有效
    if (m_diParameters.find(sonarID) == m_diParameters.end()) {
        LOG_WARNF("No DI parameters found for sonar %d, using default", sonarID);
        return 9.5;  // 默认值
    }

    const DIParameters& params = m_diParameters[sonarID];
    double frequency = std::min(params.frequency_khz, MAX_FREQUENCY_KHZ);  // 限制5kHz上限

    // 根据声纳类型使用不同的计算公式
    double multiplier;
    if (sonarID == 0 || sonarID == 1) {
        // 艏端声纳(ID=0)和舷侧声纳(ID=1): DI = 20lg(f) + offset
        multiplier = 20.0;
    } else if (sonarID == 2 || sonarID == 3) {
        // 粗拖声纳(ID=2)和细拖声纳(ID=3): DI = 10lg(f) + offset
        multiplier = 10.0;
    } else {
        // 其他声纳使用默认20倍系数
        multiplier = 20.0;
    }

    if (frequency > 0.0) {
        double di = multiplier * log10(frequency) + params.offset;
        LOG_DEBUGF("Calculated DI for sonar %d: f=%.1fkHz, multiplier=%.0f, offset=%.2f, DI=%.2f",
                   sonarID, frequency, multiplier, params.offset, di);
        return di;
    } else {
        return params.offset;  // 频率为0时只返回偏移量
    }
}

double DeviceModel::calculateTargetSonarEquation(int sonarID, const TargetData& targetData)
{
    // 检查目标数据有效性
    if (!targetData.isValid || targetData.propagatedSpectrum.empty()) {
        LOG_WARNF("Invalid target data for sonar %d, target %d", sonarID, targetData.targetId);
        return 0.0;
    }

    // 检查平台自噪声和环境噪声数据
    auto platformIt = m_multiTargetCache.platformSelfSoundSpectrum.find(sonarID);
    auto environmentIt = m_multiTargetCache.environmentNoiseSpectrum.find(sonarID);

    if (platformIt == m_multiTargetCache.platformSelfSoundSpectrum.end() ||
        environmentIt == m_multiTargetCache.environmentNoiseSpectrum.end()) {
        LOG_WARNF("Missing platform or environment noise data for sonar %d", sonarID);
        return 0.0;
    }

    // 计算频谱累加求和
    double propagatedSum = calculateSpectrumSum(targetData.propagatedSpectrum);     // |阵元谱级|
    double platformSum = calculateSpectrumSum(platformIt->second);                  // |平台背景|
    double environmentSum = calculateSpectrumSum(environmentIt->second);            // |海洋噪声|

    // 计算SL-TL-NL = 10lg |阵元谱级|^2/(|平台背景|^2+|海洋噪声|^2)
    double propagatedSquare = propagatedSum * propagatedSum;            // |阵元谱级|^2
    double platformSquare = platformSum * platformSum;                 // |平台背景|^2
    double environmentSquare = environmentSum * environmentSum;         // |海洋噪声|^2

    double denominator = platformSquare + environmentSquare;
    double sl_tl_nl = 0.0;

    if (denominator > 0.0 && propagatedSquare > 0.0) {
        sl_tl_nl = 10.0 * log10(propagatedSquare / denominator);
    } else {
        LOG_WARNF("Invalid spectrum data for sonar %d target %d: propagated=%.2f, platform=%.2f, environment=%.2f",
                  sonarID, targetData.targetId, propagatedSum, platformSum, environmentSum);
        return 0.0;
    }

    // 计算DI值
    double di = calculateDI(sonarID);

    // 计算最终结果 X = SL-TL-NL + DI
    double result = sl_tl_nl + di;

    LOG_DEBUGF("Target sonar equation result for sonar %d target %d: SL-TL-NL=%.2f, DI=%.2f, X=%.2f",
               sonarID, targetData.targetId, sl_tl_nl, di, result);

    return result;
}

void DeviceModel::performMultiTargetSonarEquationCalculation()
{
    LOG_DEBUG("Performing multi-target sonar equation calculation for all sonars");

    // 清空之前的计算结果
    m_multiTargetCache.multiTargetEquationResults.clear();

    // 为每个声纳位置计算所有目标的声纳方程
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        // 检查该声纳是否启用
        auto stateIt = m_sonarStates.find(sonarID);
        if (stateIt == m_sonarStates.end() ||
            !stateIt->second.arrayWorkingState ||
            !stateIt->second.passiveWorkingState) {
            LOG_INFOF("Sonar %d is disabled, skipping calculation", sonarID);
            continue;
        }

        LOG_INFOF("Sonar %d is enabled, calculating equations for all targets", sonarID);

        const auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];
        std::vector<TargetEquationResult> sonarResults;

        for (const auto& targetData : targetsData) {
            // 计算该目标的声纳方程
            double result = calculateTargetSonarEquation(sonarID, targetData);

            TargetEquationResult targetResult;
            targetResult.targetId = targetData.targetId;
            targetResult.equationResult = result;
            targetResult.targetDistance = targetData.targetDistance;
            targetResult.targetBearing = targetData.targetBearing;
            targetResult.isValid = (result > 0.0);

            sonarResults.push_back(targetResult);

            if (targetResult.isValid) {
                LOG_INFOF("Sonar %d target %d equation calculated: X=%.2f (distance=%.1fm, bearing=%.1f°)",
                          sonarID, targetData.targetId, result, targetData.targetDistance, targetData.targetBearing);
            } else {
                LOG_WARNF("Sonar %d target %d equation calculation failed", sonarID, targetData.targetId);
            }
        }

        // 存储该声纳的所有目标结果
        m_multiTargetCache.multiTargetEquationResults[sonarID] = sonarResults;

        LOG_INFOF("Sonar %d completed calculation for %zu targets", sonarID, sonarResults.size());
    }
}

bool DeviceModel::isTargetInSonarRange(int sonarID, float targetBearing, float targetDistance)
{
    // 检查距离范围
    if (targetDistance <= 0 || targetDistance > MAX_DETECTION_RANGE) {
        return false;
    }

    // 获取声纳的探测角度范围
    auto angleRange = getSonarDetectionAngleRange(sonarID);
    float startAngle = angleRange.first;
    float endAngle = angleRange.second;

    // 将角度标准化到0-360度范围
    auto normalizeAngle = [](float angle) -> float {
        while (angle < 0) angle += 360.0f;
        while (angle >= 360) angle -= 360.0f;
        return angle;
    };

    float normalizedTargetBearing = normalizeAngle(targetBearing);
    float normalizedStartAngle = normalizeAngle(startAngle);
    float normalizedEndAngle = normalizeAngle(endAngle);

    // 检查目标是否在角度范围内
    bool inRange = false;
    if (normalizedStartAngle <= normalizedEndAngle) {
        // 角度范围不跨越0度
        inRange = (normalizedTargetBearing >= normalizedStartAngle &&
                  normalizedTargetBearing <= normalizedEndAngle);
    } else {
        // 角度范围跨越0度
        inRange = (normalizedTargetBearing >= normalizedStartAngle ||
                  normalizedTargetBearing <= normalizedEndAngle);
    }

    LOG_DEBUGF("Target bearing %.1f° in sonar %d range [%.1f°, %.1f°]: %s",
               targetBearing, sonarID, startAngle, endAngle, inRange ? "YES" : "NO");

    return inRange;
}

std::pair<float, float> DeviceModel::getSonarDetectionAngleRange(int sonarID)
{
    switch (sonarID) {
        case 0: // 艏端声纳 - 前向扇形区域
            return std::make_pair(315.0f, 45.0f);  // -45° to +45° (相对艏向)

        case 1: // 舷侧声纳 - 两段式：左舷和右舷
            // 注意：舷侧声纳实际上需要特殊处理，这里简化为左舷范围
            // 在实际应用中可能需要分别处理左舷和右舷
            return std::make_pair(90.0f, 270.0f);  // 左舷90°到右舷270°

        case 2: // 粗拖声纳 - 后向扇形区域
            return std::make_pair(135.0f, 225.0f); // 后方90度扇形

        case 3: // 细拖声纳 - 后向扇形区域
            return std::make_pair(120.0f, 240.0f); // 后方120度扇形

        default:
            LOG_WARNF("Unknown sonar ID %d, using default range", sonarID);
            return std::make_pair(0.0f, 360.0f);   // 全方位
    }
}

// *** 公共接口实现 ***
std::vector<DeviceModel::TargetEquationResult> DeviceModel::getSonarTargetsResults(int sonarID)
{
    auto it = m_multiTargetCache.multiTargetEquationResults.find(sonarID);
    if (it != m_multiTargetCache.multiTargetEquationResults.end()) {
        return it->second;
    }
    return std::vector<TargetEquationResult>();
}

std::map<int, std::vector<DeviceModel::TargetEquationResult>> DeviceModel::getAllSonarTargetsResults()
{
    return m_multiTargetCache.multiTargetEquationResults;
}

int DeviceModel::getSonarDetectedTargetCount(int sonarID)
{
    auto it = m_multiTargetCache.multiTargetEquationResults.find(sonarID);
    if (it != m_multiTargetCache.multiTargetEquationResults.end()) {
        // 只统计有效结果的目标
        int validCount = 0;
        for (const auto& result : it->second) {
            if (result.isValid) {
                validCount++;
            }
        }
        return validCount;
    }
    return 0;
}

void DeviceModel::setDIParameters(int sonarID, double frequency_khz, double offset)
{
    if (sonarID >= 0 && sonarID < 4) {
        m_diParameters[sonarID] = {frequency_khz, offset};
        LOG_INFOF("Updated DI parameters for sonar %d: f=%.1fkHz, offset=%.1f",
                  sonarID, frequency_khz, offset);
    } else {
        LOG_WARNF("Invalid sonar ID for DI parameters: %d", sonarID);
    }
}

bool DeviceModel::isEquationDataValid(int sonarID)
{
    int64 currentTime = QDateTime::currentMSecsSinceEpoch();

    // 检查平台自噪声和环境噪声数据时效性
    bool platformValid = (currentTime - m_multiTargetCache.lastPlatformSoundTime) <= DATA_UPDATE_INTERVAL_MS;
    bool environmentValid = (currentTime - m_multiTargetCache.lastEnvironmentNoiseTime) <= DATA_UPDATE_INTERVAL_MS;

    // 检查是否有目标数据
    auto it = m_multiTargetCache.sonarTargetsData.find(sonarID);
    bool hasTargets = (it != m_multiTargetCache.sonarTargetsData.end() && !it->second.empty());

    return platformValid && environmentValid && hasTargets;
}

// *** 兼容性接口实现 ***
double DeviceModel::getSonarEquationResult(int sonarID)
{
    auto results = getSonarTargetsResults(sonarID);
    if (!results.empty()) {
        // 返回第一个有效目标的结果
        for (const auto& result : results) {
            if (result.isValid) {
                return result.equationResult;
            }
        }
    }
    return 0.0;
}

std::map<int, double> DeviceModel::getAllSonarEquationResults()
{
    std::map<int, double> compatResults;
    auto allResults = getAllSonarTargetsResults();

    for (const auto& sonarResults : allResults) {
        int sonarID = sonarResults.first;
        const auto& targets = sonarResults.second;

        // 返回第一个有效目标的结果
        for (const auto& target : targets) {
            if (target.isValid) {
                compatResults[sonarID] = target.equationResult;
                break;
            }
        }
    }

    return compatResults;
}

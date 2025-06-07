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
#include <QMap>

constexpr const double DeviceModel::MAX_FREQUENCY_KHZ;

DeviceModel::DeviceModel()
{
    m_agent = nullptr; // CSimModelAgentBase 代理对象

    m_initialized = false; // 声纳状态信息 初始化标志

    // 初始化多目标缓存
    m_multiTargetCache = MultiTargetSonarEquationCache();

    // 初始化平台机动信息
    m_platformMotion = CData_Motion();
    m_platformMotion.action = true;
    m_platformMotion.isPending = false;

    // 生成带时间戳的日志文件名     // 配置 DMLogger 同时输出到控制台和文件
    // 只在 Logger 未初始化时才初始化
     if (!Logger::getInstance().isInitialized()) {  // 需要添加这个方法
         QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
         QString logFileName = QString("device_model_%1.log").arg(timestamp);
         Logger::getInstance().initialize(logFileName.toStdString(), true);
     }

    LOG_INFO("Sonar model created with multi-target equation calculation capability");

    // 尝试加载配置文件，如果不存在则使用默认值
    loadExtendedConfig("threshold_config.ini");

    LOG_INFO("声纳模型已创建，支持可配置探测阈值");
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

void DeviceModel::onMessage(CSimMessage* simMessage)
{
    if (!simMessage || !m_agent) {
       std::cout << __FUNCTION__ << ":" << __LINE__ << " simMessage or m_agent is null!" << std::endl;
       return;
   }

   // 根据消息主题进行相应处理
   std::string topic = simMessage->topic;

   // 对特定消息主题进行5秒间隔日志控制
   if (topic == MSG_PropagatedContinuousSound ||
       topic == MSG_EnvironmentNoiseToSonar ||
       topic == Data_PlatformSelfSound) {

       int64 currentTime = QDateTime::currentMSecsSinceEpoch();
       bool shouldLog = false;

       if (topic == MSG_PropagatedContinuousSound) {
           if (currentTime - m_lastPropagatedSoundLogTime >= PROPAGATED_SOUND_LOG_INTERVAL) {
               m_lastPropagatedSoundLogTime = currentTime;
               shouldLog = true;
           }
       } else if (topic == MSG_EnvironmentNoiseToSonar) {
           if (currentTime - m_lastEnvironmentNoiseLogTime >= PROPAGATED_SOUND_LOG_INTERVAL) {
               m_lastEnvironmentNoiseLogTime = currentTime;
               shouldLog = true;
           }
       } else if (topic == Data_PlatformSelfSound) {
           if (currentTime - m_lastPlatformSelfSoundLogTime >= PROPAGATED_SOUND_LOG_INTERVAL) {
               m_lastPlatformSelfSoundLogTime = currentTime;
               shouldLog = true;
           }
       }

       if (shouldLog) {
           LOG_EMPTY("");
           LOG_INFOF("==========onMessage!!! Topic: %s", topic.c_str());
           LOG_INFOF("Received message with topic: %s", topic.c_str());
       }
   } else {
       // 其他消息正常打印
       LOG_EMPTY("");
       LOG_INFOF("==========onMessage!!! Topic: %s", topic.c_str());
       LOG_INFOF("Received message with topic: %s", topic.c_str());
   }






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
//        handlePropagatedSound(simMessage);
        std::string topic = simMessage->topic;

        // 使用新的多目标处理方法
        if (topic == MSG_PropagatedContinuousSound) {
            updateMultiTargetPropagatedSoundCache(simMessage);
        }

        else if (topic == MSG_PropagatedActivePulseSound) {
            const CMsg_PropagatedActivePulseSoundListStruct* sounds = reinterpret_cast<const CMsg_PropagatedActivePulseSoundListStruct*>(simMessage->data);
            std::cout << __FUNCTION__ << ":" << __LINE__ << " Received active pulse sound data, count: " << sounds->propagateActivePulseList.size() << std::endl;
        }
        else if (topic == MSG_PropagatedCommPulseSound) {
            const CMsg_PropagatedCommPulseSoundListStruct* sounds = reinterpret_cast<const CMsg_PropagatedCommPulseSoundListStruct*>(simMessage->data);
            std::cout << __FUNCTION__ << ":" << __LINE__ << " Received comm pulse sound data, count: " << sounds->propagatedCommList.size() << std::endl;
        }
    }
    else if (topic == MSG_EnvironmentNoiseToSonar) {
        // 处理环境噪声数据
        updateEnvironmentNoiseCache(simMessage);
    }
    else {
        std::cout << __FUNCTION__ << ":" << __LINE__ << " Unknown message topic: " << topic << std::endl;
    }
}

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

    int64 currentTime = simMessage->time;

    // 修改：如果接收到空的目标列表，立即清空所有声纳的目标数据
    if (soundListStruct->propagatedContinuousList.empty()) {
        LOG_INFO("Received empty target list, clearing all sonar target data");

        for (int sonarID = 0; sonarID < 4; sonarID++) {
            m_multiTargetCache.sonarTargetsData[sonarID].clear();
            m_multiTargetCache.multiTargetEquationResults[sonarID].clear();
        }

//        LOG_INFO("All sonar target data cleared");
        return;
    }

    // 为每个声纳清空过期的目标数据
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

        // 获取目标的绝对方位角和距离
        float targetBearing = soundData.arrivalSideAngle;  // 这是绝对方位角
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

            // 使用修复后的角度范围检查方法
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
                // 新目标数据
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
    LOG_EMPTY("");
}
bool DeviceModel::isTargetInSonarRange(int sonarID, float targetBearing, float targetDistance)
{
    // 检查距离范围
    if (targetDistance <= 0 || targetDistance > MAX_DETECTION_RANGE) {
        return false;
    }

    // 获取本艇当前航向
    float ownShipHeading = static_cast<float>(m_platformMotion.rotation);

    LOG_INFOF("=== Sonar Range Check Debug ===");
    LOG_INFOF("SonarID: %d, Target bearing: %.1f°, Target distance: %.1fm",
              sonarID, targetBearing, targetDistance);
    LOG_INFOF("Own ship heading: %.1f°", ownShipHeading);

    // 计算相对目标方位
    float relativeTargetBearing = targetBearing - ownShipHeading;

    // 标准化相对角度到-180~180度范围
    while (relativeTargetBearing > 180.0f) relativeTargetBearing -= 360.0f;
    while (relativeTargetBearing <= -180.0f) relativeTargetBearing += 360.0f;

    LOG_INFOF("Relative target bearing: %.1f°", relativeTargetBearing);

    bool inRange = false;

    // 特殊处理舷侧声纳（ID=1）- 覆盖左右两侧
    if (sonarID == 1) {
        // 左舷范围：45° to 135°（右前方到右后方）
        bool inStarboardRange = (relativeTargetBearing >= 45.0f && relativeTargetBearing <= 135.0f);

        // 右舷范围：-135° to -45°（左后方到左前方）
        bool inPortRange = (relativeTargetBearing >= -135.0f && relativeTargetBearing <= -45.0f);

        inRange = inStarboardRange || inPortRange;

        LOG_INFOF("Sonar %d (舷侧) range check:", sonarID);
        LOG_INFOF("  Starboard range (45°-135°): %s", inStarboardRange ? "true" : "false");
        LOG_INFOF("  Port range (-135°--45°): %s", inPortRange ? "true" : "false");
        LOG_INFOF("  Final result: %s", inRange ? "IN RANGE" : "OUT OF RANGE");
    }
    else {
        // 其他声纳使用原有逻辑
        auto relativeRange = getRelativeSonarAngleRange(sonarID);
        float relativeStart = relativeRange.first;
        float relativeEnd = relativeRange.second;

        LOG_INFOF("Sonar %d relative range: [%.1f°, %.1f°]", sonarID, relativeStart, relativeEnd);

        // 标准化声纳范围角度
        float normalizedStart = relativeStart;
        float normalizedEnd = relativeEnd;

        while (normalizedStart > 180.0f) normalizedStart -= 360.0f;
        while (normalizedStart <= -180.0f) normalizedStart += 360.0f;
        while (normalizedEnd > 180.0f) normalizedEnd -= 360.0f;
        while (normalizedEnd <= -180.0f) normalizedEnd += 360.0f;

        if (normalizedStart <= normalizedEnd) {
            // 正常情况，不跨越±180度边界
            inRange = (relativeTargetBearing >= normalizedStart && relativeTargetBearing <= normalizedEnd);
        } else {
            // 跨越±180度边界的情况
            inRange = (relativeTargetBearing >= normalizedStart || relativeTargetBearing <= normalizedEnd);
        }

        LOG_INFOF("Normal range check result: %s", inRange ? "IN RANGE" : "OUT OF RANGE");
    }

    LOG_INFOF("=== End Debug ===");
    return inRange;
}
std::pair<float, float> DeviceModel::getRelativeSonarAngleRange(int sonarID)
{
    switch (sonarID) {
        case 0: // 艏端声纳 - 前向扇形区域（相对艏向 -45° to +45°）
            return std::make_pair(-45.0f, 45.0f);

        case 2: // 粗拖声纳 - 后向扇形区域（相对艏向后方90度扇形）
            return std::make_pair(135.0f, 225.0f);

        case 3: // 细拖声纳 - 后向扇形区域（相对艏向后方120度扇形）
            return std::make_pair(120.0f, 240.0f);

        default:
            LOG_WARNF("Unknown sonar ID %d, using default range", sonarID);
            return std::make_pair(0.0f, 360.0f);
    }
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
        m_multiTargetCache.environmentNoiseSpectrumMap[sonarID] = spectrum;
    }

    LOG_INFOF("Updated environment noise cache for all sonars, spectrum size: %zu",
              spectrum.size());

    m_multiTargetCache.lastEnvironmentNoiseTime = simMessage->time;
    LOG_INFOF("Environment noise time updated to: %lld", simMessage->time);
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
        updatePlatformSelfSoundCache(selfSoundData);
    } else {
        LOG_WARN("Failed to retrieve platform self sound data in step()");
        LOG_WARNF("Topic: %s, PlatformId: %lld", Data_PlatformSelfSound, platformId);
    }

    // 执行多目标声纳方程计算
    performMultiTargetSonarEquationCalculation();
}

// 平台自噪声数据
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
    m_multiTargetCache.platformSelfSoundSpectrumMap.clear();

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
        m_multiTargetCache.platformSelfSoundSpectrumMap[sonarID] = spectrum;

        LOG_INFOF("Updated platform self sound cache for sonar %d", sonarID);
    }

    m_multiTargetCache.lastPlatformSoundTime = simData->time;
    LOG_INFOF("Platform self sound time updated to: %lld", simData->time);
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

            // 获取当前声纳的有效阈值
            double threshold = getEffectiveThreshold(sonarID);

            TargetEquationResult targetResult;
            targetResult.targetId = targetData.targetId;
            targetResult.equationResult = result;
            targetResult.targetDistance = targetData.targetDistance;
            targetResult.targetBearing = targetData.targetBearing;
            // 使用配置的阈值进行判断
            targetResult.isValid = (result > threshold);

            sonarResults.push_back(targetResult);

            if (targetResult.isValid) {
                LOG_INFOF("声纳%d目标%d探测成功: X=%.2f > 阈值%.2f (距离=%.1fm, 方位=%.1f°)",
                          sonarID, targetData.targetId, result, threshold,
                          targetData.targetDistance, targetData.targetBearing);
            } else {
                LOG_INFOF("声纳%d目标%d探测失败: X=%.2f <= 阈值%.2f (距离=%.1fm, 方位=%.1f°)",
                          sonarID, targetData.targetId, result, threshold,
                          targetData.targetDistance, targetData.targetBearing);
            }
        }

        // 存储该声纳的所有目标结果
        m_multiTargetCache.multiTargetEquationResults[sonarID] = sonarResults;

        LOG_INFOF("Sonar %d completed calculation for %zu targets", sonarID, sonarResults.size());
    }
}
double DeviceModel::calculateTargetSonarEquation(int sonarID, const TargetData& targetCachePropagatedSpectrum)
{
    LOG_INFOF("=== Calculating equation for sonar %d, target %d ===", sonarID, targetCachePropagatedSpectrum.targetId);

    // 检查目标数据有效性
    if (!targetCachePropagatedSpectrum.isValid || targetCachePropagatedSpectrum.propagatedSpectrum.empty()) {
        LOG_WARNF("Invalid target data for sonar %d, target %d - isValid:%d, spectrumSize:%zu",
                  sonarID, targetCachePropagatedSpectrum.targetId, targetCachePropagatedSpectrum.isValid, targetCachePropagatedSpectrum.propagatedSpectrum.size());
        return 0.0;
    }

    // 检查平台自噪声和环境噪声数据
    auto targetCachePlatformSpectrumVector = m_multiTargetCache.platformSelfSoundSpectrumMap.find(sonarID);
    auto targetCacheEnvironmentSpectrumVector = m_multiTargetCache.environmentNoiseSpectrumMap.find(sonarID);

    if (targetCachePlatformSpectrumVector == m_multiTargetCache.platformSelfSoundSpectrumMap.end()) {
        LOG_WARNF("Missing platform noise data for sonar %d", sonarID);
        return 0.0;
    }

    if (targetCacheEnvironmentSpectrumVector == m_multiTargetCache.environmentNoiseSpectrumMap.end()) {
        LOG_WARNF("Missing environment noise data for sonar %d", sonarID);
        return 0.0;
    }

    LOG_INFOF("Sonar %d data check passed - platform spectrum size:%zu, environment spectrum size:%zu, target spectrum size:%zu",
              sonarID, targetCachePlatformSpectrumVector->second.size(), targetCacheEnvironmentSpectrumVector->second.size(), targetCachePropagatedSpectrum.propagatedSpectrum.size());





    // ############# 步骤1：计算频谱累加求和 #############
    double propagatedSum = calculateSpectrumSum(targetCachePropagatedSpectrum.propagatedSpectrum);     // |阵元谱级|
    double platformSum = calculateSpectrumSum(targetCachePlatformSpectrumVector->second);                  // |平台背景|
    double environmentSum = calculateSpectrumSum(targetCacheEnvironmentSpectrumVector->second);            // |海洋噪声|

    LOG_INFOF("Spectrum sums >>>>>> propagated:%.2f, platform:%.2f, environment:%.2f",
                 propagatedSum, platformSum, environmentSum);

    // ############# 步骤2：计算平方值 #############
    // 计算SL-TL-NL = 10lg |阵元谱级|^2/(|平台背景|^2+|海洋噪声|^2)
    double propagatedSquare = propagatedSum * propagatedSum;            // |阵元谱级|^2
    double platformSquare = platformSum * platformSum;                 // |平台背景|^2
    double environmentSquare = environmentSum * environmentSum;         // |海洋噪声|^2

    // ############# 步骤3：计算 SL-TL-NL #############
    double denominator = platformSquare + environmentSquare; // 分母：噪声总和
    double sl_tl_nl = 0.0;

    if (denominator > 0.0 && propagatedSquare > 0.0) {
        sl_tl_nl = 10.0 * log10(propagatedSquare / denominator);  // 10lg(信号/噪声)

    } else {
        LOG_WARNF("Invalid spectrum data for sonar %d target %d >>>>>> propagated=%.2f, platform=%.2f, environment=%.2f",
                  sonarID, targetCachePropagatedSpectrum.targetId, propagatedSum, platformSum, environmentSum);
        return 0.0;
    }

    // ############# 步骤4：计算DI值 #############
    double di = calculateDI(sonarID);

    // ############# 步骤5：计算最终结果 X = SL-TL-NL + DI #############
    double result = sl_tl_nl + di;

    // 获取当前声纳的探测阈值
    double threshold = getEffectiveThreshold(sonarID);

    LOG_DEBUGF("声纳%d目标%d方程计算: X=%.2f, 阈值=%.2f, 可探测=%s",
               sonarID, targetCachePropagatedSpectrum.targetId, result, threshold,
               (result > threshold) ? "是" : "否");

    return result;
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








const char* DeviceModel::getVersion()
{
    return "Multi-Target Sonar Model V2.0.0.20241205";
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

        LOG_INFOF("Updated sonar %d state: array=%d active=%d passive=%d scouting=%d",
                  order->sonarID, state.arrayWorkingState, state.activeWorkingState,
                  state.passiveWorkingState, state.scoutingWorkingState);

        // 发布更新后的声纳状态
        updateSonarState();
    }
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

void DeviceModel::stop()
{
    LOG_INFO("Multi-target sonar model stopped");
}

void DeviceModel::destroy()
{
    LOG_INFO("Multi-target sonar model destroyed");
}






std::map<int, std::vector<DeviceModel::TargetEquationResult>> DeviceModel::getAllSonarTargetsResults()
{
    return m_multiTargetCache.multiTargetEquationResults;
}

void DeviceModel::setFileLogEnabled(bool enabled) {
    Logger::getInstance().enableFileOutput(enabled);
}












void DeviceModel::setSonarDetectionThreshold(int sonarID, double threshold)
{
    if (sonarID < 0 || sonarID >= 4) {
        LOG_WARNF("无效的声纳ID: %d", sonarID);
        return;
    }

    m_detectionThresholds[sonarID] = threshold;
    LOG_INFOF("设置声纳%d探测阈值: %.2f", sonarID, threshold);
}

double DeviceModel::getSonarDetectionThreshold(int sonarID) const
{
    if (sonarID < 0 || sonarID >= 4) {
        LOG_WARNF("无效的声纳ID: %d，返回默认阈值", sonarID);
        return 33.0;
    }

    return getEffectiveThreshold(sonarID);
}

double DeviceModel::getEffectiveThreshold(int sonarID) const
{
    if (sonarID < 0 || sonarID >= 4) {
        return 33.0; // 默认阈值
    }

    auto it = m_detectionThresholds.find(sonarID);
    return (it != m_detectionThresholds.end()) ? it->second : 33.0;
}

void DeviceModel::saveThresholdConfig(const std::string& filename) const
{
    try {
        std::ofstream configFile(filename);
        if (!configFile.is_open()) {
            LOG_ERRORF("无法打开配置文件进行写入: %s", filename.c_str());
            return;
        }

        // 写入配置文件头
        configFile << "# 声纳探测阈值配置文件\n";
        configFile << "# 生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString() << "\n\n";

        // 写入各声纳的阈值
        configFile << "[SonarThresholds]\n";
        const char* sonarNames[] = {"艏端声纳", "舷侧声纳", "粗拖声纳", "细拖声纳"};
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            auto it = m_detectionThresholds.find(sonarID);
            double threshold = (it != m_detectionThresholds.end()) ? it->second : 33.0;
            configFile << "Sonar" << sonarID << "_Threshold=" << std::fixed << std::setprecision(2) << threshold
                      << "  # " << sonarNames[sonarID] << "\n";
        }

        configFile.close();
        LOG_INFOF("阈值配置已保存到: %s", filename.c_str());

    } catch (const std::exception& e) {
        LOG_ERRORF("保存阈值配置时出错: %s", e.what());
    }
}

void DeviceModel::loadThresholdConfig(const std::string& filename)
{
    try {
        std::ifstream configFile(filename);
        if (!configFile.is_open()) {
            LOG_WARNF("无法打开配置文件: %s，使用默认阈值", filename.c_str());
            return;
        }

        std::string line;
        std::string currentSection;

        while (std::getline(configFile, line)) {
            // 跳过空行和注释
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // 检查是否是节标题
            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }

            // 解析键值对
            size_t equalPos = line.find('=');
            if (equalPos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);

            // 移除注释部分
            size_t commentPos = value.find('#');
            if (commentPos != std::string::npos) {
                value = value.substr(0, commentPos);
            }

            // 移除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // 处理声纳阈值设置
            if (currentSection == "SonarThresholds") {
                if (key.substr(0, 5) == "Sonar" && key.substr(6) == "_Threshold") {
                    int sonarID = std::stoi(key.substr(5, 1));
                    if (sonarID >= 0 && sonarID < 4) {
                        m_detectionThresholds[sonarID] = std::stod(value);
                        LOG_INFOF("加载声纳%d阈值: %.2f", sonarID, std::stod(value));
                    }
                }
            }
        }

        configFile.close();
        LOG_INFOF("阈值配置已从文件加载: %s", filename.c_str());

        for (int sonarID = 0; sonarID < 4; sonarID++) {
            LOG_INFOF("声纳%d最终阈值: %.2f", sonarID, getEffectiveThreshold(sonarID));
        }

    } catch (const std::exception& e) {
        LOG_ERRORF("加载阈值配置时出错: %s，使用默认值", e.what());
    }
}


DeviceModel::~DeviceModel()
{
    LOG_INFO("Sonar model destroyed");
}



// 实现扩展配置保存方法
void DeviceModel::saveExtendedConfig(const std::string& filename, const std::map<int, SonarRangeConfig>& sonarRangeMap) const
{
    try {
        std::ofstream configFile(filename);
        if (!configFile.is_open()) {
            LOG_ERRORF("无法打开配置文件进行写入: %s", filename.c_str());
            return;
        }

        // 写入配置文件头
        configFile << "# 声纳探测阈值和角度范围配置文件\n";
        configFile << "# 生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString() << "\n\n";

        // 写入各声纳的阈值
        configFile << "[SonarThresholds]\n";
        const char* sonarNames[] = {"艏端声纳", "舷侧声纳", "粗拖声纳", "细拖声纳"};
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            auto it = m_detectionThresholds.find(sonarID);
            double threshold = (it != m_detectionThresholds.end()) ? it->second : 33.0;
            configFile << "Sonar" << sonarID << "_Threshold=" << std::fixed << std::setprecision(2) << threshold
                      << "  # " << sonarNames[sonarID] << "\n";
        }

        // 写入声纳角度范围配置
        configFile << "\n[SonarAngles]\n";
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            auto it = m_sonarAngleConfigs.find(sonarID);
            if (it != m_sonarAngleConfigs.end()) {
                const SonarAngleConfig& config = it->second;
                configFile << "Sonar" << sonarID << "_StartAngle1=" << std::fixed << std::setprecision(1) << config.startAngle1
                          << "  # " << sonarNames[sonarID] << " 起始角度1\n";
                configFile << "Sonar" << sonarID << "_EndAngle1=" << std::fixed << std::setprecision(1) << config.endAngle1
                          << "  # " << sonarNames[sonarID] << " 结束角度1\n";

                if (config.hasTwoSegments) {
                    configFile << "Sonar" << sonarID << "_StartAngle2=" << std::fixed << std::setprecision(1) << config.startAngle2
                              << "  # " << sonarNames[sonarID] << " 起始角度2\n";
                    configFile << "Sonar" << sonarID << "_EndAngle2=" << std::fixed << std::setprecision(1) << config.endAngle2
                              << "  # " << sonarNames[sonarID] << " 结束角度2\n";
                    configFile << "Sonar" << sonarID << "_HasTwoSegments=true"
                              << "  # " << sonarNames[sonarID] << " 是否有两个分段\n";
                } else {
                    configFile << "Sonar" << sonarID << "_HasTwoSegments=false"
                              << "  # " << sonarNames[sonarID] << " 是否有两个分段\n";
                }
            }
        }

        // 写入声纳最大显示距离配置
        configFile << "\n[SonarRanges]\n";
        for (const auto& pair : sonarRangeMap) {
            int sonarID = pair.first;
            const SonarRangeConfig& rangeConfig = pair.second;
            if (sonarID >= 0 && sonarID < 4) {
                configFile << "Sonar" << sonarID << "_MaxRange=" << std::fixed << std::setprecision(0) << rangeConfig.maxRange
                          << "  # " << sonarNames[sonarID] << " 最大显示距离(米)\n";
            }
        }

        configFile.close();
        LOG_INFOF("扩展配置已保存到: %s", filename.c_str());

    } catch (const std::exception& e) {
        LOG_ERRORF("保存扩展配置时出错: %s", e.what());
    }
}

// 实现扩展配置加载方法（包括加载显示距离配置）
void DeviceModel::loadExtendedConfig(const std::string& filename)
{
    try {
        std::ifstream configFile(filename);
        if (!configFile.is_open()) {
            LOG_WARNF("无法打开配置文件: %s，使用默认配置", filename.c_str());
            return;
        }

        std::string line;
        std::string currentSection;

        // 临时存储加载的角度配置
        std::map<int, SonarAngleConfig> tempAngleConfigs;

        while (std::getline(configFile, line)) {
            // 跳过空行和注释
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // 检查是否是节标题
            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }

            // 解析键值对
            size_t equalPos = line.find('=');
            if (equalPos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);

            // 移除注释部分
            size_t commentPos = value.find('#');
            if (commentPos != std::string::npos) {
                value = value.substr(0, commentPos);
            }

            // 移除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // 处理声纳阈值设置
            if (currentSection == "SonarThresholds") {
                if (key.substr(0, 5) == "Sonar" && key.substr(6) == "_Threshold") {
                    int sonarID = std::stoi(key.substr(5, 1));
                    if (sonarID >= 0 && sonarID < 4) {
                        m_detectionThresholds[sonarID] = std::stod(value);
                        LOG_INFOF("加载声纳%d阈值: %.2f", sonarID, std::stod(value));
                    }
                }
            }
            // 处理声纳角度范围设置
            else if (currentSection == "SonarAngles") {
                if (key.substr(0, 5) == "Sonar" && key.length() > 6) {
                    int sonarID = std::stoi(key.substr(5, 1));
                    if (sonarID >= 0 && sonarID < 4) {
                        // 确保临时配置结构存在
                        if (tempAngleConfigs.find(sonarID) == tempAngleConfigs.end()) {
                            tempAngleConfigs[sonarID] = SonarAngleConfig();
                        }

                        std::string paramName = key.substr(7); // 从第7个字符开始

                        if (paramName == "_StartAngle1") {
                            tempAngleConfigs[sonarID].startAngle1 = std::stof(value);
                        } else if (paramName == "_EndAngle1") {
                            tempAngleConfigs[sonarID].endAngle1 = std::stof(value);
                        } else if (paramName == "_StartAngle2") {
                            tempAngleConfigs[sonarID].startAngle2 = std::stof(value);
                        } else if (paramName == "_EndAngle2") {
                            tempAngleConfigs[sonarID].endAngle2 = std::stof(value);
                        } else if (paramName == "_HasTwoSegments") {
                            tempAngleConfigs[sonarID].hasTwoSegments = (value == "true" || value == "1");
                        }
                    }
                }
            }
            // 处理声纳最大显示距离设置
//            else if (currentSection == "SonarRanges") {
//                if (key.substr(0, 5) == "Sonar" && key.substr(6) == "_MaxRange") {
//                    int sonarID = std::stoi(key.substr(5, 1));
//                    if (sonarID >= 0 && sonarID < 4) {
//                        m_sonarMaxDisplayRanges[sonarID] = std::stof(value);
//                        LOG_INFOF("加载声纳%d最大显示距离: %.0f米", sonarID, std::stof(value));
//                    }
//                }
//            }
        }

        configFile.close();

        // 应用加载的角度配置
        for (const auto& pair : tempAngleConfigs) {
            m_sonarAngleConfigs[pair.first] = pair.second;
            const SonarAngleConfig& config = pair.second;
            LOG_INFOF("加载声纳%d角度配置: [%.1f°-%.1f°]%s",
                      pair.first, config.startAngle1, config.endAngle1,
                      config.hasTwoSegments ? " + [" + std::to_string(config.startAngle2) + "°-" + std::to_string(config.endAngle2) + "°]" : "");
        }

        LOG_INFOF("扩展配置已从文件加载: %s", filename.c_str());

    } catch (const std::exception& e) {
        LOG_ERRORF("加载扩展配置时出错: %s，使用默认值", e.what());
    }
}

// 实现声纳角度配置的设置和获取方法
void DeviceModel::setSonarAngleConfig(int sonarID, const SonarAngleConfig& config)
{
    if (sonarID >= 0 && sonarID < 4) {
        m_sonarAngleConfigs[sonarID] = config;
        LOG_INFOF("设置声纳%d角度配置: [%.1f°-%.1f°]%s",
                  sonarID, config.startAngle1, config.endAngle1,
                  config.hasTwoSegments ? " + [" + std::to_string(config.startAngle2) + "°-" + std::to_string(config.endAngle2) + "°]" : "");
    } else {
        LOG_WARNF("无效的声纳ID: %d", sonarID);
    }
}

DeviceModel::SonarAngleConfig DeviceModel::getSonarAngleConfig(int sonarID) const
{
    auto it = m_sonarAngleConfigs.find(sonarID);
    if (it != m_sonarAngleConfigs.end()) {
        return it->second;
    }

    LOG_WARNF("无效的声纳ID: %d，返回默认角度配置", sonarID);
    return SonarAngleConfig();
}

// 获取所有声纳的角度配置
std::map<int, DeviceModel::SonarAngleConfig> DeviceModel::getAllSonarAngleConfigs() const
{
    return m_sonarAngleConfigs;
}

// 设置和获取声纳最大显示距离
void DeviceModel::setSonarMaxDisplayRange(int sonarID, float maxRange)
{
    if (sonarID >= 0 && sonarID < 4) {
        m_sonarMaxDisplayRanges[sonarID] = maxRange;
        LOG_INFOF("设置声纳%d最大显示距离: %.0f米", sonarID, maxRange);
    } else {
        LOG_WARNF("无效的声纳ID: %d", sonarID);
    }
}

float DeviceModel::getSonarMaxDisplayRange(int sonarID) const
{
    auto it = m_sonarMaxDisplayRanges.find(sonarID);
    if (it != m_sonarMaxDisplayRanges.end()) {
        return it->second;
    }

    LOG_WARNF("无效的声纳ID: %d，返回默认显示距离", sonarID);
    return 30000.0f; // 默认30km
}

// 获取所有声纳的显示距离配置
std::map<int, float> DeviceModel::getAllSonarMaxDisplayRanges() const
{
    return m_sonarMaxDisplayRanges;
}

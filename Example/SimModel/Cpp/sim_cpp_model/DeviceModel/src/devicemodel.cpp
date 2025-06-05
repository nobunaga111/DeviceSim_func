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

    //***********
    m_equationCache = SonarEquationCache();
    //***********

    LOG_INFO("Sonar model created with equation calculation capability");

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
}

bool DeviceModel::init(CSimModelAgentBase* simModelAgent, CSimComponentAttribute* attr)
{
    LOG_INFO("Sonar model init");
    (void)attr;

    if (!simModelAgent)
    {
        LOG_WARN("simModelAgent is null!");
        return false;
    }
    m_agent = simModelAgent;


    initDetectionTrack();

    return true;
}

void DeviceModel::start()
{
    LOG_INFO("Sonar model started");

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
    LOG_INFO("Sonar model stopped");
}

void DeviceModel::destroy()
{
    LOG_INFO("Sonar model destroyed");
}

void DeviceModel::onMessage(CSimMessage* simMessage)
{
    LOG_INFO("onMessage!!!1");
    if (!simMessage || !m_agent) {
        std::cout << __FUNCTION__ << ":" << __LINE__ << " simMessage or m_agent is null!" << std::endl;
        return;
    }

    // 根据消息主题进行相应处理
    std::string topic = simMessage->topic;
    log<<"onMessage!!!Topic:"<<topic<<"\n";
    LOG_INFOF("onMessage!!!2 Received message with topic: %s, comparing with: %s",
              topic.c_str(), MSG_PropagatedContinuousSound);

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
        LOG_INFO("onMessage!!!3");
        handlePropagatedSound(simMessage);
    }
    else if (topic == MSG_EnvironmentNoiseToSonar) {

        //*******************
        updateEnvironmentNoiseCache(simMessage);
        //*******************

        // 处理环境噪声数据
        const CMsg_EnvironmentNoiseToSonarStruct* noiseData =
            reinterpret_cast<const CMsg_EnvironmentNoiseToSonarStruct*>(simMessage->data);

        // 环境噪声等待处理

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

    //*******************
    if (topic == MSG_PropagatedContinuousSound) {
        updatePropagatedSoundCache(simMessage);
    }
    //*******************

    // 根据消息类型处理传播声音数据
    if (topic == MSG_PropagatedContinuousSound) {
        const CMsg_PropagatedContinuousSoundListStruct* soundListStruct =
            reinterpret_cast<const CMsg_PropagatedContinuousSoundListStruct*>(simMessage->data);
        LOG_INFOF("onMessage!!! handlePropagatedSound Received continuous sound data, count: %d", soundListStruct->propagatedContinuousList.size());
        if(ifOutput){
            log << __FUNCTION__ << ":" << __LINE__
                      << " Received continuous sound data, count: "
                      << soundListStruct->propagatedContinuousList.size() << std::endl;
            log.flush();
        }

        // 等待进一步处理

    }


    else if (topic == MSG_PropagatedActivePulseSound) {
        const CMsg_PropagatedActivePulseSoundListStruct* sounds =
            reinterpret_cast<const CMsg_PropagatedActivePulseSoundListStruct*>(simMessage->data);

        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Received active pulse sound data, count: "
                  << sounds->propagateActivePulseList.size() << std::endl;

        // 等待进一步处理

    }
    else if (topic == MSG_PropagatedCommPulseSound) {
        const CMsg_PropagatedCommPulseSoundListStruct* sounds =
            reinterpret_cast<const CMsg_PropagatedCommPulseSoundListStruct*>(simMessage->data);

        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Received comm pulse sound data, count: "
                  << sounds->propagatedCommList.size() << std::endl;

        // 等待进一步处理
    }
}

// 平台自噪声数据
void DeviceModel::handlePlatformSelfSound(CSimData* simData)
{
    if (!simData->data)
    {
        return;
    }

    //*******************
    updatePlatformSelfSoundCache(simData);
    //*******************

    const CData_PlatformSelfSound* selfSound =
        reinterpret_cast<const CData_PlatformSelfSound*>(simData->data);

    // 处理平台自噪声数据
    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Received platform self sound data, spectrum count: "
              << selfSound->selfSoundSpectrumList.size() << std::endl;

    // 此处可以保存自噪声数据供后续处理
}

void DeviceModel::step(int64 curTime, int32 step)
{
    (void)step;

    std::cout << __FUNCTION__ << ":" << __LINE__ << "stepstep" << std::endl;

    if (!m_agent || !m_initialized)
    {
        return;
    }
    this->curTime = curTime;
    CMsg_SonarWorkState sta;
    sta.platformId = m_agent->getPlatformEntity()->id;
    sta.maxDetectRange = 200000;
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
    CSimData* selfSoundData = m_agent->getSubscribeSimData(Data_PlatformSelfSound, m_agent->getPlatformEntity()->id);
    if (selfSoundData) {
        handlePlatformSelfSound(selfSoundData);
    }



    // 等待进一步处理
    //*******************
    performSonarEquationCalculation();
    //*******************
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
    return "Sonar Model V1.0.0.20240320";
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

// 在 devicemodel.cpp 中实现声纳方程计算相关函数

void DeviceModel::updatePropagatedSoundCache(CSimMessage* simMessage)
{
    if (!simMessage || !simMessage->data) {
        LOG_WARN("Invalid propagated sound message");
        return;
    }

    const CMsg_PropagatedContinuousSoundListStruct* soundListStruct =
        reinterpret_cast<const CMsg_PropagatedContinuousSoundListStruct*>(simMessage->data);

    LOG_INFOF("Updating propagated sound cache, sound count: %zu",
              soundListStruct->propagatedContinuousList.size());

    // 清空旧的传播声数据
    m_equationCache.propagatedContinuousSpectrum.clear();

    // 处理每个传播后的连续声数据
    int soundIndex = 0;
    for (const auto& soundData : soundListStruct->propagatedContinuousList) {
        // 根据声音的到达方向或其他特征确定对应的声纳ID
        // 这里简化处理：假设按顺序对应4个声纳位置，循环分配
        int sonarID = soundIndex % 4;

        // 提取频谱数据
        std::vector<float> spectrum;
        spectrum.reserve(SPECTRUM_DATA_SIZE);
        for (int i = 0; i < SPECTRUM_DATA_SIZE; i++) {
            spectrum.push_back(soundData.spectrumData[i]);
        }

        // 如果该声纳ID已有数据，合并频谱（可选：取平均值或累加）
        if (m_equationCache.propagatedContinuousSpectrum.find(sonarID) !=
            m_equationCache.propagatedContinuousSpectrum.end()) {
            // 简单累加处理
            auto& existingSpectrum = m_equationCache.propagatedContinuousSpectrum[sonarID];
            for (size_t i = 0; i < spectrum.size() && i < existingSpectrum.size(); i++) {
                existingSpectrum[i] += spectrum[i];
            }
        } else {
            // 存储新的频谱数据
            m_equationCache.propagatedContinuousSpectrum[sonarID] = spectrum;
        }

        LOG_INFOF("Updated propagated sound cache for sonar %d, spectrum size: %zu",
                  sonarID, spectrum.size());

        soundIndex++;
    }

    // 更新时间戳
    m_equationCache.lastPropagatedSoundTime = curTime;
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
    m_equationCache.platformSelfSoundSpectrum.clear();

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
        m_equationCache.platformSelfSoundSpectrum[sonarID] = spectrum;

        LOG_INFOF("Updated platform self sound cache for sonar %d", sonarID);
    }

    // 更新时间戳
    m_equationCache.lastPlatformSoundTime = curTime;
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
        m_equationCache.environmentNoiseSpectrum[sonarID] = spectrum;
    }

    LOG_INFOF("Updated environment noise cache for all sonars, spectrum size: %zu",
              spectrum.size());

    // 更新时间戳
    m_equationCache.lastEnvironmentNoiseTime = curTime;
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

    // DI = 20lgf + offset
    if (frequency > 0.0) {
        double di = 20.0 * log10(frequency) + params.offset;
        LOG_INFOF("Calculated DI for sonar %d: f=%.1fkHz, DI=%.2f", sonarID, frequency, di);
        return di;
    } else {
        return params.offset;  // 频率为0时只返回偏移量
    }
}

double DeviceModel::calculateSonarEquation(int sonarID)
{
    // 检查数据有效性
    if (!isEquationDataValid(sonarID)) {
        LOG_WARNF("Invalid or stale data for sonar %d equation calculation", sonarID);
        return 0.0;
    }

    // 获取各频谱数据
    const auto& propagatedSpectrum = m_equationCache.propagatedContinuousSpectrum[sonarID];
    const auto& platformSpectrum = m_equationCache.platformSelfSoundSpectrum[sonarID];
    const auto& environmentSpectrum = m_equationCache.environmentNoiseSpectrum[sonarID];

    // 计算频谱累加求和
    double propagatedSum = calculateSpectrumSum(propagatedSpectrum);     // |阵元谱级|
    double platformSum = calculateSpectrumSum(platformSpectrum);         // |平台背景|
    double environmentSum = calculateSpectrumSum(environmentSpectrum);   // |海洋噪声|

    // 计算SL-TL-NL = 10lg |阵元谱级|^2/(|平台背景|^2+|海洋噪声|^2)
    double propagatedSquare = propagatedSum * propagatedSum;            // |阵元谱级|^2
    double platformSquare = platformSum * platformSum;                 // |平台背景|^2
    double environmentSquare = environmentSum * environmentSum;         // |海洋噪声|^2

    double denominator = platformSquare + environmentSquare;
    double sl_tl_nl = 0.0;

    if (denominator > 0.0 && propagatedSquare > 0.0) {
        sl_tl_nl = 10.0 * log10(propagatedSquare / denominator);
    } else {
        LOG_WARNF("Invalid spectrum data for sonar %d: propagated=%.2f, platform=%.2f, environment=%.2f",
                  sonarID, propagatedSum, platformSum, environmentSum);
        return 0.0;
    }

    // 计算DI值
    double di = calculateDI(sonarID);

    // 计算最终结果 X = SL-TL-NL + DI
    double result = sl_tl_nl + di;

    LOG_INFOF("Sonar equation result for sonar %d: SL-TL-NL=%.2f, DI=%.2f, X=%.2f",
              sonarID, sl_tl_nl, di, result);

    return result;
}

bool DeviceModel::isEquationDataValid(int sonarID)
{
    // 检查数据时效性（5秒内更新）
    int64 currentTime = curTime;
    bool propagatedValid = (currentTime - m_equationCache.lastPropagatedSoundTime) <= DATA_UPDATE_INTERVAL_MS;
    bool platformValid = (currentTime - m_equationCache.lastPlatformSoundTime) <= DATA_UPDATE_INTERVAL_MS;
    bool environmentValid = (currentTime - m_equationCache.lastEnvironmentNoiseTime) <= DATA_UPDATE_INTERVAL_MS;

    // 检查数据是否存在
    bool propagatedExists = (m_equationCache.propagatedContinuousSpectrum.find(sonarID) !=
                            m_equationCache.propagatedContinuousSpectrum.end());
    bool platformExists = (m_equationCache.platformSelfSoundSpectrum.find(sonarID) !=
                          m_equationCache.platformSelfSoundSpectrum.end());
    bool environmentExists = (m_equationCache.environmentNoiseSpectrum.find(sonarID) !=
                             m_equationCache.environmentNoiseSpectrum.end());

    bool dataValid = propagatedValid && platformValid && environmentValid;
    bool dataExists = propagatedExists && platformExists && environmentExists;

    if (!dataValid) {
        LOG_DEBUGF("Data time validity check for sonar %d: prop=%s, plat=%s, env=%s",
                   sonarID,
                   propagatedValid ? "OK" : "STALE",
                   platformValid ? "OK" : "STALE",
                   environmentValid ? "OK" : "STALE");
    }

    return dataValid && dataExists;
}

void DeviceModel::performSonarEquationCalculation()
{
    LOG_DEBUG("Performing sonar equation calculation for all sonars");

    // 为每个声纳位置计算声纳方程
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        // 检查该声纳是否启用
        auto stateIt = m_sonarStates.find(sonarID);
        if (stateIt != m_sonarStates.end() &&
            stateIt->second.arrayWorkingState &&
            stateIt->second.passiveWorkingState) {

            // 计算声纳方程
            double result = calculateSonarEquation(sonarID);

            // 存储计算结果
            m_equationCache.equationResults[sonarID] = result;

            if (result != 0.0) {  // 只有有效结果才记录
                LOG_INFOF("Sonar %d equation calculated: X = %.2f", sonarID, result);
            }
        } else {
            // 声纳未启用，清除旧结果
            m_equationCache.equationResults.erase(sonarID);
        }
    }
}

// 公共接口实现
double DeviceModel::getSonarEquationResult(int sonarID)
{
    auto it = m_equationCache.equationResults.find(sonarID);
    if (it != m_equationCache.equationResults.end()) {
        return it->second;
    }
    return 0.0;
}

std::map<int, double> DeviceModel::getAllSonarEquationResults()
{
    return m_equationCache.equationResults;
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

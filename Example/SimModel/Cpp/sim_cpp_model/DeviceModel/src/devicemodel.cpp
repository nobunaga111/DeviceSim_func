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

    LOG_INFO("Sonar model created");

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

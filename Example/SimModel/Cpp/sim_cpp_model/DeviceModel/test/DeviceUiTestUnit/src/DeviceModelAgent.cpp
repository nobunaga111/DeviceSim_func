#include "DeviceModelAgent.h"
#include "DeviceTestInOut.h"

#include <iostream>
#include <iomanip>
#include <cstring>
#include "../../DeviceModel/src/common/DMLogger.h"

DeviceModelAgent::DeviceModelAgent()
{
    // 初始化平台实体
    m_platform.id = 456;
    strncpy(m_platform.name, "TestSonarPlatform", sizeof(m_platform.name) - 1);
    m_platform.name[sizeof(m_platform.name) - 1] = '\0';  // 确保以null结尾
    m_platform.entityType[0] = '\0';
    m_platform.campId = 1;
    m_platform.componentCount = 0;

    // 初始化组件属性
    m_attribute.id = 789;
    m_attribute.name = "SonarComponent";
    m_attribute.dataFormat = CDataFormat::STRUCT;
}

DeviceModelAgent::~DeviceModelAgent()
{
    // 清理已分配的数据
    for (auto& pair : m_subscribedData) {
        if (pair.second) {
            delete pair.second;
        }
    }
    m_subscribedData.clear();
}

void DeviceModelAgent::publishSimData(CSimData* simData)
{
    if (!simData) {
        return;
    }

    // 根据主题处理不同类型的数据
    std::string topic = simData->topic;

    if (topic == Data_SonarState_Topic) {
        CData_SonarState* state = (CData_SonarState*)simData->data;
        LOG_INFOF("Sonar %d state published: array=%d active=%d passive=%d scouting=%d",
                  state->sonarID, state->arrayWorkingState, state->activeWorkingState,
                  state->passiveWorkingState, state->scoutingWorkingState);
    }
    else {
        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Unknown topic in publishSimData: " << topic
                  << std::endl;
    }
}

CSimData* DeviceModelAgent::getSubscribeSimData(const char* topic, int64 platformId)
{
    if (!topic) {
        return nullptr;
    }

    std::string key = std::string(topic) + "_" + std::to_string(platformId);

    if (m_subscribedData.find(key) != m_subscribedData.end()) {
        return m_subscribedData[key];
    }

    return nullptr;
}

CSimData* DeviceModelAgent::getSubscribeSimData(const char* topic, int64 platformId, int64 componentId)
{
    if (!topic) {
        return nullptr;
    }

    std::string key = std::string(topic) + "_" + std::to_string(platformId) + "_" + std::to_string(componentId);

    if (m_subscribedData.find(key) != m_subscribedData.end()) {
        return m_subscribedData[key];
    }

    return nullptr;
}

void DeviceModelAgent::subscribeSimData(CSubscribeSimData* subscribeSimData)
{
    if (!subscribeSimData) {
        return;
    }

    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Subscribed to data topic: " << subscribeSimData->topic
              << std::endl;
}

void DeviceModelAgent::subscribeCampSimData(CSubscribeSimData* subscribeSimData)
{
    if (!subscribeSimData) {
        return;
    }

    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Subscribed to camp data topic: " << subscribeSimData->topic
              << std::endl;
}

void DeviceModelAgent::unsubscribSimData(CSubscribeSimData* subscribeSimData)
{
    if (!subscribeSimData) {
        return;
    }

    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Unsubscribed from data topic: " << subscribeSimData->topic
              << std::endl;
}

void DeviceModelAgent::unsubscribCampSimData(CSubscribeSimData* subscribeSimData)
{
    if (!subscribeSimData) {
        return;
    }

    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Unsubscribed from camp data topic: " << subscribeSimData->topic
              << std::endl;
}

void DeviceModelAgent::sendMessage(CSimMessage* simMessage)
{
    if (!simMessage) {
        return;
    }

    std::string topic = simMessage->topic;

    if (topic == MSG_PassiveSonarResult_Topic) {
        CMsg_PassiveSonarResultStruct* result = (CMsg_PassiveSonarResultStruct*)simMessage->data;
        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Passive sonar " << result->sonarID << " result received: "
                  << " detections=" << result->detectionNumber
                  << " trackings=" << result->PassiveSonarTrackingResult.size()
                  << std::endl;

        // 打印部分检测结果
        for (size_t i = 0; i < result->PassiveSonarDetectionResult.size() && i < 3; i++) {
            auto& det = result->PassiveSonarDetectionResult[i];
            std::cout << "  Detection " << i << ": "
                      << " arrayDir=" << det.detectionDirArray
                      << " groundDir=" << det.detectionDirGroud
                      << std::endl;
        }
    }
    else if (topic == MSG_ActiveSonarResult_Topic) {
        CMsg_ActiveSonarResult* result = (CMsg_ActiveSonarResult*)simMessage->data;
        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Active sonar " << result->sonarID << " result received: "
                  << " detections=" << result->detectionNumber
                  << " trackings=" << result->ActiveSonarTrackingResult.size()
                  << std::endl;

        // 打印部分检测结果
        for (size_t i = 0; i < result->ActiveSonarDetectionResult.size() && i < 3; i++) {
            auto& det = result->ActiveSonarDetectionResult[i];
            std::cout << "  Detection " << i << ": "
                      << " SNR=" << det.SNR
                      << " groundDir=" << det.detectionDirGroud
                      << " distance=" << det.detectionDistance
                      << "m velocity=" << det.detectionVelocity
                      << std::endl;
        }
    }
    else if (topic == MSG_ScoutingSonarResult_Topic) {
        CMsg_ScoutingSonarResult* result = (CMsg_ScoutingSonarResult*)simMessage->data;
        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Scouting sonar " << result->sonarID << " result received: "
                  << " signals=" << result->signalNumber
                  << std::endl;

        // 打印部分侦察结果
        for (size_t i = 0; i < result->ScoutingSonarDetectionResult.size() && i < 3; i++) {
            auto& det = result->ScoutingSonarDetectionResult[i];
            std::cout << "  Signal " << i << ": "
                      << " ID=" << det.signalId
                      << " SNR=" << det.SNR
                      << " groundDir=" << det.detectionDirGroud
                      << " type=" << det.recognitionResult
                      << " freq=" << det.freqResult
                      << std::endl;
        }
    }
    else {
        std::cout << __FUNCTION__ << ":" << __LINE__
                  << " Message sent with topic: " << topic
                  << std::endl;
    }
}

void DeviceModelAgent::subscribeMessage(const char* topic)
{
    if (!topic) {
        return;
    }

    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Subscribed to message topic: " << topic
              << std::endl;
}

void DeviceModelAgent::unsubscribeMessage(const char* topic)
{
    if (!topic) {
        return;
    }

    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Unsubscribed from message topic: " << topic
              << std::endl;
}

void DeviceModelAgent::publishCriticalEvent(CCriticalEvent* criticalEvent)
{
    if (!criticalEvent) {
        return;
    }

    std::cout << __FUNCTION__ << ":" << __LINE__
              << " Critical event published: " << criticalEvent->type
              << std::endl;
}

CSimPlatformEntity* DeviceModelAgent::getPlatformEntity()
{
    return &m_platform;
}

CSimComponentAttribute* DeviceModelAgent::getComponentAttribute()
{
    return &m_attribute;
}

std::vector<int32> DeviceModelAgent::getCamp()
{
    std::vector<int32> camps;
    camps.push_back(1);  // 返回一个示例阵营ID
    return camps;
}

std::vector<CSimPlatformEntity*> DeviceModelAgent::getPlatformEntiysByCampId(int32 campId)
{
    std::vector<CSimPlatformEntity*> entities;

    if (campId == m_platform.campId) {
        entities.push_back(&m_platform);
    }

    return entities;
}

int64 DeviceModelAgent::getComponentElapseTime()
{
    return 0;
}

int64 DeviceModelAgent::getEngineElapseTime()
{
    return 0;
}

int32 DeviceModelAgent::getStep()
{
    return 1000;  // 默认步长1000ms
}

void DeviceModelAgent::setStep(int32 step)
{
    (void)step;
}

int64 DeviceModelAgent::getStartTime()
{
    return 0;
}

int64 DeviceModelAgent::getEndTime()
{
    return 3600000;  // 默认仿真1小时
}

CSimIntegrationLogger* DeviceModelAgent::getLogger()
{
    // 返回空指针，如果需要日志功能可以在这里实现
    return nullptr;
}

// 添加订阅数据的新方法
void DeviceModelAgent::addSubscribedData(const char* topic, int64 platformId, CSimData* data)
{
    if (!topic || !data) {
        std::cout << "Error: Invalid topic or data pointer" << std::endl;
        return;
    }

    // 使用安全的转换
    try {
        std::string key = std::string(topic) + "_" + std::to_string(platformId);

        // 删除旧数据（如果存在）
        auto it = m_subscribedData.find(key);
        if (it != m_subscribedData.end() && it->second) {
            delete it->second;
        }

        // 存储新数据
        m_subscribedData[key] = data;

        std::cout << "Added data with key: " << key << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception in addSubscribedData: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unknown exception in addSubscribedData" << std::endl;
    }
}

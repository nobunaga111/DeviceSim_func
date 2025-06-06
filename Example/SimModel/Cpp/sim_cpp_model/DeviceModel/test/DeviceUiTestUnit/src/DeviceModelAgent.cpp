#include "DeviceModelAgent.h"
#include "DeviceTestInOut.h"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <QDateTime>
#include "../../DeviceModel/src/common/DMLogger.h"

DeviceModelAgent::DeviceModelAgent()
    : m_enableDebugOutput(true)
    , m_lastCleanupTime(0)
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
    m_attribute.name = "MultiTargetSonarComponent";
    m_attribute.dataFormat = CDataFormat::STRUCT;

    debugLog("DeviceModelAgent initialized with multi-target support");
}

DeviceModelAgent::~DeviceModelAgent()
{
    debugLog("DeviceModelAgent destructor called");

    // 清理所有数据 - 智能指针会自动释放内存
    m_subscribedDataMap.clear();

    debugLog("DeviceModelAgent destroyed, all data cleaned up");
}

void DeviceModelAgent::publishSimData(CSimData* simData)
{
    if (!simData) {
        debugLog("Error: publishSimData called with null simData");
        return;
    }

    // 根据主题处理不同类型的数据
    std::string topic = simData->topic;

    if (topic == Data_SonarState_Topic) {
        CData_SonarState* state = (CData_SonarState*)simData->data;
        if (state) {
            debugLog("Sonar " + std::to_string(state->sonarID) + " state published: array=" +
                    std::to_string(state->arrayWorkingState) + " active=" + std::to_string(state->activeWorkingState) +
                    " passive=" + std::to_string(state->passiveWorkingState) + " scouting=" + std::to_string(state->scoutingWorkingState));
        }
    }
    else {
        debugLog("Published data with unknown topic: " + topic);
    }
}

CSimData* DeviceModelAgent::getSubscribeSimData(const char* topic, int64 platformId)
{
    if (!topic) {
        debugLog("Error: topic is null in getSubscribeSimData");
        return nullptr;
    }

    std::string key = generateDataKey(topic, platformId);

    // 更新访问统计
    m_dataAccessCount[key]++;
    m_lastAccessTime[key] = QDateTime::currentMSecsSinceEpoch();

    if (m_enableDebugOutput) {
        debugLog("Looking for data with key: " + key);
    }

    auto it = m_subscribedDataMap.find(key);
    if (it != m_subscribedDataMap.end() && it->second) {
        CSimData* result = it->second.get();
        debugLog("Found data for key: " + key + ", timestamp: " + std::to_string(result->time));
        return result;
    }

    debugLog("No data found for key: " + key);
    return nullptr;
}

CSimData* DeviceModelAgent::getSubscribeSimData(const char* topic, int64 platformId, int64 componentId)
{
    if (!topic) {
        debugLog("Error: topic is null in getSubscribeSimData (with componentId)");
        return nullptr;
    }

    std::string key = generateDataKey(topic, platformId, componentId);

    // 更新访问统计
    m_dataAccessCount[key]++;
    m_lastAccessTime[key] = QDateTime::currentMSecsSinceEpoch();

    auto it = m_subscribedDataMap.find(key);
    if (it != m_subscribedDataMap.end() && it->second) {
        debugLog("Found data for key with componentId: " + key);
        return it->second.get();
    }

    debugLog("No data found for key with componentId: " + key);
    return nullptr;
}

void DeviceModelAgent::subscribeSimData(CSubscribeSimData* subscribeSimData)
{
    if (!subscribeSimData) {
        debugLog("Error: subscribeSimData called with null parameter");
        return;
    }

    debugLog("Subscribed to data topic: " + std::string(subscribeSimData->topic) +
             " for platform: " + std::to_string(subscribeSimData->platformId));
}

void DeviceModelAgent::subscribeCampSimData(CSubscribeSimData* subscribeSimData)
{
    if (!subscribeSimData) {
        debugLog("Error: subscribeCampSimData called with null parameter");
        return;
    }

    debugLog("Subscribed to camp data topic: " + std::string(subscribeSimData->topic) +
             " for platform: " + std::to_string(subscribeSimData->platformId));
}

void DeviceModelAgent::unsubscribSimData(CSubscribeSimData* subscribeSimData)
{
    if (!subscribeSimData) {
        debugLog("Error: unsubscribSimData called with null parameter");
        return;
    }

    debugLog("Unsubscribed from data topic: " + std::string(subscribeSimData->topic));
}

void DeviceModelAgent::unsubscribCampSimData(CSubscribeSimData* subscribeSimData)
{
    if (!subscribeSimData) {
        debugLog("Error: unsubscribCampSimData called with null parameter");
        return;
    }

    debugLog("Unsubscribed from camp data topic: " + std::string(subscribeSimData->topic));
}

void DeviceModelAgent::sendMessage(CSimMessage* simMessage)
{
    if (!simMessage) {
        debugLog("Error: sendMessage called with null simMessage");
        return;
    }

    std::string topic = simMessage->topic;

    if (topic == MSG_PassiveSonarResult_Topic) {
        CMsg_PassiveSonarResultStruct* result = (CMsg_PassiveSonarResultStruct*)simMessage->data;
        if (result) {
            debugLog("Passive sonar " + std::to_string(result->sonarID) + " result received: " +
                    " detections=" + std::to_string(result->detectionNumber) +
                    " trackings=" + std::to_string(result->PassiveSonarTrackingResult.size()));

            // 打印部分检测结果
            for (size_t i = 0; i < result->PassiveSonarDetectionResult.size() && i < 3; i++) {
                auto& det = result->PassiveSonarDetectionResult[i];
                debugLog("  Detection " + std::to_string(i) + ": " +
                        " arrayDir=" + std::to_string(det.detectionDirArray) +
                        " groundDir=" + std::to_string(det.detectionDirGroud));
            }
        }
    }
    else if (topic == MSG_ActiveSonarResult_Topic) {
        CMsg_ActiveSonarResult* result = (CMsg_ActiveSonarResult*)simMessage->data;
        if (result) {
            debugLog("Active sonar " + std::to_string(result->sonarID) + " result received: " +
                    " detections=" + std::to_string(result->detectionNumber) +
                    " trackings=" + std::to_string(result->ActiveSonarTrackingResult.size()));

            // 打印部分检测结果
            for (size_t i = 0; i < result->ActiveSonarDetectionResult.size() && i < 3; i++) {
                auto& det = result->ActiveSonarDetectionResult[i];
                debugLog("  Detection " + std::to_string(i) + ": " +
                        " SNR=" + std::to_string(det.SNR) +
                        " groundDir=" + std::to_string(det.detectionDirGroud) +
                        " distance=" + std::to_string(det.detectionDistance) +
                        "m velocity=" + std::to_string(det.detectionVelocity));
            }
        }
    }
    else if (topic == MSG_ScoutingSonarResult_Topic) {
        CMsg_ScoutingSonarResult* result = (CMsg_ScoutingSonarResult*)simMessage->data;
        if (result) {
            debugLog("Scouting sonar " + std::to_string(result->sonarID) + " result received: " +
                    " signals=" + std::to_string(result->signalNumber));

            // 打印部分侦察结果
            for (size_t i = 0; i < result->ScoutingSonarDetectionResult.size() && i < 3; i++) {
                auto& det = result->ScoutingSonarDetectionResult[i];
                debugLog("  Signal " + std::to_string(i) + ": " +
                        " ID=" + std::to_string(det.signalId) +
                        " SNR=" + std::to_string(det.SNR) +
                        " groundDir=" + std::to_string(det.detectionDirGroud) +
                        " type=" + std::to_string(det.recognitionResult) +
                        " freq=" + std::to_string(det.freqResult));
            }
        }
    }
    else {
        debugLog("Message sent with topic: " + topic);
    }
}

void DeviceModelAgent::subscribeMessage(const char* topic)
{
    if (!topic) {
        debugLog("Error: subscribeMessage called with null topic");
        return;
    }

    debugLog("Subscribed to message topic: " + std::string(topic));
}

void DeviceModelAgent::unsubscribeMessage(const char* topic)
{
    if (!topic) {
        debugLog("Error: unsubscribeMessage called with null topic");
        return;
    }

    debugLog("Unsubscribed from message topic: " + std::string(topic));
}

void DeviceModelAgent::publishCriticalEvent(CCriticalEvent* criticalEvent)
{
    if (!criticalEvent) {
        debugLog("Error: publishCriticalEvent called with null criticalEvent");
        return;
    }

    debugLog("Critical event published: " + std::string(criticalEvent->type));
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
    return QDateTime::currentMSecsSinceEpoch();
}

int64 DeviceModelAgent::getEngineElapseTime()
{
    return QDateTime::currentMSecsSinceEpoch();
}

int32 DeviceModelAgent::getStep()
{
    return 1000;  // 默认步长1000ms
}

void DeviceModelAgent::setStep(int32 step)
{
    debugLog("Step size set to: " + std::to_string(step) + "ms");
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
    // 返回空指针，如需日志功能可以在这加
    return nullptr;
}



void DeviceModelAgent::addSubscribedData(const char* topic, int64 platformId, CSimData* data)
{
    addSubscribedData(topic, platformId, -1, data);
}

void DeviceModelAgent::addSubscribedData(const char* topic, int64 platformId, int64 componentId, CSimData* data)
{
    if (!topic || !data) {
        debugLog("Error: Invalid topic or data pointer in addSubscribedData");
        if (data) {
            safeDeleteSimData(data);
        }
        return;
    }

    // topic字符串有效性检查
    std::string topicStr;
    try {
        topicStr = std::string(topic);
        if (topicStr.empty()) {
            debugLog("Error: Empty topic string");
            safeDeleteSimData(data);
            return;
        }
    } catch (const std::exception& e) {
        debugLog("Error: Invalid topic string: " + std::string(e.what()));
        safeDeleteSimData(data);
        return;
    }

    // 检查data->topic的有效性
    if (strlen(data->topic) == 0) {
        debugLog("Warning: data->topic is empty, using provided topic");
        strncpy(data->topic, topic, EventTypeLen - 1);
        data->topic[EventTypeLen - 1] = '\0';
    }

    try {
        std::string key = generateDataKey(topic, platformId, componentId);

        // 删除旧数据（如果存在）
        auto it = m_subscribedDataMap.find(key);
        if (it != m_subscribedDataMap.end()) {
            debugLog("Replacing existing data for key: " + key);
        }

        // 创建带自定义删除器的shared_ptr
        auto customDeleter = [this, topicStr](CSimData* simData) {
            if (simData) {
                try {
                    if (simData->data) {
                        safeDeleteDataContent(topicStr, simData->data);
                    }
                    delete simData;
                } catch (...) {
                    debugLog("Exception in custom deleter for topic: " + topicStr);
                    delete simData; // 确保simData被删除
                }
            }
        };

        // 存储新数据
        m_subscribedDataMap[key] = std::shared_ptr<CSimData>(data, customDeleter);

        // 更新访问时间
        m_lastAccessTime[key] = QDateTime::currentMSecsSinceEpoch();

        debugLog("Successfully stored data with key: " + key +
                ", timestamp: " + std::to_string(data->time) +
                ", topic: " + topicStr +
                ", platformId: " + std::to_string(platformId) +
                (componentId != -1 ? (", componentId: " + std::to_string(componentId)) : ""));

        // 定期清理过期数据
        int64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - m_lastCleanupTime > CLEANUP_INTERVAL_MS) {
            clearExpiredData();
            m_lastCleanupTime = currentTime;
        }

    } catch (const std::exception& e) {
        debugLog("Exception in addSubscribedData: " + std::string(e.what()));
        safeDeleteSimData(data);
    } catch (...) {
        debugLog("Unknown exception in addSubscribedData");
        safeDeleteSimData(data);
    }
}

void DeviceModelAgent::safeDeleteSimData(CSimData* data)
{
    if (!data) return;

    try {
        if (data->data) {
            // 根据topic判断如何删除data->data
            std::string topic(data->topic);
            safeDeleteDataContent(topic, data->data);
        }
        delete data;
        debugLog("Safely deleted CSimData pointer");
    } catch (const std::exception& e) {
        debugLog("Exception in safeDeleteSimData: " + std::string(e.what()));
        // 即使出现异常也要尝试删除主结构
        delete data;
    } catch (...) {
        debugLog("Unknown exception in safeDeleteSimData");
        delete data;
    }
}

void DeviceModelAgent::safeDeleteDataContent(const std::string& topic, const void* dataPtr)
{
    if (!dataPtr) return;

    try {
        if (topic == Data_PlatformSelfSound) {
            delete static_cast<const CData_PlatformSelfSound*>(dataPtr);
            debugLog("Deleted CData_PlatformSelfSound content");
        }
        else if (topic == MSG_PropagatedContinuousSound) {
            delete static_cast<const CMsg_PropagatedContinuousSoundListStruct*>(dataPtr);
            debugLog("Deleted PropagatedContinuousSound content");
        }
        else if (topic == MSG_EnvironmentNoiseToSonar) {
            delete static_cast<const CMsg_EnvironmentNoiseToSonarStruct*>(dataPtr);
            debugLog("Deleted EnvironmentNoise content");
        }
        // 其他数据类型的清理
        else {
            debugLog("Unknown data type for topic: " + topic + ", skipping content deletion");
        }
    } catch (const std::exception& e) {
        debugLog("Exception in safeDeleteDataContent for topic " + topic + ": " + std::string(e.what()));
    } catch (...) {
        debugLog("Unknown exception in safeDeleteDataContent for topic: " + topic);
    }
}

void DeviceModelAgent::clearExpiredData(int64 maxAge)
{
    int64 currentTime = QDateTime::currentMSecsSinceEpoch();
    int cleanupCount = 0;

    auto it = m_subscribedDataMap.begin();
    while (it != m_subscribedDataMap.end()) {
        if (it->second && (currentTime - it->second->time) > maxAge) {
            debugLog("Removing expired data for key: " + it->first +
                    " (age: " + std::to_string(currentTime - it->second->time) + "ms)");

            // 清理相关的访问统计
            m_dataAccessCount.erase(it->first);
            m_lastAccessTime.erase(it->first);

            it = m_subscribedDataMap.erase(it);
            cleanupCount++;
        } else {
            ++it;
        }
    }

    if (cleanupCount > 0) {
        debugLog("Cleaned up " + std::to_string(cleanupCount) + " expired data entries");
    }
}

std::string DeviceModelAgent::getDataStatistics() const
{
    std::stringstream ss;
    ss << "=== DeviceModelAgent Data Statistics ===\n";
    ss << "Total stored data entries: " << m_subscribedDataMap.size() << "\n";
    ss << "Data access statistics:\n";

    for (const auto& pair : m_dataAccessCount) {
        ss << "  " << pair.first << ": accessed " << pair.second << " times";

        auto timeIt = m_lastAccessTime.find(pair.first);
        if (timeIt != m_lastAccessTime.end()) {
            int64 currentTime = QDateTime::currentMSecsSinceEpoch();
            int64 ageMs = currentTime - timeIt->second;
            ss << ", last access " << ageMs << "ms ago";
        }
        ss << "\n";
    }

    ss << "Current data entries:\n";
    for (const auto& pair : m_subscribedDataMap) {
        if (pair.second) {
            int64 currentTime = QDateTime::currentMSecsSinceEpoch();
            int64 dataAge = currentTime - pair.second->time;
            ss << "  " << pair.first << ": timestamp=" << pair.second->time
               << ", age=" << dataAge << "ms\n";
        } else {
            ss << "  " << pair.first << ": NULL DATA\n";
        }
    }

    return ss.str();
}

bool DeviceModelAgent::isDataValid(const char* topic, int64 platformId, int64 maxAge) const
{
    if (!topic) {
        return false;
    }

    std::string key = generateDataKey(topic, platformId);

    auto it = m_subscribedDataMap.find(key);
    if (it == m_subscribedDataMap.end() || !it->second) {
        return false;
    }

    int64 currentTime = QDateTime::currentMSecsSinceEpoch();
    int64 dataAge = currentTime - it->second->time;

    bool valid = dataAge <= maxAge;

    if (!valid && m_enableDebugOutput) {
        debugLog("Data for key " + key + " is expired (age: " + std::to_string(dataAge) +
                "ms, maxAge: " + std::to_string(maxAge) + "ms)");
    }

    return valid;
}

// ========== 私有辅助方法实现 ==========

std::string DeviceModelAgent::generateDataKey(const char* topic, int64 platformId, int64 componentId) const
{
    std::string key = std::string(topic) + "_" + std::to_string(platformId);
    if (componentId != -1) {
        key += "_" + std::to_string(componentId);
    }
    return key;
}

void DeviceModelAgent::safeDeleteData(CSimData* data)
{
    // 使用智能指针后，这个方法主要用于调试
    if (data) {
        debugLog("Safely deleting data pointer");
        // 实际删除由智能指针自动处理
    }
}

void DeviceModelAgent::debugLog(const std::string& message) const
{
    if (m_enableDebugOutput) {
        // 使用LOG_INFO输出调试信息
        LOG_INFO(message);

        // 同时输出到标准输出（用于调试）
        std::cout << "[DeviceModelAgent] " << message << std::endl;
    }
}

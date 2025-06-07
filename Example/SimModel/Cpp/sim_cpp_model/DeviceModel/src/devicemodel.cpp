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



#include <windows.h>
#include <dbghelp.h>
// 异常过滤器函数
LONG WINAPI CustomExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo)
{
    LOG_ERRORF("=== CRASH DETECTED ===");
    LOG_ERRORF("Exception Code: 0x%08X", ExceptionInfo->ExceptionRecord->ExceptionCode);
    LOG_ERRORF("Exception Address: %p", ExceptionInfo->ExceptionRecord->ExceptionAddress);

    // 尝试获取更多信息
    CONTEXT* context = ExceptionInfo->ContextRecord;
    #ifdef _WIN64
    LOG_ERRORF("RAX: %016llX RBX: %016llX RCX: %016llX RDX: %016llX",
              context->Rax, context->Rbx, context->Rcx, context->Rdx);
    LOG_ERRORF("RSI: %016llX RDI: %016llX RBP: %016llX RSP: %016llX",
              context->Rsi, context->Rdi, context->Rbp, context->Rsp);
    LOG_ERRORF("RIP: %016llX", context->Rip);
    #else
    LOG_ERRORF("EAX: %08X EBX: %08X ECX: %08X EDX: %08X",
              context->Eax, context->Ebx, context->Ecx, context->Edx);
    LOG_ERRORF("ESI: %08X EDI: %08X EBP: %08X ESP: %08X",
              context->Esi, context->Edi, context->Ebp, context->Esp);
    LOG_ERRORF("EIP: %08X", context->Eip);
    #endif

    // 刷新日志确保写入文件
    Logger::getInstance().flush();

    return EXCEPTION_EXECUTE_HANDLER;
}

DeviceModel::DeviceModel()
{
    // 设置全局异常处理器
    SetUnhandledExceptionFilter(CustomExceptionFilter);

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
            m_debugStats.totalMessagesReceived++;

            LOG_INFOF("=== MSG_PropagatedContinuousSound RECEIVED (#%d) ===",
                     m_debugStats.totalMessagesReceived);
            LOG_INFOF("Message details:");
            LOG_INFOF("  - Time: %lld", simMessage->time);
            LOG_INFOF("  - Sender: %d", simMessage->sender);
            LOG_INFOF("  - Length: %d", simMessage->length);
            LOG_INFOF("  - DataFormat: %d", simMessage->dataFormat);
            LOG_INFOF("  - Data ptr: %p", simMessage->data);

            // 验证数据长度是否合理
            if (simMessage->length < sizeof(CMsg_PropagatedContinuousSoundListStruct)) {
                LOG_ERRORF("CRITICAL: Message length too small! Expected >= %zu, got %d",
                          sizeof(CMsg_PropagatedContinuousSoundListStruct), simMessage->length);
                m_debugStats.failedProcessings++;
                m_debugStats.lastFailedTime = simMessage->time;
                m_debugStats.lastErrorMsg = "Message length too small";
                return;
            }

            // 内存地址对齐检查
            if (reinterpret_cast<uintptr_t>(simMessage->data) % alignof(CMsg_PropagatedContinuousSoundListStruct) != 0) {
                LOG_WARNF("WARNING: Data pointer not properly aligned for struct access: %p", simMessage->data);
            }

            try {
                updateMultiTargetPropagatedSoundCache(simMessage); //


                m_debugStats.successfulProcessings++;
                m_debugStats.lastSuccessfulTime = simMessage->time;
                LOG_INFOF("✓ MSG_PropagatedContinuousSound processed successfully");
            } catch (const std::exception& e) {
                m_debugStats.failedProcessings++;
                m_debugStats.lastFailedTime = simMessage->time;
                m_debugStats.lastErrorMsg = e.what();
                LOG_ERRORF("CRASH: Exception in updateMultiTargetPropagatedSoundCache: %s", e.what());
            } catch (...) {
                m_debugStats.failedProcessings++;
                m_debugStats.lastFailedTime = simMessage->time;
                m_debugStats.lastErrorMsg = "Unknown exception";
                LOG_ERRORF("CRASH: Unknown exception in updateMultiTargetPropagatedSoundCache");
            }

            // 打印调试统计信息
            LOG_INFOF("Debug Stats: Total=%d, Success=%d, Failed=%d, Success Rate=%.1f%%",
                     m_debugStats.totalMessagesReceived, m_debugStats.successfulProcessings,
                     m_debugStats.failedProcessings,
                     (m_debugStats.totalMessagesReceived > 0 ?
                      100.0 * m_debugStats.successfulProcessings / m_debugStats.totalMessagesReceived : 0.0));

            return; // 重要：在这里直接返回，避免继续执行原来的处理逻辑
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

// 全面的异常捕获、参数验证和crash定位
void DeviceModel::updateMultiTargetPropagatedSoundCache_yuan(CSimMessage* simMessage)
{
    LOG_INFOF("=== ENTERING updateMultiTargetPropagatedSoundCache ===");
    LOG_INFOF("Function entry - simMessage ptr: %p", simMessage);

    try {
        // ========== 第一层检查：基本参数验证 ==========
        if (!simMessage) {
            LOG_ERRORF("CRITICAL: simMessage is NULL!");
            return;
        }
        LOG_INFOF("✓ simMessage is valid: %p", simMessage);

        if (!simMessage->data) {
            LOG_ERRORF("CRITICAL: simMessage->data is NULL!");
            return;
        }
        LOG_INFOF("✓ simMessage->data is valid: %p", simMessage->data);
        LOG_INFOF("✓ simMessage->length: %d", simMessage->length);
        LOG_INFOF("✓ simMessage->time: %lld", simMessage->time);
        LOG_INFOF("✓ simMessage->topic: %s", simMessage->topic);

        // ========== 第二层检查：数据转换和验证 ==========
        LOG_INFOF("--- Attempting reinterpret_cast ---");
        const CMsg_PropagatedContinuousSoundListStruct* soundListStruct = nullptr;

        try {
            soundListStruct = reinterpret_cast<const CMsg_PropagatedContinuousSoundListStruct*>(simMessage->data);
            LOG_INFOF("✓ reinterpret_cast successful: %p", soundListStruct);
        } catch (const std::exception& e) {
            LOG_ERRORF("CRASH: reinterpret_cast failed with exception: %s", e.what());
            return;
        } catch (...) {
            LOG_ERRORF("CRASH: reinterpret_cast failed with unknown exception");
            return;
        }

        if (!soundListStruct) {
            LOG_ERRORF("CRITICAL: soundListStruct is NULL after cast!");
            return;
        }

        // ========== 第三层检查：访问list容器 ==========
        LOG_INFOF("--- Accessing propagatedContinuousList ---");
        size_t listSize = 0;
        try {
            listSize = soundListStruct->propagatedContinuousList.size();
            LOG_INFOF("✓ List size accessed successfully: %zu", listSize);
        } catch (const std::exception& e) {
            LOG_ERRORF("CRASH: Failed to access list size with exception: %s", e.what());
            return;
        } catch (...) {
            LOG_ERRORF("CRASH: Failed to access list size with unknown exception");
            return;
        }

        LOG_INFOF("Updating multi-target propagated sound cache, sound count: %zu", listSize);

        int64 currentTime = simMessage->time;
        LOG_INFOF("Current time set to: %lld", currentTime);

        // ========== 第四层检查：处理空列表情况 ==========
        if (listSize == 0) {
            LOG_INFO("Received empty target list, clearing all sonar target data");

            try {
                for (int sonarID = 0; sonarID < 4; sonarID++) {
                    LOG_INFOF("Clearing sonar %d data...", sonarID);
                    m_multiTargetCache.sonarTargetsData[sonarID].clear();
                    m_multiTargetCache.multiTargetEquationResults[sonarID].clear();
                    LOG_INFOF("✓ Sonar %d data cleared", sonarID);
                }
                LOG_INFO("✓ All sonar target data cleared successfully");
                return;
            } catch (const std::exception& e) {
                LOG_ERRORF("CRASH: Failed to clear sonar data with exception: %s", e.what());
                return;
            } catch (...) {
                LOG_ERRORF("CRASH: Failed to clear sonar data with unknown exception");
                return;
            }
        }

        // ========== 第五层检查：清理过期数据 ==========
        LOG_INFOF("--- Cleaning expired data for all sonars ---");
        try {
            for (int sonarID = 0; sonarID < 4; sonarID++) {
                LOG_INFOF("Processing sonar %d expired data cleanup...", sonarID);

                auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];
                size_t beforeSize = targetsData.size();
                LOG_INFOF("Sonar %d targets before cleanup: %zu", sonarID, beforeSize);

                // 移除过期数据（超过5秒未更新）
                targetsData.erase(
                    std::remove_if(targetsData.begin(), targetsData.end(),
                        [currentTime](const TargetData& target) {
                            return (currentTime - target.lastUpdateTime) > DATA_UPDATE_INTERVAL_MS;
                        }),
                    targetsData.end()
                );

                size_t afterSize = targetsData.size();
                LOG_INFOF("✓ Sonar %d targets after cleanup: %zu (removed: %zu)",
                         sonarID, afterSize, beforeSize - afterSize);
            }
            LOG_INFOF("✓ All sonars expired data cleanup completed");
        } catch (const std::exception& e) {
            LOG_ERRORF("CRASH: Failed during expired data cleanup with exception: %s", e.what());
            return;
        } catch (...) {
            LOG_ERRORF("CRASH: Failed during expired data cleanup with unknown exception");
            return;
        }

        // ========== 第六层检查：遍历目标数据 ==========
        LOG_INFOF("--- Processing target data iteration ---");
        int targetIndex = 0;

        try {
            for (const auto& soundData : soundListStruct->propagatedContinuousList) {
                LOG_INFOF("=== Processing target index %d ===", targetIndex);

                // ========== 检查soundData的各个字段 ==========
                LOG_INFOF("Target %d - arrivalSideAngle: %.3f°", targetIndex, soundData.arrivalSideAngle);
                LOG_INFOF("Target %d - arrivalPitchAngle: %.3f°", targetIndex, soundData.arrivalPitchAngle);
                LOG_INFOF("Target %d - targetDistance: %.3f m", targetIndex, soundData.targetDistance);
                LOG_INFOF("Target %d - platType: %d", targetIndex, soundData.platType);

                // ========== 验证频谱数据访问 ==========
                LOG_INFOF("--- Checking spectrum data for target %d ---", targetIndex);
                try {
                    // 检查频谱数据的前几个和后几个元素
                    LOG_INFOF("Target %d spectrum[0]: %.6f", targetIndex, soundData.spectrumData[0]);
                    LOG_INFOF("Target %d spectrum[1]: %.6f", targetIndex, soundData.spectrumData[1]);
                    LOG_INFOF("Target %d spectrum[2]: %.6f", targetIndex, soundData.spectrumData[2]);

                    // 检查中间位置
                    LOG_INFOF("Target %d spectrum[2648]: %.6f", targetIndex, soundData.spectrumData[2648]); // 中间位置

                    // 检查最后几个元素（这里最容易出现越界）
                    LOG_INFOF("Target %d spectrum[5293]: %.6f", targetIndex, soundData.spectrumData[5293]);
                    LOG_INFOF("Target %d spectrum[5294]: %.6f", targetIndex, soundData.spectrumData[5294]);
                    LOG_INFOF("Target %d spectrum[5295]: %.6f", targetIndex, soundData.spectrumData[5295]); // 最后一个元素

                    LOG_INFOF("✓ Target %d spectrum data access validation passed", targetIndex);
                } catch (const std::exception& e) {
                    LOG_ERRORF("CRASH: Failed to access spectrum data for target %d with exception: %s",
                              targetIndex, e.what());
                    return;
                } catch (...) {
                    LOG_ERRORF("CRASH: Failed to access spectrum data for target %d with unknown exception", targetIndex);
                    return;
                }

                // 为每个目标分配一个唯一ID
                int targetId = targetIndex + 1000; // 从1000开始编号避免与其他ID冲突

                // 获取目标的绝对方位角和距离
                float targetBearing = soundData.arrivalSideAngle;  // 这是绝对方位角
                float targetDistance = soundData.targetDistance;

                LOG_INFOF("Processing target %d: bearing=%.1f°, distance=%.1fm",
                         targetId, targetBearing, targetDistance);

                // ========== 第七层检查：为每个声纳处理目标 ==========
                for (int sonarID = 0; sonarID < 4; sonarID++) {
                    LOG_INFOF("--- Processing target %d for sonar %d ---", targetId, sonarID);

                    try {
                        // 检查声纳是否启用
                        auto stateIt = m_sonarStates.find(sonarID);
                        if (stateIt == m_sonarStates.end()) {
                            LOG_INFOF("Sonar %d state not found, skipping", sonarID);
                            continue;
                        }

                        if (!stateIt->second.arrayWorkingState || !stateIt->second.passiveWorkingState) {
                            LOG_INFOF("Sonar %d not enabled (array:%d, passive:%d), skipping",
                                     sonarID, stateIt->second.arrayWorkingState, stateIt->second.passiveWorkingState);
                            continue; // 声纳未启用，跳过
                        }

                        // 使用修复后的角度范围检查方法
                        if (!isTargetInSonarRange(sonarID, targetBearing, targetDistance)) {
                            LOG_INFOF("Target %d not in sonar %d range, skipping", targetId, sonarID);
                            continue; // 不在探测范围内，跳过
                        }

                        LOG_INFOF("✓ Target %d is valid for sonar %d", targetId, sonarID);

                        auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];
                        LOG_INFOF("Sonar %d current targets count: %zu", sonarID, targetsData.size());

                        // 检查是否已达到最大目标数限制
                        if (targetsData.size() >= MAX_TARGETS_PER_SONAR) {
                            LOG_INFOF("Sonar %d reached max targets (%d), checking for replacement...",
                                     sonarID, MAX_TARGETS_PER_SONAR);

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

                        LOG_INFOF("--- Creating target data for target %d sonar %d ---", targetId, sonarID);

                        TargetData targetData;
                        targetData.targetId = targetId;
                        targetData.targetDistance = targetDistance;
                        targetData.targetBearing = targetBearing;
                        targetData.lastUpdateTime = currentTime;
                        targetData.isValid = true;

                        // ========== 第八层检查：频谱数据复制 ==========
                        LOG_INFOF("--- Copying spectrum data for target %d sonar %d ---", targetId, sonarID);
                        try {
                            targetData.propagatedSpectrum.reserve(SPECTRUM_DATA_SIZE);
                            LOG_INFOF("Reserved spectrum vector for %d elements", SPECTRUM_DATA_SIZE);

                            for (int i = 0; i < SPECTRUM_DATA_SIZE; i++) {
                                // 在关键位置进行额外检查
                                if (i == 0 || i == SPECTRUM_DATA_SIZE/2 || i == SPECTRUM_DATA_SIZE-1) {
                                    LOG_INFOF("Copying spectrum[%d] = %.6f", i, soundData.spectrumData[i]);
                                }
                                targetData.propagatedSpectrum.push_back(soundData.spectrumData[i]);
                            }
                            LOG_INFOF("✓ Successfully copied %d spectrum elements", SPECTRUM_DATA_SIZE);

                        } catch (const std::exception& e) {
                            LOG_ERRORF("CRASH: Failed to copy spectrum data for target %d sonar %d with exception: %s",
                                      targetId, sonarID, e.what());
                            return;
                        } catch (...) {
                            LOG_ERRORF("CRASH: Failed to copy spectrum data for target %d sonar %d with unknown exception",
                                      targetId, sonarID);
                            return;
                        }

                        // ========== 第九层检查：更新目标数据 ==========
                        LOG_INFOF("--- Updating target data for target %d sonar %d ---", targetId, sonarID);
                        try {
                            if (existingIt != targetsData.end()) {
                                // 更新现有目标数据
                                LOG_INFOF("Updating existing target %d for sonar %d", targetId, sonarID);
                                *existingIt = targetData;
                            } else {
                                // 添加新目标数据
                                LOG_INFOF("Adding new target %d for sonar %d", targetId, sonarID);
                                targetsData.push_back(targetData);
                            }
                            LOG_INFOF("✓ Target %d data updated successfully for sonar %d", targetId, sonarID);

                        } catch (const std::exception& e) {
                            LOG_ERRORF("CRASH: Failed to update target data for target %d sonar %d with exception: %s",
                                      targetId, sonarID, e.what());
                            return;
                        } catch (...) {
                            LOG_ERRORF("CRASH: Failed to update target data for target %d sonar %d with unknown exception",
                                      targetId, sonarID);
                            return;
                        }

                    } catch (const std::exception& e) {
                        LOG_ERRORF("CRASH: Exception in sonar %d processing for target %d: %s",
                                  sonarID, targetId, e.what());
                        return;
                    } catch (...) {
                        LOG_ERRORF("CRASH: Unknown exception in sonar %d processing for target %d",
                                  sonarID, targetId);
                        return;
                    }
                }

                targetIndex++;
                LOG_INFOF("✓ Completed processing target index %d", targetIndex - 1);

            } // end for each soundData

        } catch (const std::exception& e) {
            LOG_ERRORF("CRASH: Exception during target iteration at index %d: %s", targetIndex, e.what());
            return;
        } catch (...) {
            LOG_ERRORF("CRASH: Unknown exception during target iteration at index %d", targetIndex);
            return;
        }

        LOG_INFOF("✓ Successfully processed all %d targets", targetIndex);
        LOG_INFOF("=== EXITING updateMultiTargetPropagatedSoundCache SUCCESSFULLY ===");

    } catch (const std::exception& e) {
        LOG_ERRORF("CRASH: Top-level exception in updateMultiTargetPropagatedSoundCache: %s", e.what());

        // 尝试获取堆栈跟踪信息
        #ifdef _WIN32
        // Windows下的堆栈跟踪
        LOG_ERRORF("Stack trace information may be available in debugger");
        #endif

        return;
    } catch (...) {
        LOG_ERRORF("CRASH: Top-level unknown exception in updateMultiTargetPropagatedSoundCache");
        return;
    }
}


// 增强版本的updateMultiTargetPropagatedSoundCache - 带内存保护
/**
void DeviceModel::updateMultiTargetPropagatedSoundCache_Enhanced(CSimMessage* simMessage)
{
    LOG_INFOF("=== ENHANCED updateMultiTargetPropagatedSoundCache START ===");

    // 使用原有的调试版本，但增加额外的内存保护检查

    // 验证simMessage基本结构
    if (!simMessage) {
        LOG_ERRORF("FATAL: simMessage is NULL");
        __debugbreak(); // 在Windows下触发调试器断点
        return;
    }

    // 检查内存是否可读
    if (IsBadReadPtr(simMessage, sizeof(CSimMessage))) {
        LOG_ERRORF("FATAL: simMessage memory is not readable");
        return;
    }

    if (!simMessage->data) {
        LOG_ERRORF("FATAL: simMessage->data is NULL");
        return;
    }

    // 检查data内存是否可读
    if (IsBadReadPtr(simMessage->data, simMessage->length)) {
        LOG_ERRORF("FATAL: simMessage->data memory is not readable, length=%d", simMessage->length);
        return;
    }

    // 尝试安全的类型转换
    const CMsg_PropagatedContinuousSoundListStruct* soundListStruct = nullptr;

    __try {
        soundListStruct = reinterpret_cast<const CMsg_PropagatedContinuousSoundListStruct*>(simMessage->data);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERRORF("FATAL: Exception during reinterpret_cast, exception code: 0x%x", GetExceptionCode());
        return;
    }

    if (!soundListStruct) {
        LOG_ERRORF("FATAL: soundListStruct is NULL after cast");
        return;
    }

    // 尝试安全访问list大小
    size_t listSize = 0;
    __try {
        listSize = soundListStruct->propagatedContinuousList.size();
        LOG_INFOF("✓ List size: %zu", listSize);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERRORF("FATAL: Exception accessing list size, exception code: 0x%x", GetExceptionCode());
        return;
    }

    // 如果列表过大，可能是数据损坏
    if (listSize > 1000) { // 合理的上限
        LOG_ERRORF("SUSPICIOUS: List size too large: %zu, possible data corruption", listSize);
        return;
    }

    int64 currentTime = simMessage->time;

    // 处理空列表
    if (listSize == 0) {
        LOG_INFO("Empty list, clearing data");
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            m_multiTargetCache.sonarTargetsData[sonarID].clear();
            m_multiTargetCache.multiTargetEquationResults[sonarID].clear();
        }
        return;
    }

    // 清理过期数据
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];
        size_t beforeSize = targetsData.size();

        targetsData.erase(
            std::remove_if(targetsData.begin(), targetsData.end(),
                [currentTime](const TargetData& target) {
                    return (currentTime - target.lastUpdateTime) > DATA_UPDATE_INTERVAL_MS;
                }),
            targetsData.end()
        );

        LOG_INFOF("Sonar %d: cleaned %zu expired targets", sonarID, beforeSize - targetsData.size());
    }

    // 处理每个目标 - 使用安全的迭代器
    int targetIndex = 0;

    __try {
        for (auto it = soundListStruct->propagatedContinuousList.begin();
             it != soundListStruct->propagatedContinuousList.end(); ++it) {

            LOG_INFOF("=== Processing target %d ===", targetIndex);

            const auto& soundData = *it;

            // 详细打印目标数据
            LOG_INFOF("Target %d data:", targetIndex);
            LOG_INFOF("  - arrivalSideAngle: %.6f", soundData.arrivalSideAngle);
            LOG_INFOF("  - arrivalPitchAngle: %.6f", soundData.arrivalPitchAngle);
            LOG_INFOF("  - targetDistance: %.6f", soundData.targetDistance);
            LOG_INFOF("  - platType: %d", soundData.platType);

            // 验证数据合理性
            if (soundData.targetDistance < 0 || soundData.targetDistance > 1000000) {
                LOG_WARNF("SUSPICIOUS: Target %d distance out of range: %.2f",
                         targetIndex, soundData.targetDistance);
            }

            if (soundData.arrivalSideAngle < -360 || soundData.arrivalSideAngle > 360) {
                LOG_WARNF("SUSPICIOUS: Target %d angle out of range: %.2f",
                         targetIndex, soundData.arrivalSideAngle);
            }

            // 安全访问频谱数据 - 检查关键位置
            float spectrumValue;
            std::vector<int> checkIndices = {0, 1, 100, 1000, 2648, 5000, 5294, 5295};

            for (int idx : checkIndices) {
                if (!safeAccessSpectrumData(soundData, idx, spectrumValue)) {
                    LOG_ERRORF("FATAL: Failed to access spectrum[%d] for target %d", idx, targetIndex);
                    return;
                }
                LOG_INFOF("  - spectrum[%d]: %.6f", idx, spectrumValue);
            }

            // 检查频谱数据是否全为0或异常值
            bool hasValidSpectrum = false;
            int nonZeroCount = 0;
            for (int i = 0; i < 5296; i += 100) { // 采样检查
                if (safeAccessSpectrumData(soundData, i, spectrumValue)) {
                    if (spectrumValue != 0.0f) {
                        nonZeroCount++;
                        if (spectrumValue > 1e-10 && spectrumValue < 1e10) { // 合理范围
                            hasValidSpectrum = true;
                        }
                    }
                }
            }

            LOG_INFOF("Target %d spectrum analysis: hasValid=%s, nonZeroCount=%d",
                     targetIndex, hasValidSpectrum ? "YES" : "NO", nonZeroCount);

            if (!hasValidSpectrum) {
                LOG_WARNF("WARNING: Target %d has suspicious spectrum data", targetIndex);
            }

            // 继续处理目标（为每个声纳检查）...
            int targetId = targetIndex + 1000;
            float targetBearing = soundData.arrivalSideAngle;
            float targetDistance = soundData.targetDistance;

            for (int sonarID = 0; sonarID < 4; sonarID++) {
                // 检查声纳状态
                auto stateIt = m_sonarStates.find(sonarID);
                if (stateIt == m_sonarStates.end() ||
                    !stateIt->second.arrayWorkingState ||
                    !stateIt->second.passiveWorkingState) {
                    continue;
                }

                // 检查范围
                if (!isTargetInSonarRange(sonarID, targetBearing, targetDistance)) {
                    continue;
                }

                auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];

                // 限制检查
                if (targetsData.size() >= MAX_TARGETS_PER_SONAR) {
                    auto farthestIt = std::max_element(targetsData.begin(), targetsData.end(),
                        [](const TargetData& a, const TargetData& b) {
                            return a.targetDistance < b.targetDistance;
                        });

                    if (farthestIt != targetsData.end() && targetDistance < farthestIt->targetDistance) {
                        targetsData.erase(farthestIt);
                    } else {
                        continue;
                    }
                }

                // 创建目标数据
                TargetData targetData;
                targetData.targetId = targetId;
                targetData.targetDistance = targetDistance;
                targetData.targetBearing = targetBearing;
                targetData.lastUpdateTime = currentTime;
                targetData.isValid = true;

                // 安全复制频谱数据
                targetData.propagatedSpectrum.reserve(5296);
                bool copySuccess = true;

                for (int i = 0; i < 5296; i++) {
                    float value;
                    if (safeAccessSpectrumData(soundData, i, value)) {
                        targetData.propagatedSpectrum.push_back(value);
                    } else {
                        LOG_ERRORF("FATAL: Failed to copy spectrum[%d] for target %d sonar %d",
                                  i, targetId, sonarID);
                        copySuccess = false;
                        break;
                    }
                }

                if (!copySuccess) {
                    LOG_ERRORF("FATAL: Spectrum copy failed for target %d sonar %d", targetId, sonarID);
                    return;
                }

                // 更新或添加目标
                auto existingIt = std::find_if(targetsData.begin(), targetsData.end(),
                    [targetId](const TargetData& target) {
                        return target.targetId == targetId;
                    });

                if (existingIt != targetsData.end()) {
                    *existingIt = targetData;
                    LOG_INFOF("✓ Updated target %d for sonar %d", targetId, sonarID);
                } else {
                    targetsData.push_back(targetData);
                    LOG_INFOF("✓ Added new target %d for sonar %d", targetId, sonarID);
                }
            }

            targetIndex++;
        }

    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERRORF("FATAL: Exception during target processing at index %d, exception code: 0x%x",
                  targetIndex, GetExceptionCode());
        return;
    }

    LOG_INFOF("✓ Successfully processed %d targets", targetIndex);
    LOG_INFOF("=== ENHANCED updateMultiTargetPropagatedSoundCache END ===");
}
**/

void DeviceModel::updateMultiTargetPropagatedSoundCache(CSimMessage* simMessage)
{
    // 崩溃保护计数检查
    if (m_crashProtectionCount >= m_maxCrashProtections) {
        LOG_CRASH("Maximum crash protections reached (%d), function disabled to prevent infinite loops",
                 m_maxCrashProtections);
        return;
    }

    m_crashProtectionCount++;

    try {
        LOG_SAFE_INFO("=== 防崩溃 updateMultiTargetPropagatedSoundCache START (Protection #%d) ===",
                     m_crashProtectionCount);

        // ========== 第一级保护：基本参数验证 ==========
        if (!SAFE_ACCESS_PTR(simMessage, sizeof(CSimMessage), "simMessage")) {
            emergencyCleanup("simMessage pointer validation failed", __FILENAME__, __FUNCTION__, __LINE__);
            return;
        }

        if (!SAFE_ACCESS_PTR(simMessage->data, simMessage->length, "simMessage->data")) {
            emergencyCleanup("simMessage->data pointer validation failed", __FILENAME__, __FUNCTION__, __LINE__);
            return;
        }

        LOG_SAFE_INFO("✓ Basic pointer validation passed");
        LOG_SAFE_INFO("Message details: time=%lld, sender=%d, length=%d, data=%p",
                     simMessage->time, simMessage->sender, simMessage->length, simMessage->data);

        // ========== 第二级保护：数据结构转换 ==========
        const CMsg_PropagatedContinuousSoundListStruct* soundListStruct = nullptr;

        try {
            // 检查数据长度是否足够
            if (simMessage->length < sizeof(CMsg_PropagatedContinuousSoundListStruct)) {
                LOG_SAFE_ERROR("Message length too small: %d < %zu",
                              simMessage->length, sizeof(CMsg_PropagatedContinuousSoundListStruct));
                emergencyCleanup("Insufficient message length", __FILENAME__, __FUNCTION__, __LINE__);
                return;
            }

            soundListStruct = reinterpret_cast<const CMsg_PropagatedContinuousSoundListStruct*>(simMessage->data);

            if (!SAFE_ACCESS_PTR(soundListStruct, sizeof(CMsg_PropagatedContinuousSoundListStruct), "soundListStruct")) {
                emergencyCleanup("soundListStruct pointer validation failed", __FILENAME__, __FUNCTION__, __LINE__);
                return;
            }

            LOG_SAFE_INFO("✓ Data structure conversion successful");

        } catch (const std::bad_cast& e) {
            LOG_CRASH("Bad cast exception during reinterpret_cast: %s", e.what());
            emergencyCleanup("Bad cast during type conversion", __FILENAME__, __FUNCTION__, __LINE__);
            return;
        } catch (const std::exception& e) {
            LOG_CRASH("Exception during data structure conversion: %s", e.what());
            emergencyCleanup("Exception during type conversion", __FILENAME__, __FUNCTION__, __LINE__);
            return;
        } catch (...) {
            LOG_CRASH("Unknown exception during data structure conversion");
            emergencyCleanup("Unknown exception during type conversion", __FILENAME__, __FUNCTION__, __LINE__);
            return;
        }

        // ========== 第三级保护：STL容器访问 ==========
        size_t listSize = 0;
        if (!SAFE_ACCESS_CONTAINER(soundListStruct->propagatedContinuousList, listSize)) {
            emergencyCleanup("STL container access failed", __FILENAME__, __FUNCTION__, __LINE__);
            return;
        }

        LOG_SAFE_INFO("✓ Container access successful, size: %zu", listSize);

        int64 currentTime = simMessage->time;

        // ========== 第四级保护：处理空列表 ==========
        if (listSize == 0) {
            LOG_SAFE_INFO("Empty target list received, performing safe cleanup");

            try {
                for (int sonarID = 0; sonarID < 4; sonarID++) {
                    safeResetTargetCache(sonarID, __FILENAME__, __FUNCTION__, __LINE__);
                }
                LOG_SAFE_INFO("✓ Safe cleanup completed for empty list");
                return;

            } catch (const std::exception& e) {
                LOG_CRASH("Exception during empty list cleanup: %s", e.what());
                emergencyCleanup("Exception during empty list cleanup", __FILENAME__, __FUNCTION__, __LINE__);
                return;
            } catch (...) {
                LOG_CRASH("Unknown exception during empty list cleanup");
                emergencyCleanup("Unknown exception during empty list cleanup", __FILENAME__, __FUNCTION__, __LINE__);
                return;
            }
        }

        // ========== 第五级保护：清理过期数据 ==========
        LOG_SAFE_INFO("Performing safe cleanup of expired data");

        try {
            for (int sonarID = 0; sonarID < 4; sonarID++) {
                try {
                    auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];
                    size_t beforeSize = targetsData.size();

                    // 安全的过期数据移除
                    auto removeIt = std::remove_if(targetsData.begin(), targetsData.end(),
                        [currentTime](const TargetData& target) -> bool {
                            try {
                                return (currentTime - target.lastUpdateTime) > DATA_UPDATE_INTERVAL_MS;
                            } catch (...) {
                                return true; // 如果比较出错，就移除这个目标
                            }
                        });

                    targetsData.erase(removeIt, targetsData.end());

                    size_t afterSize = targetsData.size();
                    LOG_SAFE_INFO("Sonar %d: cleaned %zu expired targets (before=%zu, after=%zu)",
                                 sonarID, beforeSize - afterSize, beforeSize, afterSize);

                } catch (const std::exception& e) {
                    LOG_CRASH("Exception cleaning sonar %d expired data: %s", sonarID, e.what());
                    safeResetTargetCache(sonarID, __FILENAME__, __FUNCTION__, __LINE__);
                } catch (...) {
                    LOG_CRASH("Unknown exception cleaning sonar %d expired data", sonarID);
                    safeResetTargetCache(sonarID, __FILENAME__, __FUNCTION__, __LINE__);
                }
            }

            LOG_SAFE_INFO("✓ Expired data cleanup completed");

        } catch (const std::exception& e) {
            LOG_CRASH("Exception during expired data cleanup: %s", e.what());
            emergencyCleanup("Exception during expired data cleanup", __FILENAME__, __FUNCTION__, __LINE__);
            return;
        } catch (...) {
            LOG_CRASH("Unknown exception during expired data cleanup");
            emergencyCleanup("Unknown exception during expired data cleanup", __FILENAME__, __FUNCTION__, __LINE__);
            return;
        }

        // ========== 第六级保护：安全迭代目标数据 ==========
        LOG_SAFE_INFO("Starting safe iteration of %zu targets", listSize);

        // 使用安全的迭代器处理
        bool iterationSuccess = safeIterateSTLContainer(
            soundListStruct->propagatedContinuousList,
            [this, currentTime](const C_PropagatedContinuousSoundStruct& soundData, int targetIndex) -> bool {

                try {
                    LOG_SAFE_INFO("=== Processing target %d ===", targetIndex);
                    LOG_SAFE_INFO("Target %d spectrumData size: %zu elements", targetIndex, sizeof(soundData.spectrumData)/sizeof(float));

                    // ========== 第七级保护：验证目标数据 ==========
                    // 验证基本数据字段
                    if (std::isnan(soundData.arrivalSideAngle) || std::isinf(soundData.arrivalSideAngle) ||
                        soundData.arrivalSideAngle < -360.0f || soundData.arrivalSideAngle > 360.0f) {
                        LOG_SAFE_WARN("Invalid arrivalSideAngle for target %d: %f", targetIndex, soundData.arrivalSideAngle);
                        return true; // 跳过这个目标，继续处理下一个
                    }

                    if (std::isnan(soundData.targetDistance) || std::isinf(soundData.targetDistance) ||
                        soundData.targetDistance < 0.0f || soundData.targetDistance > 1000000.0f) {
                        LOG_SAFE_WARN("Invalid targetDistance for target %d: %f", targetIndex, soundData.targetDistance);
                        return true; // 跳过这个目标，继续处理下一个
                    }

                    LOG_SAFE_INFO("Target %d data: angle=%.3f°, distance=%.3fm, platType=%d",
                                 targetIndex, soundData.arrivalSideAngle, soundData.targetDistance, soundData.platType);

                    // ========== 第八级保护：验证频谱数据 ==========
                    LOG_SAFE_INFO("Validating spectrum data for target %d", targetIndex);
                    LOG_SAFE_INFO("Target %d spectrumData array size: %zu elements", targetIndex, sizeof(soundData.spectrumData)/sizeof(float));

                    // 检查关键位置的频谱数据
                    std::vector<int> testIndices = {0, 1, 100, 1000, 2648, 5000, 5294, 5295};
                    bool spectrumValid = true;

                    for (int idx : testIndices) {
                        float value;
                        if (!SAFE_ACCESS_SPECTRUM(soundData.spectrumData, idx, value)) {
                            LOG_SAFE_ERROR("Failed to access spectrum[%d] for target %d", idx, targetIndex);
                            spectrumValid = false;
                            break;
                        }

                        if (idx < 50) { // 只打印前几个避免日志过多
                            LOG_SAFE_INFO("spectrum[%d] = %.6f", idx, value);
                        }
                    }

                    if (!spectrumValid) {
                        LOG_SAFE_WARN("Spectrum validation failed for target %d, skipping", targetIndex);
                        return true; // 跳过这个目标
                    }

                    // ========== 第九级保护：为每个声纳处理目标 ==========
                    int targetId = targetIndex + 1000;
                    float targetBearing = soundData.arrivalSideAngle;
                    float targetDistance = soundData.targetDistance;

                    for (int sonarID = 0; sonarID < 4; sonarID++) {
                        try {
                            LOG_SAFE_INFO("Processing target %d for sonar %d", targetId, sonarID);

                            // 声纳状态检查
                            auto stateIt = m_sonarStates.find(sonarID);
                            if (stateIt == m_sonarStates.end()) {
                                LOG_SAFE_INFO("Sonar %d state not found, skipping", sonarID);
                                continue;
                            }

                            if (!stateIt->second.arrayWorkingState || !stateIt->second.passiveWorkingState) {
                                LOG_SAFE_INFO("Sonar %d not enabled, skipping", sonarID);
                                continue;
                            }

                            // 范围检查
                            if (!isTargetInSonarRange(sonarID, targetBearing, targetDistance)) {
                                LOG_SAFE_INFO("Target %d not in sonar %d range, skipping", targetId, sonarID);
                                continue;
                            }

                            // ========== 第十级保护：安全的目标数据管理 ==========
                            auto& targetsData = m_multiTargetCache.sonarTargetsData[sonarID];

                            // 检查目标数限制
                            if (targetsData.size() >= MAX_TARGETS_PER_SONAR) {
                                LOG_SAFE_INFO("Sonar %d at max capacity (%d), checking replacement",
                                             sonarID, MAX_TARGETS_PER_SONAR);

                                try {
                                    auto farthestIt = std::max_element(targetsData.begin(), targetsData.end(),
                                        [](const TargetData& a, const TargetData& b) -> bool {
                                            try {
                                                return a.targetDistance < b.targetDistance;
                                            } catch (...) {
                                                return false; // 如果比较出错，保持原顺序
                                            }
                                        });

                                    if (farthestIt != targetsData.end() && targetDistance < farthestIt->targetDistance) {
                                        LOG_SAFE_INFO("Replacing farthest target (%.1fm) with closer target (%.1fm)",
                                                      farthestIt->targetDistance, targetDistance);
                                        targetsData.erase(farthestIt);
                                    } else {
                                        LOG_SAFE_INFO("New target not closer than existing, skipping");
                                        continue;
                                    }

                                } catch (const std::exception& e) {
                                    LOG_CRASH("Exception during target replacement for sonar %d: %s", sonarID, e.what());
                                    continue;
                                } catch (...) {
                                    LOG_CRASH("Unknown exception during target replacement for sonar %d", sonarID);
                                    continue;
                                }
                            }

                            // ========== 第十一级保护：创建和复制目标数据 ==========
                            try {
                                TargetData targetData;
                                targetData.targetId = targetId;
                                targetData.targetDistance = targetDistance;
                                targetData.targetBearing = targetBearing;
                                targetData.lastUpdateTime = currentTime;
                                targetData.isValid = true;

                                // 安全复制频谱数据
                                LOG_SAFE_INFO("Copying spectrum data for target %d sonar %d", targetId, sonarID);

                                targetData.propagatedSpectrum.clear();
                                targetData.propagatedSpectrum.reserve(5296);

                                bool copySuccess = true;
                                for (int i = 0; i < 5296; i++) {
                                    float value;
                                    if (SAFE_ACCESS_SPECTRUM(soundData.spectrumData, i, value)) {
                                        targetData.propagatedSpectrum.push_back(value);
                                    } else {
                                        LOG_SAFE_ERROR("Failed to copy spectrum[%d], using 0.0", i);
                                        targetData.propagatedSpectrum.push_back(0.0f);
                                        copySuccess = false;
                                    }
                                }

                                if (!copySuccess) {
                                    LOG_SAFE_WARN("Spectrum copy had errors for target %d sonar %d, but continuing",
                                                  targetId, sonarID);
                                }

                                // 安全更新目标数据
                                auto existingIt = std::find_if(targetsData.begin(), targetsData.end(),
                                    [targetId](const TargetData& target) -> bool {
                                        try {
                                            return target.targetId == targetId;
                                        } catch (...) {
                                            return false;
                                        }
                                    });

                                if (existingIt != targetsData.end()) {
                                    *existingIt = std::move(targetData);
                                    LOG_SAFE_INFO("✓ Updated existing target %d for sonar %d", targetId, sonarID);
                                } else {
                                    targetsData.push_back(std::move(targetData));
                                    LOG_SAFE_INFO("✓ Added new target %d for sonar %d", targetId, sonarID);
                                }

                            } catch (const std::bad_alloc& e) {
                                LOG_CRASH("Memory allocation failed for target %d sonar %d: %s", targetId, sonarID, e.what());
                                continue;
                            } catch (const std::exception& e) {
                                LOG_CRASH("Exception creating target data for target %d sonar %d: %s", targetId, sonarID, e.what());
                                continue;
                            } catch (...) {
                                LOG_CRASH("Unknown exception creating target data for target %d sonar %d", targetId, sonarID);
                                continue;
                            }

                        } catch (const std::exception& e) {
                            LOG_CRASH("Exception in sonar %d processing for target %d: %s", sonarID, targetId, e.what());
                            continue;
                        } catch (...) {
                            LOG_CRASH("Unknown exception in sonar %d processing for target %d", sonarID, targetId);
                            continue;
                        }
                    }

                    LOG_SAFE_INFO("✓ Completed processing target %d", targetIndex);
                    return true; // 继续处理下一个目标

                } catch (const std::exception& e) {
                    LOG_CRASH("Exception processing target %d: %s", targetIndex, e.what());
                    return true; // 继续处理下一个目标，不中断整个过程
                } catch (...) {
                    LOG_CRASH("Unknown exception processing target %d", targetIndex);
                    return true; // 继续处理下一个目标
                }
            },
            __FILENAME__, __FUNCTION__, __LINE__
        );

        if (!iterationSuccess) {
            LOG_SAFE_ERROR("Target iteration failed, but function completed safely");
        } else {
            LOG_SAFE_INFO("✓ All targets processed successfully");
        }

        // 重置崩溃保护计数（成功完成）
        if (m_crashProtectionCount > 0) {
            m_crashProtectionCount--;
        }

        LOG_SAFE_INFO("=== 防崩溃 updateMultiTargetPropagatedSoundCache END SUCCESS ===");

    } catch (const std::exception& e) {
        LOG_CRASH("Top-level exception in updateMultiTargetPropagatedSoundCache: %s", e.what());
        emergencyCleanup("Top-level exception", __FILENAME__, __FUNCTION__, __LINE__);
    } catch (...) {
        LOG_CRASH("Top-level unknown exception in updateMultiTargetPropagatedSoundCache");
        emergencyCleanup("Top-level unknown exception", __FILENAME__, __FUNCTION__, __LINE__);
    }
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

        // 发送被动声呐探测结果
        sendPassiveSonarResultsInStep();
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
              sonarID, targetCachePlatformSpectrumVector->second.size(),
              targetCacheEnvironmentSpectrumVector->second.size(), targetCachePropagatedSpectrum.propagatedSpectrum.size());

    // ############# 步骤1：根据声纳类型计算对应频率范围的频谱累加求和 #############
    double propagatedSum = calculateSpectrumSumByFreqRange(targetCachePropagatedSpectrum.propagatedSpectrum, sonarID);     // |阵元谱级|
    double platformSum = calculateSpectrumSumByFreqRange(targetCachePlatformSpectrumVector->second, sonarID);                  // |平台背景|
    double environmentSum = calculateSpectrumSumByFreqRange(targetCacheEnvironmentSpectrumVector->second, sonarID);            // |海洋噪声|

    LOG_INFOF("Spectrum sums (frequency range filtered) >>>>>> propagated:%.2f, platform:%.2f, environment:%.2f",
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

    // ############# 步骤4：计算动态频率并计算DI值 #############
    // 这里是关键修改：根据传播频谱动态计算频率
    double dynamicFrequency = calculateMedianFrequencyFromSpectrum(targetCachePropagatedSpectrum.propagatedSpectrum, sonarID);
    double di = calculateDynamicDI(sonarID, dynamicFrequency);

    // ############# 步骤5：计算最终结果 X = SL-TL-NL + DI #############
    double result = sl_tl_nl + di;

    // 获取当前声纳的探测阈值
    double threshold = getEffectiveThreshold(sonarID);

    LOG_DEBUGF("声纳%d目标%d方程计算: SL-TL-NL=%.2f, 动态频率=%.3fkHz, DI=%.2f, X=%.2f, 阈值=%.2f, 可探测=%s",
               sonarID, targetCachePropagatedSpectrum.targetId, sl_tl_nl, dynamicFrequency, di, result, threshold,
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





int DeviceModel::getSpectrumIndexFromFrequency(int frequency_hz)
{
    if (frequency_hz < 10) {
        return 0;  // 最小频率为10Hz
    }

    if (frequency_hz <= 10000) {
        // 10Hz-10kHz范围：每2Hz一个点
        // 索引 = (频率 - 10) / 2
        return (frequency_hz - 10) / 2;
    } else if (frequency_hz <= 40000) {
        // 10kHz-40kHz范围：每100Hz一个点
        // 前面10Hz-10kHz有4995个点，然后从10kHz开始每100Hz一个点
        int base_index = 4995;  // 10Hz-10kHz的点数
        return base_index + (frequency_hz - 10000) / 100;
    } else {
        // 超过40kHz的频率，返回最大索引
        return SPECTRUM_DATA_SIZE - 1;
    }
}

double DeviceModel::calculateSpectrumSumByFreqRange(const std::vector<float>& spectrum, int sonarID)
{
    if (spectrum.empty() || spectrum.size() != SPECTRUM_DATA_SIZE) {
        LOG_WARNF("Invalid spectrum data size: %zu, expected: %d", spectrum.size(), SPECTRUM_DATA_SIZE);
        return 0.0;
    }

    int start_freq_hz, end_freq_hz;

    // 根据声纳ID确定有效频率范围
    switch (sonarID) {
        case 0:  // 艏端声纳：500Hz-7500Hz
            start_freq_hz = 500;
            end_freq_hz = 7500;
            break;

        case 1:  // 舷侧声纳：400Hz-3200Hz
            start_freq_hz = 400;
            end_freq_hz = 3200;
            break;

        case 2:  // 粗拖声纳：47Hz-750Hz (从48Hz开始，因为47是奇数)
            start_freq_hz = 48;  // 从48Hz开始
            end_freq_hz = 750;
            break;

        case 3:  // 细拖声纳：10Hz-500Hz
            start_freq_hz = 10;
            end_freq_hz = 500;
            break;

        default:
            LOG_WARNF("Unknown sonar ID: %d, using full spectrum", sonarID);
            return calculateSpectrumSum(spectrum);  // 使用原始的全频谱求和
    }

    // 计算对应的数组索引范围
    int start_index = getSpectrumIndexFromFrequency(start_freq_hz);
    int end_index = getSpectrumIndexFromFrequency(end_freq_hz);

    // 确保索引在有效范围内
    start_index = std::max(0, start_index);
    end_index = std::min(static_cast<int>(spectrum.size()) - 1, end_index);

    if (start_index > end_index) {
        LOG_WARNF("Invalid frequency range for sonar %d: start_index=%d, end_index=%d",
                  sonarID, start_index, end_index);
        return 0.0;
    }

    // 计算指定范围内的频谱求和
    double sum = 0.0;
    for (int i = start_index; i <= end_index; i++) {
        sum += static_cast<double>(spectrum[i]);
    }

    int freq_count = end_index - start_index + 1;
    const char* sonar_names[] = {"艏端", "舷侧", "粗拖", "细拖"};

    LOG_INFOF("声纳%d(%s) 频率范围: %dHz-%dHz, 索引范围: %d-%d, 频率点数: %d, 累加和: %.2f",
              sonarID, sonar_names[sonarID], start_freq_hz, end_freq_hz,
              start_index, end_index, freq_count, sum);

    return sum;
}










double DeviceModel::calculateMedianFrequencyFromSpectrum(const std::vector<float>& spectrum, int sonarID)
{
    if (spectrum.empty() || spectrum.size() != SPECTRUM_DATA_SIZE) {
        LOG_WARNF("Invalid spectrum data size: %zu, expected: %d", spectrum.size(), SPECTRUM_DATA_SIZE);
        return 0.0;
    }

    int start_freq_hz, end_freq_hz;

    // 根据声纳ID确定有效频率范围
    switch (sonarID) {
        case 0:  // 艏端声纳：500Hz-7500Hz
            start_freq_hz = 500;
            end_freq_hz = 7500;
            break;
        case 1:  // 舷侧声纳：400Hz-3200Hz
            start_freq_hz = 400;
            end_freq_hz = 3200;
            break;
        case 2:  // 粗拖声纳：48Hz-750Hz
            start_freq_hz = 48;
            end_freq_hz = 750;
            break;
        case 3:  // 细拖声纳：10Hz-500Hz
            start_freq_hz = 10;
            end_freq_hz = 500;
            break;
        default:
            LOG_WARNF("Unknown sonar ID: %d", sonarID);
            return 0.0;
    }

    // 计算对应的数组索引范围
    int start_index = getSpectrumIndexFromFrequency(start_freq_hz);
    int end_index = getSpectrumIndexFromFrequency(end_freq_hz);

    // 确保索引在有效范围内
    start_index = std::max(0, start_index);
    end_index = std::min(static_cast<int>(spectrum.size()) - 1, end_index);

    if (start_index > end_index) {
        LOG_WARNF("Invalid frequency range for sonar %d: start_index=%d, end_index=%d",
                  sonarID, start_index, end_index);
        return 0.0;
    }

    // 计算总的频谱能量
    double totalEnergy = 0.0;
    for (int i = start_index; i <= end_index; i++) {
        totalEnergy += static_cast<double>(spectrum[i]);
    }

    if (totalEnergy <= 0.0) {
        LOG_WARNF("Zero total energy in frequency range for sonar %d", sonarID);
        return 0.0;
    }

    // 寻找中位数频率：累积能量达到总能量一半的频率
    double halfEnergy = totalEnergy / 2.0;
    double cumulativeEnergy = 0.0;
    int medianIndex = start_index;

    for (int i = start_index; i <= end_index; i++) {
        cumulativeEnergy += static_cast<double>(spectrum[i]);
        if (cumulativeEnergy >= halfEnergy) {
            medianIndex = i;
            break;
        }
    }

    // 将索引转换回频率值(Hz)
    double medianFreq_hz = 0.0;
    if (medianIndex < 4995) {
        // 10Hz-10kHz范围：每2Hz一个点
        medianFreq_hz = 10.0 + medianIndex * 2.0;
    } else {
        // 10kHz-40kHz范围：每100Hz一个点
        medianFreq_hz = 10000.0 + (medianIndex - 4995) * 100.0;
    }

    // 转换为kHz
    double medianFreq_khz = medianFreq_hz / 1000.0;

    const char* sonar_names[] = {"艏端", "舷侧", "粗拖", "细拖"};
    LOG_INFOF("声纳%d(%s) 中位数频率计算: 范围%dHz-%dHz, 索引%d-%d, 中位数索引=%d, 频率=%.3fkHz",
              sonarID, sonar_names[sonarID], start_freq_hz, end_freq_hz,
              start_index, end_index, medianIndex, medianFreq_khz);

    LOG_INFOF("!!!!!medianFreq_khz : %.3f", medianFreq_khz);
    return medianFreq_khz;
}

double DeviceModel::calculateDynamicDI(int sonarID, double dynamicFrequency)
{
    // 检查声纳ID是否有效
    if (m_diParameters.find(sonarID) == m_diParameters.end()) {
        LOG_WARNF("No DI parameters found for sonar %d, using default", sonarID);
        return 9.5;  // 默认值
    }

    const DIParameters& params = m_diParameters[sonarID];

    // 根据声纳类型设置频率上限并应用限制
    double frequency = dynamicFrequency;
    double maxFrequency = 0.0;

    switch (sonarID) {
        case 0:  // 艏端声纳：最大5kHz
            maxFrequency = 5.0;
            break;
        case 1:  // 舷侧声纳：最大3kHz
            maxFrequency = 3.0;
            break;
        case 2:  // 粗拖声纳：最大0.5kHz
            maxFrequency = 0.5;
            break;
        case 3:  // 细拖声纳：最大0.5kHz
            maxFrequency = 0.5;
            break;
        default:
            maxFrequency = MAX_FREQUENCY_KHZ;
            break;
    }

    // 应用频率限制
    frequency = std::min(frequency, maxFrequency);

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
        const char* sonar_names[] = {"艏端", "舷侧", "粗拖", "细拖"};
        LOG_DEBUGF("声纳%d(%s)动态DI计算: 原始频率=%.3fkHz, 限制后频率=%.3fkHz, 最大频率=%.1fkHz, 倍数=%.0f, 偏移=%.2f, DI=%.2f",
                   sonarID, sonar_names[sonarID], dynamicFrequency, frequency, maxFrequency, multiplier, params.offset, di);
        return di;
    } else {
        return params.offset;  // 频率为0时只返回偏移量
    }
}








/**
 * @brief 组装并发送被动声呐探测结果
 * @param sonarID 声呐编号 (0:艏端, 1:舷侧, 2:粗拖, 3:细拖)
 * @param detectionThreshold 探测阈值 X
 * @param currentTime 当前时间戳
 */
void DeviceModel::assembleAndSendPassiveSonarResult(int sonarID, double detectionThreshold, int64 currentTime)
{
    if (!m_agent) {
        LOG_WARN("Agent is null, cannot send passive sonar result");
        return;
    }

    // 验证声呐ID有效性
    if (sonarID < 0 || sonarID >= 4) {
        LOG_WARNF("Invalid sonar ID: %d", sonarID);
        return;
    }

    // 检查声呐是否启用
    auto stateIt = m_sonarStates.find(sonarID);
    if (stateIt == m_sonarStates.end() ||
        !stateIt->second.arrayWorkingState ||
        !stateIt->second.passiveWorkingState) {
        LOG_INFOF("Sonar %d is disabled, not sending result", sonarID);
        return;
    }

    // 创建被动声呐结果结构体
    CMsg_PassiveSonarResultStruct passiveSonarResult;

    // 设置声呐ID (转换为1-7的编号，项目中使用1开始编号)
    passiveSonarResult.sonarID = sonarID + 1;  // 0->1, 1->2, 2->3, 3->4

    // 获取该声呐的计算结果
    auto resultIt = m_multiTargetCache.multiTargetEquationResults.find(sonarID);
    if (resultIt == m_multiTargetCache.multiTargetEquationResults.end()) {
        LOG_INFOF("No calculation results for sonar %d", sonarID);
        passiveSonarResult.detectionNumber = 0;
    } else {
        const auto& targetResults = resultIt->second;

        // 统计可探测的目标
        std::vector<TargetEquationResult> detectedTargets;
        std::vector<TargetEquationResult> validTargets;  // 用于跟踪的目标

        for (const auto& result : targetResults) {
            // 检查目标是否在声呐有效角度范围内
            if (!isTargetInSonarRange(sonarID, result.targetBearing, result.targetDistance)) {
                continue;
            }

            // 根据阈值判断是否可探测
            if (result.equationResult > detectionThreshold) {
                detectedTargets.push_back(result);

                // SNR较高的目标可以进行跟踪（这里用简单规则：X值高于阈值5dB以上）
                if (result.equationResult > (detectionThreshold + 5.0)) {
                    validTargets.push_back(result);
                }
            }
        }

        // 设置检测目标数量
        passiveSonarResult.detectionNumber = static_cast<int>(detectedTargets.size());

        // 组装检测结果
        for (size_t i = 0; i < detectedTargets.size() && i < 100; i++) {  // 最多100个目标
            C_PassiveSonarDetectionResult detection;

            // 方位角估计值（阵坐标系）
            detection.detectionDirArray = detectedTargets[i].targetBearing;

            // 方位角估计值（大地坐标系）
            // 转换为0-360度范围
            float groundBearing = detectedTargets[i].targetBearing + m_platformMotion.rotation;
            while (groundBearing < 0) groundBearing += 360.0f;
            while (groundBearing >= 360) groundBearing -= 360.0f;
            detection.detectionDirGroud = groundBearing;

            passiveSonarResult.PassiveSonarDetectionResult.push_back(detection);
        }

        // 组装跟踪结果
        for (size_t i = 0; i < validTargets.size() && i < 50; i++) {  // 最多50个跟踪目标
            C_PassiveSonarTrackingResult tracking;

            // 跟踪批次号
            tracking.trackingID = static_cast<int>(i + 1);

            // 信噪比（基于声呐方程结果估算）
            tracking.SNR = std::min(50.0f, std::max(-50.0f,
                static_cast<float>(validTargets[i].equationResult - detectionThreshold)));

            // 跟踪时长（模拟数据，实际应该是累积时间）
            tracking.trackingStep = std::min(1000, static_cast<int>(currentTime / 1000) % 1000);

            // 方位角估计值（阵坐标系）
            tracking.trackingDirArray = validTargets[i].targetBearing;

            // 方位角估计值（大地坐标系）
            float groundBearing = validTargets[i].targetBearing + m_platformMotion.rotation;
            while (groundBearing < 0) groundBearing += 360.0f;
            while (groundBearing >= 360) groundBearing -= 360.0f;
            tracking.trackingDirGroud = groundBearing;

            // 目标识别结果（基于SNR和距离的简单规则）
            if (tracking.SNR > 30.0f) {
                tracking.recognitionResult = 2;  // 潜艇
                tracking.recognitionPercent[1] = 0.8f;  // 潜艇置信度
            } else if (tracking.SNR > 20.0f) {
                tracking.recognitionResult = 1;  // 水面舰
                tracking.recognitionPercent[0] = 0.7f;  // 水面舰置信度
            } else {
                tracking.recognitionResult = 0;  // 未知
                tracking.recognitionPercent[4] = 0.5f;  // 未知置信度
            }

            // 频谱数据填充（这里使用模拟数据，实际应该从目标频谱获取）
            fillMockSpectrumData(tracking.spectumData, validTargets[i].targetId);

            passiveSonarResult.PassiveSonarTrackingResult.push_back(tracking);
        }
    }

    // 创建仿真消息
    CSimMessage simMessage;
    simMessage.dataFormat = STRUCT;
    simMessage.time = currentTime;
    simMessage.sender = m_agent->getPlatformEntity()->id;
    simMessage.senderComponentId = 1;
    simMessage.receiver = 0;  // 广播
    simMessage.data = &passiveSonarResult;
    simMessage.length = sizeof(passiveSonarResult);

    // 设置消息主题
    memset(simMessage.topic, 0, sizeof(simMessage.topic));
    strncpy(simMessage.topic, MSG_PassiveSonarResult_Topic, strlen(MSG_PassiveSonarResult_Topic));

    // 发送消息
    m_agent->sendMessage(&simMessage);

    LOG_INFOF("Sent passive sonar result for sonar %d: %d detections, %d trackings",
              sonarID, passiveSonarResult.detectionNumber,
              static_cast<int>(passiveSonarResult.PassiveSonarTrackingResult.size()));
}

/**
 * @brief 填充模拟频谱数据
 * @param spectrumData 频谱数据数组
 * @param targetId 目标ID
 */
void DeviceModel::fillMockSpectrumData(float spectrumData[5296], int targetId)
{
    // 清零频谱数据
    memset(spectrumData, 0, sizeof(float) * 5296);

    // 基于目标ID生成特征频谱
    int baseFreq = (targetId % 10) * 100 + 500;  // 500-1400Hz基频
    float amplitude = 100.0f + (targetId % 50);  // 基础幅度

    for (int i = 0; i < 5296; i++) {
        float freq = i * 10000.0f / 5296.0f;  // 0-10kHz频率范围

        // 在基频附近生成峰值
        if (freq >= baseFreq - 50 && freq <= baseFreq + 50) {
            float diff = std::abs(freq - baseFreq);
            spectrumData[i] = amplitude * exp(-diff * diff / (2 * 25 * 25));  // 高斯分布
        }

        // 添加一些谐波
        float harmonic2 = baseFreq * 2;
        if (freq >= harmonic2 - 25 && freq <= harmonic2 + 25) {
            float diff = std::abs(freq - harmonic2);
            spectrumData[i] += amplitude * 0.3f * exp(-diff * diff / (2 * 15 * 15));
        }

        // 添加背景噪声
        spectrumData[i] += (rand() % 20 - 10) * 0.1f;
    }
}

/**
 * @brief 批量发送所有声呐的被动探测结果
 * @param detectionThresholds 各声呐的探测阈值映射
 * @param currentTime 当前时间戳
 */
void DeviceModel::sendAllPassiveSonarResults(const std::map<int, double>& detectionThresholds, int64 currentTime)
{
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        double threshold = 33.0;  // 默认阈值

        auto thresholdIt = detectionThresholds.find(sonarID);
        if (thresholdIt != detectionThresholds.end()) {
            threshold = thresholdIt->second;
        } else {
            // 使用配置的阈值
            threshold = getEffectiveThreshold(sonarID);
        }

        assembleAndSendPassiveSonarResult(sonarID, threshold, currentTime);
    }
}

/**
 * @brief 在step函数中调用的简化发送方法
 */
void DeviceModel::sendPassiveSonarResultsInStep()
{
    // 使用当前配置的阈值发送所有声呐结果
    std::map<int, double> currentThresholds;
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        currentThresholds[sonarID] = getEffectiveThreshold(sonarID);
    }

    sendAllPassiveSonarResults(currentThresholds, curTime);
}





bool DeviceModel::safeAccessSpectrumData(const C_PropagatedContinuousSoundStruct& soundData,
                                         int index, float& value) const
{
    if (index < 0 || index >= 5296) {
        LOG_ERRORF("CRITICAL: Spectrum index out of bounds: %d (valid range: 0-5295)", index);
        return false;
    }

    try {
        value = soundData.spectrumData[index];
        return true;
    } catch (const std::exception& e) {
        LOG_ERRORF("CRASH: Exception accessing spectrum[%d]: %s", index, e.what());
        return false;
    } catch (...) {
        LOG_ERRORF("CRASH: Unknown exception accessing spectrum[%d]", index);
        return false;
    }
}













bool DeviceModel::safeAccessPointer(const void* ptr, size_t size, const char* ptrName,
                                   const char* fileName, const char* funcName, int line) const
{
    try {
        if (!ptr) {
            LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] NULL pointer access attempted for %s",
                      fileName, funcName, line, ptrName);
            return false;
        }

        // 基本的指针有效性检查 - 跨平台版本
        // 尝试读取第一个字节来验证内存可访问性
        try {
            volatile char testByte = *static_cast<const volatile char*>(ptr);
            (void)testByte; // 避免未使用变量警告
        } catch (...) {
            LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Memory access failed for %s (ptr=%p, size=%zu)",
                      fileName, funcName, line, ptrName, ptr, size);
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Exception accessing %s: %s",
                  fileName, funcName, line, ptrName, e.what());
        return false;
    } catch (...) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Unknown exception accessing %s",
                  fileName, funcName, line, ptrName);
        return false;
    }
}

bool DeviceModel::safeAccessSpectrumArray(const float* spectrumData, int index, float& outValue,
                                          const char* fileName, const char* funcName, int line) const
{
    try {
        // 边界检查
        if (index < 0 || index >= 5296) {
            LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Spectrum array index out of bounds: %d (valid: 0-5295)",
                      fileName, funcName, line, index);
            outValue = 0.0f;
            return false;
        }

        // 指针检查
        if (!spectrumData) {
            LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Spectrum array pointer is NULL",
                      fileName, funcName, line);
            outValue = 0.0f;
            return false;
        }

        // 基本内存访问检查
        if (!SAFE_ACCESS_PTR(spectrumData + index, sizeof(float), "spectrumData[index]")) {
            LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Cannot access spectrum[%d] memory",
                      fileName, funcName, line, index);
            outValue = 0.0f;
            return false;
        }

        // 安全读取值
        outValue = spectrumData[index];

        // 检查值的合理性
        if (std::isnan(outValue) || std::isinf(outValue)) {
            LOG_SAFE_WARN("Spectrum[%d] contains invalid float value: %f", index, outValue);
            outValue = 0.0f;
        }

        return true;

    } catch (const std::exception& e) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Exception reading spectrum[%d]: %s",
                  fileName, funcName, line, index, e.what());
        outValue = 0.0f;
        return false;
    } catch (...) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Unknown exception reading spectrum[%d]",
                  fileName, funcName, line, index);
        outValue = 0.0f;
        return false;
    }
}

bool DeviceModel::safeAccessSTLContainer(const std::list<C_PropagatedContinuousSoundStruct>& container,
                                         size_t& outSize, const char* fileName, const char* funcName, int line) const
{
    try {
        outSize = container.size();

        // 检查容器大小的合理性
        if (outSize > 10000) {  // 合理的上限
            LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] STL container size suspiciously large: %zu",
                      fileName, funcName, line, outSize);
            outSize = 0;
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Exception accessing STL container size: %s",
                  fileName, funcName, line, e.what());
        outSize = 0;
        return false;
    } catch (...) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Unknown exception accessing STL container size",
                  fileName, funcName, line);
        outSize = 0;
        return false;
    }
}

bool DeviceModel::safeIterateSTLContainer(const std::list<C_PropagatedContinuousSoundStruct>& container,
                                          std::function<bool(const C_PropagatedContinuousSoundStruct&, int)> processor,
                                          const char* fileName, const char* funcName, int line) const
{
    try {
        size_t containerSize;
        if (!SAFE_ACCESS_CONTAINER(container, containerSize)) {
            return false;
        }

        if (containerSize == 0) {
            LOG_SAFE_INFO("Container is empty, nothing to iterate");
            return true;
        }

        int index = 0;
        for (const auto& item : container) {
            try {
                if (!processor(item, index)) {
                    LOG_SAFE_WARN("Processor returned false for item %d, stopping iteration", index);
                    break;
                }
                index++;

                // 防止无限循环
                if (index > static_cast<int>(containerSize) + 10) {
                    LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Iterator index exceeded container size, possible corruption",
                              fileName, funcName, line);
                    break;
                }

            } catch (const std::exception& e) {
                LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Exception processing container item %d: %s",
                          fileName, funcName, line, index, e.what());
                continue; // 继续处理下一个项目
            } catch (...) {
                LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Unknown exception processing container item %d",
                          fileName, funcName, line, index);
                continue;
            }
        }

        return true;

    } catch (const std::exception& e) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Exception during container iteration: %s",
                  fileName, funcName, line, e.what());
        return false;
    } catch (...) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Unknown exception during container iteration",
                  fileName, funcName, line);
        return false;
    }
}

void DeviceModel::emergencyCleanup(const char* reason, const char* fileName, const char* funcName, int line)
{
    try {
        LOG_ERRORF("[EMERGENCY_CLEANUP][%s::%s:%d] Performing emergency cleanup due to: %s",
                  fileName, funcName, line, reason);

        // 清空所有缓存数据
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            safeResetTargetCache(sonarID, fileName, funcName, line);
        }

        // 重置统计信息
        m_debugStats.failedProcessings++;
        m_debugStats.lastErrorMsg = std::string(reason) + " (Emergency cleanup performed)";

        LOG_SAFE_INFO("Emergency cleanup completed successfully");

    } catch (...) {
        // 即使紧急清理也失败了，至少记录日志
        LOG_ERRORF("[EMERGENCY_CLEANUP][%s::%s:%d] Emergency cleanup itself failed!",
                  fileName, funcName, line);
    }
}

void DeviceModel::safeResetTargetCache(int sonarID, const char* fileName, const char* funcName, int line)
{
    try {
        if (sonarID < 0 || sonarID >= 4) {
            LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Invalid sonarID for reset: %d",
                      fileName, funcName, line, sonarID);
            return;
        }

        m_multiTargetCache.sonarTargetsData[sonarID].clear();
        m_multiTargetCache.multiTargetEquationResults[sonarID].clear();

        LOG_SAFE_INFO("Successfully reset cache for sonar %d", sonarID);

    } catch (const std::exception& e) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Exception resetting sonar %d cache: %s",
                  fileName, funcName, line, sonarID, e.what());
    } catch (...) {
        LOG_ERRORF("[CRASH_PROTECTED][%s::%s:%d] Unknown exception resetting sonar %d cache",
                  fileName, funcName, line, sonarID);
    }
}

// 安全执行包装器实现
template<typename Func>
bool DeviceModel::safeExecute(Func&& func, const char* funcName, const char* fileName, int line)
{
    if (!m_globalProtection.isProtectionEnabled) {
        LOG_SAFE_WARN("Global protection disabled, executing function %s without protection", funcName);
        try {
            func();
            return true;
        } catch (...) {
            return false;
        }
    }

    try {
        LOG_SAFE_INFO("Executing protected function: %s", funcName);

        auto startTime = std::chrono::steady_clock::now();

        // 执行函数
        bool result = func();

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (duration.count() > 1000) { // 超过1秒记录警告
            LOG_SAFE_WARN("Function %s took %lld ms to execute", funcName, duration.count());
        }

        LOG_SAFE_INFO("Function %s completed successfully in %lld ms", funcName, duration.count());
        return result;

    } catch (const std::bad_alloc& e) {
        LOG_ERRORF("[MEMORY_ERROR][%s::%s:%d] Memory allocation failed in %s: %s",
                  fileName, funcName, line, funcName, e.what());
        recordException("Memory allocation failed: " + std::string(e.what()), fileName, funcName, line);
        emergencyCleanup("Memory allocation failed", fileName, funcName, line);
        return false;

    } catch (const std::runtime_error& e) {
        LOG_ERRORF("[RUNTIME_ERROR][%s::%s:%d] Runtime error in %s: %s",
                  fileName, funcName, line, funcName, e.what());
        recordException("Runtime error: " + std::string(e.what()), fileName, funcName, line);
        return false;

    } catch (const std::logic_error& e) {
        LOG_ERRORF("[LOGIC_ERROR][%s::%s:%d] Logic error in %s: %s",
                  fileName, funcName, line, funcName, e.what());
        recordException("Logic error: " + std::string(e.what()), fileName, funcName, line);
        return false;

    } catch (const std::exception& e) {
        LOG_ERRORF("[EXCEPTION][%s::%s:%d] Exception in %s: %s",
                  fileName, funcName, line, funcName, e.what());
        recordException("Standard exception: " + std::string(e.what()), fileName, funcName, line);
        return false;

    } catch (...) {
        LOG_ERRORF("[UNKNOWN_EXCEPTION][%s::%s:%d] Unknown exception in %s",
                  fileName, funcName, line, funcName);
        recordException("Unknown exception", fileName, funcName, line);
        return false;
    }
}

// 保护状态检查函数实现
bool DeviceModel::checkExecutionTimeout(const char* funcName, const char* fileName, int line)
{
    try {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - m_globalProtection.functionStartTime);

        if (elapsed.count() > m_globalProtection.maxExecutionTimeMs) {
            LOG_ERRORF("[TIMEOUT][%s::%s:%d] Function %s exceeded maximum execution time: %lld ms",
                      fileName, funcName, line, funcName, elapsed.count());
            emergencyCleanup("Execution timeout", fileName, funcName, line);
            return false;
        }

        return true;

    } catch (...) {
        LOG_ERRORF("[TIMEOUT_CHECK_FAILED][%s::%s:%d] Failed to check timeout for %s",
                  fileName, funcName, line, funcName);
        return true; // 如果检查失败，允许继续执行
    }
}

void DeviceModel::startExecutionTimer()
{
    try {
        m_globalProtection.functionStartTime = std::chrono::steady_clock::now();
    } catch (...) {
        // 即使计时器启动失败也不影响主流程
        LOG_SAFE_WARN("Failed to start execution timer");
    }
}

bool DeviceModel::checkMemoryUsage(const char* funcName, const char* fileName, int line)
{
    try {
        // 简单的内存使用估算
        size_t estimatedUsage = 0;

        // 估算目标缓存使用的内存
        for (const auto& sonarPair : m_multiTargetCache.sonarTargetsData) {
            for (const auto& target : sonarPair.second) {
                estimatedUsage += sizeof(TargetData) + target.propagatedSpectrum.size() * sizeof(float);
            }
        }

        // 估算其他缓存
        for (const auto& spectrumPair : m_multiTargetCache.platformSelfSoundSpectrumMap) {
            estimatedUsage += spectrumPair.second.size() * sizeof(float);
        }

        for (const auto& spectrumPair : m_multiTargetCache.environmentNoiseSpectrumMap) {
            estimatedUsage += spectrumPair.second.size() * sizeof(float);
        }

        m_globalProtection.currentMemoryEstimate = estimatedUsage;

        if (estimatedUsage > m_globalProtection.maxMemoryUsage) {
            m_globalProtection.maxMemoryUsage = estimatedUsage;
        }

        // 检查是否超过合理限制（比如500MB）
        const size_t MAX_MEMORY_LIMIT = 500 * 1024 * 1024; // 500MB
        if (estimatedUsage > MAX_MEMORY_LIMIT) {
            LOG_ERRORF("[MEMORY_LIMIT][%s::%s:%d] Memory usage too high in %s: %zu bytes",
                      fileName, funcName, line, funcName, estimatedUsage);

            // 尝试清理一些数据
            emergencyCleanup("Memory limit exceeded", fileName, funcName, line);
            return false;
        }

        return true;

    } catch (...) {
        LOG_SAFE_WARN("Failed to check memory usage, allowing continuation");
        return true; // 如果检查失败，允许继续执行
    }
}

void DeviceModel::updateMemoryEstimate()
{
    try {
        checkMemoryUsage("updateMemoryEstimate", __FILENAME__, __LINE__);
    } catch (...) {
        // 静默处理，不影响主流程
    }
}

bool DeviceModel::checkExceptionLimit(const char* funcName, const char* fileName, int line)
{
    try {
        if (m_globalProtection.totalExceptions >= m_globalProtection.maxExceptionsAllowed) {
            LOG_ERRORF("[EXCEPTION_LIMIT][%s::%s:%d] Maximum exceptions reached (%d) in %s, disabling protection",
                      fileName, funcName, line, m_globalProtection.maxExceptionsAllowed, funcName);
            m_globalProtection.isProtectionEnabled = false;
            return false;
        }

        return true;

    } catch (...) {
        return true; // 如果检查失败，允许继续执行
    }
}

void DeviceModel::recordException(const std::string& exceptionInfo, const char* fileName, const char* funcName, int line)
{
    try {
        m_globalProtection.totalExceptions++;
        m_globalProtection.lastExceptionTime = std::chrono::steady_clock::now();
        m_globalProtection.lastExceptionLocation = std::string(fileName) + "::" + funcName + ":" + std::to_string(line);

        LOG_ERRORF("[EXCEPTION_RECORDED][%s::%s:%d] Exception #%d: %s",
                  fileName, funcName, line, m_globalProtection.totalExceptions, exceptionInfo.c_str());

        // 如果异常过于频繁，考虑临时禁用某些功能
        auto now = std::chrono::steady_clock::now();
        static auto lastExceptionTime = now;
        auto timeSinceLastException = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastExceptionTime);

        if (timeSinceLastException.count() < 100) { // 100毫秒内连续异常
            LOG_SAFE_WARN("Frequent exceptions detected, consider function throttling");
        }

        lastExceptionTime = now;

    } catch (...) {
        // 即使记录异常失败也不要影响主流程
    }
}

#include "DMLogger.h"
#include <chrono>
#include <iomanip>
#include <ctime>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::initialize(const std::string& logFilePath, bool enableConsole) {
    // 保存当前用户设置的文件输出状态
    bool userFileOutputSetting = m_fileOutputEnabled;
    bool wasInitialized = m_initialized;

    m_enableConsole = enableConsole;
    m_logFilePath = logFilePath;

    if (!logFilePath.empty()) {
        // 如果文件已经打开且是同一个文件，不重新打开
        if (m_logFile.is_open() && m_logFilePath == logFilePath) {
            std::cout << "Logger: Same log file already open, keeping current state" << std::endl;
        } else {
            // 关闭旧文件，打开新文件
            if (m_logFile.is_open()) {
                m_logFile.close();
            }

            m_logFile.open(logFilePath, std::ios::out | std::ios::app);
            if (m_logFile.is_open()) {
                m_enableFile = true;
                m_logFile << "\n========== Logger " << (wasInitialized ? "Re-" : "") << "Initialized at " << getTimestamp() << " ==========\n";
                m_logFile.flush();
            }
        }
    }

    m_initialized = true;

    // 如果之前已经初始化过，保持用户的文件输出设置
    if (wasInitialized) {
        m_fileOutputEnabled = userFileOutputSetting;
        std::cout << "Logger re-initialized, preserved user file output setting: "
                  << (m_fileOutputEnabled ? "ENABLED" : "DISABLED") << std::endl;
    } else {
        // 首次初始化，默认启用文件输出
        m_fileOutputEnabled = true;
        std::cout << "Logger first-time initialized, file output: ENABLED" << std::endl;
    }

    if (m_enableConsole) {
        std::cout << "Logger state: Console=" << (enableConsole ? "ON" : "OFF")
                  << ", File=" << (m_enableFile ? "ON" : "OFF")
                  << ", FileOutput=" << (m_fileOutputEnabled ? "ENABLED" : "DISABLED") << std::endl;
    }
}
void Logger::shutdown() {
    if (m_enableFile && m_logFile.is_open()) {
        m_logFile << "========== Logger Shutdown at " << getTimestamp() << " ==========\n";
        m_logFile.close();
        m_enableFile = false;
    }
    m_initialized = false;
}

Logger::~Logger() {
    shutdown();
}

void Logger::setLogLevel(LogLevel level) {
    m_logLevel = level;
}

void Logger::log(LogLevel level, const char* function, int line, const std::string& message) {
    if (!isEnabled(level)) {
        return;
    }

    writeLog(level, function, line, message);
}

void Logger::debug(const char* function, int line, const std::string& message) {
    if (isEnabled(LogLevel::DEBUG)) {
        writeLog(LogLevel::DEBUG, function, line, message);
    }
}

void Logger::info(const char* function, int line, const std::string& message) {
    if (isEnabled(LogLevel::INFO)) {
        writeLog(LogLevel::INFO, function, line, message);
    }
}

void Logger::warn(const char* function, int line, const std::string& message) {
    if (isEnabled(LogLevel::WARN)) {
        writeLog(LogLevel::WARN, function, line, message);
    }
}

void Logger::error(const char* function, int line, const std::string& message) {
    if (isEnabled(LogLevel::ERROR)) {
        writeLog(LogLevel::ERROR, function, line, message);
    }
}

void Logger::empty(const char* function, int line, const std::string& message) {
    if (isEnabled(LogLevel::INFO)) {
        writeLogEmpty(LogLevel::INFO, function, line, message);
    }
}

void Logger::flush() {
    if (m_enableConsole) {
        std::cout.flush();
    }
    if (m_enableFile && m_logFile.is_open()) {
        m_logFile.flush();
    }
}

bool Logger::isEnabled(LogLevel level) const {
    return level >= m_logLevel;
}

std::string Logger::getLevelString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKN ";
    }
}

std::string Logger::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Logger::writeLog(LogLevel level, const char* function, int line, const std::string& message) {



    std::stringstream logStream;
    logStream << "[" << getTimestamp() << "] "
              << "[" << getLevelString(level) << "] "
              << "[" << function << ":" << line << "] "
              << message;

    std::string logLine = logStream.str();

    // 输出到控制台
    if (m_enableConsole) {
        std::cout << logLine << std::endl;
    }

    // 输出到文件 - 明显的状态检查
    std::cout << "DEBUG writeLog: this=" << this
                  << ", m_fileOutputEnabled=" << this->m_fileOutputEnabled
                  << ", m_enableFile=" << this->m_enableFile
                  << ", file_open=" << (this->m_logFile.is_open() ? 1 : 0) << std::endl;

    if (m_fileOutputEnabled && m_enableFile && m_logFile.is_open()) {
        m_logFile << logLine << std::endl;
        m_logFile.flush();
//        std::cout << "DEBUG: Log written to file" << std::endl;
    } else {
//        std::cout << "DEBUG: Log NOT written to file" << std::endl;
    }
}

void Logger::writeLogEmpty(LogLevel level, const char* function, int line, const std::string& message) {



    std::stringstream logStream;
    logStream << message;

    std::string logLine = logStream.str();

    // 输出到控制台
    if (m_enableConsole) {
        std::cout << logLine << std::endl;
    }

    // 输出到文件 - 明显的状态检查
    std::cout << "DEBUG writeLog: this=" << this
                  << ", m_fileOutputEnabled=" << this->m_fileOutputEnabled
                  << ", m_enableFile=" << this->m_enableFile
                  << ", file_open=" << (this->m_logFile.is_open() ? 1 : 0) << std::endl;

    if (m_fileOutputEnabled && m_enableFile && m_logFile.is_open()) {
        m_logFile << logLine << std::endl;
        m_logFile.flush();
        std::cout << "DEBUG: Log written to file" << std::endl;
    } else {
        std::cout << "DEBUG: Log NOT written to file" << std::endl;
    }
}

void Logger::enableFileOutput(bool enable) {


    std::cout << "=== enableFileOutput DEBUG ===" << std::endl;
    std::cout << "Before: m_fileOutputEnabled = " << m_fileOutputEnabled << std::endl;
    std::cout << "Setting to: " << enable << std::endl;

    m_fileOutputEnabled = enable;

    std::cout << "After: m_fileOutputEnabled = " << m_fileOutputEnabled << std::endl;
    std::cout << "=== END DEBUG ===" << std::endl;



    if (!enable) {
        // 暂停文件输出
        if (m_enableFile && m_logFile.is_open()) {
            m_logFile << "========== File Output Paused at " << getTimestamp() << " ==========\n";
            m_logFile.flush();
        }
        std::cout << "File output disabled" << std::endl; // 调试输出
    } else {
        // 恢复文件输出
        if (m_enableFile && m_logFile.is_open()) {
            m_logFile << "========== File Output Resumed at " << getTimestamp() << " ==========\n";
            m_logFile.flush();
        }
        std::cout << "File output enabled" << std::endl; // 调试输出
    }
}

bool Logger::isFileOutputEnabled() const {
    bool result = m_fileOutputEnabled;
    std::cout << "isFileOutputEnabled() called, returning: " << result << std::endl;
    return result;
}

std::string Logger::getCurrentLogFilePath() const {
    return m_logFilePath;
}

bool Logger::isInitialized() const {
    return m_initialized;
}

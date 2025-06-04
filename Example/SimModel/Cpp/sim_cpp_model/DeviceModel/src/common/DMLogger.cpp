#include "DMLogger.h"
#include <chrono>
#include <iomanip>
#include <ctime>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::initialize(const std::string& logFilePath, bool enableConsole) {
    m_enableConsole = enableConsole;
    m_logFilePath = logFilePath;

    if (!logFilePath.empty()) {
        m_logFile.open(logFilePath, std::ios::out | std::ios::app);
        if (m_logFile.is_open()) {
            m_enableFile = true;
            m_logFile << "\n========== Logger Initialized at " << getTimestamp() << " ==========\n";
            m_logFile.flush();
        }
    }

    m_initialized = true;

    if (m_enableConsole) {
        std::cout << "Logger initialized. Console: " << (enableConsole ? "ON" : "OFF")
                  << ", File: " << (m_enableFile ? "ON" : "OFF") << std::endl;
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

    // 输出到文件
    if (m_enableFile && m_logFile.is_open()) {
        m_logFile << logLine << std::endl;
    }
}

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>

// 日志级别枚举
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    // 获取单例实例
    static Logger& getInstance();

    // 初始化日志系统
    void initialize(const std::string& logFilePath = "", bool enableConsole = true);

    // 销毁日志系统
    void shutdown();

    // 设置日志级别
    void setLogLevel(LogLevel level);

    // 核心日志方法
    void log(LogLevel level, const char* function, int line, const std::string& message);

    // 便捷方法
    void debug(const char* function, int line, const std::string& message);
    void info(const char* function, int line, const std::string& message);
    void warn(const char* function, int line, const std::string& message);
    void error(const char* function, int line, const std::string& message);

    // 格式化日志方法
    template<typename... Args>
    void debugf(const char* function, int line, const char* format, Args... args);

    template<typename... Args>
    void infof(const char* function, int line, const char* format, Args... args);

    template<typename... Args>
    void warnf(const char* function, int line, const char* format, Args... args);

    template<typename... Args>
    void errorf(const char* function, int line, const char* format, Args... args);

    // 强制刷新日志
    void flush();

    // 检查是否启用了某个级别的日志
    bool isEnabled(LogLevel level) const;



    // 启用/禁用文件输出
    void enableFileOutput(bool enable);

    // 检查文件输出是否启用
    bool isFileOutputEnabled() const;

    // 获取当前日志文件路径
    std::string getCurrentLogFilePath() const;

    // 检查是否已初始化
    bool isInitialized() const;

private:
    Logger() = default;
    ~Logger();

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 获取日志级别字符串
    std::string getLevelString(LogLevel level) const;

    // 格式化时间戳
    std::string getTimestamp() const;

    // 内部日志写入方法
    void writeLog(LogLevel level, const char* function, int line, const std::string& message);

    // 格式化字符串
    template<typename... Args>
    std::string formatString(const char* format, Args... args);


    bool m_fileOutputEnabled = true;  // 文件输出开关

private:
    LogLevel m_logLevel = LogLevel::DEBUG;
    bool m_enableConsole = true;
    bool m_enableFile = false;
    std::ofstream m_logFile;
    std::string m_logFilePath;
    bool m_initialized = false;
};

// 模板方法实现
template<typename... Args>
void Logger::debugf(const char* function, int line, const char* format, Args... args) {
    if (isEnabled(LogLevel::DEBUG)) {
        log(LogLevel::DEBUG, function, line, formatString(format, args...));
    }
}

template<typename... Args>
void Logger::infof(const char* function, int line, const char* format, Args... args) {
    if (isEnabled(LogLevel::INFO)) {
        log(LogLevel::INFO, function, line, formatString(format, args...));
    }
}

template<typename... Args>
void Logger::warnf(const char* function, int line, const char* format, Args... args) {
    if (isEnabled(LogLevel::WARN)) {
        log(LogLevel::WARN, function, line, formatString(format, args...));
    }
}

template<typename... Args>
void Logger::errorf(const char* function, int line, const char* format, Args... args) {
    if (isEnabled(LogLevel::ERROR)) {
        log(LogLevel::ERROR, function, line, formatString(format, args...));
    }
}

template<typename... Args>
std::string Logger::formatString(const char* format, Args... args) {
    int size = std::snprintf(nullptr, 0, format, args...) + 1;
    if (size <= 0) {
        return std::string("");
    }
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format, args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

// 便捷宏定义
#define LOG_DEBUG(msg) Logger::getInstance().debug(__FUNCTION__, __LINE__, msg)
#define LOG_INFO(msg) Logger::getInstance().info(__FUNCTION__, __LINE__, msg)
#define LOG_WARN(msg) Logger::getInstance().warn(__FUNCTION__, __LINE__, msg)
#define LOG_ERROR(msg) Logger::getInstance().error(__FUNCTION__, __LINE__, msg)

#define LOG_DEBUGF(format, ...) Logger::getInstance().debugf(__FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define LOG_INFOF(format, ...) Logger::getInstance().infof(__FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define LOG_WARNF(format, ...) Logger::getInstance().warnf(__FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERRORF(format, ...) Logger::getInstance().errorf(__FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#endif // LOGGER_H

#ifndef CSIMINTEGRATIONLOGGER_H
#define CSIMINTEGRATIONLOGGER_H
#include "SimBasicTypes.h"

/**
 * @brief The CSimIntegrationLogger class
 * 日志功能类
 */
class SIM_SDK_API CSimIntegrationLogger
{
public:
    CSimIntegrationLogger();

    virtual ~CSimIntegrationLogger();

    /**
     * @brief info   输出信息
     * @param value
     */
    virtual void info(const char* value);

    /**
     * @brief debug  输出调试信息
     * @param value
     */
    virtual void debug(const char* value);

    /**
     * @brief warn      输出警告信息
     * @param value
     */
    virtual void warn(const char* value);

    /**
     * @brief error     输出错误信息
     * @param value
     */
    virtual void error(const char* value);

};

#endif // CSIMINTEGRATIONLOGGER_H

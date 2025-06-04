#ifndef CREATEDEVICEMODEL_H
#define CREATEDEVICEMODEL_H

// 导入导出配置
#ifdef __linux__
#	ifdef DEVICEMODEL_LIBRARY
#		define DEVICEMODEL_EXPORT __attribute__((visibility("default")))
#	else
#		define DEVICEMODEL_EXPORT
#	endif
#else
#   ifdef DEVICEMODEL_LIBRARY
#       define DEVICEMODEL_EXPORT __declspec(dllexport)
#   else
#       define DEVICEMODEL_EXPORT __declspec(dllimport)
#   endif
#endif

#include <string>

class CSimComponentBase;

extern "C" DEVICEMODEL_EXPORT CSimComponentBase *createComponent(std::string componentName);

#endif // CREATEDEVICEMODEL_H

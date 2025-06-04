#pragma once

typedef signed char        int8;
typedef short              int16;
typedef int                int32;
typedef long long          int64;
typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;


#ifdef __linux__
#	ifdef SIM_SDK_EXPORTS
#		define SIM_SDK_API __attribute__((visibility("default")))
#	else
#		define SIM_SDK_API
#	endif
#else
#   ifdef SIM_SDK_EXPORTS
#       define SIM_SDK_API __declspec(dllexport)
#   else
#       define SIM_SDK_API __declspec(dllimport)
#   endif

#endif


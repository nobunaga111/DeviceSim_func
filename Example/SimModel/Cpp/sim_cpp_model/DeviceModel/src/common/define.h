#ifndef DEFINE_H
#define DEFINE_H

// 数学常量定义
#define M_PI 3.14159265358979323846

// 声纳系统常量定义
#define MAX_DETECTION_RANGE 30000.0  // 最大探测距离(米)
#define MAX_PASSIVE_TRACKS 10        // 最大被动跟踪目标数
#define MAX_ACTIVE_TRACKS 10         // 最大主动跟踪目标数
#define MAX_SCOUTING_TRACKS 10       // 最大侦察跟踪目标数

// 声纳数量和ID定义
#define SONAR_COUNT 4               // 声纳总数量
#define SONAR_ID_MIN 0              // 最小声纳ID
#define SONAR_ID_MAX 3              // 最大声纳ID

// 声纳类型定义
#define SONAR_TYPE_BOW 0            // 艏端声纳
#define SONAR_TYPE_SIDE 1           // 舷侧声纳
#define SONAR_TYPE_COARSE_TOW 2     // 粗拖声纳
#define SONAR_TYPE_FINE_TOW 3       // 细拖声纳

#endif // DEFINE_H

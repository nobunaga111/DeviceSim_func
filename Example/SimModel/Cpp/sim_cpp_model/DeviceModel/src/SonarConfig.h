#ifndef SONARCONFIG_H
#define SONARCONFIG_H

#include <QString>

/**
 * @brief 声纳范围配置结构体
 */
struct SonarRangeConfig {
    int sonarId;
    QString name;
    float maxRange;           // 最大探测距离（米）
    float startAngle1;        // 第一段起始角度
    float endAngle1;          // 第一段结束角度
    float startAngle2;        // 第二段起始角度（仅舷侧声纳使用）
    float endAngle2;          // 第二段结束角度（仅舷侧声纳使用）
    bool hasTwoSegments;      // 是否有两个角度段

    // 默认构造函数
    SonarRangeConfig() : sonarId(-1), maxRange(30000.0f),
                       startAngle1(0.0f), endAngle1(0.0f),
                       startAngle2(0.0f), endAngle2(0.0f),
                       hasTwoSegments(false) {}

    // 带参数的构造函数
    SonarRangeConfig(int id, const QString& n, float maxR,
                    float start1, float end1, float start2,
                    float end2, bool twoSeg)
        : sonarId(id), name(n), maxRange(maxR),
          startAngle1(start1), endAngle1(end1),
          startAngle2(start2), endAngle2(end2),
          hasTwoSegments(twoSeg) {}
};

#endif // SONARCONFIG_H

#ifndef __CSIMTYPES_H__
#define __CSIMTYPES_H__

#include "SimBasicTypes.h"


/**
 * 数据的格式枚举
 */
enum CDataFormat
{
    STRUCT = 0, // 结构体
    JSON,       // json字符串
};

/**
 * 组件信息
 */
typedef struct CSimPlatformComponent
{
    int64 id;				// id
    int64 platformEnityId;	// 平台实体id
	char name[128];			// 名称
} CSimPlatformComponent;

/**
 * 武器挂架
 */
typedef struct CMount_t
{
    int64 id;               // id
    int64 templateID;       // 武器模板id
    char weaponKind[64];    // 武器类型
    char weaponName[64];    // 武器名称
    int32 weaponNum;        // 武器数量
} CMount;

/**
 * 实体信息
 */
typedef struct CSimPlatformEntity
{
    int64 id;                // id
    char name[64];          // 名称
    int64 templateID;        // 实体模板ID
    char entityType[256];   // 类型
    int64 campId;            // 阵营Id
    long carriersId;    // 装载父ID
	CSimPlatformComponent *componentList[64]; // 组件列表
	int16 componentCount;					 // 组件个数
    CMount mountList[64];                   // 挂架列表
    int16 mountCount;                       // 挂架数量
    long command;                           // 指挥父ID
    long supTeamId;                         // 编队ID

} CSimPlatformEntity;

/**
 * 关键事件
 */
typedef struct CCriticalEvent
{
    int64 time;		   // 仿真时间（ms）
    char type[64];     // 消息类型
    int64 sender;	   // 发送者（实体id）
    int64 receiver;	   // 接收者（实体id）
    char content[256]; // 消息内容
} CCriticalEvent;

struct CSimString
{
    char* data;
};

/**
 * 坐标结构体
 */
typedef struct CVec4
{
	long id;
	double x;
	double y;
	double z;
	double rotation;

}CVec4;//初始化结构体

#endif

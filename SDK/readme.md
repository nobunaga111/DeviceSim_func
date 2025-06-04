# 目录结构说明

整体结构包括使用说明和接口说明两部分：

1.使用说明：主要说明使用sdk和基于sdk开发模型的过程说明，使用说明包括环境配置，开发步骤，输出内容和测试工具。环境配置为模型开发工具引入sdk需要配置的项；开发步骤说明基于sdk的开发过程；输出内容为模型开发完之后需要提供给引擎使用的内容；测试工具为模型开发过程中可使用测试工具进行模型测试。

2.接口说明：主要提供sdk提供的接口和参数说明。

# 1.使用说明

## 1.1 环境配置

使用Qt Creator创建生成库的工程，并配置如下：

1）工程配置设置导出宏定义

```
DEFINES += MOVETESTMODEL_EXPORTS
```

2）附加头文件目录配置，主要引用装备模型SDK的头文件和提供给外部模型调用的头文件

```
INCLUDEPATH += \
    $$PWD/../../../../SDK/SimModel/Cpp/include/ \
    include/
```

4）输入和输出配置，用于生成debug和release库的路径，并引入装备模型的库路径和库名

```
CONFIG(debug,debug|release){
    DESTDIR = $$PWD/bin/Debug
    LIBS += -L$$PWD/../../../../SDK/SimModel/Cpp/lib/debug -lSimSdk
}
else {
    DESTDIR = $$PWD/bin/Release
    LIBS += -L$$PWD/../../../../SDK/SimModel/Cpp/lib/Release -lSimSdk
}
```

5）头文件和源文件配置，内部头文件和源文件放到src目录下，提供给其他模型的数据定义放到include目录下

```
SOURCES += \
    src/CreateTest.cpp \
    src/MoveTestModel.cpp

HEADERS += \
    include/MoveTestModel/MoveTestData.h \
    src/CreateTest.h \
    src/MoveTestModel.h
```

## 1.2 开发步骤

1）实现组件创建接口，参考2.1.1 组件创建接口；

2）创建实体组件类，继承实体组件基类，实现基类的接口；

3）开发实体组件类接口时，使用代理接口调用引擎功能。

4）实现发送数据给引擎或者模型需要将对外输出的数据定义好，并生成想要的数据包，使用数据转换工具使用文档

## 1.3 输出内容

1）提供对外输出的头文件；

2）提供模型lib库，包括debug和release版本；

3）提供模型dll库，包括debug和release版本。

# 2.接口说明

## 2.1 引擎调用组件接口

### 2.1.1 组件创建接口

模型库须实现创建组件函数接口，用于动态创建组件

extern "C" {
	__declspec(dllexport) CSimComponentBase* createComponent(string componentName);
}

实现类如下：

CSimComponentBase* createComponent(string componentName) {
	return new AircraftComponent();
}

### 2.1.2 实体组件类接口

CSimComponentBase为实体组件基类，模型须继承实现该类

| 序号 |  接口名称  |    接口说明    |                             参数                             | 返回值                      |
| :--: | :--------: | :------------: | :----------------------------------------------------------: | --------------------------- |
|  1   |    init    |     初始化     | CSimModelAgentBase* simModelAgent:代理接口<br />CSimComponentAttribute* attr:组件属性 | 空                          |
|  2   |  destroy   |    销毁组件    |                              空                              |                             |
|  3   | onMessage  |  处理仿真消息  |               CSimMessage* simMessage:仿真消息               | 空                          |
|  4   |    step    |    仿真步进    |     int64 curTime:当前仿真时间<br />int32 step:推进步长      | 空                          |
|  5   | getVersion | 返回该模型版本 |                              空                              | char*:格式是VX.X.X.20240326 |

### 2.1.2 实体组件属性类参数

​													CSimComponentAttributeBase实体组件属性类

| 序号 | 字段名称 |  类型  |   字段说明   |
| :--: | :-------------: | :----: | :----------: |
|  1   |    id           | int64  |  获取属性id  |
|  2   |   name          | string | 获取属性名称 |
|  3   |   data          | char*  | 组件属性数据 （javacpp转换） |
|  3   |   dynamicData   | char*  | 组件属性数据 （json数据格式） |

### 2.1.3 仿真消息类参数

​																			CSimMessage仿真消息类

| 序号 | 字段名称 |  类型  |       说明       |
| :--: | :-----------------: | :----: | :--------------: |
|  1   |         time        | int64  |  仿真时间（ms）  |
|  2   |         topic       | char[] |     消息主题     |
|  3   |         sender      | int64  | 发送者（实体id） |
|  4   |         receiver    | int64  | 接收者（实体id） |
|  5   |         data        | char*  |   消息数据载荷   |
|  6   |         length      | uint32 |     数据长度     |
|  7   |  senderComponentId  | uint32 |     发送者组件id     |

### 2.1.4 仿真数据参数

​																				CSimData仿真数据类

| 序号 | 字段名称 |  类型  |               说明               |
| :--: | :-----------: | :----: | :------------------------------: |
|  1   |      time     | int64  |          仿真时间（ms）          |
|  2   |     topic     | char[] |           仿真数据主题           |
|  3   |     sender    | int64  |         发送者（实体id）         |
|  4   |     receiver  | int64  | 接收者（实体id）(公布数据可不填) |
|  5   |      data     | char*  |           仿真数据载荷           |
|  6   |     length    | uint32 |             数据长度             |
|  7   |  componentId  | int64  |             组件id             |

## 2.2 组件调用引擎接口

## 2.2.1 模型代理接口

| 序号 |         接口名称          |           接口说明           |                     参数                      | 返回值                                   |
| :--: | :-----------------------: | :--------------------------: | :-------------------------------------------: | ---------------------------------------- |
|  1   |      publishSimData       |      更新/发布仿真数据       |          CSimData* simData:仿真数据           | 空                                       |
|  2   |    getSubscribeSimData    |     获取已订阅的仿真数据     | char* topic:主题<br />int64 platformId:平台id | CSimData*:仿真数据                       |
|  3   |     subscribeSimData      |         订阅仿真数据         | CSubscribeSimData* subscribeSimData:订阅数据  | 空                                       |
|  4   |   subscribeCampSimData    |       订阅阵营仿真数据       | CSubscribeSimData* subscribeSimData:订阅数据  | 空                                       |
|  5   |     unsubscribSimData     |       取消订阅仿真数据       | CSubscribeSimData* subscribeSimData:订阅数据  | 空                                       |
|  6   |   unsubscribCampSimData   |       取消订阅阵营数据       | CSubscribeSimData* subscribeSimData:订阅数据  | 空                                       |
|  7   |        sendMessage        |         发送交互信息         |       CSimMessage* simMessage:仿真消息        | 空                                       |
|  8   |     subscribeMessage      |           订阅消息           |          const char *topic:消息主题           | 空                                       |
|  9   |   unsubscribeMessagege    |         取消订阅消息         |          const char *topic:消息主题           | 空                                       |
|  10  |     getPlatformEntity     | 获取实体信息（本组件的实体） |                      空                       | CSimPlatformEntity*:实体                 |
|  11  |   getComponentAttribute   |       获取当前组件属性       |                      空                       | CSimComponentAttribute*：组件属性        |
|  12  |          getCamp          |        获取阵营id集合        |                      空                       | std::vector<int32>:阵营id集合            |
|  13  | getPlatformEntiysByCampId |     获取阵营所有实体集合     |              int32 campId:阵营id              | vector<CSimPlatformEntity*>:阵营实体集合 |
|  14  |  getComponentElapseTime   |   获取当前组件推进累积时长   |                      空                       | int64:组件推进累积时长                   |
|  15  |    getEngineElapseTime    | 获取当前引擎仿真推进累积时长 |                      空                       | int64:引擎推进累积时长                   |
|  16  |          getStep          |        获取步长/间隔         |                      空                       | int32:步长时间                           |
|  17  |          setStep          |           调整步长           |              int32 step:步长时间              | 空                                       |
|  18  |      getStartTime()       |      获取想定的开始时间      |                      空                       | int64:想定开始时间                       |
|  19  |       getEndTime()        |      获取想定的结束时间      |                      空                       | int64:想定结束时间                       |

### 2.2.2 订阅仿真数据参数

CSubscribeSimData为仿真数据类

| 序号 |  字段名称  |  类型  |         说明         |
| :--: | :--------: | :----: | :------------------: |
|  1   |   topic    | char[] |     仿真数据主题     |
|  2   | platformId | int64  |      自身实体id      |
|  3   |  targetId  | int64  | 目标实体id或者阵营id |

### 2.2.3 platFormEntity 实体字段说明

| 序号 |          字段名称        |                类型          |         说明            |
| :--: | :-------------------: | :-------------------------:  | :------------------:   |
|  1   |    attribute          | PlatformEntityAttribute      |     实体的属性值字段      |
|  2   |    components         |  PriorityQueue<IComponent>   |      实体的组件列表       |
|  3   |  platformEntityAgent  |  IPlatformEntityAgent        |      实体创建的代理类     |

### 2.2.4 IComponent 组件字段说明

| 序号 |  字段名称  |  类型  |         说明     |
| :--: | :--------------: | :-------------: | :------------------: |
|  1   |  platformEntity  |  PlatformEntity |          实体            |
|  2   |     attributes   |  ISimAttribute  |        组件的属性      |
|  3   |       enable     |     boolean     |  组件是否可用 默认为true    |
|  4   | componentCurTime |       long      |    组件的当前推演时间（ms）  |
|  5   |      stepTime    |       long      |    组件的步长（ms）   |
|  6   |  simModelAgent   |  ISimModelAgent |        引擎代理类 |
|  7   |        id        |       long      |         组件的id |

1）提供模型对外的头文件；

提供模型lib库，包括debug和release；

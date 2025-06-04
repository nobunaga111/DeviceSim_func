#pragma once
#include <list>
#include <vector>
#include <map>

#include <cstring>  // 使用memset函数




//接口主题名

//被动探测声呐系统模型
#define ATTR_PassiveSonarComponent				"ATTR_PassiveSonarComponent"				//被动探测声呐初始化信息


//导航模型

#define DATA_NAVTOPIC_DATA_NAVINFO				"DATA_NAVTOPIC_DATA_NAVINFO"				//导航位置速度信息，信源：导航模型


#define Data_PlatformSelfSound					"Data_PlatformSelfSound"					//平台区自噪声模型输出接口 信源：平台区自噪声模型
#define Data_PlatformSelfSound_X1				"Data_PlatformSelfSound_X1"					//平台区自噪声模型输出接口 信源：X1平台区自噪声模型


#define MSG_PropagatedActivePulseSoundToTS		"MSG_PropagatedActivePulseSoundToTS"		//传播后主动脉冲声呐特性给目特



#define Data_Motion								"Data_Motion"								//平台机动信息接口 信源：机动模型
#define MSG_ActivePulseSound					"MSG_ActivePulseSound"						//主动脉冲声呐
#define MSG_CommPulseSound						"MSG_CommPulseSound"						//通信脉冲声纳
#define MSG_PropagatedContinuousSound			"MSG_PropagatedContinuousSound"				//传播后连续声特性 信源：信道
#define MSG_PropagatedActivePulseSound			"MSG_PropagatedActivePulseSound"			//传播后主动脉冲声呐特性给声呐 信源：信道
#define MSG_PropagatedCommPulseSound			"MSG_PropagatedCommPulseSound"				//传播后通信脉冲声呐特性信源：信道
#define MSG_EnvironmentNoiseToSonar				"MSG_EnvironmentNoiseToSonar"				//海洋环境噪声 信源：信道
#define MSG_ReverberationSound					"MSG_ReverberationSound"					//混响声呐数据 信源：信道
#define MSG_PropagatedInstantSound				"MSG_PropagatedInstantSound"				//传播后瞬态声特性 信源：信道


/********声纳导调配置初始化数据结构*******/
//导调

#define     MSG_ActiveTransmitControlPara	  "MSG_ActiveTransmitControlPara"          //导调主动发射声纳功能配置
#define     MSG_ActiveSonarControlPara	      "MSG_ActiveSonarControlPara"          //导调主动声纳功能配置
#define     MSG_PassiveSonarControlPara	      "MSG_PassiveSonarControlPara"         //导调被动声纳功能配置
#define     MSG_DetectiveSonarControlPara	  "MSG_DetectiveSonarControlPara"       //导调侦察声纳功能配置
#define     MSG_DataCombineControlPara			"MSG_DataCombineControlPara"       //导调单平台多阵数据融合功能配置
//导调类主题-探测能力分析
#define MSG_Direct_DetectAnalysisInput_Topic	"MSG_Direct_DetectAnalysisInput_Topic"        //引擎发送探测能力计算导调输入;
#define Data_Direct_DetectAnalysisOutput_Sonar_Topic	"DATA_Direct_DetectAnalysisOutput_Sonar_Topic"        //声呐模型探测能力计算结果输出;

//声纳指控指令
#define     MSG_SonarCommandControlOrder	  "MSG_SonarCommandControlOrder"       //声纳指控指令参数


//声纳输出结果
#define 	Data_SonarState_Topic			  "Data_SonarState_Topic"               //状态类接口  声纳工作状态输出
#define 	Msg_SonarWorkState				"Msg_SonarWorkState"					//声呐工作状态
#define 	MSG_PassiveSonarResult_Topic      "MSG_PassiveSonarResult_Topic"        //交互类接口 被动声纳处理结果输出
#define 	MSG_TorpedoResult_Topic           "MSG_TorpedoResult_Topic"             //交互类接口  鱼雷报警处理结果输出
#define     MSG_ActiveSonarResult_Topic		  "MSG_ActiveSonarResult_Topic"	        //主动声纳处理结果输出
#define     MSG_ScoutingSonarResult_Topic	  "MSG_ScoutingSonarResult_Topic"		//侦察声纳处理结果输出
#define     MSG_PassiveCombineModelResult		  "MSG_PassiveCombineModelResult"			//单平台多阵被动数据融合处理结果输出
#define     MSG_ScoutingCombineModelResult	  "MSG_ScoutingCombineModelResult"		//单平台多阵侦察数据融合处理结果输出
#define     MSG_TorpedoCombineModelResult	  "MSG_TorpedoCombineModelResult"		//单平台多阵鱼雷数据融合处理结果输出
#define		MSG_SyncSituation			      "MSG_SyncSituation"			            	//主动、多基地同步态势输出
#define		MSG_SonarTaskState			      "MSG_SonarTaskState"                     //声纳任务状态输出
#define 	DATA_PassiveSonarResult_Topic      "DATA_PassiveSonarResult_Topic"        //数据类接口 被动声纳处理结果输出
#define 	DATA_TorpedoResult_Topic           "DATA_TorpedoResult_Topic"             //数据类接口  鱼雷报警处理结果输出
#define     DATA_ActiveSonarResult_Topic		"DATA_ActiveSonarResult_Topic"	        //主动声纳处理结果输出
#define     DATA_ScoutingSonarResult_Topic	  	"DATA_ScoutingSonarResult_Topic"		//侦察声纳处理结果输出


//蓝方行为模型输出

#define DATA_PX_PULLMOTION_TOPIC "DATA_PX_PULLMOTION_TOPIC"            //拖曳阵机动数据;


// 导航状态枚举
enum C_e_navmodel_nav_status {
    NMNS_INVALID = 0,   // 无效状态
    NMNS_NORMAL = 1,    // 正常状态
    NMNS_WARNING = 2,   // 警告状态
    NMNS_ERROR = 3      // 错误状态
};

// 位置XYZ结构体
struct positionXYZ {
    double x;
    double y;
    double z;
    positionXYZ() : x(0.0), y(0.0), z(0.0) {}
};


/********威胁判断及静音方案模型*******/
// 声呐模型利用此接口发布目标情报数据	 主题：MSG_TOPIC_MUTE_SENSOR_DETECTED
struct CMsg_SensorDetectedTarget
{
    int m_targetType;				//目标类型   0：未知、1：水面舰、2：潜艇、3：直升机、4：浮标、5：鱼雷
    float m_targetChord;			//目标舷角（度）	-180~180
    float m_targetDistance;			//目标俯仰角（度）  -90~90
    int m_pulseObj;					//脉冲对象类型		0：未知、1：水面舰、2：潜艇、3：直升机、4：浮标、5：鱼雷
    CMsg_SensorDetectedTarget() :m_targetType(0)
        , m_targetChord(0)
        , m_targetDistance(0)
        , m_pulseObj(0) {}
};



//侦查结果,主题:MSG_PX_SONARSCOUTINGRESULT_TOPIC
struct CMsg_SonarScoutingResut
{
    int m_num;							// 信号数量
    double m_signalID[50];				// 信号 ID
    double m_signalSide[50];			// 信号舷朝向
    double m_signalPulseBearing[50];	// 信号脉冲方位
    double m_signalWidth[50];			// 脉冲宽度
    double m_sn[50];					// 信噪比
    int m_freq[50];						// 信号频率
    int m_signalForm[50];				// 信号形式 1 主动脉冲信号 2 通信脉冲信号
    int m_scoutingInterval;				// 侦查周期 秒
    CMsg_SonarScoutingResut() :m_num(1)
        , m_scoutingInterval(1)
    {
        memset(m_signalID, 0, sizeof(m_signalID));
        memset(m_signalSide, 0, sizeof(m_signalSide));
        memset(m_signalPulseBearing, 0, sizeof(m_signalPulseBearing));
        memset(m_signalWidth, 0, sizeof(m_signalWidth));
        memset(m_sn, 0, sizeof(m_sn));
        memset(m_freq, 0, sizeof(m_freq));
        memset(m_signalForm, 0, sizeof(m_signalForm));
    }
};


/***************防御鱼雷方案规划********************/
//被动声纳探测结果,主题:DATA_PX_PASSSONARDETECTRESULT_TOPIC
struct CData_PassSonarDetectResult
{
    int targetType;				//目标类型
    double targetAngle;			//目标方位
    CData_PassSonarDetectResult() :targetType(1)
        , targetAngle(0)
    {}
};

//侦察声纳探测结果主题,主题:DATA_PX_DETECTSONARDETECTRESULT_TOPIC
struct CData_DetectSonarDetectResult
{
    int targetType;				//目标类型
    double targetAngle;			//目标方位
    CData_DetectSonarDetectResult() :targetType(1)
        , targetAngle(0)
    {}
};

//声纳鱼雷报警结果,主题：DATA_PX_SONARTORPEDOALARMRESULT_TOPIC
struct CData_SonarTorpedoAlarmResult
{
    double torpedoAngle; //鱼雷的方位
    double torpedoDistance; //鱼雷的距离
    CData_SonarTorpedoAlarmResult() :torpedoAngle(0)
        , torpedoDistance(0)
    {}
};



/************被动探测声呐系统模型**************/
//被动探测声呐初始化信息 主题：ATTR_PassiveSonarComponent
struct CAttr_PassiveSonarComponent
{
    int sonarID;				//声呐编号	1~7   1：艏部  2：  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵
    int sonarMod;				//工作模型			1：主动   2：被动
    int sfb;					//子带频宽				1~10000
    int arrayNumber;			//传感器数目			1~100
    int fs;						//采样率				0~10000
    int SSpeed_range;			//可工作的航速范围		0~100
    float Depth_range;			//可工作的深度范围		0~10000
    float RPhi;					//声呐水平开角
    float RTheta;				//声呐俯仰开角			-180~180
    int BandRange;				//声呐可工作频带范围		0~10000
    int BandFormingAnum;		//水平波束数		1~360
    float BandFormingAdeg;		//水平波束角度		-180~180
    int BandFormingPnum;		//垂直波束数		1~180
    float BandFormingPdeg;		//垂直波束角度		-90~90
    float array_xyz;			//阵元位置坐标		-1000~1000
    bool array_work;			//阵元工作状态    0：失效   1：工作
    int TrackingTime;			//稳定跟踪时长		1~100
    float Gnf;					//阵型噪声增益		0~100
    float DOA_mse_surf;			//测向精度曲面		0~100
    int RelativeBandwidth;		//相干带宽 Hz  0~40000
    float RelativeTimewidth;	//相干时间S		0~100
    int DetectionAlgorithm;		//检测方法		0：能量检测   1：线谱检测
    int BFAlgorithm;			//波束形成方法	0~5  	0：线列CBF  1：补偿CBF   2：体积CBF		4：阵元畸变线列CBF  5：阵元失效鲁棒波束形成
    int TrackingNum;			//最大跟踪目标数   0~100
    CAttr_PassiveSonarComponent() :sonarID(1)
        , sonarMod(1)
        , sfb(1)
        , arrayNumber(1)
        , fs(0)
        , SSpeed_range(0)
        , Depth_range(0)
        , RPhi(0)
        , RTheta(0)
        , BandRange(0)
        , BandFormingAnum(1)
        , BandFormingAdeg(0)
        , BandFormingPnum(1)
        , BandFormingPdeg(0)
        , array_xyz(0)
        , array_work(1)
        , TrackingTime(1)
        , Gnf(0)
        , DOA_mse_surf(0)
        , RelativeBandwidth(0)
        , RelativeTimewidth(0)
        , DetectionAlgorithm(0)
        , BFAlgorithm(0)
        , TrackingNum(0) {}
};



/*************************/

// 导航信息定义,主题：DATA_NAVTOPIC_DATA_NAVINFO
struct CData_st_navmodel_navinfo
{
    uint64_t timestamp;						// 系统时间(毫秒)
    uint64_t timestamp_status;				// 当前状态时间(毫秒)
    C_e_navmodel_nav_status status;			// 导航状态，定义见 @e_navmodel_nav_status
    double latitude;						// 纬度(度，北纬为正，南纬为负)
    double longitude;						// 经度(度，东经为正，西经为负)
    double altitude;						// 高度/深度(米 往上为正，往下为负)
    double velocity_east;					// 东向速度(节)
    double velocity_north;					// 北向速度(节)
    double velocity_upper;					// 垂向速度(节)
    double velocity;						// 合速度(节)
    double roll;							// 横摇角(度，左起为正，-180~180)
    double pitch;							// 纵摇角(度，抬头为正，-90~90)
    double heading;							// 航向角(度，顺时针为正，0~360)
    double roll_rate;						// 横摇角速度(度/秒)s
    double pitch_rate;						// 俯仰角速度(度/秒)
    double heading_rate;					// 航向角速度(度/秒)
    double heave;							// 升沉位移(米 往上为正，往下为负)
    CData_st_navmodel_navinfo() :timestamp(0)
        , timestamp_status(0)
        , status(NMNS_INVALID)
        , latitude(0)
        , longitude(0)
        , altitude(0)
        , velocity_east(0)
        , velocity_north(0)
        , velocity_upper(0)
        , velocity(0)
        , roll(0)
        , pitch(0)
        , heading(0)
        , roll_rate(0)
        , pitch_rate(0)
        , heading_rate(0)
        , heave(0)
    {}
};



struct C_SelfSoundSpectrumStruct
{
    int sonarID;			//声纳编号		1~7
    float spectumData[5296];	//频谱数据dB/Hz
    C_SelfSoundSpectrumStruct() :sonarID(1)
    {
        memset(spectumData, 0, sizeof(spectumData));
    }
};

//潜艇平台区噪声数据  主题：Data_PlatformSelfSound
struct CData_PlatformSelfSound
{
    std::list<C_SelfSoundSpectrumStruct> selfSoundSpectrumList;
};




//实体机动状态，主题名：Data_Motion
struct CData_Motion
{
    char* name;					//实体名
    bool action;				//实体行动状态 false:异常   true：正常
    bool isPending;				//实体挂起状态  false：正常      true：挂起
    double x;					//经度
    double y;					//纬度
    double z;					//高度  水下为负
    double curSpeed;			//速度
    double mVerticalSpeed;		//垂向速度（m/s)
    double rotation;			//航向
    double mAcceleration;		//纵向加速度
    double roll;				//横摇角
    double yaw;					//艏摇角
    double pitch;				//纵摇角
    double mRollVel;			//横摇角速度
    double mPitchVel;			//纵摇角速度
    double mYawVel;				//艏摇角速度
    CData_Motion() :name(nullptr)
        , action(true)
        , isPending(true)
        , x(0)
        , y(0)
        , z(0)
        , mVerticalSpeed(0)
        , mAcceleration(0)
        , rotation(0.0)
        , roll(0.0)
        , yaw(0.0)
        , pitch(0.0)
        , curSpeed(0.0)
        , mRollVel(0.0)
        , mPitchVel(0.0)
        , mYawVel(0.0) {}
};

//主动声纳脉冲数据，主题名；MSG_ActivePulseSound
struct CMsg_ActivePulseSoundSpectrumStruct
{
    float spectrumData[5296];		//频谱数据	100Hz~40kHz,间隔2Hz(100Hz-10kHz)、间隔1kHz(10kHz-40kHz)
    float horizontalDirection[72];			//水平指向
    float durationTime;						//信号长度，单位（s）
    float verticalAngle[2];		//垂直开角，单位（度）  0~90

    float centralFreq;						//中心频率
    float signalWidth;						//信号带宽
    int signalType;							//信号类型
    float pulseWidth;						//脉宽			秒
    int targetType;							//目标类型      1-潜艇，2-大型水面舰，3-中小型水面舰，4-民船，5-直升机，6-反潜飞机，7-浮标，8-鱼雷，9-水雷，10-不明
    long long transmitTime;					//信号发射时间，单位（ms）
    CMsg_ActivePulseSoundSpectrumStruct() :durationTime(0.0)
        , centralFreq(0.0)
        , signalWidth(0.0)
        , signalType(0)
        , pulseWidth(0.0)
        , targetType(0)
        , transmitTime(0)
    {
        memset(horizontalDirection, 0, sizeof(horizontalDirection));
        memset(spectrumData, 0, sizeof(spectrumData));
        memset(verticalAngle, 0, sizeof(verticalAngle));
    }
};

//通信声纳脉冲数据，主题名：MSG_CommPulseSound
struct CMsg_CommPulseSoundSpectrumStruct
{
    float spectrumData[5296];		//频谱数据	100Hz~40kHz,间隔2Hz(100Hz-10kHz)、间隔1kHz(10kHz-40kHz)
    float horizontalDirection[72];			//水平指向
    float durationTime;						//信号长度，单位（s）
    int signalType;							//信号类型
    float verticalAngle;					//垂直开角，单位（度） 0~90
    float centralFreq;						//中心频率
    float signalWidth;						//信号带宽
    float pulseWidth;						//脉宽
    float expandCoefficient;				//扩频系数
    float propLoss;							//中心频率传播损失
    float commRate;							//通信速率，单位（bps）
    int commDataLenth;						//通信数据长度，单位（BYTE）
    char commContent[10000];						//通信数据内容
    CMsg_CommPulseSoundSpectrumStruct() :durationTime(0.0)
        , signalType(0)
        , verticalAngle(0.0)
        , centralFreq(0.0)
        , signalWidth(0.0)
        , pulseWidth(0.0)
        , expandCoefficient(0.0)
        , propLoss(0.0)
        , commRate(0.0)
        , commDataLenth(0) {
        memset(horizontalDirection, 0, sizeof(horizontalDirection));
        memset(spectrumData, 0, sizeof(spectrumData));
        memset(commContent, 0, 10000);
    }
};

struct C_PropagatedContinuousSoundStruct
{
    float arrivalSideAngle;					//波达舷角
    float arrivalPitchAngle;				//波达俯仰角
    float targetDistance;					//目标距离，单位（米）
    int platType;							//目标类型
    float spectrumData[5296];				//频谱数据	10Hz~40kHz,间隔2Hz(10Hz-10kHz)、间隔1kHz(10kHz-40kHz)

    C_PropagatedContinuousSoundStruct() :arrivalSideAngle(0.0)
        , arrivalPitchAngle(0.0)
        , targetDistance(0.0)
        , platType(0)
    {
        memset(spectrumData, 0, sizeof(spectrumData));
    }
};

//平台辐射噪声经传播后，到达声纳的信号数据，属连续声 主题名：MSG_PropagatedContinuousSound
struct CMsg_PropagatedContinuousSoundListStruct
{
    std::list<C_PropagatedContinuousSoundStruct> propagatedContinuousList; // ?
};

struct C_PropagatedInstantSoundStruct
{
    float targetDistance;					//目标距离，单位（米）
    long long trrivalTime;						//波到达时间，单位（s）
    float arrivalSideAngle;					//波达舷角
    float arrivalPitchAngle;				//波达俯仰角
    double lastTime;						//持续时间
    float spectrumData[5296];				//频谱数据	10Hz~40kHz,间隔2Hz(10Hz-10kHz)、间隔1kHz(10kHz-40kHz)
    C_PropagatedInstantSoundStruct() : targetDistance(0.0)
        , trrivalTime(0)
        , arrivalSideAngle(0.0)
        , arrivalPitchAngle(0.0)
        , lastTime(0.0)
    {
        memset(spectrumData, 0, sizeof(spectrumData));
    }
};
//平台瞬态噪声经传播后，到达脉冲声纳的信号数据， 主题名： MSG_PropagatedInstantSound
struct CMsg_PropagatedInstantSoundListStruct
{
    std::list<C_PropagatedInstantSoundStruct> propagatedInstantSoundList;
};

struct C_ShockResStruct
{
    float waveTime;							//激波时间，单位（s）
    float waveDegree;						//幅值
    C_ShockResStruct() :waveTime(0.0)
        , waveDegree(0.0) {}
};
typedef std::list<C_ShockResStruct> C_ShockResListStruct;

struct C_PropagatedActivePulseSoundStruct
{
    long long arrivalTime;						//波到达时间，单位（s）
    float arrivalSideAngle;					//波达舷角
    float arrivalPitchAngle;				//波达俯仰角
    float targetDistance;					//目标距离，单位（米）
    C_ShockResListStruct shockRes;			//冲激响应
    float spectrumData[5296];				//频谱数据	100Hz~40kHz,间隔2Hz(100Hz-10kHz)、间隔1kHz(10kHz-40kHz)
    float signalLength;						//信号长度
    float centralFreq;						//中心频率
    float signalWidth;						//信号带宽
    int signalType;							//信号类型
    float pulseWidth;						//脉宽
    int targetType;							//目标类型
    C_PropagatedActivePulseSoundStruct() :arrivalTime(0)
        , arrivalSideAngle(0.0)
        , arrivalPitchAngle(0.0)
        , targetDistance(0.0)
        , signalLength(0.0)
        , centralFreq(0.0)
        , signalWidth(0.0)
        , signalType(0)
        , pulseWidth(0.0)
        , targetType(0)
    {
        memset(spectrumData, 0, sizeof(spectrumData));
    }
};

//主动脉冲声经传播后，到达声纳的信号数据， 主题名：MSG_PropagatedActivePulseSound
struct CMsg_PropagatedActivePulseSoundListStruct
{
    std::list<C_PropagatedActivePulseSoundStruct> propagateActivePulseList;
};

//传播后主动脉冲声呐特性给目特		主题：MSG_PropagatedActivePulseSoundToTS
struct  CMsg_PropagatedActivePulseSoundToTS
{
    std::list<C_PropagatedActivePulseSoundStruct> propagateActivePulseList;
};

struct C_PropagatedCommPulseSoundStruct
{
    long long arrivalTime;						//波达时间，单位（s）
    float arrivalSideAngle;					//波达舷角
    float arrivalPitchAngle;				//波达俯仰角
    float targetDistance;					//目标距离，单位（米）
    float durationTime;						//信号长度，单位（s）
    float spectrumData[5296];				//频谱数据
    C_ShockResListStruct shockRes;			//冲激响应
    float centralFreq;						//中心频率
    float signalWidth;						//信号带宽
    int signalType;							//信号类型
    float pulseWidth;						//脉宽
    int targetType;							//目标类型
    float expandCoefficient;				//扩频系数
    float propLoss;							//中心频率传播损失
    float commRate;							//通信速率，单位（bps）
    int commDatalenth;						//通信数据长度
    char commContent[10000];				//通信数据内容
    C_PropagatedCommPulseSoundStruct() :arrivalTime(0)
        , arrivalSideAngle(0.0)
        , arrivalPitchAngle(0.0)
        , targetDistance(0.0)
        , durationTime(0.0)
        , centralFreq(0.0)
        , signalWidth(0.0)
        , signalType(0)
        , pulseWidth(0.0)
        , targetType(0)
        , expandCoefficient(0.0)
        , propLoss(0.0)
        , commRate(0.0)
        , commDatalenth(0)
    {
        memset(spectrumData, 0, sizeof(spectrumData));
        memset(commContent, 0, 10000);
    }
};

//通信脉冲声经传播后，到达声纳的信号数据 主题名：MSG_PropagatedCommPulseSound
struct CMsg_PropagatedCommPulseSoundListStruct
{
    std::list<C_PropagatedCommPulseSoundStruct> propagatedCommList;
};


//海洋环境噪声数据 主题名：MSG_EnvironmentNoiseToSonar
struct CMsg_EnvironmentNoiseToSonarStruct
{
    float spectrumData[5296];				//频谱数据	10Hz~40kHz,间隔2Hz(10Hz-10kHz)、间隔1kHz(10kHz-40kHz)
    float acousticVel;						//接收点声速  m/s
    CMsg_EnvironmentNoiseToSonarStruct() :acousticVel(0.0)
    {
        memset(spectrumData, 0, sizeof(spectrumData));
    }
};


struct C_RLDataStruct
{
    float time;								//时间，单位（s）
    float rl;								//混响级
    C_RLDataStruct() :time(0.0), rl(0.0) {}
};
typedef std::list<C_RLDataStruct> C_RLDataListStruct;

//混响声呐数据 主题名：MSG_ReverberationSound
struct CMsg_ReverberationSoundStruct
{
    C_RLDataListStruct RLGraph;			//混响级数据曲线
};




//导调主动声纳配置，主题名；MSG_ActiveSonarControlPara
struct CMsg_ActiveSonarControlPara {
    int filterDigitalLevel; //数字滤波器幅频响应档位
    int falsealarmProperty;//虚警概率	 0至1  【放信号处理参数里】1
    int verticalBeam;	//波束垂直俯仰角  取值范围:- 90至90  【放信号处理参数里】

    CMsg_ActiveSonarControlPara() :
        filterDigitalLevel(0),
        falsealarmProperty(0),
        verticalBeam(0)
    {}
};
//主动发射导调  topic： MSG_ActiveTransmitControlPara
struct CMsg_ActiveTransmitControlPara
{
    int sonarID;
    int sonarKeying;	//主动声na开关	  0 - 关1 - 开	【放信号处理参数里】
    /*************************************new*******************************************************************************************************************/
    int gearSpectrumTransmit; //发射信号频谱挡位    【发射阵参数里，spectrumDataTransmit的key】
    /********************************************************************************************************************************************************/
    int gearTransmit;	//发射声源级档位，是个计算系数	取值范围:1 - 3	 1 - 全功率 2 - 半功率 3 - 1 / 4功率     【放发射阵参数里】1
    float centralFreq;//	中心频率	取值范围:0至40000		【放发射阵参数里】1
    float signalWidth;//	信号带宽	取值范围:0至40000		【放发射阵参数里】1
    int signalType;//  信号类型  取值范围:11/21/22/31/32/41/42/51/52   【放发射阵参数里】1
                   //定义：11-CW；21-上LFM；22-下LFM；31-上HFM；32-下HFM；41-上LFM+CW；42-下LFM+CW；51-上HFM+CW；52-下HFM+CW
    float pulseWidth;//	脉宽	取值范围:0 - 10				【放发射阵参数里】1
    float rangeDetection;//探测量程 //依据建模工具中配置的参数项进行配置
    int methodDetection;//探测方式 //依据建模工具中配置的参数项进行配置
    int numberDetection;//探测次数 //依据建模工具中配置的参数项进行配置
    int filterDigitalLevel;
    CMsg_ActiveTransmitControlPara() :
        sonarID(0),
        sonarKeying(0),
        gearSpectrumTransmit(0),
        gearTransmit(0),
        centralFreq(0.0),
        signalWidth(0.0),
        signalType(0),
        pulseWidth(0.0),
        rangeDetection(0.0),
        methodDetection(0),
        numberDetection(0),
        filterDigitalLevel(0)

    {}
};




//导调被动声纳配置，主题名；MSG_PassiveSonarControlPara
struct CMsg_PassiveSonarControlPara
{
    // 被动声纳可配置参数
    int sonarID;
    int workMode;
    int processingAlgorithm;
    float thresholdValue;

    CMsg_PassiveSonarControlPara() :
        sonarID(0),
        workMode(0),
        processingAlgorithm(0),
        thresholdValue(0.0f)
    {}
};

//导调侦察声纳配置，主题名；MSG_DetectiveSonarControlPara
struct CMsg_DetectiveSonarControlPara
{
    // 侦察声纳可配置参数
    int sonarID;
    int scanMode;
    int frequencyRange;
    int scanInterval;

    CMsg_DetectiveSonarControlPara() :
        sonarID(0),
        scanMode(0),
        frequencyRange(0),
        scanInterval(0)
    {}
};

/************声呐阵系统初始化数据结构**************/
//声纳初始化信息 主题：ATTR_SonarArrayComponent
//阵列参数
//主动静态参数配置 C_ActiveSonarSetting
struct C_ActiveSonarSetting
{
    int sonarType;
    float operatingFrequency;
    float maxTransmitPower;
    int beamFormingMethod;

    C_ActiveSonarSetting() :
        sonarType(0),
        operatingFrequency(0.0f),
        maxTransmitPower(0.0f),
        beamFormingMethod(0)
    {}
};

//被动静态参数配置  C_PassiveSonarSetting
struct C_PassiveSonarSetting
{
    int hydrophones;
    float sensitivity;
    int spectrumAnalysisType;

    C_PassiveSonarSetting() :
        hydrophones(0),
        sensitivity(0.0f),
        spectrumAnalysisType(0)
    {}
};


//侦察静态参数配置
struct C_ScoutingSonarSetting
{
    int signalAnalysisMode;
    float frequencyScanStart;
    float frequencyScanEnd;

    C_ScoutingSonarSetting() :
        signalAnalysisMode(0),
        frequencyScanStart(0.0f),
        frequencyScanEnd(0.0f)
    {}
};

// 位置（阵列静态参数配置使用）
struct C_Position
{
    float x;
    float y;
    float z;
    C_Position() : x(0.0)
        , y(0.0)
        , z(0.0) {}
};
//阵列静态参数配置
struct C_SonarArrayarraySetting
{
    int sonarID;				//声呐编号	1~7   1：艏部  2：舷侧左前  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵
    int activeSonarKeying;		//主动声纳功能 0:没有此功能   1:有此功能
    int passiveSonarKeying;     //被动声纳功能 0:没有此功能   1:有此功能
    int scoutingSonarKeying;    //侦察声纳功能 0:没有此功能   1:有此功能

    // 其他阵列设置参数
    int arrayType;
    float workingDepthMin;
    float workingDepthMax;

    C_SonarArrayarraySetting() :
        sonarID(0),
        activeSonarKeying(0),
        passiveSonarKeying(0),
        scoutingSonarKeying(0),
        arrayType(0),
        workingDepthMin(0.0f),
        workingDepthMax(0.0f)
    {}
};

//发射阵阵列参数
struct C_ArrayOther
{
    int transmitterElements;
    float maxOutputPower;
    float beamWidth;

    C_ArrayOther() :
        transmitterElements(0),
        maxOutputPower(0.0f),
        beamWidth(0.0f)
    {}
};

//发射信号处理参数
struct C_ActiveTransmitPara
{
    int sonarKeying;//主动声纳开关
    float rangeDetection;//探测量程
    int methodDetection;//探测方式
    int numberDetection;//探测次数

    C_ActiveTransmitPara() :
        sonarKeying(0),
        rangeDetection(0.0f),
        methodDetection(0),
        numberDetection(0)
    {}
};
//发射阵静态参数
struct CAttr_ActiveTransmitComponent
{
    C_ArrayOther arrayOther;
    C_ActiveTransmitPara activeTransmitPara;

};


//接收阵
struct CAttr_SonarArrayComponent
{
    C_ActiveSonarSetting activeSonarSetting;
    C_PassiveSonarSetting passiveSonarSetting;
    C_ScoutingSonarSetting scoutingSonarSetting;
    C_SonarArrayarraySetting SonarArrayarraySetting;

};


//声纳指控+指令参数
struct CMsg_SonarCommandControlOrder
{
    int sonarID;			//声呐编号	1~8   1：艏部  2：舷侧左前  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵  8：吊声收
    int arrayWorkingOrder; //阵列工作状态 0:关机、1:开机
    int activeWorkingOrder; //接收阵主动声纳工作状态  0:关机、1:开机
    int passiveWorkingOrder; //接收阵被动声纳功能状态  0:关机、1:开机
    int scoutingWorkingOrder; //接收阵侦察声纳功能状态  0:关机、1:开机
    int multiSendWorkingOrder; //接收阵多基地声纳发射功能状态  0:关机、1:开机
    int multiReceiveWorkingOrder; //接收阵多基地声纳接收功能状态  0:关机、1:开机
    int activeTransmitWorkingOrder;//发射阵主动发射功能状态 0:关机、1:开机

    CMsg_SonarCommandControlOrder() : sonarID(0)
        , arrayWorkingOrder(0)
        , activeWorkingOrder(0)
        , passiveWorkingOrder(0)
        , scoutingWorkingOrder(0)
        , multiSendWorkingOrder(0)
        , multiReceiveWorkingOrder(0)
        , activeTransmitWorkingOrder(0) {}
};
/********声纳模型*******/
// 声纳模型利用此接口发布声纳的工作状态,主题：Data_SonarState_Topic
struct CData_SonarState
{
    int sonarID;			//声呐编号	1~7   1：艏部  2：舷侧左前  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵  8：吊声收
    int arrayWorkingState; //阵列工作状态 0:关机、1:开机
    int activeWorkingState; //接收阵主动声纳工作状态  0:关机、1:开机
    int passiveWorkingState; //接收阵被动声纳功能状态  0:关机、1:开机
    int scoutingWorkingState; //接收阵侦察声纳功能状态  0:关机、1:开机
    int multiSendWorkingState; //接收阵多基地声纳发射功能状态  0:关机、1:开机
    int multiReceiveWorkingState; //接收阵多基地声纳接收功能状态  0:关机、1:开机
    int activeTransmitWorkingState;//发射阵主动发射功能状态 0:关机、1:开机

    CData_SonarState() : sonarID(0)
        , arrayWorkingState(0)
        , activeWorkingState(0)
        , passiveWorkingState(0)
        , scoutingWorkingState(0)
        , multiSendWorkingState(0)
        , multiReceiveWorkingState(0)
        , activeTransmitWorkingState(0) {}
};

//声呐工作状态 主题名称：Msg_SonarWorkState

struct CMsg_SonarWorkState
{
    CMsg_SonarWorkState()
    {
        platformId = 0;
        sonarOnOff = false;
        maxDetectRange = 0;
    }

    int   platformId;	    //平台编号
    int   maxDetectRange;	//最大探测距离，单位为米
    bool  sonarOnOff;	    //声纳开机状态 false-关机 true-开机
};


//被动声纳检测结果结构体
struct C_PassiveSonarDetectionResult
{
    float detectionDirArray; //方位角估计值检测-阵坐标系 -180至180
    float detectionDirGroud; //方位角估计值检测-大地坐标系 0至360
    C_PassiveSonarDetectionResult() :detectionDirArray(0.0)
        , detectionDirGroud(0.0)
    {}
};
//被动声纳跟踪结果结构体
struct C_PassiveSonarTrackingResult
{
    int trackingID; // 跟踪批次号 0~1000
    float SNR;	//信噪比  精度范围：-3  取值范围：-50~50
    float spectumData[5296];	//频谱数据 跟踪批次号对应的 0~10000
    int trackingStep; //跟踪时长 0~1000
    float trackingDirArray; //方位角估计值跟踪-阵坐标系 -180至180
    float trackingDirGroud; //方位角估计值跟踪-大地坐标系 0至360
    int recognitionResult; //目标识别结果 0-10
    float recognitionPercent[5]; //目标识别置信度 0-1
    C_PassiveSonarTrackingResult() :trackingID(0)
        , trackingStep(0)
        , trackingDirArray(0.0)
        , trackingDirGroud(0.0)
        , recognitionResult(0)
    {
        memset(spectumData, 0, sizeof(spectumData));
        memset(recognitionPercent, 0, sizeof(recognitionPercent));
    }
};


//被动声纳处理结果输出,主题：MSG_PassiveSonarResult_Topic
struct CMsg_PassiveSonarResultStruct
{
    int sonarID;			//声呐编号	1~7   1：艏部  2：舷侧左前  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵
    int detectionNumber;    //检测目标数量 0~100
    std::vector<C_PassiveSonarDetectionResult> PassiveSonarDetectionResult; //检测结果
    std::vector<C_PassiveSonarTrackingResult>  PassiveSonarTrackingResult;  //跟踪结果
    CMsg_PassiveSonarResultStruct() :detectionNumber(0)
    {
        PassiveSonarDetectionResult.clear();
        PassiveSonarTrackingResult.clear();
    }

};

//单个鱼雷报警处理结果输出结果
struct C_TorpedoResult
{
    int TrackingID;		//跟踪批次号		继承跟踪批次号
    int warningLevel; // 鱼雷威胁等级 1无威胁（非鱼雷）、2低威胁、3中威胁、4高威胁
    float warningDirArray; //方位角估计值跟踪-阵坐标系-180至180
    float warningDirGroud; //方位角估计值跟踪-大地坐标系0至360
    float warningDistance; //敌方鱼雷距离0-100km
    long long PulseTime;		//瞬态噪声的到达时刻
    float PulseDirArray;	//瞬态噪声方位角
    float PulseDirGround;	//瞬态噪声舷角
    int warningRecognitionResult; //鱼雷识别类型 1~3类 1-潜射	2-空投 3-舰射管装鱼雷
    float warningRecognitionPercent[4];	//鱼雷识别置信度

    C_TorpedoResult() :
        TrackingID(0),
        warningLevel(0),
        warningDirArray(0.0),
        warningDirGroud(0.0),
        warningDistance(0.0),
        PulseTime(0),
        PulseDirArray(0.0),
        PulseDirGround(0.0),
        warningRecognitionResult(0),
        warningRecognitionPercent{ 0.0, 0.0, 0.0, 0.0 }
    {}

};

//鱼雷报警处理结果输出，主题名；MSG_TorpedoResult_Topic
struct CMsg_TorpedoResult
{
    int sonarID;			//声呐编号	1~7   1：艏部  2：舷侧左前  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵
    std::vector<C_TorpedoResult> TorpedoResult;  //0~100
};


//主动声纳检测结果结构体
struct C_ActiveSonarDetectionResult
{
    float SNR;
    float detectionDirArray; //方位角估计值检测-阵坐标系 -180至180
    float detectionDirGroud; //方位角估计值检测-大地坐标系0至360
    float detectionDistance; //距离估计值检测 0至30000
    float detectionVelocity; //径向速度估计值检测 -30至30
    float detectionLat; //定位结果(纬度)-90至90
    float detectionLon; //定位结果(经度)-180至180
    C_ActiveSonarDetectionResult() :
        SNR(0.0),
        detectionDirArray(0.0),
        detectionDirGroud(0.0),
        detectionDistance(0.0),
        detectionVelocity(0.0),
        detectionLat(0.0),
        detectionLon(0.0)
    {}
};
//主动声纳跟踪结果结构体
//改recognitionPercent[5]为recognitionPercent[3]
struct C_ActiveSonarTrackingResult
{
    int trackingID; // 跟踪批次号 0~1000
    int trackingStep; //跟踪时长 0~1000
    float trackingLat; //跟踪结果(纬度-90至90
    float trackingLon; //跟踪结果(经度) -180至180
    int recognitionResult; //目标识别结果 0-10  1-潜艇；2-大型水面舰；3 -其他（包括噪声）
    float recognitionPercent[3]; //目标识别置信度 0-1
    C_ActiveSonarTrackingResult() :
        trackingID(0),
        trackingStep(0),
        trackingLat(0.0),
        trackingLon(0.0),
        recognitionResult(0)
    {
        memset(recognitionPercent, 0, sizeof(recognitionPercent));
    }

};
//主动声纳处理结果输出，主题名；MSG_ActiveSonarResult_Topic
struct CMsg_ActiveSonarResult
{
    int sonarID;			//声呐编号	1~7   1：艏部  2：舷侧左前  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵
    int detectionNumber; //检测目标数量 0~100
    std::vector<C_ActiveSonarDetectionResult> ActiveSonarDetectionResult; //检测结果
    std::vector<C_ActiveSonarTrackingResult>  ActiveSonarTrackingResult;  //跟踪结果
    CMsg_ActiveSonarResult() :detectionNumber(0)
    {
        //ActiveSonarDetectionResult.clear();
        //ActiveSonarTrackingResult.clear();
    }
};

//侦察声纳检测结果
struct C_ScoutingSonarDetectionResult
{
    int signalId; //信号批次 1至100
    float SNR; //信噪比 -50至50
    float detectionDirArray; //方位角估计值检测-阵坐标系 -180至180
    float detectionDirGroud; //方位角估计值大地坐标系 0至360
    int recognitionResult; //侦察识别结果 1至9 1. CW 2. 调频信号 3. 组合脉冲 4. DSSS 5. BPSK 6. QPSK 7. OFDM 8. 2FSK 9. 4FSK
    float freqResult; //中心频率/载频估计结果 0至100000Hz
    float bandRsult; //带宽估计结果 0至50000Hz
    float codeResult; //码率估计结果 0至5000bps
    float pulsewidthResult; //脉宽估计结果 0至20s
    float pulseResult; //脉冲重复周期估计结果 0至50s
    int m_pulseObj;	//脉冲对象类型		0：未知、1：水面舰、2：潜艇、3：直升机、4：浮标、5：鱼雷
    C_ScoutingSonarDetectionResult() :
        signalId(0),
        SNR(0.0),
        detectionDirArray(0.0),
        detectionDirGroud(0.0),
        recognitionResult(0.0),
        freqResult(0.0),
        bandRsult(0.0),
        codeResult(0.0),
        pulsewidthResult(0.0),
        pulseResult(0.0),
        m_pulseObj(0)
    {}
};

//侦察声纳处理结果输出，主题名；MSG_ScoutingSonarResult_Topic
struct CMsg_ScoutingSonarResult
{
    int sonarID;			//声呐编号	1~7   1：艏部  2：舷侧左前  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵
    int signalNumber; //信号数量 0~100
    std::vector<C_ScoutingSonarDetectionResult> ScoutingSonarDetectionResult; //检测结果
    CMsg_ScoutingSonarResult() :signalNumber(0)
    {
        //ScoutingSonarDetectionResult.clear();
    }
};



//数据融合
//声纳模型被动探测数据融合结果结构体
struct C_passiveSonarFusionResult
{

    int fusionID; //融合批次号
    float fusionDirGroud; //方位角融合结果-大地坐标系
    float fusionDirArray;//舷角
    float range;//距离
    std::vector<int> fusionAssociationResult;//融合来源组件
    int fusionRecognitionResult;//融合识别结果
    float fusionRecognitionPercent[5];//目标识别置信度

    C_passiveSonarFusionResult() :fusionID(0),
        fusionDirGroud(0.0),
        fusionDirArray(0.0),
        range(0.0),
        fusionRecognitionResult(0),
        fusionRecognitionPercent{ -1.0,-1.0,-1.0,-1.0,-1.0 }
    {
        fusionAssociationResult.clear();
    }
};


//声纳模型被动探测数据融合单平台多阵结果输出 topic:MSG_PassiveCombineModelResult
struct CMsg_PassiveCombineModelResult {

    int fusionNumber;//融合目标数量
    std::vector<C_passiveSonarFusionResult> fusionResult; //融合结果

    CMsg_PassiveCombineModelResult() :fusionNumber(0)
    {
        fusionResult.clear();
    }

};


//导调声纳数据融合模型，主题名；MSG_DataCombineControlPara
struct CMsg_DataCombineControlPara {
    int sonarKeying;	//多阵融合开关	  0 - 关1 - 开
    CMsg_DataCombineControlPara() :
        sonarKeying(1)
    {}
};

//声纳模型侦察融合结果子结构
struct C_ScoutingSonarCombinationResult
{
    int signalId; //信号批次 1至100
    float SNR; //信噪比 -50至50
    float detectionDirArray; //方位角估计值检测-阵坐标系 -180至180
    float detectionDirGroud; //方位角估计值大地坐标系 0至360
    int recognitionResult; //侦察识别结果 1至9 1. CW 2. 调频信号 3. 组合脉冲 4. DSSS 5. BPSK 6. QPSK 7. OFDM 8. 2FSK 9. 4FSK
    float freqResult; //中心频率/载频估计结果 0至100000Hz
    float bandRsult; //带宽估计结果 0至50000Hz
    float codeResult; //码率估计结果 0至5000bps
    float pulsewidthResult; //脉宽估计结果 0至20s
    float pulseResult; //脉冲重复周期估计结果 0至50s
    int m_pulseObj;	//脉冲对象类型		0：未知、1：水面舰、2：潜艇、3：直升机、4：浮标、5：鱼雷
    std::vector<int> fusionAssociationResult;//融合来源组件
    C_ScoutingSonarCombinationResult() :
        signalId(0),
        SNR(0.0),
        detectionDirArray(0.0),
        detectionDirGroud(0.0),
        recognitionResult(0.0),
        freqResult(0.0),
        bandRsult(0.0),
        codeResult(0.0),
        pulsewidthResult(0.0),
        pulseResult(0.0),
        m_pulseObj(0)
    {
        fusionAssociationResult.clear();
    }
};
//声纳模型侦察融合结果输出  topic:MSG_ScoutingCombineModelResult
struct CMsg_ScoutingSonarCombineResult
{
    int fusionNumber;
    std::vector<C_ScoutingSonarCombinationResult> scoutingFusionResult;
    CMsg_ScoutingSonarCombineResult() :
        fusionNumber(0)
    {
        scoutingFusionResult.clear();
    }
};
//声纳模型鱼雷预警融合结果子结构
struct C_TorpedoCombineModelResult
{
    int TrackingID;		//跟踪批次号		继承跟踪批次号
    int warningLevel; // 鱼雷威胁等级 1无威胁（非鱼雷）、2低威胁、3中威胁、4高威胁
    float warningDirArray; //方位角估计值跟踪-阵坐标系-180至180
    float warningDirGroud; //方位角估计值跟踪-大地坐标系0至360
    float warningDistance; //敌方鱼雷距离0-100km
    long long PulseTime;		//瞬态噪声的到达时刻
    float PulseDirArray;	//瞬态噪声方位角
    float PulseDirGround;	//瞬态噪声舷角
    int warningRecognitionResult; //鱼雷识别类型 1~3类 1-潜射	2-空投 3-舰射管装鱼雷
    float warningRecognitionPercent[4];	//鱼雷识别置信度

    C_TorpedoCombineModelResult() :
        TrackingID(0),
        warningLevel(0),
        warningDirArray(0.0),
        warningDirGroud(0.0),
        warningDistance(0.0),
        PulseTime(0),
        PulseDirArray(0.0),
        PulseDirGround(0.0),
        warningRecognitionResult(0),
        warningRecognitionPercent{ 0.0, 0.0, 0.0, 0.0 }
    {}

};


//声纳模型鱼雷预警融合结果输出 topic: MSG_TorpedoCombineModelResult
struct CMsg_TorpedoCombineModelResult
{
    int fusionNumber;
    std::vector<C_TorpedoCombineModelResult> TorpedoFusionResult;
    CMsg_TorpedoCombineModelResult() :
        fusionNumber(0)
    {
        TorpedoFusionResult.clear();
    }
};



//拖曳阵机动数据输入数据定义;
struct CData_PullMotion
{
    double lon;    //经纬;
    double lat;    //维度;
    double alt;    //深度;
    std::vector<std::vector<double>> courseSensorDataVec;   //航向传感器数据;
    std::vector<int> headingSensorIndexStereotype;      //航向传感器输出当前向对应第几个阵元数
    std::vector<double> positionDeepVec;               //阵深度数据;
    std::vector<positionXYZ> arrayLocation;             //阵元位置;
    CData_PullMotion() :lon(0.0),
        lat(0.0), alt(0.0)
    {
        courseSensorDataVec.clear();
        headingSensorIndexStereotype.clear();
        positionDeepVec.clear();
        arrayLocation.clear();
    }
};


//同步态势输出主题名：MSG_SyncSituation
struct CMsg_SyncSituation
{
    int sonarID;  //声纳编号
    int multiStaticGroupNo; //多基地组号
    long long multiSendTime; //信号发射时刻
    CMsg_ActivePulseSoundSpectrumStruct singalSendOutput; //发射输出
    CData_st_navmodel_navinfo navmodel_navinfo;//信号发射时刻对应导航信息
    float rangeDetection;//探测量程
    int methodDetection;//探测方式
    int numberDetection;//探测次数 //依据建模工具中配置的参数项进行配置

    int filterDigitalLevel;
    CMsg_SyncSituation() :sonarID(0),
        multiStaticGroupNo(0),
        multiSendTime(0),
        rangeDetection(0.0),
        methodDetection(0),
        numberDetection(0),
        filterDigitalLevel(0) {}
};
//声呐任务状态输出 主题名：Msg_SonarTaskState
struct CMsg_SonarTaskState
{
    int sonarID;			//声呐编号	1~7   1：艏部  2：舷侧左前  3：舷侧左后		4：舷侧右前    5：舷侧右后    6：拖拽（通过缆船舶）		7：宽频带接收阵   8：吊声收
    int workMode;			//1：主动；2：多基地
    bool isFinish; 			// 0:正在执行；1：完成
    CMsg_SonarTaskState() :
        workMode(1),
        isFinish(0)
    {}
};


/*****************************导调类主题-探测能力分析*********************************/
//引擎发送探测能力计算输入 主题名称：MSG_Direct_DetectAnalysisInput_Topic

struct CMSG_Direct_DetectAnalysisInput
{
    CMSG_Direct_DetectAnalysisInput() : MissionID(0), SourcePlatformID(0), DesPlatformID(0), SonarID(0), WorkState(0), IsWork(0), MaxSL(0), AngleNum(0), WorkFreq(0){ }
    int MissionID;//任务编号
    bool IsWork;//1-开始计算 0-停止计算
    int SourcePlatformID;//探测平台ID
    int DesPlatformID;//目标平台ID
    int SonarID;//探测声呐ID
    int WorkState;//工作模式 0-被动 1-主动
    float WorkFreq;//工作频率
    int   AngleNum;//声场计算方位数
    float MaxSL;//最大声源级

};

//声呐模型探测能力计算结果输出 主题名称：Data_Direct_DetectAnalysisOutput_Sonar_Topic

struct CData_Direct_DetectAnalysisOutput_Sonar
{
    CData_Direct_DetectAnalysisOutput_Sonar() : MissionID(0), SourcePlatformID(0), SonarID(0), WorkState(0), DIMinusDT{ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } { }
    int MissionID;//任务编号
    int SourcePlatformID;//探测平台ID
    int SonarID;//探测声呐ID
    int WorkState;//工作模式 0-被动 1-主动
    std::vector<float> DIMinusDT;//DI-DT值
};

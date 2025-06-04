#pragma once

#include "SimBasicTypes.h"
#include "CSimTypes.h"

#include <string>


/**
* 实体组件属性
*/
class CSimComponentAttribute {
public:
    /**
    *组件 id
    */
    int64 id;

    /**
    * 名称
    */
    std::string name;


    /**
    * 组件属性数据
    */
    void* data;

    /**
    * 动态属性数据
    */
    void* dynamicData;

    /**
     * 数据的格式
     */
    CDataFormat dataFormat;



    CSimComponentAttribute()
        : data(nullptr)
        , dynamicData(nullptr)
    {

    }

    CSimComponentAttribute(char* paramAttribute)
        : data(paramAttribute)
        , dynamicData(nullptr)
        , dataFormat(CDataFormat::STRUCT)
    {

    }

    CSimComponentAttribute(CDataFormat paramDataFormat, char* paramAttribute)
        : data(paramAttribute)
        , dynamicData(nullptr)
        , dataFormat(paramDataFormat)
    {

    }

    CSimComponentAttribute(char* paramAttribute, char* paramDynamicAttribute)
        :  data(paramAttribute)
        , dynamicData(paramDynamicAttribute)
        , dataFormat(CDataFormat::STRUCT)
    {

    }

    CSimComponentAttribute(CDataFormat paramDataFormat, char* paramAttribute, char* paramDynamicAttribute)
        : data(paramAttribute)
        , dynamicData(paramDynamicAttribute)
        , dataFormat(paramDataFormat)
    {

    }

    ~CSimComponentAttribute() {
    }
};

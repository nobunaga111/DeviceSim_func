QT += core gui widgets

CONFIG += c++11
CONFIG -= console
CONFIG += app_bundle

# 下面的定义使得编译器在使用任何已标记为已弃用的Qt功能时发出警告
DEFINES += QT_DEPRECATED_WARNINGS

# 取消下面的注释也可以选择仅禁用到某个特定Qt版本的已弃用API
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # 禁用Qt 6.0.0之前的所有已弃用API

INCLUDEPATH += \
    $$PWD/../../../../../../../SDK/SimModel/Cpp/include/ \
    $$PWD/../../src/ \
    $$PWD/src/

CONFIG(debug,debug){
    LIBS += -L$$PWD/../../../../../../../SDK/SimModel/Cpp/bin/winDebug -lSimSdk
    LIBS += -L$$PWD/../../../bin2/winDebug -lDeviceModel
    DESTDIR = $$PWD/../../../bin2/winDebug/
}
else {
    LIBS += -L$$PWD/../../../../../../../SDK/SimModel/Cpp/bin/winRelease -lSimSdk
    LIBS += -L$$PWD/../../../bin2/winRelease -lDeviceModel
    DESTDIR = $$PWD/../../../bin2/winRelease/
}

SOURCES += \
        src/DeviceModelAgent.cpp \
        src/mainWithUi.cpp \
        src/mainwindow.cpp \
        ../../src/common/DMLogger.cpp \
        ../../src/devicemodel.cpp

HEADERS += \
    src/DeviceModelAgent.h \
    src/mainwindow.h \
    ../../src/common/DMLogger.h \
    ../../src/devicemodel.h

FORMS += src/mainwindow.ui  # 声明 UI 文件

# 默认部署规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

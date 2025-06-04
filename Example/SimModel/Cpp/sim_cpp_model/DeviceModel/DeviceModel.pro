TEMPLATE = lib
DEFINES += DEVICEMODEL_LIBRARY
CONFIG += c++11
QT += core

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
    $$PWD/../../../../../SDK/SimModel/Cpp/include/

CONFIG(debug,debug|release){
    LIBS += -L$$PWD/../../../../../SDK/SimModel/Cpp/bin/winDebug -lSimSdk
    DESTDIR = $$PWD/../bin2/winDebug/
}
else {
    LIBS += -L$$PWD/../../../../../SDK/SimModel/Cpp/bin/winRelease -lSimSdk
    DESTDIR = $$PWD/../bin2/winRelease/
}

SOURCES += \
    src/CreateDeviceModel.cpp \
    src/devicemodel.cpp \
    src/common/DMLogger.cpp

HEADERS += \
    src/CreateDeviceModel.h \
    src/DeviceTestInOut.h \
    src/common/define.h \
    src/devicemodel.h \
    src/common/DMLogger.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

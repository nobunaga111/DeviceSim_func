TEMPLATE = subdirs

SUBDIRS += \
    DeviceModel \
    #DeviceModel/test/DeviceTestUnit \
    DeviceModel/test/DeviceUiTestUnit

CONFIG += qt

QT += widgets

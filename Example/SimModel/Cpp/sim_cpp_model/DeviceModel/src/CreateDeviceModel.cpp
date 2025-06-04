#include "CreateDeviceModel.h"
#include "devicemodel.h"

CSimComponentBase *createComponent(std::string componentName)
{
    (void)componentName;
    return new DeviceModel();
}

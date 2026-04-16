#include "Mqtt/MqttFaultPolicy.h"

FaultCode faultCodeForMqttConnectState(int state)
{
    switch (state)
    {
    case 5:
        return FaultCode::MqttAuthFailure;
    default:
        return FaultCode::None;
    }
}

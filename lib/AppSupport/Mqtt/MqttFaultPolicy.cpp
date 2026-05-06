// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

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

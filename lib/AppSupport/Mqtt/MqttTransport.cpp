// SPDX-License-Identifier: GPL-3.0-or-later
#include "Mqtt/MqttTransport.h"
#include "Mqtt/MqttWrite.h"
#include "Runtime/StallRecorder.h"
#include <lwip/sockets.h>

size_t MqttTransport::write(const uint8_t *data, size_t size)
{
    const int socket = fd();
    if (!_connected || socket < 0 || size == 0)
        return 0;
    StallRecorder::ScopedStage stage(StallRecorder::Stage::MqttWrite);
    const uint32_t started = millis();
    const int error = MqttWrite::sendPacket(data, size,
        [socket](const uint8_t *bytes, size_t length) {
            return ::send(socket, bytes, length, MSG_DONTWAIT);
        }, millis, [] { delay(1); });
    if (!error)
        return size;

    const uint32_t elapsed = millis() - started;
    StallRecorder::mqttWriteFailed(elapsed, error);
    // A partial frame cannot be retried on this stream. lwIP also uses
    // SO_SNDTIMEO when close needs to wait for TCP send-queue space.
    const timeval timeout{0, MqttWrite::deadlineMs * 1000};
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    stop();
    failure_.at = millis();
    failure_.elapsedMs = elapsed;
    failure_.error = error;
    return 0;
}

uint8_t MqttTransport::connected()
{
    StallRecorder::ScopedStage stage(StallRecorder::Stage::MqttStatus);
    return WiFiClient::connected();
}

void MqttTransport::stop()
{
    StallRecorder::ScopedStage stage(StallRecorder::Stage::MqttClose);
    WiFiClient::stop();
}

bool MqttTransport::takeWriteFailure(WriteFailure &failure)
{
    if (!failure_.error)
        return false;
    failure = failure_;
    failure_.error = 0;
    return true;
}

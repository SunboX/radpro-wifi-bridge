// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/BridgeDnsResolver.h"
#include "Network/DnsRequest.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

namespace
{
constexpr uint32_t kWaitMs = 15000;
BridgeDns::Request request;
QueueHandle_t queue = nullptr;
SemaphoreHandle_t callers = nullptr;
std::atomic<bool> ready{false};
std::atomic<uint32_t> lookups{0}, successes{0}, failures{0}, timeouts{0}, busy{0}, late{0};

void worker(void *)
{
    uint8_t token;
    for (;;)
    {
        if (xQueueReceive(queue, &token, portMAX_DELAY) != pdTRUE || !request.workerStart())
            continue;
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo *result = nullptr;
        // The socket API dispatches DNS work to tcpip_thread and keeps its
        // callback storage alive until DNS completes, including all retries.
        const int error = lwip_getaddrinfo(request.name(), nullptr, &hints, &result);
        const bool ok = error == 0 && result && result->ai_addr && result->ai_family == AF_INET;
        uint32_t address = ok ? reinterpret_cast<sockaddr_in *>(result->ai_addr)->sin_addr.s_addr : 0;
        if (result)
            lwip_freeaddrinfo(result);
        if (!request.workerFinish(ok && address != 0, address))
            ++late;
    }
}
}

bool BridgeDnsResolver::begin()
{
    if (ready.load())
        return true;
    queue = xQueueCreate(1, sizeof(uint8_t));
    callers = xSemaphoreCreateMutex();
    if (!queue || !callers || xTaskCreate(worker, "bridge-dns", 4096, nullptr, 1, nullptr) != pdPASS)
    {
        if (queue) vQueueDelete(queue);
        if (callers) vSemaphoreDelete(callers);
        queue = nullptr;
        callers = nullptr;
        return false;
    }
    ready.store(true);
    return true;
}

BridgeDnsResolver::Snapshot BridgeDnsResolver::snapshot()
{
    return {ready.load(), !request.idle(), lookups.load(), successes.load(), failures.load(),
            timeouts.load(), busy.load(), late.load()};
}

extern "C" int radpro_resolve_hostname(const char *hostname, uint32_t *address)
{
    if (!address)
        return 0;
    *address = 0;
    if (!ready.load() || xSemaphoreTake(callers, pdMS_TO_TICKS(kWaitMs)) != pdTRUE)
        return 0;
    if (!request.start(hostname))
    {
        ++busy;
        xSemaphoreGive(callers);
        return 0;
    }
    ++lookups;
    const uint8_t token = 1;
    if (xQueueSend(queue, &token, 0) != pdTRUE)
    {
        request.abandon();
        // No token exists: this request has not been handed to the worker.
        request.workerStart();
        ++failures;
        xSemaphoreGive(callers);
        return 0;
    }
    const TickType_t startedAt = xTaskGetTickCount();
    while (!request.complete() && xTaskGetTickCount() - startedAt < pdMS_TO_TICKS(kWaitMs))
        vTaskDelay(pdMS_TO_TICKS(10));
    bool ok = false;
    if (request.complete())
    {
        ok = request.take(*address);
        if (ok) ++successes;
        else ++failures;
    }
    else
    {
        request.abandon();
        ++timeouts;
    }
    xSemaphoreGive(callers);
    return ok ? 1 : 0;
}

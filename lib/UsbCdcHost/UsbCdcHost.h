#pragma once
#include <Arduino.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"

extern "C"
{
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_types_stack.h" // usb_device_desc_t
}

class UsbCdcHost
{
public:
    using DeviceCb = void (*)();
    using LineCb = void (*)(const String &line);
    using RawCb = void (*)(const uint8_t *data, size_t len);

    UsbCdcHost();
    ~UsbCdcHost();

    // Lifecycle
    bool begin();
    void stop();

    // Safe from any task: enqueues TX
    bool sendCommand(const String &cmd, bool append_crlf = true, uint32_t timeout_ms = 1000);
    bool sendLine(const String &line, uint32_t timeout_ms = 1000);
    bool send(const uint8_t *data, size_t len, uint32_t timeout_ms = 1000);

    // Line coding (ignored if device doesn’t support it)
    bool setBaud(uint32_t baud);

    // App callbacks
    void setDeviceCallbacks(DeviceCb on_connected, DeviceCb on_disconnected);
    void setLineCallback(LineCb on_line);
    void setRawCallback(RawCb cb);

    // Optional: restrict to a specific VID/PID (0 = ANY)
    void setVidPidFilter(uint16_t vid, uint16_t pid);

    // Status
    bool isConnected() const { return dev_ != nullptr || vcp_dev_ != nullptr; }

private:
    // Tasks
    static void UsbLibTaskThunk(void *arg);
    static void CdcTaskThunk(void *arg);
    static void TxTaskThunk(void *arg);

    void usbLibTask();
    void cdcTask();
    void txTask();

    // CDC callbacks
    static bool DataCb(const uint8_t *data, size_t len, void *user_arg);
    static void DevEventCb(const cdc_acm_host_dev_event_data_t *event, void *user_arg);

    bool onRx(const uint8_t *data, size_t len);
    void onDevEvent(const cdc_acm_host_dev_event_data_t *event);

    // Helpers
    esp_err_t configureLineCoding(uint32_t baud);
    bool enqueueRaw(const uint8_t *data, size_t len, uint32_t timeout_ms);

    static constexpr const char *TAG = "UsbCdcHost";

    TaskHandle_t lib_task_ = nullptr;
    TaskHandle_t cdc_task_ = nullptr;
    TaskHandle_t tx_task_ = nullptr;

    cdc_acm_dev_hdl_t dev_ = nullptr;

    uint32_t target_baud_ = 115200;
    volatile bool running_ = false;
    uint8_t opened_intf_idx_ = 0xFF; // which CDC interface we opened (for logs)

    // Callbacks
    DeviceCb on_connected_ = nullptr;
    DeviceCb on_disconnected_ = nullptr;
    LineCb on_line_ = nullptr;
    RawCb on_raw_ = nullptr;

    // RX line buffer
    String line_buf_;
    volatile TickType_t last_rx_tick_ = 0;

    // TX queue + mutex
    struct TxItem
    {
        char *data;
        size_t len;
        uint32_t timeout_ms;
    };
    QueueHandle_t tx_q_ = nullptr;
    SemaphoreHandle_t tx_mutex_ = nullptr;

    // Optional VID/PID filter (0 => ANY)
    uint16_t allow_vid_ = 0;
    uint16_t allow_pid_ = 0;

    // Post-connect settle time for first TX
    volatile TickType_t ready_after_tick_ = 0;

    // VCP wrapper state (when using the VCP service for CH34x/CP210x/FTDI)
    bool use_vcp_ = false;
    CdcAcmDevice *vcp_dev_ = nullptr; // returned by VCP::open(); owned by the VCP service

    // Descriptor logging client
    usb_host_client_handle_t dbg_client_ = nullptr;
    TaskHandle_t dbg_task_ = nullptr;
    static void DbgClientTaskThunk(void *arg);
    static void DbgClientCb(const usb_host_client_event_msg_t *evt, void *arg);
    void dbgClientTask();
};

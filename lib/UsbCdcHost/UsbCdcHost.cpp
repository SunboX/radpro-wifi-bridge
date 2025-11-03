#include "UsbCdcHost.h"
#include <cstring>
#include <inttypes.h>
#include <new>

namespace
{
    constexpr uint32_t kConnectionTimeoutMs = 1000;
    constexpr size_t kOutBufferSize = 512;
    constexpr size_t kInBufferSize = 512;
    constexpr uint8_t kDefaultInterfaceIndex = 0;
}

UsbCdcHost::UsbCdcHost() {}
UsbCdcHost::~UsbCdcHost() { stop(); }

void UsbCdcHost::setDeviceCallbacks(DeviceCb on_connected, DeviceCb on_disconnected)
{
    on_connected_ = on_connected;
    on_disconnected_ = on_disconnected;
}

void UsbCdcHost::setLineCallback(LineCb on_line) { on_line_ = on_line; }

void UsbCdcHost::setVidPidFilter(uint16_t vid, uint16_t pid)
{
    allow_vid_ = vid;
    allow_pid_ = pid;
}

bool UsbCdcHost::begin()
{
    if (running_)
        return true;
    running_ = true;

    tx_q_ = xQueueCreate(16, sizeof(TxItem *));
    tx_mutex_ = xSemaphoreCreateMutex();
    if (!tx_q_ || !tx_mutex_)
    {
        ESP_LOGE(TAG, "Failed to create TX queue/mutex");
        stop();
        return false;
    }

    const usb_host_config_t host_cfg = {.skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1};
    esp_err_t err = usb_host_install(&host_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "usb_host_install failed: %s", esp_err_to_name(err));
        stop();
        return false;
    }
    host_installed_ = (err == ESP_OK);

    const cdc_acm_host_driver_config_t acm_cfg = {
        .driver_task_stack_size = 4096,
        .driver_task_priority = 20,
        .xCoreID = tskNO_AFFINITY,
        .new_dev_cb = nullptr,
    };
    err = cdc_acm_host_install(&acm_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "cdc_acm_host_install failed: %s", esp_err_to_name(err));
        stop();
        return false;
    }
    acm_installed_ = (err == ESP_OK);

    usb_host_client_config_t cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 16,
        .async = {.client_event_callback = &UsbCdcHost::DbgClientCb, .callback_arg = this}};
    if (usb_host_client_register(&cfg, &dbg_client_) == ESP_OK)
    {
        xTaskCreatePinnedToCore(DbgClientTaskThunk, "usb_dbg", 4096, this, 19, &dbg_task_, tskNO_AFFINITY);
    }

    if (xTaskCreatePinnedToCore(UsbLibTaskThunk, "usb_lib", 4096, this, 20, &lib_task_, tskNO_AFFINITY) != pdPASS ||
        xTaskCreatePinnedToCore(CdcTaskThunk, "cdc", 6144, this, 21, &cdc_task_, tskNO_AFFINITY) != pdPASS ||
        xTaskCreatePinnedToCore(TxTaskThunk, "cdc_tx", 4096, this, 22, &tx_task_, tskNO_AFFINITY) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create one or more tasks");
        stop();
        return false;
    }

    ESP_LOGI(TAG, "USB host + TX queue started");
    return true;
}

void UsbCdcHost::stop()
{
    if (!running_)
        return;
    running_ = false;

    if (tx_task_)
    {
        vTaskDelete(tx_task_);
        tx_task_ = nullptr;
    }
    if (cdc_task_)
    {
        vTaskDelete(cdc_task_);
        cdc_task_ = nullptr;
    }
    if (lib_task_)
    {
        vTaskDelete(lib_task_);
        lib_task_ = nullptr;
    }

    closeDevice();
    if (tx_q_)
    {
        TxItem *it = nullptr;
        while (xQueueReceive(tx_q_, &it, 0) == pdTRUE)
        {
            delete it;
        }
        vQueueDelete(tx_q_);
        tx_q_ = nullptr;
    }
    if (tx_mutex_)
    {
        vSemaphoreDelete(tx_mutex_);
        tx_mutex_ = nullptr;
    }

    if (acm_installed_)
    {
        cdc_acm_host_uninstall();
        acm_installed_ = false;
    }
    if (host_installed_)
    {
        usb_host_uninstall();
        host_installed_ = false;
    }

    if (dbg_task_)
    {
        vTaskDelete(dbg_task_);
        dbg_task_ = nullptr;
    }
    if (dbg_client_)
    {
        usb_host_client_deregister(dbg_client_);
        dbg_client_ = nullptr;
    }
}

bool UsbCdcHost::enqueueRaw(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!data || !len || !tx_q_ || !tx_mutex_)
        return false;

    auto *item = new (std::nothrow) TxItem();
    if (!item)
        return false;
    item->data.reset(new (std::nothrow) uint8_t[len]);
    if (!item->data)
    {
        delete item;
        return false;
    }
    memcpy(item->data.get(), data, len);
    item->len = len;
    item->timeout_ms = timeout_ms;

    bool queued = false;
    if (xSemaphoreTake(tx_mutex_, pdMS_TO_TICKS(200)) == pdTRUE)
    {
        queued = (xQueueSend(tx_q_, &item, pdMS_TO_TICKS(50)) == pdTRUE);
        xSemaphoreGive(tx_mutex_);
    }

    if (!queued)
    {
        ESP_LOGW(TAG, "enqueueRaw: queue full/busy; dropping");
        delete item;
    }

    return queued;
}

bool UsbCdcHost::send(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    return enqueueRaw(data, len, timeout_ms);
}

bool UsbCdcHost::sendLine(const String &line, uint32_t timeout_ms)
{
    return send(reinterpret_cast<const uint8_t *>(line.c_str()), line.length(), timeout_ms);
}

bool UsbCdcHost::sendCommand(const String &cmd, bool append_crlf, uint32_t timeout_ms)
{
    String out = cmd;
    if (append_crlf)
        out += "\r\n";
    ESP_LOGI(TAG, "Queue cmd: %s", out.c_str());
    return send(reinterpret_cast<const uint8_t *>(out.c_str()), out.length(), timeout_ms);
}

void UsbCdcHost::closeDevice()
{
    if (dev_)
    {
        cdc_acm_host_close(dev_);
        dev_ = nullptr;
    }
    ready_after_tick_ = 0;
}

bool UsbCdcHost::setBaud(uint32_t baud)
{
    target_baud_ = baud;
    if (dev_)
        return (configureLineCoding(baud) == ESP_OK);
    return true;
}

esp_err_t UsbCdcHost::configureLineCoding(uint32_t baud)
{
    if (!dev_)
        return ESP_ERR_INVALID_STATE;

    cdc_acm_line_coding_t lc = {};
    lc.dwDTERate = baud; // use the function argument
    lc.bDataBits = 8;
    lc.bParityType = 0; // none
    lc.bCharFormat = 0; // 1 stop bit

    esp_err_t err = cdc_acm_host_line_coding_set(dev_, &lc);
    if (err == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "line_coding_set not supported; continuing");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "line_coding_set failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Line set to %u 8N1", (unsigned)baud);
    return ESP_OK;
}

// ---------- tasks ----------
void UsbCdcHost::UsbLibTaskThunk(void *arg) { static_cast<UsbCdcHost *>(arg)->usbLibTask(); }

void UsbCdcHost::CdcTaskThunk(void *arg) { static_cast<UsbCdcHost *>(arg)->cdcTask(); }

void UsbCdcHost::TxTaskThunk(void *arg) { static_cast<UsbCdcHost *>(arg)->txTask(); }

void UsbCdcHost::usbLibTask()
{
    while (running_)
    {
        uint32_t flags = 0;
        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "usb_host_lib_handle_events: %s", esp_err_to_name(err));
        }
        if (flags)
        {
            ESP_LOGI(TAG, "usb_host_lib flags=0x%08" PRIx32, flags);
        }
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            usb_host_device_free_all();
    }
    vTaskDelete(nullptr);
}

void UsbCdcHost::cdcTask()
{
    ESP_LOGI(TAG, "CDC task started");

    // --- Common device config & callback thunk
    cdc_acm_host_device_config_t dev_cfg{};
    dev_cfg.connection_timeout_ms = kConnectionTimeoutMs;
    dev_cfg.out_buffer_size = kOutBufferSize;
    dev_cfg.in_buffer_size = kInBufferSize;
    dev_cfg.event_cb = &UsbCdcHost::DevEventCb;
    dev_cfg.data_cb = &UsbCdcHost::DataCb;
    dev_cfg.user_arg = this;

    // housekeeping in case we were restarted
    if (dev_)
    {
        cdc_acm_host_close(dev_);
        dev_ = nullptr;
    }
    while (running_)
    {
        if (!dev_)
        {
            const uint16_t open_vid = allow_vid_ ? allow_vid_ : CDC_HOST_ANY_VID;
            const uint16_t open_pid = allow_pid_ ? allow_pid_ : CDC_HOST_ANY_PID;
            opened_intf_idx_ = kDefaultInterfaceIndex;

            ESP_LOGI(TAG, "Attempting to open CDC device (VID=0x%04X, PID=0x%04X, iface=%u)",
                     open_vid, open_pid, opened_intf_idx_);

            const esp_err_t err = cdc_acm_host_open(open_vid, open_pid, opened_intf_idx_, &dev_cfg, &dev_);
            if (err == ESP_OK && dev_ != nullptr)
            {
                ESP_LOGI(TAG, "CDC device opened (iface=%u, %04X:%04X)",
                         opened_intf_idx_, open_vid, open_pid);
                const esp_err_t lerr = cdc_acm_host_set_control_line_state(dev_, true, true);
                if (lerr != ESP_OK && lerr != ESP_ERR_NOT_SUPPORTED)
                {
                    ESP_LOGW(TAG, "set_control_line_state: %s", esp_err_to_name(lerr));
                }
                (void)configureLineCoding(target_baud_);
                ready_after_tick_ = xTaskGetTickCount() + pdMS_TO_TICKS(80);
                if (on_connected_)
                    on_connected_();
                continue;
            }

            ESP_LOGW(TAG, "cdc_acm_host_open failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(25));
    }

    closeDevice();

    ESP_LOGI(TAG, "CDC task stopped");
}

void UsbCdcHost::txTask()
{
    ESP_LOGI(TAG, "TX task started");
    while (running_)
    {
        TxItem *it = nullptr;
        if (xQueueReceive(tx_q_, &it, pdMS_TO_TICKS(200)) != pdTRUE || !it)
            continue;

        // Wait for device
        while (running_ && dev_ == nullptr)
            vTaskDelay(pdMS_TO_TICKS(50));
        if (!running_)
        {
            delete it;
            break;
        }

        // Honor settle delay after (re)connect
        TickType_t now = xTaskGetTickCount();
        if (ready_after_tick_ && now < ready_after_tick_)
        {
            vTaskDelay(ready_after_tick_ - now);
            now = xTaskGetTickCount();
        }

        // Idle flush of unterminated lines after ~100ms of inactivity
        if (!line_buf_.isEmpty() && (now - last_rx_tick_) > pdMS_TO_TICKS(100))
        {
            if (on_line_)
                on_line_(line_buf_);
            line_buf_.clear();
        }

        ESP_LOGI(TAG, "TX %u bytes (intf=%u)", static_cast<unsigned>(it->len), static_cast<unsigned>(opened_intf_idx_));
        esp_err_t err = ESP_FAIL;
        if (dev_)
        {
            err = cdc_acm_host_data_tx_blocking(dev_, it->data.get(), it->len, it->timeout_ms);
        }
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "TX failed: %s", esp_err_to_name(err));
            if (running_ && xQueueSendToFront(tx_q_, &it, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                continue;
            }
        }

        delete it;
    }
    ESP_LOGI(TAG, "TX task exit");
    vTaskDelete(nullptr);
}

// ---------- callbacks ----------
bool UsbCdcHost::DataCb(const uint8_t *data, size_t len, void *user_arg)
{
    auto *self = static_cast<UsbCdcHost *>(user_arg);
    return self ? self->onRx(data, len) : true;
}

void UsbCdcHost::DevEventCb(const cdc_acm_host_dev_event_data_t *event, void *user_arg)
{
    auto *self = static_cast<UsbCdcHost *>(user_arg);
    if (self)
        self->onDevEvent(event);
}

bool UsbCdcHost::onRx(const uint8_t *data, size_t len)
{
    if (on_raw_)
        on_raw_(data, len);              // raw bytes
    last_rx_tick_ = xTaskGetTickCount(); // remember last RX

    for (size_t i = 0; i < len; ++i)
    {
        char c = (char)data[i];
        if (c == '\r' || c == '\n')
        {
            if (!line_buf_.isEmpty())
            {
                if (on_line_)
                    on_line_(line_buf_);
                line_buf_.clear();
            }
        }
        else
        {
            line_buf_ += c;
            if (line_buf_.length() > 512)
            {
                if (on_line_)
                    on_line_(line_buf_ + " …(truncated)");
                line_buf_.clear();
            }
        }
    }
    return true;
}

void UsbCdcHost::onDevEvent(const cdc_acm_host_dev_event_data_t *event)
{
    switch (event->type)
    {
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "CDC device disconnected");
        if (event->data.cdc_hdl)
            cdc_acm_host_close(event->data.cdc_hdl);
        dev_ = nullptr;
        ready_after_tick_ = 0;
        if (!line_buf_.isEmpty() && on_line_)
        {
            on_line_(line_buf_);
            line_buf_.clear();
        }
        if (on_disconnected_)
            on_disconnected_();
        break;

    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM driver error: %d", event->data.error);
        break;

    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "Serial state: 0x%04X", event->data.serial_state.val);
        break;

    default:
        ESP_LOGW(TAG, "Unhandled CDC event: %d", event->type);
        break;
    }
}

void UsbCdcHost::setRawCallback(RawCb cb) { on_raw_ = cb; }

void UsbCdcHost::DbgClientTaskThunk(void *arg) { static_cast<UsbCdcHost *>(arg)->dbgClientTask(); }

void UsbCdcHost::dbgClientTask()
{
    ESP_LOGI(TAG, "DBG client task started");
    while (running_ && dbg_client_)
    {
        esp_err_t err = usb_host_client_handle_events(dbg_client_, pdMS_TO_TICKS(1000));
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT)
            ESP_LOGW(TAG, "dbg handle_events: %s", esp_err_to_name(err));
    }
    vTaskDelete(nullptr);
}

void UsbCdcHost::DbgClientCb(const usb_host_client_event_msg_t *evt, void *arg)
{
    auto *self = static_cast<UsbCdcHost *>(arg);
    if (!self || !evt)
        return;

    switch (evt->event)
    {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
    {
        usb_device_handle_t devH = nullptr;
        if (usb_host_device_open(self->dbg_client_, evt->new_dev.address, &devH) == ESP_OK)
        {
            const usb_device_desc_t *dd = nullptr;
            if (usb_host_get_device_descriptor(devH, &dd) == ESP_OK && dd)
            {
                ESP_LOGI(TAG, "NEW DEV addr=%u VID=0x%04X PID=0x%04X class=0x%02X",
                         evt->new_dev.address, dd->idVendor, dd->idProduct, dd->bDeviceClass);
            }
            const usb_config_desc_t *cfg = nullptr;
            if (usb_host_get_active_config_descriptor(devH, &cfg) == ESP_OK && cfg)
            {
                const uint8_t *p = (const uint8_t *)cfg;
                const uint8_t *end = p + cfg->wTotalLength;
                while (p + 2 <= end)
                {
                    uint8_t len = p[0], type = p[1];
                    if (!len || p + len > end)
                        break;
                    if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE && len >= sizeof(usb_intf_desc_t))
                    {
                        auto *ifd = (const usb_intf_desc_t *)p;
                        ESP_LOGI(TAG, "  IF#%u class=0x%02X sub=0x%02X proto=0x%02X",
                                 ifd->bInterfaceNumber, ifd->bInterfaceClass, ifd->bInterfaceSubClass, ifd->bInterfaceProtocol);
                    }
                    p += len;
                }
            }
            usb_host_device_close(self->dbg_client_, devH);
        }
        break;
    }

    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        // IDF 4.4.x: no dev_addr field here; keep it simple and portable
        ESP_LOGI(TAG, "DEV GONE");
        break;

    default:
        break;
    }
}

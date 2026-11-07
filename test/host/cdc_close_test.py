# SPDX-FileCopyrightText: 2026 André Fiedler
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the production CDC close path with delayed and failed USB completions."""
import argparse
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cxx', default='c++')
parser.add_argument('--sdk')
args = parser.parse_args()
source = (root / 'components/usb_host_cdc_acm/cdc_acm_host.c').read_text()

def function(signature, text=source):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

harness = r'''
#include <cassert>
#include <atomic>
#include <cstdint>
#include <cstdio>
using esp_err_t = int;
using TickType_t = uint32_t;
using usb_device_handle_t = int;
constexpr int ESP_OK=0, ESP_ERR_INVALID_STATE=1, ESP_ERR_NOT_FOUND=2, ESP_ERR_INVALID_ARG=3, ESP_ERR_TIMEOUT=4, ESP_FAIL=5;
constexpr int pdTRUE=1, portMAX_DELAY=-1;
#define pdMS_TO_TICKS(x) (x)
#define CDC_ACM_CHECK(x,e) do { if (!(x)) return e; } while (0)
#define CDC_ACM_ENTER_CRITICAL() ((void)0)
#define CDC_ACM_EXIT_CRITICAL() ((void)0)
#define ESP_RETURN_ON_ERROR(call,...) do { int err=(call); if(err) return err; } while(0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
struct usb_transfer_t { int bEndpointAddress; };
struct Interface { int bInterfaceNumber; };
struct Device {
    bool closing=false;
    int dev_hdl=1;
    struct { usb_transfer_t *in_xfer; Interface *intf_desc; void *in_cb; } data;
    struct { usb_transfer_t *xfer; Interface *intf_desc; void *cb; } notif;
};
using cdc_dev_t=Device;
using cdc_acm_dev_hdl_t=Device *;
struct List { Device *head; };
struct Driver { int open_close_mutex=1, cdc_acm_client_hdl=1; List cdc_devices_list; };
static Driver driver, *p_cdc_acm_obj=&driver;
static int now, removed, halts, flushes, clears, mode, locked;
static bool dataReleased, notifReleased;
#define SLIST_FOREACH(v,l,f) for ((v)=(l)->head;(v);(v)=nullptr)
#define SLIST_REMOVE(l,v,t,f) do { assert((l)->head==(v)); (l)->head=nullptr; } while(0)
int xSemaphoreTake(int,int) { assert(!locked); locked=1; return pdTRUE; }
void xSemaphoreGive(int) { assert(locked); locked=0; }
int xTaskGetTickCount() { return now; }
void vTaskDelay(int n) { now+=n; }
const char *esp_err_to_name(int) { return "stub"; }
int usb_host_endpoint_halt(int,int) { ++halts; return 0; }
int usb_host_endpoint_flush(int,int) { ++flushes; return 0; }
int usb_host_endpoint_clear(int,int) { ++clears; return 0; }
int usb_host_interface_release(int,int,int interface) {
    if(mode==1 || (mode==0 && now<3)) return ESP_ERR_INVALID_STATE;
    if(mode==2 && interface==2) return ESP_FAIL;
    bool &released=interface==1 ? dataReleased : notifReleased;
    if(released) return ESP_ERR_NOT_FOUND;
    released=true; return ESP_OK;
}
void cdc_acm_device_remove(Device *) {
    assert(dataReleased && notifReleased); ++removed;
}
using TaskHandle_t=void *;
static int created=0, createResult=1;
constexpr int pdPASS=1;
int xTaskCreatePinnedToCore(void (*)(void *), const char *, int, void *, int, void **handle, int core) {
    assert(core==0); ++created; *handle=reinterpret_cast<void *>(1); return createResult;
}
class UsbCdcHost {
public:
    std::atomic<bool> restart_requested_{false};
    TaskHandle_t restart_task_=nullptr;
    int host_core_=0;
    static void RestartTaskThunk(void *) {}
    bool requestRestart();
};
FUNCTIONS
RESTART_METHOD
int main() {
    usb_transfer_t in{0x81}, notif{0x82}; Interface dataIntf{1}, notifIntf{2};
    Device dev; dev.data={&in,&dataIntf,reinterpret_cast<void *>(1)};
    dev.notif={&notif,&notifIntf,reinterpret_cast<void *>(1)};
    auto reset=[&]() {
        driver.cdc_devices_list.head=&dev; now=removed=halts=flushes=clears=mode=locked=0;
        dataReleased=notifReleased=false; dev.closing=false;
    };
    reset();
    assert(cdc_acm_host_close(&dev)==ESP_OK);
    assert(now>=3 && removed==1 && dev.closing && !dev.data.in_cb && !dev.notif.cb);
    assert(halts==2 && flushes==2 && clears==0 && locked==0);
    reset(); mode=1; // Inflight transfers never drain: preserve storage for a later retry.
    assert(cdc_acm_host_close(&dev)==ESP_ERR_TIMEOUT);
    assert(now==2000 && removed==0 && driver.cdc_devices_list.head==&dev && !locked);
    mode=0; assert(cdc_acm_host_close(&dev)==ESP_OK && removed==1);
    reset(); mode=2; // A partial close must tolerate the already-released first interface.
    assert(cdc_acm_host_close(&dev)==ESP_FAIL && removed==0 && !locked);
    mode=0; assert(cdc_acm_host_close(&dev)==ESP_OK && removed==1);
    assert(cdc_acm_host_close(&dev)==ESP_OK && removed==1); // Already closed, no double free.
    UsbCdcHost host;
    assert(host.requestRestart() && created==1);
    assert(host.requestRestart() && created==1); // Coalesce requests while owner-core work is pending.
    host.restart_requested_=false; createResult=0;
    assert(!host.requestRestart() && !host.restart_requested_ && !host.restart_task_);
    createResult=1; assert(host.requestRestart() && created==3);
}
'''
code = '\n'.join(function(s) for s in [
    'static esp_err_t cdc_acm_stop_transfer_endpoint(',
    'esp_err_t cdc_acm_host_close('])
with tempfile.TemporaryDirectory(prefix='radpro-cdc-close-') as temp:
    cpp = Path(temp) / 'test.cpp'
    restart = function('bool UsbCdcHost::requestRestart()', (root / 'lib/UsbCdcHost/UsbCdcHost.cpp').read_text())
    cpp.write_text(harness.replace('FUNCTIONS', code).replace('RESTART_METHOD', restart))
    command = [args.cxx, '-std=c++17']
    if args.sdk: command += ['-isysroot', args.sdk]
    binary = Path(temp) / 'test'
    subprocess.run(command + [str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('CDC close waits for completion; timeout/error paths preserve resources and allow retry')

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the vendored CDC input callback using a controllable USB host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2] / "components" / "usb_host_cdc_acm"
driver = (root / "cdc_acm_host.c").read_text()
callbacks = driver[driver.index("static void report_transfer_error("):
                   driver.index("static void notif_xfer_cb(usb_transfer_t *transfer)\n{")]
types = (root / "include/usb/cdc_host_types.h").read_text()
events = types[types.index("typedef struct cdc_dev_s"):types.index("} cdc_acm_host_dev_event_data_t;") +
               len("} cdc_acm_host_dev_event_data_t;")]

prefix = r"""
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#define SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE 1
#define ESP_LOGD(...)
#define ESP_LOGW(...)
#define CDC_ACM_ENTER_CRITICAL()
#define CDC_ACM_EXIT_CRITICAL()
#define ESP_OK 0
typedef int esp_err_t;
typedef int cdc_acm_uart_state_t;
enum { USB_TRANSFER_STATUS_COMPLETED, USB_TRANSFER_STATUS_NO_DEVICE, USB_TRANSFER_STATUS_CANCELED,
       USB_TRANSFER_STATUS_ERROR, USB_TRANSFER_STATUS_TIMED_OUT, USB_TRANSFER_STATUS_STALL,
       USB_TRANSFER_STATUS_OVERFLOW, USB_TRANSFER_STATUS_SKIPPED };
typedef struct { void *context; int status; uint8_t *data_buffer; size_t actual_num_bytes; } usb_transfer_t;
"""
stub = r"""
typedef struct cdc_dev_s {
    bool closing;
    void *cb_arg;
    struct { void (*cb)(const cdc_acm_host_dev_event_data_t *, void *); } notif;
    struct { usb_transfer_t *in_xfer; bool (*in_cb)(uint8_t *, size_t, void *); } data;
} cdc_dev_t;
static int submissions, submit_result, reports;
static cdc_acm_host_dev_event_data_t last_event;
static int usb_host_transfer_submit(usb_transfer_t *t) { ++submissions; return submit_result; }
static void cdc_acm_reset_in_transfer(cdc_dev_t *d) {}
static void record(const cdc_acm_host_dev_event_data_t *event, void *arg) {
    ++reports; last_event = *event;
}
"""
cases = r"""
int main(void) {
    cdc_dev_t dev = { .notif.cb = record };
    usb_transfer_t rx = { .context = &dev }, notification = { .context = &dev, .status = USB_TRANSFER_STATUS_ERROR };
    dev.data.in_xfer = &rx;
    in_xfer_cb(&rx);
    assert(submissions == 1 && reports == 0);
    rx.status = USB_TRANSFER_STATUS_TIMED_OUT;
    in_xfer_cb(&rx);
    assert(submissions == 1 && reports == 1 && last_event.type == CDC_ACM_HOST_ERROR);
    assert(last_event.error_source == CDC_ACM_ERROR_BULK_IN_TRANSFER && last_event.data.error == rx.status);
    assert(!cdc_acm_is_transfer_completed(&notification));
    assert(reports == 2 && last_event.error_source == CDC_ACM_ERROR_OTHER);
    rx.status = USB_TRANSFER_STATUS_CANCELED; in_xfer_cb(&rx);
    rx.status = USB_TRANSFER_STATUS_NO_DEVICE; in_xfer_cb(&rx);
    assert(submissions == 1 && reports == 2);
    rx.status = USB_TRANSFER_STATUS_COMPLETED; submit_result = 77; in_xfer_cb(&rx);
    assert(submissions == 2 && reports == 3);
    assert(last_event.error_source == CDC_ACM_ERROR_BULK_IN_RESUBMIT && last_event.data.error == 77);
    dev.closing = true; in_xfer_cb(&rx);
    assert(submissions == 2 && reports == 3);
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "receive.c"
    exe = Path(tmp) / "receive"
    src.write_text(prefix + events + stub + callbacks + cases)
    subprocess.run(["cc", "-std=c11", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

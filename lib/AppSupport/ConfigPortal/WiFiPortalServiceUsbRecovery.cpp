// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ConfigPortal/WiFiPortalService.h"
#include "AppConfig/UsbRecoverySettings.h"
#include "ConfigPortal/PortalSecurity.h"

void WiFiPortalService::sendUsbRecoveryForm(const char *noticeKey, bool error)
{
    TemplateReplacements vars = {
        {"{{NOTICE_CLASS}}", noticeKey ? String(error ? "error" : "success") : String("hidden")},
        {"{{NOTICE_KEY}}", noticeKey ? String(noticeKey) : String()},
        {"{{RECOVERY_ENABLED}}", config_.usbRecoveryTimeoutSeconds ? String("checked") : String()},
        {"{{RECOVERY_MINUTES}}", String((config_.usbRecoveryTimeoutSeconds ? config_.usbRecoveryTimeoutSeconds : kDefaultUsbRecoveryTimeoutSeconds) / 60)}};
    appendCommonTemplateVars(vars);
    sendTemplate("/portal/usb-recovery.html", vars);
}

void WiFiPortalService::handleUsbRecoveryPost()
{
    uint32_t seconds = 0;
    if (manager_.server->hasArg("enabled") &&
        !UsbRecoverySettings::parseMinutes(manager_.server->arg("timeoutMinutes"), seconds))
    {
        sendUsbRecoveryForm("T_USB_RECOVERY_INVALID", true);
        return;
    }
    AppConfig updated = config_;
    updated.usbRecoveryTimeoutSeconds = seconds;
    const auto changed = collectChangedConfigFields(config_, updated);
    if (!store_.save(updated))
    {
        led_.activateFault(FaultCode::NvsWriteFailure);
        PortalSecurity::logConfigSaveFailure(log_, "/usb-recovery", currentClientIp(), changed);
        sendUsbRecoveryForm("T_USB_RECOVERY_SAVE_FAILED", true);
        return;
    }
    config_ = updated;
    led_.clearFault(FaultCode::NvsWriteFailure);
    logConfigSave("/usb-recovery", changed);
    sendUsbRecoveryForm("T_USB_RECOVERY_SAVED");
}

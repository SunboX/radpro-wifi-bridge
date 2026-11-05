// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ConfigPortal/WiFiPortalService.h"
#include "RadProUpdate/RadProUpdateService.h"
#include <ArduinoJson.h>

void WiFiPortalService::registerRadProUpdateRoutes()
{
    manager_.server->on("/radpro-update", HTTP_GET, [this]() {
        TemplateReplacements variables;
        appendCommonTemplateVars(variables);
        variables.emplace_back("{{LOCALE}}", resolvePortalLocale());
        if (!sendTemplate("/portal/radpro-update.html", variables))
            sendTemplateError("/portal/radpro-update.html");
    });
    manager_.server->on("/portal/js/radpro-update.js", HTTP_GET, [this]() {
        if (!sendStaticFile("/portal/js/radpro-update.js", "application/javascript"))
            sendTemplateError("/portal/js/radpro-update.js");
    });
    manager_.server->on("/radpro-update/status", HTTP_GET, [this]() {
        if (!radProUpdater_) { sendJson(503, "{\"error\":\"Updater unavailable.\"}"); return; }
        sendJson(200, radProUpdater_->statusJson());
    });
    // Form fields keep the same CSRF contract as the rest of the portal.
    auto handle = [this](const char *action) {
        const String route = String("/radpro-update/") + action;
        if (!requirePortalPost(route.c_str(), {}, true)) return;
        if (!radProUpdater_ || otaTaskHandle_ || otaService_.status().busy || restartScheduled_)
        { sendJson(409, "{\"error\":\"Bridge update or restart in progress.\"}"); return; }
        auto &server = *manager_.server;
        String error;
        bool ok = false;
        if (!strcmp(action, "begin"))
            ok = radProUpdater_->beginUpload(server.arg("locale"), error);
        else if (!strcmp(action, "chunk"))
        {
            // Bound memory consumption before base64 allocation; enforce strict offsets.
            const String offset = server.arg("offset");
            bool validOffset = offset.length() > 0 && offset.length() <= 6;
            for (size_t i = 0; i < offset.length(); ++i)
                validOffset = validOffset && offset[i] >= '0' && offset[i] <= '9';
            std::vector<uint8_t> bytes;
            if (!validOffset || server.arg("data").length() > 4096)
                error = "Invalid offset or oversized firmware chunk.";
            else if (decodeBase64Chunk(server.arg("data"), bytes, error))
                ok = radProUpdater_->uploadChunk(strtoul(offset.c_str(), nullptr, 10), bytes.data(), bytes.size(), error);
        }
        else if (!strcmp(action, "start"))
        {
            if (server.arg("confirm") != "3.1.1") error = "Explicit firmware confirmation required.";
            else ok = radProUpdater_->start(error);
        }
        else if (!strcmp(action, "cancel")) ok = radProUpdater_->cancel(error);
        JsonDocument response;
        response["ok"] = ok;
        if (!ok) response["error"] = error;
        String json;
        serializeJson(response, json);
        sendJson(ok ? 200 : 409, json);
    };
    for (const char *action : {"begin", "chunk", "start", "cancel"})
    {
        const String route = String("/radpro-update/") + action;
        manager_.server->on(route.c_str(), HTTP_POST, [handle, action]() { handle(action); });
    }
}

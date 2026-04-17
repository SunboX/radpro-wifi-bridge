#pragma once

#include <Arduino.h>
#include <cstring>

namespace OpenRadiationPortalView
{
struct LatestMeasurementViewModel
{
    String reportUuid;
    String valueText;
    String startTime;
    String qualification;
    bool atypical = false;
    String mapUrl;
    String errorMessage;
};

inline String escapeHtml(const String &value)
{
    String out;
    const char *raw = value.c_str();
    if (!raw)
        return out;

    for (size_t i = 0; raw[i] != '\0'; ++i)
    {
        const char c = raw[i];
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
        {
            const char text[2] = {c, '\0'};
            out += text;
            break;
        }
        }
    }
    return out;
}

inline String buildLinksSection(const String &mapUrl, const String &latestPath)
{
    String html;
    html += "<section style='margin:16px 0;padding:14px;border:1px solid #444;border-radius:8px;background:#1a1a1a;'>";

    if (mapUrl.length())
    {
        html += "<p style='margin:0 0 10px 0;'><a href='";
        html += escapeHtml(mapUrl);
        html += "' target='_blank' rel='noopener'>Open on OpenRadiation map</a></p>";
    }
    else
    {
        html += "<p style='margin:0 0 10px 0;color:#bbb;'>Add latitude and longitude to enable the public OpenRadiation map link.</p>";
    }

    if (latestPath.length())
    {
        html += "<p style='margin:0;'><a href='";
        html += escapeHtml(latestPath);
        html += "'>Open latest published measurement</a></p>";
    }
    else
    {
        html += "<p style='margin:0;color:#bbb;'>Latest published measurement becomes available after the first successful upload.</p>";
    }

    html += "</section>";
    return html;
}

inline String buildMeasurementEnvironmentOptions(const String &selectedValue)
{
    struct Option
    {
        const char *value;
        const char *label;
    };

    static constexpr Option kOptions[] = {
        {"countryside", "Countryside"},
        {"city", "City"},
        {"ontheroad", "On the road"},
        {"inside", "Inside"},
        {"plane", "Plane"},
    };

    String html;
    for (const auto &option : kOptions)
    {
        html += "<option value='";
        html += option.value;
        html += "'";
        if (std::strcmp(selectedValue.c_str(), option.value) == 0)
            html += " selected";
        html += ">";
        html += option.label;
        html += "</option>";
    }
    return html;
}

inline String buildLatestMeasurementPage(const LatestMeasurementViewModel &model)
{
    String html;
    html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'/>"
            "<title>Latest OpenRadiation measurement</title>"
            "<style>body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#eee;margin:0;padding:24px;display:flex;justify-content:center;}"
            ".wrap{display:inline-block;min-width:260px;max-width:640px;width:100%;}"
            "a{color:#7ec8ff;}dl{display:grid;grid-template-columns:max-content 1fr;gap:10px 14px;}"
            "dt{font-weight:bold;}dd{margin:0;}button{padding:10px;border:none;border-radius:4px;background:#2196F3;color:#fff;font-size:15px;cursor:pointer;width:100%;}</style>"
            "</head><body><div class='wrap'><h1>Latest OpenRadiation measurement</h1>";

    if (model.errorMessage.length())
    {
        html += "<p style='color:#ffb4b4;'>";
        html += escapeHtml(model.errorMessage);
        html += "</p>";
        html += "<form action='/openradiation' method='get'><button type='submit'>Back to OpenRadiation settings</button></form>";
        html += "</div></body></html>";
        return html;
    }

    html += "<dl>";
    html += "<dt>Report UUID</dt><dd>";
    html += escapeHtml(model.reportUuid);
    html += "</dd>";
    html += "<dt>Value</dt><dd>";
    html += escapeHtml(model.valueText);
    html += "</dd>";
    html += "<dt>Start time</dt><dd>";
    html += escapeHtml(model.startTime);
    html += "</dd>";
    if (model.qualification.length())
    {
        html += "<dt>Qualification</dt><dd>";
        html += escapeHtml(model.qualification);
        html += "</dd>";
    }
    html += "<dt>Atypical</dt><dd>";
    html += model.atypical ? "yes" : "no";
    html += "</dd>";
    html += "</dl>";

    if (model.mapUrl.length())
    {
        html += "<p style='margin-top:16px;'><a href='";
        html += escapeHtml(model.mapUrl);
        html += "' target='_blank' rel='noopener'>Open on OpenRadiation map</a></p>";
    }

    html += "<form action='/openradiation' method='get' style='margin-top:20px;'><button type='submit'>Back to OpenRadiation settings</button></form>";
    html += "</div></body></html>";
    return html;
}
} // namespace OpenRadiationPortalView

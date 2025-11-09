#include "DeviceInfoPage.h"

DeviceInfoPage::DeviceInfoPage(DeviceInfoStore &store) : store_(store) {}

void DeviceInfoPage::handlePage(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;

    String html;
    html.reserve(6000);
    html += buildHtml();
    html += buildScript();
    html += F("</body></html>");
    manager->server->send(200, "text/html", html);
}

void DeviceInfoPage::handleJson(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;
    manager->server->send(200, "application/json", store_.toJson());
}

String DeviceInfoPage::buildHtml() const
{
    String html;
    html += F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'/>"
              "<title>RadPro Device Info</title>"
              "<style>body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#eee;margin:0;padding:24px;display:flex;justify-content:center;}"
              ".wrap{max-width:620px;width:100%;}"
              "h1{margin-top:0;}section{background:#1e1e1e;border-radius:8px;padding:16px;margin-bottom:20px;box-shadow:0 4px 18px rgba(0,0,0,0.35);}"
              "dl{display:grid;grid-template-columns:minmax(100px,1fr) minmax(180px,2fr);gap:8px;margin:0;}"
              "dt{font-weight:bold;color:#9ecfff;}dd{margin:0;}"
              ".measurements{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin-top:12px;}"
              ".card{background:#222;padding:12px;border-radius:6px;text-align:center;border:1px solid #333;}"
              ".card span.value{display:block;font-size:1.6rem;font-weight:bold;color:#8be78b;}"
              ".card span.label{font-size:0.85rem;color:#aaa;letter-spacing:0.05em;}"
              "button{padding:10px;border:none;border-radius:4px;background:#2196F3;color:#fff;font-size:15px;cursor:pointer;margin-top:10px;width:100%;}"
              "button:hover{background:#1976D2;}a{color:#03A9F4;text-decoration:none;}"
              "</style></head><body class='invert'><div class='wrap'>");
    html += F("<h1>RadPro Device Info</h1>"
              "<section><h2>Identity</h2><dl>"
              "<dt>Manufacturer</dt><dd id='manufacturer'>—</dd>"
              "<dt>Model</dt><dd id='model'>—</dd>"
              "<dt>Firmware</dt><dd id='firmware'>—</dd>"
              "<dt>Device ID</dt><dd id='deviceId'>—</dd>"
              "<dt>Locale</dt><dd id='locale'>—</dd>"
              "<dt>Power</dt><dd id='devicePower'>—</dd>"
              "</dl></section>");
    html += F("<section><h2>Live Measurements</h2>"
              "<div class='measurements'>"
              "<div class='card'><span class='label'>Tube Rate (cpm)</span><span class='value' id='tubeRate'>—</span></div>"
              "<div class='card'><span class='label'>Dose Rate (&micro;Sv/h)</span><span class='value' id='tubeDoseRate'>—</span></div>"
              "<div class='card'><span class='label'>Pulse Count</span><span class='value' id='tubePulseCount'>—</span></div>"
              "<div class='card'><span class='label'>Battery Voltage (V)</span><span class='value' id='batteryVoltage'>—</span></div>"
              "<div class='card'><span class='label'>Battery %</span><span class='value' id='batteryPercent'>—</span></div>"
              "<div class='card'><span class='label'>Last Update</span><span class='value' id='measurementAge'>—</span></div>"
              "</div></section>"
              "<form action='/' method='get'><button type='submit'>Back to main menu</button></form>");
    html += F("</div>");
    return html;
}

String DeviceInfoPage::buildScript() const
{
    String script;
    script += F("<script>"
                "const setField=(id,val,unit='')=>{const el=document.getElementById(id);if(!el)return;if(val===null||val===undefined||val===''){el.textContent='—';return;}el.textContent=unit?`${val} ${unit}`.trim():val;};"
                "async function refreshDeviceInfo(){try{const resp=await fetch('/device.json',{cache:'no-store'});if(!resp.ok)throw new Error('bad status');const data=await resp.json();"
                "setField('manufacturer',data.manufacturer);"
                "setField('model',data.model);"
                "setField('firmware',data.firmware);"
                "setField('deviceId',data.deviceId);"
                "setField('locale',data.locale);"
                "setField('devicePower',data.devicePower==='1'?'ON':(data.devicePower==='0'?'OFF':data.devicePower));"
                "setField('batteryVoltage',data.batteryVoltage);"
                "setField('batteryPercent',data.batteryPercent?`${data.batteryPercent} %`:data.batteryPercent);"
                "setField('tubeRate',data.tubeRate);"
                "setField('tubeDoseRate',data.tubeDoseRate);"
                "setField('tubePulseCount',data.tubePulseCount);"
                "if(data.measurementAgeMs!==null&&data.measurementAgeMs!==undefined){const seconds=(data.measurementAgeMs/1000).toFixed(1);setField('measurementAge',seconds+' s ago');}"
                "else{setField('measurementAge','—');}"
                "}catch(err){console.warn('Device info refresh failed',err);}}"
                "refreshDeviceInfo();setInterval(refreshDeviceInfo,10000);</script>");
    return script;
}

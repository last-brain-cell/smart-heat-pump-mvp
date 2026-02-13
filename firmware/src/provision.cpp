/**
 * @file provision.cpp
 * @brief WiFi provisioning portal implementation
 */

#include "provision.h"
#include "globals.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_task_wdt.h>

// =============================================================================
// WIFI SCANNING
// =============================================================================

static int scanCount = 0;

static void doWiFiScan() {
    Log.println(F("[PROV] Scanning WiFi networks..."));
    scanCount = WiFi.scanNetworks(false, false);
    if (scanCount < 0) scanCount = 0;
    Log.print(F("[PROV] Found "));
    Log.print(scanCount);
    Log.println(F(" networks"));
}

static String htmlEscape(const String& s) {
    String out = s;
    out.replace("&", "&amp;");
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    out.replace("\"", "&quot;");
    return out;
}

// =============================================================================
// HTML FRAGMENTS (PROGMEM)
// =============================================================================

static const char PAGE_HEAD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Heat Pump Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:20px;min-height:100vh}
h1{text-align:center;color:#00d4ff;margin-bottom:24px;font-size:1.4em}
fieldset{border:1px solid #333;border-radius:8px;padding:16px;margin-bottom:16px}
legend{color:#00d4ff;font-weight:bold;padding:0 8px}
label{display:block;margin-top:12px;font-size:0.9em;color:#aaa}
label:first-child{margin-top:0}
input,select{width:100%;padding:10px;margin-top:4px;background:#16213e;border:1px solid #444;border-radius:4px;color:#fff;font-size:1em}
input:focus,select:focus{outline:none;border-color:#00d4ff}
button{width:100%;padding:14px;margin-top:20px;background:#00d4ff;color:#1a1a2e;border:none;border-radius:8px;font-size:1.1em;font-weight:bold;cursor:pointer}
button:hover{background:#00b8d4}
.note{text-align:center;color:#666;font-size:0.8em;margin-top:12px}
.rescan{display:inline-block;color:#00d4ff;font-size:0.8em;margin-top:6px;text-decoration:none}
.rescan:hover{text-decoration:underline}
</style>
</head>
<body>
<h1>Heat Pump Setup</h1>
<form method="POST" action="/save">
)rawliteral";

static const char PAGE_FOOT[] PROGMEM = R"rawliteral(
<button type="submit">Save &amp; Connect</button>
</form>
<p class="note">Saved! Connect to your WiFi to see the log viewer.</p>
</body>
</html>
)rawliteral";

static const char PROVISION_SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Saved</title>
<style>
body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh}
.ok{text-align:center}
h1{color:#00d4ff;margin-bottom:12px}
</style>
</head>
<body>
<div class="ok">
<h1>Configuration Saved</h1>
<p>Connecting to WiFi...</p>
</div>
</body>
</html>
)rawliteral";

// =============================================================================
// NVS FUNCTIONS
// =============================================================================

void loadConfig(RuntimeConfig& cfg) {
    Preferences prefs;
    prefs.begin(PROVISION_NVS_NS, true);  // read-only

    if (!prefs.getBool("configured", false)) {
        Log.println(F("[PROV] No stored config, using compile-time defaults"));
        prefs.end();
        return;
    }

    String s;

    s = prefs.getString("wifi_ssid", "");
    if (s.length() > 0) strncpy(cfg.wifiSSID, s.c_str(), sizeof(cfg.wifiSSID) - 1);

    s = prefs.getString("wifi_pass", "");
    if (s.length() > 0) strncpy(cfg.wifiPass, s.c_str(), sizeof(cfg.wifiPass) - 1);

    s = prefs.getString("mqtt_host", "");
    if (s.length() > 0) strncpy(cfg.mqttHost, s.c_str(), sizeof(cfg.mqttHost) - 1);

    cfg.mqttPort = prefs.getUShort("mqtt_port", cfg.mqttPort);

    s = prefs.getString("mqtt_user", "");
    if (s.length() > 0) strncpy(cfg.mqttUser, s.c_str(), sizeof(cfg.mqttUser) - 1);

    s = prefs.getString("mqtt_pass", "");
    if (s.length() > 0) strncpy(cfg.mqttPass, s.c_str(), sizeof(cfg.mqttPass) - 1);

    prefs.end();

    Log.println(F("[PROV] Config loaded from NVS"));
    Log.print(F("[PROV]   WiFi SSID: "));
    Log.println(cfg.wifiSSID);
    Log.print(F("[PROV]   MQTT Host: "));
    Log.print(cfg.mqttHost);
    Log.print(F(":"));
    Log.println(cfg.mqttPort);
}

void saveConfig(const RuntimeConfig& cfg) {
    Preferences prefs;
    prefs.begin(PROVISION_NVS_NS, false);

    prefs.putString("wifi_ssid", cfg.wifiSSID);
    prefs.putString("wifi_pass", cfg.wifiPass);
    prefs.putString("mqtt_host", cfg.mqttHost);
    prefs.putUShort("mqtt_port", cfg.mqttPort);
    prefs.putString("mqtt_user", cfg.mqttUser);
    prefs.putString("mqtt_pass", cfg.mqttPass);
    prefs.putBool("configured", true);

    prefs.end();
    Log.println(F("[PROV] Config saved to NVS"));
}

void clearConfig() {
    Preferences prefs;
    prefs.begin(PROVISION_NVS_NS, false);
    prefs.clear();
    prefs.end();
    Log.println(F("[PROV] NVS config cleared"));
}

bool isProvisioned() {
    Preferences prefs;
    prefs.begin(PROVISION_NVS_NS, true);
    bool result = prefs.getBool("configured", false);
    prefs.end();
    return result;
}

// =============================================================================
// CAPTIVE PORTAL
// =============================================================================

static WebServer* portalServer = nullptr;
static DNSServer* dnsServer = nullptr;
static bool portalSubmitted = false;
static RuntimeConfig* portalCfg = nullptr;

static void handleRoot() {
    portalServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
    portalServer->send(200, "text/html", "");

    // Page header with styles
    portalServer->sendContent_P(PAGE_HEAD);

    // WiFi fieldset — dropdown with scanned networks
    portalServer->sendContent_P(PSTR(
        "<fieldset><legend>WiFi</legend>"
        "<label>Available Networks</label>"
        "<select id=\"sel\" onchange=\"if(this.value)document.getElementById('si').value=this.value\">"
        "<option value=\"\">-- Select --</option>"));

    // Dynamic options from WiFi scan
    for (int i = 0; i < scanCount; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;  // skip hidden networks

        // Dedup: skip if same SSID appeared earlier with stronger signal
        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (WiFi.SSID(j) == ssid) { dup = true; break; }
        }
        if (dup) continue;

        int rssi = WiFi.RSSI(i);
        int quality = rssi > -50 ? 100 : rssi < -100 ? 0 : 2 * (rssi + 100);
        String escaped = htmlEscape(ssid);

        char opt[192];
        snprintf(opt, sizeof(opt),
            "<option value=\"%s\">%s (%d%%)</option>",
            escaped.c_str(), escaped.c_str(), quality);
        portalServer->sendContent(opt);
    }

    portalServer->sendContent_P(PSTR(
        "</select>"
        "<a href=\"/rescan\" class=\"rescan\">&#8635; Rescan</a>"
        "<label>SSID</label>"));

    // SSID input — pre-filled with current config
    char buf[192];
    snprintf(buf, sizeof(buf),
        "<input name=\"wifi_ssid\" id=\"si\" required maxlength=\"63\" value=\"%s\">",
        htmlEscape(String(portalCfg->wifiSSID)).c_str());
    portalServer->sendContent(buf);

    // WiFi password — pre-filled
    snprintf(buf, sizeof(buf),
        "<label>Password</label>"
        "<input name=\"wifi_pass\" type=\"password\" maxlength=\"63\" value=\"%s\">"
        "</fieldset>",
        htmlEscape(String(portalCfg->wifiPass)).c_str());
    portalServer->sendContent(buf);

    // MQTT fieldset — pre-filled with current config
    char mqttBuf[640];
    snprintf(mqttBuf, sizeof(mqttBuf),
        "<fieldset><legend>MQTT Broker</legend>"
        "<label>Host / IP</label>"
        "<input name=\"mqtt_host\" required maxlength=\"63\" value=\"%s\">"
        "<label>Port</label>"
        "<input name=\"mqtt_port\" type=\"number\" value=\"%u\" min=\"1\" max=\"65535\">"
        "<label>Username</label>"
        "<input name=\"mqtt_user\" maxlength=\"31\" value=\"%s\">"
        "<label>Password</label>"
        "<input name=\"mqtt_pass\" type=\"password\" maxlength=\"63\" value=\"%s\">"
        "</fieldset>",
        htmlEscape(String(portalCfg->mqttHost)).c_str(),
        portalCfg->mqttPort,
        htmlEscape(String(portalCfg->mqttUser)).c_str(),
        htmlEscape(String(portalCfg->mqttPass)).c_str());
    portalServer->sendContent(mqttBuf);

    // Page footer
    portalServer->sendContent_P(PAGE_FOOT);
}

static void handleSave() {
    if (!portalServer->hasArg("wifi_ssid") || !portalServer->hasArg("mqtt_host")) {
        portalServer->send(400, "text/plain", "Missing required fields");
        return;
    }

    String ssid = portalServer->arg("wifi_ssid");
    String pass = portalServer->arg("wifi_pass");
    String host = portalServer->arg("mqtt_host");
    String port = portalServer->arg("mqtt_port");
    String user = portalServer->arg("mqtt_user");
    String mpass = portalServer->arg("mqtt_pass");

    strncpy(portalCfg->wifiSSID, ssid.c_str(), sizeof(portalCfg->wifiSSID) - 1);
    portalCfg->wifiSSID[sizeof(portalCfg->wifiSSID) - 1] = '\0';
    strncpy(portalCfg->wifiPass, pass.c_str(), sizeof(portalCfg->wifiPass) - 1);
    portalCfg->wifiPass[sizeof(portalCfg->wifiPass) - 1] = '\0';
    strncpy(portalCfg->mqttHost, host.c_str(), sizeof(portalCfg->mqttHost) - 1);
    portalCfg->mqttHost[sizeof(portalCfg->mqttHost) - 1] = '\0';
    portalCfg->mqttPort = port.length() > 0 ? port.toInt() : 1883;
    strncpy(portalCfg->mqttUser, user.c_str(), sizeof(portalCfg->mqttUser) - 1);
    portalCfg->mqttUser[sizeof(portalCfg->mqttUser) - 1] = '\0';
    strncpy(portalCfg->mqttPass, mpass.c_str(), sizeof(portalCfg->mqttPass) - 1);
    portalCfg->mqttPass[sizeof(portalCfg->mqttPass) - 1] = '\0';

    saveConfig(*portalCfg);

    portalServer->send(200, "text/html", FPSTR(PROVISION_SUCCESS_HTML));
    portalSubmitted = true;
}

static void handleNotFound() {
    portalServer->sendHeader("Location", "/", true);
    portalServer->send(302, "text/plain", "");
}

static void handleRescan() {
    doWiFiScan();
    portalServer->sendHeader("Location", "/", true);
    portalServer->send(302, "text/plain", "");
}

void startProvisioningPortal(RuntimeConfig& cfg) {
    Log.println(F("[PROV] Starting AP provisioning portal..."));
    Log.print(F("[PROV] AP SSID: "));
    Log.println(PROVISION_AP_SSID);

    portalCfg = &cfg;
    portalSubmitted = false;

    // AP+STA mode: AP serves the portal, STA enables WiFi scanning
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(PROVISION_AP_SSID);
    delay(100);

    // Scan for nearby networks
    doWiFiScan();

    IPAddress apIP = WiFi.softAPIP();
    Log.print(F("[PROV] AP IP: "));
    Log.println(apIP);

    // Start DNS server for captive portal redirect
    dnsServer = new DNSServer();
    dnsServer->start(53, "*", apIP);

    // Start web server
    portalServer = new WebServer(80);
    portalServer->on("/", HTTP_GET, handleRoot);
    portalServer->on("/save", HTTP_POST, handleSave);
    portalServer->on("/rescan", HTTP_GET, handleRescan);
    portalServer->onNotFound(handleNotFound);
    portalServer->begin();

    Log.println(F("[PROV] Portal active — connect to AP and open 192.168.4.1"));
    Log.print(F("[PROV] Timeout in "));
    Log.print(PROVISION_TIMEOUT_MS / 1000);
    Log.println(F(" seconds"));

    // Block until submit or timeout
    unsigned long start = millis();
    while (!portalSubmitted && (millis() - start < PROVISION_TIMEOUT_MS)) {
        dnsServer->processNextRequest();
        portalServer->handleClient();
        esp_task_wdt_reset();
        delay(10);
    }

    // Cleanup
    portalServer->stop();
    delete portalServer;
    portalServer = nullptr;
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;

    WiFi.scanDelete();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    if (portalSubmitted) {
        Log.println(F("[PROV] Portal completed — config saved"));
    } else {
        Log.println(F("[PROV] Portal timed out — using current defaults"));
    }

    portalCfg = nullptr;
}

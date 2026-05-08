/**
 * @file dashboard.cpp
 * @brief Serial log viewer web server implementation
 *
 * Serves a dark-themed <pre> log viewer that polls /api/log every 2s,
 * appends new text, and auto-scrolls. Reads from the LogCapture ring buffer.
 */

#include "dashboard.h"
#include "globals.h"
#include <WiFi.h>

// =============================================================================
// SERVER STATE
// =============================================================================

static WiFiServer* dashServer = nullptr;
static bool dashRunning = false;

// =============================================================================
// LOG VIEWER HTML (PROGMEM)
// =============================================================================

static const char DASH_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Heat Pump Log</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#e0e0e0;font-family:monospace;padding:0;height:100vh;display:flex;flex-direction:column}
h1{text-align:center;color:#00d4ff;padding:12px;font-size:1.1em;border-bottom:1px solid #333;flex-shrink:0}
#log{flex:1;overflow-y:auto;padding:12px;font-size:13px;line-height:1.4;white-space:pre-wrap;word-wrap:break-word;color:#b0b0b0}
#bar{text-align:center;color:#555;font-size:0.75em;padding:4px;border-top:1px solid #333;flex-shrink:0}
</style>
</head>
<body>
<h1>Heat Pump Log Viewer</h1>
<pre id="log"></pre>
<div id="bar">Connecting...</div>
<script>
var pos=0,el=document.getElementById('log'),bar=document.getElementById('bar'),auto=true;
el.addEventListener('scroll',function(){
  auto=(el.scrollTop+el.clientHeight>=el.scrollHeight-30);
});
function poll(){
fetch('/api/log?pos='+pos).then(function(r){return r.json();}).then(function(d){
  if(d.text.length>0){el.textContent+=d.text;if(auto)el.scrollTop=el.scrollHeight;}
  pos=d.pos;
  bar.textContent='pos: '+pos+' | heap: '+d.heap+' B';
}).catch(function(){bar.textContent='Connection lost - retrying...';});
}
poll();setInterval(poll,2000);
</script>
</body>
</html>
)rawliteral";

// =============================================================================
// HTTP RESPONSE HELPERS
// =============================================================================

static void sendResponse(WiFiClient& client, const char* status,
                          const char* contentType, const char* body, size_t bodyLen) {
    client.printf("HTTP/1.1 %s\r\n", status);
    client.printf("Content-Type: %s\r\n", contentType);
    client.printf("Content-Length: %u\r\n", (unsigned int)bodyLen);
    client.print("Connection: close\r\n");
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("\r\n");
    client.write((const uint8_t*)body, bodyLen);
}

static void sendProgmemResponse(WiFiClient& client, const char* status,
                                 const char* contentType, const char* pgmData) {
    size_t len = strlen_P(pgmData);
    client.printf("HTTP/1.1 %s\r\n", status);
    client.printf("Content-Type: %s\r\n", contentType);
    client.printf("Content-Length: %u\r\n", (unsigned int)len);
    client.print("Connection: close\r\n");
    client.print("\r\n");

    const size_t chunkSize = 256;
    char chunk[chunkSize];
    size_t sent = 0;
    while (sent < len) {
        size_t toSend = len - sent;
        if (toSend > chunkSize) toSend = chunkSize;
        memcpy_P(chunk, pgmData + sent, toSend);
        client.write((const uint8_t*)chunk, toSend);
        sent += toSend;
    }
}

// =============================================================================
// LOG API HANDLER
// =============================================================================

static void handleLogAPI(WiFiClient& client, const String& requestLine) {
    // Parse pos parameter
    size_t fromPos = 0;
    bool hasPos = false;
    int posIdx = requestLine.indexOf("pos=");
    if (posIdx > 0) {
        fromPos = strtoul(requestLine.c_str() + posIdx + 4, nullptr, 10);
        hasPos = true;
    }

    size_t head = Log.getHead();

    // If no pos specified, start from ~2KB before head
    if (!hasPos && head > 2048) {
        fromPos = head - 2048;
    }

    // Read log data
    char logBuf[2048];
    size_t bytesRead = Log.readLog(logBuf, sizeof(logBuf), fromPos);

    // Build JSON response with escaped text
    size_t respCapacity = bytesRead * 2 + 128;
    char* resp = (char*)malloc(respCapacity);
    if (!resp) {
        const char* err = "{\"error\":\"oom\"}";
        sendResponse(client, "500 Internal Server Error", "application/json", err, strlen(err));
        return;
    }

    size_t w = snprintf(resp, respCapacity, "{\"pos\":%u,\"heap\":%u,\"text\":\"",
                        (unsigned int)head, (unsigned int)ESP.getFreeHeap());

    // JSON-escape the log text
    for (size_t i = 0; i < bytesRead && w < respCapacity - 4; i++) {
        char c = logBuf[i];
        if (c == '"')       { resp[w++] = '\\'; resp[w++] = '"'; }
        else if (c == '\\') { resp[w++] = '\\'; resp[w++] = '\\'; }
        else if (c == '\n') { resp[w++] = '\\'; resp[w++] = 'n'; }
        else if (c == '\r') { resp[w++] = '\\'; resp[w++] = 'r'; }
        else if (c == '\t') { resp[w++] = '\\'; resp[w++] = 't'; }
        else if ((uint8_t)c >= 0x20) { resp[w++] = c; }
    }

    w += snprintf(resp + w, respCapacity - w, "\"}");

    sendResponse(client, "200 OK", "application/json", resp, w);
    free(resp);
}

// =============================================================================
// SERVER IMPLEMENTATION
// =============================================================================

void initDashboard() {
    if (dashRunning) return;

    dashServer = new WiFiServer(80);
    dashServer->begin();
    dashRunning = true;

    Log.print(F("[DASH] Log viewer started at http://"));
    Log.print(WiFi.localIP());
    Log.println(F("/"));
}

void stopDashboard() {
    if (!dashRunning) return;

    dashServer->stop();
    delete dashServer;
    dashServer = nullptr;
    dashRunning = false;

    Log.println(F("[DASH] Dashboard stopped"));
}

void handleDashboard() {
    if (!dashRunning || dashServer == nullptr) return;

    WiFiClient client = dashServer->available();
    if (!client) return;

    // Wait briefly for data
    unsigned long start = millis();
    while (client.connected() && !client.available() && millis() - start < 200) {
        delay(1);
    }

    if (!client.available()) {
        client.stop();
        return;
    }

    // Read the first line of the request
    String requestLine = client.readStringUntil('\n');
    requestLine.trim();

    // Consume remaining headers
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.length() <= 1) break;
    }

    // Route request
    if (requestLine.startsWith("GET /api/log")) {
        handleLogAPI(client, requestLine);
    } else if (requestLine.startsWith("GET / ") || requestLine.startsWith("GET / HTTP") ||
               requestLine == "GET /") {
        sendProgmemResponse(client, "200 OK", "text/html; charset=utf-8", DASH_HTML);
    } else {
        const char* notFound = "404 Not Found";
        sendResponse(client, "404 Not Found", "text/plain", notFound, strlen(notFound));
    }

    client.stop();
}

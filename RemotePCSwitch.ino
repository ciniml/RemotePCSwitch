/**
 * RemotePCSwitch.ino
 * Author: Kenta IDA
 * License: Boost Software License 1.0
 */ 

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <string>

static const char* WIFI_SSID = "ssid";
static const char* WIFI_PASS = "password";

static const char* WEB_USER = "admin";
static const char* WEB_PASS = "admin";

enum class AppState
{
    WiFiConnecting,
    ServerRunning,
};

static AppState state = AppState::WiFiConnecting;
static WebServer server(8000);

static constexpr const int RELAY_PIN = GPIO_NUM_26;
static constexpr const char* DEFAULT_NODE_NAME = "RemotePCSwitch";
static constexpr const char* NODE_NAME_PATH = "/spiffs/node_name";

static String node_name(DEFAULT_NODE_NAME);


static void initializeRelay()
{
    digitalWrite(RELAY_PIN, 0);
    pinMode(RELAY_PIN, OUTPUT);
}

static void setRelay(bool value)
{
    digitalWrite(RELAY_PIN, value ? 1 : 0);
}

static void loadNodeName(void)
{
    if( SPIFFS.exists(NODE_NAME_PATH) ) {
        auto file = SPIFFS.open(NODE_NAME_PATH);
        if( file ) {
            char buffer[257];
            auto bytesRead = file.readBytes(buffer, sizeof(buffer)-1);
            buffer[bytesRead] = 0;
            node_name = buffer;
        }
    }
    else {
        node_name = DEFAULT_NODE_NAME;
    }
}

static void saveNodeName(void)
{
    auto file = SPIFFS.open(NODE_NAME_PATH, "w");
    if( file ) {
        file.write(reinterpret_cast<const uint8_t*>(node_name.c_str()), node_name.length());
    }
}

void setup()
{
    initializeRelay();

    Serial.begin(115200);

    SPIFFS.begin(true);
    loadNodeName();

    WiFi.mode(WIFI_MODE_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    server.on("/", [](){
        Serial.println("Requested");
        if( !server.authenticate(WEB_USER, WEB_PASS) ) {
            return server.requestAuthentication();
        }
        server.send(200, "text/plain", "Login OK");
    });
    server.on("/name", []() {
        if( !server.authenticate(WEB_USER, WEB_PASS) ) {
            return server.requestAuthentication();
        }
        server.send(200, "text/plain", node_name);
    });
    server.on("/name/{}", []() {
        if( !server.authenticate(WEB_USER, WEB_PASS) ) {
            return server.requestAuthentication();
        }
        node_name = server.pathArg(0);
        saveNodeName();
        server.send(200, "text/plain", node_name);
    });
    server.on("/switch/{}", []() {
        String arg = server.pathArg(0);
        if( !server.authenticate(WEB_USER, WEB_PASS) ) {
            return server.requestAuthentication();
        }
        if( arg == "on" ) {
            setRelay(true);
            server.send(200, "text/plain", "turn on");
        }
        else if( arg == "off" ) {
            setRelay(false);
            server.send(200, "text/plain", "turn off");
        }
        else {
            server.send(400, "text/plain", "invalid argument: " + arg);
        }
    });
    server.on("/cycle/{}", []() {
        String arg = server.pathArg(0);
        if( !server.authenticate(WEB_USER, WEB_PASS) ) {
            return server.requestAuthentication();
        }
        if( arg == "short" ) {
            setRelay(true);
            delay(1000);
            setRelay(false);
            server.send(200, "text/plain", "short");
        }
        else if( arg == "long" ) {
            setRelay(true);
            delay(5000);
            setRelay(false);
            server.send(200, "text/plain", "long");
        }
        else {
            server.send(400, "text/plain", "invalid argument: " + arg);
        }
    });
}

void loop()
{
    switch(state)
    {
    case AppState::WiFiConnecting: {
        if( WiFi.isConnected() ) {
            Serial.println("WiFi connected");
            server.begin();
            state = AppState::ServerRunning;
        }
        else {
            delay(1000);
        }
        break;
    }
    case AppState::ServerRunning: {
        if( !WiFi.isConnected() ) {
            Serial.println("WiFi disconnected");
            server.close();
            state = AppState::WiFiConnecting;
        }
        else {
            server.handleClient();
        }
        break;
    }
    }
}
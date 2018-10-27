#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <NeoPixelBus.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
// #include <WiFiManager.h>
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>
//#include "secrets.h"

#define HOSTNAME "TheRideNeoPixel"

#ifndef SECRET
#define WIFI_SSID "WiFi SSID"
#define WIFI_PASS "WiFi Password"
#define OTA_PASS "Arduino OTA Password"
String RIDE_API = "API from Ride.org";
#define BUS_ROUTE 4    // Bus Route
#define DAY_STOP 130   // YTC
#define NIGHT_STOP 145 // BTC
#define MQTT_SERVER "192.168.0.xxx"
#define MQTT_PORT 1883
#define MQTT_USER "" // MQTT User Name
#define MQTT_PASS "" // MQTT Password
#endif

#define MQTT_TOPIC "home/TheRideNeoPixel/data"

float updateInterval = 30.0; // in seconds
const uint16_t PixelCount = 16;

#define TZ -4     // (utc+) TZ in hours
#define DST_MN 00 // use 60mn for summer time in some countries

////////////////////////////////////////////////////////

#define TZ_MN ((TZ)*60)
#define TZ_SEC ((TZ)*3600)
#define DST_SEC ((DST_MN)*60)

#define USE_SERIAL Serial

#define NO_STEPS 10
#define RED 0xFF0000
#define GREEN 0x00FF00
#define BLUE 0x0000FF
#define WHITE 0xFFFFFF
#define BLACK 0x000000
#define YELLOW 0xFFFF00
#define CYAN 0x00FFFF
#define MAGENTA 0xFF00FF
#define PURPLE 0x400080
#define ORANGE 0xFF3000

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount); // Use RX Pin on ESP8266
NeoGamma<NeoGammaTableMethod> colorGamma;
WiFiClient espClient;
PubSubClient client(espClient);

Ticker UpdateRideData;
Ticker BlinkBlue;
Ticker fadeNeo;

boolean UpdateRideDataBool = true;
boolean blinker = true;

timeval cbtime; // time set in callback
bool cbtime_set = false;
timeval tv;
timespec tp;
time_t now;

uint32_t colorArray[PixelCount];

void time_is_set(void)
{
    gettimeofday(&cbtime, NULL);
    cbtime_set = true;
    Serial.println("------------------ settimeofday() was called ------------------");
}

void setStripColor(uint16_t pixel, uint32_t c)
{
    RgbColor color_tmp = (RgbColor)HtmlColor((uint32_t)c);
    color_tmp = colorGamma.Correct(color_tmp);
    strip.SetPixelColor(pixel, color_tmp);
}

void setAllStripColor()
{
    for (uint16_t i = 0; i < PixelCount; i++)
    {
        setStripColor(i, colorArray[i]);
    }
}

void clearAllData(){
    for (uint16_t i = 0; i < PixelCount; i++)
    {
        colorArray[i] = BLACK;
    }
}

void convertDataToColor(uint8_t index, boolean variant, int predmin)
{
    if (variant)
    {
        colorArray[index * 5] = BLUE;
    }
    else
    {
        colorArray[index * 5] = MAGENTA;
    }
    predmin = predmin % 61;
    int greenData = predmin / 15;
    int remaiData = predmin % 15;
    for (uint16_t i = 0; i < greenData; i++)
    {
        colorArray[index * 5 + 1 + i] = GREEN;
    }
    if (remaiData > 7)
    {
        colorArray[index * 5 + 1 + greenData] = YELLOW;
    }
    else
    {
        colorArray[index * 5 + 1 + greenData] = RED;
    }
}

void blinkBlue()
{
    if (blinker)
    {
        for (uint16_t i = 0; i < PixelCount; i++)
        {
            setStripColor(i, BLUE);
        }
    }
    else
    {
        strip.ClearTo((RgbColor)HtmlColor(BLACK));
    }
    strip.Show();
    blinker = !blinker;
}

boolean UpdateRideDataFn(uint8_t rt = BUS_ROUTE, uint16_t stpid = DAY_STOP)
{
    HTTPClient http;
    // USE_SERIAL.print("[HTTP] begin...\n");
    String http_url = "http://rt.theride.org/bustime/api/v3/getpredictions?key=" + RIDE_API +
                      "&format=json&rt=" + rt +
                      "&stpid=" + stpid +
                      "&top=3";
    http.begin(http_url);
    // USE_SERIAL.print("[HTTP] GET...\n");
    int httpCode = http.GET();

    if (httpCode > 0)
    {
        // USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();  //JSON reply stored as String
            // USE_SERIAL.println(payload);
            http.end();
            clearAllData();

            // const size_t bufferSize = JSON_ARRAY_SIZE(4) + 2 * JSON_OBJECT_SIZE(1) + 4 * JSON_OBJECT_SIZE(16) + 1500;
            // DynamicJsonDocument jsonBuffer(bufferSize);
            DynamicJsonDocument jsonBuffer;
            DeserializationError error = deserializeJson(jsonBuffer, payload);
            if (error)
            {
                USE_SERIAL.print("Parsing JSON error: ");
                USE_SERIAL.println(error.c_str());
            }
            JsonObject root = jsonBuffer.as<JsonObject>();
            JsonArray bustime_response_prd = root["bustime-response"]["prd"];

            DynamicJsonDocument jsonBuffer2;
            JsonObject outJSON = jsonBuffer2.to<JsonObject>();
            outJSON["time"] = now;
            JsonArray outJSONArr = outJSON.createNestedArray("data");

            for (uint8_t i = 0; i < bustime_response_prd.size(); i++)
            {
                String des = bustime_response_prd[i]["des"].as<String>();
                String pred = bustime_response_prd[i]["prdctdn"].as<String>();
                int predictedArrival = pred.toInt();
                des = des.substring(0, 1);
                USE_SERIAL.print("4");
                USE_SERIAL.print(des);
                USE_SERIAL.print(" ");
                USE_SERIAL.print(predictedArrival);
                USE_SERIAL.print(" min // ");

                convertDataToColor(i, des == "A", predictedArrival);
                JsonObject nested = outJSONArr.createNestedObject();
                nested["route"] = String(rt) + String((des =="A") ? "A" : "B");
                nested["prediction"] = predictedArrival;
            }
            serializeJson(outJSON, USE_SERIAL);
            char buffer[measureJson(outJSON) + 1];
            serializeJson(outJSON, buffer, sizeof(buffer));
            client.publish(MQTT_TOPIC, buffer, true);

            USE_SERIAL.println();

            return true;
        }

        http.end();
        return false;
    }
    else
    {
        USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }

    http.end();
    return false;
}

void sendReqTheRide()
{
    UpdateRideDataBool = true;
}

void fade_out()
{
    for (uint16_t i = 0; i < PixelCount; i++)
    {
        RgbColor rgbcolortmp = strip.GetPixelColor(i);
        RgbColor color_tmp((uint8_t)(rgbcolortmp.R / 2), (uint8_t)(rgbcolortmp.G / 2), (uint8_t)(rgbcolortmp.B / 2));
        strip.SetPixelColor(i, color_tmp);
    }
}

uint8_t fadeCounter = 0;

void fadeNeoRing()
{
    if (!fadeCounter)
    {
        setAllStripColor(); //dont fade_out()
    }
    else
    {
        fade_out();
    }
    fadeCounter++;
    fadeCounter = fadeCounter % NO_STEPS;
    strip.Show();
}

void mqtt_reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        USE_SERIAL.print("Attempting MQTT connection...");
        if (client.connect(HOSTNAME, MQTT_USER, MQTT_PASS))
        {
            USE_SERIAL.println("connected");
        }
        else
        {
            USE_SERIAL.print("failed, rc=");
            USE_SERIAL.print(client.state());
            USE_SERIAL.println(" try again in 5 seconds");
            delay(5000); // Wait 5 seconds before retrying
        }
    }
}

void setup()
{

    USE_SERIAL.begin(115200);
    // USE_SERIAL.setDebugOutput(true);
    settimeofday_cb(time_is_set);

    strip.Begin();
    strip.Show();
    BlinkBlue.attach(1.0, blinkBlue);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    for (uint8_t t = 4; t > 0; t--)
    {
        USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

    configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

    WiFi.mode(WIFI_STA);
    WiFi.hostname(HOSTNAME);
    WiFi.disconnect();
    delay(100);

    // Attempt to connect to Wifi network:
    USE_SERIAL.print("Connecting Wifi: ");
    USE_SERIAL.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        USE_SERIAL.println("Connection Failed! Rebooting...");
        delay(5000);
        ESP.restart();
    }

    // WiFiManager wifiManager;
    // wifiManager.autoConnect(HOSTNAME);

    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
            type = "sketch";
        }
        else
        { // U_SPIFFS
            type = "filesystem";
        }
        USE_SERIAL.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() { USE_SERIAL.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { USE_SERIAL.printf("Progress: %u%%\r", (progress / (total / 100))); });
    ArduinoOTA.onError([](ota_error_t error) {
        USE_SERIAL.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
        {
            USE_SERIAL.println("Auth Failed");
        }
        else if (error == OTA_BEGIN_ERROR)
        {
            USE_SERIAL.println("Begin Failed");
        }
        else if (error == OTA_CONNECT_ERROR)
        {
            USE_SERIAL.println("Connect Failed");
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
            USE_SERIAL.println("Receive Failed");
        }
        else if (error == OTA_END_ERROR)
        {
            USE_SERIAL.println("End Failed");
        }
    });
    ArduinoOTA.begin();
    USE_SERIAL.println("");
    USE_SERIAL.println("WiFi connected");
    USE_SERIAL.println("IP address: ");
    IPAddress ip = WiFi.localIP();
    Serial.println(ip);

    client.setServer(MQTT_SERVER, MQTT_PORT);

    BlinkBlue.detach();
    UpdateRideData.attach(updateInterval, sendReqTheRide);
    fadeNeo.attach_ms(1000/NO_STEPS, fadeNeoRing);
}

extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);

void loop()
{
    gettimeofday(&tv, nullptr);
    clock_gettime(0, &tp);
    now = time(nullptr);
    ArduinoOTA.handle();
    if (!client.connected())
    {
        mqtt_reconnect();
    }
    client.loop();
    if (UpdateRideDataBool)
    {
        const tm *tm = localtime(&now);
        uint8_t time_hr = (uint8_t)tm->tm_hour;
        USE_SERIAL.print(time_hr);
        USE_SERIAL.print(" // ");

        if (time_hr >= 0 and time_hr < 12)
        {
            UpdateRideDataFn(BUS_ROUTE, DAY_STOP);
        }
        else
        {
            UpdateRideDataFn(BUS_ROUTE, NIGHT_STOP);
        }
        UpdateRideDataBool = false;
    }
}
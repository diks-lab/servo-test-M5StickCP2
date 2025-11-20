#include <Arduino.h>
#include <M5Unified.h>
// #include <M5StickCPlus2.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Adafruit_PWMServoDriver.h>
#include "ssid.h"

#define dT_SECOND 1000000

// ----- Function prototypes
void webConnectStatus(uint8_t num, WStype_t type);
void onWebSocketCallback(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void infoBatteryLevel(void);
void openningGuidance(void);
void powerOffGuidance(void);
void panic_message(String msg);

// ----- Definitions and variables
// -- Web server
AsyncWebServer httpServer(80);
WebSocketsServer wsServer = WebSocketsServer(81);

// -- WiFi
char hostName[] = "st10";

#define USMIN 500     // This is the 'minimum' microsecond
#define USMAX 2400    // This is the 'maximum' microsecond
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
#define SERVO_PIN 26
#define CHANNEL 0

uint16_t period = 1000000 / SERVO_FREQ;
uint16_t pulse = (USMAX - USMIN) / 2 + USMIN;
uint32_t hNow = micros();

void setup()
{
  auto cfg = M5.config();
  cfg.internal_imu = false;
  //  StickCP2.begin(cfg);
  M5.begin(cfg);

  M5.Lcd.setRotation(3);

  openningGuidance();

  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }

  // LittleFS setup
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS mount failed");
    panic_message("LittleFS mount failed.");
    return;
  }
  else
  {
    Serial.println("LittleFS mount success");
  }

  // Wifi setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_NAME, SSID_KEY);
  if (!(WiFi.waitForConnectResult() != WL_CONNECTED))
  {
    Serial.print("Connected to WiFi with IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Could not connect to known WiFi network");
    panic_message("Could not connect to WiFi.");
    return;
  }

  // Start DNS server
  if (MDNS.begin(hostName))
  {
    Serial.print("MDNS responder started, name: ");
    Serial.println(hostName);
  }
  else
  {
    Serial.println("Could not start MDNS responder");
  }

  httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                {
        Serial.println("Loading index.html");
        request->send(LittleFS, "/index.html"); });

  httpServer.serveStatic("/", LittleFS, "/");
  httpServer.onNotFound([](AsyncWebServerRequest *request)
                        { request->send(404, "text/plain", "FileNotFound"); });

  httpServer.begin();

  wsServer.begin();
  wsServer.onEvent(onWebSocketCallback);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setFont(&fonts::lgfxJapanGothic_16);
  M5.Lcd.setTextColor(LIGHTGREY);
  M5.Lcd.setCursor(2, M5.Display.height() - 18);
  M5.Lcd.print("[M5/OFF]");
  M5.Lcd.setCursor(150, M5.Display.height() - 18);
  M5.Lcd.print("[Batt]");

  M5.Lcd.setFont(&fonts::lgfxJapanGothic_20);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.printf("IP: %s", WiFi.localIP().toString().c_str());

  M5.Lcd.setCursor(0, 30);
  M5.Lcd.printf("Period:");
  M5.Lcd.setCursor(82, 30);
  M5.Lcd.printf("%5.2f mSec\n", period / 1000.);
  M5.Lcd.setCursor(0, 55);
  M5.Lcd.println("Pulse :");
  M5.Lcd.setCursor(82, 55);
  M5.Lcd.printf("%5.2f mSec\n", pulse / 1000.);

  infoBatteryLevel();

  ledcSetup(CHANNEL, SERVO_FREQ, 10);

  ledcAttachPin(SERVO_PIN, CHANNEL);

  Serial.println("Ready");
}

void loop()
{
  static uint32_t tLast = 0;
  uint32_t tNow = micros();

  M5.update();
  if (M5.BtnA.wasPressed())
  {
    powerOffGuidance();
  }
  if (M5.BtnB.wasPressed())
  {
    ESP.restart();
  }
  if (tNow - tLast > dT_SECOND * 300)
  {
    infoBatteryLevel();
    tLast = tNow;
  }

  wsServer.loop();
}

void webConnectStatus(uint8_t num, WStype_t type)
{
  char wBuf[10];

  Serial.printf("[%u] connected status!\n", num);
  IPAddress ip = wsServer.remoteIP(num);
  Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);

  switch (type)
  {
  case WStype_DISCONNECTED:
    M5.Lcd.fillRect(M5.Display.width() - 50, 3, 44, 22, TFT_BLACK);
    break;
  case WStype_CONNECTED:
    M5.Lcd.fillRect(M5.Display.width() - 50, 3, 44, 22, TFT_GREEN);
    M5.Lcd.setTextColor(BLACK, GREEN);
    M5.Lcd.setCursor(M5.Display.width() - 46, 4);
    M5.Lcd.println("CONN");
    M5.Lcd.setTextColor(WHITE, BLACK);

    sprintf(wBuf, "w%d", pulse);
    wsServer.sendTXT(num, wBuf);
    break;
  }
}

void onWebSocketCallback(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  char wBuf[10];

  char *data = (char *)payload;
  hNow = micros();

  switch (type)
  {
  case WStype_DISCONNECTED:
  case WStype_CONNECTED:
    webConnectStatus(num, type);
    break;
  case WStype_TEXT:
    Serial.printf("[%u] get Text: %s\n", num, payload);
    if ((data[length - 1] == 'x') && length >= 3)
    {
      memset(wBuf, 0, sizeof(wBuf));
      memmove(wBuf, data, length - 1);
      switch (data[0])
      {
      case 'm':
        uint8_t num = 0;
        pulse = atoi(wBuf + 1);
        float pwmValue = (((float)pulse * 1024.) / 20000.) + 0.5;
        ledcWrite(CHANNEL, (uint32_t)pwmValue);
        M5.Lcd.fillRect(82, 55, 60, 22, TFT_BLACK);
        M5.Lcd.setCursor(82, 55);
        M5.Lcd.setFont(&fonts::lgfxJapanGothic_20);
        M5.Lcd.printf("%5.2f", pulse / 1000.);
        // Serial.printf("num:%d  pulse:%4d uSec  pwm:%d\n", num, pulse, (uint32_t)pwmValue);
      }
    }
    break;
  case WStype_BIN:
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    break;
  }
}

void infoBatteryLevel()
{
  uint8_t BatteryLevel = M5.Power.getBatteryLevel();
  //  Serial.println(BatteryLevel);

  M5.Lcd.fillRect(200, M5.Display.height() - 18, M5.Display.width() - 200, 18, TFT_BLACK);
  M5.Lcd.setFont(&fonts::lgfxJapanGothic_16);
  M5.Lcd.setTextColor(LIGHTGREY);
  M5.Lcd.setCursor(200, M5.Display.height() - 18);
  M5.Lcd.printf("%3d%%", BatteryLevel);
}

void openningGuidance()
{

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setTextDatum(middle_center);
  M5.Lcd.setFont(&fonts::Orbitron_Light_24);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString("Servo Test", M5.Lcd.width() / 2,
                    M5.Lcd.height() / 2);
  M5.Lcd.setTextDatum(top_left);
}

void powerOffGuidance()
{
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setTextDatum(middle_center);
  M5.Lcd.setFont(&fonts::Orbitron_Light_24);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString("Power OFF", M5.Lcd.width() / 2,
                    M5.Lcd.height() / 2);

  delay(2000);
  M5.Power.powerOff(); // shutdown
}

void panic_message(String msg)
{
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setFont(&fonts::lgfxJapanGothic_16);
  M5.Lcd.setTextColor(WHITE, RED);
  M5.Lcd.setCursor(10, M5.Display.height() / 2 - 10);
  M5.Lcd.println(msg);
  while (1)
  {
    delay(10);
  }
}
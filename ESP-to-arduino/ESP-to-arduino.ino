//----------Thu vien bo nho va du lieu----------
#include <EEPROM.h>
#include <ArduinoJson.h>

//----------Thu vien WiFi va Web----------
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>

//----------Thu vien debugArduino----------
#include "debugArduino.h"
#define DEBUGLEVEL DEBUGLEVEL_DEBUGGING

//----------Khai bao cac bien giao tiep voi server----------
#define WS_HOST "socket-dev.e-gate.vn"
#define WS_PORT 443
#define WS_URL "/socket.io/?token=123456789&egcode=E1-XYZ21M01-001&eghash=12345678911112131415&EIO=3"
#define PRODUCT_CODE "E1-XYZ21M01-001"
#define WS_PROTOCOL "arduino"

//----------Khai bao cac doi tuong----------
SocketIOclient webSocket;
WiFiManager wifiManager;
WebSocketsClient wsClient;

//----------Khai bao cac bien logic----------
bool wifiNonBlockingF = false;

//----------Cac bien lien quan toi don hang----------
int orderNumber = 0;

void setup() {
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  Serial.setTimeout(1000);
  Serial.println("$L^");

  Serial.setDebugOutput(true);
  wifiActive();
  webSocket.setReconnectInterval(5000);

  Serial.println("$M^");
  
}
void loop() {
  webSocket.loop();
  
}

void serialEvent() {
  while (Serial.available() && Serial.read() == '@') {
    String fromArduino = Serial.readStringUntil('&');

    if (fromArduino.equals("PORTAL")) {
      debuglnD("Bat che do Portal");
    } else if (fromArduino.equals("RSTESP")) {
      debuglnD("Khoi dong lai ESP");
      ESP.restart();
    } else if (fromArduino.equals("asyncOK")) {
      debuglnD("Dong bo don hang thanh cong");
    } else if (fromArduino.equals("UserpassOK")) {
      debuglnD("Dong bo mat khau thanh cong");
    }
  }
}

void wifiActive() {
  debuglnD("WiFi active start...");
  
  wifiManager.setWiFiAutoReconnect(true);
  if (wifiNonBlockingF) wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConnectRetries(5);
  wifiManager.setTimeout(120);
  wifiManager.getWiFiIsSaved(); //Kiem tra WiFi da duoc luu chua
  
  std::vector<const char *> menu = {"wifi", "info", "param", "sep", "restart", "exit"};
  wifiManager.setMenu(menu);

  wifiManager.setClass("invert");
  String ip = (String)WiFi.localIP();
  if (wifiManager.autoConnect(PRODUCT_CODE)) {
    Serial.printf("[SETUP] WiFi Connected %s\n", ip.c_str());
  } else {
    Serial.println("Config portal running...");
  }
  Serial.println("$H^");

  webSocket.beginSSL(WS_HOST, WS_PORT, WS_URL, WS_PROTOCOL);
  webSocket.onEvent(socketIOEvent);
}

void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t _length) {
  debugD("Event's type: ");
  debuglnD(type);
  
  switch(type) {
    case sIOtype_DISCONNECT:
      debuglnD("[IOc] Disconnected");
      break;
    case sIOtype_CONNECT:
      debuglnD("[IOc] Connected");
      break;
    case sIOtype_EVENT: {
      StaticJsonDocument<1024> doc;
      DeserializationError err = deserializeJson(doc, payload, _length);
      if (err) {
        debugD("deserializeJson() failed: ");
        debuglnD(err.c_str());
        return;
      }

      String eventName = doc[0];
      debugD("[IOc] event name: ");
      debuglnD(eventName.c_str());

      if (eventName.equals("801")) {
        debuglnD("Hay kiem tra thiet bi");
      }
      else if (eventName.equals("802")) {
        debuglnD("Hay gui trang thai cua thiet bi");
      }
      else if (eventName.equals("803")) {
        debuglnD("Gui mat khau cho arduino");
        Serial.flush();
        Serial.print("$");
        serializeJson(doc[1], Serial);
        Serial.println("^");
        Serial.flush();
      }
      else if (eventName.equals("804")) { //Dieu khien dong mo cua
        int key = doc[1]["key"];
        bool value = doc[1]["status"];
          
        if (key == 101 && value) {
          Serial.println("$A^");
        } else if (key == 101 && !value) {
          Serial.println("$B^");
        } else if (key == 102 && value) {
          Serial.println("$C^");
        } else if (key == 102 && !value) {
          Serial.println("$D^");
        }
      }
      else if (eventName.equals("805")) {
        debuglnD("Dong bo don hang");

        orderNumber = doc[1]["codes"]["num"];
        StaticJsonDocument<100> docNum;
        docNum["orderNumber"] = orderNumber;
        Serial.flush();
        Serial.print("$");
        serializeJson(docNum, Serial);
        Serial.println("^");
        Serial.flush();

        for (int i = 0; i < orderNumber; i++) {
          String index = (String)(i + 201);
          StaticJsonDocument<100> docOrder;
          docOrder[index] = doc[1]["codes"][index];

          Serial.flush();
          Serial.print("$");
          serializeJson(docOrder, Serial);
          Serial.println("^");
          Serial.flush();
          delay(50);
        }
      } 
      else if (eventName.equals("807")) {
        debuglnD("Dong bo thoi gian");
        Serial.flush();
        Serial.print("$");
        serializeJson(doc[1], Serial);
        Serial.println("^");
        Serial.flush();
      }
      else if (eventName.equals("809")) {
        debuglnD("Gui master-key cho arduino");
      }
    }
      break;
    case sIOtype_ACK:
      debugD("[IOc] get ack: ");
      debuglnD(_length);
      break;
    case sIOtype_ERROR:
      debugD("[IOc] get error: ");
      debuglnD(_length);
      break;
    case sIOtype_BINARY_EVENT:
      debugD("[IOc] get binary: ");
      debuglnD(_length);
      break;
    case sIOtype_BINARY_ACK:
      debugD("[IOc] get binary ack: ");
      debuglnD(_length);
      break;
  }
}

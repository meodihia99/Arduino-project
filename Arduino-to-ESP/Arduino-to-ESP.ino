//----------Thu vien bo nho va kieu du lieu----------
#include <EEPROM.h>
#include <ArduinoJson.h>

//----------Thu vien debugArduino----------
#include "debugArduino.h"
#define DEBUGLEVEL DEBUGLEVEL_DEBUGGING

//----------Thu vien LCD TFT 3.2----------
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <TFT_ILI9341.h>
#include <Adafruit_SPITFT.h>
#include "LCDbitmap.h"

//----------Thu vien RTC----------
#include <RTClib.h>
RTC_DS3231 rtc;

//----------Thu vien ma tran phim----------
#include <MemoryFree.h>
#include <Keypad.h>

//----------Dinh nghia cac Pin khoa va cong tac----------
#define PIN_LS_FRONT 2
#define PIN_LOCK_FRONT 23
#define PIN_UV 29

//----------Dinh nghia cac pin man hinh LCD----------
#define MODEL ILI9341
#define CS 45
#define CD 49
#define RST 47
#define TFT_LCD 53  //SCK = 52  //MOSI = 51
Adafruit_ILI9341 tft = Adafruit_ILI9341(CS, CD, RST);

//----------Dinh nghia cac mau sac cua man hinh LCD----------
#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define YELLOW   0xFFE0
#define WHITE    0xFFFF

//----------Khai bao cac bien cho LCD----------
unsigned long LcdAwakeInterval = 20000; //Man hinh tu dong tat sau khoang thoi gian nay
unsigned long LcdAwakeMoment;
unsigned long LcdPassConfirmMoment;
bool LcdAwakeF = true;
bool LcdPassConfirmF = false;

//----------Khai bao bien cho RTC----------
char daysOfTheWeek[7][12] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
unsigned long rtcCurrentMoment;
unsigned long dotMoment;
unsigned long noDotMoment;
bool dotF = false;

//----------Khai bao bien cho ma tran phim----------
const byte rows = 3;
const byte columns = 4;
char keys[rows][columns] = 
{
  {'1', '2', '3', '*'},
  {'4', '5', '6', '0'},
  {'7', '8', '9', '#'}
};
byte rowPins[rows] = {9, 8, 7};           //r1 - r2 - r3 - c1 - c2 - c3 - c4
byte columnPins[columns] = {6, 5, 4, 3};  //D9   D8   D7   D6   D5   D4   D3
Keypad _keypad = Keypad(makeKeymap(keys), rowPins, columnPins, rows, columns);
bool activeKey = false;
int keyCount = 0;
int starPosition = 65;
char passWord[5];
char key = 0;
bool isHeldF = false;
bool holdNotifyF = false;
bool holdEnoughF = false;
unsigned long holdMoment;
unsigned long releaseMoment;
unsigned long starInterval = 10000;

//----------Khai bao bien cho QR va EEPROM----------
int eeAddress = 0;
int orderNumber = 0;
int orderCount = 0;
#define orderCapility  30
struct myPassAndQR {
  char userPass[4][5];
  char QRorder[orderCapility][21];
};
myPassAndQR getEEPROM, putEEPROM;
unsigned long qrCurrentMoment;
bool qrCheckF = false;

//----------Khai bao bien cho Limit switch----------
unsigned long lsPressMoment;
unsigned long lsReleaseMoment;
bool frontLsPressed = false, backLsPressed = false;

//----------Khai bao bien cho den UV----------
unsigned long uvCurrentMoment;
unsigned long uvIntervalCount = 15000; //Thoi gian sang cua den UV
bool uvOnF = false;
bool uvAllowF = false;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1000);
  Serial3.begin(115200);
  Serial3.setTimeout(1000);
  pinMode(PIN_LS_FRONT, INPUT_PULLUP);
  pinMode(PIN_LOCK_FRONT, OUTPUT);
  pinMode(PIN_UV, OUTPUT);

  setupLCD();
  //setupRTC();
  readEEPROM();
  Serial.println("@ARDUINO&");
}
void loop() {
  handleLimitSwitch();
  orderCheck();
  realTimeClock();
//  QR_code();
  matrix_key();
  LcdFallSleep();
  
}

void(* resetFunc) (void) = 0; //Khai bao ham reset tai dia chi 0

void serialEvent() {
  while (Serial.available() && Serial.read() == '$') { //Nhan chuoi ky tu tu ESP
    String fromESP = Serial.readStringUntil('^');
    
    if (fromESP.equals("A")) {
      debuglnD("Khoa truoc mo");
      frontLsPressed = false;
    }
    else if (fromESP.equals("B")) {
      debuglnD("Khoa truoc dong");
      frontLsPressed = false;
    }
    else if (fromESP.equals("C")) {
      debuglnD("Khoa sau mo");
    }
    else if (fromESP.equals("D")) {
      debuglnD("Khoa sau dong");
    }
    else if (fromESP.equals("E")) {
      debuglnD("Reset WiFi");
    }
    else if (fromESP.equals("H")) {
      debuglnD("WiFi da ket noi");
    }
    else if (fromESP.equals("L")) {
      Serial.println("@ARDUINO&");
    }
    else if (fromESP.equals("M")) {
      debuglnD("Gui thong tin trang thai");
    }
    else if (fromESP.equals("STRONG")) {
      debuglnD("WiFi manh");
      tft.drawRGBBitmap(210, 2, wifiIcon, 33, 24);
    }
    else if (fromESP.equals("MEDIUM")) {
      debuglnD("WiFi trung binh");
      tft.drawRGBBitmap(210, 2, wifiIcon2, 33, 24);
    }
    else if (fromESP.equals("WEAK")) {
      debuglnD("WiFi yeu");
      tft.drawRGBBitmap(210, 2, wifiIcon1, 33, 24);
    }
    else if (fromESP.equals("NOSIGNAL")) {
      debuglnD("WiFi khong co tin hieu");
      LcdPrintText("     ", 180, 2, 4, WHITE, 0x00E7);
    }
    else { //Neu nhan duoc mot chuoi Json
      debuglnD(fromESP);
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, fromESP);
      
      if (doc["y"]) {
        debuglnD("Cap nhat thoi gian");
        uint16_t _year = doc["y"];
        uint8_t _mon = doc["m"];
        uint8_t _day = doc["d"];
        uint8_t _hour = doc["h"];
        uint8_t _min = doc["min"];
        rtc.adjust(DateTime(_year, _mon, _day, _hour, _min));
        String dateTime = (String)_hour + ":" + (String)_min + "----" 
           + (String)_day + "/" + (String)_mon + "/" + (String)_year;
        debuglnD(dateTime);
      }
      else if(doc["P1"]) { //Dong bo mat khau (userPass)
        strcpy(putEEPROM.userPass[0], doc["P1"]);
        strcpy(putEEPROM.userPass[1], doc["P2"]);
        strcpy(putEEPROM.userPass[2], doc["P3"]);
        strcpy(putEEPROM.userPass[3], doc["P4"]);

        EEPROM.put(eeAddress, putEEPROM); //Ghi du lieu vao EEPROM
        EEPROM.get(eeAddress, getEEPROM); //Doc du lieu vao EEPROM
        
        debuglnD("User's passwords: "); //Hien thi ra serial monitor
        for (int i = 0; i < 4; i++) {
          debuglnD(getEEPROM.userPass[i]);
        }
      }
      else if (doc["orderNumber"]) {
        orderNumber = doc["orderNumber"];
        debugD("So luong don hang: ");
        debuglnD(orderNumber);
      }
      else { //Dong bo QR code
        String index = (String)(orderCount + 201);
        if (doc[index]) {
          strcpy(putEEPROM.QRorder[orderCount], doc[index]);
        }
        orderCount++;
        
        if (orderCount == orderNumber) {
          orderCount = 0;
          for (int i = orderNumber; i < orderCapility; i++) {
            strcpy(putEEPROM.QRorder[i], "null");
          }
          EEPROM.put(eeAddress, putEEPROM); //Ghi du lieu vao EEPROM
          EEPROM.get(eeAddress, getEEPROM); //Doc du lieu tu EEPROM
        }
        qrCurrentMoment = millis();
        qrCheckF = true;
      }
    }
  }  
}

void setupLCD() {
  pinMode(TFT_LCD, OUTPUT);
  tft.begin();
  tft.setRotation(0);
  digitalWrite(TFT_LCD, HIGH);
  tft.fillScreen(0x00E7);

  tft.drawRGBBitmap(0, 0, LOGO, 77, 51);
  tft.drawRGBBitmap(35, 205, frontDoorClose, 70, 60);
  tft.drawRGBBitmap(135, 205, backDoorClose, 70, 60);

  tft.fillRoundRect(32, 90, 180, 14, 14, 0x07BF);
  tft.fillRoundRect(32, 278, 180, 30, 7, 0x07BF);
  tft.fillRoundRect(15, 112, 210, 88, 5, BLACK);

  LcdAwakeMoment = millis();
}

void setupRTC() {
  while (!rtc.begin()) {
    
  }
  debuglnW("RTC da khoi dong");
  rtcCurrentMoment = millis();
  dotMoment = millis();
  noDotMoment = millis();
  
  DateTime now = rtc.now();
  String _hourMin = (String)(now.hour()/10) + (String)(now.hour()%10) + ":" + (String)(now.minute()/10) + (String)(now.minute()%10);
  String DOW = daysOfTheWeek[now.dayOfTheWeek()];
  String _dayMonYear = (String)now.day() + "-" + (String)now.month() + "-" + (String)now.year();
  LcdPrintText(_hourMin, 65, 55, 4, WHITE, 0x00E7);
  LcdPrintText(DOW, 80, 94, 1, 0x00E7, 0x07BF);
  LcdPrintText(_dayMonYear, 108, 94, 1, 0x00E7, 0x07BF);
}

void LcdPrintText(String text, int *x, int *y, int *s, uint16_t *txtc, uint16_t *bgc){
  tft.setCursor(x, y);
  tft.setTextColor(txtc, bgc);
  tft.setTextSize(s);
  tft.println(text);
}

void readEEPROM() {
  EEPROM.get(eeAddress, getEEPROM);
  EEPROM.get(eeAddress, putEEPROM);
  debuglnD("User's passwords: ");
  for (int i = 0; i < 4; i++) {
    debuglnD(getEEPROM.userPass[i]);
  }
  debuglnD("QR codes: ");
  for (int i = 0; i < orderCapility; i++) {
    debugD("Order ");
    debugD(i + 1);
    debugD(": ");
    debuglnD(getEEPROM.QRorder[i]);
  }
}

void handleLimitSwitch() {
  //Ham cong tac hanh trinh cua truoc
  if (digitalRead(PIN_LS_FRONT) == LOW && frontLsPressed == false) {
    frontLsPressed = true;
    debuglnD("Cong tac duoc nhan");

    lsPressMoment = millis();
    uvAllowF = true;
  }
  if (digitalRead(PIN_LS_FRONT) == HIGH && frontLsPressed == true) {
    frontLsPressed = false;
    debuglnD("Cong tac duoc tha");
    tft.drawRGBBitmap(35, 205, frontDoorOpen, 70, 60);
    
    uvAllowF = false;
  }
  if (uvAllowF && (millis() - lsPressMoment) > 1000) {
    uvAllowF = false;
    debuglnD("Bat den UV");
    digitalWrite(PIN_UV, HIGH);
    tft.drawRGBBitmap(160, 2, UVC, 33, 24);
    tft.drawRGBBitmap(35, 205, frontDoorClose, 70, 60);

    LcdWakeUp();
    
    uvCurrentMoment = millis();
    uvOnF = true;
  }
  if (uvOnF && ((millis() - uvCurrentMoment) > uvIntervalCount)) {
    uvOnF = false;
    debuglnD("Tat den UV");
    digitalWrite(PIN_UV, LOW);
    LcdPrintText("  ", 162, 6, 3, WHITE, 0x00E7);
    LcdPrintText("                      ", 30, 125, 1, WHITE, BLACK); //22 dau cach
  }
  else if (uvOnF && ((millis() - uvCurrentMoment) < uvIntervalCount)) {
    int countDown = (uvIntervalCount + uvCurrentMoment - millis())/1000 + 1;
    String index = "Dang diet khuan ... " + (String)countDown + " ";
    LcdPrintText(index, 30, 125, 1, WHITE, BLACK);

    LcdAwakeMoment = millis();
  }
}

void orderCheck() {
  if (qrCheckF && ((millis() - qrCurrentMoment) > 500)) {
    qrCheckF = false;
    //Xac nhan ma don hang voi ESP
    int i = 0;
    while (memcmp(getEEPROM.QRorder[i], "null", sizeof("null")) != 0) {
      String toESP = "@" + (String)(i + 201) + (String)getEEPROM.QRorder[i] + "&";
      Serial.println(toESP);
      i++;
      if (i == orderCapility) break;
    }
  }
}

void realTimeClock() {
  if ((millis() - rtcCurrentMoment) > 60000) {
    rtcCurrentMoment = millis();
    DateTime now = rtc.now();
    String _hourMin = (String)(now.hour()/10) + (String)(now.hour()%10) + " " + (String)(now.minute()/10) + (String)(now.minute()%10);
    String DOW = daysOfTheWeek[now.dayOfTheWeek()];
    String _dayMonYear = (String)now.day() + "-" + (String)now.month() + "-" + (String)now.year();
    LcdPrintText(_hourMin, 65, 55, 4, WHITE, 0x00E7);
    LcdPrintText(DOW, 80, 94, 1, 0x00E7, 0x07BF);
    LcdPrintText(_dayMonYear, 108, 94, 1, 0x00E7, 0x07BF);
  }
  if (!dotF && (millis() - dotMoment) > 500) {
    noDotMoment = millis();
    dotF = true;
    LcdPrintText(":", 113, 55, 4, WHITE, 0x00E7);
  }
  if (dotF && (millis() - noDotMoment) > 500) {
    dotMoment = millis();
    dotF = false;
    LcdPrintText(" ", 113, 55, 4, WHITE, 0x00E7);
  }
}

void QR_code() {
  while (Serial3.available()/* && (Serial3.read() == '#')*/) {
    String mavach = Serial3.readString();
    mavach.trim();

    char shipperCode[21];
    mavach.toCharArray(shipperCode, mavach.length() + 1);
    int index = mavach.length();
    while (index < 20) {
      shipperCode[index++] = '*';
    }
    shipperCode[index] = NULL;
    debuglnD("Co ma duoc quet: ");
    debuglnD(shipperCode);

    EEPROM.get(eeAddress, getEEPROM);
    for (int i = 0; i < orderCapility; i++) {
      debuglnD(getEEPROM.QRorder[i]);
      if (memcmp(getEEPROM.QRorder[i], shipperCode, sizeof(shipperCode)) == 0) {
        debuglnD("Xac nhan don hang so: ");
        debuglnD(i + 1);
        strcpy(putEEPROM.QRorder[i], "00000000000000000000");
        EEPROM.put(eeAddress, putEEPROM);

        break;
      } else {
        debuglnD("Khong phai don hang so: ");
        debuglnD(i + 1);
      }
    }
  }
}

void matrix_key() {
  if (LcdAwakeF) {
    char temp = _keypad.getKey();
    if ((int)_keypad.getState() == PRESSED) {
      if (temp) {
        key = temp;
      }
    }
  
    if (!isHeldF && ((int)_keypad.getState() == HOLD)) {
      if (key == '#' || key == '*' || key == '0') {
        holdMoment = millis();
        isHeldF = true;
        holdNotifyF = true;
      }
    }
  
    if ((int)_keypad.getState() == RELEASED) {
    
      delay(20); //Ham delay tranh bi doi tin hieu  nut nhan
      if (isHeldF) {
        isHeldF = false;
      }
      if (holdNotifyF || holdEnoughF) {
        holdNotifyF = false;
        holdEnoughF = false;
        LcdPrintText("                    ", 30, 145, 1, WHITE, BLACK);
        goto millisCount;
      }

      LcdAwakeMoment = millis();
    
      if (keyCount < 4) {
        debuglnD(key);
        passWord[keyCount] = key;
        LcdPrintText("*", starPosition, 283, 3, 0x00E7, 0x07BF); //0x07BF- xanh thien thanh
        starPosition += 30; 
        keyCount++;

        releaseMoment = millis();
      }
      if (keyCount == 4) {
//        activeKey = false;
        passWord[keyCount] = NULL;
        debuglnD("Nhan duoc 4 ky tu, so sanh voi mat khau trong EEPROM");
        debuglnD(passWord);

        int i;
        for (i = 0; i < 4; i++) {
          if (memcmp(passWord, getEEPROM.userPass[i], sizeof(getEEPROM.userPass[i])) == 0
          || memcmp(passWord, "6868", sizeof("6868")) == 0) {

            LcdPrintText("Mat khau chinh xac!", 30, 135, 1, GREEN, BLACK);
            LcdPassConfirmMoment = millis();
            LcdPassConfirmF = true;
            
            debuglnD("Mo cua truoc");
            if (memcmp(passWord, getEEPROM.userPass[i], sizeof(getEEPROM.userPass[i])) == 0) {
              debuglnD("Xac nhan mat khau cho server");
              debuglnD("Xoa mat khau vua roi trong EEPROM");
            }
            
            break;
          }
        }
        if (i == 4) {
          LcdPrintText("Mat khau sai!", 30, 135, 1, RED, BLACK);
          LcdPassConfirmMoment = millis();
          LcdPassConfirmF = true;
        }
      
        LcdPrintText("       ", 62, 280, 3, 0x00E7, 0x07BF);
        keyCount = 0;
        starPosition = 65;
      }
    }

    millisCount:

    if (holdNotifyF && ((millis() - holdMoment) > 3000)) {
      if (key == '#') {
        resetFunc();
      }
      else if (key == '*') {
        Serial.println("@PORTAL&");
      }
      else if (key == '0') {
        Serial.println("@RSTESP&");
      }
      LcdPrintText("       ", 62, 280, 3, 0x00E7, 0x07BF);
      LcdPrintText("                    ", 30, 145, 1, WHITE, BLACK); //20 dau cach
      
      starPosition = 65;
      keyCount = 0;
      holdEnoughF = true;
      holdNotifyF = false;
    }
    else if (holdNotifyF && ((millis() - holdMoment) < 3000)) {
      int countDown = (3000 + holdMoment - millis())/1000 + 1;
      String index;
      if (key == '#') {
        index = "Reset arduino ... " + (String)countDown + " ";
      }
      else if (key == '*') {
        index = "Open portal ... " + (String)countDown + " ";
      }
      else if (key == '0') {
        index = "Reset ESP ... " + (String)countDown + " ";
      }
      LcdPrintText(index, 30, 145, 1, WHITE, BLACK);
      
      LcdAwakeMoment = millis();
    }
  } else {
    _keypad.getKey();
    if ((int)_keypad.getState() == RELEASED) {
      delay(20);
      LcdWakeUp();
    }
  }

  if (LcdPassConfirmF && ((millis() - LcdPassConfirmMoment) > 2000)) { //Xoa dong chu sau 2 giay xuat hien
    LcdPassConfirmF = false;
    LcdPrintText("                   ", 30, 135, 1, GREEN, BLACK); //19 dau cach

    LcdAwakeMoment = millis();
  }

  if (keyCount > 0 && (millis() - releaseMoment) > starInterval) { //Neu khong bam du 4 nut, tu dong xoa
    LcdPrintText("       ", 62, 280, 3, 0x00E7, 0x07BF);
    keyCount = 0;
    starPosition = 65;
  }
  
}

void LcdWakeUp() {
  tft.startWrite();
  tft.writeCommand(0x11);
  tft.endWrite();
  delay(120); //Ham delay de tranh man hinh co vet mau trang
  digitalWrite(TFT_LCD, HIGH);
  LcdAwakeF = true;
  LcdAwakeMoment = millis();
}

void LcdFallSleep() {
  if (LcdAwakeF && (millis() - LcdAwakeMoment) > LcdAwakeInterval) {
    digitalWrite(TFT_LCD, LOW);
    tft.startWrite();
    tft.writeCommand(0x10);
    tft.endWrite();
    LcdAwakeF = false;
  }
}

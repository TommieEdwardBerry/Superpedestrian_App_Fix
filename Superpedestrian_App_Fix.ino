#include "BoardDisplay.h"
#include "WheelConfig.h"
#include "WheelPresets.h"
#include <BLEDevice.h>
#include <atomic>

std::atomic<int> request{0}; // 0=read at boot, -1=down, 1=up, 9=idle
std::atomic<int> shownMode{-1}, statusCode{0};
std::atomic<bool> busy{true}, linked{false};
class WheelCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient*) override {}
  void onDisconnect(BLEClient*) override { linked=false; shownMode=-1; }
};
WheelCallbacks wheelCallbacks;
#include "WheelTelemetry.h"
const char* modeNames[]={"ECO","STANDARD","TURBO"};
const char* statusNames[]={"Connecting to Terry...","Ready - tap an arrow","Wheel unavailable; tap to retry","Wrong wheel identity","Authentication failed","Mode read failed","Unrecognized wheel mode","Mode write failed","Readback did not match","Touch needs calibration"};
const char* boardUuid="52756265-6e43-6167-6e69-654350485105";
const char* authUuid="52756265-6e43-6167-6e69-654350485101";
const char* accessUuid="52756265-6e43-6167-6e69-654350485104";
const char* modeUuid="52756265-6e43-6167-6e69-65435048500a";
int identifyMode(const String& value) {
  if(value.length()!=20)return -1;
  // Read responses may omit CRC; compare all 18 configuration bytes.
  for(int i=0;i<3;i++)if(memcmp(value.c_str(),modePackets[i],18)==0)return i;
  return -1;
}
void wheelTransaction(BLEClient* client,int direction) {
  bool newConnection=!client->isConnected();
  if(newConnection && !client->connect(BLEAddress(wheelAddress),BLE_ADDR_PUBLIC,8000)) {statusCode=2;return;}
  do {
    auto service=client->getService(BLEUUID("52756265-6e43-6167-6e69-654350485100"));
    auto control=client->getService(BLEUUID("52756265-6e43-6167-6e69-654350485000"));
    if(!service || !control){statusCode=3;break;}
    auto serial=service->getCharacteristic(BLEUUID(boardUuid));
    auto auth=service->getCharacteristic(BLEUUID(authUuid));
    auto access=service->getCharacteristic(BLEUUID(accessUuid));
    auto mode=control->getCharacteristic(BLEUUID(modeUuid));
    if(!serial || !auth || !access || !mode){statusCode=3;break;}
    String identity=serial->readValue();
    if(identity.length()<14 || memcmp(identity.c_str(),wheelBoardSerial,14)!=0){statusCode=3;break;}
    if(newConnection) {
    uint8_t passcode[16];memcpy(passcode,wheelPasscode,16);
    if(!auth->writeValue(passcode,16,true)){statusCode=4;break;}
    vTaskDelay(pdMS_TO_TICKS(1000));
    String level=access->readValue();
    // Only accept the exact access state verified during Windows testing.
    if(level.length()!=1 || uint8_t(level[0])!=1){statusCode=4;break;}
    }
    linked=client->isConnected();
    String current=mode->readValue();
    if(current.length()!=20){statusCode=5;break;}
    int index=identifyMode(current);
    shownMode=index;
    if(index<0){statusCode=6;break;}
    if(direction==0){statusCode=1;break;}
    int next=constrain(index+direction,0,2);
    if(next==index){statusCode=1;break;}
    uint8_t packet[20];memcpy(packet,modePackets[next],20);
    if(!mode->writeValue(packet,20,true)){statusCode=7;shownMode=-1;break;}
    vTaskDelay(pdMS_TO_TICKS(300));
    String actual=mode->readValue();
    int verified=identifyMode(actual);
    shownMode=verified;
    if(verified!=next){statusCode=8;break;}
    statusCode=1;
    Serial.printf("Verified mode: %s\n",modeNames[next]);
  }while(false);
  if(statusCode.load()!=1) {linked=false;client->disconnect();}
}
void bleWorker(void*) {
  BLEDevice::init("Terry_Handlebar");
  BLEClient* client=BLEDevice::createClient();
  client->setClientCallbacks(&wheelCallbacks);
  uint32_t lastCheck=0,lastAttempt=0;
  bool first=true;
  for(;;) {
    uint32_t now=millis();
    if(!client->isConnected()) {
      linked=false;shownMode=-1;
      loseTelemetry();
      if(request.exchange(9)==10){resetTrip();busy=false;}
      saveTrip(false);
      request=9; // Never replay a tap after losing the connection.
      if(first || uint32_t(now-lastAttempt)>=5000) {
        first=false;busy=true;
        wheelTransaction(client,0);
        lastAttempt=millis();lastCheck=lastAttempt;
        Serial.printf("Wheel: %s; mode=%d; connected=%d\n",statusNames[statusCode.load()],shownMode.load(),linked.load());
        busy=false;
      }
    } else {
      int command=request.exchange(9);
      if(command!=9 || uint32_t(now-lastCheck)>=3000) {
        busy=true;
        if(command==10)resetTrip();
        else wheelTransaction(client,command==9?0:command);
        lastCheck=millis();busy=false;
        if(command!=9)Serial.printf("Wheel: %s; mode=%d; connected=%d\n",statusNames[statusCode.load()],shownMode.load(),linked.load());
      }
    }
    if(client->isConnected() && linked)pollTelemetry(client);
    saveTrip(false);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
void centeredText(const char* text,int cx,int cy,int size,uint16_t color) {
  tft.setTextSize(size);tft.setTextWrap(false);
  int16_t x,y;uint16_t w,h;
  tft.getTextBounds(text,0,0,&x,&y,&w,&h);
  tft.setTextColor(color,ILI9488_BLACK);
  tft.setCursor(cx-int(w)/2-x,cy-int(h)/2-y);tft.print(text);
}
void drawPage() {
  tft.fillScreen(ILI9488_BLACK);
  centeredText("TERRY",240,31,4,ILI9488_WHITE);
  centeredText("MPH",105,68,2,ILI9488_WHITE);
  centeredText("TRIP MILES",285,68,2,ILI9488_WHITE);
  tft.drawRoundRect(385,65,85,65,10,ILI9488_WHITE);
  centeredText("RESET",427,97,2,ILI9488_WHITE);
  tft.drawRoundRect(10,205,110,100,12,ILI9488_WHITE);
  tft.drawRoundRect(360,205,110,100,12,ILI9488_WHITE);
  forceScreen=true;
}
void setup() {
  Serial.begin(115200);delay(1000);
  Serial.println("Terry controller v7: speed/trip; r=read mode, z=reset trip, d=telemetry, c=touch calibration.");
  initScreen();loadTouchCalibration();loadTrip();drawPage();
  if(xTaskCreatePinnedToCore(bleWorker,"wheel",12288,nullptr,1,nullptr,0)!=pdPASS) {
    busy=false;statusCode=2;request=9;
  }
}
void loop() {
  static int lastMode=-99,lastStatus=-99;
  static bool held=false,lastCalibrated=false;
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='d')Serial.printf("TELEMETRY valid=%d speed_tenths_mph=%lu trip_mm=%lu\n",telemetryValid.load(),(unsigned long)speedTenths.load(),(unsigned long)tripMm.load());
    if(c=='z' && !busy){busy=true;request=10;}
    if(c=='r' && !busy){busy=true;request=0;}
    if(c=='c' && !busy){calibrateTouch();drawPage();}
  }
  // Long press the title for calibration when no transaction is active.
  static uint32_t pressAt=0;
  bool pressed=touchReady && touch.touched();
  if(pressed) {
    TS_Point p=touch.getPoint();
    if(!held)pressAt=millis();
    if(!busy && p.z>200) {
      if(!touchCalibrated && !held){calibrateTouch();drawPage();pressed=false;}
      else if(touchCalibrated) {
        int x=lroundf(calibration.ax*p.x+calibration.bx*p.y+calibration.cx);
        int y=lroundf(calibration.ay*p.x+calibration.by*p.y+calibration.cy);
        if(!held && x>=385 && x<470 && y>=65 && y<130) {busy=true;request=10;}
        else if(linked && !held && y>=205 && y<305 && ((x>=10 && x<120)||(x>=360 && x<470))) {
          busy=true;request=shownMode.load()<0?0:(x<120?-1:1);
        }
        if(x<350 && y<60 && millis()-pressAt>2000){calibrateTouch();drawPage();pressed=false;}
      }
    }
  }
  held=pressed;
  int m=shownMode.load(),s=statusCode.load();bool online=linked.load();
  static bool lastOnline=false;
  bool fullRedraw=forceScreen || online!=lastOnline;
  if(fullRedraw) {
    if(!online) {
      tft.fillScreen(ILI9488_BLACK);
      centeredText("Turn On",240,115,10,ILI9488_WHITE);
      centeredText("Bike",240,205,10,ILI9488_WHITE);
    } else {
      drawPage();
    }
  }
  static uint32_t lastSpeed=UINT32_MAX,lastTrip=UINT32_MAX;
  static bool lastValid=false;
  if(online) {
    bool valid=telemetryValid.load() && uint32_t(millis()-telemetryAt.load())<3000;
    uint32_t speed=speedTenths.load(),distance=uint32_t(uint64_t(tripMm.load())*1000/1609344); // thousandths of a mile
    char text[32];
    if(fullRedraw || valid!=lastValid || (valid && speed!=lastSpeed)) {
      tft.fillRect(10,83,190,40,ILI9488_BLACK);
      if(valid)snprintf(text,sizeof(text),"%lu.%lu",(unsigned long)(speed/10),(unsigned long)(speed%10));
      else snprintf(text,sizeof(text),"--");
      centeredText(text,105,103,4,ILI9488_WHITE);
    }
    if(fullRedraw || distance!=lastTrip) {
      tft.fillRect(205,88,170,32,ILI9488_BLACK);
      snprintf(text,sizeof(text),"%lu.%03lu",(unsigned long)(distance/1000),(unsigned long)(distance%1000));
      centeredText(text,285,104,3,ILI9488_WHITE);
    }
    lastSpeed=speed;lastTrip=distance;lastValid=valid;
    // Background checks/busy state do not affect the display.
    // Only replace the label's region when the confirmed mode changes.
    if(fullRedraw || m!=lastMode) {
      tft.fillRect(135,239,210,32,ILI9488_BLACK);
      centeredText(m<0?"UNKNOWN":modeNames[m],240,255,3,ILI9488_WHITE);
    }
    if(fullRedraw || s!=lastStatus) {
      screenText(15,133,450,s==1?"":statusNames[s],ILI9488_WHITE,1);
    }
    if(fullRedraw || touchCalibrated!=lastCalibrated) {
      tft.fillRect(0,155,480,24,ILI9488_BLACK);
      centeredText(touchCalibrated?"ECO < STANDARD < TURBO":"Tap screen to calibrate touch",240,168,2,ILI9488_WHITE);
      uint16_t color=touchCalibrated?ILI9488_WHITE:ILI9488_DARKGREY;
      tft.fillTriangle(30,255,90,225,90,285,color);
      tft.fillTriangle(450,255,390,225,390,285,color);
    }
  }
  lastOnline=online;lastMode=m;lastStatus=s;lastCalibrated=touchCalibrated;forceScreen=false;
  delay(10);
}

#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <ILI9488.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <math.h>

// Tommie Berry Rev6: confirmed against schematic and existing working TFT sketch.
ILI9488 tft(14,12,13); // CS, DC, RESET; hardware global SPI.
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(18,7); // CS, IRQ.
Adafruit_NeoPixel boardLeds(12,8,NEO_GRB+NEO_KHZ800);
bool forceScreen=true, touchReady=false, touchCalibrated=false;
struct TouchCalibration { uint32_t magic; float ax,bx,cx,ay,by,cy; };
TouchCalibration calibration{};

void screenText(int x,int y,int width,const char* text,uint16_t color,int size) {
  tft.fillRect(x,y,width,8*size,ILI9488_BLACK);
  tft.setTextWrap(false);tft.setTextSize(size);tft.setTextColor(color,ILI9488_BLACK);
  tft.setCursor(x,y);tft.print(text);
}
void initScreen() {
  // SD is unused and deselected. Backlight jumper JP4 remains as on working board.
  pinMode(41,OUTPUT);digitalWrite(41,HIGH);
  pinMode(18,OUTPUT);digitalWrite(18,HIGH);
  // Tom confirmed GPIO35 through JP4 is the active-high backlight.
  // Match the working sketch with PSRAM Disabled.
  #if defined(BOARD_HAS_PSRAM)
  #error "Rev6 GPIO35 backlight requires PSRAM Disabled"
  #endif
  pinMode(35,OUTPUT);digitalWrite(35,HIGH);
  boardLeds.begin();boardLeds.clear();boardLeds.show();
  SPI.begin(10,-1,11,14); // TFT has no separate MISO; GPIO42 is touch MISO.
  tft.begin();tft.setRotation(3);tft.fillScreen(ILI9488_BLACK);
  touchSPI.begin(21,42,9,18);
  touchReady=touch.begin(touchSPI);touch.setRotation(0);
}
bool loadTouchCalibration() {
  Preferences prefs;
  if(!prefs.begin("bms-tft-touch",true))return false;
  bool ok=prefs.getBytesLength("cal")==sizeof(calibration);
  if(ok)prefs.getBytes("cal",&calibration,sizeof(calibration));
  prefs.end();
  ok=ok && calibration.magic==0x42544631 && isfinite(calibration.ax) && isfinite(calibration.bx) &&
    isfinite(calibration.cx) && isfinite(calibration.ay) && isfinite(calibration.by) && isfinite(calibration.cy) &&
    fabs(calibration.ax*calibration.by-calibration.bx*calibration.ay)>0.000001;
  touchCalibrated=ok;return ok;
}
bool captureTouch(int x,int y,TS_Point& out) {
  tft.fillScreen(ILI9488_BLACK);
  screenText(70,115,340,"Tap and release the cross",ILI9488_WHITE,2);
  screenText(70,143,340,"Touch calibration (20s timeout)",ILI9488_LIGHTGREY,1);
  tft.drawFastHLine(x-12,y,25,ILI9488_YELLOW);tft.drawFastVLine(x,y-12,25,ILI9488_YELLOW);
  uint32_t began=millis();
  while(touch.touched()) {if(millis()-began>20000)return false;delay(10);}
  while(!touch.touched()) {if(millis()-began>20000)return false;delay(10);}
  int32_t sx=0,sy=0;int n=0;
  for(int i=0;i<8;i++) {TS_Point p=touch.getPoint();if(p.z>200){sx+=p.x;sy+=p.y;n++;}delay(10);}
  if(n<3)return false;
  out.x=sx/n;out.y=sy/n;
  while(touch.touched()) {if(millis()-began>20000)return false;delay(10);}
  return true;
}
void calibrateTouch() {
  if(!touchReady)return;
  TS_Point a,b,c;
  bool ok=captureTouch(30,40,a) && captureTouch(450,40,b) && captureTouch(30,260,c);
  if(ok) {
    float ux=b.x-a.x,uy=b.y-a.y,vx=c.x-a.x,vy=c.y-a.y;
    float determinant=ux*vy-uy*vx;
    ok=fabs(determinant)>10000;
    if(ok) {
      calibration={0x42544631,420*vy/determinant,-420*vx/determinant,0,
        -220*uy/determinant,220*ux/determinant,0};
      calibration.cx=30-calibration.ax*a.x-calibration.bx*a.y;
      calibration.cy=40-calibration.ay*a.x-calibration.by*a.y;
      touchCalibrated=true;
      Preferences prefs;
      if(prefs.begin("bms-tft-touch",false)) {
        bool saved=prefs.putBytes("cal",&calibration,sizeof(calibration))==sizeof(calibration);prefs.end();
        Serial.println(saved?"Touch calibrated and saved.":"Touch calibrated in RAM; save failed.");
      } else Serial.println("Touch calibrated in RAM; storage unavailable.");
    }
  }
  if(!ok)Serial.println("Touch calibration incomplete. Monitoring remains available; c retries calibration when idle.");
  tft.fillScreen(ILI9488_BLACK);forceScreen=true;
}

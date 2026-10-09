#pragma once
// Controller characteristic 500b, byte 7: remaining battery charge (0..100).
// Verified against saved Terry packets and the inspected R1.03 protocol reference.
std::atomic<int> batteryPercent{-1};
std::atomic<uint32_t> batteryAt{0};
uint32_t lastBatteryPoll=0;
bool batteryPollStarted=false;
void pollBattery(BLEClient* client) {
  if(!client->isConnected() || !linked){batteryPercent=-1;return;}
  uint32_t now=millis();
  if(batteryPollStarted && uint32_t(now-lastBatteryPoll)<5000)return;
  batteryPollStarted=true;lastBatteryPoll=now;
  auto service=client->getService(BLEUUID("52756265-6e43-6167-6e69-654350485000"));
  auto ch=service?service->getCharacteristic(BLEUUID("52756265-6e43-6167-6e69-65435048500b")):nullptr;
  if(!ch){batteryPercent=-1;return;}
  String data=ch->readValue();
  if(data.length()!=20 || !client->isConnected() || !linked){batteryPercent=-1;return;}
  unsigned int percentage=uint8_t(data[7]);
  if(percentage>100){batteryPercent=-1;return;}
  batteryAt=millis();batteryPercent=int(percentage);
}

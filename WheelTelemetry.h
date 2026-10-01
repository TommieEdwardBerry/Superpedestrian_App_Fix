#pragma once
// Speed scale confirmed by Terry's measured 20-foot rollout (2026-10-01).
std::atomic<uint32_t> speedTenths{0},tripMm{0},telemetryAt{0};
std::atomic<bool> telemetryValid{false};
double tripMeters=0,previousMps=0,lastSavedMeters=0;
uint32_t previousAt=0,lastPoll=0,lastSave=0;
bool previousValid=false;
void loadTrip() {
  Preferences p;
  if(p.begin("terry-trip",true)){tripMeters=p.getDouble("meters",0);p.end();}
  if(!isfinite(tripMeters) || tripMeters<0 || tripMeters>4000000)tripMeters=0;
  lastSavedMeters=tripMeters;tripMm=uint32_t(lround(tripMeters*1000));
}
void saveTrip(bool force) {
  if(!force && uint32_t(millis()-lastSave)<30000)return;
  lastSave=millis();
  if(tripMeters==lastSavedMeters)return;
  Preferences p;
  if(p.begin("terry-trip",false)) {
    if(p.putDouble("meters",tripMeters)==sizeof(double))lastSavedMeters=tripMeters;
    p.end();
  }
}
void loseTelemetry() {
  if(previousValid)saveTrip(true);
  previousValid=false;telemetryValid=false;speedTenths=0;
}
void resetTrip() {
  tripMeters=0;tripMm=0;previousValid=false;
  // Force a zero write even if no movement has occurred since the last checkpoint.
  lastSavedMeters=-1;saveTrip(true);Serial.println("Trip reset to zero.");
}
void pollTelemetry(BLEClient* client) {
  if(uint32_t(millis()-lastPoll)<500)return;
  lastPoll=millis();
  auto service=client->getService(BLEUUID("52756265-6e43-6167-6e69-654350485000"));
  auto ch=service?service->getCharacteristic(BLEUUID("52756265-6e43-6167-6e69-654350485002")):nullptr;
  if(!ch){loseTelemetry();return;}
  String data=ch->readValue();
  if(data.length()!=20 || !client->isConnected() || !linked){loseTelemetry();return;}
  const auto* bytes=reinterpret_cast<const uint8_t*>(data.c_str());
  uint16_t raw=uint16_t(bytes[0])|(uint16_t(bytes[1])<<8);
  double mps=raw*0.0005;
  uint32_t now=millis(),dt=now-previousAt;
  // Never integrate missing readings, reconnect time, or a blocked BLE read.
  if(previousValid && dt<=2000)tripMeters+=(previousMps+mps)*0.5*dt/1000.0;
  previousAt=now;previousMps=mps;previousValid=true;
  tripMm=uint32_t(lround(tripMeters*1000));
  speedTenths=uint32_t(lround(mps*22.369362920544));
  telemetryAt=now;telemetryValid=true;
}

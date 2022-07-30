#define USE_GET_MILLISECOND_TIMER
#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include <FastLED.h>
#include <painlessMesh.h>

#define BRIGHTNESS         30
#define NUM_LEDS    30 //32 on skate
#define FRAMES_PER_SECOND  60
#define USING_SPEED_DATA false
FASTLED_USING_NAMESPACE
CRGB leds[NUM_LEDS];
painlessMesh  mesh;
String ssid = "WrongoWrongo";
String password = "Stratovo";

#ifdef ARDUINO_ARCH_ESP32
#define DATA_PIN    13 //13 for lolin32
#include <BLEDevice.h>
static BLEAddress   myaddr("08:08:08:08:08:01");
static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb"); //use fff0 to find it, this to connect
static BLEUUID    charUUID("ffe1");
static BLEClient* myDevice = NULL;
static BLERemoteCharacteristic* pRemChar = NULL;
#else
#define DATA_PIN    2 //gpio2=D4 for D1 mini
#endif

#define BREAKING_MIN 8
#define BREAKING_MAX 20
#define BREAKING_LEDS 8
#define QUICKSPEED 3000 //30kph 18mph

String str(const char *fmtStr, ...) {
  static char buf[200] = {'\0'};
  va_list arg_ptr;
  va_start(arg_ptr, fmtStr);
  vsprintf(buf, fmtStr, arg_ptr);
  va_end(arg_ptr);
  return String(buf);
}
String str(std::string s) { return String(s.c_str()); }
String str(bool v) { return v? " WIN" : " FAIL"; }

struct WheelData {
  int voltage;
  int speed, lastspeed;
  float dspeedfilt, speedfilt;
  int totalDist;
  int current;
  int temp;
  uint32_t time;
  String toString() const {
    return str("[%dV %0.1fdam/h %ddam %dA %dC]", voltage, speedfilt, totalDist, current, temp);
  }
};

WheelData wheelDat = {0};
int32_t timeOffset = 0;

uint32_t get_millisecond_timer() { return millis() + timeOffset; }

int byteArrayInt2(byte low, byte high) {
  return (low & 255) + ((high & 255) * 256);
}

long byteArrayInt4(byte value1, byte value2, byte value3, byte value4) {
  return (((((long) ((value1 & 255) << 16))) | ((long) ((value2 & 255) << 24))) | ((long) (value3 & 255))) | ((long) ((value4 & 255) << 8));
}

uint8_t fadeit(int16_t in, int16_t minv, int16_t maxv) { //returns map between 0-255
  return ::map(constrain(in, minv, maxv), minv, maxv, 0, 255);
}

void juggle(uint8_t hue, uint8_t brightness) {
  // eight colored dots, weaving in and out of sync with each other
  // fadeToBlackBy( leds, NUM_LEDS, 40);
  byte dothue = 0;
  for( int i = 0; i < 10; i++) {
    leds[beatsin16( 3*i/2 + 2, 0, NUM_LEDS-1 )] |= CHSV(dothue + hue, 220, brightness);
    dothue += 6;
  }
}

void confetti(uint8_t hue, uint8_t brightness) {
  // random colored speckles that blink in and fade smoothly
  //fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(hue + random8(64), 200, brightness);
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2811,DATA_PIN,GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(ssid, password, 1337);
  mesh.onReceive([](uint32_t from, const String &msg) {
    Serial.printf("SYSTEM: Received from %u msg=%s\n", from, msg.c_str());
  });
  mesh.onNewConnection([](uint32_t nodeId) {
    Serial.printf("SYSTEM: New Connection, nodeId = %u\n", nodeId);
  });
  mesh.onChangedConnections([](){
    Serial.printf("SYSTEM: Changed connections %s\n",mesh.subConnectionJson().c_str());
  });
  mesh.onNodeTimeAdjusted([](int32_t offset) {
    Serial.printf("SYSTEM: Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
    timeOffset += offset / 1000;
  });
}

#define MAX_VOXEL (NUM_LEDS/2)

int mapc(int v, int a, int b, int c, int d) { return constrain(::map(v, a, b, c, d), c, d); }

class Voxel {
  uint32_t starttime;
  uint16_t speed;
public:
  Voxel(uint16_t s=0, uint32_t t=0) : starttime(t), speed(s) { }
  String toString(uint32_t now) const { return str("<v s%d,%dmsa> ", speed, now - starttime); }
  bool valid(uint32_t now) const { return speed > 0 && getLedPos(now) < MAX_VOXEL; }
  uint8_t reset() { speed = 0; }
  uint8_t getLedPos(uint32_t now) const {
    uint16_t beat = ((now - starttime) * (speed << 12)) >> 16;
    return ::map(beat, 0, SHRT_MAX, 0, MAX_VOXEL); // = 1/(10/60/60 * 23leds/27cm)
  }
    // map(pos, 0, MAX_POS, 0, NUM_LEDS/2);
};
#define VOXELS_NUM 6
Voxel voxels[VOXELS_NUM] = {};
void enqueVox(int speed, uint32_t now) {
  for (int i = 0; i < VOXELS_NUM; i++)
    if (! voxels[i].valid(now)) {
      voxels[i] = Voxel(speed, now);
      return;
    }
}

//true=right-side
CRGB& side(bool side, int16_t pos) {
  if (side) return leds[constrain(MAX_VOXEL     + pos, MAX_VOXEL, NUM_LEDS)];
  else      return leds[constrain(MAX_VOXEL - 1 - pos,         0, MAX_VOXEL)];
}

uint16_t newVoxThresh = 3;
uint16_t fps = 0;

void loop() {
  mesh.update();
  uint32_t now = get_millisecond_timer();
  fps++;
  uint8_t bright = fadeit(quadwave8((now >> 6) + 50), 65, 191);

  // EVERY_N_MILLISECONDS(350) { newVoxThresh = mapc(wheelDat.speedfilt, 0, QUICKSPEED, 4, 2*MAX_VOXEL/3); }
  EVERY_N_MILLISECONDS(1000) { Serial.printf("  (%d fps, bright %d)\n", fps, bright); fps = 0; }

  // None of this is getting used because we're not receiving serial data constantly.  Only shows racing LEDs anyway
  /*
  while (Serial.available()) {
    int v = Serial.parseInt() * 100;
    while (Serial.available() > 0)
       Serial.read();
    Serial.printf("read in %d\n", v);
    wheelDat.speedfilt = wheelDat.speed = v;
    wheelDat.time = now;
  }

  
  if ((now - wheelDat.time) < 4000) { //valid data!
    fadeToBlackBy(leds, NUM_LEDS, 40);
    uint8_t bright = fadeit(wheelDat.speedfilt, 100, 800); //1-8kph
    if (bright < 253) { //when going slow do this pattern
      //juggle(quadwave8(now >> 6) / 6 + 165, 255 - bright); //color specific
    }
    if (bright > 0) {

      fadeToBlackBy(leds, NUM_LEDS, mapc(wheelDat.speedfilt, 0, QUICKSPEED, 0, 80));
      uint8_t minvoxp = 255;
      for (int i = 0; i < VOXELS_NUM; i++) {
        if (! voxels[i].valid(now)) { voxels[i].reset(); continue; }
        uint8_t pos = voxels[i].getLedPos(now);
        uint8_t spread = mapc(wheelDat.speedfilt, 0, QUICKSPEED, 1, 3);
        CHSV c1(quadwave8((now >> 5) + 88) / 6 + 160, 220, 255);
        CHSV c2(quadwave8( now >> 5      ) / 6 + 160, 220, 255);
        //TODO when speed > 3000 (18mph) GO RED
        for (uint8_t s = 0; s < spread; s++) {
          c1.v = c2.v = 255 - s*16;
          side(1, pos - s) |= c1; //right side
          side(0, pos - s) |= c2; //left side
        }
        minvoxp = min(minvoxp, pos);
      }
      if (minvoxp > newVoxThresh) //add new voxel
        enqueVox(wheelDat.speedfilt, now);

      if (wheelDat.dspeedfilt < -BREAKING_MIN) {
        for (int i = 0; i < BREAKING_LEDS; i++) {
          CHSV clr(0, 255, ::map(wheelDat.dspeedfilt, -BREAKING_MIN, -BREAKING_MAX, 0, 255)); //hue 0=red
          leds[i]                |= clr;
          leds[NUM_LEDS - 1 - i] |= clr;
        }
      }
      //TODO do brake lights when stopping.. at all?
    }

  } else {
    fadeToBlackBy( leds, NUM_LEDS, 18);
    uint8_t bright = fadeit(quadwave8((now >> 6) + 50), 65, 191);
    juggle(now >> 8, bright);
    confetti((now >> 8) + 20, 255 - bright);
  }
  */
  fadeToBlackBy( leds, NUM_LEDS, 18);
  juggle(now >> 8, bright);
  confetti((now >> 8) + 20, 255 - bright);

  FastLED.show();
  FastLED.delay(1000/FRAMES_PER_SECOND);
  // EVERY_N_SECONDS( 5 ) { }
}


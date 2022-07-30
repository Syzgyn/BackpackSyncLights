#define USE_GET_MILLISECOND_TIMER
#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include <FastLED.h>
#include <painlessMesh.h>

#define BRIGHTNESS          30
#define NUM_LEDS            30 //32 on skate
#define FRAMES_PER_SECOND   60
FASTLED_USING_NAMESPACE

CRGB leds[NUM_LEDS];
painlessMesh  mesh;
String ssid = "WrongoWrongo";
String password = "Stratovo";

#define DATA_PIN    2 //gpio2=D4 for D1 mini

int32_t timeOffset = 0;

uint32_t get_millisecond_timer() { return millis() + timeOffset; }

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

int mapc(int v, int a, int b, int c, int d) { return constrain(::map(v, a, b, c, d), c, d); }

uint16_t fps = 0;
uint16_t count = 0;

void loop() {
  mesh.update();
  uint32_t now = get_millisecond_timer();
  fps++;

  EVERY_N_MILLISECONDS(1000) { Serial.printf("  (%d fps, timer %d, count %d)\n", fps, now >> 16, count); fps = 0; count++;}

  fadeToBlackBy( leds, NUM_LEDS, 18);
  uint8_t bright = fadeit(quadwave8((now >> 6) + 50), 65, 191);
  juggle(now >> 8, bright);
  confetti((now >> 8) + 20, 255 - bright);

  FastLED.show();
  FastLED.delay(1000/FRAMES_PER_SECOND);
  // EVERY_N_SECONDS( 5 ) { }
}
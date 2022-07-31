#define USE_GET_MILLISECOND_TIMER
#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include <FastLED.h>
#include <painlessMesh.h>

#define BRIGHTNESS          30
#define NUM_LEDS            30 //32 on skate
#define FRAMES_PER_SECOND   60
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
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

//Pattern related variables
uint8_t currentPattern = 0;
bool isCrossfading = false;
uint8_t currentFade = 0;
uint32_t now = get_millisecond_timer();
uint8_t bright = fadeit(quadwave8((now >> 6) + 50), 65, 191);

void juggle(uint8_t fade) {
  // eight colored dots, weaving in and out of sync with each other
  // fadeToBlackBy( leds, NUM_LEDS, 40);
  uint8_t hue = now >> 8;

  byte dothue = 0;
  for( int i = 0; i < 10; i++) {
    leds[beatsin16( 3*i/2 + 2, 0, NUM_LEDS-1 )] |= CHSV(dothue + hue, 220, fade);
    dothue += 6;
  }
}

// FastLED's built-in rainbow generator
void rainbow(uint8_t fade) 
{
  uint8_t hue = (now >> 8);

  fill_rainbow( leds, NUM_LEDS, hue, 7);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

// built-in FastLED rainbow, plus some random sparkly glitter
void rainbowWithGlitter(uint8_t fade) 
{
  rainbow(fade);
  addGlitter(80);
}

// random colored speckles that blink in and fade smoothly
void confetti(uint8_t fade) {
  //fadeToBlackBy( leds, NUM_LEDS, 10);
  uint8_t hue = (now >> 8) + 20;

  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(hue + random8(64), 200, fade);
}

// a colored dot sweeping back and forth, with fading trails
void sinelon(uint8_t fade)
{
  uint8_t hue = (now >> 8) + 20;
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( hue, 255, 192);
}

// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
void bpm(uint8_t fade)
{
  uint8_t hue = (now >> 8) + 20;
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, hue+(i*2), beat-hue+(i*10));
  }
}

typedef void (*SimplePatternList[])(uint8_t fade);
SimplePatternList patterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };

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

//TODO: Cleanup loop(), implement crossfading
//https://github.com/jasoncoon/esp8266-fastled-webserver/issues/192
void loop() {
  fps++;
  EVERY_N_MILLISECONDS(1000) { Serial.printf("  (%d fps, pattern %d)\n", fps, currentPattern); fps = 0;}

  mesh.update();
  now = get_millisecond_timer();
  uint8_t pattern = (now >> 13) % 5; // >> 11 = every 2 seconds, >> 16 = every 64 seconds

  if (pattern != currentPattern) {
    isCrossfading = true;
  }

  uint8_t fade = fadeit(quadwave8((now >> 6) + 50), 65, 191);

  fadeToBlackBy( leds, NUM_LEDS, 18);
  patterns[2](255 - fade);
  patterns[4](fade);
  // juggle(fade);
  // confetti(255 - fade);
 
  FastLED.show();
  FastLED.delay(1000/FRAMES_PER_SECOND);
  // EVERY_N_SECONDS( 5 ) { }
}
#define USE_GET_MILLISECOND_TIMER
#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include <FastLED.h>
#include <painlessMesh.h>
#include "patterns.h"
#include "globalVars.h"

// #define DEBUG_PATTERN 1

#define BRIGHTNESS          100
#define FRAMES_PER_SECOND   400
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// Three array solution from Stefan Petrick (https://gist.github.com/StefanPetrick/0c0d54d0f35ea9cca983)
CRGB leds[NUM_LEDS];  // Output
CRGB ledsA[NUM_LEDS]; // Source A
CRGB ledsB[NUM_LEDS]; // Source B

painlessMesh  mesh;
String ssid = "Stratovo";
String password = "Stratovo";

#define DATA_PIN    2 //gpio2=D4 for D1 mini

int32_t timeOffset = 0;

uint32_t get_millisecond_timer() { return millis() + timeOffset; }

uint32_t now = get_millisecond_timer();

//Pattern related variables
uint8_t currentPattern = 0;
bool isCrossfading = false;
uint8_t currentFade = 0;

uint16_t fps = 0;

typedef void (*SimplePatternList[])(CRGB *leds, uint32_t now);
SimplePatternList patterns = { lava, rainbow, waves,  bpm };

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

void loop() {
  //Update mesh and time
  mesh.update();
  now = get_millisecond_timer();

  //Update pattern timer
  uint8_t pattern = (now >> 18) % ARRAY_SIZE(patterns); // >> 11 = every 2 seconds, >> 16 = every 64 seconds

  fps++;
  EVERY_N_MILLISECONDS(1000) { Serial.printf("  (%d fps, pattern %d, currentPattern %d)\n", fps, pattern, currentPattern); fps = 0;}

  // Dim everything in the last frame
  fadeToBlackBy( ledsA, NUM_LEDS, 18);
  fadeToBlackBy( ledsB, NUM_LEDS, 18);

  // For debugging a single pattern
  #ifdef DEBUG_PATTERN
    pattern = DEBUG_PATTERN;
    currentPattern = DEBUG_PATTERN;
  #endif

  // Trigger once when the pattern first starts to change
  if (pattern != currentPattern && !isCrossfading) {
    isCrossfading = true;
  }

  // While crossfading, increase the fade and run the new pattern
  if (isCrossfading) {
    currentFade = qadd8(currentFade, 1);

    patterns[pattern](ledsB, now);
  }

  // At the end of the crossfade, update to the new pattern
  if (currentFade == 255) {
    currentPattern = pattern;
    isCrossfading = false;
    currentFade = 0;

    //Swap the arrays to prevent glitching
    for (int i = 0; i < NUM_LEDS; i++) {
      ledsA[i] = ledsB[i];
    }
  }

  // Run the current pattern always
  patterns[currentPattern](ledsA, now);
  
  // mix the 2 arrays together
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = blend(ledsA[i], ledsB[i], currentFade);
  }
 
  FastLED.show();
  FastLED.delay(1000/FRAMES_PER_SECOND);
}
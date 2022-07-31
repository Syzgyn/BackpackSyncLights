#include <FastLED.h>
#include "globalVars.h"

#define qsuba(x, b)  ((x>b)?x-b:0)  // Analog Unsigned subtraction macro. if result <0, then => 0


void juggle(CRGB *array, uint32_t now)
{
  // eight colored dots, weaving in and out of sync with each other
  uint8_t hue = now >> 8;

  byte dothue = 0;
  for( int i = 0; i < 10; i++) {
    array[beatsin16( 3*i/2 + 2, 0, NUM_LEDS-1 )] |= CHSV(dothue + hue, 220, 255);
    dothue += 6;
  }
}

// FastLED's built-in rainbow generator
void rainbow(CRGB *array, uint32_t now)
{
  uint8_t initialHue = (now >> 5);
  uint8_t deltaHue = 7;

  fill_rainbow( array, NUM_LEDS, initialHue, deltaHue);
}

void addGlitter(CRGB *array, fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    array[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

// built-in FastLED rainbow, plus some random sparkly glitter
void rainbowWithGlitter(CRGB *array, uint32_t now)
{
  rainbow(array, now);
  addGlitter(array, 80);
}

// random colored speckles that blink in and fade smoothly
void confetti(CRGB *array, uint32_t now)
{
  uint8_t hue = (now >> 8) + 20;

  int pos = random16(NUM_LEDS);
  array[pos] += CHSV(hue + random8(64), 200, 255);
}

// a colored dot sweeping back and forth, with fading trails
void sinelon(CRGB *array, uint32_t now)
{
  uint8_t hue = (now >> 8) + 20;
  int pos = beatsin16(13,0,NUM_LEDS - 1);
  array[pos] += CHSV( hue, 255, 192);
}

// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
void bpm(CRGB *array, uint32_t now)
{
  uint8_t hue = (now >> 8) + 20;
  uint8_t BeatsPerMinute = 20;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) {
    array[i] = ColorFromPalette(palette, hue+(i*2), beat-hue+(i*10));
  }
}
// https://github.com/atuline/FastLED-Demos/blob/master/plasma/plasma.ino
// Not implemented.  I don't love it, seems too dark currently.
void plasma(CRGB *array, uint32_t now) {

  int thisPhase = beatsin8(6,-64,64);                           // Setting phase change for a couple of waves.
  int thatPhase = beatsin8(7,-64,64);

  for (int k=0; k<NUM_LEDS; k++) {                              // For each of the LED's in the strand, set a brightness based on a wave as follows:

    int colorIndex = cubicwave8((k*23)+thisPhase)/2 + cos8((k*15)+thatPhase)/2;           // Create a wave and add a phase change and add another wave with its own phase change.. Hey, you can even change the frequencies if you wish.
    int thisBright = qsuba(colorIndex, beatsin8(7,0,96));                                 // qsub gives it a bit of 'black' dead space by setting sets a minimum value. If colorIndex < current value of beatsin8(), then bright = 0. Otherwise, bright = colorIndex..

    array[k] = ColorFromPalette(OceanColors_p, colorIndex, thisBright, LINEARBLEND);  // Let's now add the foreground colour.
  }

} // plasma()
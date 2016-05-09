#include <FastLED.h>
#include <Wire.h>
#include <avr/pgmspace.h>

FASTLED_USING_NAMESPACE

#define BORDER_LED_PIN    6
#define SHELF_LED_PIN    3
#define STROBE_PIN 4
#define RESET_PIN 5
#define LEFT_EQ_PIN A0
#define RIGHT_EQ_PIN A1

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

#define NUM_SHELF_LEDS 60
#define LEDS_PER_SHELF 20
#define NUM_BORDER_LEDS 147
#define NUM_TOTAL_LEDS NUM_BORDER_LEDS + NUM_SHELF_LEDS

#define BRIGHTNESS         120
#define FRAMES_PER_SECOND  240

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

CRGB borderLeds[NUM_BORDER_LEDS];
CRGB allShelves[NUM_SHELF_LEDS];
CRGB* bottomShelfLeds = allShelves + 0;
CRGB* middleShelfLeds = allShelves + LEDS_PER_SHELF;
CRGB* topShelfLeds = allShelves + 2 * LEDS_PER_SHELF;

int frequenciesLeft[7];
int frequenciesRight[7];

uint8_t currentPatternNumber = 0; // Index number of which pattern is current
uint8_t hueCounter = 0; // rotating "base color" used by many of the patterns

void setup() {
    FastLED.addLeds<LED_TYPE, BORDER_LED_PIN, COLOR_ORDER>(borderLeds, NUM_BORDER_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, SHELF_LED_PIN, COLOR_ORDER>(allShelves, NUM_SHELF_LEDS).setCorrection(TypicalLEDStrip);

    FastLED.setBrightness(BRIGHTNESS);

    pinMode(RESET_PIN, OUTPUT); // reset
    pinMode(STROBE_PIN, OUTPUT); // strobe
    digitalWrite(RESET_PIN,LOW); // reset low
    digitalWrite(STROBE_PIN,HIGH); //pin 5 is RESET on the shield
}

/*******FOR THE LOVE OF GOD, WHY DOES THIS ONLY WORK WHEN ITS AFTER THE SETUP!!********/
// List of patterns to cycle through.
typedef void (*SimplePatternList[])();

//SimplePatternList patterns = {rainbow, confetti, sinelon, juggle, flowEqualizer, bpm};
SimplePatternList patterns = {waterfall};

void loop() {
    readFrequencies();

    patterns[currentPatternNumber]();

    FastLED.show();
    FastLED.delay(1000 / FRAMES_PER_SECOND);

    EVERY_N_MILLISECONDS(20) { hueCounter++; } // slowly cycle the "base color" through the rainbow
    EVERY_N_SECONDS(15) { nextPattern(); } // change patterns periodically
}

void nextPattern() {
    currentPatternNumber = (currentPatternNumber + 1) % ARRAY_SIZE(patterns);
}

void readFrequencies(){
    //reset the data
    digitalWrite(RESET_PIN, HIGH);
    digitalWrite(RESET_PIN, LOW);

    //loop through all 7 bands
    for(int band=0; band < 7; band++) {
        digitalWrite(STROBE_PIN, LOW); // go to the next band
        delayMicroseconds(50); //gather some data
        frequenciesLeft[band] = analogRead(LEFT_EQ_PIN); // store left band reading
        frequenciesRight[band] = analogRead(RIGHT_EQ_PIN); // store right band reading
        digitalWrite(STROBE_PIN, HIGH); // reset the strobe pin
    }
}

void rainbow() {
    if(frequenciesLeft[2] > 600) {
        fill_rainbow(borderLeds, NUM_BORDER_LEDS, hueCounter, 7);
    }
}

// random colored speckles that blink in and fade smoothly
void confetti() {
    fadeToBlackBy(borderLeds, NUM_BORDER_LEDS, 10);
    uint8_t pos = random16(NUM_BORDER_LEDS);

    if(frequenciesLeft[0] > 600) {
        borderLeds[pos] += CHSV(hueCounter + random8(64), 200, 255);
    }
}

// a colored dot sweeping back and forth, with fading trails
void sinelon() {
    fadeToBlackBy(borderLeds, NUM_BORDER_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_BORDER_LEDS);
    if(frequenciesLeft[3] > 600) {
        borderLeds[pos] += CHSV(hueCounter, 255, 192);
    }
}

// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
void bpm() {
    uint8_t BeatsPerMinute = 120;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for (int i = 0; i < NUM_BORDER_LEDS; i++) { //9948
        borderLeds[i] = ColorFromPalette(palette, hueCounter + (i * 2), beat - hueCounter + (i * 10));
    }
}

// eight colored dots, weaving in and out of sync with each other
void juggle() {
    fadeToBlackBy(borderLeds, NUM_BORDER_LEDS, 20);
    byte dothue = 0;
    if(frequenciesLeft[1] > 600) {
        for (int i = 0; i < 8; i++) {
            borderLeds[beatsin16(i + 7, 0, NUM_BORDER_LEDS)] |= CHSV(dothue, 200, 255);
            dothue += 32;
        }
    }
}

void waterfallShelf(CRGB shelf[], int spectrum, int threashold) {
    if(frequenciesLeft[spectrum] > threashold) {
        Serial.println(map(frequenciesLeft[spectrum], 500, 1023, 0, 255));
        shelf[LEDS_PER_SHELF/2] = CHSV(map(frequenciesLeft[spectrum], 500, 1023, 0, 255), 200, 255);
        shelf[LEDS_PER_SHELF/2 + 1] = CHSV(map(frequenciesLeft[spectrum], 500, 1023, 0, 255), 200, 255);
    } else {
        shelf[LEDS_PER_SHELF/2] = CRGB(0,0,0);
        shelf[LEDS_PER_SHELF/2 + 1] = CRGB(0,0,0);
    }
    memmove( &shelf[0], &shelf[1], LEDS_PER_SHELF/2 * sizeof(CRGB) );
    memmove( &shelf[LEDS_PER_SHELF/2], &shelf[LEDS_PER_SHELF/2 - 1], LEDS_PER_SHELF/2 * sizeof(CRGB) );
}

void waterfallBorder(int spectrum) {
    if(frequenciesRight[1] > 500) {
        borderLeds[NUM_BORDER_LEDS/2] = CHSV(map(frequenciesRight[spectrum], 500, 1023, 0, 255), 200, 255);
    }  else {
        borderLeds[NUM_BORDER_LEDS / 2] = CRGB(0, 0, 0);
    }
    memmove( &borderLeds[0], &borderLeds[1], NUM_BORDER_LEDS/2 * sizeof(CRGB) );
    memmove( &borderLeds[NUM_BORDER_LEDS/2], &borderLeds[NUM_BORDER_LEDS/2 - 1], NUM_BORDER_LEDS/2 * sizeof(CRGB) );
}

void waterfall() {
    waterfallShelf(topShelfLeds, 6, 500);
    waterfallShelf(middleShelfLeds, 1, 500);
    waterfallShelf(bottomShelfLeds, 0, 500);
    waterfallBorder(4);
}


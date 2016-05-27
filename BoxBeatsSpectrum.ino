//TODO:Implement some sort of sensitivity selection mode
//TODO:Fix all of the modes broken by the change in led strip setup
#include <FastLED.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include "ClickEncoder.h"
#include "TimerOne.h"

FASTLED_USING_NAMESPACE

#define BORDER_LED_PIN 6
#define SHELF_LED_PIN 3
#define STROBE_PIN 4
#define RESET_PIN 5
#define LEFT_EQ_PIN A0
#define RIGHT_EQ_PIN A1

#define PIN_ENCODER_A 0
#define PIN_ENCODER_B 1
#define PIN_ENCODER_SWITCH 10

#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define NUM_SHELF_LEDS 60
#define LEDS_PER_SHELF 20
#define NUM_BORDER_LEDS 147
#define NUM_TOTAL_LEDS NUM_BORDER_LEDS + NUM_SHELF_LEDS

#define BRIGHTNESS 120
#define FRAMES_PER_SECOND 240

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

CRGBArray<NUM_BORDER_LEDS> borderLeds;
CRGBArray<NUM_SHELF_LEDS> allShelves;

CRGB *topShelfLedArray = allShelves + 2 * LEDS_PER_SHELF;
CRGB *middleShelfLedArray = allShelves + LEDS_PER_SHELF;
CRGB *bottomShelfLedArray = allShelves + 0;
CRGBSet topShelfLeds(topShelfLedArray, LEDS_PER_SHELF);
CRGBSet middleShelfLeds(middleShelfLedArray, LEDS_PER_SHELF);
CRGBSet bottomShelfLeds(bottomShelfLedArray, LEDS_PER_SHELF);

int frequenciesLeft[7];
int frequenciesRight[7];

int currentGlobalSensitivity = 0;
ClickEncoder *encoder;
int16_t previousEncoderValue, currentEncoderValue;
uint8_t currentPattern = 0;
uint8_t currentSelectionMode = 0;
uint8_t hueCounter = 0;

void timerIsr() {
    encoder->service();
}

void setup() {
    Serial.begin(9600);
    FastLED.addLeds<LED_TYPE, BORDER_LED_PIN, COLOR_ORDER>(borderLeds, NUM_BORDER_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, SHELF_LED_PIN, COLOR_ORDER>(allShelves, NUM_SHELF_LEDS).setCorrection(TypicalLEDStrip);

    FastLED.setBrightness(BRIGHTNESS);

    pinMode(RESET_PIN, OUTPUT); // reset
    pinMode(STROBE_PIN, OUTPUT); // strobe
    digitalWrite(RESET_PIN, LOW); // reset low
    digitalWrite(STROBE_PIN, HIGH); //pin 5 is RESET on the shield

    //Initialize the encoder (knob)
    encoder = new ClickEncoder(0, 1, 10, 4, false);
    Timer1.initialize(1000);
    Timer1.attachInterrupt(timerIsr);

    previousEncoderValue = 0;
}

/*******FOR THE LOVE OF GOD, WHY DOES THIS ONLY WORK WHEN ITS AFTER THE SETUP!?!********/
// List of patterns to cycle through.
typedef void (*SimplePatternList[])();

SimplePatternList patterns = {waterfallCascading, waterfall, confetti, equalizerRightToLeftBottomToTop, equalizerLeftToRightBottomToTop,
                              rainbow, sinelon, juggle, bpm};

void loop() {
    checkRotaryEncoderForInput();

    readAudioFrequencies();

    patterns[currentPattern]();

    FastLED.show();
    FastLED.delay(1000 / FRAMES_PER_SECOND);

    EVERY_N_MILLISECONDS(20)
    { hueCounter++; } // slowly cycle the "base color" through the rainbow
//    EVERY_N_SECONDS(15) { nextPattern(); } // change patterns periodically
}

void checkRotaryEncoderForInput() {
    currentEncoderValue += encoder->getValue();
    ClickEncoder::Button b = encoder->getButton();
    if (b != ClickEncoder::Open) {
        switch (b) {
            case ClickEncoder::Clicked:
                currentSelectionMode = wrapToRange(currentSelectionMode + 1, 0, 1);
                break;
            case ClickEncoder::DoubleClicked:
                encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
                Serial.print("Acceleration ");
                Serial.println(encoder->getAccelerationEnabled());
                break;
            case ClickEncoder::Released:
                Serial.println("Released");
                break;
            default:;
        }
    }

    if (b == ClickEncoder::Open) {
        if (currentEncoderValue != previousEncoderValue) {
            if (currentSelectionMode == 0) {
                if (currentEncoderValue < previousEncoderValue) {
                    nextPattern();
                } else {
                    previousPattern();
                }
            } else if (currentSelectionMode == 1) {
                if (currentEncoderValue < previousEncoderValue) {
                    currentGlobalSensitivity += 50;
                } else {
                    currentGlobalSensitivity -= 50;
                }
            }
            previousEncoderValue = currentEncoderValue;
        }
    }
}

void nextPattern() {
    currentPattern = (currentPattern + 1) % ARRAY_SIZE(patterns);
}

void previousPattern() {
    currentPattern = (currentPattern - 1) % ARRAY_SIZE(patterns);
}

int clampToRange(int numberToClamp, int lowerBound, int upperBound) {
    if (numberToClamp > upperBound) {
        return upperBound;
    } else if (numberToClamp < lowerBound) {
        return lowerBound;
    }
    return numberToClamp;
}

int clampSensitivity(int sensitivity) {
    return clampToRange(sensitivity, 0, 1023);
}

int wrapToRange(int numberToWrap, int lowerBound, int upperBound) {
    if (numberToWrap > upperBound) {
        return numberToWrap % upperBound;
    } else if (numberToWrap < lowerBound) {
        return numberToWrap += upperBound;
    }
    return numberToWrap;
}

void readAudioFrequencies() {
    // reset the data
    digitalWrite(RESET_PIN, HIGH);
    digitalWrite(RESET_PIN, LOW);

    // loop through all 7 bands
    for (int band = 0; band < 7; band++) {
        digitalWrite(STROBE_PIN, LOW); // go to the next band
        delayMicroseconds(50); // gather some data
        frequenciesLeft[band] = analogRead(LEFT_EQ_PIN); // store left band reading
        frequenciesRight[band] = analogRead(RIGHT_EQ_PIN); // store right band reading
        digitalWrite(STROBE_PIN, HIGH); // reset the strobe pin
    }
}

void rainbow() {
    if (frequenciesLeft[2] > 600) {
        fill_rainbow(borderLeds, NUM_TOTAL_LEDS, hueCounter, 7);
    }
}

// random colored speckles that blink in and fade smoothly
void confetti() {
    fadeToBlackBy(borderLeds, NUM_BORDER_LEDS, 10);
    fadeToBlackBy(allShelves, NUM_SHELF_LEDS, 10);
    uint8_t pos = random16(NUM_TOTAL_LEDS);

    if (frequenciesLeft[0] > 600) {
        if (pos > NUM_BORDER_LEDS) {
            allShelves[pos % NUM_BORDER_LEDS] += CHSV(hueCounter + random8(64), 200, 255);
        } else {
            borderLeds[pos] += CHSV(hueCounter + random8(64), 200, 255);
        }
    }
}

// a colored dot sweeping back and forth, with fading trails
void sinelon() {
    fadeToBlackBy(borderLeds, NUM_BORDER_LEDS, 5);
    fadeToBlackBy(allShelves, NUM_SHELF_LEDS, 5);
    int pos = beatsin16(13, 0, NUM_TOTAL_LEDS);
    if (frequenciesLeft[2] > 600) {
        if (pos > NUM_BORDER_LEDS) {
            allShelves[pos % NUM_BORDER_LEDS] += CHSV(hueCounter, 255, 192);
        } else {
            borderLeds[pos] += CHSV(hueCounter, 255, 192);
        }
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
    if (frequenciesLeft[1] > 600) {
        for (int i = 0; i < 8; i++) {
            borderLeds[beatsin16(i + 7, 0, NUM_BORDER_LEDS)] |= CHSV(dothue, 200, 255);
            dothue += 32;
        }
    }
}

void waterfall() {
    waterfallShelf(topShelfLeds, 6, clampSensitivity(currentGlobalSensitivity + 500));
    waterfallShelf(middleShelfLeds, 1, clampSensitivity(currentGlobalSensitivity + 500));
    waterfallShelf(bottomShelfLeds, 0, clampSensitivity(currentGlobalSensitivity + 500));
    waterfallBorder(4, clampSensitivity((currentGlobalSensitivity + 500)));
}

void waterfallCascading() {
    waterfallBorderCascading(4, clampSensitivity((currentGlobalSensitivity + 500)));
    waterfallShelf(topShelfLeds, 6, clampSensitivity(currentGlobalSensitivity + 500));
    waterfallShelf(middleShelfLeds, 1, clampSensitivity(currentGlobalSensitivity + 500));
    waterfallShelf(bottomShelfLeds, 0, clampSensitivity(currentGlobalSensitivity + 500));
}

void waterfallShelf(CRGB shelf[], int spectrum, int sensitivityThreashold) {
    if (frequenciesLeft[spectrum] > sensitivityThreashold) {
        shelf[LEDS_PER_SHELF / 2] = CHSV(map(frequenciesLeft[spectrum], sensitivityThreashold, 1023, 0, 255), 200, 255);
        shelf[LEDS_PER_SHELF / 2 + 1] = CHSV(map(frequenciesLeft[spectrum], sensitivityThreashold, 1023, 0, 255), 200,
                                             255);
    } else {
        shelf[LEDS_PER_SHELF / 2] = CRGB(0, 0, 0);
        shelf[LEDS_PER_SHELF / 2 + 1] = CRGB(0, 0, 0);
    }
    memmove(&shelf[0], &shelf[1], LEDS_PER_SHELF / 2 * sizeof(CRGB));
    memmove(&shelf[LEDS_PER_SHELF / 2], &shelf[LEDS_PER_SHELF / 2 - 1], LEDS_PER_SHELF / 2 * sizeof(CRGB));
}

void waterfallBorder(int spectrum, int sensitivityThreashold) {
    if (frequenciesRight[1] > sensitivityThreashold) {
        borderLeds[NUM_BORDER_LEDS / 2] = CHSV(map(frequenciesRight[spectrum], sensitivityThreashold, 1023, 0, 255),
                                               200, 255);
    } else {
        borderLeds[NUM_BORDER_LEDS / 2] = CRGB(0, 0, 0);
    }
    memmove(&borderLeds[0], &borderLeds[1], NUM_BORDER_LEDS / 2 * sizeof(CRGB));
    memmove(&borderLeds[NUM_BORDER_LEDS / 2], &borderLeds[NUM_BORDER_LEDS / 2 - 1], NUM_BORDER_LEDS / 2 * sizeof(CRGB));
}

void waterfallBorderCascading(int spectrum, int sensitivityThreashold) {
    if (frequenciesRight[1] > sensitivityThreashold) {
        borderLeds[NUM_BORDER_LEDS / 2] = CHSV(map(frequenciesRight[spectrum], sensitivityThreashold, 1023, 0, 255), 200, 255);
    } else {
        borderLeds[NUM_BORDER_LEDS / 2] = CRGB(0, 0, 0);
    }
    if(topShelfLeds[0]){
        borderLeds[42]+=topShelfLeds[0];
    }
    if(topShelfLeds[LEDS_PER_SHELF-1]) {
        borderLeds[104]+=topShelfLeds[LEDS_PER_SHELF-1];
    }
    if(middleShelfLeds[0]){
        borderLeds[21]+=middleShelfLeds[0];
    }
    if(middleShelfLeds[LEDS_PER_SHELF-1]) {
        borderLeds[125]+=middleShelfLeds[LEDS_PER_SHELF-1];
    }
    memmove(&borderLeds[0], &borderLeds[1], NUM_BORDER_LEDS / 2 * sizeof(CRGB));
    memmove(&borderLeds[NUM_BORDER_LEDS / 2], &borderLeds[NUM_BORDER_LEDS / 2 - 1], NUM_BORDER_LEDS / 2 * sizeof(CRGB));
}

void equalizerLeftToRightBottomToTop() {
    equalizerLeftBorder(0, clampSensitivity(currentGlobalSensitivity + 200), false);
    equalizerRightBorder(6, clampSensitivity(currentGlobalSensitivity + 200), false);
    equalizerTopBorder(5, clampSensitivity(currentGlobalSensitivity + 400), true);
    equalizerShelf(topShelfLeds, 4, clampSensitivity(currentGlobalSensitivity + 400), false);
    equalizerShelf(middleShelfLeds, 3, clampSensitivity(currentGlobalSensitivity + 400), true);
    equalizerShelf(bottomShelfLeds, 2, clampSensitivity(currentGlobalSensitivity + 400), false);
}

void equalizerRightToLeftBottomToTop() {
    equalizerLeftBorder(0, clampSensitivity(currentGlobalSensitivity + 200), false);
    equalizerRightBorder(6, clampSensitivity(currentGlobalSensitivity + 200), false);
    equalizerTopBorder(5, clampSensitivity(currentGlobalSensitivity + 400), false);
    equalizerShelf(topShelfLeds, 4, clampSensitivity(currentGlobalSensitivity + 400), true);
    equalizerShelf(middleShelfLeds, 3, clampSensitivity(currentGlobalSensitivity + 400), false);
    equalizerShelf(bottomShelfLeds, 2, clampSensitivity(currentGlobalSensitivity + 400), true);
}

void equalizerLeftBorder(int frequencyBin, int sensitivity, bool direction) {
    int ledsInSection = 62;
    borderLeds(0, ledsInSection).fadeToBlackBy(40);
    if (frequenciesLeft[frequencyBin] > sensitivity) {
        int numberToLight = map(frequenciesLeft[frequencyBin], sensitivity, 1023, 0, ledsInSection);
        CRGB color = CHSV(map(frequenciesLeft[frequencyBin], sensitivity, 1023, 0, 255), 200, 255);
        if (direction == true) {
            borderLeds(ledsInSection - numberToLight, ledsInSection);
        } else {
            borderLeds(0, numberToLight) = color;
        }
    }
}

void equalizerRightBorder(int frequencyBin, int sensitivity, bool direction) {
    int ledsInSection = 62;
    int locationOffset = 84;
    borderLeds(locationOffset, ledsInSection + locationOffset).fadeToBlackBy(40);
    if (frequenciesLeft[frequencyBin] > sensitivity) {
        int numberToLight = map(frequenciesLeft[frequencyBin], sensitivity, 1023, 0, ledsInSection);
        CRGB color = CHSV(map(frequenciesLeft[frequencyBin], sensitivity, 1023, 0, 255), 200, 255);
        if (direction == true) {
            borderLeds(locationOffset, locationOffset + numberToLight) = color;
        } else {
            borderLeds(locationOffset + ledsInSection - numberToLight, locationOffset + ledsInSection) = color;
        }
    }
}

void equalizerTopBorder(int frequencyBin, int sensitivity, bool direction) {
    int ledsInSection = 20;
    int locationOffset = 63;
    borderLeds(locationOffset, ledsInSection + locationOffset).fadeToBlackBy(45);
    if (frequenciesLeft[frequencyBin] > sensitivity) {
        int numberToLight = map(frequenciesLeft[frequencyBin], sensitivity, 1023, 0, ledsInSection);
        CRGB color = CHSV(map(frequenciesLeft[frequencyBin], sensitivity, 1023, 0, 255), 200, 255);
        if (direction == true) {
            borderLeds(locationOffset, locationOffset + numberToLight) = color;
        } else {
            borderLeds(locationOffset + ledsInSection - numberToLight, locationOffset + ledsInSection) = color;
        }
    }
}

void equalizerShelf(CRGBSet shelf, int frequencyBin, int sensitivity, bool direction) {
    fadeToBlackBy(shelf, LEDS_PER_SHELF, 45);
    if (frequenciesLeft[frequencyBin] > sensitivity) {
        int numberToLight = map(frequenciesLeft[frequencyBin], sensitivity, 1023, 0, LEDS_PER_SHELF);
        CRGB color = CHSV(map(frequenciesLeft[frequencyBin], sensitivity, 1023, 0, 255), 200, 255);
        if (direction == true) {
            shelf(LEDS_PER_SHELF - numberToLight - 1, LEDS_PER_SHELF - 1) = color;
        } else {
            shelf(0, numberToLight) = color;
        }
    }
}





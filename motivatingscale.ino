/*

   Motivational scale

   Required Arduino libraries:
   https://github.com/bogde/HX711
   http://fastled.io

   Required hardware:
   1 cheap bathroom scale,
   1 LED strip (WS2812B controllers or similar)
   1 Arduino nano or equivalent
   1 HX711 ADC
   1 9V battery

*/

// Include the HX711 library (for the scale ADC)
#include "HX711.h"
// Include the FastLED library for the LED strip
#include "FastLED.h"
// Include EEPROM library to save user data persistently
#include <EEPROM.h>

#define NUMBER_LEDS 10
#define NUMBER_USERS 2

//#define DEBUG

float scaleFactor = -20200.0; // roughly the scale for kg

// 10 coloured leds
CRGB leds[NUMBER_LEDS];

// HX711.DOUT  - pin #A1
// HX711.PD_SCK - pin #A0
HX711 scale(A1, A0);    // parameter "gain" is ommited; the default value 128 is used by the library

struct UserData {
  bool  returning;
  float latestReading;
  float latestPercentage;
};

UserData Users[NUMBER_USERS];

struct SetupData {
  bool initialized;
  int  clearMemory;
  long latestOffset;
} setupData;

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("V5");
#endif
  // 10 leds connected to digital pin 6, note that G and R are switched
  FastLED.addLeds<WS2812B, 6, GRB>(leds, NUMBER_LEDS).setCorrection( TypicalLEDStrip );

  for (int i = 0; i < NUMBER_LEDS; i++) {
    leds[i] = CRGB::Black;
  }

  leds[0] = CRGB::DarkOrange;
  leds[0].fadeLightBy(128);

  FastLED.show();

  int eeAddress = 0;
  EEPROM.get(eeAddress, setupData);
  setupData.clearMemory++;
  EEPROM.put(eeAddress, setupData);
  if (setupData.clearMemory > 1) {
    for (int i = 0; i < NUMBER_LEDS; i++) {
      leds[i] = CRGB::Red;
      FastLED.show();
      leds[i] = CRGB::Black;
      FastLED.delay(50);
    }
    for (int i = 0 ; i < EEPROM.length() ; i++) {
      EEPROM.write(i, 0);
    }
    for (int i = NUMBER_LEDS - 1; i >= 0; i--) {
      leds[i] = CRGB::Red;
      FastLED.show();
      leds[i] = CRGB::Black;
      FastLED.delay(50);
    }
    FastLED.show();
  }
  eeAddress += sizeof(SetupData);
  for (int i = 0; i < NUMBER_USERS; i++) {
    EEPROM.get(eeAddress, Users[i]);
    eeAddress += sizeof(UserData);
#ifdef DEBUG
    Serial.print("latestReading: ");
    Serial.println(Users[i].latestReading);
    Serial.println(Users[i].returning);
#endif
  }

  leds[1] = CRGB::DarkOrange;
  leds[1].fadeLightBy(128);
  leds[0].fadeLightBy(128);

  FastLED.show();

  scale.tare();

  scale.set_scale(scaleFactor);

  long new_offset = scale.get_offset();

  if (setupData.initialized) {
    double offs = double(new_offset) / double(setupData.latestOffset) - 1.0;
    if (offs < 0) offs = -offs;
#ifdef DEBUG
    if (offs > 0.1) {
      Serial.print("Offset difference too large ");
      Serial.println(offs);

    }
#endif
  }

  leds[2] = CRGB::DarkOrange;
  leds[2].fadeLightBy(128);
  leds[1].fadeLightBy(128);
  leds[0].fadeLightBy(128);

  FastLED.delay(1000);

  for (int i = 0; i < NUMBER_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  leds[0] = CRGB::Green;
  leds[0].fadeLightBy(128);
  FastLED.show();

  setupData.latestOffset = new_offset;
  setupData.initialized = true;
  setupData.clearMemory = 0;
  eeAddress = 0;
  EEPROM.put(eeAddress, setupData);
}

volatile int currentLed = 0;

enum States { WAITING_FOR_MEASUREMENT, SAVE_MEASUREMENT, DISPLAY_RESULT, STATE_DONE };
volatile int currentState = WAITING_FOR_MEASUREMENT;

int userno = -1;
float userperc = -1;
float latestMeasurement;
float prevPerc;

void loop() {

  switch (currentState) {
    case WAITING_FOR_MEASUREMENT:
      {
        float latest = scale.get_units(10);
#ifdef DEBUG
        Serial.print("avg reading: ");
        Serial.println(latest, 1);
#endif
        scale.power_down();              // put the ADC in sleep mode

        if (latest < 10.0) {
#ifdef DEBUG
          Serial.println("Waiting for a measurement...");
#endif
          latestMeasurement = 0.0;
          delay(5000);
          scale.power_up();
          break;
        }

        float meas = latestMeasurement / latest;
        meas = (meas > 0) ? meas : -meas;
        if (meas < 0.975 || meas > 1.025 ) {
#ifdef DEBUG
          Serial.println("Waiting to settle down...");
#endif
          latestMeasurement = latest;
          delay(3000);
          scale.power_up();
          break;
        }

        //for(int i=0;i<NUMBER_LEDS;i++) {
        //  leds[i] = CRGB::Black;
        //}
        leds[0].fadeLightBy(128);
        leds[1] = CRGB::Green;
        leds[1].fadeLightBy(128);
        FastLED.show();

        latestMeasurement = latest;

        userno = -1;
        int firstNew = -1;
        float minperc = 1.0, perc;
        float absperc;
        for (int i = 0; i < NUMBER_USERS; i++) {
          delay(50);
#ifdef DEBUG
          Serial.println(i);
#endif
          if (Users[i].returning) {
#ifdef DEBUG
            Serial.print("Checking returning user ");
            Serial.print(i);
            Serial.print(" : ");
#endif
            perc = 1.0 - Users[i].latestReading / latest;
#ifdef DEBUG
            Serial.println(perc);
#endif
            absperc = (perc >= 0 ? perc : -perc);
            if (absperc < minperc) {
              userno = i;
              minperc = absperc;
              userperc = perc;
            }
          } else {
#ifdef DEBUG
            Serial.print("User ");
            Serial.print(i);
            Serial.println(" new");
#endif
            if (firstNew < 0) firstNew = i;
          }
        }
        if (minperc > 0.1 && firstNew >= 0) {
#ifdef DEBUG
          Serial.print("Minimum percentage too large, create new user number ");
          Serial.println(firstNew);
#endif
          userno = firstNew;
          userperc = 0.0;
        }
#ifdef DEBUG
        Serial.print("Choosing user ");
        Serial.println(userno);
        Serial.print("Percentage ");
        Serial.println(userperc);
#endif
        delay(5000);

        currentState = SAVE_MEASUREMENT;
      }
      break;
    case SAVE_MEASUREMENT:
      {
        Users[userno].latestReading = latestMeasurement;
        if (Users[userno].returning) {
          prevPerc = Users[userno].latestPercentage;
          Users[userno].latestPercentage = userperc;
        } else {
          Users[userno].returning = true;
          Users[userno].latestPercentage = 0.0;
          prevPerc = 0.0;
        }
        int eeAddress = sizeof(SetupData);
        for (int i = 0; i < NUMBER_USERS; i++) {
          EEPROM.put(eeAddress, Users[i]);
          eeAddress += sizeof(UserData);
        }
#ifdef DEBUG
        Serial.println("Saved data");
#endif

        leds[0] = CRGB::Black; //.fadeLightBy(128);
        leds[1] = CRGB::Black; //.fadeLightBy(128);
        leds[userno] = CRGB::Blue;
        //leds[userno].fadeLightBy(128);

        FastLED.show();

        delay(3000);

        currentState = DISPLAY_RESULT;
      }
      break;
    case DISPLAY_RESULT:
      {
        for (int i = 0; i < NUMBER_LEDS; i++) {
          leds[i] = CRGB::Black;
        }
        if (userperc > -0.0025 && userperc < 0.0025) {
          leds[NUMBER_LEDS / 2 - 1] = CRGB::Green;
          leds[NUMBER_LEDS / 2] = CRGB::Red;
        } else {
          if (prevPerc > -0.0025 && prevPerc < 0.0025) {
            if (userperc > 0.0) {
              leds[NUMBER_LEDS / 2 + 1] = CRGB::Red;
              for (int i = NUMBER_LEDS / 2 + 2, j = 1 ; i < NUMBER_LEDS; i++, j++) {
                if (userperc > 0.005 * j) {
                  leds[i] = CRGB::Red;
                } else {
                  break;
                }
              }
            } else {
              leds[NUMBER_LEDS / 2 - 1] = CRGB::Green;
              for (int i = NUMBER_LEDS / 2 - 2, j = 1 ; i >= 0; i--, j++) {
                if (userperc < -0.005 * j) {
                  leds[i] = CRGB::Green;
                } else {
                  break;
                }
              }
            }
          } else {
            prevPerc = (prevPerc > 0) ? prevPerc : -prevPerc;
            if (userperc > 0.0) {
              leds[NUMBER_LEDS / 2 + 1] = CRGB::Red;
              for (int i = NUMBER_LEDS / 2 + 2, j = 1 ; i < NUMBER_LEDS; i++, j++) {
                if (userperc > prevPerc * j) {
                  leds[i] = CRGB::Red;
                } else {
                  break;
                }
              }
            } else {
              leds[NUMBER_LEDS / 2 - 1] = CRGB::Green;
              for (int i = NUMBER_LEDS / 2 - 2, j = 1 ; i >= 0; i--, j++) {
                if (userperc < -0.5 * prevPerc * j) {
                  leds[i] = CRGB::Green;
                } else {
                  break;
                }
              }
            }
          }
        }
        FastLED.show();
        currentState = STATE_DONE;

      }
      break;
    case STATE_DONE:
      FastLED.delay(10000);
  }

  /*
    int eeAddress = sizeof(SetupData);
    for(int i=0; i<NUMBER_USERS; i++) {
      EEPROM.put(eeAddress, Users[i]);
      eeAddress += sizeof(UserData);
    }
  */
  //leds[0] = CRGB::Blue;leds[1] = CRGB::Black; FastLED.show(); delay(30);
  //leds[0] = CRGB::Black;leds[1] = CRGB::Red; FastLED.show(); delay(30);

  //delay(5000);
  //scale.power_up();
}


/*

 DATO MIDI sequencer
 Copyright 2015 David Menting | Nut & Bolt <david@nut-bolt.nl>

 Eight step midi note sequencer coupled to an eight note keyboard.

 Played notes are stored in the sequencer with an initial velocity of 100.
 The notes 'fade out', reducing in velocity every time they are played.

 The sequencer sends on MIDI channel 1. Tempo is regulated by a potmeter.
 Sends a sync pulse to sync with Volca/Pocket Operator gear on pin 2.

*/

#include <MIDI.h>
#include <Keypad.h>
#include <FastLED.h>


// LED strip
const unsigned char LED_DT = 17;       // Data pin to connect to the strip.
#define COLOR_ORDER GRB                // It's GRB for WS2812B
#define LED_TYPE WS2812                // What kind of strip are you using (WS2801, WS2812B or APA102)?
const unsigned char NUM_LEDS = 8;      // Number of LED's.
struct CRGB leds[NUM_LEDS];            // Initialize our LED array.


// Pins
const unsigned char TEMPO_PIN = 14;
const unsigned char SYNC_PIN = 2;
const unsigned char EXPR_PIN = A9;


// Key matrix
const byte ROWS = 4;
const byte COLS = 4; 
byte row_pins[ROWS] = {20, 23, 22, 21}; //connect to the row pinouts of the keypad
byte col_pins[COLS] = {12, 18, 16, 15}; //connect to the column pinouts of the keypad

char keys[ROWS][COLS] = {
  {'A','B','C','D'},
  {'E','F','G','H'},
  {'a','b','c','d'},
  {'e','f','g','h'}
};
Keypad keypad = Keypad( makeKeymap(keys), row_pins, col_pins, ROWS, COLS );


// MIDI & sync settings
const int CHANNEL = 1;
const int GATE_LENGTH = 40;
const int SYNC_LENGTH = 1;


// Sequencer settings
const int NUM_STEPS = 8;
const int VELOCITY_STEP = 5;
const int INITIAL_VELOCITY = 100;
const int VELOCITY_THRESHOLD = 50;
unsigned int current_step = 0;
unsigned long next_step, gate_off, sync_off;

//const int SCALE[] = {46,49,51,54,56,58,61,63};
const int SCALE[] = {46,49,51,54,61,63,66,68}; // Low with 2 note split
//const int SCALE[] = {58,61,63,66,73,75,78,80};
//const int SCALE[] = {61,63,66,68,75,78,80,82};

// Note to colour mapping
const CRGB COLOURS[] = {
  CRGB::Purple,
  0xCC0000,
  0xFF6322,
  0x999900,
  0x77DD00,
  CRGB::Green,
  0x008060,
  CRGB::Blue
};

// Initial sequencer values
byte step_note[] = { 0,1,2,3,4,5,6,7 };
byte step_enable[] = { 1,1,1,1,1,1,1,1 };
int step_velocity[] = { 127,127,127,127,127,127,127,127 };
char set_key = 9;

void setup() {
  FastLED.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(32);        // Use this for WS2812B
  FastLED.clear();
  FastLED.show();

  keypad.setDebounceTime(5);

  pinMode(13, OUTPUT);
  pinMode(TEMPO_PIN, INPUT);
  pinMode(SYNC_PIN, OUTPUT);

  MIDI.begin();  

  for(int i = 0; i < 127; i++) {
    MIDI.sendNoteOff(i, 100, CHANNEL);
  }
}

void loop() {

  for(int s = 0; s < 8; s++) {

    FastLED.clear();

    if(step_velocity[s] <= VELOCITY_STEP) { /*step_velocity[s] = 0; step_enable[s] = 0;*/}
    else{step_velocity[s] -= VELOCITY_STEP;}

    Serial.print(s, DEC);
    Serial.print(" v:");
    Serial.print(step_velocity[s],DEC);
    Serial.print(" ");

    for(int l = 0; l < 8; l++) {
      if(step_enable[l]) {
        leds[l] = COLOURS[step_note[l]];
        if(step_velocity[l] < VELOCITY_THRESHOLD) {
          leds[l].nscale8_video(step_velocity[l]+20);
        }
      }
      else leds[l] = CRGB::Black;
      leds[s] = CRGB::White;
    }

    FastLED.show();

    if(set_key < 9) {
      step_note[s] = set_key;
      step_enable[s] = 1;
      step_velocity[s] = INITIAL_VELOCITY;
      set_key = 9;
    }

    digitalWrite(SYNC_PIN, HIGH); // Volca sync
    sync_off = millis() + SYNC_LENGTH;

    if(step_enable[s]) {
      MIDI.sendNoteOn(SCALE[step_note[s]], step_velocity[s], CHANNEL);
    }

    while(millis() < sync_off) {
      handle_keys();
    } 
    digitalWrite(SYNC_PIN, LOW);


    gate_off = millis() + GATE_LENGTH;


    while(millis() < gate_off) {
      handle_keys();
      if(set_key < 9) {
        step_enable[s] = 1;
        step_velocity[s] = INITIAL_VELOCITY;

        if(set_key != step_note[s]) {
          MIDI.sendNoteOff(SCALE[step_note[s]], 64, CHANNEL);
          step_note[s] = set_key;
        }
        set_key = 9;
      }
    }

    MIDI.sendNoteOff(SCALE[step_note[s]], 64, CHANNEL);

    int tempo = map(analogRead(TEMPO_PIN),0,1023,200,GATE_LENGTH);
    next_step = millis() + tempo;

    while(millis() < next_step) {
      handle_keys();
      if(set_key < 9) {
        if(s == 7) {
          step_note[0] = set_key;
          step_enable[0] = 1;
          step_velocity[0] = INITIAL_VELOCITY;
        }
        else {
          step_note[s+1] = set_key;
          step_enable[s+1] = 1;
          step_velocity[s+1] = INITIAL_VELOCITY;
        }
        set_key = 9;
      }
    }
  }
}

void handle_keys() {
   // Fills keypad.key[ ] array with up-to 10 active keys.
  // Returns true if there are ANY active keys.
  if (keypad.getKeys())
  {
    for (int i=0; i<LIST_MAX; i++)   // Scan the whole key list.
    {
      if ( keypad.key[i].stateChanged )   // Only find keys that have changed state.
      {
        switch (keypad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
            case PRESSED:
                //msg = " PRESSED.";
                // we want to do something here
                // either toggle the active step
                char k = keypad.key[i].kchar;
                if(k >= 'a') {
                  step_enable[k-'a'] = 1-step_enable[k-'a'];
                  step_velocity[k-'a'] = INITIAL_VELOCITY;
                } else {
                  //step_note
                  set_key = k - 'A';
                }
                // or set the note
                break;
            /*case HOLD:
                msg = " HOLD.";
                break;
            case RELEASED:
                msg = " RELEASED.";
                break;
            case IDLE:
                msg = " IDLE.";
                break;
            //*/
        }
      }
    }
  }
}
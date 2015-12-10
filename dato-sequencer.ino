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
const int MIN_TEMPO = 300; // Tempo is actually an interval in ms

// Sequencer settings
const int NUM_STEPS = 8;
const int VELOCITY_STEP = 5;
const int INITIAL_VELOCITY = 100;
const int VELOCITY_THRESHOLD = 50;
unsigned int current_step = 0, tempo;
unsigned long next_step, gate_off, sync_off;


const int SCALE[] = { 46,49,51,54,61,63,66,68 }; // Low with 2 note split
//const int BLACK_KEYS[] = {22,25,27,30,32,34,37,39,42,44,46,49,51,54,56,58,61,63,66,68,73,75,78,80};


// Note to colour mapping
const CRGB COLOURS[] = {
  0x990066,
  0xCC0000,
  0xCC6300,
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
    
    current_step = s;

    // Decrement the velocity of the current note. If minimum velocity is reached leave it there
    if(step_velocity[s] <= VELOCITY_STEP) { /*step_velocity[s] = 0; step_enable[s] = 0;*/ }
    else{step_velocity[s] -= VELOCITY_STEP;}

    handle_leds();

    digitalWrite(SYNC_PIN, HIGH); // Volca sync on

    sync_off = millis() + SYNC_LENGTH;
    
    if(step_enable[s]) {
      MIDI.sendNoteOn(SCALE[step_note[s]], step_velocity[s], CHANNEL);
    }
    gate_off = millis() + GATE_LENGTH;

    while(millis() < sync_off) {
      handle_keys();
    } 

    digitalWrite(SYNC_PIN, LOW); // Volca sync off

    while(millis() < gate_off) {
      handle_keys();
    }

    MIDI.sendNoteOff(SCALE[step_note[s]], 64, CHANNEL);

    next_step = millis() + get_tempo();

    // Very crude sequencer off implementation
    while(get_tempo() >= MIN_TEMPO)
    {
      delay(20);
      play_keys();
      tempo = get_tempo();
    }

    // Advance the step number already, so any pressed keys end up in the next step
    if(s == 7) current_step = 0;
    else current_step = s+1;

    while(millis() < next_step) {
      handle_keys();
    }
  }
}

// Updates the LED colour and brightness to match the stored sequence
void handle_leds() {
  FastLED.clear();

  for(int l = 0; l < 8; l++) 
  {
    if(step_enable[l])
    {
      leds[l] = COLOURS[step_note[l]];

      if(step_velocity[l] < VELOCITY_THRESHOLD) 
      {
        leds[l].nscale8_video(step_velocity[l]+20);
      }
    }
    else leds[l] = CRGB::Black;
    leds[current_step] = CRGB::White;
  }

  FastLED.show();
}

// Scans the keypad and handles step enable and keys
void handle_keys() {
   // Fills keypad.key[ ] array with up-to 10 active keys.
  // Returns true if there are ANY active keys.
  if (keypad.getKeys())
  {
    for (int i=0; i<LIST_MAX; i++)   // Scan the whole key list.
    {
      if ( keypad.key[i].stateChanged )   // Only find keys that have changed state.
      {
        char k = keypad.key[i].kchar;
        switch (keypad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
            case PRESSED:    
                if(k >= 'a') {
                  step_enable[k-'a'] = 1-step_enable[k-'a'];
                  step_velocity[k-'a'] = INITIAL_VELOCITY;
                } else {
                  step_note[current_step] = set_key = k - 'A';
                  step_enable[current_step] = 1;
                  step_velocity[current_step] = INITIAL_VELOCITY;
                }
                break;
            /*case HOLD:
                break;*/
            case RELEASED:
                if(k < 'a') {
                  MIDI.sendNoteOff(SCALE[k-'A'], 64, CHANNEL);
                }
                break;
            /*case IDLE:
                break;
            */
        }
      }
    }
  }
}

// Sends midi noteon as keys are played and noteoff as keys are released
void play_keys() {
  // Fills keypad.key[ ] array with up-to 10 active keys.
  // Returns true if there are ANY active keys.
  if (keypad.getKeys())
  {
    for (int i=0; i<LIST_MAX; i++)   // Scan the whole key list.
    {
      if ( keypad.key[i].stateChanged )   // Only find keys that have changed state.
      {
        char k = keypad.key[i].kchar;
        switch (keypad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
            case PRESSED:
                if(k < 'a') {
                  MIDI.sendNoteOn(SCALE[k-'A'], INITIAL_VELOCITY, CHANNEL);
                }
                break;
            /*case HOLD:
                break;*/
            case RELEASED:
                if(k < 'a') {
                  MIDI.sendNoteOff(SCALE[k-'A'], 64, CHANNEL);
                }
                break;
            /*case IDLE:
                break;
            */
        }
      }
    }
  }
}

int get_tempo() {
  return map(analogRead(TEMPO_PIN),0,1023,MIN_TEMPO,GATE_LENGTH);
}
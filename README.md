# DATO MIDI sequencer

 Copyright 2015 David Menting | Nut & Bolt <david@nut-bolt.nl>

 Eight step midi note sequencer coupled to an eight note keyboard.

 Played notes are stored in the sequencer with an initial velocity of 100.
 The notes 'fade out', reducing in velocity every time they are played.

 The sequencer sends on MIDI channel 1. Tempo is regulated by a potmeter.
 Sends a sync pulse to sync with Volca/Pocket Operator gear on pin 2.

 It runs on a Teensy LC or 3.1. Push buttons are read out in a matrix. 
 LEDs are WS2812 individually addressable.
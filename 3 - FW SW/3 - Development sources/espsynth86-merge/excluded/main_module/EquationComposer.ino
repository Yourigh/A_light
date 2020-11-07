/*
 ___  __        __  ___ _  __           ___  __        __   __   ___  ___  __  
|___ |  | |  | |__|  |  | |  | |\ |    |    |  | |\/| |__] |  | [__  |___ |__/ 
|___ |_\| |__| |  |  |  | |__| | \|    |___ |__| |  | |    |__| ___] |___ |  \ 

                                                         // by microbe modular
// =============================================================================
       
Copyright 2014 Bret Truchan

  The Equation Composer source code is free software: you can redistribute it 
  and/or modify it under the terms of the GNU General Public License as 
  published by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
                                                                       
Based on:
  http://rcarduino.blogspot.com/2012/12/arduino-due-dds-part-1-sinewaves-and.html
  Equation playback based on techniques documented by Viznut

Credits:
  Circuit/PCB design, parts selection, and manufacturing oversight by John Staskevich
  Front panel design by Hannes Pasqualini
  Sound design by Sunsine Audio and Bret Truchan
  Some equations gathered from around the net for which there was no attribution
  Includes some libraries from Mozzi (http://sensorium.github.io/Mozzi/)
  Additional programming by Gaétan Ro
  Thanks to Ivan Seidel Gomes for creating the Due Timer library
  Significant parts of the circuitry come directly from the Arduino Due Reference Design
  Special thanks to Josh from Toppobrillo for his support and mentoring

Equation testing tools:
  http://wurstcaptures.untergrund.net/music/
  http://entropedia.co.uk/generative_music/
  http://dev.fritzu.com/bbx/
  Note: When using any bytebeat tool, make sure to set the output rate to 22,050  

TODO:
  - Make inverter module
  
  
// =========================== - 80 column - ===================================
12345678901234567890123456789012345678901234567890123456789012345678901234567890

*/




#include "Defines.h"

// Setting the debug flag to 1 turns off the timer functions.  This causes the
// code to run slowly, and at that slow frequency the module is inaudible.  
// However, it does allow for us to add Serial.println() calls within the code, 
// which normally woudn't work because the interrupt code gets called at
// a higher frequency than the serial port can function.
//
// So, in other words, if you want to use Serial.println(), uncomment this line
// but don't expect to hear any audio output from the module.
//
// Don't ever set these to '0'.  Instead, comment them out.
//
// #define DEBUG 1

// Include all of the equation banks
#include "EquationBankKhepri.h"
#include "EquationBankPtah.h"
#include "EquationBankSobek.h"

// More traditional array-based wavetables
#include "GlobalWavetables.h"

// Include each synth
#include "Synth3Osc.h"
#include "SynthArpeggio1.h"
#include "SynthAutoDrum.h"
#include "SynthChords.h"
#include "SynthClickers.h"
#include "SynthDrumPlayer.h"
#include "SynthDrumSelektor.h"
#include "SynthEquationPlayer.h"
#include "SynthEquationLooper.h"
#include "SynthLooper.h"
#include "SynthMini.h"
#include "SynthMumbler.h"
#include "SynthPatterns.h"
#include "SynthWavetable.h"
#include "SynthWavetableFolder.h"
#include "SynthWavetableDelay.h"

// DueTimer is a Due library for handling interrupts
#include "DueTimer.h"

  
// Create an inputs object, which contains a bunch of input modules and serves
// as a convienient wrapper class.
Inputs *inputs = new Inputs();

// Array to speed up main loop by removing call to map( ... ) when
// mapping the prg input to the synths array.
uint16_t prg_input_mapping[4095];

// Load up equation banks.  These are where the equations are defined.
// Equation Banks are only used in some of the synths.
EquationBankKhepri *equation_bank_khepri = new EquationBankKhepri();
EquationBankPtah *equation_bank_ptah = new EquationBankPtah();
EquationBankSobek *equation_bank_sobek = new EquationBankSobek();

// Instantiate synths, which are selectable via the PRG knob.
// Any new synth must be added to this list

#define NUMBER_OF_SYNTHS 10

Synth *active_synths[] {
  new SynthEquationPlayer(inputs, equation_bank_khepri),
  new SynthEquationPlayer(inputs, equation_bank_ptah),
  new SynthEquationPlayer(inputs, equation_bank_sobek),
  new SynthDrumSelektor(inputs),  
  new SynthWavetableFolder(inputs),
  new SynthPatterns(inputs),
  new SynthChords(inputs),
  new Synth3Osc(inputs),
  new SynthDrumPlayer(inputs),
  new SynthLooper(inputs)
};


// The 'cycle' variable increments every time the interrupt is called.
// Modules use this counter to determine if their output has already been calculated
// during the interrupt cycle.  If it has, then the module won't bother calculating its 
// output until the next cycle.  This is important for situations where one module's 
// output is fed into the inputs of two different modules.  Without this type of 
// "output caching", the parent module's code could be run twice unnecessarily.
uint8_t cycle = 0;

// Currently selected synth
uint8_t synth = 0;


void setup()
{

  // A note about using Serial.println() for testing:
  // Testing using Serial.println() is difficult because you can't put a 
  // Serial.println() in the code that's called by the timer interrupt.  The only 
  // way I've been able to use Serial.println() is by temporarily removing the 
  // timer and by calling the TC4_Handler() function myself from within the main loop.
  // Using the DEBUG flag does just that.  It disabled the interrupt and instead
  // calls TC4_Handler from the main loop().
  
  #ifdef DEBUG
    Serial.begin(9600);
  #endif

  // Set the Due's analog read resolution
  // The LSB is shaved off in ModuleAnalogInput.cpp to reduce noise
  analogReadResolution(ANALOG_READ_RESOLUTION);

  // Enable the DAC
  analogWrite(DAC1, 0);

  // Set the pinmode for digital pins.  This is not required for the analog inputs.
  pinMode(PIN_GATE, INPUT);

  // Pre-compute the prg_input_mapping array.
  for(uint16_t i=0; i < 4096; i++)
  {
    prg_input_mapping[i] = map(i, 0, MAX_CV, 0, NUMBER_OF_SYNTHS);
  }

  // Configure the interrupt(s) if NOT in debug mode.  
  // Notice that's ifndef (with an 'n'), not ifdef.
  #ifndef DEBUG
    Timer0.attachInterrupt(audioRateInterrupt).setFrequency(SAMPLE_RATE).start();
  #endif
  
}

void loop()
{  
  // In the main loop, the analog and digital inputs are read. The values are 
  // stored in the global input modules as well as being returned.  It wouldn't
  // make sense to poll the Arduino inputs in the timer function TC4_Handler()
  // because that function gets called at a higher frequency than the analog
  // inputs can be read (I'm assuming!).  That's why they're polled here, in
  // the main loop, at a reasonable rate.

  inputs->read();

  synth = prg_input_mapping[inputs->prg->getValue()];

  // If in debug mode, then call the audio_rate_interrupt manually
  #ifdef DEBUG
    audioRateInterrupt();
  #endif
}

void audioRateInterrupt()
{
  // I'm using dacc_write_conversion_data() because it writes 12-bit data to
  // the DAC as opposed to 8-bit resolution that analogWrite() does.
  dacc_write_conversion_data(DACC_INTERFACE, active_synths[synth]->run(cycle));
  
  // Increment the global time.  This variable is used by modules in their
  // output caching code. (see Module.cpp)
  cycle++;
}
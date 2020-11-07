/*
 *   +----------------------+
 *   | ModuleQuantizer      |
 *   |----------------------|
 *   > cv_input             |
 *   > scale_input          |
 *   |               output >
 *   +----------------------+
 *
 */
// =============================================================================
// 
// ModuleQuantizer is a simple note quantizer based on scales.
//
// There are two inputs to the ModuleQuantizer:
//
//   cv_input: The signal to quantize
//   scale_input: Selects the scale for quantization
//
// The scales themselves are defined in GlobalScales.cpp:
//
//   0: CHROMATIC
//   1: IONIAN
//   2: DORIAN
//   3: LYDIAN
//   4: PHRYGIAN
//   5: MIXOLYDIAN
//   6: AEOLIAN
//   7: LOCRIAN
//   8: MAJOR_PENTATONIC
//   9: MINOR_PENTATONIC
//   10: BLUES
//   11: DIMINISHED
//   12: ARABIAN
//   13: MAJOR
//   14: MINOR
//   15: PRISM
//
// Example usage:
//
//   ModuleQuantizer *quantizer = new ModuleQuantizer();
//   ModuleWavetableOsc *osc = new ModuleWavetableOsc();
//
//   osc->wavetable_input  = inputs->mod;
//   osc->frequency_input  = inputs->sr;
//
//   quantizer->cv_input = osc;
//   quantizer->scale_input = inputs->param1;
//
//   this->last_module = quantizer;
//
// Also see: SynthPatterns.cpp

#ifndef ModuleQuantizer_h
#define ModuleQuantizer_h

#include "Arduino.h"
#include "Module.h"

class ModuleQuantizer : public Module
{
  
  public:
    uint16_t compute();
    
    // Inputs
    Module *cv_input;
    Module *scale_input;

};

#endif

// =============================================================================
//
// Name: SynthMumbler
// Type: Audio
// Written by: Gaetan Ro, 2014
// 
// Description: Mumbler is a Formant Synthesizer with a built-in lowpass filter 
// that makes sounds similar to vowel sounds.
//
// SR  - Sample Rate (pitch)
// MOD - Formant scale input
// [1] - Phase input
// [2] - Lowpass Filter Resonance input
// [3] - Lowpass Filter Cutoff Input
// GATE - Not used
//
// =============================================================================

#ifndef SynthMumbler_h
#define SynthMumbler_h

#include "Arduino.h"
#include "Synth.h"

class SynthMumbler : public Synth
{
  public:
  
    SynthMumbler(Inputs* inputs); 
};

#endif

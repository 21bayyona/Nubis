// =============================================================================
//  NUBIS - Parameters.h : parameter IDs, layout and modulation destinations
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EffectsEngine.h"
#include "dsp/ModulationEngine.h"

namespace nubis::pid
{
// global
inline constexpr const char* source = "source", *internalType = "internalType", *playMode = "playMode",
    *inputGain = "inputGain", *dryWet = "dryWet", *outputGain = "outputGain", *limiter = "limiter";
// grain engine
inline constexpr const char* size = "size", *density = "density", *position = "position", *posRandom = "posRandom",
    *pitch = "pitch", *pitchRandom = "pitchRandom", *jitter = "jitter", *overlap = "overlap", *spread = "spread",
    *pan = "pan", *reverse = "reverse", *direction = "direction", *envelope = "envelope", *freeze = "freeze";
// texture engine
inline constexpr const char* texture = "texture", *smoothness = "smoothness", *blur = "blur", *randomness = "randomness",
    *diffusion = "diffusion", *thickness = "thickness", *width = "width", *feedback = "feedback";
// special
inline constexpr const char* infFreeze = "infFreeze", *spectralMode = "spectralMode", *spectralAmount = "spectralAmount",
    *chaos = "chaos", *evolve = "evolve", *evolveRate = "evolveRate", *reverseCloud = "reverseCloud", *drone = "drone";
// fx
inline constexpr const char* revSize = "rev_size", *revDecay = "rev_decay", *revDamp = "rev_damp", *revPredelay = "rev_predelay", *revMix = "rev_mix",
    *dlyTime = "dly_time", *dlySync = "dly_sync", *dlyDiv = "dly_div", *dlyFb = "dly_fb", *dlyPP = "dly_pp", *dlyTone = "dly_tone", *dlyMix = "dly_mix",
    *distDrive = "dist_drive", *distType = "dist_type", *distTone = "dist_tone", *distMix = "dist_mix",
    *crushBits = "crush_bits", *crushDown = "crush_down", *crushMix = "crush_mix",
    *filtCut = "filt_cut", *filtRes = "filt_res", *filtMode = "filt_mode", *filtDrive = "filt_drive",
    *chRate = "ch_rate", *chDepth = "ch_depth", *chMix = "ch_mix",
    *phRate = "ph_rate", *phDepth = "ph_depth", *phFb = "ph_fb", *phMix = "ph_mix",
    *fsShift = "fs_shift", *fsMix = "fs_mix", *fsFb = "fs_fb",
    *imgWidth = "img_width", *imgBass = "img_bass";

// indexed helpers
juce::String macro (int i);                       // macro1..4
juce::String lfo (int i, const char* field);      // lfo1_rate ...
juce::String env (int i, const char* field);      // env1_a ...
juce::String step (int i);                        // seq_step1..16
inline constexpr const char* rndRate = "rnd_rate", *rndSmooth = "rnd_smooth", *seqLen = "seq_len", *seqDiv = "seq_div", *seqGlide = "seq_glide";
juce::String modSrc (int slot);
juce::String modDst (int slot);
juce::String modAmt (int slot);
juce::String fxOn (int fx);
} // namespace nubis::pid

namespace nubis
{
/** Parameters that can be targeted by the modulation matrix. */
struct ModDestination { const char* paramId; const char* label; };
extern const std::vector<ModDestination> kModDestinations;
int findModDestination (const juce::String& paramId);

/** Parameters that preset loading never touches (they describe the routing context). */
bool isGlobalContextParam (const juce::String& paramId);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool instrumentBuild);
} // namespace nubis

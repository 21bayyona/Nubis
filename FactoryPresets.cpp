// =============================================================================
//  NUBIS - FactoryPresets.cpp : 32 original factory presets in 10 categories.
//  Values are plain units (ms, Hz, st, 0..1). Choices are indices.
//  Macros are pre-routed in every preset so they are playable immediately.
// =============================================================================
#include "PresetManager.h"
#include "dsp/ModulationEngine.h"

namespace nubis
{
namespace
{
    // modulation sources
    [[maybe_unused]] constexpr int L1 = SrcLfo1, L2 = SrcLfo2, L3 = SrcLfo3, L4 = SrcLfo4, E1 = SrcEnv1, E2 = SrcEnv2, RN = SrcRandom, SQ = SrcSeq;
    [[maybe_unused]] constexpr int M1 = SrcMacro1, M2 = SrcMacro2, M3 = SrcMacro3, M4 = SrcMacro4;

    // shapes / divisions / misc indices
    [[maybe_unused]] constexpr float SINE = 0, TRI = 1, SAW = 2, SQR = 3, RNDS = 4, SH = 5;
    [[maybe_unused]] constexpr float D16T = 2, D16 = 3, D16D = 4, D8T = 5, D8 = 6, D8D = 7, D4T = 8, D4 = 9, D4D = 10, D2 = 11, BAR = 12;
    [[maybe_unused]] constexpr float HANN = 0, GAUSS = 1, TRIENV = 2, TRAP = 3, PERC = 4, SWELL = 5, TUKEY = 6;
    [[maybe_unused]] constexpr float CHROM = 1, MAJ = 2, MIN = 3, PENT = 4, FIFTHS = 5, HARM = 6;
    [[maybe_unused]] constexpr float LP = 0, HP = 1, BP = 2;
    [[maybe_unused]] constexpr float SOFT = 0, FOLD = 2, TUBE = 3;
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
    //==========================================================================
    // ATMOSPHERIC
    //==========================================================================
    { "Glass Horizon", "Atmospheric",
      { { "size", 380 }, { "density", 22 }, { "posRandom", 0.35f }, { "pitchRandom", 0.12f }, { "overlap", 0.9f }, { "spread", 0.8f },
        { "texture", 0.35f }, { "smoothness", 0.25f }, { "diffusion", 0.5f }, { "blur", 0.2f }, { "width", 1.4f }, { "dryWet", 0.7f },
        { "lfo1_rate", 0.07f }, { "lfo1_shape", SINE }, { "lfo2_rate", 0.11f }, { "lfo2_shape", TRI },
        { "fx_REV_on", 1 }, { "rev_size", 0.85f }, { "rev_decay", 7 }, { "rev_mix", 0.45f }, { "fx_CHOR_on", 1 }, { "ch_mix", 0.3f } },
      { { L1, "position", 0.15f }, { L2, "blur", 0.25f }, { M1, "size", 0.4f }, { M1, "diffusion", 0.4f }, { M2, "rev_mix", 0.5f }, { M3, "pitchRandom", 0.4f } } },

    { "Morning Haze", "Atmospheric",
      { { "size", 600 }, { "density", 12 }, { "posRandom", 0.2f }, { "jitter", 0.6f }, { "overlap", 1 }, { "smoothness", 0.45f },
        { "spectralMode", MAJ }, { "spectralAmount", 0.5f }, { "pitchRandom", 7 }, { "evolve", 0.4f }, { "evolveRate", 0.06f }, { "dryWet", 0.65f },
        { "fx_REV_on", 1 }, { "rev_decay", 5 }, { "rev_mix", 0.4f }, { "fx_DLY_on", 1 }, { "dly_div", D4D }, { "dly_fb", 0.35f }, { "dly_pp", 0.8f }, { "dly_tone", 0.4f }, { "dly_mix", 0.2f },
        { "lfo1_rate", 0.13f } },
      { { L1, "pan", 0.3f }, { M1, "smoothness", 0.5f }, { M2, "spectralAmount", 0.5f }, { M3, "evolve", 0.6f }, { M4, "dly_mix", 0.5f } } },

    { "Aurora Veil", "Atmospheric",
      { { "size", 900 }, { "density", 8 }, { "overlap", 1 }, { "pitch", 12 }, { "pitchRandom", 0.2f }, { "reverse", 0.3f }, { "blur", 0.45f },
        { "diffusion", 0.6f }, { "width", 1.6f }, { "evolve", 0.5f }, { "evolveRate", 0.05f }, { "dryWet", 0.75f },
        { "fx_SHIFT_on", 1 }, { "fs_shift", 4 }, { "fs_mix", 0.3f }, { "fx_REV_on", 1 }, { "rev_size", 0.95f }, { "rev_decay", 12 }, { "rev_mix", 0.5f } },
      { { M1, "pitch", -0.25f }, { M2, "blur", 0.5f }, { M3, "fs_shift", 0.15f }, { M4, "rev_size", -0.6f } } },

    //==========================================================================
    // EXPERIMENTAL
    //==========================================================================
    { "Particle Collider", "Experimental",
      { { "size", 18 }, { "density", 220 }, { "jitter", 1 }, { "posRandom", 0.8f }, { "pitchRandom", 18 }, { "spread", 1 }, { "randomness", 0.7f },
        { "chaos", 0.6f }, { "texture", 0.8f }, { "dryWet", 0.8f }, { "fx_REV_on", 1 }, { "rev_mix", 0.25f },
        { "fx_CRUSH_on", 1 }, { "crush_bits", 10 }, { "crush_down", 3 }, { "crush_mix", 0.4f },
        { "fx_FILT_on", 1 }, { "filt_mode", BP }, { "filt_cut", 2400 }, { "filt_res", 0.5f },
        { "lfo1_shape", SH }, { "lfo1_rate", 6 } },
      { { L1, "filt_cut", 0.35f }, { M1, "density", -0.5f }, { M2, "chaos", 0.4f }, { M3, "crush_bits", -0.5f }, { M4, "filt_res", 0.4f } } },

    { "Quantum Foam", "Experimental",
      { { "size", 6 }, { "density", 380 }, { "jitter", 0.9f }, { "pitch", -12 }, { "pitchRandom", 24 }, { "texture", 1 },
        { "spectralMode", HARM }, { "spectralAmount", 0.8f }, { "dryWet", 0.8f },
        { "fx_SHIFT_on", 1 }, { "fs_shift", -140 }, { "fs_fb", 0.4f }, { "fs_mix", 0.5f }, { "fx_PHAS_on", 1 }, { "ph_mix", 0.4f },
        { "rnd_rate", 4 }, { "rnd_smooth", 0.2f } },
      { { RN, "fs_shift", 0.1f }, { M1, "size", 0.6f }, { M2, "spectralAmount", -0.8f }, { M3, "fs_fb", 0.5f }, { M4, "density", -0.4f } } },

    { "Inside Out", "Experimental",
      { { "reverse", 1 }, { "direction", -0.6f }, { "size", 260 }, { "density", 30 }, { "feedback", 0.6f }, { "diffusion", 0.3f }, { "dryWet", 0.75f },
        { "fx_SHIFT_on", 1 }, { "fs_shift", 37 }, { "fs_mix", 0.4f }, { "fx_DLY_on", 1 }, { "dly_div", D8T }, { "dly_fb", 0.55f }, { "dly_mix", 0.3f },
        { "lfo1_shape", TRI }, { "lfo1_rate", 0.2f } },
      { { L1, "direction", 0.4f }, { M1, "feedback", 0.35f }, { M2, "direction", 0.8f }, { M3, "fs_shift", 0.2f }, { M4, "dly_fb", 0.4f } } },

    { "Spectral Lattice", "Experimental",
      { { "size", 160 }, { "density", 45 }, { "jitter", 0.05f }, { "pitchRandom", 24 }, { "spectralMode", HARM }, { "spectralAmount", 1 },
        { "blur", 0.35f }, { "thickness", 0.4f }, { "spread", 0.9f }, { "dryWet", 0.8f }, { "fx_REV_on", 1 }, { "rev_mix", 0.35f },
        { "fx_PHAS_on", 1 }, { "ph_rate", 0.08f }, { "ph_fb", 0.6f }, { "ph_mix", 0.45f }, { "lfo2_rate", 0.17f }, { "lfo2_shape", RNDS } },
      { { L2, "jitter", 0.4f }, { M1, "spectralAmount", -0.7f }, { M2, "blur", 0.5f }, { M3, "pitch", -0.25f } } },

    //==========================================================================
    // VOCAL PROCESSING
    //==========================================================================
    { "Ghost Choir", "Vocal Processing",
      { { "size", 220 }, { "density", 35 }, { "overlap", 0.8f }, { "envelope", GAUSS }, { "pitchRandom", 12 }, { "spectralMode", MIN }, { "spectralAmount", 1 },
        { "thickness", 0.7f }, { "spread", 0.9f }, { "smoothness", 0.3f }, { "dryWet", 0.7f },
        { "fx_CHOR_on", 1 }, { "ch_mix", 0.5f }, { "fx_REV_on", 1 }, { "rev_decay", 6 }, { "rev_mix", 0.5f } },
      { { M1, "pitchRandom", 0.4f }, { M2, "thickness", 0.3f }, { M3, "pitch", 0.25f }, { M4, "rev_mix", 0.4f } } },

    { "Vocal Smear", "Vocal Processing",
      { { "blur", 0.75f }, { "size", 140 }, { "density", 40 }, { "overlap", 1 }, { "diffusion", 0.5f }, { "posRandom", 0.25f }, { "width", 1.5f }, { "dryWet", 0.7f },
        { "fx_FILT_on", 1 }, { "filt_mode", 1 }, { "filt_cut", 180 }, { "fx_REV_on", 1 }, { "rev_mix", 0.35f } },
      { { M1, "blur", 0.25f }, { M2, "posRandom", 0.5f }, { M3, "filt_cut", 0.4f }, { M4, "width", 0.25f } } },

    { "Formant Robot", "Vocal Processing",
      { { "size", 12 }, { "density", 110 }, { "jitter", 0 }, { "overlap", 0.3f }, { "envelope", TRAP }, { "texture", 0.8f }, { "posRandom", 0.02f }, { "dryWet", 0.85f },
        { "fx_FILT_on", 1 }, { "filt_mode", BP }, { "filt_cut", 1200 }, { "filt_res", 0.6f },
        { "fx_DIST_on", 1 }, { "dist_drive", 0.4f }, { "fx_REV_on", 0 },
        { "lfo1_sync", 1 }, { "lfo1_div", D8 }, { "lfo1_shape", TRI } },
      { { L1, "filt_cut", 0.25f }, { M1, "density", 0.4f }, { M2, "dist_drive", 0.5f }, { M3, "filt_res", 0.35f }, { M4, "size", 0.3f } } },

    //==========================================================================
    // GRANULAR PADS
    //==========================================================================
    { "Silk Pad", "Granular Pads",
      { { "size", 700 }, { "density", 20 }, { "overlap", 1 }, { "posRandom", 0.15f }, { "pitchRandom", 0.15f }, { "smoothness", 0.4f },
        { "thickness", 0.6f }, { "spread", 0.7f }, { "drone", 0.3f }, { "dryWet", 0.8f },
        { "fx_CHOR_on", 1 }, { "ch_mix", 0.4f }, { "fx_REV_on", 1 }, { "rev_decay", 6 }, { "rev_mix", 0.4f }, { "lfo1_rate", 0.09f } },
      { { L1, "position", 0.08f }, { M1, "smoothness", 0.5f }, { M2, "drone", 0.6f }, { M3, "thickness", 0.4f }, { M4, "ch_mix", 0.5f } } },

    { "Velvet Strata", "Granular Pads",
      { { "size", 1100 }, { "density", 10 }, { "drone", 0.6f }, { "spectralMode", FIFTHS }, { "spectralAmount", 0.5f }, { "pitchRandom", 12 },
        { "blur", 0.3f }, { "width", 1.3f }, { "dryWet", 0.8f },
        { "fx_FILT_on", 1 }, { "filt_cut", 3500 }, { "filt_res", 0.2f }, { "fx_REV_on", 1 }, { "rev_mix", 0.4f }, { "lfo1_rate", 0.05f } },
      { { L1, "filt_cut", 0.3f }, { M1, "filt_cut", 0.4f }, { M2, "blur", 0.5f }, { M3, "spectralAmount", 0.5f }, { M4, "drone", 0.4f } } },

    { "Halo Pad", "Granular Pads",
      { { "pitch", 12 }, { "thickness", 1 }, { "size", 500 }, { "density", 28 }, { "overlap", 1 }, { "smoothness", 0.2f },
        { "spectralMode", MAJ }, { "spectralAmount", 0.6f }, { "pitchRandom", 5 }, { "dryWet", 0.75f },
        { "fx_REV_on", 1 }, { "rev_mix", 0.5f }, { "rev_decay", 8 }, { "fx_DLY_on", 1 }, { "dly_div", D4 }, { "dly_pp", 0.2f }, { "dly_mix", 0.2f } },
      { { M1, "pitch", -0.25f }, { M2, "pitchRandom", 0.3f }, { M3, "rev_size", 0.3f }, { M4, "dly_mix", 0.4f } } },

    //==========================================================================
    // GLITCH
    //==========================================================================
    { "Stutter Grid", "Glitch",
      { { "size", 60 }, { "density", 8 }, { "jitter", 0 }, { "overlap", 0 }, { "envelope", TUKEY }, { "posRandom", 0.5f }, { "reverse", 0.3f }, { "dryWet", 0.8f },
        { "seq_div", D16 }, { "fx_CRUSH_on", 1 }, { "crush_bits", 12 }, { "crush_down", 2 }, { "crush_mix", 0.5f }, { "fx_REV_on", 0 },
        { "seq_step1", 1 }, { "seq_step2", 0 }, { "seq_step3", 0.5f }, { "seq_step4", 0.25f }, { "seq_step5", 0.75f }, { "seq_step6", 0 }, { "seq_step7", 1 }, { "seq_step8", 0.4f },
        { "seq_step9", 0.1f }, { "seq_step10", 0.9f }, { "seq_step11", 0.3f }, { "seq_step12", 0.6f }, { "seq_step13", 1 }, { "seq_step14", 0.2f }, { "seq_step15", 0.8f }, { "seq_step16", 0 },
        { "lfo1_shape", SH }, { "lfo1_sync", 1 }, { "lfo1_div", D16 } },
      { { SQ, "position", 0.6f }, { SQ, "pitch", 0.125f }, { L1, "reverse", 0.5f }, { M1, "density", 0.5f }, { M2, "crush_bits", -0.5f }, { M3, "size", 0.4f } } },

    { "Data Shards", "Glitch",
      { { "size", 25 }, { "density", 60 }, { "jitter", 0.2f }, { "envelope", PERC }, { "texture", 1 }, { "pitchRandom", 12 }, { "spectralMode", 1 }, { "spectralAmount", 1 },
        { "chaos", 0.4f }, { "dryWet", 0.8f },
        { "fx_CRUSH_on", 1 }, { "crush_bits", 6 }, { "crush_down", 6 }, { "crush_mix", 0.6f }, { "fx_FILT_on", 1 }, { "filt_mode", 1 }, { "filt_cut", 400 }, { "fx_REV_on", 0 } },
      { { M1, "chaos", 0.5f }, { M2, "crush_down", 0.5f }, { M3, "density", 0.4f }, { M4, "pitchRandom", 0.4f } } },

    { "Tape Skip", "Glitch",
      { { "size", 140 }, { "density", 6 }, { "overlap", 0.2f }, { "direction", 1 }, { "posRandom", 0.05f }, { "reverse", 0.5f }, { "envelope", TRAP }, { "dryWet", 0.8f },
        { "seq_div", D8 }, { "fx_DLY_on", 1 }, { "dly_div", D16 }, { "dly_fb", 0.3f }, { "dly_mix", 0.25f },
        { "fx_DIST_on", 1 }, { "dist_type", TUBE }, { "dist_drive", 0.25f }, { "fx_REV_on", 0 } },
      { { SQ, "size", 0.4f }, { M1, "direction", -0.9f }, { M2, "dist_drive", 0.5f }, { M3, "density", 0.4f }, { M4, "dly_mix", 0.5f } } },

    //==========================================================================
    // METALLIC
    //==========================================================================
    { "Iron Bells", "Metallic",
      { { "density", 140 }, { "jitter", 0 }, { "size", 30 }, { "overlap", 0.4f }, { "envelope", PERC }, { "spectralMode", HARM }, { "spectralAmount", 0.6f }, { "dryWet", 0.75f },
        { "fx_SHIFT_on", 1 }, { "fs_shift", 23 }, { "fs_mix", 0.35f }, { "fx_REV_on", 1 }, { "rev_decay", 3.5f }, { "rev_mix", 0.3f },
        { "fx_FILT_on", 1 }, { "filt_mode", BP }, { "filt_cut", 3000 }, { "filt_res", 0.3f } },
      { { M1, "density", 0.3f }, { M2, "fs_shift", 0.2f }, { M3, "filt_cut", 0.3f }, { M4, "spectralAmount", 0.4f } } },

    { "Resonant Steel", "Metallic",
      { { "density", 220 }, { "jitter", 0.02f }, { "size", 9 }, { "pitch", 7 }, { "texture", 0.9f }, { "feedback", 0.5f }, { "dryWet", 0.75f },
        { "fx_PHAS_on", 1 }, { "ph_fb", 0.7f }, { "ph_mix", 0.5f }, { "fx_DIST_on", 1 }, { "dist_type", FOLD }, { "dist_drive", 0.3f }, { "dist_mix", 0.3f } },
      { { M1, "density", -0.3f }, { M2, "feedback", 0.4f }, { M3, "dist_drive", 0.5f }, { M4, "pitch", 0.15f } } },

    { "Chrome Drift", "Metallic",
      { { "size", 200 }, { "density", 25 }, { "dryWet", 0.7f }, { "fx_SHIFT_on", 1 }, { "fs_shift", 450 }, { "fs_fb", 0.6f }, { "fs_mix", 0.6f },
        { "fx_CHOR_on", 1 }, { "fx_REV_on", 1 }, { "rev_mix", 0.35f }, { "lfo1_rate", 0.05f } },
      { { L1, "fs_shift", 0.2f }, { M1, "fs_fb", 0.3f }, { M2, "fs_mix", 0.4f }, { M3, "size", 0.5f } } },

    //==========================================================================
    // CINEMATIC
    //==========================================================================
    { "Tension Rise", "Cinematic",
      { { "reverseCloud", 0.8f }, { "size", 400 }, { "density", 30 }, { "pitch", -12 }, { "overlap", 1 }, { "evolve", 0.5f }, { "dryWet", 0.8f },
        { "fx_REV_on", 1 }, { "rev_decay", 9 }, { "rev_mix", 0.5f }, { "fx_FILT_on", 1 }, { "filt_cut", 1500 },
        { "lfo1_shape", SAW }, { "lfo1_rate", 0.03f } },
      { { L1, "pitch", 0.1f }, { L1, "filt_cut", 0.4f }, { M1, "reverseCloud", 0.2f }, { M2, "filt_cut", 0.5f }, { M3, "pitch", 0.25f } } },

    { "Monolith", "Cinematic",
      { { "pitch", -24 }, { "size", 1500 }, { "density", 6 }, { "drone", 0.7f }, { "blur", 0.4f }, { "diffusion", 0.7f }, { "dryWet", 0.8f },
        { "fx_DIST_on", 1 }, { "dist_drive", 0.2f }, { "fx_REV_on", 1 }, { "rev_size", 1 }, { "rev_decay", 15 }, { "rev_mix", 0.6f },
        { "fx_IMG_on", 1 }, { "img_bass", 120 } },
      { { M1, "pitch", 0.25f }, { M2, "dist_drive", 0.5f }, { M3, "blur", 0.5f }, { M4, "rev_mix", -0.4f } } },

    { "Nebula Score", "Cinematic",
      { { "spectralMode", MIN }, { "spectralAmount", 0.6f }, { "pitchRandom", 12 }, { "thickness", 0.5f }, { "size", 800 }, { "density", 16 }, { "evolve", 0.6f }, { "dryWet", 0.75f },
        { "fx_REV_on", 1 }, { "rev_decay", 8 }, { "rev_mix", 0.5f }, { "fx_DLY_on", 1 }, { "dly_div", D2 }, { "dly_fb", 0.5f }, { "dly_mix", 0.25f } },
      { { M1, "evolve", 0.4f }, { M2, "pitchRandom", 0.3f }, { M3, "dly_mix", 0.5f }, { M4, "thickness", 0.5f } } },

    //==========================================================================
    // DRONES
    //==========================================================================
    { "Endless Om", "Drones",
      { { "drone", 1 }, { "size", 1500 }, { "density", 10 }, { "pitch", -12 }, { "smoothness", 0.5f }, { "spectralMode", FIFTHS }, { "spectralAmount", 0.8f },
        { "blur", 0.5f }, { "width", 1.4f }, { "dryWet", 0.85f }, { "fx_REV_on", 1 }, { "rev_decay", 10 }, { "rev_mix", 0.45f } },
      { { M1, "smoothness", 0.4f }, { M2, "blur", 0.4f }, { M3, "pitch", 0.25f }, { M4, "width", 0.25f } } },

    { "Deep Current", "Drones",
      { { "drone", 0.8f }, { "direction", 0.15f }, { "size", 1200 }, { "density", 14 }, { "dryWet", 0.85f },
        { "fx_FILT_on", 1 }, { "filt_cut", 900 }, { "filt_res", 0.4f }, { "fx_CHOR_on", 1 }, { "fx_REV_on", 1 }, { "rev_mix", 0.4f }, { "lfo1_rate", 0.04f } },
      { { L1, "filt_cut", 0.35f }, { M1, "filt_cut", 0.5f }, { M2, "direction", 0.3f }, { M3, "filt_res", 0.4f } } },

    { "Solar Hum", "Drones",
      { { "density", 80 }, { "jitter", 0 }, { "size", 600 }, { "overlap", 1 }, { "drone", 0.5f }, { "spectralMode", HARM }, { "spectralAmount", 0.4f }, { "dryWet", 0.8f },
        { "fx_PHAS_on", 1 }, { "ph_rate", 0.05f }, { "ph_mix", 0.4f }, { "fx_SHIFT_on", 1 }, { "fs_shift", 2 }, { "fs_mix", 0.3f } },
      { { M1, "density", 0.3f }, { M2, "spectralAmount", 0.5f }, { M3, "ph_mix", 0.4f }, { M4, "fs_shift", 0.1f } } },

    //==========================================================================
    // RHYTHMIC
    //==========================================================================
    { "Pulse Engine", "Rhythmic",
      { { "size", 90 }, { "density", 16 }, { "jitter", 0 }, { "overlap", 0 }, { "envelope", PERC }, { "dryWet", 0.7f }, { "seq_div", D16 },
        { "seq_step1", 1 }, { "seq_step2", 0.3f }, { "seq_step3", 0.6f }, { "seq_step4", 0.3f }, { "seq_step5", 0.9f }, { "seq_step6", 0.1f }, { "seq_step7", 0.6f }, { "seq_step8", 0.4f },
        { "seq_step9", 1 }, { "seq_step10", 0.2f }, { "seq_step11", 0.7f }, { "seq_step12", 0.3f }, { "seq_step13", 0.8f }, { "seq_step14", 0.5f }, { "seq_step15", 0.2f }, { "seq_step16", 0.6f },
        { "fx_DLY_on", 1 }, { "dly_div", D8D }, { "dly_fb", 0.4f }, { "dly_mix", 0.3f }, { "fx_REV_on", 1 }, { "rev_mix", 0.2f } },
      { { SQ, "position", 0.5f }, { SQ, "size", 0.3f }, { M1, "density", 0.4f }, { M2, "dly_fb", 0.4f }, { M3, "pitch", 0.25f } } },

    { "Grain Gate", "Rhythmic",
      { { "size", 70 }, { "density", 40 }, { "envelope", TRAP }, { "overlap", 0.3f }, { "dryWet", 0.8f },
        { "lfo1_sync", 1 }, { "lfo1_div", D16 }, { "lfo1_shape", SQR }, { "lfo2_sync", 1 }, { "lfo2_div", D4D }, { "lfo2_shape", TRI },
        { "fx_FILT_on", 1 }, { "filt_cut", 4000 }, { "filt_res", 0.35f }, { "fx_CRUSH_on", 1 }, { "crush_bits", 12 }, { "crush_mix", 0.3f }, { "fx_REV_on", 0 } },
      { { L1, "density", -0.6f }, { L2, "filt_cut", 0.3f }, { M1, "filt_cut", 0.4f }, { M2, "size", 0.4f }, { M3, "crush_mix", 0.6f } } },

    { "Polymeter Dust", "Rhythmic",
      { { "seq_len", 7 }, { "seq_div", D16 }, { "spectralMode", PENT }, { "spectralAmount", 1 }, { "size", 50 }, { "density", 30 }, { "envelope", PERC }, { "dryWet", 0.75f },
        { "seq_step1", 0.5f }, { "seq_step2", 0.7f }, { "seq_step3", 0.4f }, { "seq_step4", 1 }, { "seq_step5", 0.6f }, { "seq_step6", 0.3f }, { "seq_step7", 0.8f },
        { "lfo2_sync", 1 }, { "lfo2_div", D4T }, { "fx_DLY_on", 1 }, { "dly_div", D8T }, { "dly_pp", 1 }, { "dly_mix", 0.3f } },
      { { SQ, "pitch", 0.25f }, { L2, "pan", 0.8f }, { M1, "density", 0.4f }, { M2, "dly_mix", 0.5f }, { M3, "size", 0.4f } } },

    //==========================================================================
    // DARK TEXTURES
    //==========================================================================
    { "Abyssal Bloom", "Dark Textures",
      { { "pitch", -12 }, { "pitchRandom", 1 }, { "size", 700 }, { "density", 14 }, { "smoothness", 0.6f }, { "blur", 0.5f }, { "dryWet", 0.8f },
        { "fx_DIST_on", 1 }, { "dist_type", TUBE }, { "dist_drive", 0.4f }, { "dist_mix", 0.4f }, { "fx_FILT_on", 1 }, { "filt_cut", 1200 }, { "filt_res", 0.3f },
        { "fx_REV_on", 1 }, { "rev_decay", 10 }, { "rev_damp", 0.8f }, { "rev_mix", 0.5f }, { "rnd_rate", 0.3f }, { "rnd_smooth", 0.9f } },
      { { RN, "position", 0.2f }, { M1, "filt_cut", 0.5f }, { M2, "dist_drive", 0.5f }, { M3, "blur", 0.4f } } },

    { "Rust Choir", "Dark Textures",
      { { "pitch", -5 }, { "spectralMode", MIN }, { "spectralAmount", 1 }, { "pitchRandom", 7 }, { "thickness", 0.7f }, { "chaos", 0.2f }, { "dryWet", 0.75f },
        { "fx_CRUSH_on", 1 }, { "crush_bits", 9 }, { "crush_down", 2 }, { "crush_mix", 0.3f }, { "fx_DIST_on", 1 }, { "dist_type", FOLD }, { "dist_drive", 0.25f }, { "dist_mix", 0.3f },
        { "fx_REV_on", 1 }, { "rev_mix", 0.4f } },
      { { M1, "crush_mix", 0.6f }, { M2, "chaos", 0.5f }, { M3, "thickness", 0.3f }, { M4, "dist_drive", 0.5f } } },

    { "Night Engine", "Dark Textures",
      { { "density", 45 }, { "size", 80 }, { "reverse", 0.5f }, { "feedback", 0.4f }, { "dryWet", 0.75f },
        { "fx_SHIFT_on", 1 }, { "fs_shift", -60 }, { "fs_fb", 0.3f }, { "fs_mix", 0.4f }, { "fx_FILT_on", 1 }, { "filt_mode", BP }, { "filt_cut", 700 }, { "filt_res", 0.5f },
        { "lfo1_shape", TRI }, { "lfo1_rate", 0.2f }, { "fx_REV_on", 1 }, { "rev_mix", 0.3f } },
      { { L1, "filt_cut", 0.3f }, { M1, "fs_shift", -0.2f }, { M2, "feedback", 0.4f }, { M3, "filt_res", 0.4f } } },

    { "Black Ice", "Dark Textures",
      { { "envelope", PERC }, { "size", 35 }, { "density", 50 }, { "pitch", 12 }, { "pitchRandom", 24 }, { "spectralMode", MIN }, { "spectralAmount", 1 },
        { "posRandom", 0.9f }, { "texture", 0.9f }, { "dryWet", 0.8f }, { "fx_CRUSH_on", 1 }, { "crush_bits", 12 }, { "crush_mix", 0.4f },
        { "fx_REV_on", 1 }, { "rev_decay", 12 }, { "rev_damp", 0.2f }, { "rev_mix", 0.6f }, { "fx_IMG_on", 1 }, { "img_width", 1.8f } },
      { { M1, "density", 0.4f }, { M2, "rev_mix", 0.4f }, { M3, "pitch", -0.25f }, { M4, "crush_bits", -0.4f } },
      { 3, 4, 0, 1, 2, 5, 6, 7, 8 } },
    };
    return presets;
}

} // namespace nubis

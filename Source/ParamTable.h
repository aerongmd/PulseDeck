#pragma once
#include <array>

// The 15 effect knobs shown in the editor. IDs match pd::Params field names.
struct FxDef { const char* id; const char* name; float def; };

inline const std::array<FxDef, 15>& fxDefs()
{
    static const std::array<FxDef, 15> defs {{
        { "drive",         "Drive",        0.0f  },
        { "bitcrush",      "Bitcrush",     0.0f  },
        { "downsample",    "Downsample",   0.0f  },
        { "lowpass",       "LP Cutoff",    1.0f  },
        { "highpass",      "HP Cutoff",    0.0f  },
        { "resonance",     "Resonance",    0.15f },
        { "chorusDepth",   "Chorus",       0.0f  },
        { "chorusRate",    "Chorus Rate",  0.3f  },
        { "delayMix",      "Delay",        0.0f  },
        { "delayTime",     "Delay Time",   0.4f  },
        { "delayFeedback", "Feedback",     0.35f },
        { "reverbMix",     "Reverb",       0.0f  },
        { "reverbSize",    "Reverb Size",  0.5f  },
        { "width",         "Width",        0.5f  },
        { "tremolo",       "Tremolo",      0.0f  }
    }};
    return defs;
}

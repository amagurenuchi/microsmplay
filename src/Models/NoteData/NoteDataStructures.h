#pragma once

#include <array>

struct NoteInfo {
    float rowTime = 0.0f;
    int notes = 0;
};

enum Skillset {
    Skill_Overall = 0,
    Skill_Stream,
    Skill_Jumpstream,
    Skill_Handstream,
    Skill_Stamina,
    Skill_JackSpeed,
    Skill_Chordjack,
    Skill_Technical,
    NUM_Skillset
};

enum CalcPatternMod {
    Stream = 0,
    JS,
    HS,
    CJ,
    CJDensity,
    HSDensity,
    FlamJam,
    TheThing,
    TheThing2,
    GChordStream,
    OHJumpMod,
    Balance,
    Roll,
    RollJS,
    OHTrill,
    VOHTrill,
    Chaos,
    WideRangeBalance,
    WideRangeRoll,
    WideRangeJumptrill,
    WideRangeJJ,
    WideRangeAnchor,
    RanMan,
    Minijack,
    CJOHJump,
    CJOHAnchor,
    GStream,
    GBracketing,
    NUM_CalcPatternMod
};

enum CalcDiffValue {
    NPSBase = 0,
    MSBase,
    MSD = MSBase,
    JackBase,
    CJBase,
    TechBase,
    RMABase,
    NUM_CalcDiffValue
};

enum CalcDebugMisc {
    Pts = 0,
    StamMod,
    PtLoss,
    NUM_CalcDebugMisc
};

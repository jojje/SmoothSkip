// Utility functions from avisynth
#ifndef __AVSUTIL_H
#define __AVSUTIL_H

#include "3rd-party/avisynth.h"

inline AVSValue GetVar(IScriptEnvironment* env, const char* name) {
    try {
        return env->GetVar(name);
    }
    catch (const IScriptEnvironment::NotFound&) {}

    return AVSValue();
}

#endif
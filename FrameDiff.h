#pragma once
#include "3rd-party/avisynth.h"

// Returns the difference between frame n and the frame at the provided offset from n.
float YDiff(AVSValue clip, int n, int offset, IScriptEnvironment* env);
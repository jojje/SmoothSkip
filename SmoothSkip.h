// SmoothSkip: AVISynth filter for processing stuttering clips
// Copyright (C) 2015 Jonas Tingeborn
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
// USA.

#ifndef __SMOOTHSKIP_H
#define __SMOOTHSKIP_H

#include "3rd-party/avisynth.h"
#include "cycle.h"
#include "CycleCache.h"
#include "FrameDiff.h"

#define VERSION "1.0.3"

class SmoothSkip : public GenericVideoFilter {
	PClip altclip;   // The super clip from MVTools2 
	bool debug;      // debug arg
	int offset;      // frame offset used to get frame from the alternate clip.

public:
	CycleCache* cycles;
	SmoothSkip(PClip _child, PClip _altclip, int cycleLen, int creates, int offset, 
		       bool _debug, IScriptEnvironment* env);
	~SmoothSkip();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	void updateCycle(IScriptEnvironment* env, int n, VideoInfo cvi, Cycle& cycle);
	PVideoFrame info(IScriptEnvironment* env, PVideoFrame src, char* msg, int x, int y);
	double GetDiffFromPrevious(IScriptEnvironment* env, int n);
	FrameMap SmoothSkip::getFrameMapping(IScriptEnvironment* env, int n);
};

AVSValue __cdecl Create_SmoothSkip(AVSValue args, void* user_data, IScriptEnvironment* env);

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env) {
	env->AddFunction("SmoothSkip", "cc[CYCLE]i[CREATE]i[OFFSET]i[DEBUG]b", Create_SmoothSkip, 0);
	return "'SmoothSkip' plugin v" VERSION ", author: tinjon[at]gmail.com";
}

void DrawString(PVideoFrame &dst, int x, int y, const char *s, int bIsYUY2);

#endif
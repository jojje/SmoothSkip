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

#include <Windows.h>
#include <stdio.h>
#include <new>
#include "SmoothSkip.h"
#include "3rd-party/info.h"


// clip is the clip to have its frame compared,
// n = frame number to compare to its previous
double GetDiffFromPrevious(IScriptEnvironment* env, PClip clip, int n);
void raiseError(IScriptEnvironment* env, const char* msg);
double GetFps(PClip clip);

// ==========================================================================
// PUBLIC methods
// ==========================================================================

PVideoFrame __stdcall SmoothSkip::GetFrame(int n, IScriptEnvironment* env) {
	// original frame number for child clip
	int cn = getChildFrameNumber(n);
	// original frame number for alternate clip, offset as specified by user.
	int acn = getChildFrameNumber( min(max(n + offset, 0), altclip->GetVideoInfo().num_frames) );

	if (!cycle.includes(cn)) {                         // cycle boundary crossed, so update the cycle info.
		updateCycle(env, cn, child->GetVideoInfo());
	}

	// Only alternate the first frame to clip B in case scaled frames point to the same underlying child frame
	bool alt = cycle.isBadFrame(cn) && cn != acn;
	PVideoFrame frame;

	if (alt) {
		frame = altclip->GetFrame(acn, env);
	} else {
		frame = child->GetFrame(cn, env);
	}

	if (debug) {
		char msg[256];
		int row = 0;
		sprintf(msg, "SmoothSkip v%s", VERSION);
		frame = info(env, frame, msg, 0, row++);
		sprintf(msg, "Frame: %d (child: %d)", n, cn);
		frame = info(env, frame, msg, 0, row++);
		sprintf(msg, "Using: %d, clip %s", (alt ? acn : cn), (alt ? "B" : "A"));
		frame = info(env, frame, msg, 0, row++);
		sprintf(msg, "DM:    %s", diffmethod == 1 ? "CFrameDiff" : "YDiff");
		frame = info(env, frame, msg, 0, row++);
		sprintf(msg, "FPS:   %.3f (child: %.3f)", GetFps(this), GetFps(child));
		frame = info(env, frame, msg, 0, row++);
		sprintf(msg, "Cycle frame diffs (child):");
		frame = info(env, frame, msg, 0, row++);
		for (int i = 0; i < cycle.length; i++) {
			sprintf(msg, "%s %d (%.5f) ",
				cycle.isBadFrame(cycle.diffs[i].frame) ? "*" : " ",
				cycle.diffs[i].frame,
				cycle.diffs[i].diff);
			frame = info(env, frame, msg, 0, row++);
		}
	}

	return frame;
}

void SmoothSkip::updateCycle(IScriptEnvironment* env, int cn, VideoInfo cvi) {
	double diff = 0;
	double maxDiff = -1;
	int i, j;

	cycle.reset();

	int cycleStartFrame = (cn / cycle.length) * cycle.length;
	int cycleEndFrame = min(cycleStartFrame + cycle.length - 1, cvi.num_frames - 1);

	for (i = cycleStartFrame, j = 0; i <= cycleEndFrame; i++, j++) {
		diff = GetDiffFromPrevious(env, child, i);
		cycle.diffs[j].frame = i;
		cycle.diffs[j].diff = diff;
	}
	for (; j < cycle.length - 1; j++) {                  // for debugging
		cycle.diffs[j].frame = -1;                       // for debugging
		cycle.diffs[j].diff = -1;                        // for debugging
	}
}

// Constructor
SmoothSkip::SmoothSkip(PClip _child, PClip _altclip, int cycleLen, int creates, int _offset, 
	                   int _diffmethod, bool _debug, IScriptEnvironment* env) :
GenericVideoFilter(_child), altclip(_altclip), offset(_offset), diffmethod(_diffmethod), debug(_debug) {
	VideoInfo avi = altclip->GetVideoInfo();
	if (!(vi.IsYV12() || vi.IsYUY2())) raiseError(env, "Input clip must be YV12 or YUY2");
	if (!(avi.IsYV12() || avi.IsYUY2())) raiseError(env, "Alternate clip must be YV12 or YUY2");

	if (cycleLen < 1) raiseError(env, "Cycle must be > 0");
	if (!(diffmethod == 0 || diffmethod == 1)) raiseError(env, "Diff method (dm) must be 0 or 1");
	if (!cycle.initialize(cycleLen, creates)) raiseError(env, "Failed to allocate cycle memory");

	int newFrames = (vi.num_frames / cycleLen) * creates;    // a non-full last cycle will still introduce a new frame
	newFrames += min(vi.num_frames % cycleLen, creates);
	vi.MulDivFPS(cycleLen + creates, cycleLen);
	vi.num_frames += newFrames;

	child->SetCacheHints(CACHE_RANGE, cycle.length);
}

SmoothSkip::~SmoothSkip() {
}

AVSValue __cdecl Create_SmoothSkip(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new SmoothSkip(args[0].AsClip(),
		args[1].AsClip(),      // altclip
		args[2].AsInt(4),      // cycle
		args[3].AsInt(1),      // create
		args[4].AsInt(0),      // offset
		args[5].AsInt(0),      // dm (diffmethod)
		args[6].AsBool(false), // debug
		env);
}


// ========================================================================
//  Helper functions
// ========================================================================

PVideoFrame SmoothSkip::info(IScriptEnvironment* env, PVideoFrame src, char* msg, int x, int y) {
	env->MakeWritable(&src);
	DrawString(src, x, y, msg, child->GetVideoInfo().IsYUY2());
	return src;
}

double GetFps(PClip clip) {
	VideoInfo info = clip->GetVideoInfo();
	return (double)info.fps_numerator / (double)info.fps_denominator;
}

double yDifferenceFromPrevious(IScriptEnvironment* env, PClip clip, int n) {
	env->SetVar("current_frame", (AVSValue)n);     //    allow us to use RT filters anyway

	AVSValue args[1] = { clip };  // time, ml
	return env->Invoke("YDifferenceFromPrevious", AVSValue(args, 1)).AsFloat();
}

double TcDifferenceFromPrevious(IScriptEnvironment* env, PClip clip, int n) {
	env->SetVar("current_frame", (AVSValue)n);     //    allow us to use RT filters anyway
	AVSValue args[1] = { clip };  // time, ml
	return env->Invoke("CFrameDiff", AVSValue(args, 1)).AsFloat();
}

double SmoothSkip::GetDiffFromPrevious(IScriptEnvironment* env, PClip clip, int n) {
	if (diffmethod == 1) {
		return TcDifferenceFromPrevious(env, child, n);
	} else {
		return yDifferenceFromPrevious(env, child, n);
	}
}

int SmoothSkip::getChildFrameNumber(int n) {
	return (int)(n * GetFps(child) / GetFps(this));
}

void raiseError(IScriptEnvironment* env, const char* msg) {
	char buff[1024];
	sprintf_s(buff, sizeof(buff), "[SmoothSkip] %s", msg);
	env->ThrowError(buff);
}

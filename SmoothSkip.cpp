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
#include <mutex>
#include <stdio.h>
#include "SmoothSkip.h"
#include "CycleCache.h"
#include "Cycle.h"
#include "FrameDiff.h"
#include "3rd-party/info.h"

#ifdef _DEBUG
	#define DEBUG 1
#else
	#define DEBUG 0
#endif

void raiseError(IScriptEnvironment* env, const char* msg);
double GetFps(PClip clip);
std::mutex frameMappingMutex;

// ==========================================================================
// PUBLIC methods
// ==========================================================================

PVideoFrame __stdcall SmoothSkip::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame frame;

	Cycle &cycle = *cycles->GetCycleForFrame(n);

	if (DEBUG) {
		printf("frame %d, cycle-address %X, thread-id: %X\n", n, (unsigned int)&cycle, GetCurrentThreadId());
	}

	// Comparison of the source clip's previous frame is currently done single-threaded.
	FrameMap map = getFrameMapping(env, n);

	// Fetching the alternative clip, or the source clip a second time
	// (from avisynth-cache) is done multi-threaded.
	int cn = map.srcframe, acn = cn;
	bool alt = map.altclip;

	if (alt) {
		acn = max(cn + offset, 0);                              // original frame number for alternate clip, offset as specified by user.
		acn = min(acn, altclip->GetVideoInfo().num_frames - 1); // ensure the altclip frame to get is 0 <= x <= [last frame number] in alt clip
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
		sprintf(msg, "FPS:   %.3f (child: %.3f)", GetFps(this), GetFps(child));
		frame = info(env, frame, msg, 0, row++);
		sprintf(msg, "Scene: %.1f", sceneThreshold);
		frame = info(env, frame, msg, 0, row++);
		sprintf(msg, "Cycle frame diffs (child):");
		frame = info(env, frame, msg, 0, row++);
		for (int i = 0; i < cycle.length; i++) {
			sprintf(msg, "%s %d (%.5f) ",
				cycle.isSceneChange(cycle.diffs[i].frame) ? "S" : cycle.isBadFrame(cycle.diffs[i].frame) ? "*" : " ",
				cycle.diffs[i].frame,
				cycle.diffs[i].diff);
			frame = info(env, frame, msg, 0, row++);
		}
	}

	return frame;
}

void SmoothSkip::updateCycle(IScriptEnvironment* env, int cn, VideoInfo cvi, Cycle& cycle) {
	float diff = 0;
	int i, j;

	cycle.reset();

	int cycleStartFrame = (cn / cycle.length) * cycle.length;
	int cycleEndFrame = min(cycleStartFrame + cycle.length - 1, cvi.num_frames - 1);

	for (i = cycleStartFrame, j = 0; i <= cycleEndFrame; i++, j++) {
		diff = GetDiffFromPrevious(env, i);
		cycle.diffs[j].frame = i;
		cycle.diffs[j].diff = diff;
	}

	cycle.updateFrameMap();
}

// Constructor
SmoothSkip::SmoothSkip(PClip _child, PClip _altclip, int cycleLen, int creates, int _offset, 
	                   double sceneThresh, bool _debug, IScriptEnvironment* env) :
GenericVideoFilter(_child), altclip(_altclip), offset(_offset), debug(_debug) {
	VideoInfo avi = altclip->GetVideoInfo();
	VideoInfo cvi = child->GetVideoInfo();
	if (!(vi.IsYV12() || vi.IsYUY2())) raiseError(env, "Input clip must be YV12 or YUY2");
	if (!(avi.IsYV12() || avi.IsYUY2())) raiseError(env, "Alternate clip must be YV12 or YUY2");

	if (cycleLen < 1) raiseError(env, "Cycle must be > 0");
	if (cycleLen > cvi.num_frames) raiseError(env, "Cycle can't be larger than the frames in source clip");
	if (cycleLen > avi.num_frames) raiseError(env, "Cycle can't be larger than the frames in alt clip");
	if (creates < 1 || creates > cycleLen) raiseError(env, "Create must be between 1 and the value of cycle (1 <= create <= cycle)");
	if (sceneThresh < 0) raiseError(env, "Scene threshold must be >= 0.0");

	sceneThreshold = static_cast<float>(sceneThresh); // Assign to static variable in the cycle header, so it can be used in the cycle logic

	try {
		cycles = new CycleCache(cycleLen, creates, cvi.num_frames);
	}
	catch (std::bad_alloc) {
		raiseError(env, "Failed to allocate cycle memory");
	}

	int newFrames = (vi.num_frames / cycleLen) * creates;    // a non-full last cycle will still introduce a new frame.
	newFrames += min(vi.num_frames % cycleLen, creates);     // account for when the last clip cycle isn't a full one.
	vi.MulDivFPS(cycleLen + creates, cycleLen);
	vi.num_frames += newFrames;
}

SmoothSkip::~SmoothSkip() {
	delete cycles;
}

AVSValue __cdecl Create_SmoothSkip(AVSValue args, void* user_data, IScriptEnvironment* env) {
	return new SmoothSkip(args[0].AsClip(),
		args[1].AsClip(),      // altclip
		args[2].AsInt(4),      // cycle
		args[3].AsInt(1),      // create
		args[4].AsInt(0),      // offset
		args[5].AsFloat(32),   // offset
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

float SmoothSkip::GetDiffFromPrevious(IScriptEnvironment* env, int n) {
	return YDiff(child, n, -1, env);
}

FrameMap SmoothSkip::getFrameMapping(IScriptEnvironment* env, int n) {
	Cycle& cycle = *cycles->GetCycleForFrame(n);
	int cycleCount = n / (cycle.length + cycle.creates);
	int cycleOffset = n % (cycle.length + cycle.creates);
	int ccsf = cycleCount * cycle.length;                          // Child cycle start frame

	{
		std::lock_guard<std::mutex> lockGuard(frameMappingMutex);  // Ensure only one thread updates the frame map at a time to optimize disk I/O and Avisynth cache use.
		if (!cycle.includes(ccsf)) {                               // Cycle stats have not been computed, so try to update the cycle.
			if (DEBUG) {
				printf("Frame %d not in cycle, updating!\n", n);
			}
			updateCycle(env, ccsf, child->GetVideoInfo(), cycle);
		}
	}

	FrameMap map = cycle.frameMap[cycleOffset];
	if (map.dstframe != n)
		raiseError(env, "BUG! Frame counting is out of whack. Please report this to the author.");

	return map;
}

void raiseError(IScriptEnvironment* env, const char* msg) {
	char buff[1024];
	sprintf_s(buff, sizeof(buff), "[SmoothSkip] %s", msg);
	env->ThrowError(buff);
}

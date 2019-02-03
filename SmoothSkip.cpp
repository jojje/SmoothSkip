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
#include <thread>
#include <stdio.h>
#include "SmoothSkip.h"
#include "3rd-party/info.h"
#include "3rd-party/avsutil.h"

#ifdef _DEBUG
	#define DEBUG_LOG 1
#else
	#define DEBUG_LOG 0
#endif

CRITICAL_SECTION lock;

void raiseError(IScriptEnvironment* env, const char* msg);
double GetFps(PClip clip);
void initDiffClip(int diffmethod, PClip child, IScriptEnvironment* env);

// ==========================================================================
// PUBLIC methods
// ==========================================================================

PVideoFrame __stdcall SmoothSkip::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame frame;

	EnterCriticalSection(&lock);   // Naive support for MT 2 mode. Make access essentially single-treaded.
	                               // Approx. 20% slow down with 8 threads in MT mode 2 compared to single-threaded.
	                               // Still > 400 FPS for 1080P clip on i4770 CPU so should not be the bottleneck.

	AVSValue prev_current_frame = GetVar(env, "current_frame"); // Store previous current_frame

	if (DEBUG_LOG) {
		char BUF[256];
		std::thread::id tid = std::this_thread::get_id();
		#pragma warning(suppress: 4477)
		sprintf(BUF, "frame %d, cycle-address %X, thread-id: %X\n", n, (unsigned int)&cycle, tid);
		printf(BUF);
	}

	FrameMap map = getFrameMapping(env, n);
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

	env->SetVar("current_frame", prev_current_frame);           // Restore current_frame
	// env->SetGlobalVar("current_frame", prev_current_frame);  // Scope uncertain in MT mode. Restoring this may interfere with other filters in MT mode, if they also explicitly set this magic variable and we unset it during parallel plugin execution.

	LeaveCriticalSection(&lock);
	return frame;
}

void SmoothSkip::updateCycle(IScriptEnvironment* env, int cn, VideoInfo cvi) {
	double diff = 0;
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

void SmoothSkip::initDiffClip(IScriptEnvironment* env) {
	if (diffClip != NULL)
		return;

	if (diffmethod == 1) {
		AVSValue args[2] = { child, "diff = CFrameDiff \r\n last"};
		diffClip = env->Invoke("ScriptClip", AVSValue(args, 2)).AsClip();
	}
	else {
		AVSValue args[2] = { child, "diff = YDifferenceFromPrevious \r\n last" };
		diffClip = env->Invoke("ScriptClip", AVSValue(args, 2)).AsClip();
	}
}

// Constructor
SmoothSkip::SmoothSkip(PClip _child, PClip _altclip, int cycleLen, int creates, int _offset, 
	                   int _diffmethod, bool _debug, IScriptEnvironment* env) :
GenericVideoFilter(_child), altclip(_altclip), offset(_offset), diffmethod(_diffmethod), debug(_debug) {
	VideoInfo avi = altclip->GetVideoInfo();
	VideoInfo cvi = child->GetVideoInfo();
	if (!(vi.IsYV12() || vi.IsYUY2())) raiseError(env, "Input clip must be YV12 or YUY2");
	if (!(avi.IsYV12() || avi.IsYUY2())) raiseError(env, "Alternate clip must be YV12 or YUY2");

	if (cycleLen < 1) raiseError(env, "Cycle must be > 0");
	if (cycleLen > cvi.num_frames) raiseError(env, "Cycle can't be larger than the frames in source clip");
	if (cycleLen > avi.num_frames) raiseError(env, "Cycle can't be larger than the frames in alt clip");
	if (creates < 1 || creates > cycleLen) raiseError(env, "Create must be between 1 and the value of cycle (1 <= create <= cycle)");
	if (!(diffmethod == 0 || diffmethod == 1)) raiseError(env, "Diff method (dm) must be 0 or 1");
	if (!cycle.initialize(cycleLen, creates)) raiseError(env, "Failed to allocate cycle memory");

	int newFrames = (vi.num_frames / cycleLen) * creates;    // a non-full last cycle will still introduce a new frame.
	newFrames += min(vi.num_frames % cycleLen, creates);     // account for when the last clip cycle isn't a full one.
	vi.MulDivFPS(cycleLen + creates, cycleLen);
	vi.num_frames += newFrames;

	child->SetCacheHints(CACHE_RANGE, cycle.length);

	initDiffClip(env);
}

SmoothSkip::~SmoothSkip() {
	diffClip = NULL;
}

AVSValue __cdecl Create_SmoothSkip(AVSValue args, void* user_data, IScriptEnvironment* env) {
	InitializeCriticalSection(&lock);

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
	AVSValue args[1] = { clip };
	return env->Invoke("YDifferenceFromPrevious", AVSValue(args, 1)).AsFloat();
}

double TcDifferenceFromPrevious(IScriptEnvironment* env, PClip clip, int n) {
	AVSValue args[1] = { clip };
	return env->Invoke("CFrameDiff", AVSValue(args, 1)).AsFloat();
}

double SmoothSkip::GetDiffFromPrevious(IScriptEnvironment* env, int n) {
	env->SetVar("current_frame", n);        // Set frame to be tested by the conditional filters
	env->SetGlobalVar("current_frame", n);
	diffClip->GetFrame(n, env);
	return env->GetVar("diff").AsFloat();
}

FrameMap SmoothSkip::getFrameMapping(IScriptEnvironment* env, int n) {
	int cycleCount = n / (cycle.length + cycle.creates);
	int cycleOffset = n % (cycle.length + cycle.creates);
	int ccsf = cycleCount * cycle.length;              // child cycle start frame

	if (!cycle.includes(ccsf)) {                       // cycle boundary crossed, so update the cycle info.
		updateCycle(env, ccsf, child->GetVideoInfo());
	}

	FrameMap map = cycle.frameMap[cycleOffset];
	if (map.dstframe != n) raiseError(env, "Frame counting is out of whack");

	return map;
}

void raiseError(IScriptEnvironment* env, const char* msg) {
	char buff[1024];
	sprintf_s(buff, sizeof(buff), "[SmoothSkip] %s", msg);
	env->ThrowError(buff);
}

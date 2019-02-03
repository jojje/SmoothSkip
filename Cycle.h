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

#pragma once

#include <memory>

typedef struct {
	int frame;      // frame number
	double diff;    // frame diff to previous
} CycleDiff;

typedef struct {
	int dstframe;   // frame number in the resulting clip this filter creates
	int srcframe;   // frame number in one of the source clips
	bool altclip;   // the clip ("last" or alt) to pick the frame from
} FrameMap;

class Cycle {
	bool sorted;

public:
	int creates;        // number of frames to create in the cycle  (n in m creation)
	int length;         // cycle length in frames (size of diffs)

	std::unique_ptr<CycleDiff[]> diffs;         // Array of frame diffs for the current cycle, in frame order
	std::unique_ptr<CycleDiff[]> sortedDiffs;   // Array of frame diffs for the current cycle, in reverse diff order
	std::unique_ptr<FrameMap[]> frameMap;       // Clip frame mapping for the current cycle

	Cycle(int length, int creates);

	int getFrameWithLargestDiff(int offset);
	bool includes(int frame);
	bool isBadFrame(int n);
	void reset();
	void updateFrameMap();
};

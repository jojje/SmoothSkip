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

#ifndef __CYCLE_H
#define __CYCLE_H

typedef struct {
	int frame;      // frame number
	double diff;    // frame diff to previous
} CycleDiff;

class Cycle {
	bool sorted;

public:
	int creates;        // number of frames to create in the cycle  (n in m creation)
	int length;         // cycle length in frames (size of diffs)

	CycleDiff* diffs;         // Array of frame diffs for the current cycle, in frame order
	CycleDiff* sortedDiffs;   // Array of frame diffs for the current cycle, in reverse diff order

	Cycle();
	~Cycle();

	// Lazy initialization, allocate memory etc as needed and return success status of the allocation. Used to avoid relying on cpp exceptions in constructor.
	bool initialize(int length, int creates); 

	int getFrameWithLargestDiff(int offset);
	bool includes(int frame);
	bool isBadFrame(int n);

	void reset();

private:
	int getBorderFrame(bool first);
};

#endif
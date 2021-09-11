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

#include <string.h>
#include <memory>
#include "Cycle.h"

float sceneThreshold;

int cmpDescendingDiff(const void * a, const void * b);

Cycle::Cycle(int length, int creates) :
	length(length),
	creates(creates),
	diffs(std::make_unique<CycleDiff[]>(length)),
	sortedDiffs(std::make_unique<CycleDiff[]>(length)),
	frameMap(std::make_unique<FrameMap[]>(length + creates)),
	sorted(false)
{
	reset();
}

void Cycle::reset() {
	for (int i = 0; i < length; i++) {
		diffs[i].frame   = -1;
		diffs[i].diff    = -1;
		sortedDiffs[i].frame = -1;
		sortedDiffs[i].diff  = -1;
	}
	sorted = false;
}

void Cycle::updateFrameMap() {
	if (diffs[0].frame == -1) return;

	int scaledLength = length + creates;
	int srcCycleStart = diffs[0].frame;
	int dstCycleStart = srcCycleStart * scaledLength / length;
	int cn;

	for (int i = 0, di = 0; i < length; i++, di++) {
		cn = diffs[i].frame;
		if (isSceneChange(cn)) {
			frameMap[di].dstframe = dstCycleStart + di;
			frameMap[di].srcframe = cn;
			frameMap[di].altclip = false;
			di++;
		} else if (isBadFrame(cn)) {
			frameMap[di].dstframe = dstCycleStart + di;
			frameMap[di].srcframe = cn;
			frameMap[di].altclip = true;
			di++;
		}
		frameMap[di].dstframe = dstCycleStart + di;
		frameMap[di].srcframe = cn;
		frameMap[di].altclip = false;
	}
}

bool Cycle::includes(int frame) {
	for (int i = 0; i < length; i++) {
		if (diffs[i].frame == frame) {
			return true;
		}
	}
	return false;
}

bool Cycle::isBadFrame(int frame) {
	// Rules:
	// 1. A cycle contains at most one scene change.
	// 2. A scene change is not considered a bad frame.
	// 3. A scene change is a frame that would be classified as Bad, but is so bad that it exceeds the schene threshold.
	// 4. Any frame except a scene frame is judged depending on number of creates required for the cycle (ordered by diffs in descending order).
	// Thus the top "create" frames with respect to their diff values, exclusing a possible scene change, are considered bad frames in the cycle.
	sortDiffsIfNeeded();
	int sceneSchangesInCycle = hasSceneChange() ? 1 : 0;

	for (int i = sceneSchangesInCycle; i < creates && i < length; i++) {
		if (getFrameWithLargestDiff(i) == frame) {
			return true;
		}
	}
	return false;
}

bool Cycle::isSceneChange(int frame) {
	sortDiffsIfNeeded();
	return sortedDiffs[0].frame == frame && hasSceneChange();
}

bool Cycle::hasSceneChange() {
	return sceneThreshold < sortedDiffs[0].diff;
}

int Cycle::getFrameWithLargestDiff(int offset) {
	sortDiffsIfNeeded();
	if (offset > length - 1) return -1;
	return sortedDiffs[offset].frame;
}

void Cycle::sortDiffsIfNeeded() {
	if (!sorted) {
		memcpy(sortedDiffs.get(), diffs.get(), length * sizeof(CycleDiff));
		qsort(sortedDiffs.get(), length, sizeof(CycleDiff), cmpDescendingDiff);
		sorted = true;
	}
}

int cmpDescendingDiff(const void * a, const void * b) {
	auto pa = reinterpret_cast<const CycleDiff*>(a);
	auto pb = reinterpret_cast<const CycleDiff*>(b);
	double result = static_cast<double>(pa->diff) - static_cast<double>(pb->diff);
	return result > 0 ? -1 : result < 0 ? 1 : 0;
}
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
#include <stdlib.h>
#include "Cycle.h"

int cmpDescendingDiff(const void * a, const void * b);

Cycle::Cycle(){
	length = -1;
	creates = -1;
}

Cycle::~Cycle() {
	free(diffs);
	free(sortedDiffs);
}

bool Cycle::initialize(int length, int creates) {
	if (this->length != -1) return true;
	this->length = length;
	this->creates = creates;
	this->diffs = (CycleDiff*)malloc(length * sizeof(CycleDiff));
	this->sortedDiffs = (CycleDiff*)malloc(length * sizeof(CycleDiff));
	if (diffs && sortedDiffs) reset();
	return diffs && sortedDiffs;
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

bool Cycle::includes(int frame) {
	for (int i = 0; i < length; i++) {
		if (diffs[i].frame == frame) {
			return true;
		}
	}
	return false;
}

bool Cycle::isBadFrame(int frame) {
	for (int i = 0; i < creates; i++) {
		if (getFrameWithLargestDiff(i) == frame) {
			return true;
		}
	}
	return false;
}

int Cycle::getFrameWithLargestDiff(int offset) {
	if (!sorted) {
		memcpy(sortedDiffs, diffs, length * sizeof(CycleDiff));
		qsort(sortedDiffs, length, sizeof(CycleDiff), cmpDescendingDiff);
		sorted = true;
	}
	if (offset > length - 1) return -1;
	return sortedDiffs[offset].frame;
}

int cmpDescendingDiff(const void * a, const void * b) {
	double result = ((*(CycleDiff*)a).diff - (*(CycleDiff*)b).diff);
	return result > 0 ? -1 : result < 0 ? 1 : 0;
}
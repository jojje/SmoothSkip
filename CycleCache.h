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

#ifndef __CYCLECACHE_H
#define __CYCLECACHE_H

#include "cycle.h"
#include <vector>

class CycleCache {
	int cycleCount;
	int cycleLen;
	int creates;
	std::vector<Cycle*> cycles;

public:
	CycleCache(int cycleLength, int createsPerCycle, int clipFrameCount);
	~CycleCache();
	bool initialize();
	Cycle* CycleCache::GetCycleForFrame(int n);
};

#endif
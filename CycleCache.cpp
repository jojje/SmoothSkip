#include <stdlib.h>
#include "Cycle.h"
#include "CycleCache.h"

using namespace std;

CycleCache::CycleCache(int cycleLength, int createsPerCycle, int clipFrameCount) : 
	cycleLen(cycleLength), creates(createsPerCycle)
{
	// 1. If the final cycle is a partial, account for it by adding an extra cycle in which the partial cycle frames can be stored.
	// 2. Expand the vector in one go to avoid incremental expansions and memory fragmentations.
	// 3. Zero-allocate and initialize the vector elements for the entire clip.

	cycleCount = clipFrameCount / cycleLength;
	if (clipFrameCount % cycleLength != 0) {
		++cycleCount;
	}

	cycles.reserve(cycles.capacity() + cycleCount);

	for (int i = 0; i < cycleCount; i++) {
		cycles.emplace_back(make_unique<Cycle>(cycleLen, creates));
	}
}

Cycle* CycleCache::GetCycleForFrame(int n)
{
	int CycleIdx = n / (cycleLen + creates);
	return cycles.at(CycleIdx).get();
}


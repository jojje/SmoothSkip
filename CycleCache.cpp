#include <stdlib.h>
#include <new>
#include "Cycle.h"
#include "CycleCache.h"


CycleCache::CycleCache(int cycleLength, int createsPerCycle, int clipFrameCount) : 
	cycleLen(cycleLength), creates(createsPerCycle)
{
	cycleCount = clipFrameCount / cycleLength;
	if (clipFrameCount % cycleLength != 0) {
		++cycleCount;
	}

	cycles.reserve(cycles.capacity() + cycleCount);
	for (int i = 0; i < cycleCount; i++) {
		cycles.push_back(new Cycle(cycleLen, creates));
	}
}

CycleCache::~CycleCache()
{
	for (auto const& cycle : cycles) {
		delete cycle;
	}
	cycles.clear();
	cycles.swap(std::vector<Cycle*>());
}

Cycle* CycleCache::GetCycleForFrame(int n)
{
	int CycleIdx = n / (cycleLen + creates);
	return cycles.at(CycleIdx);
}


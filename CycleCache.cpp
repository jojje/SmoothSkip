#include <stdlib.h>
#include "Cycle.h"
#include "CycleCache.h"

using namespace std;

CycleCache::CycleCache(int cycleLength, int createsPerCycle, int clipFrameCount) : 
	cycleLen(cycleLength), creates(createsPerCycle)
{
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


#include "mem/cache/prefetch/adaptive.hh"

#include "base/trace.hh"
#include "debug/AdaptivePrefetcher.hh"
#include "sim/cur_tick.hh"
#include <cstdlib> 

namespace gem5
{
namespace prefetch {

AdaptivePrefetcher::AdaptivePrefetcher(const AdaptivePrefetcherParams &p)
    : Queued(p),
      mode1(p.mode1),
      mode2(p.mode2),
      currentMode(1),
      windowSize(p.window_size),
      windowAccesses(0),

      nextLineCount(0), // next line 
      streamCount(0), // next line > 1 time in a row 
      randomCount(0), // random 
      sameLineCount(0), // same cache line 
      fixedStrideCount(0), // fixed |stride| > 1
      backwardCount(0), // backward 

      lastAbsStride(0),
      lastLine(0),
      lastStride(0),
      lastValid(false),

      lastModeSwitchTick(curTick())
{
}

void
AdaptivePrefetcher::regStats()
{
    // 1) Always call base-class first
    Queued::regStats();

    using namespace statistics;

    // 2) Register mode-switch counter
    numModeSwitches
        .name(name() + ".num_mode_switches")
        .desc("Number of adaptive prefetcher mode switches");

    // 3) Register time-in-mode counters
    timeInMode0
        .name(name() + ".time_in_mode0")
        .desc("Total ticks spent in mode 0 (disabled)");

    timeInMode1
        .name(name() + ".time_in_mode1")
        .desc("Total ticks spent in mode 1 (stride)");

    timeInMode2
        .name(name() + ".time_in_mode2")
        .desc("Total ticks spent in mode 2 (tagged)");
}


void
AdaptivePrefetcher::chargeTimeInCurrentMode(Tick now)
{
    Tick delta = now - lastModeSwitchTick;
    switch (currentMode) {
      case 0: timeInMode0 += delta; break;
      case 1: timeInMode1 += delta; break;
      case 2: timeInMode2 += delta; break;
      default: break;
    }
    lastModeSwitchTick = now;
}

void
AdaptivePrefetcher::chooseMode()
{
    uint64_t total = nextLineCount + streamCount + randomCount + backwardCount + sameLineCount + fixedStrideCount;
    if (total == 0)
        return;

    double p_next    = double(nextLineCount)    / double(total);
    double p_stream = double(streamCount) / double(total);
    double p_random = double(randomCount) / double(total);
    double p_backward = double(backwardCount) / double(total);
    double p_same     = double(sameLineCount) / double(total);
    double p_stride = double(fixedStrideCount) / double(total);

    int oldMode = currentMode;

    if (p_random > 0.6 || p_same > 0.6) {
        currentMode = 0; // disabled
    } else if ((p_next + p_stream + p_backward + p_stride) > 0.6) {
        currentMode = 1; // stride/stream
    } else {
        currentMode = 2; // tagged/BOP
    }

    if (currentMode != oldMode) {
        chargeTimeInCurrentMode(curTick());
        numModeSwitches++;
        DPRINTF(AdaptivePrefetcher, "Switching mode %d -> %d "
                "(p_next=%.2f p_stream=%.2f p_random=%.2f p_backward=%.2f p_same=%.2f p_stride=%.2f)\n",
                oldMode, currentMode, p_next, p_stream, p_random, p_backward, p_same, p_stride);
    }

    // Reset window
    windowAccesses = 0;
    streamCount = randomCount = sameLineCount = backwardCount = nextLineCount = fixedStrideCount = 0 ;
}

void
AdaptivePrefetcher::updatePatternStats(const PrefetchInfo &pf_info)
{
    // using PrefetchInfo = Base::PrefetchInfo;

    // Only care about reads
    if (pf_info.isWrite())
        return;

    Addr line = pf_info.getAddr() / blkSize;

    if (!lastValid) {
        lastLine = line;
        lastStride = 0;
        lastAbsStride = 0;
        lastValid = true;
        randomCount++;
        windowAccesses++;
        return;
    }

    int64_t stride = (int64_t)line - (int64_t)lastLine;
    uint64_t absStride = llabs(stride);

    if (stride == 0) {
        sameLineCount++;
    } 
    else if (stride == 1) {
        if (lastStride == 1)
            streamCount++;
        else
            nextLineCount++;
    } 
    else if (stride == -1) {
        if (lastStride == -1)    
            streamCount++;
        else
            backwardCount++;
    } 
    else if (absStride > 1) {
        if (absStride == lastAbsStride)
            fixedStrideCount++;
        else 
            randomCount++;
    }
    else {
        randomCount++;
    }

    lastLine = line;
    lastStride = stride;
    lastAbsStride = absStride;
    windowAccesses++;
}

void
AdaptivePrefetcher::calculatePrefetch(
    const PrefetchInfo &pfi,
    std::vector<AddrPriority> &addresses,
    const CacheAccessor &cache)
{
    // Update pattern stats
    updatePatternStats(pfi);

    // Decide mode at window boundary
    if (windowAccesses >= windowSize) {
        chooseMode();
    }

    // Delegate based on current mode
    switch (currentMode) {
      case 0:
        // Disabled: do nothing
        break;

      case 1:
        if (mode1) {
            mode1->calculatePrefetch(pfi, addresses, cache);
        }
        break;

      case 2:
        if (mode2) {
            mode2->calculatePrefetch(pfi, addresses, cache);
        }
        break;
      default:
        break;
    }
}

void
AdaptivePrefetcher::notifyFill(const CacheAccessProbeArg &arg)
{
    switch (currentMode) {
      case 1:
        if (mode1)
            mode1->notifyFill(arg);
        break;
      case 2:
        if (mode2)
            mode2->notifyFill(arg);
        break;
      default:
        break; // disabled
    }
}

void
AdaptivePrefetcher::notify(const CacheAccessProbeArg &acc,
                           const PrefetchInfo &pfi)
{
    Queued::notify(acc, pfi);

    // 2) Safety guard
    if (!acc.pkt || !acc.pkt->hasData())
        return;

    // 3) Forward to BOTH modes to keep their stats correct
    if (mode1)
        mode1->notify(acc, pfi);

    if (mode2)
        mode2->notify(acc, pfi);
}


} // namespace prefetch
} // namespace gem5

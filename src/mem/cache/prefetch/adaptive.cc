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

      lastLine(0),
      lastStride(0),
      lastValid(false),
      stableStrideCount(0),

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
AdaptivePrefetcher::chargeTimeInCurrentMode(Tick now, int oldMode, int newMode)
{
    Tick delta = now - lastModeSwitchTick;
    switch (oldMode) {
      case 0: timeInMode0 += delta; break;
      case 1: timeInMode1 += delta; break;
      case 2: timeInMode2 += delta; break;
      default: break;
    }
    lastModeSwitchTick = now;
    currentMode = newMode;
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
    int newMode;

    if (p_random > 0.85) {
        newMode = 0; // disabled
    } else if ((p_same + p_next + p_stream + p_backward + p_stride) > 0.6) {
        newMode = 1; // stride/stream
    } else {
        newMode = 2; // tagged/BOP
    }

    if (newMode != oldMode) {
        chargeTimeInCurrentMode(curTick(), oldMode, newMode);
        numModeSwitches++;
        DPRINTF(AdaptivePrefetcher, "Switching mode %d -> %d "
                "(p_next=%.2f p_stream=%.2f p_random=%.2f p_backward=%.2f p_same=%.2f p_stride=%.2f)\n",
                oldMode, currentMode, p_next, p_stream, p_random, p_backward, p_same, p_stride);
    }

    // Reset window
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
        lastValid = true;
        randomCount++;
        stableStrideCount=0;
        return;
    }

    int64_t stride = (int64_t)line - (int64_t)lastLine;

    if (stride == 0) {
        sameLineCount++;
        stableStrideCount=0;
    } 
    else if (stride == 1) {
        if (lastStride == 1)
            streamCount++;
        else
            nextLineCount++;

        stableStrideCount=0;
    } 
    else if (stride == -1) {
            backwardCount++;
            stableStrideCount=0;
    }
    else {
        if (stride == lastStride) {
            stableStrideCount++;
        } else {
            stableStrideCount=0;
        }

        if (stableStrideCount >=1) {
            fixedStrideCount++;
            // DPRINTF(AdaptivePrefetcher, "FIXED STRIDE %ld detected\n", stride);
        } else {
            randomCount++;
            // DPRINTF(AdaptivePrefetcher, "RANDOM STRIDE %ld detected\n", stride);
        }
    }

    lastLine = line;
    lastStride = stride;
}

void
AdaptivePrefetcher::calculatePrefetch(
    const PrefetchInfo &pfi,
    std::vector<AddrPriority> &addresses,
    const CacheAccessor &cache)
{
     if (currentMode == 1 && mode1) {
        mode1->calculatePrefetch(pfi, addresses, cache);
    } else if (currentMode == 2 && mode2) {
        mode2->calculatePrefetch(pfi, addresses, cache);
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


    if (!acc.pkt->isRead() || acc.pkt->req->isInstFetch())
        return;

    if (currentMode == 1 && mode1)
        mode1->notify(acc, pfi);
    else if (currentMode == 2 && mode2)
        mode2->notify(acc, pfi);

    // Update pattern stats
    updatePatternStats(pfi);

    // Decide mode at window boundary
    if (++windowAccesses >= windowSize) {
        chooseMode();
        windowAccesses = 0;
    }
}


} // namespace prefetch
} // namespace gem5

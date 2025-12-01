#ifndef __MEM_CACHE_PREFETCH_ADAPTIVE_HH__
#define __MEM_CACHE_PREFETCH_ADAPTIVE_HH__

#include "mem/cache/prefetch/queued.hh"
#include "base/statistics.hh"
#include "params/AdaptivePrefetcher.hh"


#include <vector>
#include <utility>
#include <cstdint>
#include <cstdlib>  



namespace gem5
{
namespace prefetch
{

class AdaptivePrefetcher : public Queued
{
  public:
    // using PrefetchInfo = Base::PrefetchInfo;
    // using AddrPriority = std::pair<Addr, int32_t>;
    AdaptivePrefetcher(const AdaptivePrefetcherParams &p);

    void calculatePrefetch(const PrefetchInfo &pfi,
                       std::vector<AddrPriority> &addresses,
                       const CacheAccessor &cache) override;


    void regStats() override;

    void notifyFill(const CacheAccessProbeArg &arg) override;
    void notify(const CacheAccessProbeArg &acc, const PrefetchInfo &pfi) override;

    


  private:
    // Sub-prefetchers (configured via Python)
    Queued *mode1;
    Queued *mode2;


    // 0: disabled, 1: mode1, 2: mode2
    int currentMode;

    // Pattern stats for the current window
    uint64_t windowSize;
    uint64_t windowAccesses;

    uint64_t nextLineCount;
    uint64_t streamCount;
    uint64_t randomCount;
    uint64_t sameLineCount;
    uint64_t backwardCount;
    uint64_t fixedStrideCount;

    Addr lastLine;
    int64_t lastStride;
    bool lastValid;
    uint64_t lastAbsStride; 

    // Stats
    statistics::Scalar numModeSwitches;
    statistics::Scalar timeInMode0;
    statistics::Scalar timeInMode1;
    statistics::Scalar timeInMode2;

    Tick lastModeSwitchTick;

    // Helpers
    void updatePatternStats(const PrefetchInfo &pf_info);
    void chooseMode();
    void chargeTimeInCurrentMode(Tick now);
};

}
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_ADAPTIVE_HH__
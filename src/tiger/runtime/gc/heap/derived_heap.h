#pragma once

#include "algorithm"
#include "heap.h"
#include <vector>

namespace gc {

class DerivedHeap : public TigerHeap {
  // TODO(lab7): You need to implement those interfaces inherited from TigerHeap
  // correctly according to your design.
public:
  char *Allocate(uint64_t size);
  uint64_t Used() const;
  uint64_t MaxFree() const;
  void Initialize(uint64_t size);
  void GC();

public:
  struct RecordInfo {
    char *record_begin;
    int record_size;
    int descriptor_size;
    unsigned char *descriptor;
  };

  struct ArrayInfo {
    char *array_begin;
    int array_size;
  };

  struct FreeIntervalInfo {
    int interval_size;
    char *interval_start;
    char *interval_end;
  };

  struct MarkResult {
    std::vector<int> arraies_active_bitmap;
    std::vector<int> records_active_bitmap;
  };

  struct PointerMapBin {
    uint64_t return_address_;
    uint64_t next_pointerMap_;
    uint64_t is_main_;
    uint64_t frame_size_;
    std::vector<int64_t> offsets_;
  };

  static inline bool CompareIntervalBySize(FreeIntervalInfo interval_A,
                                           FreeIntervalInfo interval_B) {
    return interval_A.interval_size < interval_B.interval_size;
  }

  static inline bool CompareIntervalByStartAddr(FreeIntervalInfo interval_A,
                                                FreeIntervalInfo interval_B) {
    return (uint64_t)interval_A.interval_start <
           (uint64_t)interval_B.interval_start;
  }

  inline void SortIntervalBySize();

  inline void SortIntervalByStartAddr();

  char *AllocateRecord(uint64_t size, int des_size, unsigned char *des_ptr,
                       uint64_t *sp);

  char *AllocateArray(uint64_t size, uint64_t *sp);

  void GetAllPointerMaps();

  FreeIntervalInfo FindFit(int size);

  MarkResult Mark();

  std::vector<uint64_t> AddressToMark();

  void MarkAnAddress(uint64_t address, std::vector<int> &arraies_active_bitmap,
                     std::vector<int> &records_active_bitmap);

  inline void ScanARecord(RecordInfo record,
                          std::vector<int> &arraies_active_bitmap,
                          std::vector<int> &records_active_bitmap);

  void Sweep(MarkResult bitmaps);

  void Coalesce();

private:
  char *heap_root;
  uint64_t *tiger_stack;
  std::vector<RecordInfo> records_in_heap;
  std::vector<ArrayInfo> arraies_in_heap;
  std::vector<FreeIntervalInfo> free_intervals;
  std::vector<PointerMapBin> pointer_maps;
};
} // namespace gc

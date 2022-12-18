#include "derived_heap.h"
#include <cstring>
#include <stack>
#include <stdio.h>

namespace gc {
// TODO(lab7): You need to implement those interfaces inherited from TigerHeap
// correctly according to your design.
char *DerivedHeap::Allocate(uint64_t size) {
  if (size > MaxFree())
    return nullptr;
  return FindFit(size).interval_start;
}

uint64_t DerivedHeap::Used() const {
  uint64_t used = 0;
  for (RecordInfo record_info : records_in_heap)
    used += record_info.record_size;
  for (ArrayInfo array_info : arraies_in_heap)
    used += array_info.array_size;
  return used;
}

uint64_t DerivedHeap::MaxFree() const {
  int max_free = 0;
  for (FreeIntervalInfo free_interval : free_intervals)
    if (free_interval.interval_size > max_free)
      max_free = free_interval.interval_size;
  return max_free;
}

void DerivedHeap::Initialize(uint64_t size) {
  heap_root = (char *)malloc(size);
  FreeIntervalInfo free_interval;
  free_interval.interval_size = size;
  free_interval.interval_start = heap_root;
  free_interval.interval_end = heap_root + size;
  free_intervals.push_back(free_interval);
  GetAllPointerMaps();
}

void DerivedHeap::GC() { Sweep(Mark()); }

inline void DerivedHeap::SortIntervalBySize() {
  std::sort(free_intervals.begin(), free_intervals.end(),
            DerivedHeap::CompareIntervalBySize);
}

inline void DerivedHeap::SortIntervalByStartAddr() {
  std::sort(free_intervals.begin(), free_intervals.end(),
            DerivedHeap::CompareIntervalByStartAddr);
}

char *DerivedHeap::AllocateRecord(uint64_t size, int des_size,
                                  unsigned char *des_ptr, uint64_t *sp) {
  tiger_stack = sp;
  char *record_begin = Allocate(size);
  if (!record_begin)
    return nullptr;
  records_in_heap.push_back(
      RecordInfo{record_begin, (int)size, des_size, des_ptr});
  return record_begin;
}

char *DerivedHeap::AllocateArray(uint64_t size, uint64_t *sp) {
  tiger_stack = sp;
  char *array_begin = Allocate(size);
  if (!array_begin)
    return nullptr;
  arraies_in_heap.push_back(ArrayInfo{array_begin, (int)size});
  return array_begin;
}

void DerivedHeap::GetAllPointerMaps() {
  uint64_t *iter = &GLOBAL_GC_ROOTS;
  while (true) {
    uint64_t return_address = *iter;
    iter++;
    uint64_t nextPointerMapAddress = *iter;
    iter++;
    uint64_t is_main = *iter;
    iter++;
    uint64_t frame_size = *iter;
    iter++;
    std::vector<int64_t> offsets;
    while (true) {
      int64_t offset = *iter;
      iter++;
      if (offset == -1)
        break;
      offsets.push_back(offset);
    }
    pointer_maps.push_back(PointerMapBin{return_address, nextPointerMapAddress,
                                         is_main, frame_size, offsets});
    if (nextPointerMapAddress == 0)
      break;
  }
}

DerivedHeap::FreeIntervalInfo DerivedHeap::FindFit(int size) {
  for (auto free_intervals_it = free_intervals.begin();
       free_intervals_it != free_intervals.end(); free_intervals_it++) {
    if ((*free_intervals_it).interval_size == size) {
      FreeIntervalInfo best_fit = (*free_intervals_it);
      free_intervals.erase(free_intervals_it);
      return best_fit;
    }
    if ((*free_intervals_it).interval_size > size) {
      FreeIntervalInfo best_fit =
          FreeIntervalInfo{size, (*free_intervals_it).interval_start,
                           (*free_intervals_it).interval_start + size};
      FreeIntervalInfo rest_interval{(*free_intervals_it).interval_size - size,
                                     best_fit.interval_end,
                                     (*free_intervals_it).interval_end};
      free_intervals.erase(free_intervals_it);
      free_intervals.push_back(rest_interval);
      SortIntervalBySize();
      return best_fit;
    }
  }
}

DerivedHeap::MarkResult DerivedHeap::Mark() {
  std::vector<int> arraies_active_bitmap(arraies_in_heap.size(), 0);
  std::vector<int> records_active_bitmap(records_in_heap.size(), 0);
  std::vector<uint64_t> pointers = AddressToMark();
  for (auto pointer : pointers)
    MarkAnAddress(pointer, arraies_active_bitmap, records_active_bitmap);
  return MarkResult{arraies_active_bitmap, records_active_bitmap};
}

std::vector<uint64_t> DerivedHeap::AddressToMark() {
  std::vector<uint64_t> pointers;
  uint64_t *sp = tiger_stack;
  bool is_main = false;
  while (!is_main) {
    uint64_t return_address = *(sp - 1);
    for (auto pointMap : pointer_maps)
      if (pointMap.return_address_ == return_address) {
        for (auto offset : pointMap.offsets_)
          pointers.push_back(*(uint64_t *)(offset + (int64_t)sp +
                                           (int64_t)pointMap.frame_size_));
        sp += (pointMap.frame_size_ / WORD_SIZE + 1);
        is_main = pointMap.is_main_;
        break;
      }
  }
  return pointers;
}

void DerivedHeap::MarkAnAddress(uint64_t address,
                                std::vector<int> &arraies_active_bitmap,
                                std::vector<int> &records_active_bitmap) {
  for (int i = 0; i < (int)arraies_active_bitmap.size(); i++) {
    int delta = address - (uint64_t)arraies_in_heap[i].array_begin;
    if (delta >= 0 && delta <= (arraies_in_heap[i].array_size - WORD_SIZE)) {
      arraies_active_bitmap[i] = 1;
      return;
    }
  }

  int size = records_in_heap.size();
  for (int i = 0; i < (int)records_active_bitmap.size(); i++) {
    int64_t delta = (int64_t)address - (int64_t)records_in_heap[i].record_begin;
    if (delta >= 0 && delta <= (records_in_heap[i].record_size - WORD_SIZE)) {
      if (records_active_bitmap[i])
        return;
      ScanARecord(records_in_heap[i], arraies_active_bitmap,
                  records_active_bitmap);
      records_active_bitmap[i] = 1;
      return;
    }
  }
}

inline void DerivedHeap::ScanARecord(RecordInfo record,
                                     std::vector<int> &arraies_active_bitmap,
                                     std::vector<int> &records_active_bitmap) {
  for (int i = 0; i < record.descriptor_size; i++)
    if (record.descriptor[i] == '1')
      MarkAnAddress(*((uint64_t *)(record.record_begin + WORD_SIZE * i)),
                    arraies_active_bitmap, records_active_bitmap);
}

void DerivedHeap::Sweep(MarkResult bitmaps) {
  std::vector<ArrayInfo> new_arraies_in_heap;
  for (int i = 0; i < (int)bitmaps.arraies_active_bitmap.size(); i++) {
    if (bitmaps.arraies_active_bitmap[i])
      new_arraies_in_heap.push_back(arraies_in_heap[i]);
    else
      free_intervals.push_back(FreeIntervalInfo{
          arraies_in_heap[i].array_size, (char *)arraies_in_heap[i].array_begin,
          (char *)arraies_in_heap[i].array_begin +
              arraies_in_heap[i].array_size});
  }

  std::vector<RecordInfo> new_records_in_heap;
  for (int i = 0; i < (int)bitmaps.records_active_bitmap.size(); i++) {
    if (bitmaps.records_active_bitmap[i])
      new_records_in_heap.push_back(records_in_heap[i]);
    else
      free_intervals.push_back(
          FreeIntervalInfo{records_in_heap[i].record_size,
                           (char *)records_in_heap[i].record_begin,
                           (char *)records_in_heap[i].record_begin +
                               records_in_heap[i].record_size});
  }

  records_in_heap = new_records_in_heap;
  arraies_in_heap = new_arraies_in_heap;
  Coalesce();
}

void DerivedHeap::Coalesce() {
  SortIntervalByStartAddr();
  for (auto free_intervals_it = free_intervals.begin();
       std::next(free_intervals_it) != free_intervals.end();) {
    auto it_next = std::next(free_intervals_it);
    if ((uint64_t)(*free_intervals_it).interval_end ==
        (uint64_t)(*it_next).interval_start) {
      FreeIntervalInfo newIterval{
          (*free_intervals_it).interval_size + (*it_next).interval_size,
          (*free_intervals_it).interval_start, (*it_next).interval_end};
      free_intervals_it = free_intervals.erase(free_intervals_it);
      free_intervals_it = free_intervals.erase(free_intervals_it);
      free_intervals_it = free_intervals.insert(free_intervals_it, newIterval);
      continue;
    }
    free_intervals_it++;
  }
  SortIntervalBySize();
}
} // namespace gc

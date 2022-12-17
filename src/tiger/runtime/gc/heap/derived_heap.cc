#include "derived_heap.h"
#include <stdio.h>
#include <stack>
#include <cstring>

namespace gc {
// TODO(lab7): You need to implement those interfaces inherited from TigerHeap correctly according to your design.

inline void DerivedHeap::SortIntervalBySize() {
  std::sort(free_intervals.begin(), free_intervals.end(),
            DerivedHeap::compareIntervalBySize);
}

inline void DerivedHeap::SortIntervalByStartAddr() {
  std::sort(free_intervals.begin(), free_intervals.end(),
            DerivedHeap::compareIntervalByStartAddr);
}

/* 合并相邻的空闲区间 */
void DerivedHeap::Coalesce() {
  SortIntervalByStartAddr();
  auto iter = free_intervals.begin();
  while ((iter + 1) != free_intervals.end()) {
    auto iter_next = iter;
    iter_next++;
    if ((uint64_t)(*iter).intervalEnd == (uint64_t)(*iter_next).intervalStart) {
      freeIntervalInfo newIterval;
      newIterval.intervalStart = (*iter).intervalStart;
      newIterval.intervalEnd = (*iter_next).intervalEnd;
      newIterval.intervalSize =
          (*iter).intervalSize + (*iter_next).intervalSize;

      iter = free_intervals.erase(iter);  // iter指向iter_next
      iter = free_intervals.erase(iter);  // iter指向下一个item
      iter = free_intervals.insert(iter, newIterval);  // iter指向newInterval
      continue;
    }
    iter++;
  }

  SortIntervalBySize();
}

/* 寻找interval_size最接近(>=)size的空闲区间, 并将其从free_intervals中删除 */
DerivedHeap::freeIntervalInfo DerivedHeap::FindFit(int size) {
  auto iter = free_intervals.begin();
  freeIntervalInfo bestFit;
  while (iter != free_intervals.end()) {
    if ((*iter).intervalSize == size) {
      bestFit = (*iter);
      free_intervals.erase(iter);
      return bestFit;
    }
    if ((*iter).intervalSize > size) {
      bestFit.intervalStart = (*iter).intervalStart;
      bestFit.intervalSize = size;
      bestFit.intervalEnd = (*iter).intervalStart + size;
      freeIntervalInfo restInterval;
      restInterval.intervalStart = bestFit.intervalEnd;
      restInterval.intervalSize = (*iter).intervalSize - size;
      restInterval.intervalEnd = (*iter).intervalEnd;
      free_intervals.erase(iter);
      free_intervals.push_back(restInterval);
      // printf("erase interal(size = %d) and add interval(size  = %d)",
      //        (int)bestFit.intervalSize, (int)restInterval.intervalSize);
      SortIntervalBySize();
      return bestFit;
    }
    iter++;
  }
}

char *DerivedHeap::Allocate(uint64_t size) {
  if (size > MaxFree()) {
    // std::cout << "space not enough" << std::endl;
    return nullptr;
  }
  freeIntervalInfo interValToUse = FindFit(size);
  return interValToUse.intervalStart;
}

char *DerivedHeap::AllocateRecord(uint64_t size, int des_size,
                                unsigned char *des_ptr, uint64_t *sp) {
  tigerStack = sp;
  char *record_begin = Allocate(size);
  if (!record_begin) return nullptr;
  // std::cout << "alloc_record success" << std::endl;
  recordInfo info;
  info.descriptor = des_ptr;
  info.descriptorSize = des_size;
  info.recordBeginPtr = record_begin;
  info.recordSize = size;
  recordsInHeap.push_back(info);
  return record_begin;
}

/* To ease your burden, you don't need to consider the situation that
   program allocate pointer in array. */
char *DerivedHeap::AllocateArray(uint64_t size, uint64_t *sp) {
  tigerStack = sp;
  char *array_begin = Allocate(size);
  if (!array_begin) return nullptr;
  arrayInfo info;
  info.arrayBeginPtr = array_begin;
  info.arraySize = size;
  arraiesInHeap.push_back(info);

  return array_begin;
}

uint64_t DerivedHeap::Used() const {
  uint64_t used = 0;
  for (const recordInfo &info_rec : recordsInHeap) used += info_rec.recordSize;
  for (const arrayInfo &info_arr : arraiesInHeap) used += info_arr.arraySize;
  return used;
}

uint64_t DerivedHeap::MaxFree() const {
  int maxFree = 0;
  for (const freeIntervalInfo interval : free_intervals)
    if (interval.intervalSize > maxFree) maxFree = interval.intervalSize;
  // std::cout << "maxFree = " << maxFree << std::endl;
  return maxFree;
}

void DerivedHeap::Initialize(uint64_t size) {
  heap_root = (char *)malloc(size);
  freeIntervalInfo freeInterval;
  freeInterval.intervalStart = heap_root;
  freeInterval.intervalSize = size;
  freeInterval.intervalEnd = heap_root + size;  // sizeof(char)=1
  // std::cout << "initialize heap success from "
  //           << (long)freeInterval.intervalStart << " to "
  //           << (long)freeInterval.intervalEnd << std::endl;
  free_intervals.push_back(freeInterval);
  GetAllPointerMaps();
}

void DerivedHeap::Sweep(markResult bitMaps) {
  std::vector<arrayInfo> new_arraiesInHeap;
  for (int i = 0; i < bitMaps.arraiesActiveBitMap.size(); i++) {
    if (bitMaps.arraiesActiveBitMap[i])  // marked, cannot sweep
      new_arraiesInHeap.push_back(arraiesInHeap[i]);
    else {
      freeIntervalInfo freedInterval;
      freedInterval.intervalStart = (char *)arraiesInHeap[i].arrayBeginPtr;
      freedInterval.intervalSize = arraiesInHeap[i].arraySize;
      freedInterval.intervalEnd =
          (char *)arraiesInHeap[i].arrayBeginPtr + arraiesInHeap[i].arraySize;
      free_intervals.push_back(freedInterval);
    }
  }
  std::vector<recordInfo> new_recordsInHeap;
  for (int i = 0; i < bitMaps.recordsActiveBitMap.size(); i++) {
    if (bitMaps.recordsActiveBitMap[i])
      new_recordsInHeap.push_back(recordsInHeap[i]);
    else {
      freeIntervalInfo freedInterval;
      freedInterval.intervalStart = (char *)recordsInHeap[i].recordBeginPtr;
      freedInterval.intervalSize = recordsInHeap[i].recordSize;
      freedInterval.intervalEnd =
          (char *)recordsInHeap[i].recordBeginPtr + recordsInHeap[i].recordSize;
      free_intervals.push_back(freedInterval);
    }
  }
  recordsInHeap = new_recordsInHeap;
  arraiesInHeap = new_arraiesInHeap;
  // std::cout << "interval free size=" << free_intervals.size() << std::endl;
  // std::cout << "new_recordsInHeap  size=" << new_recordsInHeap.size()
  //           << std::endl;
  Coalesce();
}

DerivedHeap::markResult DerivedHeap::Mark() {
  std::vector<int> arraiesActiveBitMap(arraiesInHeap.size(), 0);
  std::vector<int> recordsActiveBitMap(recordsInHeap.size(), 0);
  std::vector<uint64_t> pointers = addressToMark();
  for (uint64_t pointer : pointers)
    MarkAnAddress(pointer, arraiesActiveBitMap, recordsActiveBitMap);
  //以pointer为root开始mark
  markResult bitMaps;
  bitMaps.arraiesActiveBitMap = arraiesActiveBitMap;
  bitMaps.recordsActiveBitMap = recordsActiveBitMap;
  return bitMaps;
}

inline void DerivedHeap::ScanARecord(recordInfo record,
                                   std::vector<int> &arraiesActiveBitMap,
                                   std::vector<int> &recordsActiveBitMap) {
  // std::cout << (uint64_t)record.recordBeginPtr << std::endl;
  long beginAddress = (long)record.recordBeginPtr;
  for (int i = 0; i < record.descriptorSize; i++)
    if (record.descriptor[i] == '1') {
      uint64_t targetAddress = *((uint64_t *)(beginAddress + WORD_SIZE * i));
      //存储指针的帧地址处存储的值
      MarkAnAddress(targetAddress, arraiesActiveBitMap, recordsActiveBitMap);
    }
}

void DerivedHeap::MarkAnAddress(uint64_t address,
                              std::vector<int> &arraiesActiveBitMap,
                              std::vector<int> &recordsActiveBitMap) {
  int arraiesNUM = (int)arraiesActiveBitMap.size();
  int recordsNUM = (int)recordsActiveBitMap.size();

  for (int i = 0; i < arraiesNUM; i++) {
    int delta = address - (uint64_t)arraiesInHeap[i].arrayBeginPtr;
    //解决指向某个中间地址的问题
    if (delta >= 0 && delta <= (arraiesInHeap[i].arraySize - 8)) {
      arraiesActiveBitMap[i] = 1;
      return;
    }
  }
  int size = recordsInHeap.size();
  for (int i = 0; i < recordsNUM; i++) {
    int64_t delta = (int64_t)address - (int64_t)recordsInHeap[i].recordBeginPtr;
    //解决指向某个中间地址的问题
    if (delta >= 0 && delta <= (recordsInHeap[i].recordSize - 8)) {
      if (recordsActiveBitMap[i])
        return;  //已经扫描并mark过，不需要重复进行，避免死循环
      else {
        ScanARecord(recordsInHeap[i], arraiesActiveBitMap, recordsActiveBitMap);
        recordsActiveBitMap[i] = 1;
        return;
      }
    }
  }
}

void DerivedHeap::GC() {
  markResult bitMaps = Mark();
  Sweep(bitMaps);
}


/*************** Root Protocol ***************/
/* 读取pointerMaps */
void DerivedHeap::GetAllPointerMaps() {
  uint64_t *pointerMapStart = &GLOBAL_GC_ROOTS;
  uint64_t *iter = pointerMapStart;
  // loop开始时iter指向map第一个字段的地址
  while (true) {
    //.quad $the call's return address$
    uint64_t returnAddress = *iter;
    //.quad $next pointer map's address$
    iter += 1;
    uint64_t nextPointerMapAddress = *iter;
    //.quad $0/1(is tigermain)$
    iter += 1;
    uint64_t isMain = *iter;
    //.quad $frame size$
    iter += 1;
    uint64_t frameSize = *iter;
    // .quad .quad offset1
    iter += 1;
    std::vector<int64_t> offsets;
    while (true) {
      int64_t offset = *iter;
      iter += 1;
      if (offset == -1)  //.quad -1(end label)
        break;
      else
        offsets.push_back(offset);
    }
    pointerMaps.push_back(PointerMapBin(returnAddress, nextPointerMapAddress,
                                        frameSize, isMain, offsets));
    if (nextPointerMapAddress == 0) break;
  }
//   std::cout << "successfully read pointerMaps" << std::endl;
//   printPointerMap();
}

/* 返回栈中的root */
std::vector<uint64_t> DerivedHeap::addressToMark() {
  std::vector<uint64_t> pointers;
  uint64_t *sp = tigerStack;  // sp为alloc_record的RBP
  /* 每个loop开始时sp指向函数的帧底
   * (1)首先-8(-1 * uint64_t)得到return address
   * (2)sp + framesize为函数的帧顶, +offset即为指针位置
   * (3)+pointMap.frameSize/WORD_SIZE+1即为下一个帧的帧底
   * (4)如果是main函数则终止循环
   *  */
  bool isMain = false;
  while (!isMain) {
    uint64_t returnAddress = *(sp - 1);  //(1)
    for (const PointerMapBin &pointMap : pointerMaps)
      if (pointMap.returnAddress == returnAddress) {
        for (int64_t offset : pointMap.offsets) {
          uint64_t *pointerAddress =
              (uint64_t *)(offset + (int64_t)sp + (int64_t)pointMap.frameSize);
          pointers.push_back(*pointerAddress);  //(2)
        }

        sp += (pointMap.frameSize / WORD_SIZE + 1);
        // std::cout << "mark pointers with return address: " << returnAddress
        //           << std::endl;
        isMain = pointMap.isMain;
        break;
      }
  }
  return pointers;
}

void DerivedHeap::printPointerMap() {
  for (PointerMapBin &pointMap : pointerMaps) {
    std::cout << "return address " << pointMap.returnAddress << std::endl;
    std::cout << "next pointer map address" << pointMap.nextPointerMapAddress
              << std::endl;
    std::cout << "frame size " << pointMap.frameSize << std::endl;
    std::cout << "offset size " << pointMap.offsets.size() << std::endl;
    for (int64_t offset : pointMap.offsets)
      std::cout << "offset " << offset << std::endl;
  }
}

} // namespace gc


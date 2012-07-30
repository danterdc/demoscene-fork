#include <strings.h>

#include "std/debug.h"
#include "std/memory.h"
#include "std/ptrarray.h"

static void DeletePtrArray(PtrArrayT *self) {
  if (self->managed)
    PtrArrayForEach(self, (IterFuncT)MemUnref, NULL);
  MemUnref(self->data);
}

PtrArrayT *NewPtrArray(size_t reserved, bool managed) {
  PtrArrayT *array = NewRecordGC(PtrArrayT, (FreeFuncT)DeletePtrArray);

  array->managed = managed;
  array->reserved = 0;

  PtrArrayResize(array, reserved);

  return array;
}

/*
 * Helper functions.
 */

static void MaybeGrow(PtrArrayT *self, size_t count);
static void MaybeShrink(PtrArrayT *self, size_t count);

static void ClearRange(PtrArrayT *self, size_t begin, size_t end) {
  PtrT *item = &self->data[begin]; 
  PtrT *last = &self->data[end]; 

  do { *item++ = NULL; } while (item <= last);
}

static void FreeRange(PtrArrayT *self, size_t begin, size_t end) {
  if (self->managed) {
    PtrT *item = &self->data[begin];
    PtrT *last = &self->data[end];

    do { MemUnref(*item++); } while (item <= last);
  }
}

static void ShiftRangeLeft(PtrArrayT *self, size_t first, size_t last, size_t by) {
  size_t count = last - first + 1;

  PtrT *item = &self->data[first];
  PtrT *dest = item - by;

  do { *dest++ = *item++; } while (--count);
}

static void ShiftRangeRight(PtrArrayT *self, size_t first, size_t last, size_t by) {
  size_t count = last - first + 1;

  PtrT *item = &self->data[last];
  PtrT *dest = item + by;

  do { *dest-- = *item--; } while (--count);
}

static void CopyIntoRange(PtrArrayT *self, size_t index,
                          PtrT *data, size_t count)
{
  PtrT *item = &self->data[index];

  do { *item++ = *data++; } while (--count);
}

static void RemoveRange(PtrArrayT *self, size_t begin, size_t end) {
  size_t first = end + 1;
  size_t last = self->size - 1;
  size_t toFree = end - begin + 1;

  FreeRange(self, begin, end);
  ShiftRangeLeft(self, first, last, toFree);
  ClearRange(self, first, last);
  MaybeShrink(self, toFree);
}

static void RemoveFast(PtrArrayT *self asm("a0"), PtrT *item asm("a1")) {
  PtrT *last = &self->data[self->size - 1];

  if (self->managed)
      MemUnref(*item);

  if (item != last)
    *item = *last;

  *last = NULL;

  self->size--;
}

/*
 * @brief Check if index is valid and normalise it.
 *
 * Accepts negative indices which point to elements starting from the end of
 * array (eg. -1 is the last element).  Terminates program if index is invalid.
 */

static size_t CheckIndex(PtrArrayT *self, ssize_t index) {
  ASSERT((index < self->size) && (index <= -self->size),
         "Index %d out of bound.", index);

  return (index < 0) ? (self->size - index) : index;
}

/*
 * Getter & setter.
 */

PtrT PtrArrayGet(PtrArrayT *self asm("a0"), ssize_t index asm("d0")) {
  return PtrArrayGetFast(self, CheckIndex(self, index));
}

void PtrArraySet(PtrArrayT *self asm("a0"), ssize_t index asm("d0"),
              PtrT data asm("a1"))
{
  self->data[CheckIndex(self, index)] = data;
}

/*
 * Iteration functions.
 */

void PtrArrayForEach(PtrArrayT *self, IterFuncT func, PtrT data) {
  PtrT *item = &self->data[0];
  PtrT *last = &self->data[self->size - 1];

  do { func(*item++, data); } while (item <= last);
}

void PtrArrayForEachInRange(PtrArrayT *self, ssize_t begin, ssize_t end,
                            IterFuncT func, PtrT data)
{
  PtrT *item = &self->data[CheckIndex(self, begin)];
  PtrT *last = &self->data[CheckIndex(self, end)];

  ASSERT(item < last, "Invalid range of elements specified [%d..%d]!",
         begin, end);

  do { func(*item++, data); } while (item <= last);
}

/*
 * Element adding functions.
 */
void PtrArrayInsertFast(PtrArrayT *self, ssize_t index, PtrT data) {
  MaybeGrow(self, 1);

  {
    PtrT *item = &self->data[0];
    PtrT *last = &self->data[self->size - 1];

    *last = *item;
    *item = data;
  }
}

void PtrArrayInsert(PtrArrayT *self, ssize_t index, PtrT data) {
  index = CheckIndex(self, index);

  MaybeGrow(self, 1);
  ShiftRangeRight(self, index, self->size - 2, 1);

  self->data[index] = data;
}

void PtrArrayInsertElements(PtrArrayT *self, ssize_t index,
                            PtrT *data, size_t count)
{
  index = CheckIndex(self, index);

  MaybeGrow(self, count);
  ShiftRangeRight(self, index, self->size - count - 1, count);
  CopyIntoRange(self, index, data, count);
}

void PtrArrayAppend(PtrArrayT *self, PtrT data) {
  MaybeGrow(self, 1);

  self->data[self->size - 1] = data;
}

void PtrArrayAppendElements(PtrArrayT *self, PtrT *data, size_t count) {
  size_t last = self->size - 1;

  MaybeGrow(self, count);

  {
    PtrT *item = &self->data[last];

    do { *item++ = *data++; } while (--count);
  }
}

/*
 * Element removal functions.
 */

void PtrArrayRemoveFast(PtrArrayT *self, size_t index) {
  PtrT *item = &self->data[index];

  RemoveFast(self, item);
}

void PtrArrayRemove(PtrArrayT *self, ssize_t index) {
  size_t first = CheckIndex(self, index);

  RemoveRange(self, first, first);
}

void PtrArrayRemoveRange(PtrArrayT *self, ssize_t first, ssize_t last) {
  first = CheckIndex(self, first);
  last = CheckIndex(self, last);

  ASSERT(first <= last, "Invalid range of elements specified [%d..%d]!",
         first, last);

  RemoveRange(self, first, last);
}

void PtrArrayFilterFast(PtrArrayT *self, PredicateT func) {
  PtrT *item = self->data;
  PtrT *last = &self->data[self->size - 1];

  while (item <= last) {
    if (func(*item)) {
      item++;
    } else {
      RemoveFast(self, item);
    }
  }
}

/*
 * @brief Change maximum number of element the array can hold.
 *
 * 1) newSize > current size => extra space will be allocated.
 * 2) newSize < current size => some elements will be removed and
 *    reserved space will be shrinked.
 *
 * Note: Allocates one more element that will serve the purpose of temporary
 * element for some operations (i.e. sort, swap).
 */
void PtrArrayResize(PtrArrayT *self, size_t newSize) {
  if (self->data) {
    if (newSize == self->size)
      return;

    if (newSize < self->size) {
      FreeRange(self, newSize, self->size - 1);
      self->size = newSize;
    }
  }

  if (!self->data) {
    self->data = MemNew(newSize * sizeof(PtrT), NULL);
    self->size = 0;
  } else {
    PtrT oldData = self->data;
    self->data = MemDupGC(oldData, newSize * sizeof(PtrT), NULL);
    MemUnref(oldData);
  }

  if (newSize > self->reserved)
    ClearRange(self, self->reserved, newSize);

  self->reserved = newSize;
}

#define MIN_SIZE 16

static size_t NearestPow2(size_t num) {
  size_t i = 1;

  while (num > i)
    i += i;

  return max(i, MIN_SIZE);
}

/*
 * @brief Check if array can accomodate extra elements and resize if needed.
 */
static void MaybeGrow(PtrArrayT *self, size_t count) {
  if (self->size + count <= self->reserved) {
    self->size += count;
  } else {
    PtrArrayResize(self, NearestPow2(self->size));
  }
}

/*
 * @brief Check if array doesn't have too much of an extra space and shrink.
 */
static void MaybeShrink(PtrArrayT *self, size_t count) {
  self->size -= count;
}

/*
 * Sorting functions.
 */

void PtrArrayInsertionSort(PtrArrayT *self, ssize_t begin, ssize_t end) {
  CompareFuncT cmp = self->compareFunc;

  begin = CheckIndex(self, begin);
  end = CheckIndex(self, end);

  ASSERT(cmp, "Compare function not set!");
  ASSERT(begin < end, "Invalid range of elements specified [%d..%d]!", begin, end);

  {
    PtrT *data = self->data;
    size_t toInsert = begin + 1;

    while (toInsert <= end) {
      size_t index = begin;

      while ((index < toInsert) && (cmp(data[index], data[toInsert]) <= 0))
        index++;

      if (index < toInsert) {
        PtrT tmp = data[toInsert];
        ShiftRangeRight(self, index, toInsert - 1, 1);
        data[index] = tmp;
      }

      toInsert++;
    }
  }
}

size_t PtrArrayPartition(PtrArrayT *self, size_t left, size_t right, PtrT pivot) {
  CompareFuncT cmp = self->compareFunc;

  ASSERT(cmp, "Compare function not set!");
  ASSERT(left < right, "Invalid range of elements specified [%d..%d]!",
         (int)left, (int)right);

  {
    PtrT *data = self->data;
    size_t partition = left;

    while (left < right) {
      while ((cmp(data[left], pivot) < 0) && (left < right)) {
        left++;
        partition++;
      }

      while ((cmp(pivot, data[right]) < 0) && (left < right))
        right--;

      PtrArraySwapFast(self, left, right);
      left++;
      right--;
      partition++;
    }

    return partition;
  }
}

static void QuickSort(PtrArrayT *self, size_t left, size_t right) {
  while (left < right) {
    PtrT pivot = PtrArrayGetFast(self, (left + right) / 2);
    size_t i = PtrArrayPartition(self, left, right, pivot);

    if (i - left <= right - i) {
      QuickSort(self, left, i);
      left = i + 1;
    } else {
      QuickSort(self, i + 1, right);
      right = i;
    }
  }
}

void PtrArrayQuickSort(PtrArrayT *self, ssize_t begin, ssize_t end) {
  begin = CheckIndex(self, begin);
  end = CheckIndex(self, end);

  ASSERT(self->compareFunc, "Compare function not set!");
  ASSERT(begin < end, "Invalid range of elements specified [%d..%d]!",
         begin, end);

  QuickSort(self, begin, end);
}

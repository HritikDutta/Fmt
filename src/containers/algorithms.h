#pragma once

#include "darray.h"

template <typename T>
inline u64 q_sort_partition(T* array, int start, int end)
{
    const T& pivot = array[end];
    u64 left_end = start - 1;  // Index for end of left partition

    for (u64 i = start; i <= end - 1; i++)
    {
        if (array[i] < pivot)
        {
            left_end++;
            swap(array[left_end], array[i]);
        }
    }

    swap(array[left_end + 1], array[end]);
    return left_end + 1;
}

template <typename T>
inline void q_sort(T* array, int start, int end)
{
    if (start < end)
    {
        u64 partition_idx = q_sort_partition(array, start, end);
        q_sort(array, start, partition_idx - 1);
        q_sort(array, partition_idx + 1, end);
    }
}

template <typename T>
inline void sort(DynamicArray<T>& array)
{
    q_sort(array.data, 0, array.size - 1);
}
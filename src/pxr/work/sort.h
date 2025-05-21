// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
// Modified by Jeremy Retailleau.

#ifndef PXR_WORK_SORT_H
#define PXR_WORK_SORT_H

/// \file

#include "./threadLimits.h"

#include <tbb/parallel_sort.h>
#include <algorithm>

namespace pxr {

/// Sorts in-place a container that provides begin() and end() methods
///
template <typename C>
void 
WorkParallelSort(C* container)
{
    // Don't bother with parallel_for, if concurrency is limited to 1.
    if (WorkHasConcurrency()) {
        tbb::parallel_sort(container->begin(), container->end());
    }else{
        std::sort(container->begin(), container->end());
    }
}


/// Sorts in-place a container that provides begin() and end() methods,
/// using a custom comparison functor.
///
template <typename C, typename Compare>
void 
WorkParallelSort(C* container, const Compare& comp)
{
    // Don't bother with parallel_for, if concurrency is limited to 1.
    if (WorkHasConcurrency()) {
        tbb::parallel_sort(container->begin(), container->end(), comp);
    }else{
        std::sort(container->begin(), container->end(), comp);
    }
}

}  // namespace pxr

#endif

// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
// Modified by Jeremy Retailleau.

#ifndef PXR_WORK_DETACHED_TASK_H
#define PXR_WORK_DETACHED_TASK_H

/// \file work/detachedTask.h

#include <pxr/tf/errorMark.h>
#include "./api.h"
#include "./dispatcher.h"

#include <type_traits>
#include <utility>

namespace pxr {

template <class Fn>
struct Work_DetachedTask
{
    explicit Work_DetachedTask(Fn &&fn) : _fn(std::move(fn)) {}
    explicit Work_DetachedTask(Fn const &fn) : _fn(fn) {}
    void operator()() const {
        TfErrorMark m;
        _fn();
        m.Clear();
    }
private:
    Fn _fn;
};

WORK_API
WorkDispatcher &Work_GetDetachedDispatcher();

WORK_API
void Work_EnsureDetachedTaskProgress();

/// Invoke \p fn asynchronously, discard any errors it produces, and provide
/// no way to wait for it to complete.
template <class Fn>
void WorkRunDetachedTask(Fn &&fn)
{
    using FnType = typename std::remove_reference<Fn>::type;
    Work_DetachedTask<FnType> task(std::forward<Fn>(fn));
    if (WorkHasConcurrency()) {
        Work_GetDetachedDispatcher().Run(std::move(task));
        Work_EnsureDetachedTaskProgress();
    }
    else {
        task();
    }
}

}  // namespace pxr

#endif // PXR_WORK_DETACHED_TASK_H

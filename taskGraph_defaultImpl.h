//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_BASE_WORK_TASK_GRAPH_DEFAULT_IMPL_H
#define PXR_BASE_WORK_TASK_GRAPH_DEFAULT_IMPL_H

#include "pxr/pxr.h"

#include "pxr/base/work/api.h"
#include "pxr/base/work/dispatcher.h"

#include <atomic>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

// Default implementation for WorkTaskGraph.
// See documentation on WorkTaskGraph and WorkTaskGraph::BaskTask for API docs.

class WorkTaskGraph_DefaultImpl
{
public:
    WorkTaskGraph_DefaultImpl() = default;
    ~WorkTaskGraph_DefaultImpl() noexcept = default;

    WorkTaskGraph_DefaultImpl(WorkTaskGraph_DefaultImpl const &) = delete;
    WorkTaskGraph_DefaultImpl &
    operator=(WorkTaskGraph_DefaultImpl const &) = delete;

    class BaseTask;

    template <typename F, typename ... Args>
    F * AllocateTask(Args&&... args);

    template <typename F>
    inline void RunTask(F * task) {
        _dispatcher.Run(std::ref(*task));
    }

    WORK_API void Wait();

private:
    // Work dispatcher for running and synchronizing on tasks.
    WorkDispatcher _dispatcher;
};

class WorkTaskGraph_DefaultImpl::BaseTask {
public:
    BaseTask() = default;
    WORK_API virtual ~BaseTask();

    /// Callable operator that implements continuation passing, recycling, and 
    /// scheduler bypass. 
    WORK_API void operator()() const;

    virtual BaseTask * execute() = 0;

    void AddChildReference() {
        _childCount.fetch_add(1, std::memory_order_acquire);
    }

    int RemoveChildReference() {
        return _childCount.fetch_sub(1, std::memory_order_release) - 1;
    }
    
    template <typename F, typename ... Args>
    F * AllocateChild(Args&&... args) {
        AddChildReference();
        return _AllocateChildImpl<F>(std::forward<Args>(args)...);
    }

    template <typename C, typename ... Args>
    C * AllocateContinuingChild(Args&&... args) {
        return _AllocateChildImpl<C>(std::forward<Args>(args)...);
    }

protected:
    template <typename C, typename... Args>
    C * _AllocateContinuation(int ref, Args&&... args) {
        C* continuation = new C{std::forward<Args>(args)...};
        continuation->_ResetParent(_ResetParent());
        continuation->_childCount = ref;
        return continuation;
    }

    void _RecycleAsContinuation() {
        _recycle = true;
    }

    template <typename C>
    void _RecycleAsChildOf(C &c) {
        _recycle = true;
        _ResetParent(&c);
    }

private:
    // Allocates a child of this task. 
    template <typename F, typename... Args>
    F* _AllocateChildImpl(Args&&... args) {
        F* obj = new F{std::forward<Args>(args)...};
        obj->_ResetParent(this);
        return obj;
    }

    // Reparent this task under \p ptr
    BaseTask * _ResetParent(BaseTask *ptr = nullptr) {
        BaseTask * p = _parent;
        _parent = ptr;
        return p;
    }

    // The parent/successor of this task. 
    BaseTask * _parent = nullptr;

    // A flag that indicates whether this task is awaiting re-execution.
    bool _recycle = false;

    // An atomic reference count to track this task's pending children. 
    std::atomic<int> _childCount = 0;
};

template <typename F, typename ... Args>
F * WorkTaskGraph_DefaultImpl::AllocateTask(Args&&... args) {
    return new F{std::forward<Args>(args)...};
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif 

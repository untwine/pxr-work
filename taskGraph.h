//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_BASE_WORK_TASK_GRAPH_H
#define PXR_BASE_WORK_TASK_GRAPH_H

#include "pxr/pxr.h"

#include "pxr/base/work/api.h"

#include <tbb/enumerable_thread_specific.h>

#include <utility>
#include <vector>

#if TBB_INTERFACE_VERSION_MAJOR >= 12
#include "pxr/base/work/dispatcher.h"
#include <atomic>
#else
#include <tbb/task.h>
#endif

PXR_NAMESPACE_OPEN_SCOPE

/// Instances of this class are used to spawn and wait on a directed graph of 
/// tasks, where tasks preserve a pointer to their parent task/successor and 
/// a ref count to dynamically track pending children. It supports adding new 
/// tasks during the execution of running tasks, continuation passing, 
/// recycling of task resources, and scheduler bypass. 
///
/// This organization of tasks is suited for problems that exhibit hierarchical 
/// structured parallelism: tasks that discover additional work during their 
/// execution. If these optimizations are not required, consider a higher-level 
/// abstraction, e.g. WorkParallelForEach or direct submission of work via 
/// WorkDispatcher. 
///
class WorkTaskGraph 
{
public:
    WORK_API WorkTaskGraph();
    WORK_API ~WorkTaskGraph() noexcept;

    WorkTaskGraph(WorkTaskGraph const &) = delete;
    WorkTaskGraph &operator=(WorkTaskGraph const &) = delete;

    /// Base class for parallel tasks. 
    class BaseTask;

    /// Container for allocated tasks to be spawned. 
    using TaskList = std::vector<BaseTask *>;

    /// Thread-local storage for allocated tasks to be spawned. 
    using TaskLists = tbb::enumerable_thread_specific<TaskList>;

    /// Allocate and construct a new top-level task to run with RunTask() or
    /// RunLists().
    /// 
    /// The caller owns the returned task until it is passed to RunTask() or
    /// RunLists().
    template <typename F, typename ... Args>
    F * AllocateTask(Args&&... args);

    /// Submit a concurrent task for execution.
    ///
    /// Transfers ownership of \p task to this task graph instance.
    template <typename F>
    inline void RunTask(F * task) {
#if TBB_INTERFACE_VERSION_MAJOR >= 12
        _dispatcher.Run(std::ref(*task));
#else
        tbb::task::spawn(*task);
#endif
    }

    /// Submit concurrent tasks accumulated in thread-local lists for 
    /// execution.
    /// 
    /// Transfers ownership of all the tasks in the \p taskLists to this task
    /// graph instance.
    WORK_API void RunLists(const TaskLists &taskLists);

    /// Wait on all the running tasks to complete.
    WORK_API void Wait();

private: 
#if TBB_INTERFACE_VERSION_MAJOR >= 12
    // Work dispatcher for running and synchronizing on tasks.
    WorkDispatcher _dispatcher;
#else 
    // Task group context for initializing the root task. 
    tbb::task_group_context _context;

    // Root task that serves as an anchor to allocate additional children 
    // during execution. 
    tbb::empty_task *_rootTask;
#endif
};

/// Base class for a parallel task that emulates tbb::task (deprecated in the 
/// oneTBB version upgrade.) This task abstracts a block of concurrent work   
/// by exploiting knowledge of TBB's task-based work stealing scheduler 
/// architecture to provide memory and runtime optimizations. 
/// 
/// This is a callable object that can serve as an anchor to dynamically spawn 
/// additional children. It supports continuation passing, recycling of task 
/// resources, and scheduler bypass. All task graph tasks are heap-allocated 
/// and automatically released/reclaimed using reference counting.  
///
#if TBB_INTERFACE_VERSION_MAJOR >= 12
class WorkTaskGraph::BaseTask {
public:
    BaseTask() = default;
    WORK_API virtual ~BaseTask();

    /// Callable operator that implements continuation passing, recycling, and 
    /// scheduler bypass. 
    WORK_API void operator()() const;

    /// Derived classes override this method to implement a parallel unit of 
    /// work.  
    virtual BaseTask * execute() = 0;

    /// Increment the reference count of child tasks that must complete before 
    /// this task can proceed. 
    void AddChildReference() {
        _childCount.fetch_add(1, std::memory_order_acquire);
    }

    /// Decrement the reference count of child tasks that must complete before 
    /// this task can proceed. 
    int RemoveChildReference() {
        return _childCount.fetch_sub(1, std::memory_order_release) - 1;
    }
    
    /// Construct a new subtask and increment the ref count of the calling 
    /// task. 
    template <typename F, typename ... Args>
    F * AllocateChild(Args&&... args) {
        AddChildReference();
        return _AllocateChildImpl<F>(std::forward<Args>(args)...);
    }

    /// Construct a new subtask of a continuation task and refrain from 
    /// incrementing the reference count of the calling task, since ownership 
    /// of this child has been transferred to the continuation task.   
    template <typename C, typename ... Args>
    C * AllocateContinuingChild(Args&&... args) {
        return _AllocateChildImpl<C>(std::forward<Args>(args)...);
    }

protected:
    /// Allocate a continuation task with \p ref children. 
    /// 
    /// Continuation passing provides an alternative to the task-blocking style 
    /// of execution that results from recursively spawning children and 
    /// waiting for them to complete (e.g. the common fork-join pattern). 
    /// Continuation passing reduces overhead and mitigates the latency 
    /// introduced by work stealing by re-parenting a continuation task under 
    /// children that are about to be spawned. 
    /// 
    /// \note Consider continuation passing if reducing the growth of the stack 
    /// is desirable at the cost of growing the heap. 
    template <typename C, typename... Args>
    C * _AllocateContinuation(int ref, Args&&... args) {
        C* continuation = new C{std::forward<Args>(args)...};
        continuation->_ResetParent(_ResetParent());
        continuation->_childCount = ref;
        return continuation;
    }

    /// Recycles this as a continuation task to mitigate the allocation 
    /// overhead of the continuation task.
    /// 
    /// \note Note that the task graph performs safe continuation passing by 
    /// default, i.e. it assumes an extra increment of the child reference 
    /// count to handle the case when the continued task returns before a 
    /// longer-lived child task. In this case, the extra reference prevents 
    /// the continuation task from executing prematurely and orphaning its 
    /// running child task.   
    void _RecycleAsContinuation() {
        _recycle = true;
    }

    /// Recycles this as a child of continuation task \p c. 
    ///
    /// Mitigates allocation/deallocation overhead by reparenting this under 
    /// \p c. 
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

#else 
class WorkTaskGraph::BaseTask : public tbb::task { 
public:
    BaseTask() = default;
    WORK_API virtual ~BaseTask();

    virtual tbb::task* execute() = 0;

    /// Increment the reference count of child tasks that must complete before 
    /// this task can proceed. 
    void AddChildReference() {
        increment_ref_count();
    }

    /// Decrement the reference count of child tasks that must complete before 
    /// this task can proceed. 
    int RemoveChildReference() {
        return decrement_ref_count();
    }

    /// Construct a new subtask and increment the ref count of the calling 
    /// task.
    template <typename F, typename... Args>
    F * AllocateChild(Args &&...args) {
        return new (tbb::task::allocate_additional_child_of(*this))
            F{std::forward<Args>(args)...};
    }

    /// Construct a new subtask of a continuation task and refrain from 
    /// incrementing the reference count of the calling task, since ownership 
    /// of this child has been transferred to the continuation task.   
    template <typename F, typename... Args>
    F * AllocateContinuingChild(Args &&...args) {
        return new (allocate_child()) F{std::forward<Args>(args)...};
    }

protected:
    /// Allocate a continuation task with \p ref children. 
    /// 
    /// Continuation passing provides an alternative to the task-blocking style 
    /// of execution that results from recursively spawning children and 
    /// waiting for them to complete (e.g. the common fork-join pattern). 
    /// Continuation passing reduces overhead and mitigates the latency 
    /// introduced by work stealing by re-parenting a continuation task under 
    /// children that are about to be spawned. 
    /// 
    /// Consider continuation passing if reducing the growth of the stack 
    /// is desirable at the cost of growing the heap. 
    template <typename C, typename... Args>
    C * _AllocateContinuation(int ref, Args&&... args) {
        C* continuation = new (tbb::task::allocate_continuation()) 
            C{std::forward<Args>(args)...};
        continuation->set_ref_count(ref);
        return continuation;
    }

    /// Recycle this as a continuation task. 
    ///
    /// \note Note that the task graph performs safe continuation passing by 
    /// default, i.e. it assumes an extra increment of the child reference 
    /// count to handle the case when the continued task returns before a 
    /// longer-lived child task. In this case, the extra reference prevents 
    /// the continuation task from executing prematurely and orphaning its 
    /// running child task.   
    void _RecycleAsContinuation() {
        recycle_as_safe_continuation();
    }

    /// Recycle this as a child of continuation task \p c. 
    ///
    /// Reparent this task under \p c to mitigate the extra allocation cost 
    /// of the continuation task. 
    template <typename C>
    void _RecycleAsChildOf(C &c) {
        tbb::task::recycle_as_child_of(c);
    }
};
#endif

template <typename F, typename ... Args>
F * WorkTaskGraph::AllocateTask(Args&&... args) {
#if TBB_INTERFACE_VERSION_MAJOR >= 12
    return new F{std::forward<Args>(args)...};
#else
    return new (tbb::task::allocate_additional_child_of(*_rootTask))
        F{std::forward<Args>(args)...};
#endif
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif 

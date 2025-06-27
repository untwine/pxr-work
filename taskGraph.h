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
#include "pxr/base/work/dispatcher.h"
#include "pxr/base/work/taskGraph_defaultImpl.h"
#include "pxr/base/work/workTBB/impl.h"

#include <type_traits>
#include <utility>
#include <vector>
#include <tbb/enumerable_thread_specific.h>

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
private:
    // Select underlying implementation.
    template <typename, typename = void>
    struct _IsDefined
        : public std::false_type {};
    
    template <typename T>
    struct _IsDefined<T, std::void_t<decltype(sizeof(T))>> 
        : public std::true_type {};

    using _Impl = std::conditional_t<
        _IsDefined<class WorkImpl_TaskGraph>::value,
        WorkImpl_TaskGraph, WorkTaskGraph_DefaultImpl>;

public:
    WorkTaskGraph() = default;
    ~WorkTaskGraph() noexcept = default;

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
    F * AllocateTask(Args&&... args) {
        return _impl.AllocateTask<F>(std::forward<Args>(args)...);
    }

    /// Submit a concurrent task for execution.
    ///
    /// Transfers ownership of \p task to this task graph instance.
    template <typename F>
    inline void RunTask(F * task) {
        _impl.RunTask(task);
    }

    /// Submit concurrent tasks accumulated in thread-local lists for 
    /// execution.
    /// 
    /// Transfers ownership of all the tasks in the \p taskLists to this task
    /// graph instance.
    WORK_API void RunLists(const TaskLists &taskLists);

    /// Wait on all the running tasks to complete.
    inline void Wait() {
        _impl.Wait();
    }

private: 
    _Impl _impl;
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
class WorkTaskGraph::BaseTask : public WorkTaskGraph::_Impl::BaseTask {
private:
    using _Base = WorkTaskGraph::_Impl::BaseTask;

public:
    BaseTask() = default;
    WORK_API virtual ~BaseTask();

    /// Derived classes override this method to implement a parallel unit of 
    /// work.  
    virtual BaseTask * execute() = 0;

    /// Increment the reference count of child tasks that must complete before 
    /// this task can proceed. 
    void AddChildReference() {
        _Base::AddChildReference();
    }

    /// Decrement the reference count of child tasks that must complete before 
    /// this task can proceed. 
    int RemoveChildReference() {
        return _Base::RemoveChildReference();
    }
    
    /// Construct a new subtask and increment the ref count of the calling 
    /// task. 
    template <typename F, typename ... Args>
    F * AllocateChild(Args&&... args) {
        return _Base::AllocateChild<F>(std::forward<Args>(args)...);
    }

    /// Construct a new subtask of a continuation task and refrain from 
    /// incrementing the reference count of the calling task, since ownership 
    /// of this child has been transferred to the continuation task.   
    template <typename C, typename ... Args>
    C * AllocateContinuingChild(Args&&... args) {
        return _Base::AllocateContinuingChild<C>(std::forward<Args>(args)...);
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
        return _Base::_AllocateContinuation<C>(ref, std::forward<Args>(args)...);
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
        _Base::_RecycleAsContinuation();
    }

    /// Recycles this as a child of continuation task \p c. 
    ///
    /// Mitigates allocation/deallocation overhead by reparenting this under 
    /// \p c. 
    template <typename C>
    void _RecycleAsChildOf(C &c) {
        _Base::_RecycleAsChildOf(c);
    }
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif 

//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/base/work/taskGraph.h"

#include "pxr/base/work/loops.h"

PXR_NAMESPACE_OPEN_SCOPE

#if TBB_INTERFACE_VERSION_MAJOR >= 12
void
WorkTaskGraph::BaseTask::operator()() const {
    // Since oneTBB requires a const call operator, we must const_cast here. 
    WorkTaskGraph::BaseTask * const thisTask = 
        const_cast<WorkTaskGraph::BaseTask *>(this);

    // When running a task, it is initially not recycled. The execute() method
    // will recycle the task if needed.
    thisTask->_recycle = false;

    // Perform the work defined by the derived task class.
    WorkTaskGraph::BaseTask * const nextTask = thisTask->execute();

    // The task has been recycled.
    if (_recycle) {
        // If this task is recycled, we expect the reference count was increment
        // within the execute() method. Remove this extra reference now.
        // 
        // If the reference count reaches zero, there are no more child tasks
        // running, and we are responsible for re-executing the task here.
        if (thisTask->RemoveChildReference() == 0) {
            thisTask->operator()();
        }
    }
        
    // The task has not been recycled.
    else {
        // Retain a pointer to the parent task, if any, before deleting this
        // task.
        WorkTaskGraph::BaseTask * const parentTask = _parent;

        // If this task is completed, destroy it and reclaim its resources. 
        delete this;

        // If this is the last child of a recycled parent task, execute the
        // parent task. 
        if (parentTask && parentTask->RemoveChildReference() == 0 && 
                parentTask->_recycle) {
            parentTask->operator()();
        }
    }

    // Run the next task, if one has been provided.
    if (nextTask) {
        // Note that directly invoking the next task grows the stack and can be
        // mitigated by introducing a cutoff depth to truncate the recursion. 
        nextTask->operator()();
    }
}
#endif

WorkTaskGraph::WorkTaskGraph() 
#if TBB_INTERFACE_VERSION_MAJOR >= 12
    = default;
#else
    : _context(
        tbb::task_group_context::isolated,
        tbb::task_group_context::concurrent_wait | 
        tbb::task_group_context::default_traits)
    , _rootTask(new (tbb::task::allocate_root(_context)) tbb::empty_task()) 
{
    _rootTask->set_ref_count(1);
}
#endif 

WorkTaskGraph::~WorkTaskGraph() 
#if TBB_INTERFACE_VERSION_MAJOR >= 12
    = default;
#else
{
    _rootTask->wait_for_all();
    tbb::task::destroy(*_rootTask);
}
#endif 

WorkTaskGraph::BaseTask::~BaseTask() = default;

void
WorkTaskGraph::Wait() {
#if TBB_INTERFACE_VERSION_MAJOR >= 12
    _dispatcher.Wait();
#else 
    _rootTask->wait_for_all();
#endif
}

void
WorkTaskGraph::RunLists(const TaskLists &taskLists)
{
    WorkParallelForEach(
        taskLists.begin(), taskLists.end(),
        [this] (const TaskList &taskList) {
            for (const auto task : taskList) {
                RunTask(task);
            }
        });
}

PXR_NAMESPACE_CLOSE_SCOPE
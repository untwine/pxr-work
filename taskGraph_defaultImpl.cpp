//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/base/work/taskGraph_defaultImpl.h"

PXR_NAMESPACE_OPEN_SCOPE

WorkTaskGraph_DefaultImpl::BaseTask::~BaseTask() = default;

void
WorkTaskGraph_DefaultImpl::BaseTask::operator()() const {
    // Since oneTBB requires a const call operator, we must const_cast here. 
    WorkTaskGraph_DefaultImpl::BaseTask * const thisTask = 
        const_cast<WorkTaskGraph_DefaultImpl::BaseTask *>(this);

    // When running a task, it is initially not recycled. The execute() method
    // will recycle the task if needed.
    thisTask->_recycle = false;

    // Perform the work defined by the derived task class.
    WorkTaskGraph_DefaultImpl::BaseTask * const nextTask = thisTask->execute();

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
        WorkTaskGraph_DefaultImpl::BaseTask * const parentTask = _parent;

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

void
WorkTaskGraph_DefaultImpl::Wait() {
    _dispatcher.Wait();
}

PXR_NAMESPACE_CLOSE_SCOPE

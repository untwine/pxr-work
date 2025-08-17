//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#include <pxr/work/pxr.h>
#include <pxr/work/threadLimits.h>

#include <pxr/boost/python/def.hpp>

WORK_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

void wrapThreadLimits()
{
    def("GetConcurrencyLimit", &WorkGetConcurrencyLimit);
    def("HasConcurrency", &WorkHasConcurrency);
    def("GetPhysicalConcurrencyLimit", &WorkGetPhysicalConcurrencyLimit);

    def("SetConcurrencyLimit", &WorkSetConcurrencyLimit);
    def("SetConcurrencyLimitArgument", &WorkSetConcurrencyLimitArgument);
    def("SetMaximumConcurrencyLimit", &WorkSetMaximumConcurrencyLimit);
}

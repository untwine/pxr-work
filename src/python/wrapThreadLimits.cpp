// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
// Modified by Jeremy Retailleau.

#include <pxr/work/threadLimits.h>

#include <pxr/boost/python/def.hpp>

using namespace pxr;

using namespace pxr::boost::python;

void wrapThreadLimits()
{
    def("GetConcurrencyLimit", &WorkGetConcurrencyLimit);
    def("HasConcurrency", &WorkHasConcurrency);
    def("GetPhysicalConcurrencyLimit", &WorkGetPhysicalConcurrencyLimit);

    def("SetConcurrencyLimit", &WorkSetConcurrencyLimit);
    def("SetConcurrencyLimitArgument", &WorkSetConcurrencyLimitArgument);
    def("SetMaximumConcurrencyLimit", &WorkSetMaximumConcurrencyLimit);
}

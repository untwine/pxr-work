// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
// Modified by Jeremy Retailleau.

#include <pxr/work/loops.h>
#include <pxr/work/threadLimits.h>
#include <pxr/tf/diagnostic.h>
#include <pxr/tf/getenv.h>
#include <pxr/tf/staticData.h>

#include <algorithm>
#include <functional>

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>

#if TBB_INTERFACE_VERSION_MAJOR >= 12
#include <tbb/global_control.h>
#endif

using namespace std::placeholders;

using namespace pxr;

static TfStaticData< std::set<std::thread::id> > _uniqueThreads;
static TfStaticData< std::mutex > _uniqueThreadsMutex;

static void
_CountThreads(size_t begin, size_t end)
{
    // Do something to take up some time
    for (size_t i = begin; i < end; ++i) {
        srand(rand() * rand() * rand() * rand());
    }
    std::lock_guard<std::mutex> lock(*_uniqueThreadsMutex);
    _uniqueThreads->insert(std::this_thread::get_id());
}

static unsigned
_GetConcurrencyLimit()
{
#if TBB_INTERFACE_VERSION_MAJOR >= 12
    // For oneTBB, get limit in an arena with max concurrency as
    // WorkSetConcurrencyLimit by itself no longer increases the concurrency
    // beyond the number of cores by itself.
    unsigned limit;
    tbb::task_arena arena(tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism));
    arena.execute([&]() {
        limit = WorkGetConcurrencyLimit();
    });
    return limit;
#else
    return WorkGetConcurrencyLimit();
#endif
}

static size_t
_ExpectedLimit(const int envVal, const size_t n)
{
    // If envVal is non-zero, it wins over n!
    // envVal may also be a negative number, which means all but that many
    // cores.
    const size_t val = envVal ? 
        (envVal < 0 ?
            std::max<int>(1, envVal+WorkGetPhysicalConcurrencyLimit()) : envVal)
        : n;

#if TBB_INTERFACE_VERSION_MAJOR >= 12
    // oneTBB has an internal limit of 256 + 1 threads.
    return std::min<size_t>(val, 257);
#else
    return val;
#endif
}

static void
_TestThreadLimit(const int envVal, const size_t n)
{
    const size_t expectedN = _ExpectedLimit(envVal, n);
    if (expectedN != n) {
        std::cout << "   env setting overrides n = " << n << "\n";
    }

    const size_t numSamples = 1000000;
    std::cout << "   expecting maximum " << expectedN << " threads\n";

    _uniqueThreads->clear();

    WorkParallelForN(numSamples, std::bind(&_CountThreads, _1, _2));

    std::cout << "   TBB used " << _uniqueThreads->size() << '\n';

    if (_uniqueThreads->size() > expectedN) {
        TF_FATAL_ERROR("TBB expected less than or equal to %zu threads, got %zu",
                       expectedN, _uniqueThreads->size());
    }

}

static void
_TestArguments(const int envVal)
{
    // Note that if envVal is set (i.e. non-zero) it will always win over the
    // value supplied through the API calls.

    // Set to maximum concurrency, which should remain within envVal.
    const int numCores = WorkGetPhysicalConcurrencyLimit();
    WorkSetConcurrencyLimitArgument(numCores);
    TF_AXIOM(_GetConcurrencyLimit() == _ExpectedLimit(envVal, numCores));

    // n = 0, means "no change"
    WorkSetConcurrencyLimitArgument(0);
    TF_AXIOM(_GetConcurrencyLimit() == _ExpectedLimit(envVal, numCores));

    // n = 1 means no threading
    WorkSetConcurrencyLimitArgument(1);
    TF_AXIOM(_GetConcurrencyLimit() == _ExpectedLimit(envVal, 1));

    // n = 3 means 3
    WorkSetConcurrencyLimitArgument(3);
    TF_AXIOM(_GetConcurrencyLimit() == _ExpectedLimit(envVal, 3));

    // n = 1000 means 1000
    WorkSetConcurrencyLimitArgument(1000);
    TF_AXIOM(_GetConcurrencyLimit() == _ExpectedLimit(envVal, 1000));

    // n = -1 means numCores - 1, with a minimum of 1
    WorkSetConcurrencyLimitArgument(-1);
    TF_AXIOM(_GetConcurrencyLimit() == 
             _ExpectedLimit(envVal, std::max(1, numCores-1)));

    // n = -3 means numCores - 3, with a minimum of 1
    WorkSetConcurrencyLimitArgument(-3);
    TF_AXIOM(_GetConcurrencyLimit() == 
             _ExpectedLimit(envVal, std::max(1, numCores-3)));

    // n = -numCores means 1 (no threading)
    WorkSetConcurrencyLimitArgument(-numCores);
    TF_AXIOM(_GetConcurrencyLimit() == _ExpectedLimit(envVal, 1));

    // n = -numCores*10 means 1 (no threading)
    WorkSetConcurrencyLimitArgument(-numCores*10);
    TF_AXIOM(_GetConcurrencyLimit() == _ExpectedLimit(envVal, 1));
}

struct _RawTBBCounter
{
    void operator()(const tbb::blocked_range<size_t> &r) const {
        _CountThreads(r.begin(), r.end());
    }
};

int
main(int argc, char **argv)
{
    // Read the env setting used to limit threading
    const int envVal = TfGetenvInt("PXR_WORK_THREAD_LIMIT", 0);
    std::cout << "PXR_WORK_THREAD_LIMIT = " << envVal << '\n';

    // Test to make sure that a call to tbb that happens before any of the
    // libWork API is touched is unrestricted.  We need to do this for now to
    // make sure that we don't break existing tbb code just by having libWork
    // linked in.
    //
    // Note that if we test this, we can't run the rest of the test because once
    // tbb is initialized by default (by just using its API) then there doesn't
    // seem to be a way to limit it again.  That's why we test this
    // functionality by itself.
    if ((argc == 2) && (strcmp(argv[1], "--rawtbb") == 0)) {
        std::cout << "Testing that libWork automatically limits tbb "
            "threading when PXR_WORK_THREAD_LIMIT is set...\n";
        _uniqueThreads->clear();
        tbb::parallel_for(
            tbb::blocked_range<size_t>(0, 100000), _RawTBBCounter(),
            tbb::simple_partitioner());
        std::cout << "   default TBB used " << _uniqueThreads->size() 
                  << " threads\n";
        
        if (envVal == 0) {
            if (_uniqueThreads->size() < WorkGetPhysicalConcurrencyLimit()) {
                TF_FATAL_ERROR("tbb only used %zu threads when it should be "
                               "unlimited (expected >= %d threads)\n", 
                               _uniqueThreads->size(),
                               WorkGetPhysicalConcurrencyLimit());
            }
        }
        else if (_uniqueThreads->size() > WorkGetConcurrencyLimit()) {
            TF_FATAL_ERROR("tbb used %zu threads, which is greater than the "
                           "concurrency limit %d (PXR_WORK_THREAD_LIMIT=%d).", 
                           _uniqueThreads->size(), WorkGetConcurrencyLimit(), 
                           envVal);
        }

        // Stop the test, now that we've initialized tbb, there's no going
        // back.
        return 0;
    }

    // 0 means all cores.
    if (envVal == 0) {
        WorkSetMaximumConcurrencyLimit();
    }
    const size_t limit = WorkGetConcurrencyLimit();

    // Make sure that we get the default thread limit
    std::cout << "Testing that the thread limit defaults to "
        "PXR_WORK_THREAD_LIMIT by default...\n";
    _TestThreadLimit(envVal, limit);

    // Now that we've invoked libWork, make sure that raw TBB API also defaults
    // to PXR_WORK_THREAD_LIMIT.
    std::cout << "Testing that raw tbb code is now also unlimited "
        "after first invocation of libWork API...\n";

    _uniqueThreads->clear();
    tbb::parallel_for(tbb::blocked_range<size_t>(0, 100000), _RawTBBCounter());
    std::cout << "   raw tbb used " << _uniqueThreads->size() << " threads\n";
    if (_uniqueThreads->size() > limit) {
        TF_FATAL_ERROR("it appears as though libWork hasn't been initialized "
                       "with PXR_WORK_THREAD_LIMIT.");
    }

    // Test with full concurrency.
    std::cout << "Testing full concurrency...\n";
    WorkSetMaximumConcurrencyLimit();
    TF_AXIOM(_GetConcurrencyLimit() ==
        _ExpectedLimit(envVal, WorkGetPhysicalConcurrencyLimit()));
    _TestThreadLimit(envVal, WorkGetPhysicalConcurrencyLimit());

    // Test with no concurrency.
    std::cout << "Testing turning off concurrency...\n";
    WorkSetConcurrencyLimit(1);
    TF_AXIOM(_GetConcurrencyLimit() ==
        _ExpectedLimit(envVal, 1));
    _TestThreadLimit(envVal, 1);

    // Test with 2 threads.
    std::cout << "Testing with 2 threads...\n";
    WorkSetConcurrencyLimit(2);
    TF_AXIOM(_GetConcurrencyLimit() ==
        _ExpectedLimit(envVal, 2));
    _TestThreadLimit(envVal, 2);

    // Test with 4 threads.
    std::cout << "Testing with 4 threads...\n";
    WorkSetConcurrencyLimit(4);
    TF_AXIOM(_GetConcurrencyLimit() ==
        _ExpectedLimit(envVal, 4));
    _TestThreadLimit(envVal, 4);

    // Test with 1000 threads.
    std::cout << "Testing with 1000 threads...\n";
    WorkSetConcurrencyLimit(1000);
    TF_AXIOM(_GetConcurrencyLimit() ==
        _ExpectedLimit(envVal, 1000));
    _TestThreadLimit(envVal, 1000);

    // Test argument parsing
    std::cout << "Testing argument parsing...\n";
    _TestArguments(envVal);
    return 0;
}

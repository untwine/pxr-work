// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
// Modified by Jeremy Retailleau.

#include <pxr/work/loops.h>

#include <pxr/work/threadLimits.h>

#include <pxr/tf/stopwatch.h>
#include <pxr/tf/iterator.h>
#include <pxr/tf/staticData.h>
#include <pxr/arch/fileSystem.h>

#include <functional>

#include <cstdio>
#include <cstring>
#include <numeric>
#include <iostream>
#include <vector>

using namespace std::placeholders;

using namespace pxr;

static void
_Double(size_t begin, size_t end, std::vector<int> *v)
{
    for (size_t i = begin; i < end; ++i)
        (*v)[i] *= 2;
}

static void
_DoubleAll(std::vector<int> &v)
{
    for (int &i : v) {
        i *= 2;
    }
}

static void
_VerifyDoubled(const std::vector<int> &v)
{
    for (size_t i = 0; i < v.size(); ++i) {
        if (static_cast<size_t>(v[i]) != (2*i)) {
            std::cout << "found error at index " << i << " is " 
                      << v[i] << std::endl;
            TF_AXIOM(static_cast<size_t>(v[i]) == (2*i));
        }
    }
}

static void
_PopulateVector(size_t arraySize, std::vector<int> *v)
{
    v->resize(arraySize);
    std::iota(v->begin(), v->end(), 0);
}

// Returns the number of seconds it took to complete this operation.
double
_DoTBBTest(bool verify, const size_t arraySize, const size_t numIterations)
{
    std::vector<int> v;
    _PopulateVector(arraySize, &v);

    TfStopwatch sw;
    sw.Start();
    for (size_t i = 0; i < numIterations; i++) {

        WorkParallelForN(arraySize, std::bind(&_Double, _1, _2, &v));       

    }

    if (verify) {
        TF_AXIOM(numIterations == 1);
        _VerifyDoubled(v);
    }

    sw.Stop();
    return sw.GetSeconds();
}


// Returns the number of seconds it took to complete this operation.
double
_DoTBBTestForEach(
    bool verify, const size_t arraySize, const size_t numIterations)
{
    static const size_t partitionSize = 20;
    std::vector< std::vector<int> > vs(partitionSize);
    for (std::vector<int> &v : vs) {
        _PopulateVector(arraySize / partitionSize, &v);
    }

    TfStopwatch sw;
    sw.Start();
    for (size_t i = 0; i < numIterations; i++) {

        WorkParallelForEach(vs.begin(), vs.end(), _DoubleAll);

    }

    if (verify) {
        TF_AXIOM(numIterations == 1);
        for (const auto& v : vs) {
            _VerifyDoubled(v);
        }
    }

    sw.Stop();
    return sw.GetSeconds();
}

void
_DoSerialTest()
{
    const size_t N = 200;
    std::vector<int> v;
    _PopulateVector(N, &v);
    WorkSerialForN(N, std::bind(&_Double, _1, _2, &v));
    _VerifyDoubled(v);
}

// Make sure that the API for WorkParallelForN and WorkSerialForN can be
// interchanged.  
void
_DoSignatureTest()
{
    struct F
    {
        // Test that this can be non-const
        void operator()(size_t start, size_t end) {
        }
    };

    F f;

    WorkParallelForN(100, f);
    WorkSerialForN(100, f);

    WorkParallelForN(100, F());
    WorkSerialForN(100, F());
}


int
main(int argc, char **argv)
{
    const bool perfMode = ((argc > 1) && !strcmp(argv[1], "--perf")); 
    const size_t arraySize = 1000000;
    const size_t numIterations = perfMode ? 1000 : 1;

    WorkSetMaximumConcurrencyLimit();

    std::cout << "Initialized with " << 
        WorkGetPhysicalConcurrencyLimit() << " cores..." << std::endl;


    double tbbSeconds = _DoTBBTest(!perfMode, arraySize, numIterations);

    std::cout << "TBB parallel_for took: " << tbbSeconds << " seconds" 
        << std::endl;


    double tbbForEachSeconds = _DoTBBTestForEach(
        !perfMode, arraySize, numIterations);

    std::cout << "TBB parallel_for_each took: " << tbbForEachSeconds
        << " seconds" << std::endl;


    _DoSerialTest();

    _DoSignatureTest();

    if (perfMode) {

        // XXX:perfgen only accepts metric names ending in _time.  See bug 97317
        FILE *outputFile = ArchOpenFile("perfstats.raw", "w");
        fprintf(outputFile,
            "{'profile':'TBB Loops_time','metric':'time','value':%f,'samples':1}\n",
            tbbSeconds);
        fprintf(outputFile,
            "{'profile':'TBB for_each Loops_time','metric':'time','value':%f,'samples':1}\n",
            tbbForEachSeconds);
        fclose(outputFile);

    }

    return 0;
}

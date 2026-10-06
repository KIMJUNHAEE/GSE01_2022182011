#include "stdafx.h"
#include "RenderStats.h"
#include "Dependencies/freeglut.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
#include <algorithm>

namespace RenderStats
{
namespace
{
    struct CategoryStat
    {
        long long drawCalls = 0;
        long long instances = 0;
    };

    int drawCallsThisFrame = 0;

    // Accumulated over a ~1 second window so the console gets one readable
    // report instead of one per frame.
    int windowStartMs = -1;
    int windowFrameCount = 0;
    long long windowDrawCallSum = 0;
    long long windowInstanceSum = 0;
    long long windowReasonCount[5] = {};
    std::map<std::string, CategoryStat> windowCategoryStats;

    // Formats into its own local stream so it can never leak std::fixed/
    // setprecision/etc. state into whatever std::cout prints next - that
    // leakage is exactly what turned "FPS: 60.1" into "FPS: 6e+01" before.
    std::string FormatFixed(double value, int precision)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        return out.str();
    }

    const char* ReasonLabel(FlushReason reason)
    {
        switch (reason)
        {
        case FlushReason::MeshKindChanged:
            return "MeshKindChanged";
        case FlushReason::TextureChanged:
            return "TextureChanged";
        case FlushReason::CapacityReached:
            return "CapacityReached";
        case FlushReason::EndOfFrame:
            return "EndOfFrame";
        default:
            return "Other";
        }
    }
} // namespace

void Reset()
{
    drawCallsThisFrame = 0;
}

void DrawCall(const std::string& category, int instanceCount, FlushReason reason)
{
    ++drawCallsThisFrame;

    windowInstanceSum += instanceCount;
    windowReasonCount[(int)reason] += 1;

    CategoryStat& stat = windowCategoryStats[category];
    stat.drawCalls += 1;
    stat.instances += instanceCount;
}

int DrawCallCount()
{
    return drawCallsThisFrame;
}

void LogFrame()
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (windowStartMs < 0)
    {
        windowStartMs = now;
    }

    ++windowFrameCount;
    windowDrawCallSum += drawCallsThisFrame;

    int elapsed = now - windowStartMs;
    if (elapsed < 1000)
    {
        return;
    }

    float fps = windowFrameCount * 1000.f / elapsed;
    float avgDrawCalls =
        windowFrameCount > 0 ? (float)windowDrawCallSum / windowFrameCount : 0.f;
    float avgInstancesPerDraw =
        windowDrawCallSum > 0 ? (float)windowInstanceSum / windowDrawCallSum : 0.f;

    std::cout << "[Perf] FPS: " << FormatFixed(fps, 1)
              << "  DrawCalls/frame(avg): " << FormatFixed(avgDrawCalls, 1)
              << "  DrawCalls(last frame): " << drawCallsThisFrame
              << "  AvgInstances/DrawCall: " << FormatFixed(avgInstancesPerDraw, 1) << std::endl;

    long long reasonTotal = 0;
    for (long long count : windowReasonCount)
    {
        reasonTotal += count;
    }
    if (reasonTotal > 0)
    {
        std::cout << "[Perf]   Flush reasons (why a batch could not be merged with the previous "
                     "draw):";
        for (int i = 0; i < 5; ++i)
        {
            if (windowReasonCount[i] <= 0)
            {
                continue;
            }
            float pct = 100.f * windowReasonCount[i] / reasonTotal;
            std::cout << "  " << ReasonLabel((FlushReason)i) << "=" << windowReasonCount[i] << " ("
                      << FormatFixed(pct, 0) << "%)";
        }
        std::cout << std::endl;
    }

    if (!windowCategoryStats.empty())
    {
        std::vector<std::pair<std::string, CategoryStat>> ranked(windowCategoryStats.begin(),
                                                                  windowCategoryStats.end());
        std::sort(ranked.begin(), ranked.end(),
                 [](const auto& a, const auto& b)
                 {
                     return a.second.drawCalls > b.second.drawCalls;
                 });
        std::cout << "[Perf]   Top batch keys by draw-call count (low instances/draw here = "
                     "shapes keep interrupting each other instead of merging):";
        for (size_t i = 0; i < ranked.size() && i < 6; ++i)
        {
            const auto& [category, stat] = ranked[i];
            float perDraw = stat.drawCalls > 0 ? (float)stat.instances / stat.drawCalls : 0.f;
            std::cout << "  " << category << "[calls=" << stat.drawCalls
                      << " inst=" << stat.instances
                      << " inst/call=" << FormatFixed(perDraw, 1) << "]";
        }
        std::cout << std::endl;
    }

    windowFrameCount = 0;
    windowDrawCallSum = 0;
    windowInstanceSum = 0;
    for (long long& count : windowReasonCount)
    {
        count = 0;
    }
    windowCategoryStats.clear();
    windowStartMs = now;
}

} // namespace RenderStats

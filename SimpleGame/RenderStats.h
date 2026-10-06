#pragma once
#include <string>

// Lightweight, engine-wide per-frame rendering counters for profiling.
//
// Any code issuing a glDraw*/glDrawArraysInstanced call should call DrawCall()
// once per call: `category` is a short human-readable batch key (e.g.
// "Quad/white", "Disc/white", "Texture/<id>") and `reason` says why THIS
// particular draw was cut off from the previous one, so the log can tell
// "many small batches because shapes keep alternating type" apart from
// "one huge batch, correctly merged, that just happened to hit the size cap".
//
// Game::Render() resets the counters at the start of each frame (Reset) and
// reports them, throttled to about once a second, at the end (LogFrame) so
// the console stays readable during play instead of scrolling at 60 lines/s.
namespace RenderStats
{
    enum class FlushReason
    {
        MeshKindChanged,  // this draw's shape (Quad/Disc/Triangle/...) differs from the previous one
        TextureChanged,   // same shape, but a different texture/atlas
        CapacityReached,  // same shape+texture, just hit the instance buffer's size cap
        EndOfFrame,       // explicit flush at the end of a frame/pass, not a batch break
        Other
    };

    void Reset();
    void DrawCall(const std::string& category, int instanceCount, FlushReason reason);
    int DrawCallCount();
    void LogFrame();
} // namespace RenderStats

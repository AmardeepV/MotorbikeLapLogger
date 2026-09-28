#include "CaptureBuffer.h"
#include <cassert>
#include <initializer_list>

int main()
{
    DiagnosticCapture::Buffer<3> buffer;
    DiagnosticCapture::Sample result{};
    assert(!buffer.pop(result));
    // Mimic a FIFO burst: identical readings are distinct sensor events.
    for (uint32_t sequence = 1; sequence <= 3; ++sequence)
        assert(buffer.push({sequence, 100, 0, 0, 8192, 4, 0, 0}));
    assert(!buffer.push({4, 100, 0, 0, 8192, 4, 0, 0}));
    assert(buffer.dropped() == 1);
    assert(buffer.pop(result) && result.sequence == 1);
    assert(buffer.push({5, 200, 1, 2, 3, 37, -16384, 65535})); // wrap around
    for (uint32_t sequence : {2u, 3u, 5u})
        assert(buffer.pop(result) && result.sequence == sequence);
    assert(result.sensorId == 37 && result.z == 3);
    assert(result.w == -16384 && result.accuracy == 65535);
    assert(buffer.size() == 0 && !buffer.pop(result));
}

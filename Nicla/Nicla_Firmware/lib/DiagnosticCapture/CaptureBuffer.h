#pragma once
#include <stddef.h>
#include <stdint.h>

namespace DiagnosticCapture
{
struct Sample
{
    uint32_t sequence;
    uint32_t hostUs; // Elapsed delivery time, NOT BHI260AP acquisition time.
    int16_t x, y, z;
    uint8_t sensorId;
    int16_t w; // Quaternion only; raw signed Q14. Zero for vector sensors.
    uint16_t accuracy; // Quaternion payload field; not confidence for Game RV.
};

// Main thread only: BHY2.update() invokes sensor callbacks synchronously.
// Preserve old evidence and count losses when serial cannot keep up.
template <size_t Capacity>
class Buffer
{
public:
    static_assert(Capacity > 0, "Capture buffer must have storage");
    bool push(const Sample& sample)
    {
        if (_size == Capacity) { ++_dropped; return false; }
        _samples[(_head + _size) % Capacity] = sample;
        ++_size;
        return true;
    }
    bool pop(Sample& sample)
    {
        if (_size == 0) return false;
        sample = _samples[_head];
        _head = (_head + 1) % Capacity;
        --_size;
        return true;
    }
    size_t size() const { return _size; }
    uint32_t dropped() const { return _dropped; }
private:
    Sample _samples[Capacity]{};
    size_t _head = 0, _size = 0;
    uint32_t _dropped = 0;
};
}

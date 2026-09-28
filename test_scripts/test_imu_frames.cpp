#include "ImuFrames.h"
#include <cassert>

using ImuFrames::Vector3;

bool equal(Vector3 a, Vector3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

Vector3 cross(Vector3 a, Vector3 b)
{
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

int main()
{
    using namespace ImuFrames;
    // Idealized observations from the six-face experiment, not arbitrary
    // assumptions that printed board labels equal library channel names.
    const Vector3 reported[] = {{0,1,0}, {0,-1,0}, {-1,0,0}, {1,0,0}, {0,0,1}, {0,0,-1}};
    const Vector3 board[] = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
    const Vector3 bike[] = {{0,-1,0}, {0,1,0}, {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1}};
    for (unsigned i = 0; i < 6; ++i)
    {
        assert(equal(reportedToMarkedBoard(reported[i]), board[i]));
        assert(equal(reportedToMotorcycle(reported[i]), bike[i]));
    }
    // A proper rotation preserves handedness and length: acceleration and
    // angular velocity can use the same transform, without a reflection.
    const Vector3 x = reportedToMotorcycle({1,0,0});
    const Vector3 y = reportedToMotorcycle({0,1,0});
    const Vector3 z = reportedToMotorcycle({0,0,1});
    assert(equal(cross(x,y),z));
    const Vector3 v = reportedToMotorcycle({3,-4,12});
    assert(v.x*v.x + v.y*v.y + v.z*v.z == 169);
    // Unit conversion and offset correction must not sneak into a frame map.
    assert(equal(reportedToMotorcycle({0,0,8192}), {0,0,8192}));
    assert(equal(reportedToMotorcycle({-32768,32767,0}), {32768,-32767,0}));
    // Upright bike: positive body-X angular velocity is a rightward roll.
    assert(equal(reportedToMotorcycle({-30,0,0}), {30,0,0}));
}

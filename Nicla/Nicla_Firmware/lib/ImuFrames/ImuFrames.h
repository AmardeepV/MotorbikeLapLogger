#pragma once

// Vector coordinates only: do not apply these operations to quaternion XYZ.
// Preserve the input units. Use floating point after reading signed IMU counts
// so negating the minimum int16_t value cannot overflow.
namespace ImuFrames
{
struct Vector3
{
    float x;
    float y;
    float z;
};

// Supported six-face captures and controlled gyro turns, September 2026.
constexpr Vector3 reportedToMarkedBoard(Vector3 reported)
{
    return {reported.y, -reported.x, reported.z};
}

// User-confirmed installation: marked +Y forward, +X right, +Z up.
// Define a right-handed motorcycle frame: X forward, Y left, Z up.
constexpr Vector3 markedBoardToMotorcycle(Vector3 board)
{
    return {board.y, -board.x, board.z};
}

// Combined mapping is (-reported.x, -reported.y, reported.z).
// Applies to acceleration and angular velocity. Positive rotation about bike X
// rolls the top toward the rider's right (matching positive lean in the app).
// Body angular velocity components are not generally Euler angle derivatives.
constexpr Vector3 reportedToMotorcycle(Vector3 reported)
{
    return markedBoardToMotorcycle(reportedToMarkedBoard(reported));
}
}

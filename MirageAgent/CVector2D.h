#pragma once

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <cmath>
using namespace std;

#define FLOAT_EPSILON 0.0001f
#pragma warning (disable : 4244)

class CVector2D
{
public:
    float fX, fY;

    // Default constructor
    CVector2D()
    {
        fX = 0.0f;
        fY = 0.0f;
    }

    // Parameterized constructor
    CVector2D(float _fX, float _fY)
    {
        fX = _fX;
        fY = _fY;
    }

    // Copy constructor
    CVector2D(const CVector2D& vec)
    {
        fX = vec.fX;
        fY = vec.fY;
    }

    // Assignment operator
    CVector2D& operator=(const CVector2D& vec)
    {
        fX = vec.fX;
        fY = vec.fY;
        return *this;
    }

    // Length (magnitude)
    float Length() const
    {
        return sqrt((fX * fX) + (fY * fY));
    }

    // Squared length
    float LengthSquared() const
    {
        return (fX * fX) + (fY * fY);
    }

    // Normalize in-place and return original length
    float Normalize()
    {
        float len = Length();
        if (len > FLOAT_EPSILON)
        {
            float inv = 1.0f / len;
            fX *= inv;
            fY *= inv;
        }
        else
        {
            len = 0.0f;
        }
        return len;
    }

    // Dot product
    float DotProduct(const CVector2D& other) const
    {
        return fX * other.fX + fY * other.fY;
    }

    // Operators: vector-scalar
    CVector2D operator*(float s) const
    {
        return CVector2D(fX * s, fY * s);
    }

    CVector2D operator/(float s) const
    {
        float inv = 1.0f / s;
        return CVector2D(fX * inv, fY * inv);
    }

    // Operators: vector-vector
    CVector2D operator+(const CVector2D& v) const
    {
        return CVector2D(fX + v.fX, fY + v.fY);
    }

    CVector2D operator-(const CVector2D& v) const
    {
        return CVector2D(fX - v.fX, fY - v.fY);
    }

    CVector2D operator*(const CVector2D& v) const
    {
        return CVector2D(fX * v.fX, fY * v.fY);
    }

    CVector2D operator/(const CVector2D& v) const
    {
        return CVector2D(fX / v.fX, fY / v.fY);
    }

    // Unary minus
    CVector2D operator-() const
    {
        return CVector2D(-fX, -fY);
    }

    // Compound assignment: scalar
    void operator+=(float s)
    {
        fX += s;
        fY += s;
    }

    void operator-=(float s)
    {
        fX -= s;
        fY -= s;
    }

    void operator*=(float s)
    {
        fX *= s;
        fY *= s;
    }

    void operator/=(float s)
    {
        float inv = 1.0f / s;
        fX *= inv;
        fY *= inv;
    }

    // Compound assignment: vector
    void operator+=(const CVector2D& v)
    {
        fX += v.fX;
        fY += v.fY;
    }

    void operator-=(const CVector2D& v)
    {
        fX -= v.fX;
        fY -= v.fY;
    }

    void operator*=(const CVector2D& v)
    {
        fX *= v.fX;
        fY *= v.fY;
    }

    void operator/=(const CVector2D& v)
    {
        fX /= v.fX;
        fY /= v.fY;
    }

    // Equality checks with epsilon
    bool operator==(const CVector2D& v) const
    {
        return (fabs(fX - v.fX) < FLOAT_EPSILON) && (fabs(fY - v.fY) < FLOAT_EPSILON);
    }

    bool operator!=(const CVector2D& v) const
    {
        return (fabs(fX - v.fX) >= FLOAT_EPSILON) || (fabs(fY - v.fY) >= FLOAT_EPSILON);
    }

    // Convert direction vector to angle (in radians)
    float ToAngle() const
    {
        // atan2 returns [-pi,pi]
        return atan2(fY, fX);
    }

    // Rotate vector by angle (radians)
    CVector2D Rotate(float angle) const
    {
        float c = cos(angle);
        float s = sin(angle);
        return CVector2D(fX * c - fY * s, fX * s + fY * c);
    }
};

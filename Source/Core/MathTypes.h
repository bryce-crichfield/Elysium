#pragma once

namespace Elysium {

    struct Vector2 {
        float x, y;

        Vector2();
        Vector2(float x, float y);

        Vector2 operator+(const Vector2& rhs) const;
        Vector2 operator-(const Vector2& rhs) const;
        Vector2 operator*(float scalar) const;
        Vector2 operator/(float scalar) const;

        Vector2& operator+=(const Vector2& rhs);
        Vector2& operator-=(const Vector2& rhs);

        bool operator==(const Vector2& rhs) const;
        bool operator!=(const Vector2& rhs) const;

        float Length() const;
        Vector2 Normalized() const;
    };

    float Dot(const Vector2& a, const Vector2& b);


    struct Vector3 {
        float x, y, z;

        Vector3();
        Vector3(float x, float y, float z);

        Vector3 operator+(const Vector3& rhs) const;
        Vector3 operator-(const Vector3& rhs) const;
        Vector3 operator*(float scalar) const;
        Vector3 operator/(float scalar) const;

        Vector3& operator+=(const Vector3& rhs);
        Vector3& operator-=(const Vector3& rhs);

        bool operator==(const Vector3& rhs) const;
        bool operator!=(const Vector3& rhs) const;

        float Length() const;
        Vector3 Normalized() const;
    };

    float Dot(const Vector3& a, const Vector3& b);
    Vector3 Cross(const Vector3& a, const Vector3& b);


    struct Matrix {
        float data[16];

        Matrix();

        Matrix operator*(const Matrix& rhs) const;

        bool operator==(const Matrix& rhs) const;
        bool operator!=(const Matrix& rhs) const;

        Matrix Transposed() const;

        static Matrix Identity();
        static Matrix Translation(const Vector3& translation);
        static Matrix Scale(const Vector3& scale);
    };


    struct Rectangle {
        float x, y, width, height;

        Rectangle();
        Rectangle(float x, float y, float width, float height);

        bool operator==(const Rectangle& rhs) const;
        bool operator!=(const Rectangle& rhs) const;

        bool Contains(const Vector2& point) const;
        bool Intersects(const Rectangle& other) const;
    };

}
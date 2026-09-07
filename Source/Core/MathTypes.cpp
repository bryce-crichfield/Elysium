#include "Core/MathTypes.h"

#include <cmath>

namespace Elysium {


    Vector2::Vector2() : x(0.0f), y(0.0f) {}

    Vector2::Vector2(float x, float y) : x(x), y(y) {}

    Vector2 Vector2::operator+(const Vector2& rhs) const {
        return Vector2(x + rhs.x, y + rhs.y);
    }

    Vector2 Vector2::operator-(const Vector2& rhs) const {
        return Vector2(x - rhs.x, y - rhs.y);
    }

    Vector2 Vector2::operator*(float scalar) const {
        return Vector2(x * scalar, y * scalar);
    }

    Vector2 Vector2::operator/(float scalar) const {
        return Vector2(x / scalar, y / scalar);
    }

    Vector2& Vector2::operator+=(const Vector2& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    Vector2& Vector2::operator-=(const Vector2& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    bool Vector2::operator==(const Vector2& rhs) const {
        return x == rhs.x && y == rhs.y;
    }

    bool Vector2::operator!=(const Vector2& rhs) const {
        return !(*this == rhs);
    }

    float Vector2::Length() const {
        return std::sqrt(x * x + y * y);
    }

    Vector2 Vector2::Normalized() const {
        return *this / Length();
    }

    float Dot(const Vector2& a, const Vector2& b) {
        return a.x * b.x + a.y * b.y;
    }


    Vector3::Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

    Vector3::Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vector3 Vector3::operator+(const Vector3& rhs) const {
        return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
    }

    Vector3 Vector3::operator-(const Vector3& rhs) const {
        return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
    }

    Vector3 Vector3::operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    Vector3 Vector3::operator/(float scalar) const {
        return Vector3(x / scalar, y / scalar, z / scalar);
    }

    Vector3& Vector3::operator+=(const Vector3& rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    Vector3& Vector3::operator-=(const Vector3& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    bool Vector3::operator==(const Vector3& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    bool Vector3::operator!=(const Vector3& rhs) const {
        return !(*this == rhs);
    }

    float Vector3::Length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3 Vector3::Normalized() const {
        return *this / Length();
    }

    float Dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    Vector3 Cross(const Vector3& a, const Vector3& b) {
        return Vector3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }


    Matrix::Matrix() {
        for (int i = 0; i < 16; ++i) {
            data[i] = 0.0f;
        }
        data[0] = 1.0f;
        data[5] = 1.0f;
        data[10] = 1.0f;
        data[15] = 1.0f;
    }

    Matrix Matrix::operator*(const Matrix& rhs) const {
        Matrix result;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += data[row * 4 + k] * rhs.data[k * 4 + col];
                }
                result.data[row * 4 + col] = sum;
            }
        }
        return result;
    }

    bool Matrix::operator==(const Matrix& rhs) const {
        for (int i = 0; i < 16; ++i) {
            if (data[i] != rhs.data[i]) {
                return false;
            }
        }
        return true;
    }

    bool Matrix::operator!=(const Matrix& rhs) const {
        return !(*this == rhs);
    }

    Matrix Matrix::Transposed() const {
        Matrix result;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                result.data[col * 4 + row] = data[row * 4 + col];
            }
        }
        return result;
    }

    Matrix Matrix::Identity() {
        return Matrix();
    }

    Matrix Matrix::Translation(const Vector3& translation) {
        Matrix result = Matrix::Identity();
        result.data[12] = translation.x;
        result.data[13] = translation.y;
        result.data[14] = translation.z;
        return result;
    }

    Matrix Matrix::Scale(const Vector3& scale) {
        Matrix result = Matrix::Identity();
        result.data[0] = scale.x;
        result.data[5] = scale.y;
        result.data[10] = scale.z;
        return result;
    }


    Rectangle::Rectangle() : x(0.0f), y(0.0f), width(0.0f), height(0.0f) {}

    Rectangle::Rectangle(float x, float y, float width, float height)
        : x(x), y(y), width(width), height(height) {}

    bool Rectangle::operator==(const Rectangle& rhs) const {
        return x == rhs.x && y == rhs.y && width == rhs.width && height == rhs.height;
    }

    bool Rectangle::operator!=(const Rectangle& rhs) const {
        return !(*this == rhs);
    }

    bool Rectangle::Contains(const Vector2& point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }

    bool Rectangle::Intersects(const Rectangle& other) const {
        return !(other.x > x + width || other.x + other.width < x ||
                 other.y > y + height || other.y + other.height < y);
    }

}
#pragma once

namespace Elysium {
    
    struct Vector2 {
        float x, y;
    };

    struct Vector3 {
        float x, y, z;
    };

    struct Matrix {
        float data[16];
    };

    struct Rectangle {
        float x, y, width, height;
    };
}
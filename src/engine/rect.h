#pragma once

#include "core/types.h"
#include "math/vecs/vector2.h"
#include "math/vecs/vector4.h"

union Rect
{
    struct { f32 left, top, right, bottom; };
    struct { Vector2 top_left, bottom_right; };
    Vector4 v4;
};

inline Rect rect_from_v4(const Vector4& v4)
{
    Rect rect;
    rect.v4 = v4;
    return rect;
}
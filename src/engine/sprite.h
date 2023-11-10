#pragma once

#include "containers/darray.h"
#include "containers/string.h"
#include "graphics/texture.h"
#include "math/vecs/vector2.h"
#include "rect.h"

struct SpriteSheet
{
    struct SpriteData
    {
        Rect tex_coords;
        Vector2 size;
        Vector2 pivot;
    };

    Texture atlas = {};
    DynamicArray<SpriteData> sprites = {};
};

struct Sprite
{
    SpriteSheet sprite_sheet;
    s64 sprite_index;
};

struct Animation2D
{
    struct Instance
    {
        u32 current_frame_index;
        f32 start_time;
        u32 loop_count;
    };

    enum struct LoopType
    {
        NONE,
        CYCLE,
        PING_PONG,

        NUM_TYPES
    };

    String name;
    DynamicArray<Sprite> sprites;
    f32 frame_rate;
    LoopType loop_type;
};

void free(SpriteSheet& sprite_sheet);

void animation_instance_start(Animation2D::Instance& instance, f32 time);
void animation_instance_step(const Animation2D& animation, Animation2D::Instance& instance, f32 time);

void free(Animation2D& animation);
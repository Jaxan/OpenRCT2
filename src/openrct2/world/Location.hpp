#pragma region Copyright (c) 2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#pragma once

#include "../common.h"

#define LOCATION_NULL     ((sint16)(uint16)0x8000)
#define RCT_XY8_UNDEFINED 0xFFFF
#define MakeXY16(x, y)    {(sint16)(x), (sint16)(y)}

#pragma pack(push, 1)
struct LocationXY8 {
    union {
        struct {
            uint8 x, y;
        };
        uint16 xy;
    };
};
assert_struct_size(LocationXY8, 2);

struct LocationXY16 {
    sint16 x, y;
};
assert_struct_size(LocationXY16, 4);


struct LocationXYZ16 {
    sint16 x, y, z;
};
assert_struct_size(LocationXYZ16, 6);
#pragma pack(pop)

constexpr sint32 COORDS_NULL = -1;


/*
 * Tile coordinates use 1 x/y increment per tile.
 * Regular ('big', 'sprite') coordinates use 32 x/y increments per tile.
 * */
struct TileCoordsXY
{
    sint32 x, y;
};

struct CoordsXY
{
    sint32 x, y;
};

struct TileCoordsXYZ
{
    sint32 x, y, z;
};

struct CoordsXYZ
{
    sint32 x, y, z;
};

struct TileCoordsXYZD
{
    sint32 x, y, z;
    uint8 direction;

    bool isNull() const { return x == COORDS_NULL; };
};

struct CoordsXYZD
{
    sint32 x, y, z;
    uint8 direction;

    bool isNull() const { return x == COORDS_NULL; };
};



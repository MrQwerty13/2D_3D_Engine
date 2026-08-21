#pragma once

// Stable header-only domain API. Projects that only edit or serialize room data
// can include this file without linking SDL or a renderer library.
#include "room_engine/core/entity.hpp"
#include "room_engine/core/floor_plan_editor.hpp"
#include "room_engine/core/input.hpp"
#include "room_engine/core/resource.hpp"
#include "room_engine/core/room_design.hpp"
#include "room_engine/core/serialization.hpp"
#include "room_engine/core/timing.hpp"
#include "room_engine/core/transform.hpp"

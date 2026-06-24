#pragma once
#include <stdint.h>
#include <beholder/proc/spawn.h>
#include <drogon/drogon.h>
#include <tensorflow/c/c_api.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <fmt/format.h>

inline constexpr size_t INPUT_HEIGHT = 299;
inline constexpr size_t INPUT_WIDTH = 299;
inline constexpr size_t INPUT_CHANNELS = 3;
inline constexpr size_t INPUT_SIZE = INPUT_HEIGHT * INPUT_WIDTH * INPUT_CHANNELS;

inline constexpr size_t INDEX_DRAWING = 0;
inline constexpr size_t INDEX_HENTAI = 1;
inline constexpr size_t INDEX_NEUTRAL = 2;
inline constexpr size_t INDEX_PORN = 3;
inline constexpr size_t INDEX_SEXY = 4;

int run_supervisor(const char* self);

int run_server();
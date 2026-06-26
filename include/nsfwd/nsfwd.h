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

/*
 * Round the input buffer size up to the next 16-byte boundary so the SSE
 * normalisation loop can always process 16 bytes per iteration without a
 * scalar tail.
 *
 * The final iteration may read a few bytes beyond the valid image data.
 * These bytes are zeroed as static thread local storage, and are converted
 * into the padded tail of the destination buffer, and are never copied into
 * the TensorFlow input tensor.
 */
inline constexpr size_t INPUT_SIZE_SSE = (INPUT_SIZE + 15) & ~size_t(15);

static_assert((INPUT_SIZE_SSE & 15) == 0, "INPUT_SIZE_SSE must be 16-byte aligned");

inline constexpr size_t INDEX_DRAWING = 0;
inline constexpr size_t INDEX_HENTAI = 1;
inline constexpr size_t INDEX_NEUTRAL = 2;
inline constexpr size_t INDEX_PORN = 3;
inline constexpr size_t INDEX_SEXY = 4;

int run_supervisor(const char* self);

int run_server();
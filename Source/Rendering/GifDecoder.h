#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Thin wrapper around stb_image's animated-GIF decoder (ThirdParty/stb_image.h,
// public domain, https://github.com/nothings/stb) -- isolated behind a plain
// byte-buffer interface, deliberately with no JUCE types in this header, so
// the stb implementation (a single giant C-style translation unit) never has
// to coexist with JUCE headers anywhere but GifDecoder.cpp. Static images
// (PNG/JPG/BMP) still go through JUCE's own ImageFileFormat as before --
// this is only reached for .gif files, which JUCE can't decode past the
// first frame.
struct DecodedGif
{
    bool ok = false;
    int width = 0;
    int height = 0;
    // One entry per frame, each width * height * 4 bytes, RGBA, already
    // fully composited (stb_image handles GIF's frame-disposal/canvas
    // compositing internally -- every frame here is a complete image, no
    // manual blending against previous frames needed).
    std::vector<std::vector<uint8_t>> frames;
    // Milliseconds to hold each frame, same length/order as `frames`. Some
    // encoders write 0 for "as fast as possible" -- callers should treat
    // 0 (or anything under a sane floor) as a small default rather than a
    // zero-length hold, which would spin-advance forever in one frame.
    std::vector<int> frameDelaysMs;
};

// Decodes every frame of an in-memory GIF file. Returns ok=false (with
// frames/width/height untouched) if `data` isn't a valid GIF or decoding
// otherwise fails -- never throws.
DecodedGif decodeGif(const uint8_t* data, size_t size);

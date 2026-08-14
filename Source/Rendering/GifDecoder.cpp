#include "GifDecoder.h"

// This translation unit owns the one and only STB_IMAGE_IMPLEMENTATION.
// STBI_ONLY_GIF trims the rest of stb_image's format decoders out entirely
// (JUCE's own ImageFileFormat already covers PNG/JPEG/BMP for static
// images -- stb is here purely for the animated-GIF frames JUCE can't
// give us). STBI_NO_STDIO: we only ever decode from an in-memory buffer.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#include "../../ThirdParty/stb_image.h"

DecodedGif decodeGif(const uint8_t* data, size_t size)
{
    DecodedGif result;
    if (data == nullptr || size == 0)
        return result;

    int width = 0, height = 0, frameCount = 0, componentsInFile = 0;
    int* delaysMs = nullptr;
    stbi_uc* pixels = stbi_load_gif_from_memory(data, (int) size, &delaysMs, &width, &height, &frameCount,
                                                 &componentsInFile, 4 /* force RGBA */);

    if (pixels == nullptr || width <= 0 || height <= 0 || frameCount <= 0)
    {
        if (pixels != nullptr)
            stbi_image_free(pixels);
        if (delaysMs != nullptr)
            STBI_FREE(delaysMs);
        return result;
    }

    result.width = width;
    result.height = height;
    result.frames.reserve((size_t) frameCount);
    result.frameDelaysMs.reserve((size_t) frameCount);

    const size_t frameBytes = (size_t) width * (size_t) height * 4;
    for (int i = 0; i < frameCount; ++i)
    {
        const stbi_uc* frameStart = pixels + (size_t) i * frameBytes;
        result.frames.emplace_back(frameStart, frameStart + frameBytes);
        // Guard against 0/negative delays (some encoders write 0 for "no
        // delay specified") -- 100ms (10fps) is a reasonable default that
        // won't spin the frame timer.
        const int delay = delaysMs != nullptr ? delaysMs[i] : 0;
        result.frameDelaysMs.push_back(delay > 0 ? delay : 100);
    }

    stbi_image_free(pixels);
    if (delaysMs != nullptr)
        STBI_FREE(delaysMs);

    result.ok = true;
    return result;
}

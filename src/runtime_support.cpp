#include <cstddef>

#if defined(_MSC_VER)
#pragma optimize("", off)
#endif

extern "C" void* __cdecl memcpy(void* destination, const void* source, std::size_t count) {
    auto* output = static_cast<unsigned char*>(destination);
    const auto* input = static_cast<const unsigned char*>(source);
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = input[index];
    }
    return destination;
}

extern "C" void* __cdecl memset(void* destination, int value, std::size_t count) {
    auto* output = static_cast<unsigned char*>(destination);
    const unsigned char byte = static_cast<unsigned char>(value);
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = byte;
    }
    return destination;
}

#if defined(_MSC_VER)
#pragma optimize("", on)
#endif

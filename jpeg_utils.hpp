#pragma once
#include <stdexcept>
#include <vector>
#include <tuple>
#include <algorithm>
#include <iostream>

/**
 * JPEG File Structure:
 * 
 * 1. SOI (Start of Image) marker: FF D8
 * 2. Multiple segments, each with:
 *    - Marker (2 bytes, starts with FF)
 *    - Length (2 bytes, big-endian, includes these 2 bytes)
 *    - Data (length-2 bytes)
 * 3. EOI (End of Image) marker: FF D9
 * 
 * Common markers:
 * - SOI (Start of Image): FF D8
 * - APP0 (JFIF): FF E0
 * - APP1 (EXIF): FF E1
 * - SOF0 (Baseline DCT): FF C0
 * - SOF2 (Progressive DCT): FF C2
 * - DHT (Define Huffman Table): FF C4
 * - DQT (Define Quantization Table): FF DB
 * - SOS (Start of Scan): FF DA
 * - EOI (End of Image): FF D9
 * 
 * Reference: https://github.com/corkami/formats/blob/master/image/jpeg.md
 */

// Log level control
// Set JPEG_LOGLEVEL to control verbosity:
// -1: No logging
//  0: ERROR only
//  1: ERROR + WARNING
//  2: ERROR + WARNING + INFO
//  3: ERROR + WARNING + INFO + DEBUG (all messages)
#ifndef JPEG_LOGLEVEL
#define JPEG_LOGLEVEL 3
#endif

// Log level constants
#define JPEG_LOGLEVEL_ERROR   0
#define JPEG_LOGLEVEL_WARNING 1
#define JPEG_LOGLEVEL_INFO    2
#define JPEG_LOGLEVEL_DEBUG   3

// Level-specific log macros
#if JPEG_LOGLEVEL >= JPEG_LOGLEVEL_ERROR
#define JPEG_ERROR(msg, ...) std::printf("[ERROR] " msg, ##__VA_ARGS__)
#else
#define JPEG_ERROR(msg, ...) ((void)0)
#endif

#if JPEG_LOGLEVEL >= JPEG_LOGLEVEL_WARNING
#define JPEG_WARNING(msg, ...) std::printf("[WARNING] " msg, ##__VA_ARGS__)
#else
#define JPEG_WARNING(msg, ...) ((void)0)
#endif

#if JPEG_LOGLEVEL >= JPEG_LOGLEVEL_INFO
#define JPEG_INFO(msg, ...) std::printf("[INFO] " msg, ##__VA_ARGS__)
#else
#define JPEG_INFO(msg, ...) ((void)0)
#endif

#if JPEG_LOGLEVEL >= JPEG_LOGLEVEL_DEBUG
#define JPEG_DEBUG(msg, ...) std::printf("[DEBUG] " msg, ##__VA_ARGS__)
#else
#define JPEG_DEBUG(msg, ...) ((void)0)
#endif

/**
 * Helper functions
 */
namespace jpeg_utils {
/**
 * Read a 16-bit big-endian value from a byte array
 * 
 * @param data Pointer to the data
 * @return The value in host byte order
 */
template<typename T>
inline T read_be16(const unsigned char* data) {
    return static_cast<T>((data[0] << 8) | data[1]);
}

/**
 * JPEG marker constants
 */
namespace markers {
    constexpr unsigned char SOI = 0xD8;  // Start Of Image
    constexpr unsigned char EOI = 0xD9;  // End Of Image
    constexpr unsigned char SOF0 = 0xC0; // Start Of Frame (Baseline DCT)
    constexpr unsigned char SOF2 = 0xC2; // Start Of Frame (Progressive DCT)
}

/**
 * Find a specific marker in JPEG data
 * 
 * @param data The JPEG data
 * @param it Iterator to start searching from
 * @param marker The marker to find (second byte, first byte is always 0xFF)
 * @return Iterator to the marker if found, or data.end() if not found
 */
template<typename Iterator>
auto find_marker(const std::vector<unsigned char>& data, const Iterator& it, unsigned char marker) {
    std::vector<unsigned char> seq = { 0xff, marker };
    return std::search(it, data.end(), seq.begin(), seq.end());
}

/**
 * Parse a single JPEG segment
 * 
 * @param data The JPEG data
 * @param it Iterator pointing to the start of the segment
 * @return Tuple containing the parsed segment and iterator to the next segment
 */
template<typename Iterator>
auto parse_jpeg_segment(const std::vector<unsigned char>& data, const Iterator& it)
{
    jpeg_image::segment segment;
    segment.marker[0] = *(it + 0);
    segment.marker[1] = *(it + 1);
    
    if (segment.marker[0] != 0xff) {
        return std::make_tuple(segment, it);
    }

    switch (segment.marker[1]) {
    case jpeg_utils::markers::SOI:
    case jpeg_utils::markers::EOI:
        segment.length = 0;
        break;
    default:
        segment.length = jpeg_utils::read_be16<uint16_t>(&*(it + 2));
        segment.data.resize(segment.length - 2); // Subtract 2 for the length bytes
        if (segment.length > 2) {
            std::copy(it + 4, it + 2 + segment.length, segment.data.begin());
        }
    }

    JPEG_DEBUG("Marker %02x %02x\n", segment.marker[0], segment.marker[1]);
    JPEG_DEBUG("  Length %d\n", segment.length);

    return std::make_tuple(segment, it + 2 + segment.length);
}

} // namespace jpeg_utils

/**
 * Structure representing a JPEG image and its components
 */
struct jpeg_image {
    /**
     * Structure representing a JPEG segment
     */
    struct segment {
        unsigned char marker[2];  // Marker bytes (first byte is always 0xFF)
        uint16_t length;          // Segment length (including these 2 bytes)
        std::vector<unsigned char> data; // Segment data
    };
    
    std::vector<segment> segments; // All segments in the JPEG
    std::size_t data_offset;       // Offset to the image data
    std::size_t eoi_offset;        // Offset to the End Of Image marker
    uint16_t width, height;        // Image dimensions

    /**
     * Parse a JPEG file
     * 
     * @param data The JPEG file data
     * @throws std::runtime_error if the file is not a valid JPEG
     */
    static jpeg_image parse(const std::vector<unsigned char>& data)
    {
        jpeg_image image;

        // Check SOI (Start Of Image)
        if (auto it = jpeg_utils::find_marker(data, data.begin(), jpeg_utils::markers::SOI); it != data.begin()) {
            throw std::runtime_error("This is not a JPEG file!");
        }

        // Parse all segments
        auto from = data.begin();
        while (true) {
            auto [segment, next_it] = jpeg_utils::parse_jpeg_segment(data, from);
            if (next_it != from) {
                image.segments.push_back(segment);
                from = next_it;
            } else {
                image.data_offset = std::distance(data.begin(), from);
                break;
            }
        }

        // Find EOI (End Of Image)
        if (auto it = jpeg_utils::find_marker(data, from, jpeg_utils::markers::EOI); it != data.end()) {
            image.eoi_offset = std::distance(data.begin(), it);
            JPEG_DEBUG("EOI offset %lld\n", image.eoi_offset);
        } else {
            throw std::runtime_error("Couldn't find EOI marker!");
        }

        // Get image size from SOF segments
        image.width = image.height = 0;
        for (const auto& seg : image.segments) {
            // Find Start Of Frame segments
            if (seg.marker[1] == jpeg_utils::markers::SOF0 || 
                seg.marker[1] == jpeg_utils::markers::SOF2) {
                
                JPEG_DEBUG("Segment %02x %02x : ", seg.marker[0], seg.marker[1]);
                #if JPEG_LOGLEVEL >= JPEG_LOGLEVEL_DEBUG
                for (size_t i = 0; i < seg.data.size(); i++) {
                    std::printf("%02x ", seg.data[i]);
                }
                std::printf("\n");
                #endif

                if (seg.data.size() >= 5) {
                    image.height = jpeg_utils::read_be16<uint16_t>(&seg.data[1]);
                    image.width = jpeg_utils::read_be16<uint16_t>(&seg.data[3]);
                }
            }

            if (seg.marker[1] == jpeg_utils::markers::SOF2) {
                JPEG_WARNING("Progressive JPEG may not be decoded\n");
            }
        }
        
        JPEG_INFO("Image size %d x %d\n", image.width, image.height);

        return image;
    }

};

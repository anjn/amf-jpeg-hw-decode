#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <thread>

#include "public/include/components/VideoDecoderUVD.h" // AMFVideoDecoderUVD_MJPEG
#include "public/common/AMFFactory.h"

// https://github.com/corkami/formats/blob/master/image/jpeg.md

struct jpeg_segment {
    unsigned char marker[2];
    uint16_t length;
    std::vector<unsigned char> data;
};

struct jpeg_image {
    std::vector<jpeg_segment> segments;
    std::size_t data_offset;
    std::size_t eoi_offset;
    uint16_t width, height;
};

template<typename Iterator>
auto find_marker(const std::vector<unsigned char>& data, const Iterator& it, unsigned char marker) {
    std::vector<unsigned char> seq = { 0xff, marker };
    return std::search(it, data.end(), seq.begin(), seq.end());
}

template<typename Iterator>
auto parse_jpeg_segment(const std::vector<unsigned char>& data, const Iterator& it)
{
    jpeg_segment segment;
    segment.marker[0] = *(it + 0);
    segment.marker[1] = *(it + 1);
    
    if (segment.marker[0] != 0xff) return std::make_tuple(segment, it);

    switch (segment.marker[1]) {
    case 0xd8: // Start Of Image
    case 0xd9: // End Of Image
        segment.length = 0;
        break;
    default:
        *(reinterpret_cast<uint8_t *>(&segment.length) + 1) = *(it + 2);
        *(reinterpret_cast<uint8_t *>(&segment.length) + 0) = *(it + 3);
        segment.data.resize(segment.length);
        memcpy(segment.data.data(), data.data() + std::distance(data.begin(), it + 2), segment.length);
    }

    printf("Marker %02x %02x\n", segment.marker[0], segment.marker[1]);
    printf("  Length %d\n", segment.length);

    return std::make_tuple(segment, it + 2 + segment.length);
}

jpeg_image parse_jpeg(const std::vector<unsigned char>& data)
{
    jpeg_image image;

    // Check SOI
    if (auto it = find_marker(data, data.begin(), 0xd8); it != data.begin()) {
        throw new std::runtime_error("This is not jpeg file!");
    }

    // Parse all segments
    auto from = data.begin();
    while (true) {
        if (auto [segment, next_it] = parse_jpeg_segment(data, from); next_it != from) {
            image.segments.push_back(segment);
            from = next_it;
        } else {
            image.data_offset = std::distance(data.begin(), from);
            break;
        }
    }

    // Find EOI
    if (auto it = find_marker(data, from, 0xd9); it != data.end()) {
        image.eoi_offset = std::distance(data.begin(), it);
        printf("EOI offset %lld\n", image.eoi_offset);
    } else {
        throw new std::runtime_error("Couldn't find EOI!");
    }

    // Get image size
    image.width = image.height = 0;
    for (auto& seg : image.segments) {
        // Find Start Of Frame
        if (seg.marker[1] == 0xc0 || seg.marker[1] == 0xc2) {
            printf("Segment %02x %02x : ", seg.marker[0], seg.marker[1]);
            for (int i = 0; i < seg.length - 2; i++) printf("%02x ", seg.data[i]);
            printf("\n");
            *(reinterpret_cast<uint8_t *>(&image.height) + 1) = seg.data[3];
            *(reinterpret_cast<uint8_t *>(&image.height) + 0) = seg.data[4];
            *(reinterpret_cast<uint8_t *>(&image.width) + 1) = seg.data[5];
            *(reinterpret_cast<uint8_t *>(&image.width) + 0) = seg.data[6];
        }

        if (seg.marker[1] == 0xc2) {
            printf("Warning: Progressive JPEG may not be decoded");
        }
    }
    printf("Image size %d x %d\n", image.width, image.height);

    return image;
}

void print(amf::AMF_ACCELERATION_TYPE accel_type)
{
    switch(accel_type) {
    case amf::AMF_ACCEL_NOT_SUPPORTED: printf("AMF_ACCEL_NOT_SUPPORTED\n"); break;
    case amf::AMF_ACCEL_HARDWARE: printf("AMF_ACCEL_HARDWARE\n"); break;
    case amf::AMF_ACCEL_GPU: printf("AMF_ACCEL_GPU\n"); break;
    case amf::AMF_ACCEL_SOFTWARE: printf("AMF_ACCEL_SOFTWARE\n"); break;
    default: printf("%d\n", accel_type);
    }
}

void print(AMF_RESULT result)
{
    switch (result) {
        case AMF_OK: printf("AMF_OK\n"); break;
        case AMF_FAIL: printf("AMF_FAIL\n"); break;
        case AMF_EOF: printf("AMF_EOF\n"); break;
        case AMF_INPUT_FULL: printf("AMF_INPUT_FULL\n"); break;
        case AMF_DECODER_NO_FREE_SURFACES: printf("AMF_DECODER_NO_FREE_SURFACES\n"); break;
        case AMF_REPEAT: printf("AMF_REPEAT\n"); break;
        default: printf("%d\n", result);
    }
}

void copy_plane(amf::AMFPlanePtr plane, uint8_t* dst)
{
    uint8_t* src = reinterpret_cast<uint8_t*>(plane->GetNative());
    int offset_x = plane->GetOffsetX();
    int offset_y = plane->GetOffsetY();
    int pixel_size = plane->GetPixelSizeInBytes();
    int height = plane->GetHeight();
    int width = plane->GetWidth();
    int pitch_h = plane->GetHPitch();

    for (int y = 0; y < height; y++) {
        int size = pixel_size * width;
        memcpy(dst, src + pitch_h * (offset_y + y) + pixel_size * offset_x, size);
        dst += size;
    }
}

void save_decoded_image(amf::AMFDataPtr data)
{
    auto mem_type = data->GetMemoryType();
    if (mem_type == amf::AMF_MEMORY_HOST) printf("memory type AMF_MEMORY_HOST\n");
    if (mem_type == amf::AMF_MEMORY_DX9) printf("memory type AMF_MEMORY_DX9\n");
    else printf("memory type %d\n", mem_type);

    auto res = data->Convert(amf::AMF_MEMORY_HOST);
    printf("Convert to host memory "); print(res);
    
    amf::AMFSurfacePtr surface(data);
    amf::AMFPlanePtr plane_y  = surface->GetPlane(amf::AMF_PLANE_Y);
    amf::AMFPlanePtr plane_uv = surface->GetPlane(amf::AMF_PLANE_UV);

    int width = plane_y->GetWidth();
    int height = plane_y->GetHeight();
    printf("Image size %d x %d\n", width, height);

    // Copy NV12 (YUV420)
    std::vector<uint8_t> img_nv12(width * height * 3 / 2);
    copy_plane(plane_y , img_nv12.data());
    copy_plane(plane_uv, img_nv12.data() + width * height);

    // Check output
    bool all_zero = true;
    for (auto& p : img_nv12) if (p != 0) all_zero = false;
    if (all_zero) printf("Output image is all zero!\n");

    // Save to a file
    std::ofstream ofs("output.nv12", std::ios::binary);
    if (!ofs) { printf("Failed to open file!\n"); return; }
    ofs.write(reinterpret_cast<char*>(img_nv12.data()), img_nv12.size());
    ofs.close();
}

int main(int argc, char** argv)
{
    std::string filepath = "test.jpg";
    if (argc >= 2) filepath = argv[1];

    // Open JPEG file
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs) { wprintf(L"Failed to open file\n"); return 1; }

    std::streamsize filesize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    wprintf(L"File size %lld\n", filesize);

    std::vector<unsigned char> filedata(filesize);
    if (!ifs.read(reinterpret_cast<char *>(filedata.data()), filesize)) {
        wprintf(L"Failed to read file\n"); return 1;
    }

    // Parse JPEG
    jpeg_image image = parse_jpeg(filedata);

    if (image.width == 0 || image.height == 0) {
        wprintf(L"Failed to detect image size\n"); return 1;
    }

    AMF_RESULT res;
    amf::AMFContextPtr context;
    amf::AMFComponentPtr component;
    amf::AMFCapsPtr caps;
    amf::AMFIOCapsPtr iocaps;
    amf::AMFBufferPtr buffer;
    amf::AMFDataPtr data;

    // Initialize decoder
    res = g_AMFFactory.Init();
    if (res != AMF_OK) { wprintf(L"AMF Failed to initialize\n"); goto terminate; }
    res = g_AMFFactory.GetFactory()->CreateContext(&context);
    if (res != AMF_OK) { wprintf(L"AMF Failed to create context\n"); goto terminate; }
    res = g_AMFFactory.GetFactory()->CreateComponent(context, AMFVideoDecoderUVD_MJPEG, &component);
    if (res != AMF_OK) { wprintf(L"AMF Failed to create component\n"); goto terminate; }
    res = component->Init(amf::AMF_SURFACE_NV12, image.width, image.height);
    if (res != AMF_OK) { wprintf(L"AMF Failed to init component\n"); goto terminate; }

    // Check capabilities
    component->GetCaps(&caps);
    caps->GetInputCaps(&iocaps);
    auto accel_type = caps->GetAccelerationType();
    printf("Acceleration type : "); print(accel_type);
    int min_width, max_width, min_height, max_height;
    iocaps->GetWidthRange(&min_width, &max_width);
    iocaps->GetHeightRange(&min_height, &max_height);
    printf("Image width : %d - %d\n", min_width, max_width);
    printf("Image height : %d - %d\n", min_height, max_height);

    // Allocate buffer
    auto image_size = image.eoi_offset + 2;
    res = context->AllocBuffer(amf::AMF_MEMORY_HOST, image_size, &buffer);
    printf("Allocated %zd bytes buffer : ", image_size); print(res);
    // Copy JPEG data
    memcpy(buffer->GetNative(), filedata.data(), image_size);

    // Submit to decoder
    res = component->SubmitInput(buffer.Detach());
    printf("AMF SubmitInput returned "); print(res);
    if (res != AMF_OK) goto terminate;

    // Query output
    res = component->QueryOutput(&data);
    printf("AMF QueryOutput returned "); print(res);
    if (res != AMF_OK) goto terminate;

    // Save decoded image to a file
    save_decoded_image(data);

    // Terminate
terminate:
    if (component != nullptr) component->Terminate();
    if (context != nullptr) context->Terminate();
    g_AMFFactory.Terminate();

    printf("Terminated\n");
}
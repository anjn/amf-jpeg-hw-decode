#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <thread>

#include "public/include/components/VideoDecoderUVD.h" // AMFVideoDecoderUVD_MJPEG
#include "public/common/AMFFactory.h"

#include "jpeg_utils.hpp"

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
    jpeg_image image;
    try {
        image = jpeg_image::parse(filedata);
    } catch (const std::exception &e) {
        wprintf(L"Failed to parse JPEG file : %hs\n", e.what()); return 1;
    }

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
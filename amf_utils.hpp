#pragma once
#include "public/include/components/VideoDecoderUVD.h" // AMFVideoDecoderUVD_MJPEG
#include "public/common/AMFFactory.h"

inline void amf_print(amf::AMF_ACCELERATION_TYPE accel_type)
{
    switch(accel_type) {
    case amf::AMF_ACCEL_NOT_SUPPORTED: printf("AMF_ACCEL_NOT_SUPPORTED\n"); break;
    case amf::AMF_ACCEL_HARDWARE: printf("AMF_ACCEL_HARDWARE\n"); break;
    case amf::AMF_ACCEL_GPU: printf("AMF_ACCEL_GPU\n"); break;
    case amf::AMF_ACCEL_SOFTWARE: printf("AMF_ACCEL_SOFTWARE\n"); break;
    default: printf("%d\n", accel_type);
    }
}

inline void amf_print(AMF_RESULT result)
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

struct amf_mjpeg_decoder {
    amf::AMFContextPtr context;
    amf::AMFComponentPtr component;

    amf_mjpeg_decoder() {}

    virtual ~amf_mjpeg_decoder() {
        if (component != nullptr) component->Terminate();
        if (context != nullptr) context->Terminate();
        g_AMFFactory.Terminate();
    }

    void init(int width, int height) {
        // Initialize decoder
        AMF_RESULT res = g_AMFFactory.Init();
        if (res != AMF_OK) { throw std::runtime_error("AMF Failed to initialize"); }
        res = g_AMFFactory.GetFactory()->CreateContext(&context);
        if (res != AMF_OK) { throw std::runtime_error("AMF Failed to create context"); }
        res = g_AMFFactory.GetFactory()->CreateComponent(context, AMFVideoDecoderUVD_MJPEG, &component);
        if (res != AMF_OK) { throw std::runtime_error("AMF Failed to create component"); }
        res = component->Init(amf::AMF_SURFACE_NV12, width, height);
        if (res != AMF_OK) { throw std::runtime_error("AMF Failed to init component"); }
    }

    void check_caps() {
        // Check capabilities
        amf::AMFCapsPtr caps;
        amf::AMFIOCapsPtr iocaps;
        
        component->GetCaps(&caps);
        caps->GetInputCaps(&iocaps);
        auto accel_type = caps->GetAccelerationType();
        printf("Acceleration type : "); amf_print(accel_type);
        int min_width, max_width, min_height, max_height;
        iocaps->GetWidthRange(&min_width, &max_width);
        iocaps->GetHeightRange(&min_height, &max_height);
        printf("Image width : %d - %d\n", min_width, max_width);
        printf("Image height : %d - %d\n", min_height, max_height);
    }


};

inline void copy_plane(amf::AMFPlanePtr plane, uint8_t* dst)
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

inline std::vector<uint8_t> get_decoded_image(amf::AMFDataPtr data)
{
    auto mem_type = data->GetMemoryType();
    if (mem_type == amf::AMF_MEMORY_HOST) printf("memory type AMF_MEMORY_HOST\n");
    if (mem_type == amf::AMF_MEMORY_DX9) printf("memory type AMF_MEMORY_DX9\n");
    else printf("memory type %d\n", mem_type);

    auto res = data->Convert(amf::AMF_MEMORY_HOST);
    printf("Convert to host memory "); amf_print(res);
    
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

    return img_nv12;
}

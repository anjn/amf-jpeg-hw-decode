#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <thread>

#include "public/include/components/VideoDecoderUVD.h" // AMFVideoDecoderUVD_MJPEG
#include "public/common/AMFFactory.h"

#include "amf_utils.hpp"
#include "jpeg_utils.hpp"

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
        image = parse_jpeg(filedata);
    } catch (const std::exception &e) {
        wprintf(L"Failed to parse JPEG file : %hs\n", e.what()); return 1;
    }

    if (image.width == 0 || image.height == 0) {
        wprintf(L"Failed to detect image size\n"); return 1;
    }

    // Initialize decoder
    amf_mjpeg_decoder decoder;
    try {
        decoder.init(image.width, image.height);
    } catch (const std::exception &e) {
        wprintf(L"Failed to init decoder : %hs\n", e.what()); return 1;
    }

    AMF_RESULT res;
    amf::AMFBufferPtr buffer;
    amf::AMFDataPtr data;

    // Allocate buffer
    auto image_size = image.eoi_offset + 2;
    res = decoder.context->AllocBuffer(amf::AMF_MEMORY_HOST, image_size, &buffer);
    printf("Allocated %zd bytes buffer : ", image_size); amf_print(res);
    // Copy JPEG data
    memcpy(buffer->GetNative(), filedata.data(), image_size);

    // Submit to decoder
    res = decoder.component->SubmitInput(buffer.Detach());
    printf("AMF SubmitInput returned "); amf_print(res);
    if (res != AMF_OK) return 1;

    // Query output
    res = decoder.component->QueryOutput(&data);
    printf("AMF QueryOutput returned "); amf_print(res);
    if (res != AMF_OK) return 1;

    // Save decoded image to a file
    auto img_dec = get_decoded_image(data);
    std::ofstream ofs("output.nv12", std::ios::binary);
    if (!ofs) { printf("Failed to open file!\n"); return 1; }
    ofs.write(reinterpret_cast<char*>(img_dec.data()), img_dec.size());
    ofs.close();
}
#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <thread>

#include "public/include/components/VideoDecoderUVD.h" // AMFVideoDecoderUVD_MJPEG
#include "public/common/AMFFactory.h"

#include "amf_utils.hpp"
#define JPEG_LOGLEVEL 1
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
    if (image.num_components == 0) {
        wprintf(L"Failed to detect the number of components\n"); return 1;
    }

    // Initialize decoder
    amf_mjpeg_decoder decoder;
    try {
        decoder.init(image.width, image.height);
    } catch (const std::exception &e) {
        wprintf(L"Failed to init decoder : %hs\n", e.what()); return 1;
    }

    const int num_input = 200;
    std::atomic<int> num_decode = 0;

    std::thread thr([&]() {
        for (int i = 0; i < num_input; i++) {
            while (true) {
                AMF_RESULT res;
                amf::AMFDataPtr data;

                // Query output
                res = decoder.component->QueryOutput(&data);
                if (res != AMF_OK) {
                    printf("AMF QueryOutput returned "); amf_print(res); return;
                }
        
                if (data == nullptr) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
                
                // Get decoded image
                auto img_dec = get_decoded_image(data);
                printf("Decoded %lld bytes\n", img_dec.size());
                
                num_decode++;

                break;
            }
        }
    });
    
    auto start = std::chrono::system_clock::now();

    for (int i = 0; i < num_input; i++) {
        while (true) {
            if (i - num_decode > 4) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            AMF_RESULT res;
            amf::AMFBufferPtr buffer;

            // Allocate buffer
            auto image_size = image.eoi_offset + 2;
            res = decoder.context->AllocBuffer(amf::AMF_MEMORY_HOST, image_size, &buffer);
            if (res != AMF_OK) {
                printf("AMF AllocBuffer returned "); amf_print(res);
                return 1;
            }
            printf("Allocated %zd bytes buffer : ", image_size); amf_print(res);

            // Copy JPEG data
            memcpy(buffer->GetNative(), filedata.data(), image_size);

            // Submit to decoder
            res = decoder.component->SubmitInput(buffer.Detach());
            if (res != AMF_OK) {
                printf("AMF SubmitInput returned "); amf_print(res);
                return 1;
            }

            break;
        }
    }

    thr.join();

    std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - start;
    std::cout << "Decoded " << num_input << " images in " << elapsed.count() << " sec" << std::endl;
    std::cout << (num_input / elapsed.count()) << " FPS" << std::endl;
}
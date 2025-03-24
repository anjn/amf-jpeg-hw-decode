#include "public/common/AMFFactory.h"

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
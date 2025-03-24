# AMD AMF JPEG Hardware Decoding Sample

This repository contains sample code demonstrating how to use AMD's Advanced Media Framework (AMF) to perform hardware-accelerated JPEG decoding on AMD GPUs.

## Overview

AMD Advanced Media Framework (AMF) is a lightweight, portable multimedia framework that abstracts away most of the platform and API-specific details and allows for easy implementation of multimedia applications using a variety of technologies, such as DirectX 11, OpenGL, and Vulkan.

This sample specifically demonstrates how to leverage AMF for hardware-accelerated JPEG decoding, which can significantly improve performance compared to CPU-based decoding, especially for high-resolution images or when processing multiple images in sequence.

## Features

- Hardware-accelerated JPEG decoding using AMD GPUs
- Parsing and processing of JPEG file structure
- Output in NV12 (YUV 4:2:0) format
- Simple API demonstration for integration into your own applications

## Prerequisites

- AMD GPU/APU with appropriate drivers
- Visual Studio Community 2022
- CMake (https://cmake.org/download/)
- FFmpeg (https://github.com/BtbN/FFmpeg-Builds/releases) - for converting the NV12 output to viewable formats
- AMD AMF SDK (should be placed in a directory adjacent to this project)

## Building the Sample

1. Ensure you have the AMD AMF SDK installed in a directory adjacent to this project (../AMF)
2. Create a build directory:
   ```
   mkdir build
   cd build
   ```
3. Generate the Visual Studio solution using CMake:
   ```
   cmake ..
   ```
4. Open the generated solution in Visual Studio 2022 and build the project, or build directly from the command line:
   ```
   cmake --build . --config Debug
   ```

## Usage

The sample application takes a JPEG file as input and outputs the decoded image in NV12 format:

```
.\build\Debug\test.exe [input_jpeg_file]
```

If no input file is specified, it defaults to "test.jpg" in the current directory.

### Example

```
.\build\Debug\test.exe test.jpg
```

This will decode the JPEG file and save the output as "output.nv12" in the current directory.

### Converting NV12 Output

Since NV12 is not directly viewable in most image viewers, you can use FFmpeg to convert it to a common format like JPEG:

```
ffmpeg.exe -f rawvideo -pix_fmt nv12 -s [width]x[height] -i output.nv12 -q:v 2 -loglevel error -y output.jpg
```

Replace `[width]x[height]` with the actual dimensions of your image. The sample code outputs the dimensions during execution.

## How It Works

The sample code demonstrates several key aspects of JPEG decoding with AMF:

1. **JPEG Parsing**: The code includes a simple JPEG parser that extracts the necessary information from the JPEG file, including image dimensions and segment data.

2. **AMF Initialization**: The sample initializes the AMF framework and creates a JPEG decoder component.

3. **Hardware Decoding**: The JPEG data is submitted to the AMF decoder, which uses the GPU for hardware-accelerated decoding.

4. **Output Processing**: The decoded image is retrieved in NV12 format (a common YUV 4:2:0 pixel format used in video processing) and saved to a file.

The NV12 format consists of a full-resolution Y (luma) plane followed by interleaved U and V (chroma) planes at half resolution, making it efficient for hardware processing but requiring conversion for viewing.

## Notes

- The sample code includes error checking and capability querying to ensure compatibility with your hardware.
- The decoded output is in NV12 format, which is commonly used in video processing but requires conversion for viewing as a standard image.
- The code demonstrates basic AMF usage patterns that can be adapted for more complex applications.

## License

Please refer to the AMD AMF SDK for licensing information regarding the AMF framework.

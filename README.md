
- Visual Studio Community 2022
- CMake
    - https://cmake.org/download/
- FFmpeg
    - https://github.com/BtbN/FFmpeg-Builds/releases


```
.\build\Debug\test.exe test.jpg
ffmpeg.exe -f rawvideo -pix_fmt nv12 -i output.nv12 -q:v 2 -loglevel error -y -s 640x360 output.jpg
```

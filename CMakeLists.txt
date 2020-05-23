cmake_minimum_required(VERSION 2.8)

project(AVCodec)

include_directories("/usr/include/ffmpeg")

add_executable(${PROJECT_NAME} "MediaCodecDemo.cpp" "MediaCodec.cpp")
target_link_libraries(${PROJECT_NAME} stdc++ avcodec avformat avfilter avresample avutil swscale)

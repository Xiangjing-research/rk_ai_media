cmake_minimum_required(VERSION 3.4.1)

project(rknn_yolov5_demo)

set(CMAKE_SYSTEM_NAME Linux)
set(TARGET_SOC rk356x)

# set(CMAKE_C_COMPILER  /home/alientek/orangepi-build/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-c++)
# set(CMAKE_CXX_COMPILER /home/alientek/orangepi-build/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn_yolov5_demo/${CMAKE_SYSTEM_NAME})

set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# rknn api
if(TARGET_SOC STREQUAL "rk356x")
  set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/../runtime/RK356X/${CMAKE_SYSTEM_NAME}/librknn_api)
elseif(TARGET_SOC STREQUAL "rk3588")
  set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/../runtime/RK3588/${CMAKE_SYSTEM_NAME}/librknn_api)
else()
  message(FATAL_ERROR "TARGET_SOC is not set, ref value: rk356x or rk3588 or rv110x")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(RKNN_RT_LIB ${RKNN_API_PATH}/${CMAKE_ANDROID_ARCH_ABI}/librknnrt.so)
else()
  if (CMAKE_C_COMPILER MATCHES "aarch64")
    set(LIB_ARCH aarch64)
  else()
    set(LIB_ARCH armhf)
  endif()
  set(RKNN_RT_LIB ${RKNN_API_PATH}/${LIB_ARCH}/librknnrt.so)
endif()
include_directories(${RKNN_API_PATH}/include)
# track
include_directories(/usr/include/eigen3)
include_directories(${PROJECT_SOURCE_DIR}/include/bytetrack)

include_directories(${PROJECT_SOURCE_DIR}/include/utils)
# # opencv
# if (CMAKE_SYSTEM_NAME STREQUAL "Android")
#     set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/../3rdparty/opencv/OpenCV-android-sdk/sdk/native/jni/abi-${CMAKE_ANDROID_ARCH_ABI})
# else()
#   if(LIB_ARCH STREQUAL "armhf")
#     set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/../3rdparty/opencv/opencv-linux-armhf/share/OpenCV)
#     message(1)
#   else()
#     set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/../3rdparty/opencv/opencv-linux-aarch64/share/OpenCV)
#     include_directories(${CMAKE_SOURCE_DIR}/../3rdparty/opencv/opencv-linux-aarch64/include)
#   endif()
# endif()
# find_package(OpenCV REQUIRED)
# 多核
# find_package(OpenMP)

#rga
if(TARGET_SOC STREQUAL "rk356x")
  set(RGA_PATH ${CMAKE_SOURCE_DIR}/../3rdparty/rga/RK356X)
elseif(TARGET_SOC STREQUAL "rk3588")
  set(RGA_PATH ${CMAKE_SOURCE_DIR}/../3rdparty/rga/RK3588)
else()
  message(FATAL_ERROR "TARGET_SOC is not set, ref value: rk356x or rk3588")
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(RGA_LIB ${RGA_PATH}/lib/Android/${CMAKE_ANDROID_ARCH_ABI}/librga.so)
else()
  if (CMAKE_C_COMPILER MATCHES "aarch64")
    set(LIB_ARCH aarch64)
  else()
    set(LIB_ARCH armhf)
  endif()
  set(RGA_LIB ${RGA_PATH}/lib/Linux//${LIB_ARCH}/librga.so)
endif()

include_directories( ${RGA_PATH}/include)

set(CMAKE_INSTALL_RPATH "lib")

# rknn_yolov5_demo
include_directories( ${CMAKE_SOURCE_DIR}/include)

add_executable(rknn_yolov5_demo
        src/main.cc
        src/bytetrack/BYTETracker.cpp
        src/bytetrack/kalmanFilter.cpp
        src/bytetrack/lapjv.cpp
        src/bytetrack/STrack.cpp
        src/bytetrack/utils.cpp
        src/bytetrack/scalar.cc
        src/utils/file_utils.c
        src/utils/image_drawing.c
        src/utils/image_utils.c
        src/preprocess.cc
        src/postprocess.cc
        src/v4l2.c
        src/yolov5.cc
)

# target_link_libraries(rknn_yolov5_demo PUBLIC OpenMP::OpenMP_CXX
target_link_libraries(rknn_yolov5_demo
  ${RKNN_RT_LIB}
  ${RGA_LIB}
)


# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn_yolov5_demo/${CMAKE_SYSTEM_NAME})
install(TARGETS rknn_yolov5_demo DESTINATION ./)

install(PROGRAMS ${RKNN_RT_LIB} DESTINATION lib)
install(PROGRAMS ${RGA_LIB} DESTINATION lib)
install(DIRECTORY model DESTINATION ./)

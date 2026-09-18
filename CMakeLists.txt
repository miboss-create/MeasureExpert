cmake_minimum_required(VERSION 3.19)
project(MeasureExpert LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

# ---------- HALCON（仓库内 3rdParty）----------
set(HALCON_ROOT "${CMAKE_SOURCE_DIR}/3rdParty/Halcon")
set(HALCON_ARCH "x64-win64")
set(HALCON_INCLUDE_DIR "${HALCON_ROOT}/include")
set(HALCON_LIB_DIR "${HALCON_ROOT}/lib/${HALCON_ARCH}")

if(NOT EXISTS "${HALCON_INCLUDE_DIR}/halconcpp/HalconCpp.h")
    message(FATAL_ERROR "HALCON headers missing: ${HALCON_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${HALCON_LIB_DIR}/halconcpp.lib")
    message(FATAL_ERROR "HALCON libs missing: ${HALCON_LIB_DIR}")
endif()

# ---------- OpenCV（与烛照一致：find_package；优先 3rdParty/OpenCV）----------
# OpenCVConfig.cmake 可能在 build 根目录，也可能在 x64/vcXX/lib
if(NOT OpenCV_DIR AND DEFINED ENV{OpenCV_DIR})
    set(OpenCV_DIR "$ENV{OpenCV_DIR}")
endif()
if(NOT OpenCV_DIR)
    set(_me_opencv_hints
        "${CMAKE_SOURCE_DIR}/3rdParty/OpenCV"
        "${CMAKE_SOURCE_DIR}/3rdParty/OpenCV/x64/vc16/lib"
        "${CMAKE_SOURCE_DIR}/3rdParty/OpenCV/x64/vc17/lib"
        "${CMAKE_SOURCE_DIR}/3rdParty/OpenCV/x64/vc15/lib"
    )
    file(GLOB _me_opencv_cfg_globs
        "${CMAKE_SOURCE_DIR}/3rdParty/OpenCV/x64/vc*/lib/OpenCVConfig.cmake")
    foreach(_cfg ${_me_opencv_cfg_globs})
        get_filename_component(_hint "${_cfg}" DIRECTORY)
        list(APPEND _me_opencv_hints "${_hint}")
    endforeach()
    foreach(_hint ${_me_opencv_hints})
        if(EXISTS "${_hint}/OpenCVConfig.cmake")
            set(OpenCV_DIR "${_hint}")
            break()
        endif()
    endforeach()
endif()
if(NOT OpenCV_DIR OR NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
    message(FATAL_ERROR
        "OpenCV not found.\n"
        "  1) mklink /J 3rdParty\\OpenCV <OpenCV>\\opencv\\build\n"
        "  2) or set OpenCV_DIR to the folder that contains OpenCVConfig.cmake\n"
        "     (often <build> or <build>/x64/vc16/lib)\n"
        "See docs/第三方依赖.md")
endif()
set(OpenCV_DIR "${OpenCV_DIR}" CACHE PATH "Directory containing OpenCVConfig.cmake" FORCE)
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV ${OpenCV_VERSION} @ ${OpenCV_DIR}")

# ---------- 输出：../MeasureExpert_runtime（Debug 后缀 D）----------
set(ME_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/../MeasureExpert_runtime" CACHE PATH
    "Executable output directory")
file(MAKE_DIRECTORY "${ME_RUNTIME_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ME_RUNTIME_DIR}")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${ME_RUNTIME_DIR}")
endforeach()

qt_add_executable(MeasureExpert
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/UI/MainWindow.cpp
    src/UI/MainWindow.h
    src/UI/CustomImageView.cpp
    src/UI/CustomImageView.h
    src/UI/CustomGraphicsView.cpp
    src/UI/CustomGraphicsView.h
    src/UI/CustomImageItem.cpp
    src/UI/CustomImageItem.h
    src/UI/LogSink.cpp
    src/UI/LogSink.h
    src/UI/LogWidget.cpp
    src/UI/LogWidget.h
    src/UI/ThumbnailStrip.cpp
    src/UI/ThumbnailStrip.h
    src/ImageConvert.h
    resources/resources.qrc
)

# Windows exe 文件图标（资源管理器 / 任务栏）
if(WIN32)
    target_sources(MeasureExpert PRIVATE resources/app.rc)
endif()

set_target_properties(MeasureExpert PROPERTIES DEBUG_POSTFIX "D")

target_include_directories(MeasureExpert PRIVATE
    src
    ${HALCON_INCLUDE_DIR}
    ${HALCON_INCLUDE_DIR}/halconcpp
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(MeasureExpert PRIVATE
    Qt::Core
    Qt::Widgets
    ${OpenCV_LIBS}
    "${HALCON_LIB_DIR}/halconcpp.lib"
    "${HALCON_LIB_DIR}/halcon.lib"
)

# 将 opencv_world*.dll 拷到运行目录（OpenCV_DIR 可能是 build 根或 x64/vcXX/lib）
if(WIN32)
    set(_me_opencv_dll_globs
        "${OpenCV_DIR}/../bin/opencv_world*.dll"
        "${OpenCV_DIR}/x64/vc*/bin/opencv_world*.dll"
        "${OpenCV_DIR}/../../bin/opencv_world*.dll"
    )
    set(ME_OPENCV_DLLS "")
    foreach(_g ${_me_opencv_dll_globs})
        file(GLOB _found ${_g})
        if(_found)
            list(APPEND ME_OPENCV_DLLS ${_found})
        endif()
    endforeach()
    list(REMOVE_DUPLICATES ME_OPENCV_DLLS)
    if(ME_OPENCV_DLLS)
        add_custom_command(TARGET MeasureExpert POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${ME_OPENCV_DLLS}
                "${ME_RUNTIME_DIR}"
            COMMENT "Copy OpenCV DLLs to MeasureExpert_runtime"
        )
    else()
        message(WARNING "OpenCV DLLs not found next to OpenCV_DIR=${OpenCV_DIR}")
    endif()
endif()

install(TARGETS MeasureExpert
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

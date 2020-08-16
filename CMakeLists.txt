
cmake_minimum_required (VERSION 3.8)
project(argus_editor_cmake)
# Добавьте источник в исполняемый файл этого проекта.
add_executable (argus_editor_cmake "argus_editor_cmake.cpp" "argus_editor_cmake.h")
set(ImageMagick_USE_STATIC_LIBS ON)
set(ImageMagick_USE_MULTITHREADED ON)

message("modules path: ${CMAKE_MODULE_PATH}")


find_package(ImageMagick COMPONENTS Magick++)
find_package(ImageMagick COMPONENTS MagickWand)
find_package(ImageMagick COMPONENTS MagickCore)

find_package(ImageMagick REQUIRED)
if(NOT ImageMagick_FOUND)
	message(SEND_ERROR "Failed to find ImageMagick")
	return()
else()
    include_directories(${ImageMagick_INCLUDE_DIRS})
endif()

message("IM_fnd: ${ImageMagick_FOUND}")
message("IM_inc: ${ImageMagick_INCLUDE_DIRS}")
message("IM_lib: ${ImageMagick_LIBRARIES}")

set(SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/argus_editor.cpp)
set(HDRS "${ImageMagick_INCLUDE_DIRS}")

include_directories("${ImageMagick_INCLUDE_DIRS}")
target_link_libraries(argus_editor_cmake ${ImageMagick_LIBRARIES})

cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Time_Circuit_Control)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Collect custom application sources (mirrors the CubeIDE project's recursive
# source folders under App/Time_Circuits, CANopen, and Libraries)
file(GLOB APP_TIME_CIRCUITS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/App/Time_Circuits/*.c)
file(GLOB CANOPEN_OD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/CANopen/*.c)
file(GLOB_RECURSE LIBRARIES_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/*.c)
list(FILTER LIBRARIES_SRC EXCLUDE REGEX "CanOpenSTM32/CANopenNode_STM32/OD\\.c$")
list(FILTER LIBRARIES_SRC EXCLUDE REGEX "CanOpenSTM32/CANopenNode/example/")
list(FILTER LIBRARIES_SRC EXCLUDE REGEX "CanOpenSTM32/examples/")
list(FILTER LIBRARIES_SRC EXCLUDE REGEX "CanOpenSTM32/legacy/")
list(FILTER LIBRARIES_SRC EXCLUDE REGEX "Peripherals/ds3231/examples/")

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${APP_TIME_CIRCUITS_SRC}
    ${CANOPEN_OD_SRC}
    ${LIBRARIES_SRC}
    FATFS/Target/user_diskio_spi.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App/Time_Circuits
    CANopen
    Libraries/Peripherals/bno055
    Libraries/Peripherals/ds3231
    Libraries/Peripherals/ht16k33
    Libraries/Peripherals/keypad3x4w
    Libraries/CanOpenSTM32/CANopenNode
    Libraries/CanOpenSTM32/CANopenNode_STM32
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

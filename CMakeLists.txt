##############################################################################
#
# TANAKA ENTERPRISES CONFIDENTIAL
# __________________
#
#  [2023] Tanaka Enterprises
#  All Rights Reserved.
#
# NOTICE:  All information contained herein is, and remains
# the property of Revolabs Incorporated and its suppliers,
# if any.  The intellectual and technical concepts contained
# herein are proprietary to Revolabs Incorporated
# and its suppliers and may be covered by U.S. and Foreign Patents,
# patents in process, and are protected by trade secret or copyright law.
# Dissemination of this information or reproduction of this material
# is strictly forbidden unless prior written permission is obtained
# from Revolabs Incorporated.
#
##############################################################################
#
# @file CMakeLists.txt
# @brief This file configures the project setting for each target
# @version 1.0.0
# @author Your name
#
# Note :
# Please build yamaha-av-components prior to build this project
#
# Usage : 
# > mkdir build
# > cd build
# > cmake .. -DPLATFORM=<PC/PIlinux/c626and>
# > make
# > make install

# ---- Project setting ----
# [TODO] Please set the project name and version
set(YAVC_PROJECT_NAME dubplate)
set(YAVC_PROJECT_VERSION "1.0.0")

# ---- YAVC location ----
# [TODO] Please set the path to YAVC
set(YAVC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../yamaha-av-components CACHE STRING "Specify path to yavc")

# ---- Required CMake version ----
cmake_minimum_required(VERSION 3.15)

# ---- Choose build type ----
set(CMAKE_BUILD_TYPE release)
# set(CMAKE_BUILD_TYPE debug)

# ---- Prepare toolchain ----
# Switch build target
set(PLATFORM PC CACHE STRING "Build target (PC / Pilinux / c626and)")

# Import platform setting
include(${YAVC_PATH}/cmakes/platform.cmake)

# ---- Project configuration ----
# Project name and language specified
project(${YAVC_PROJECT_NAME} VERSION ${YAVC_PROJECT_VERSION})

# Configure install path
include(${YAVC_PATH}/cmakes/install.cmake)

# Include path to YAVC
include_directories(
  ${YAVC_PATH}/build.${PLATFORM}/include/yavc
  ${YAVC_PATH}/src/template
)

# Link path to YAVC
link_directories   (${YAVC_PATH}/build.${PLATFORM}/lib)

# ---- Option setting ----
# JUCE
if(WITH_JUCE)
  set(CMAKE_PREFIX_PATH ${YAVC_PATH}/build.${PLATFORM}/lib/cmake/JUCE-7.0.5 ${CMAKE_PREFIX_PATH})
  find_package(JUCE REQUIRED)
endif(WITH_JUCE)

# OpenCV
if(WITH_VIDEO)
  find_package(OpenCV REQUIRED)
endif(WITH_VIDEO)

# ---- src ----
add_subdirectory(src)

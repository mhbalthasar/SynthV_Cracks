# YumeKey Tool 2.0
#
# Copyright (C) 2023 Xi Jinpwned Software
#
# This software is made available to you under the terms of the GNU Affero
# General Public License version 3.0. For more information, see the included
# LICENSE.txt file.

cmake_minimum_required(VERSION 3.15)
project(svkey VERSION 2.1.0)

if(NOT NO_TOOL)
  add_subdirectory(JUCE)

  juce_add_console_app(svkey
    PRODUCT_NAME "svkey")

  target_sources(svkey
    PRIVATE
      zlib/adler32.c
      zlib/compress.c
      zlib/crc32.c
      zlib/deflate.c
      zlib/gzclose.c
      zlib/gzlib.c
      zlib/gzread.c
      zlib/gzwrite.c
      zlib/infback.c
      zlib/inffast.c
      zlib/inflate.c
      zlib/inftrees.c
      zlib/trees.c
      zlib/uncompr.c
      zlib/zutil.c

      tool.cpp
      tool.rc)

  target_include_directories(svkey
    PRIVATE
      zlib)

  target_compile_definitions(svkey
    PRIVATE
      JUCE_WEB_BROWSER=0
      JUCE_USE_CURL=0)

  if(NOT MSVC)
    add_compile_options(-Wno-deprecated-non-prototype)

    if(WIN32)
      target_link_libraries(svkey
        PRIVATE
          -municode -static -static-libgcc -static-libstdc++ -lstdc++)
    endif()
  endif()

  if(DEV_AND_DEBUG)
    target_link_libraries(svkey
      PRIVATE
        juce::juce_core
        juce::juce_cryptography
      PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
  else()
    target_link_libraries(svkey
      PRIVATE
        juce::juce_core
        juce::juce_cryptography
      PUBLIC
       juce::juce_recommended_config_flags)
  endif()
endif()

if(NOT NO_HELPER)
  if(WIN32)
    add_library(svpatch
      SHARED
        minhook/src/buffer.c
        minhook/src/hde/hde64.c
        minhook/src/hook.c
        minhook/src/trampoline.c

        helper.c
        helper.rc)

    target_link_libraries(svpatch
      PRIVATE
        -lwininet -luuid)
  elseif(APPLE)
    add_library(svpatch
      SHARED
        fishhook/fishhook.c

        helper.c)
  else()
    add_library(svpatch
      SHARED
        helper.c)
  endif()
endif()

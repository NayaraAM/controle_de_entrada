#***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at https://curl.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# SPDX-License-Identifier: curl
#
###########################################################################
if(NOT EXISTS "/home/nayaradeandrade/ATR/build/_deps/curl-build/install_manifest.txt")
  message(FATAL_ERROR "Cannot find install manifest: /home/nayaradeandrade/ATR/build/_deps/curl-build/install_manifest.txt")
endif()

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
message(${CMAKE_INSTALL_PREFIX})

file(READ "/home/nayaradeandrade/ATR/build/_deps/curl-build/install_manifest.txt" _files)
string(REGEX REPLACE "\n" ";" _files "${_files}")
foreach(_file ${_files})
  message(STATUS "Uninstalling $ENV{DESTDIR}${_file}")
  if(IS_SYMLINK "$ENV{DESTDIR}${_file}" OR EXISTS "$ENV{DESTDIR}${_file}")
    execute_process(
      COMMAND "/usr/bin/cmake" -E remove "$ENV{DESTDIR}${_file}"
      RESULT_VARIABLE rm_retval
      OUTPUT_QUIET
      ERROR_QUIET
    )
    if(NOT "${rm_retval}" STREQUAL 0)
      message(FATAL_ERROR "Problem when removing $ENV{DESTDIR}${_file}")
    endif()
  else()
    message(STATUS "File $ENV{DESTDIR}${_file} does not exist.")
  endif()
endforeach()

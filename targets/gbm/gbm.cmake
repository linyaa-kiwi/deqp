#-------------------------------------------------------------------------
# drawElements CMake utilities
# ----------------------------
#
# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------

#
# GBM, a Generic Buffer Manager for Linux
#
# For more info, see the EGL_KHR_platform_gbm spefication [1] and the
# official gbm.h header [2].
#
# [1]: http://khronos.org/registry/egl/extensions/KHR/EGL_KHR_platform_gbm.txt
# [2]: https://cgit.freedesktop.org/mesa/mesa/tree/src/gbm/main/gbm.h?id=mesa-13.0.0
#
message("*** Using GBM")
set(DEQP_TARGET_NAME	"GBM")
set(DEQP_USE_GBM		ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GBM REQUIRED gbm)

set(DEQP_PLATFORM_LIBRARIES ${GBM_LDFLAGS})
include_directories(${GBM_INCLUDE_DIRS})

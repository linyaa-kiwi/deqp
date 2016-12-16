/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief GBM Platform
 *//*--------------------------------------------------------------------*/

#include "egluGLContextFactory.hpp"
#include "tcuGbmPlatform.hpp"
#include "tcuGbmNativeDisplayFactory.hpp"

namespace tcu
{
namespace gbm
{

Platform::Platform (void)
{
	m_nativeDisplayFactoryRegistry.registerFactory(new NativeDisplayFactory());
}

Platform::~Platform (void)
{
}

uint32_t
Platform::getGbmFormat (const eglw::Library& egl, eglw::EGLDisplay display, eglw::EGLConfig config)
{
	eglw::EGLint gbm_format = 0;
	TCU_CHECK(egl.getConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &gbm_format));
	return gbm_format;
}

} // gbm
} // tcu

tcu::Platform* createPlatform (void)
{
	return new tcu::gbm::Platform();
}

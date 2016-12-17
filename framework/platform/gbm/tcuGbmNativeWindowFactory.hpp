/*-----------------------------------------
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
 */

#pragma once

namespace tcu
{
namespace gbm
{

class NativeWindow;

class NativeWindowFactory final : public eglu::NativeWindowFactory
{
public:
							NativeWindowFactory	(void);
	eglu::NativeWindow*		createWindow		(eglu::NativeDisplay* nativeDisplay, const eglu::WindowParams& params) const override;
	eglu::NativeWindow*		createWindow		(eglu::NativeDisplay* nativeDisplay, eglw::EGLDisplay display, eglw::EGLConfig config, const eglw::EGLAttrib* attribList, const eglu::WindowParams& params) const override;

private:
							NativeWindowFactory	(const NativeWindowFactory&) = delete;
	NativeWindowFactory&	operator=			(const NativeWindowFactory&) = delete;
	eglu::NativeWindow*		createWindow		(eglu::NativeDisplay* nativeDisplay, int width, int height, uint32_t gbm_format) const;
};

} // gbm
} // tcu

/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2015 Intel Corporation
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
 * \brief DRM platform
 *//*--------------------------------------------------------------------*/

#include "tcuDrmPlatform.hpp"

#include <string>
#include <vector>

#include "deDynamicLibrary.hpp"
#include "deSTLUtil.hpp"

#include "tcuPlatform.hpp"

#include "gluPlatform.hpp"

#include "egluDefs.hpp"
#include "egluHeaderWrapper.hpp"
#include "egluUtil.hpp"
#include "egluStrUtil.hpp"

#include "glwInitFunctions.hpp"
#include "glwInitES20Direct.hpp"
#include "glwInitES30Direct.hpp"

using std::string;
using std::vector;

#if !defined(EGL_KHR_create_context)
	#define EGL_KHR_create_context 1
	#define EGL_CONTEXT_MAJOR_VERSION_KHR						0x3098
	#define EGL_CONTEXT_MINOR_VERSION_KHR						0x30FB
	#define EGL_CONTEXT_FLAGS_KHR								0x30FC
	#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR					0x30FD
	#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR	0x31BD
	#define EGL_NO_RESET_NOTIFICATION_KHR						0x31BE
	#define EGL_LOSE_CONTEXT_ON_RESET_KHR						0x31BF
	#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR					0x00000001
	#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR		0x00000002
	#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR			0x00000004
	#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR				0x00000001
	#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR	0x00000002
	#define EGL_OPENGL_ES3_BIT_KHR								0x00000040
#endif // EGL_KHR_create_context

// Default library names
#if !defined(DEQP_GLES2_LIBRARY_PATH)
#	define DEQP_GLES2_LIBRARY_PATH "libGLESv2.so"
#endif

#if !defined(DEQP_GLES3_LIBRARY_PATH)
#	define DEQP_GLES3_LIBRARY_PATH DEQP_GLES2_LIBRARY_PATH
#endif

#if !defined(DEQP_OPENGL_LIBRARY_PATH)
#	define DEQP_OPENGL_LIBRARY_PATH "libGL.so"
#endif

namespace tcu
{
namespace drm
{

bool isEGLExtensionSupported(EGLDisplay display, const std::string& extName)
{
	const vector<string> exts = eglu::getClientExtensions(display);
	return de::contains(exts.begin(), exts.end(), extName);
}

class GetProcFuncLoader : public glw::FunctionLoader
{
public:
	glw::GenericFuncType get (const char* name) const
	{
		return (glw::GenericFuncType)eglGetProcAddress(name);
	}
};

class DynamicFuncLoader : public glw::FunctionLoader
{
public:
	DynamicFuncLoader	(de::DynamicLibrary* library)
		: m_library(library)
	{
	}

	glw::GenericFuncType get (const char* name) const
	{
		return (glw::GenericFuncType)m_library->getFunction(name);
	}

private:
	de::DynamicLibrary*	m_library;
};

class Platform : public tcu::Platform, public glu::Platform
{
public:
										Platform				(void);
	const glu::Platform& 				getGLPlatform			(void) const { return *this; }
};

class ContextFactory : public glu::ContextFactory
{
public:
										ContextFactory			(void);
	glu::RenderContext*					createContext			(const glu::RenderConfig& config, const tcu::CommandLine& cmdLine) const;
};

class EglRenderContext : public glu::RenderContext
{
public:
										EglRenderContext		(const glu::RenderConfig& config, const tcu::CommandLine& cmdLine);
										~EglRenderContext		(void);

	glu::ContextType					getType					(void) const	{ return m_contextType; }
	const glw::Functions&				getFunctions			(void) const	{ return m_glFunctions; }
	const tcu::RenderTarget&			getRenderTarget			(void) const;
	void								postIterate				(void);

private:
	const glu::ContextType				m_contextType;
	::EGLDisplay						m_eglDisplay;
	::EGLContext						m_eglContext;
	de::DynamicLibrary*					m_glLibrary;
	glw::Functions						m_glFunctions;
};

Platform::Platform(void)
{
	m_contextFactoryRegistry.registerFactory(new ContextFactory());
}

ContextFactory::ContextFactory()
	: glu::ContextFactory("default", "EGL configless context")
{}

glu::RenderContext* ContextFactory::createContext(const glu::RenderConfig& config, const tcu::CommandLine& cmdLine) const
{
	return new EglRenderContext(config, cmdLine);
}

EglRenderContext::EglRenderContext(const glu::RenderConfig& config, const tcu::CommandLine& cmdLine)
	: m_contextType(config.type)
	, m_eglDisplay(EGL_NO_DISPLAY)
	, m_eglContext(EGL_NO_CONTEXT)
{
	const glu::ContextType& 	contextType 				= config.type;
	vector<EGLint> 				attribs;
	EGLint 						flags 						= 0;
	EGLint						eglMajorVersion;
	EGLint						eglMinorVersion;

	(void) cmdLine;

	m_eglDisplay = eglGetDisplay(NULL);
	EGLU_CHECK_MSG("eglGetDisplay()");
	if (m_eglDisplay == EGL_NO_DISPLAY)
		throw tcu::ResourceError("eglGetDisplay() failed");

	EGLU_CHECK_CALL(eglInitialize(m_eglDisplay, &eglMajorVersion, &eglMinorVersion));

	switch (config.surfaceType)
	{
		case glu::RenderConfig::SURFACETYPE_DONT_CARE:
		case glu::RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE:
		case glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC:
			break;
		case glu::RenderConfig::SURFACETYPE_WINDOW:
			throw tcu::NotSupportedError("DRM platform does not support --deqp-surface-type=window");
		case glu::RenderConfig::SURFACETYPE_LAST:
			TCU_CHECK_INTERNAL(false);
	}

	attribs.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
	attribs.push_back(contextType.getMajorVersion());
	attribs.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
	attribs.push_back(contextType.getMinorVersion());

	switch (contextType.getProfile())
	{
		case glu::PROFILE_ES:
			EGLU_CHECK_CALL(eglBindAPI(EGL_OPENGL_ES_API));
			break;
		case glu::PROFILE_CORE:
			EGLU_CHECK_CALL(eglBindAPI(EGL_OPENGL_API));
			attribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
			attribs.push_back(EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR);
			break;
		case glu::PROFILE_COMPATIBILITY:
			EGLU_CHECK_CALL(eglBindAPI(EGL_OPENGL_API));
			attribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
			attribs.push_back(EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR);
			break;
		case glu::PROFILE_LAST:
			TCU_CHECK_INTERNAL(false);
	}

	if ((contextType.getFlags() & glu::CONTEXT_DEBUG) != 0)
		flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

	if ((contextType.getFlags() & glu::CONTEXT_ROBUST) != 0)
		flags |= EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR;

	if ((contextType.getFlags() & glu::CONTEXT_FORWARD_COMPATIBLE) != 0)
		flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

	attribs.push_back(EGL_CONTEXT_FLAGS_KHR);
	attribs.push_back(flags);

	attribs.push_back(EGL_NONE);

	if (!isEGLExtensionSupported(m_eglDisplay, "EGL_MESA_configless_context")) {
		// FINISHME: On EGL implementations that don't support configless
		// contexts, create a throw-away config just for context creation.
		throw tcu::ResourceError("DRM platform requires EGL_MESA_configless_context");
	}

	m_eglContext = ::eglCreateContext(m_eglDisplay, static_cast<EGLConfig>(0), EGL_NO_CONTEXT, &attribs[0]);
	EGLU_CHECK_MSG("eglCreateContext()");
	if (!m_eglContext)
		throw tcu::ResourceError("eglCreateContext failed");

	EGLU_CHECK_CALL(::eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext));

	if ((eglMajorVersion == 1 && eglMinorVersion >= 5) ||
		isEGLExtensionSupported(m_eglDisplay, "EGL_KHR_get_all_proc_addresses") ||
		isEGLExtensionSupported(EGL_NO_DISPLAY, "EGL_KHR_client_get_all_proc_addresses"))
	{
		// Use eglGetProcAddress() for core functions
		GetProcFuncLoader funcLoader;
		glu::initCoreFunctions(&m_glFunctions, &funcLoader, contextType.getAPI());
	}
#if !defined(DEQP_GLES2_RUNTIME_LOAD)
	else if (contextType.getAPI() == glu::ApiType::es(2,0))
	{
		glw::initES20Direct(&m_glFunctions);
	}
#endif
#if !defined(DEQP_GLES3_RUNTIME_LOAD)
	else if (contextType.getAPI() == glu::ApiType::es(3,0))
	{
		glw::initES30Direct(&m_glFunctions);
	}
#endif
	else
	{
		const char* libraryPath = NULL;

		if (glu::isContextTypeES(contextType))
		{
			if (contextType.getMinorVersion() <= 2)
				libraryPath = DEQP_GLES2_LIBRARY_PATH;
			else
				libraryPath = DEQP_GLES3_LIBRARY_PATH;
		}
		else
		{
			libraryPath = DEQP_OPENGL_LIBRARY_PATH;
		}

		m_glLibrary = new de::DynamicLibrary(libraryPath);

		DynamicFuncLoader funcLoader(m_glLibrary);
		glu::initCoreFunctions(&m_glFunctions, &funcLoader, contextType.getAPI());
	}

	{
		GetProcFuncLoader extLoader;
		glu::initExtensionFunctions(&m_glFunctions, &extLoader, contextType.getAPI());
	}
}

EglRenderContext::~EglRenderContext(void)
{
	try
	{
		if (m_eglDisplay != EGL_NO_DISPLAY)
		{
			EGLU_CHECK_CALL(::eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

			if (m_eglContext != EGL_NO_CONTEXT)
				::eglDestroyContext(m_eglDisplay, m_eglContext);
		}

		EGLU_CHECK_CALL(eglTerminate(m_eglDisplay));
	}
	catch (...)
	{
	}
}

const tcu::RenderTarget& EglRenderContext::getRenderTarget(void) const
{
	throw tcu::InternalError("DRM platform cannot create EGL surfaces");
}

void EglRenderContext::postIterate(void)
{
	this->getFunctions().finish();
}

} // namespace drm
} // namespace tcu

tcu::Platform* createPlatform(void)
{
	return new tcu::drm::Platform();
}

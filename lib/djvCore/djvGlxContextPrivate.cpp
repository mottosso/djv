//------------------------------------------------------------------------------
// Copyright (c) 2004-2015 Darby Johnston
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions, and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions, and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the names of the copyright holders nor the names of any
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//------------------------------------------------------------------------------

//! \file djvGlxContextPrivate.cpp

#include <djvGlxContextPrivate.h>

#include <djvCoreContext.h>
#include <djvDebug.h>
#include <djvDebugLog.h>

#include <QCoreApplication>

#if defined(DJV_LINUX)
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif // DJV_LINUX

//------------------------------------------------------------------------------
// djvGlxContextPrivate
//------------------------------------------------------------------------------

struct djvGlxContextPrivate
{
#   if defined(DJV_LINUX)

    djvGlxContextPrivate() :
        display     (0),
        screen      (0),
        visuals     (0),
        visualsCount(0),
        colormap    (0),
        window      (0),
        context     (0)
    {}

    Display *     display;
    int           screen;
    XVisualInfo * visuals;
    int           visualsCount;
    Colormap      colormap;
    Window        window;
    GLXContext    context;

#   endif // DJV_LINUX
};

//------------------------------------------------------------------------------
// djvGlxContext
//------------------------------------------------------------------------------

djvGlxContext::djvGlxContext(djvCoreContext * context) throw (djvError) :
    djvOpenGlContext(context),
    _p(new djvGlxContextPrivate)
{
#   if defined(DJV_LINUX)

    //DJV_DEBUG("djvGlxContext::djvGlxContext");

    // Open the X display.
    
    DJV_LOG(context->debugLog(), "djvGlxContext", "Opening the X display...");

    _p->display = XOpenDisplay(NULL);

    if (! _p->display)
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_X_DISPLAY]);
    }
    
    _p->screen = DefaultScreen(_p->display);

    DJV_LOG(context->debugLog(), "djvGlxContext",
        QString("X screen: %1").arg(_p->screen));

    // Choose a visual.

    static const int depths [] =
    {
        32, 24
    };
    
    static const int depthsCount = sizeof(depths) / sizeof(depths[0]);

    XVisualInfo visualInfo;
    visualInfo.screen = _p->screen;
    
    for (int i = 0; i < depthsCount; ++i)
    {
        DJV_LOG(context->debugLog(), "djvGlxContext",
            QString("Checking for a X visual with a depth of %1...").
            arg(depths[i]));

        visualInfo.depth = depths[i];

        _p->visuals = XGetVisualInfo(
            _p->display,
            VisualScreenMask | VisualDepthMask,
            &visualInfo,
            &_p->visualsCount);
        
        if (_p->visuals && _p->visualsCount)
            break;
    }
    
    //DJV_DEBUG_PRINT("depth = " << visualInfo.depth);
    
    if (! _p->visuals || ! _p->visualsCount)
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_X_VISUALS]);
    }

    // Create the color map.
	
	DJV_LOG(context->debugLog(), "djvGlxContext", "Creating the color map...");

    _p->colormap = XCreateColormap(
        _p->display,
        RootWindow(_p->display, _p->screen),
        _p->visuals[0].visual,
        AllocNone);

    if (! _p->colormap)
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_X_COLORMAP]);
    }

    // Check for GLX support.

	DJV_LOG(context->debugLog(), "djvGlxContext", "Checking for GLX...");
	
    if (! glXQueryExtension(_p->display, 0, 0))
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_GLX]);
    }

    // Create a dummy window and OpenGL context for glewInit.

	DJV_LOG(context->debugLog(), "djvGlxContext", "Creating dummy window...");

    XSetWindowAttributes winAttrib;
    winAttrib.colormap     = _p->colormap;
    winAttrib.border_pixel = 0;

    _p->window = XCreateWindow(
        _p->display,
        RootWindow(_p->display, _p->screen),
        0, 0, 1, 1, 0,
        visualInfo.depth,
        InputOutput,
        _p->visuals[0].visual,
        CWColormap | CWBorderPixel,
        &winAttrib);

    if (! _p->window)
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_X_WINDOW]);
    }

    // Create the OpenGL context.

	DJV_LOG(context->debugLog(), "djvGlxContext", "Creating OpenGL context...");

    _p->context = glXCreateContext(
        _p->display,
        &_p->visuals[0],
        0,
        True);

    if (! _p->context)
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_CREATE_CONTEXT]);
    }

    // Bind the context.

    bind();

    // Initialize GLEW.
	
    DJV_LOG(context->debugLog(), "djvGlxContext", "Initializing GLEW...");

    GLint glError = glewInit();

    if (glError != GLEW_OK)
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_INIT_GLEW].arg(glError));
    }

    setVendor((const char *)glGetString(GL_VENDOR));
    setRenderer((const char *)glGetString(GL_RENDERER));
    setVersion((const char *)glGetString(GL_VERSION));

    //DJV_DEBUG_PRINT("vendor string = " << vendor());
    //DJV_DEBUG_PRINT("renderer string = " << renderer());
    //DJV_DEBUG_PRINT("version string = " << version());
    //DJV_DEBUG_PRINT("extensions = " <<
    //    (const char *)glGetString(GL_EXTENSIONS));
    //DJV_DEBUG_PRINT("glu version = " <<
    //    (const char *)gluGetString(GLU_VERSION));
    //DJV_DEBUG_PRINT("glu extensions = " <<
    //    (const char *)gluGetString(GLU_EXTENSIONS));

    DJV_LOG(context->debugLog(), "djvGlxContext",
        QString("GL vendor: \"%1\"").arg(vendor()));
    DJV_LOG(context->debugLog(), "djvGlxContext",
        QString("GL renderer: \"%1\"").arg(renderer()));
    DJV_LOG(context->debugLog(), "djvGlxContext",
        QString("GL version: \"%1\"").arg(version()));

    if (! GL_EXT_framebuffer_object)
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_NO_FBO]);
    }

#   endif // DJV_LINUX
}

djvGlxContext::~djvGlxContext()
{
#   if defined(DJV_LINUX)

    //DJV_DEBUG("djvGlxContext::~djvGlxContext");

    if (_p->context)
    {
        //DJV_DEBUG_PRINT("context");

        glXDestroyContext(_p->display, _p->context);
    }

    if (_p->window)
    {
        //DJV_DEBUG_PRINT("window");

        XDestroyWindow(_p->display, _p->window);
    }

    if (_p->colormap)
    {
        XFreeColormap(_p->display, _p->colormap);
    }

    if (_p->visuals)
    {
        XFree(_p->visuals);
    }

    if (_p->display)
    {
        //DJV_DEBUG_PRINT("display");

        XCloseDisplay(_p->display);
    }

#   endif // DJV_LINUX
    
    delete _p;
}

const QStringList & djvGlxContext::errorLabels()
{
    static const QStringList data = QStringList() <<
        qApp->translate("djvGlxContext", "Cannot open X display") <<
        qApp->translate("djvGlxContext", "No appropriate X visuals") <<
        qApp->translate("djvGlxContext", "Cannot create X colormap") <<
        qApp->translate("djvGlxContext", "No GLX extension") <<
        qApp->translate("djvGlxContext", "Cannot create an X window") <<
        qApp->translate("djvGlxContext", "Cannot create OpenGL context") <<
        qApp->translate("djvGlxContext", "Cannot initialize GLEW: #%1") <<
        qApp->translate("djvGlxContext", "No OpenGL FBO support") <<
        qApp->translate("djvGlxContext", "Invalid OpenGL context") <<
        qApp->translate("djvGlxContext", "Cannot bind OpenGL context");
    
    DJV_ASSERT(ERROR_COUNT == data.count());
    
    return data;
}

void djvGlxContext::bind() throw (djvError)
{
#   if defined(DJV_LINUX)

    if (! _p->context)
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_INVALID_CONTEXT]);
    }

    //DJV_DEBUG("djvGlxContext::bind");

    if (! glXMakeCurrent(_p->display, _p->window, _p->context))
    {
        throw djvError(
            "djvGlxContext",
            errorLabels()[ERROR_BIND_CONTEXT]);
    }

#   endif // DJV_LINUX
}

void djvGlxContext::unbind()
{
#   if defined(DJV_LINUX)

    //DJV_DEBUG("djvGlxContext::unbind");

    glXMakeCurrent(_p->display, 0, 0);

#   endif // DJV_LINUX
}


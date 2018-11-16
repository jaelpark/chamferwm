#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glad/glx.h>

#ifndef GLAD_IMPL_UTIL_C_
#define GLAD_IMPL_UTIL_C_

#ifdef _MSC_VER
#define GLAD_IMPL_UTIL_SSCANF sscanf_s
#else
#define GLAD_IMPL_UTIL_SSCANF sscanf
#endif

#endif /* GLAD_IMPL_UTIL_C_ */


int GLAD_GLX_VERSION_1_0 = 0;
int GLAD_GLX_VERSION_1_1 = 0;
int GLAD_GLX_VERSION_1_2 = 0;
int GLAD_GLX_VERSION_1_3 = 0;
int GLAD_GLX_VERSION_1_4 = 0;
int GLAD_GLX_EXT_texture_from_pixmap = 0;



PFNGLXBINDTEXIMAGEEXTPROC glad_glXBindTexImageEXT = NULL;
PFNGLXCHOOSEFBCONFIGPROC glad_glXChooseFBConfig = NULL;
PFNGLXCHOOSEVISUALPROC glad_glXChooseVisual = NULL;
PFNGLXCOPYCONTEXTPROC glad_glXCopyContext = NULL;
PFNGLXCREATECONTEXTPROC glad_glXCreateContext = NULL;
PFNGLXCREATEGLXPIXMAPPROC glad_glXCreateGLXPixmap = NULL;
PFNGLXCREATENEWCONTEXTPROC glad_glXCreateNewContext = NULL;
PFNGLXCREATEPBUFFERPROC glad_glXCreatePbuffer = NULL;
PFNGLXCREATEPIXMAPPROC glad_glXCreatePixmap = NULL;
PFNGLXCREATEWINDOWPROC glad_glXCreateWindow = NULL;
PFNGLXDESTROYCONTEXTPROC glad_glXDestroyContext = NULL;
PFNGLXDESTROYGLXPIXMAPPROC glad_glXDestroyGLXPixmap = NULL;
PFNGLXDESTROYPBUFFERPROC glad_glXDestroyPbuffer = NULL;
PFNGLXDESTROYPIXMAPPROC glad_glXDestroyPixmap = NULL;
PFNGLXDESTROYWINDOWPROC glad_glXDestroyWindow = NULL;
PFNGLXGETCLIENTSTRINGPROC glad_glXGetClientString = NULL;
PFNGLXGETCONFIGPROC glad_glXGetConfig = NULL;
PFNGLXGETCURRENTCONTEXTPROC glad_glXGetCurrentContext = NULL;
PFNGLXGETCURRENTDISPLAYPROC glad_glXGetCurrentDisplay = NULL;
PFNGLXGETCURRENTDRAWABLEPROC glad_glXGetCurrentDrawable = NULL;
PFNGLXGETCURRENTREADDRAWABLEPROC glad_glXGetCurrentReadDrawable = NULL;
PFNGLXGETFBCONFIGATTRIBPROC glad_glXGetFBConfigAttrib = NULL;
PFNGLXGETFBCONFIGSPROC glad_glXGetFBConfigs = NULL;
PFNGLXGETPROCADDRESSPROC glad_glXGetProcAddress = NULL;
PFNGLXGETSELECTEDEVENTPROC glad_glXGetSelectedEvent = NULL;
PFNGLXGETVISUALFROMFBCONFIGPROC glad_glXGetVisualFromFBConfig = NULL;
PFNGLXISDIRECTPROC glad_glXIsDirect = NULL;
PFNGLXMAKECONTEXTCURRENTPROC glad_glXMakeContextCurrent = NULL;
PFNGLXMAKECURRENTPROC glad_glXMakeCurrent = NULL;
PFNGLXQUERYCONTEXTPROC glad_glXQueryContext = NULL;
PFNGLXQUERYDRAWABLEPROC glad_glXQueryDrawable = NULL;
PFNGLXQUERYEXTENSIONPROC glad_glXQueryExtension = NULL;
PFNGLXQUERYEXTENSIONSSTRINGPROC glad_glXQueryExtensionsString = NULL;
PFNGLXQUERYSERVERSTRINGPROC glad_glXQueryServerString = NULL;
PFNGLXQUERYVERSIONPROC glad_glXQueryVersion = NULL;
PFNGLXRELEASETEXIMAGEEXTPROC glad_glXReleaseTexImageEXT = NULL;
PFNGLXSELECTEVENTPROC glad_glXSelectEvent = NULL;
PFNGLXSWAPBUFFERSPROC glad_glXSwapBuffers = NULL;
PFNGLXUSEXFONTPROC glad_glXUseXFont = NULL;
PFNGLXWAITGLPROC glad_glXWaitGL = NULL;
PFNGLXWAITXPROC glad_glXWaitX = NULL;


static void glad_glx_load_GLX_VERSION_1_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GLX_VERSION_1_0) return;
    glXChooseVisual = (PFNGLXCHOOSEVISUALPROC) load("glXChooseVisual", userptr);
    glXCopyContext = (PFNGLXCOPYCONTEXTPROC) load("glXCopyContext", userptr);
    glXCreateContext = (PFNGLXCREATECONTEXTPROC) load("glXCreateContext", userptr);
    glXCreateGLXPixmap = (PFNGLXCREATEGLXPIXMAPPROC) load("glXCreateGLXPixmap", userptr);
    glXDestroyContext = (PFNGLXDESTROYCONTEXTPROC) load("glXDestroyContext", userptr);
    glXDestroyGLXPixmap = (PFNGLXDESTROYGLXPIXMAPPROC) load("glXDestroyGLXPixmap", userptr);
    glXGetConfig = (PFNGLXGETCONFIGPROC) load("glXGetConfig", userptr);
    glXGetCurrentContext = (PFNGLXGETCURRENTCONTEXTPROC) load("glXGetCurrentContext", userptr);
    glXGetCurrentDrawable = (PFNGLXGETCURRENTDRAWABLEPROC) load("glXGetCurrentDrawable", userptr);
    glXIsDirect = (PFNGLXISDIRECTPROC) load("glXIsDirect", userptr);
    glXMakeCurrent = (PFNGLXMAKECURRENTPROC) load("glXMakeCurrent", userptr);
    glXQueryExtension = (PFNGLXQUERYEXTENSIONPROC) load("glXQueryExtension", userptr);
    glXQueryVersion = (PFNGLXQUERYVERSIONPROC) load("glXQueryVersion", userptr);
    glXSwapBuffers = (PFNGLXSWAPBUFFERSPROC) load("glXSwapBuffers", userptr);
    glXUseXFont = (PFNGLXUSEXFONTPROC) load("glXUseXFont", userptr);
    glXWaitGL = (PFNGLXWAITGLPROC) load("glXWaitGL", userptr);
    glXWaitX = (PFNGLXWAITXPROC) load("glXWaitX", userptr);
}
static void glad_glx_load_GLX_VERSION_1_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GLX_VERSION_1_1) return;
    glXGetClientString = (PFNGLXGETCLIENTSTRINGPROC) load("glXGetClientString", userptr);
    glXQueryExtensionsString = (PFNGLXQUERYEXTENSIONSSTRINGPROC) load("glXQueryExtensionsString", userptr);
    glXQueryServerString = (PFNGLXQUERYSERVERSTRINGPROC) load("glXQueryServerString", userptr);
}
static void glad_glx_load_GLX_VERSION_1_2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GLX_VERSION_1_2) return;
    glXGetCurrentDisplay = (PFNGLXGETCURRENTDISPLAYPROC) load("glXGetCurrentDisplay", userptr);
}
static void glad_glx_load_GLX_VERSION_1_3( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GLX_VERSION_1_3) return;
    glXChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC) load("glXChooseFBConfig", userptr);
    glXCreateNewContext = (PFNGLXCREATENEWCONTEXTPROC) load("glXCreateNewContext", userptr);
    glXCreatePbuffer = (PFNGLXCREATEPBUFFERPROC) load("glXCreatePbuffer", userptr);
    glXCreatePixmap = (PFNGLXCREATEPIXMAPPROC) load("glXCreatePixmap", userptr);
    glXCreateWindow = (PFNGLXCREATEWINDOWPROC) load("glXCreateWindow", userptr);
    glXDestroyPbuffer = (PFNGLXDESTROYPBUFFERPROC) load("glXDestroyPbuffer", userptr);
    glXDestroyPixmap = (PFNGLXDESTROYPIXMAPPROC) load("glXDestroyPixmap", userptr);
    glXDestroyWindow = (PFNGLXDESTROYWINDOWPROC) load("glXDestroyWindow", userptr);
    glXGetCurrentReadDrawable = (PFNGLXGETCURRENTREADDRAWABLEPROC) load("glXGetCurrentReadDrawable", userptr);
    glXGetFBConfigAttrib = (PFNGLXGETFBCONFIGATTRIBPROC) load("glXGetFBConfigAttrib", userptr);
    glXGetFBConfigs = (PFNGLXGETFBCONFIGSPROC) load("glXGetFBConfigs", userptr);
    glXGetSelectedEvent = (PFNGLXGETSELECTEDEVENTPROC) load("glXGetSelectedEvent", userptr);
    glXGetVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC) load("glXGetVisualFromFBConfig", userptr);
    glXMakeContextCurrent = (PFNGLXMAKECONTEXTCURRENTPROC) load("glXMakeContextCurrent", userptr);
    glXQueryContext = (PFNGLXQUERYCONTEXTPROC) load("glXQueryContext", userptr);
    glXQueryDrawable = (PFNGLXQUERYDRAWABLEPROC) load("glXQueryDrawable", userptr);
    glXSelectEvent = (PFNGLXSELECTEVENTPROC) load("glXSelectEvent", userptr);
}
static void glad_glx_load_GLX_VERSION_1_4( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GLX_VERSION_1_4) return;
    glXGetProcAddress = (PFNGLXGETPROCADDRESSPROC) load("glXGetProcAddress", userptr);
}
static void glad_glx_load_GLX_EXT_texture_from_pixmap( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GLX_EXT_texture_from_pixmap) return;
    glXBindTexImageEXT = (PFNGLXBINDTEXIMAGEEXTPROC) load("glXBindTexImageEXT", userptr);
    glXReleaseTexImageEXT = (PFNGLXRELEASETEXIMAGEEXTPROC) load("glXReleaseTexImageEXT", userptr);
}



static int glad_glx_has_extension(Display *display, int screen, const char *ext) {
#ifndef GLX_VERSION_1_1
    (void) display;
    (void) screen;
    (void) ext;
#else
    const char *terminator;
    const char *loc;
    const char *extensions;

    if (glXQueryExtensionsString == NULL) {
        return 0;
    }

    extensions = glXQueryExtensionsString(display, screen);

    if(extensions == NULL || ext == NULL) {
        return 0;
    }

    while(1) {
        loc = strstr(extensions, ext);
        if(loc == NULL)
            break;

        terminator = loc + strlen(ext);
        if((loc == extensions || *(loc - 1) == ' ') &&
            (*terminator == ' ' || *terminator == '\0')) {
            return 1;
        }
        extensions = terminator;
    }
#endif

    return 0;
}

static GLADapiproc glad_glx_get_proc_from_userptr(const char* name, void *userptr) {
    return (GLAD_GNUC_EXTENSION (GLADapiproc (*)(const char *name)) userptr)(name);
}

static int glad_glx_find_extensions(Display *display, int screen) {
    GLAD_GLX_EXT_texture_from_pixmap = glad_glx_has_extension(display, screen, "GLX_EXT_texture_from_pixmap");
    return 1;
}

static int glad_glx_find_core_glx(Display **display, int *screen) {
    int major = 0, minor = 0;
    if(*display == NULL) {
#ifdef GLAD_GLX_NO_X11
        (void) screen;
        return 0;
#else
        *display = XOpenDisplay(0);
        if (*display == NULL) {
            return 0;
        }
        *screen = XScreenNumberOfScreen(XDefaultScreenOfDisplay(*display));
#endif
    }
    glXQueryVersion(*display, &major, &minor);
    GLAD_GLX_VERSION_1_0 = (major == 1 && minor >= 0) || major > 1;
    GLAD_GLX_VERSION_1_1 = (major == 1 && minor >= 1) || major > 1;
    GLAD_GLX_VERSION_1_2 = (major == 1 && minor >= 2) || major > 1;
    GLAD_GLX_VERSION_1_3 = (major == 1 && minor >= 3) || major > 1;
    GLAD_GLX_VERSION_1_4 = (major == 1 && minor >= 4) || major > 1;
    return GLAD_MAKE_VERSION(major, minor);
}

int gladLoadGLXUserPtr(Display *display, int screen, GLADuserptrloadfunc load, void *userptr) {
    int version;
    glXQueryVersion = (PFNGLXQUERYVERSIONPROC) load("glXQueryVersion", userptr);
    if(glXQueryVersion == NULL) return 0;
    version = glad_glx_find_core_glx(&display, &screen);

    glad_glx_load_GLX_VERSION_1_0(load, userptr);
    glad_glx_load_GLX_VERSION_1_1(load, userptr);
    glad_glx_load_GLX_VERSION_1_2(load, userptr);
    glad_glx_load_GLX_VERSION_1_3(load, userptr);
    glad_glx_load_GLX_VERSION_1_4(load, userptr);

    if (!glad_glx_find_extensions(display, screen)) return 0;
    glad_glx_load_GLX_EXT_texture_from_pixmap(load, userptr);

    return version;
}

int gladLoadGLX(Display *display, int screen, GLADloadfunc load) {
    return gladLoadGLXUserPtr(display, screen, glad_glx_get_proc_from_userptr, GLAD_GNUC_EXTENSION (void*) load);
}


#ifdef GLAD_GLX

#ifndef GLAD_LOADER_LIBRARY_C_
#define GLAD_LOADER_LIBRARY_C_

#include <stddef.h>
#include <stdlib.h>

#if GLAD_PLATFORM_WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif


static void* glad_get_dlopen_handle(const char *lib_names[], int length) {
    void *handle = NULL;
    int i;

    for (i = 0; i < length; ++i) {
#if GLAD_PLATFORM_WIN32
  #if GLAD_PLATFORM_UWP
        size_t buffer_size = (strlen(lib_names[i]) + 1) * sizeof(WCHAR);
        LPWSTR buffer = (LPWSTR) malloc(buffer_size);
        if (buffer != NULL) {
            int ret = MultiByteToWideChar(CP_ACP, 0, lib_names[i], -1, buffer, buffer_size);
            if (ret != 0) {
                handle = (void*) LoadPackagedLibrary(buffer, 0);
            }
            free((void*) buffer);
        }
  #else
        handle = (void*) LoadLibraryA(lib_names[i]);
  #endif
#else
        handle = dlopen(lib_names[i], RTLD_LAZY | RTLD_LOCAL);
#endif
        if (handle != NULL) {
            return handle;
        }
    }

    return NULL;
}

static void glad_close_dlopen_handle(void* handle) {
    if (handle != NULL) {
#if GLAD_PLATFORM_WIN32
        FreeLibrary((HMODULE) handle);
#else
        dlclose(handle);
#endif
    }
}

static GLADapiproc glad_dlsym_handle(void* handle, const char *name) {
    if (handle == NULL) {
        return NULL;
    }

#if GLAD_PLATFORM_WIN32
    return (GLADapiproc) GetProcAddress((HMODULE) handle, name);
#else
    return GLAD_GNUC_EXTENSION (GLADapiproc) dlsym(handle, name);
#endif
}

#endif /* GLAD_LOADER_LIBRARY_C_ */

typedef void* (GLAD_API_PTR *GLADglxprocaddrfunc)(const char*);

static GLADapiproc glad_glx_get_proc(const char *name, void *userptr) {
    return GLAD_GNUC_EXTENSION ((GLADapiproc (*)(const char *name)) userptr)(name);
}

static void* _glx_handle;

int gladLoaderLoadGLX(Display *display, int screen) {
    static const char *NAMES[] = {
#if defined __CYGWIN__
        "libGL-1.so",
#endif
        "libGL.so.1",
        "libGL.so"
    };

    int version = 0;
    int did_load = 0;
    GLADglxprocaddrfunc loader;

    if (_glx_handle == NULL) {
        _glx_handle = glad_get_dlopen_handle(NAMES, sizeof(NAMES) / sizeof(NAMES[0]));
        did_load = _glx_handle != NULL;
    }

    if (_glx_handle != NULL) {
        loader = (GLADglxprocaddrfunc) glad_dlsym_handle(_glx_handle, "glXGetProcAddressARB");
        if (loader != NULL) {
            version = gladLoadGLXUserPtr(display, screen, glad_glx_get_proc, GLAD_GNUC_EXTENSION (void*) loader);
        }

        if (!version && did_load) {
            glad_close_dlopen_handle(_glx_handle);
            _glx_handle = NULL;
        }
    }

    return version;
}

void gladLoaderUnloadGLX() {
    if (_glx_handle != NULL) {
        glad_close_dlopen_handle(_glx_handle);
        _glx_handle = NULL;
    }
}

#endif /* GLAD_GLX */

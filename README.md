# chamferwm
A tiling window manager with Vulkan based compositor.

[![Preview](http://users.jyu.fi/~jaelpark/gitres/scrot-chamfer-1.jpg)](http://users.jyu.fi/~jaelpark/gitres/scrot-chamfer.png)

## Prerequisites

 - XCB libraries
 - Vulkan SDK
 - Python 3.6+ and boost modules
 - [shaderc Shader compiler](https://github.com/google/shaderc)

## Features
Window manager:

 - Dynamic horizontal and vertical tiling with gaps and stacking
 - Specify container size restrictions, overlap when necessary
 - Resize and translate individual containers in their place while keeping the surrounding layout
 - Floating containers and automatic dialog, dock, widget etc. handling
 - Yank and paste containers to move them within the tree hierarchy along with typical move operations
 - Configuration with python, scriptable behaviour with client and container specific callbacks
 - Fully keyboard controllable

Compositor:
 
 - Vulkan renderer
 - Arbitrary window decorations and borders with user supplied shaders
 - Per-client materials
 - Optional, alternatively use any other external compositor

## Installing
Currently a PKGBUILD is available for testing purposes. Install from [AUR](https://aur.archlinux.org/packages/chamfer-git/), or run meson to build manually. The package from AUR will install a default configuration and the precompiled shaders to /usr/share/chamfer/. Copy the configuration to another location to make modifications. Once ready, put the following line to your .xinitrc:

```sh
exec chamfer --config=/usr/share/chamfer/config/config.py --shader-path=/usr/share/chamfer/shaders/
```

When multiple rendering devices are available, make the choice with `--device-index=n`, where `n` is the zero-based index of the device (default = 0). Launch Xorg with `startx`.

To run the WM without the integrated compositor, use

```sh
exec chamfer --config=/usr/share/chamfer/config/config.py -n
```

In this case, any other external compositor may be used.

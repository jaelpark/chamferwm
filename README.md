# chamferwm
A tiling window manager with Vulkan based compositor. [Documentation](https://jaelpark.github.io/chamferwm-docs/)

> **Note**
> Experimental support for DMA-buf import has been added. This can be enabled with `--memory-import-mode=0`, and should bring considerable performance improvements. Be sure to use the latest drivers while testing. The feature might not work on all setups yet.

[![Preview](http://users.jyu.fi/~jaelpark/gitres/scrot-chamfer-1.jpg)](http://users.jyu.fi/~jaelpark/gitres/scrot-chamfer.png)

## Prerequisites

 - XCB libraries
 - Vulkan SDK
 - glm
 - Python 3.6+ and boost modules
 - python-xlib
 - [shaderc Shader compiler](https://github.com/google/shaderc)
 - FreeType2
 - HarfBuzz
 - fontconfig

## Features
Window manager:

 - Dynamic horizontal and vertical tiling with gaps and stacking
 - Specify container size restrictions, overlap when necessary
 - Resize and translate individual containers in their place while keeping the surrounding layout
 - Floating containers and automatic dialog, dock, widget etc. handling
 - Multiple named workspaces
 - Yank and paste containers to move them within the tree hierarchy along with typical move operations
 - Configuration with python, scriptable behaviour with client and container specific callbacks
 - Fully keyboard controllable

Compositor:
 
 - Vulkan renderer
 - Arbitrary window decorations and borders with user supplied shaders
 - Per-client materials
 - Automatically disable for fullscreen applications
 - Optional, alternatively use any other external compositor
 - Standalone compositor mode (WIP) to be used in combination with any other window manager

## Installing
Currently a PKGBUILD is available for testing purposes. Install from [AUR](https://aur.archlinux.org/packages/chamfer-git/), or run meson to build manually. The package from AUR will install a default configuration and the precompiled shaders to /usr/share/chamfer/. Copy the configuration to another location to make modifications. Once ready, put the following line to your .xinitrc:

```sh
exec chamfer --config=/usr/share/chamfer/config/config.py --shader-path=/usr/share/chamfer/shaders/
```

When multiple rendering devices are available, make the choice with `--device-index=n`, where `n` is the zero-based index of the device (default = 0). The ordering follows the list reported by `vulkaninfo` utility. Launch Xorg with `startx`.

 - To automatically let fullscreen applications bypass the compositor, use `--unredir-on-fullscreen`. A dedicated workspace with the compositor always disabled is provided, and by default bound to Alt+0.
 - NVIDIA users may have to add ``Option "AllowSHMPixmaps" "1"`` to their Xorg configuration, if shared memory import is used (`--memory-import-mode=1`, default).
 - For compositor compatibility mode if encountering any issues, use `--no-host-memory-import` and `--no-scissoring`.

To run the WM without the integrated compositor, use

```sh
exec chamfer --config=/usr/share/chamfer/config/config.py -n
```

In this case, any other external compositor may be used.

A standalone compositor mode is in the works, and can be enabled with `-C`. The standalone compositor mode is not ready for daily use, and will most likely fail to function properly.


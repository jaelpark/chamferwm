# chamferwm
A tiling window manager with Vulkan based compositor. Vulkan compositor for external X11 window managers. [Documentation](https://jaelpark.github.io/chamferwm-docs/)

> **Note**
> Support for DMA-buf import has been added, and *has been made default*. This can be disabled with `--memory-import-mode=1`, which will revert to host pointer import method.

> **Note**
> Initial support for standalone compositor feature has been merged. The program can now be configured to run the Vulkan compositor on external X11 window managers. See below for instructions.

[![Preview](http://users.jyu.fi/~jaelpark/gitres/scrot-chamfer-1.jpg)](http://users.jyu.fi/~jaelpark/gitres/scrot-chamfer.png)

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
 - Standalone compositor mode (beta) to be used in combination with any other window manager

## Installing
##### Prerequisites

 - XCB libraries
 - Vulkan SDK
 - glm
 - Python 3.6+ and boost modules
 - python-xlib
 - [shaderc Shader compiler](https://github.com/google/shaderc)
 - FreeType2
 - HarfBuzz
 - fontconfig

##### General setup

Currently a PKGBUILD is available for testing purposes. Install from [AUR](https://aur.archlinux.org/packages/chamfer-git/), or run meson to build manually. The package from AUR will install a default configuration and the precompiled shaders to /usr/share/chamfer/. Copy the configuration to another location to make modifications. Put the following line to your .xinitrc:

```sh
exec chamfer --config=/usr/share/chamfer/config/config.py --shader-path=/usr/share/chamfer/shaders/
```

When multiple rendering devices are available, make the choice with `--device-index=n`, where `n` is the zero-based index of the device (default = 0). The ordering follows the list reported by `vulkaninfo` utility. Launch Xorg with `startx`.

 - To automatically let fullscreen applications bypass the compositor, use `--unredir-on-fullscreen`. A dedicated workspace with the compositor always disabled is provided, and by default bound to Meta+0.
 - NVIDIA users may have to add ``Option "AllowSHMPixmaps" "1"`` to their Xorg configuration, if shared memory import is used (`--memory-import-mode=1`).
 - For compositor compatibility mode if encountering any issues, use `--memory-import-mode=1` or `2` and/or `--no-scissoring`.

To run the WM without the integrated compositor, use

```sh
exec chamfer --config=/usr/share/chamfer/config/config.py -n
```

In this case, any other external compositor may be used.

##### Compositor mode for other WMs

The Vulkan compositor can be used together with external X11 window managers. To launch the program in this mode, run

```sh
chamfer -C --config=/usr/share/chamfer/config/config.py --shader-path=/usr/share/chamfer/shaders/
```

##### Style and decorations

The default "demo" style draws rounded borders around the windows, as well as shadows. The style can be changed by [editing the shaders section of the config](https://github.com/jaelpark/chamferwm/blob/5ed340fece89eda381f25a8354a8c2dc18dd8787/config/config.py#L53-L71). Changing the style attributes (color, borders etc.) of the stock shaders is at the moment only possible by editing the values in the fragment shader itself, and then rebuilding. For a complete custom look, the shaders can be replaced entirely. Shaders can be specified on per-window basis, enabling some interesting looks for your desktop.


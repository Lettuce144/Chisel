# Chisel

## Dependencies ##

- [Meson](https://mesonbuild.com/) is required to build.

Handled by Meson (via pkgconfig or WrapDB):
- [fmt](https://fmt.dev/)
- [SDL](https://www.libsdl.org/)
- [GLM](https://github.com/g-truc/glm)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) (for now)

Included as Git submodules:
- [bgfx](https://github.com/bkaradzic/bgfx) (with [bx](https://github.com/bkaradzic/bx) and [bimg](https://github.com/bkaradzic/bimg))

Already included in repo:
- [VDF Parser](https://github.com/TinyTinni/ValveFileVDF)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
- [stb](https://github.com/nothings/stb) image loader and writer
- [imgui_impl_bgfx.cpp](https://gist.github.com/RichardGale/6e2b74bc42b3005e08397236e4be0fd0)
- [Material Design Icons Font](https://materialdesignicons.com/)

## Building ##

After installing all dependencies, use the tasks in `.vscode/tasks.json` to quickly setup and build. 
Or use the following commands to 

To setup on windows (run once):
```
meson --buildtype release --backend vs2022 build-msvc-x64
```
MAKE SURE YOU HAVE AN UPDATED WINDOWS SDK!!

To build:
```
Open the sln in the build-msvc-x64 folder and build for x64 Release, copy the core folder from the root dir to your build-msvc-x64/src folder!
```


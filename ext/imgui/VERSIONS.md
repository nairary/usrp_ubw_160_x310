# Bundled GUI dependencies

- Dear ImGui **docking**: https://github.com/ocornut/imgui/tree/036bf939b6f8d74ad76bcf926b757c56e68c54ff
- ImPlot: https://github.com/epezent/implot/tree/d65a2bef53d32502407de3a4be80f191e2f412d7

Headers are restored from these revisions. Bundled implementations match those
revisions after normalizing whitespace and the project's `imgui/` include prefix.
Backend headers are flattened alongside the existing backend implementations.
Licenses are in imgui-LICENSE.txt and implot-LICENSE.txt.

CMake compiles the bundled implementations once. Do not additionally link vcpkg
imgui/implot or compile their upstream sources. The ImGui OpenGL3 backend uses
its own imgui_impl_opengl3_loader.h; the unused root glad.c is excluded.

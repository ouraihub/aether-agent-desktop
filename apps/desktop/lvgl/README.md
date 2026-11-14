# LVGL Frontend preview

This is an isolated Windows-only visual Frontend adapter slice. It is not the
production Aether Frontend and has no Backend Client, Core, Runtime, database,
or business DTO dependency.

## Build

From a configured Windows MSVC build directory:

```bat
cmake -S . -B build\lvgl-msvc -G Ninja -DAETHER_BUILD_LVGL_FRONTEND=ON
cmake --build build\lvgl-msvc --target aether_frontend_lvgl -j 6
```

Run the executable from the MSVC environment established by
`scripts\msvc-env.bat`. It opens the LVGL Windows application-mode window and
prints a `preview` marker. The window is a 1040x700 Autorail-aligned model-
service settings page: 40px top bar, 192px settings navigation, 208px provider
rail, and provider details in the exact order used by
`docs/design/ui/providers-reference.md`.

It demonstrates the three default providers, empty API-key state, endpoint and
route fields, model search/tools/empty state, Chinese text, resize, and close.
All controls are visual-only and explicitly not connected to Backend. It does
not report provider detection, model synchronization, saving, or credentials.

LVGL 9.6.0 is vendored under `third_party/lvgl/`; the release source tree is
unchanged. The project-owned CMake wrapper and configuration are separate.
This preview does not prove Windows UIA, full Chinese IME composition,
macOS/Linux operation, Backend event delivery, or production shutdown. Those
remain UI-01/UI-02 acceptance work.

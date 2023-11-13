@echo off
setlocal EnableDelayedExpansion

set executable_name="fmt.exe"

if "%1"=="install" (
    set executable_name="C:/hd-tools/fmt.exe"

    set defines= /DGN_USE_OPENGL /DGN_PLATFORM_WINDOWS /DGN_RELEASE /DNDEBUG /DGN_COMPILER_MSVC /DGN_CUSTOM_MAIN
    set compile_flags= /MT /O2 /EHsc /std:c++17 /cgthreads8 /MP7 /GL
    set link_flags= /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrtd.lib /LTCG

    echo INSTALLING EXECUTABLE
) else if "%1"=="release" (
    set defines= /DGN_USE_OPENGL /DGN_PLATFORM_WINDOWS /DGN_RELEASE /DNDEBUG /DGN_COMPILER_MSVC /DGN_CUSTOM_MAIN
    set compile_flags= /MT /O2 /EHsc /std:c++17 /cgthreads8 /MP7 /GL
    set link_flags= /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrtd.lib /LTCG

    echo BUILDING RELEASE EXECUTABLE
) else (
    set defines= /DGN_USE_OPENGL /DGN_PLATFORM_WINDOWS /DGN_DEBUG /DGN_COMPILER_MSVC /DGN_CUSTOM_MAIN
    set compile_flags= /MTd /Zi /EHsc /std:c++17 /cgthreads8 /MP7 /GL
    set link_flags= /DEBUG /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrtd.lib /LTCG

    echo BUILDING DEBUG EXECUTABLE
)

set includes= /I src ^
              /I dependencies\glad\include   ^
              /I dependencies\wglext\include ^
              /I dependencies\stb\include    ^
              /I dependencies\miniz\include

set libs= shell32.lib                     ^
          user32.lib                      ^
          gdi32.lib                       ^
          openGL32.lib                    ^
          msvcrt.lib                      ^
          comdlg32.lib                    ^
          Xaudio2.lib                     ^
          Ole32.lib                       ^
          dependencies\glad\lib\glad.lib  ^
          dependencies\stb\lib\stb.lib    ^
          dependencies\miniz\lib\miniz.lib

rem Source
rem cl %compile_flags% /c src/engine/*.cpp %defines% %includes%               &^
rem cl %compile_flags% /c src/audio/*.cpp %defines% %includes%                &^

cl %compile_flags% /c src/serialization/json/*.cpp %defines% %includes%   &^
cl %compile_flags% /c src/serialization/slz/*.cpp %defines% %includes%    &^
cl %compile_flags% /c src/serialization/yaml/*.cpp %defines% %includes%   &^
cl %compile_flags% /c src/serialization/binary/*.cpp %defines% %includes% &^
cl %compile_flags% /c src/fileio/*.cpp %defines% %includes%               &^
cl %compile_flags% /c src/graphics/*.cpp %defines% %includes%             &^
cl %compile_flags% /c src/platform/*.cpp %defines% %includes%             &^
cl %compile_flags% /c src/application/*.cpp %defines% %includes%          &^
cl %compile_flags% /c src/core/*.cpp %defines% %includes%                 &^
cl %compile_flags% /c src/math/*.cpp %defines% %includes%                 &^
cl %compile_flags% /c src/formatter/*.cpp %defines% %includes%            &^
cl %compile_flags% /c src/main.cpp %defines% %includes%

link *.obj %libs% /OUT:!executable_name! %link_flags%

rem Remove intermediate files
del *.obj *.exp *.lib
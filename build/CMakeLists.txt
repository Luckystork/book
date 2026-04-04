cmake_minimum_required(VERSION 3.10)
project(ZeroPoint LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# ZeroPoint v4.3.0 — MSVC build configuration
#
# Build from Developer Command Prompt for VS 2022:
#   cd build
#   cmake .. -G "Visual Studio 17 2022" -A x64
#   cmake --build . --config Release
#
# Requirements:
#   - Windows 10 SDK (10.0.19041.0+)
#   - Microsoft.Web.WebView2 NuGet package (headers + WebView2Loader.lib)
#   - Edge WebView2 Runtime installed on target machine
#
# Optional NuGet install (from build dir):
#   nuget install Microsoft.Web.WebView2 -OutputDirectory packages
#   Then add packages/Microsoft.Web.WebView2.*/build/native/include to include
#   and packages/Microsoft.Web.WebView2.*/build/native/x64 to library paths.
# ==============================================================================

set(AETHER_SRC "${CMAKE_SOURCE_DIR}/../Aether_Core/src")
set(AETHER_INC "${CMAKE_SOURCE_DIR}/../Aether_Core/include")

add_executable(ZeroPoint WIN32
    ${AETHER_SRC}/main.cpp
    ${AETHER_SRC}/VirtualEnv.cpp
    ${AETHER_SRC}/Config.cpp
    ${AETHER_SRC}/Stealth.cpp
    ${AETHER_SRC}/CDPExtractor.cpp
    ${AETHER_SRC}/RDPWrapper.cpp
)

target_include_directories(ZeroPoint PRIVATE ${AETHER_INC})

target_link_libraries(ZeroPoint PRIVATE
    winhttp
    dwmapi
    comctl32
    gdi32
    msimg32
    gdiplus
    ole32
    shlwapi
    ws2_32
    wtsapi32
    ntdll
    crypt32
    advapi32
    WebView2Loader
)

# Suppress MSVC warnings for legacy Win32 API usage
target_compile_definitions(ZeroPoint PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

# MSVC-specific: enable optimizations in Release, debug info in Debug
if(MSVC)
    target_compile_options(ZeroPoint PRIVATE
        /W3           # Warning level 3
        /MP           # Multi-process compilation
        /Zc:inline    # Remove unreferenced functions
    )
    # Release: full optimization + link-time code generation
    target_compile_options(ZeroPoint PRIVATE $<$<CONFIG:Release>:/O2 /GL>)
    target_link_options(ZeroPoint PRIVATE $<$<CONFIG:Release>:/LTCG>)
endif()

# Embed application manifest for:
#   - DPI awareness (per-monitor v2)
#   - Admin elevation (requireAdministrator)
#   - UTF-8 code page
# Create the manifest file at configure time
file(WRITE "${CMAKE_BINARY_DIR}/ZeroPoint.manifest"
"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>
<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">
  <assemblyIdentity version=\"4.3.0.0\" name=\"ZeroPoint\" type=\"win32\"/>
  <trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level=\"requireAdministrator\" uiAccess=\"false\"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
  <application xmlns=\"urn:schemas-microsoft-com:asm.v3\">
    <windowsSettings>
      <dpiAwareness xmlns=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">PerMonitorV2</dpiAwareness>
      <dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true/pm</dpiAware>
      <activeCodePage xmlns=\"http://schemas.microsoft.com/SMI/2019/WindowsSettings\">UTF-8</activeCodePage>
    </windowsSettings>
  </application>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\"
        version=\"6.0.0.0\" processorArchitecture=\"*\"
        publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>
    </dependentAssembly>
  </dependency>
</assembly>
")

if(MSVC)
    target_sources(ZeroPoint PRIVATE "${CMAKE_BINARY_DIR}/ZeroPoint.manifest")
    set_source_files_properties("${CMAKE_BINARY_DIR}/ZeroPoint.manifest"
        PROPERTIES VS_TOOL_OVERRIDE "Manifest")
endif()

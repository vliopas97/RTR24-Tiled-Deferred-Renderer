workspace "DeferredRenderer"
    architecture "x64"
    startproject "DeferredRenderer"
    toolset "v143"

    configurations
    {
        "Debug",
        "Release"
    }

    OutputDir = "%{prj.name}-%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    OutputName ="%{prj.name}"

    group "Dependencies"
    include "ThirdParty/ImGui"
    group ""

project "DeferredRenderer"
    location "DeferredRenderer"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    staticruntime "off"
    floatingpoint "fast"

    targetdir ("bin/")
    objdir ("bin-int/".. OutputDir)

    linkoptions { "/SUBSYSTEM:WINDOWS"}

    includedirs
    {
        "%{prj.name}/src",
        "%{wks.location}/ThirdParty/dxc",
        "%{wks.location}/ThirdParty/glm",
        "%{wks.location}/ThirdParty/core",
        "%{wks.location}/ThirdParty/DirectXTex/include",
        "%{wks.location}/ThirdParty/ImGui"
    }
    
    libdirs
    {
        "%{wks.location}/dxcompiler"
    }

    links
    {
        "d3d12.lib",
        "DXGI.lib",
        "dxguid.lib",
        "d3dcompiler.lib",
        "DirectXTex.lib",
        "DirectXTK12.lib",
        "ImGui"
    }

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/src/**.hlsl",
        "%{prj.name}/src/**.hlsli"
    }

    filter "files:**.hlsl"
        shadermodel "6.0"
        buildmessage 'Compiling HLSL shader %{file.relpath}'

    filter { "files:**/VertexShaders/*.hlsl" }
        removeflags "ExcludeFromBuild"
        shadertype "Vertex"
        shaderobjectfileoutput 'src/Rendering/Shaders/build/%%(Filename)_VS.cso'

    filter { "files:**/PixelShaders/*.hlsl" }
        removeflags "ExcludeFromBuild"
        shadertype "Pixel"
        shaderobjectfileoutput 'src/Rendering/Shaders/build/%%(Filename)_PS.cso'

    filter {"files:**.hlsli"}
        flags {"ExcludeFromBuild"}

    filter "action:vs*"
        buildoptions { "/permissive" }
    
    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"
        libdirs {"%{wks.location}/ThirdParty/DirectXTex/bin/debug",
                 "%{wks.location}/ThirdParty/core/bin/debug"}
        targetname (OutputName.."_d")

    filter "configurations:Release"
        runtime "Release"
        symbols "on"
        optimize "Full"
        flags {"LinkTimeOptimization"}
        libdirs {"%{wks.location}/ThirdParty/DirectXTex/bin/release",
                "%{wks.location}/ThirdParty/core/bin/release" }
        targetname (OutputName)

        defines
        {
            "NDEBUG"
        }
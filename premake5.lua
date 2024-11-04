workspace "DeferredRenderer"
    architecture "x64"
    startproject "DeferredRenderer"
    toolset "v143"

    configurations
    {
        "Debug",
        "Release"
    }

    OutputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "DeferredRenderer"
    location "DeferredRenderer"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    staticruntime "off"
    floatingpoint "fast"

    targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

    linkoptions { "/SUBSYSTEM:WINDOWS"}

    includedirs
    {
        "%{prj.name}/src",
        "%{wks.location}/ThirdParty/dxc",
        "%{wks.location}/ThirdParty/glm",
        "%{wks.location}/ThirdParty/core"
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
        "d3dcompiler.lib"
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

    filter "configurations:Release"
        runtime "Release"
        symbols "on"
        optimize "Full"
        flags {"LinkTimeOptimization"}

        defines
        {
            "NDEBUG"
        }
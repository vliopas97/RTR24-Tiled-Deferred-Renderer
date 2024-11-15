project "ImGui"
	kind "StaticLib"
	language "C++"
    cppdialect "c++latest"
    staticruntime "on"

	targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
	objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")
    
    includedirs
    {
        "%{prj.location}"
    }
	files
	{
		"imconfig.h",
		"imgui.h",
		"imgui.cpp",
		"imgui_draw.cpp",
		"imgui_internal.h",
		"imgui_tables.cpp",
		"imgui_widgets.cpp",
		"imstb_rectpack.h",
		"imstb_textedit.h",
		"imstb_truetype.h",
		"imgui_demo.cpp",
        "backends/imgui_impl_dx12.h",
        "backends/imgui_impl_win32.h",
        "backends/imgui_impl_dx12.cpp",
        "backends/imgui_impl_win32.cpp"
	}

	defines
	{
		"IMGUI_API=__declspec(dllexport)"
	}



	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

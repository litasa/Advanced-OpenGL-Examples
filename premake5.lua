-- premake5.lua
--require "dlls"

workspace "Advanced GLSamples"
    location "project"
    configurations { "Debug", "Release" }
    platforms { "Win32" }

    filter { "platforms:Win32" }
        system "Windows"
        architecture "x86"

    filter { }

project "MultidrawIndirect with Individual Data"
    kind "ConsoleApp"
    language "C++"
    location "project/MultidrawIndirect"
    targetdir "bin/%{cfg.buildcfg}/"

    files { "./src/MultidrawIndirect/**.h", "./src/MultidrawIndirect/**.cpp" }

    --Include directories
    includedirs {
        "./dependencies/windows/glfw-3.2.1/include",
        "./dependencies/windows/glew-2.0.0/include"
    }

    --libraries and links
        --links
        links {
            "glew32",
            "opengl32",
            "glfw3"
        }
        --libs
        libdirs {
            "./dependencies/windows/glfw-3.2.1/lib",
            "./dependencies/windows/glew-2.0.0/lib"
        }
    
    --Debug and Release configurations
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter { }
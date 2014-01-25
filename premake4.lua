-- A solution contains projects, and defines the available configurations
solution "Shadow Mapping"
   configurations { "Debug", "Release" }
   location "build"

   project "Common"
      kind "StaticLib"
      language "C++"
      includedirs "include/"
      includedirs { 
               "external/glm",  
               "external/gl3w/include", 
               "external/glfw/include"
      }
      files { "include/**.h", "include/**.hpp",  }
      files { "src/common/**.hpp", "src/common/**.cpp", "src/common/**.c" }

      targetdir "bin/"

      configuration "Debug"
         links {"glfw3" }
         flags { "Symbols" }
 
      configuration "Release"
         links {"glfw3" }
         flags { "Optimize" }   
   
   -- A project defines one build target
   project "Normal with PCF"
      kind "ConsoleApp"
      language "C++"

      links "Common"

      files { "src/pcf/**.hpp", "src/pcf/**.cpp", "src/pcf/**.c", "src/pcf/**.h", }

      libdirs {"external/libs"}

      includedirs "include"
      includedirs { 
               "external/glm",  
               "external/gl3w/include", 
               "external/glfw/include"
      }

      targetdir "bin/"

      configuration "windows"
         defines "WIN32"
         links {"glu32", "opengl32", "gdi32", "winmm", "user32"}
 
      configuration "Debug"
         links {"glfw3" }
         defines { "DEBUG" }
         flags { "Symbols" }
 
      configuration "Release"
         links {"glfw3" }
         defines { "NDEBUG" }
         flags { "Optimize" }    

   project "VSM"
      kind "ConsoleApp"
      language "C++"

      files { "src/vsm/**.hpp", "src/vsm/**.cpp", "src/vsm/**.c", "src/vsm/**.h", "src/vsm/**.glsl" }

      links "Common"
      libdirs {"external/libs"}

      includedirs "include"
      includedirs { 
               "external/glm",  
               "external/gl3w/include", 
               "external/glfw/include"
      }

      targetdir "bin/"

      configuration "windows"
         defines "WIN32"
         links {"glu32", "opengl32", "gdi32", "winmm", "user32"}
 
      configuration "Debug"
         links {"glfw3" }
         defines { "DEBUG" }
         flags { "Symbols" }
 
      configuration "Release"
         links {"glfw3" }
         defines { "NDEBUG" }
         flags { "Optimize" }    

   project "Cubemapped VSM"
      kind "ConsoleApp"
      language "C++"

      files { "src/vsmcube/**.hpp", "src/vsmcube/**.cpp", "src/vsmcube/**.c", "src/vsmcube/**.h", "src/vsmcube/**.glsl" }

      links "Common"
      libdirs {"external/libs"}

      includedirs "include"
      includedirs { 
               "external/glm",  
               "external/gl3w/include", 
               "external/glfw/include"
      }

      targetdir "bin/"

      configuration "windows"
         defines "WIN32"
         links {"glu32", "opengl32", "gdi32", "winmm", "user32"}
 
      configuration "Debug"
         links {"glfw3" }
         defines { "DEBUG" }
         flags { "Symbols" }
 
      configuration "Release"
         links {"glfw3" }
         defines { "NDEBUG" }
         flags { "Optimize" } 


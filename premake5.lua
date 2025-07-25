workspace "iv-classichud"
	configurations { "ReleaseIV", "DebugIV" }
	location "project_files"
   
project "iv-classichud"
	kind "SharedLib"
	language "C++"
	targetdir "output/asi/"
	objdir ("output/obj")
	targetextension ".asi"
	characterset ("MBCS")
	linkoptions "/SAFESEH:NO"
	buildoptions { "/permissive" }
	defines { "_CRT_SECURE_NO_WARNINGS", "_CRT_NON_CONFORMING_SWPRINTFS", "_USE_MATH_DEFINES" }
	disablewarnings { "4244", "4800", "4305", "4073", "4838", "4996", "4221", "4430", "26812", "26495", "6031" }
	cppdialect "c++latest"

	files {
		"source/**.*",
	}
	
	includedirs { 
		"source/**",
		"vendor/**"
	}
	
	includedirs {
		"$(PLUGIN_SDK_DIR)/shared/",
		"$(PLUGIN_SDK_DIR)/shared/game/",
	}
	
	filter { "configurations:*IV" }
		defines { "GTAIV", "PLUGIN_SGV_CE", "RAGE" }
		includedirs {
			"$(PLUGIN_SDK_DIR)/plugin_IV/",
			"$(PLUGIN_SDK_DIR)/plugin_IV/game_IV/",
		}
		targetname "ClassicHudIV"
		debugdir "$(GTA_IV_DIR)"
		debugcommand "$(GTA_IV_DIR)/GTAIV.exe"
		postbuildcommands "copy /y \"$(TargetPath)\" \"$(GTA_IV_DIR)\\plugins\\ClassicHudIV.asi\""
		
	filter { }
	
	libdirs { 
		"$(PLUGIN_SDK_DIR)/output/lib/",
		"$(DXSDK_DIR)/Lib/x86",
		"$(PLUGIN_SDK_DIR)\\shared\\bass",
	}
	
	filter "configurations:Debug*"		
		defines { "DEBUG" }
		symbols "on"
		staticruntime "on"

	filter "configurations:Release*"
		defines { "NDEBUG" }
		symbols "off"
		optimize "On"
		staticruntime "on"
		
	filter "configurations:ReleaseIV"
		links { "plugin_iv" }
		
	filter "configurations:DebugIV"
		links { "plugin_iv_d" }
			
	filter { }

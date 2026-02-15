#pragma once

#include <string>

// Default paths if not defined by CMake (relative from build output directory)
#ifndef RESOURCE_PATH
#define RESOURCE_PATH L"../../res/"
#endif

namespace AppConfig
{
	inline int windowWidth = 2;
	inline int windowHeight = 2;
	inline int viewportWidth = 2;
	inline int viewportHeight = 2;

	inline bool needsResize = true;
	inline float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	inline bool isWindowMinimized = false;
	inline double deltaTime = 0.0;
	inline float IBLintensity = 1.0f;
	inline float IBLrotation = 0.0f;
	inline bool regeneratePrefilteredMap = false;
	inline bool isBackgroundBlurred = false;
	inline float blurAmount = 0.5f;
	inline float backgroundIntensity = 1.0f;

	inline bool drawWSUI = true;
	inline bool captureNextFrame = false;

	inline int maxBVHDepth = 1;
	inline int minBVHDepth = 0;
	inline bool showLeafsOnly = false;

	inline float aoRadius = 0.5f;
	inline float aoIntensity = 2.0f;
	inline float aoBias = 0.025f;

	inline float getAspectRatio()
	{
		return static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
	}

	// Helper to build full resource path from just the resource filename/subfolder
	inline std::wstring GetResourcePath(const std::wstring& resourcePath)
	{
		return std::wstring(RESOURCE_PATH) + resourcePath;
	}

	// Narrow string version for compatibility
	inline std::string GetResourcePathA(const std::string& resourcePath)
	{
#ifdef RELEASE_BUILD
		return std::string("res/") + resourcePath;
#else
		return std::string("../../res/") + resourcePath;
#endif
	}
} // namespace AppConfig

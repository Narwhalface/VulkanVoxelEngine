#ifndef LUA_TERRAIN_SCRIPT_BRIDGE_HPP
#define LUA_TERRAIN_SCRIPT_BRIDGE_HPP

#include <filesystem>
#include <optional>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

struct lua_State;

class LuaTerrainScriptBridge {
public:
    struct ScriptValues {
        std::optional<int> renderDistanceChunks;
        std::optional<float> noiseIntensity;
        std::optional<uint32_t> terrainSeed;
        std::optional<bool> randomizeSeed;
        std::string errorMessage;
    };

    static ScriptValues loadScript(const std::filesystem::path& scriptPath) {
        // Loads terrain settings from a Lua script path; input is scriptPath and output is parsed ScriptValues + error text.
        ScriptValues scriptValues;

        if (!std::filesystem::exists(scriptPath)) {
            return scriptValues;
        }

        HMODULE luaModule = loadLuaModule();
        if (luaModule == nullptr) {
            scriptValues.errorMessage = "Lua runtime not found. Place lua54.dll (or lua53.dll/lua52.dll) next to the executable.";
            return scriptValues;
        }

        using LuaLNewStateFn = lua_State* (__cdecl*)();
        using LuaLOpenLibsFn = void (__cdecl*)(lua_State*);
        using LuaLCloseFn = void (__cdecl*)(lua_State*);
        using LuaLLoadFileXFn = int (__cdecl*)(lua_State*, const char*, const char*);
        using LuaPCallKFn = int (__cdecl*)(lua_State*, int, int, int, long long, void*);
        using LuaGetGlobalFn = int (__cdecl*)(lua_State*, const char*);
        using LuaTypeFn = int (__cdecl*)(lua_State*, int);
        using LuaToIntegerXFn = long long (__cdecl*)(lua_State*, int, int*);
        using LuaToNumberXFn = double (__cdecl*)(lua_State*, int, int*);
        using LuaToBooleanFn = int (__cdecl*)(lua_State*, int);
        using LuaToLStringFn = const char* (__cdecl*)(lua_State*, int, size_t*);
        using LuaSetTopFn = void (__cdecl*)(lua_State*, int);

        const auto luaLNewState = reinterpret_cast<LuaLNewStateFn>(GetProcAddress(luaModule, "luaL_newstate"));
        const auto luaLOpenLibs = reinterpret_cast<LuaLOpenLibsFn>(GetProcAddress(luaModule, "luaL_openlibs"));
        const auto luaClose = reinterpret_cast<LuaLCloseFn>(GetProcAddress(luaModule, "lua_close"));
        const auto luaLLoadFileX = reinterpret_cast<LuaLLoadFileXFn>(GetProcAddress(luaModule, "luaL_loadfilex"));
        const auto luaPCallK = reinterpret_cast<LuaPCallKFn>(GetProcAddress(luaModule, "lua_pcallk"));
        const auto luaGetGlobal = reinterpret_cast<LuaGetGlobalFn>(GetProcAddress(luaModule, "lua_getglobal"));
        const auto luaType = reinterpret_cast<LuaTypeFn>(GetProcAddress(luaModule, "lua_type"));
        const auto luaToIntegerX = reinterpret_cast<LuaToIntegerXFn>(GetProcAddress(luaModule, "lua_tointegerx"));
        const auto luaToNumberX = reinterpret_cast<LuaToNumberXFn>(GetProcAddress(luaModule, "lua_tonumberx"));
        const auto luaToBoolean = reinterpret_cast<LuaToBooleanFn>(GetProcAddress(luaModule, "lua_toboolean"));
        const auto luaToLString = reinterpret_cast<LuaToLStringFn>(GetProcAddress(luaModule, "lua_tolstring"));
        const auto luaSetTop = reinterpret_cast<LuaSetTopFn>(GetProcAddress(luaModule, "lua_settop"));

        if (luaLNewState == nullptr || luaLOpenLibs == nullptr || luaClose == nullptr ||
            luaLLoadFileX == nullptr || luaPCallK == nullptr || luaGetGlobal == nullptr ||
            luaType == nullptr || luaToIntegerX == nullptr || luaToNumberX == nullptr ||
            luaToBoolean == nullptr || luaToLString == nullptr || luaSetTop == nullptr) {
            scriptValues.errorMessage = "Lua runtime is missing required API exports.";
            FreeLibrary(luaModule);
            return scriptValues;
        }

        constexpr int luaOk = 0;
        constexpr int luaTBoolean = 1;
        constexpr int luaTNumber = 3;

        lua_State* luaState = luaLNewState();
        if (luaState == nullptr) {
            scriptValues.errorMessage = "Failed to create Lua state.";
            FreeLibrary(luaModule);
            return scriptValues;
        }

        luaLOpenLibs(luaState);

        const std::string scriptPathUtf8 = scriptPath.string();
        const int loadStatus = luaLLoadFileX(luaState, scriptPathUtf8.c_str(), nullptr);
        if (loadStatus != luaOk) {
            scriptValues.errorMessage = readLuaError(luaState, luaToLString, luaSetTop);
            luaClose(luaState);
            FreeLibrary(luaModule);
            return scriptValues;
        }

        const int callStatus = luaPCallK(luaState, 0, 0, 0, 0, nullptr);
        if (callStatus != luaOk) {
            scriptValues.errorMessage = readLuaError(luaState, luaToLString, luaSetTop);
            luaClose(luaState);
            FreeLibrary(luaModule);
            return scriptValues;
        }

        luaGetGlobal(luaState, "render_distance");
        if (luaType(luaState, -1) == luaTNumber) {
            int isNumber = 0;
            const long long value = luaToIntegerX(luaState, -1, &isNumber);
            if (isNumber != 0) {
                scriptValues.renderDistanceChunks = static_cast<int>(value);
            }
        }
        luaSetTop(luaState, -2);

        luaGetGlobal(luaState, "noise_intensity");
        if (luaType(luaState, -1) == luaTNumber) {
            int isNumber = 0;
            const double value = luaToNumberX(luaState, -1, &isNumber);
            if (isNumber != 0) {
                scriptValues.noiseIntensity = static_cast<float>(value);
            }
        }
        luaSetTop(luaState, -2);

        luaGetGlobal(luaState, "terrain_seed");
        if (luaType(luaState, -1) == luaTNumber) {
            int isNumber = 0;
            const long long value = luaToIntegerX(luaState, -1, &isNumber);
            if (isNumber != 0 && value >= 0) {
                scriptValues.terrainSeed = static_cast<uint32_t>(value);
            }
        }
        luaSetTop(luaState, -2);

        luaGetGlobal(luaState, "randomize_seed");
        if (luaType(luaState, -1) == luaTBoolean) {
            scriptValues.randomizeSeed = (luaToBoolean(luaState, -1) != 0);
        }
        luaSetTop(luaState, -2);

        luaClose(luaState);
        FreeLibrary(luaModule);
        return scriptValues;
    }

private:
    static std::string readLuaError(lua_State* luaState,
                                    const char* (__cdecl* luaToLString)(lua_State*, int, size_t*),
                                    void (__cdecl* luaSetTop)(lua_State*, int)) {
        // Reads top-of-stack Lua error text using provided Lua API callbacks; returns a normalized error string.
        size_t messageLength = 0;
        const char* rawMessage = luaToLString(luaState, -1, &messageLength);
        std::string message = "Unknown Lua error.";
        if (rawMessage != nullptr && messageLength > 0) {
            message.assign(rawMessage, messageLength);
        }
        luaSetTop(luaState, -2);
        return message;
    }

    static HMODULE loadLuaModule() {
        // Tries known Lua DLL names in order and returns loaded module handle or nullptr.
        constexpr const char* moduleCandidates[] = {
            "lua54.dll",
            "lua53.dll",
            "lua52.dll"
        };

        for (const char* moduleName : moduleCandidates) {
            HMODULE module = LoadLibraryA(moduleName);
            if (module != nullptr) {
                return module;
            }
        }

        return nullptr;
    }
};

#else

class LuaTerrainScriptBridge {
public:
    struct ScriptValues {
        std::optional<int> renderDistanceChunks;
        std::optional<float> noiseIntensity;
        std::optional<uint32_t> terrainSeed;
        std::optional<bool> randomizeSeed;
        std::string errorMessage;
    };

    static ScriptValues loadScript(const std::filesystem::path&) {
        // Non-Windows fallback loader that returns empty ScriptValues; takes script path and returns defaults.
        return {};
    }
};

#endif

#endif // LUA_TERRAIN_SCRIPT_BRIDGE_HPP

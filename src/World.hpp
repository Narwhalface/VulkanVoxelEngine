#ifndef WORLD_HPP
#define WORLD_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_set>
#include <unordered_map>

struct Voxel {
    uint8_t type = 0;
    // Returns whether this voxel is non-empty/solid; takes no inputs.
    bool isSolid() const noexcept { return type != 0; }
};

struct ChunkCoord {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
};

bool operator==(const ChunkCoord& lhs, const ChunkCoord& rhs) noexcept;

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& coord) const noexcept;
};

class Chunk {
public:
    static constexpr int SIZE = 16;

    Voxel& at(int localX, int localY, int localZ);
    const Voxel& at(int localX, int localY, int localZ) const;
    void set(int localX, int localY, int localZ, const Voxel& voxel);
    void fillType(uint8_t type);
    void setColumnRangeType(int localX, int localZ, int startY, int endY, uint8_t type);

private:
    static std::size_t toIndex(int localX, int localY, int localZ);
    std::array<Voxel, SIZE * SIZE * SIZE> voxels{};
};

struct TerrainSettings {
    float baseHeight = 32.0f;
    float elevationAmplitude = 24.0f;
    float horizontalScale = 0.02f;
    int octaves = 4;
    float persistence = 0.5f;
    float lacunarity = 2.0f;
    int waterLevel = 16;
};

class TerrainGenerator {
public:
    TerrainGenerator() = default;

    void configure(uint32_t seedValue, const TerrainSettings& settingsValue);
    void disable() noexcept;
    bool isEnabled() const noexcept;
    void populateChunk(const ChunkCoord& coord, Chunk& chunk) const;
    uint32_t getSeed() const noexcept;
    TerrainSettings getSettings() const noexcept;

private:
    float fractalNoise(float x, float z) const;
    float valueNoise(float x, float z) const;
    float randomValue(int x, int z) const;
    int sampleColumnHeight(int worldX, int worldZ) const;
    void clearHeightCache();

    TerrainSettings settings{};
    uint32_t seed = 0;
    bool enabled = false;
    mutable std::unordered_map<uint64_t, int> cachedColumnHeights;
    mutable std::shared_mutex cacheMutex;
};

class World {
public:
    World() = default;
    explicit World(uint32_t terrainSeed);

    void setTerrainGenerator(uint32_t terrainSeed, const TerrainSettings& settings = TerrainSettings{});
    void disableTerrainGenerator() noexcept;
    void updateTerrainSettings(const TerrainSettings& settings);
    TerrainSettings getTerrainSettings() const noexcept;

    Voxel& setVoxel(int worldX, int worldY, int worldZ, const Voxel& voxel);
    std::optional<Voxel> getVoxel(int worldX, int worldY, int worldZ) const;
    bool hasChunk(const ChunkCoord& coord) const;
    Chunk& getOrCreateChunk(const ChunkCoord& coord);
    const Chunk* findChunk(const ChunkCoord& coord) const;
    
    void regenerateAllChunks();
    void regenerateChunk(const ChunkCoord& coord);
    void retainChunks(const std::unordered_set<ChunkCoord, ChunkCoordHash>& keepSet);
    void clearAllChunks();

private:
    ChunkCoord worldToChunk(int worldX, int worldY, int worldZ) const;

    TerrainGenerator terrainGenerator{};
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks;
};

#endif // WORLD_HPP

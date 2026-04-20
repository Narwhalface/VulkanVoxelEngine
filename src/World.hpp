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
    /**
     * Checks whether the voxel is non-empty.
     * @return True when voxel type is non-zero.
     */
    bool isSolid() const noexcept { return type != 0; }
};

struct ChunkCoord {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
};

/**
 * Compares two chunk coordinates for exact equality.
 * @param lhs Left chunk coordinate.
 * @param rhs Right chunk coordinate.
 * @return True when all coordinate components match.
 */
bool operator==(const ChunkCoord& lhs, const ChunkCoord& rhs) noexcept;

struct ChunkCoordHash {
    /**
     * Computes a hash value for a chunk coordinate.
     * @param coord Chunk coordinate to hash.
     * @return Hash value usable in unordered containers.
     */
    std::size_t operator()(const ChunkCoord& coord) const noexcept;
};

class Chunk {
public:
    static constexpr int SIZE = 16;

    /**
     * Returns a mutable voxel reference at local chunk coordinates.
     * @param localX Local X coordinate in [0, SIZE).
     * @param localY Local Y coordinate in [0, SIZE).
     * @param localZ Local Z coordinate in [0, SIZE).
     * @return Mutable voxel reference.
     */
    Voxel& at(int localX, int localY, int localZ);
    /**
     * Returns a const voxel reference at local chunk coordinates.
     * @param localX Local X coordinate in [0, SIZE).
     * @param localY Local Y coordinate in [0, SIZE).
     * @param localZ Local Z coordinate in [0, SIZE).
     * @return Const voxel reference.
     */
    const Voxel& at(int localX, int localY, int localZ) const;
    /**
     * Writes a voxel at local chunk coordinates.
     * @param localX Local X coordinate in [0, SIZE).
     * @param localY Local Y coordinate in [0, SIZE).
     * @param localZ Local Z coordinate in [0, SIZE).
     * @param voxel Voxel value to write.
     * @return No return value.
     */
    void set(int localX, int localY, int localZ, const Voxel& voxel);
    /**
     * Fills the entire chunk with one voxel type.
     * @param type Voxel type to assign.
     * @return No return value.
     */
    void fillType(uint8_t type);
    /**
     * Fills a Y range in a local X/Z column with one voxel type.
     * @param localX Local X column index.
     * @param localZ Local Z column index.
     * @param startY Inclusive start Y in local coordinates.
     * @param endY Inclusive end Y in local coordinates.
     * @param type Voxel type to assign.
     * @return No return value.
     */
    void setColumnRangeType(int localX, int localZ, int startY, int endY, uint8_t type);

private:
    /**
     * Converts local coordinates into a linear voxel array index.
     * @param localX Local X coordinate in [0, SIZE).
     * @param localY Local Y coordinate in [0, SIZE).
     * @param localZ Local Z coordinate in [0, SIZE).
     * @return Linear index into internal voxel storage.
     */
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

    /**
     * Enables and configures procedural terrain generation.
     * @param seedValue Seed used for deterministic generation.
     * @param settingsValue Terrain generation settings.
     * @return No return value.
     */
    void configure(uint32_t seedValue, const TerrainSettings& settingsValue);
    /**
     * Disables procedural terrain generation.
     * @return No return value.
     */
    void disable() noexcept;
    /**
     * Indicates whether procedural terrain generation is enabled.
     * @return True when generator is enabled.
     */
    bool isEnabled() const noexcept;
    /**
     * Populates a chunk with generated terrain data.
     * @param coord Chunk coordinate to generate.
     * @param chunk Chunk storage to populate.
     * @return No return value.
     */
    void populateChunk(const ChunkCoord& coord, Chunk& chunk) const;
    /**
     * Returns the active terrain seed.
     * @return Current generator seed.
     */
    uint32_t getSeed() const noexcept;
    /**
     * Returns the active terrain settings.
     * @return Copy of current terrain settings.
     */
    TerrainSettings getSettings() const noexcept;

private:
    /**
     * Computes multi-octave terrain noise at the given sample point.
     * @param x Sample X coordinate.
     * @param z Sample Z coordinate.
     * @return Normalized fractal noise value.
     */
    float fractalNoise(float x, float z) const;
    /**
     * Computes smooth value noise at the given sample point.
     * @param x Sample X coordinate.
     * @param z Sample Z coordinate.
     * @return Interpolated value noise result.
     */
    float valueNoise(float x, float z) const;
    /**
     * Produces deterministic pseudo-random value for grid coordinates.
     * @param x Grid X coordinate.
     * @param z Grid Z coordinate.
     * @return Pseudo-random value in normalized range.
     */
    float randomValue(int x, int z) const;
    /**
     * Samples terrain column height for world coordinates.
     * @param worldX World-space X coordinate.
     * @param worldZ World-space Z coordinate.
     * @return Maximum solid world-space Y for the column.
     */
    int sampleColumnHeight(int worldX, int worldZ) const;
    /**
     * Clears cached terrain column heights.
     * @return No return value.
     */
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
    /**
     * Constructs a world and initializes terrain generation with a seed.
     * @param terrainSeed Seed used by terrain generator.
     * @return No return value.
     */
    explicit World(uint32_t terrainSeed);

    /**
     * Enables terrain generation with seed and settings.
     * @param terrainSeed Seed used by terrain generator.
     * @param settings Terrain generation settings.
     * @return No return value.
     */
    void setTerrainGenerator(uint32_t terrainSeed, const TerrainSettings& settings = TerrainSettings{});
    /**
     * Disables terrain generation.
     * @return No return value.
     */
    void disableTerrainGenerator() noexcept;
    /**
     * Replaces terrain generation settings while preserving seed.
     * @param settings Updated terrain settings.
     * @return No return value.
     */
    void updateTerrainSettings(const TerrainSettings& settings);
    /**
     * Returns current terrain settings.
     * @return Copy of terrain settings.
     */
    TerrainSettings getTerrainSettings() const noexcept;

    /**
     * Sets a voxel at world-space coordinates.
     * @param worldX World-space X coordinate.
     * @param worldY World-space Y coordinate.
     * @param worldZ World-space Z coordinate.
     * @param voxel Voxel value to write.
     * @return Mutable reference to stored voxel.
     */
    Voxel& setVoxel(int worldX, int worldY, int worldZ, const Voxel& voxel);
    /**
     * Gets a voxel at world-space coordinates.
     * @param worldX World-space X coordinate.
     * @param worldY World-space Y coordinate.
     * @param worldZ World-space Z coordinate.
     * @return Voxel value when chunk is loaded; empty optional otherwise.
     */
    std::optional<Voxel> getVoxel(int worldX, int worldY, int worldZ) const;
    /**
     * Checks whether a chunk exists in memory.
     * @param coord Chunk coordinate to check.
     * @return True when chunk is present.
     */
    bool hasChunk(const ChunkCoord& coord) const;
    /**
     * Returns an existing chunk or creates one if missing.
     * @param coord Chunk coordinate to fetch.
     * @return Mutable reference to the chunk.
     */
    Chunk& getOrCreateChunk(const ChunkCoord& coord);
    /**
     * Finds an existing chunk without creating it.
     * @param coord Chunk coordinate to find.
     * @return Pointer to chunk when present, otherwise nullptr.
     */
    const Chunk* findChunk(const ChunkCoord& coord) const;
    
    /**
     * Regenerates terrain data for all currently loaded chunks.
     * @return No return value.
     */
    void regenerateAllChunks();
    /**
     * Regenerates terrain data for one chunk.
     * @param coord Chunk coordinate to regenerate.
     * @return No return value.
     */
    void regenerateChunk(const ChunkCoord& coord);
    /**
     * Removes loaded chunks that are not in the keep set.
     * @param keepSet Set of chunk coordinates to retain.
     * @return No return value.
     */
    void retainChunks(const std::unordered_set<ChunkCoord, ChunkCoordHash>& keepSet);
    /**
     * Clears all loaded chunks.
     * @return No return value.
     */
    void clearAllChunks();

private:
    /**
     * Converts world-space coordinates to owning chunk coordinate.
     * @param worldX World-space X coordinate.
     * @param worldY World-space Y coordinate.
     * @param worldZ World-space Z coordinate.
     * @return Chunk coordinate containing the world position.
     */
    ChunkCoord worldToChunk(int worldX, int worldY, int worldZ) const;

    TerrainGenerator terrainGenerator{};
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks;
};

#endif // WORLD_HPP

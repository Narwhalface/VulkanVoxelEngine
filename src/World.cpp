#include "World.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <utility>

namespace {
    constexpr int chunkSize = Chunk::SIZE;

    int floorDiv(int value, int divisor) noexcept {
        int quotient = value / divisor;
        const int remainder = value % divisor;
        if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
            --quotient;
        }
        return quotient;
    }

    int positiveModulo(int value, int divisor) noexcept {
        int result = value % divisor;
        if (result < 0) {
            result += divisor;
        }
        return result;
    }
}

bool operator==(const ChunkCoord& lhs, const ChunkCoord& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

std::size_t ChunkCoordHash::operator()(const ChunkCoord& coord) const noexcept {
    const std::size_t hx = std::hash<int32_t>{}(coord.x);
    const std::size_t hy = std::hash<int32_t>{}(coord.y);
    const std::size_t hz = std::hash<int32_t>{}(coord.z);
    return hx ^ (hy + 0x9e3779b9 + (hx << 6) + (hx >> 2)) ^ (hz + 0x9e3779b9 + (hy << 6) + (hy >> 2));
}

std::size_t Chunk::toIndex(int localX, int localY, int localZ) {
    assert(localX >= 0 && localX < SIZE);
    assert(localY >= 0 && localY < SIZE);
    assert(localZ >= 0 && localZ < SIZE);
    return static_cast<std::size_t>(localZ * SIZE * SIZE + localY * SIZE + localX);
}

Voxel& Chunk::at(int localX, int localY, int localZ) {
    return voxels[toIndex(localX, localY, localZ)];
}

const Voxel& Chunk::at(int localX, int localY, int localZ) const {
    return voxels[toIndex(localX, localY, localZ)];
}

void Chunk::set(int localX, int localY, int localZ, const Voxel& voxel) {
    voxels[toIndex(localX, localY, localZ)] = voxel;
}

void TerrainGenerator::configure(uint32_t seedValue, const TerrainSettings& settingsValue) {
    seed = seedValue;
    settings = settingsValue;
    enabled = true;
}

void TerrainGenerator::disable() noexcept {
    enabled = false;
}

bool TerrainGenerator::isEnabled() const noexcept {
    return enabled;
}

void TerrainGenerator::populateChunk(const ChunkCoord& coord, Chunk& chunk) const {
    if (!enabled) {
        return;
    }

    const int baseX = coord.x * chunkSize;
    const int baseY = coord.y * chunkSize;
    const int baseZ = coord.z * chunkSize;

    for (int localZ = 0; localZ < chunkSize; ++localZ) {
        const int worldZ = baseZ + localZ;
        for (int localX = 0; localX < chunkSize; ++localX) {
            const int worldX = baseX + localX;
            const float sampleX = static_cast<float>(worldX) * settings.horizontalScale;
            const float sampleZ = static_cast<float>(worldZ) * settings.horizontalScale;
            const float terrainNoise = fractalNoise(sampleX, sampleZ);
            const float columnHeight = settings.baseHeight + settings.elevationAmplitude * terrainNoise;
            const int maxSolidHeight = static_cast<int>(std::floor(columnHeight));

            for (int localY = 0; localY < chunkSize; ++localY) {
                const int worldY = baseY + localY;
                Voxel voxel{};
                if (worldY <= maxSolidHeight) {
                    voxel.type = (worldY == maxSolidHeight) ? 2 : 1;
                } else if (worldY <= settings.waterLevel) {
                    voxel.type = 3;
                }

                chunk.set(localX, localY, localZ, voxel);
            }
        }
    }
}

float TerrainGenerator::fractalNoise(float x, float z) const {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float norm = 0.0f;

    for (int octave = 0; octave < std::max(1, settings.octaves); ++octave) {
        const float sample = valueNoise(x * frequency, z * frequency) * 2.0f - 1.0f;
        sum += sample * amplitude;
        norm += amplitude;
        amplitude *= std::clamp(settings.persistence, 0.001f, 0.999f);
        frequency *= std::max(settings.lacunarity, 1.0f);
    }

    if (norm <= 0.0f) {
        return 0.0f;
    }

    return std::clamp(sum / norm, -1.0f, 1.0f);
}

float TerrainGenerator::valueNoise(float x, float z) const {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const float tx = x - static_cast<float>(x0);
    const float tz = z - static_cast<float>(z0);

    const float v00 = randomValue(x0, z0);
    const float v10 = randomValue(x1, z0);
    const float v01 = randomValue(x0, z1);
    const float v11 = randomValue(x1, z1);

    const auto fade = [](float t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    };

    const float u = fade(std::clamp(tx, 0.0f, 1.0f));
    const float v = fade(std::clamp(tz, 0.0f, 1.0f));

    const float nx0 = v00 + u * (v10 - v00);
    const float nx1 = v01 + u * (v11 - v01);
    return nx0 + v * (nx1 - nx0);
}

float TerrainGenerator::randomValue(int x, int z) const {
    uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(x)) * 0x9e3779b185ebca87ULL;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(z)) * 0xc2b2ae3d27d4eb4fULL;
    h ^= static_cast<uint64_t>(seed) * 0x165667b19e3779f9ULL;
    h ^= (h >> 33);
    h *= 0xff51afd7ed558ccdULL;
    h ^= (h >> 33);
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= (h >> 33);

    const uint32_t mantissa = static_cast<uint32_t>(h >> 40);
    return static_cast<float>(mantissa) / static_cast<float>(0xFFFFFF);
}

World::World(uint32_t terrainSeed) {
    setTerrainGenerator(terrainSeed);
}

void World::setTerrainGenerator(uint32_t terrainSeed, const TerrainSettings& settings) {
    terrainGenerator.configure(terrainSeed, settings);
}

void World::disableTerrainGenerator() noexcept {
    terrainGenerator.disable();
}

ChunkCoord World::worldToChunk(int worldX, int worldY, int worldZ) const {
    return {floorDiv(worldX, chunkSize), floorDiv(worldY, chunkSize), floorDiv(worldZ, chunkSize)};
}

Chunk& World::getOrCreateChunk(const ChunkCoord& coord) {
    auto [it, inserted] = chunks.try_emplace(coord);
    if (inserted) {
        terrainGenerator.populateChunk(coord, it->second);
    }
    return it->second;
}

const Chunk* World::findChunk(const ChunkCoord& coord) const {
    const auto it = chunks.find(coord);
    if (it == chunks.end()) {
        return nullptr;
    }
    return &it->second;
}

bool World::hasChunk(const ChunkCoord& coord) const {
    return chunks.find(coord) != chunks.end();
}

Voxel& World::setVoxel(int worldX, int worldY, int worldZ, const Voxel& voxel) {
    const ChunkCoord chunkCoord = worldToChunk(worldX, worldY, worldZ);
    Chunk& chunk = getOrCreateChunk(chunkCoord);
    const int localX = positiveModulo(worldX, chunkSize);
    const int localY = positiveModulo(worldY, chunkSize);
    const int localZ = positiveModulo(worldZ, chunkSize);
    chunk.set(localX, localY, localZ, voxel);
    return chunk.at(localX, localY, localZ);
}

std::optional<Voxel> World::getVoxel(int worldX, int worldY, int worldZ) const {
    const ChunkCoord chunkCoord = worldToChunk(worldX, worldY, worldZ);
    const auto it = chunks.find(chunkCoord);
    if (it == chunks.end()) {
        return std::nullopt;
    }
    const int localX = positiveModulo(worldX, chunkSize);
    const int localY = positiveModulo(worldY, chunkSize);
    const int localZ = positiveModulo(worldZ, chunkSize);
    return it->second.at(localX, localY, localZ);
}

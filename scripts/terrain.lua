-- Terrain script loaded at startup by VulkanApp
-- Adjust these values and restart the app to apply changes.

-- Chunk render distance (valid range in app: 2 to 16)
render_distance = 12

-- Terrain noise intensity mapped to elevation amplitude (valid range in app: 1.0 to 128.0)
noise_intensity = 10.0

-- Randomize terrain every launch (true = new random world seed each run)
randomize_seed = true

-- Optional fixed seed for reproducible terrain.
-- If this is set, it overrides randomize_seed.
--terrain_seed =  171564861

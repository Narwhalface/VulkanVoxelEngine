#ifndef TERRAIN_CONTROL_WINDOW_HPP
#define TERRAIN_CONTROL_WINDOW_HPP

#include <memory>
#include <string>
#include <windows.h>
#include <functional>

class TerrainControlWindow {
public:
    using RenderDistanceCallback = std::function<void(int)>;

    explicit TerrainControlWindow(int initialRenderDistance = 16);
    ~TerrainControlWindow();

    bool isOpen() const;
    void setRenderDistanceCallback(RenderDistanceCallback callback);
    void show();
    void hide();
    void update();
    static LRESULT CALLBACK dialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND dialogHandle = nullptr;
    int currentRenderDistance = 16;
    RenderDistanceCallback onRenderDistanceChanged;
    
    void createDialog();
    void syncRenderDistanceFromDialog();
    void updateDialogValues();
    void applyRenderDistance();
};

#endif // TERRAIN_CONTROL_WINDOW_HPP

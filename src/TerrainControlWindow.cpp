#include "TerrainControlWindow.hpp"
#include <iostream>
#include <algorithm>
#include <windowsx.h>

// Control IDs for dialog elements
enum ControlID {
    IDC_RENDER_DISTANCE_EDIT = 1001,
    IDC_APPLY_BTN = 1002,
    IDC_RESET_BTN = 1003,
    IDC_HELP_TEXT = 1004
};

static TerrainControlWindow* g_pThis = nullptr;
static WNDPROC g_prevWndProc = nullptr;

TerrainControlWindow::TerrainControlWindow(int initialRenderDistance)
    : currentRenderDistance((std::max)(2, (std::min)(16, initialRenderDistance))) {
    g_pThis = this;
    createDialog();
}

TerrainControlWindow::~TerrainControlWindow() {
    if (dialogHandle != nullptr) {
        DestroyWindow(dialogHandle);
        dialogHandle = nullptr;
    }
    g_pThis = nullptr;
}

bool TerrainControlWindow::isOpen() const {
    return dialogHandle != nullptr && IsWindow(dialogHandle);
}

void TerrainControlWindow::setRenderDistanceCallback(RenderDistanceCallback callback) {
    onRenderDistanceChanged = callback;
}

void TerrainControlWindow::show() {
    if (isOpen()) {
        ShowWindow(dialogHandle, SW_SHOW);
        SetForegroundWindow(dialogHandle);
    }
}

void TerrainControlWindow::hide() {
    if (isOpen()) {
        ShowWindow(dialogHandle, SW_HIDE);
    }
}

void TerrainControlWindow::update() {
    if (!isOpen()) {
        return;
    }
    
    // Process any pending messages for the dialog
    MSG msg{};
    while (PeekMessageA(&msg, dialogHandle, 0, 0, PM_REMOVE)) {
        if (!IsDialogMessageA(dialogHandle, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
}

void TerrainControlWindow::createDialog() {
    // Create modeless dialog window manually
    dialogHandle = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "STATIC",
        "Render Distance",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 320, 220,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr
    );

    if (!dialogHandle) {
        std::cerr << "Failed to create terrain control window\n";
        return;
    }

    // Create child controls
    int yPos = 15;
    const int labelWidth = 150;
    const int spinWidth = 60;
    const int xStart = 10;

    CreateWindowExA(0, "STATIC", "Render Distance (chunks):", WS_CHILD | WS_VISIBLE, xStart, yPos, 170, 20, dialogHandle, nullptr, GetModuleHandleA(nullptr), nullptr);
    CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "16", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT, xStart + 175, yPos, spinWidth, 20, dialogHandle, (HMENU)(intptr_t)IDC_RENDER_DISTANCE_EDIT, GetModuleHandleA(nullptr), nullptr);
    yPos += 35;

    CreateWindowExA(0, "BUTTON", "Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, xStart, yPos, 120, 25, dialogHandle, (HMENU)(intptr_t)IDC_APPLY_BTN, GetModuleHandleA(nullptr), nullptr);
    yPos += 35;

    CreateWindowExA(0, "BUTTON", "Reset (16)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, xStart, yPos, 120, 25, dialogHandle, (HMENU)(intptr_t)IDC_RESET_BTN, GetModuleHandleA(nullptr), nullptr);
    yPos += 35;

    CreateWindowExA(0, "STATIC", "Range: 2 to 16 chunks", WS_CHILD | WS_VISIBLE | SS_LEFT, xStart, yPos, 220, 20, dialogHandle, (HMENU)(intptr_t)IDC_HELP_TEXT, GetModuleHandleA(nullptr), nullptr);

    // Set window proc
    SetWindowLongPtrA(dialogHandle, GWLP_USERDATA, (LONG_PTR)this);
    g_prevWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(dialogHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&TerrainControlWindow::dialogProc))
    );
}

void TerrainControlWindow::syncRenderDistanceFromDialog() {
    if (!isOpen()) return;

    char buffer[32];
    GetDlgItemTextA(dialogHandle, IDC_RENDER_DISTANCE_EDIT, buffer, sizeof(buffer));
    try {
        const int parsed = std::stoi(buffer);
        currentRenderDistance = (std::max)(2, (std::min)(16, parsed));
    } catch (...) {
        currentRenderDistance = 16;
    }
}

void TerrainControlWindow::updateDialogValues() {
    if (!isOpen()) return;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", currentRenderDistance);
    SetDlgItemTextA(dialogHandle, IDC_RENDER_DISTANCE_EDIT, buffer);
}

void TerrainControlWindow::applyRenderDistance() {
    syncRenderDistanceFromDialog();
    updateDialogValues();

    if (onRenderDistanceChanged) {
        onRenderDistanceChanged(currentRenderDistance);
    }

    std::cout << "Render distance updated to " << currentRenderDistance << " chunks\n";
}

LRESULT CALLBACK TerrainControlWindow::dialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* pThis = reinterpret_cast<TerrainControlWindow*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    if (pThis) {
        switch (message) {
            case WM_COMMAND: {
                int controlId = GET_WM_COMMAND_ID(wParam, lParam);
                if (controlId == IDC_APPLY_BTN) {
                    pThis->applyRenderDistance();
                    return 0;
                } else if (controlId == IDC_RESET_BTN) {
                    pThis->currentRenderDistance = 16;
                    pThis->updateDialogValues();
                    pThis->applyRenderDistance();
                    return 0;
                }
                break;
            }
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                return 0;
        }
    }

    return CallWindowProcA(g_prevWndProc, hwnd, message, wParam, lParam);
}

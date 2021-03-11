// 3DEngine.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <mmsystem.h>
#include <d2d1.h>
#include <timeapi.h>

#include <vector>

#include "MainWindow.h"
#include "Mesh.h"
#include "ShapeMeshes.h"
#include "Camera.h"
#include "Device.h"

#pragma comment(lib, "winmm.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    std::vector<Mesh> Meshes;
    Meshes.push_back(ShapeMeshes::Cube());

    Camera mainCamera = Camera();

    mainCamera.Position = Vector3(0, 0, 25);
    mainCamera.Target = Vector3::Origin();

    MainWindow win;

    if (!win.Create(L"3DVisualizations", WS_OVERLAPPEDWINDOW)) {
        return 0;
    }

    win.setCamera(&mainCamera);
    win.setMeshList(&Meshes);

    ShowWindow(win.Window(), nCmdShow);
    UpdateWindow(win.Window());

    // Run the message loop.
    MSG msg;                  // Next message from top of queue
    LONGLONG cur_time;        // Current system time
    UINT32 time_count = 33;   // ms per frame, used as default if performance counter is not available
    LONGLONG perf_cnt;        // Performance timer frequency
    BOOL perf_flag = FALSE;   // Flag whether performance counter available, if false use timeGetTime()
    LONGLONG next_time = 0;   // Time to render next frame
    BOOL move_flag = TRUE;    // Flag if moved step has occurred yet

    // Is there a performance counter available?
    if (QueryPerformanceFrequency((LARGE_INTEGER*)&perf_cnt)) {
        // Yes, set time_count and timer choice flag
        perf_flag = TRUE;
        time_count = (UINT32)perf_cnt / 30;    // Calculate time per frame based on frequency (25 fps, 40 milliseconds per frame)
        QueryPerformanceCounter((LARGE_INTEGER*)&next_time);
    }
    else {
        // No performance counter, read in using timeGetTime
        next_time = timeGetTime();
    }

    // Initialize the message structure
    PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

    while (msg.message != WM_QUIT) {
        // Is there a message to process?
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            // Dispatch the message
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // Do we need to move?
            if (move_flag) {
                move_flag = FALSE;
                for (auto& mesh : Meshes) {
                    mesh.Rotation = Vector3(mesh.Rotation.X + 0.05f, mesh.Rotation.Y + 0.05f, mesh.Rotation.Z);
                }
            }
            // Use the appropriate method to get time
            if (perf_flag) {
                QueryPerformanceCounter((LARGE_INTEGER*)&cur_time);
            }
            else {
                cur_time = timeGetTime();
            }

            // Is it time to render the frame?
            if (cur_time > next_time) {
                // Render scene
                win.Render();
                // Set time for next frame
                next_time += time_count;
                // If more than a frame behind, drop the frames
                if (next_time < cur_time) {
                    next_time = cur_time + time_count;
                    OutputDebugString(L"Dropping frames...\n");
                }
                // Flag that we need to move objects again
                move_flag = TRUE;
            }
        }
    }

    return 0;
}

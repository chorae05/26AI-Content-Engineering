#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- 전역 인터페이스 포인터 ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// --- GameObject 구조체 ---
typedef struct {
    float x, y;
    float speed;
} GameObject;

// 600x600 해상도 중앙(300, 300)에서 시작, 1초에 200픽셀 이동
GameObject g_Player = { 300.0f, 300.0f, 200.0f };

bool g_KeyState[256] = { false };

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}
float4 PS(PS_INPUT input) : SV_Target { return input.col; }
)";

// --- [윈도우 프로시저] ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        if (wParam < 256) g_KeyState[wParam] = true;
        break;
    case WM_KEYUP:
        if (wParam < 256) g_KeyState[wParam] = false;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// --- [콘솔창 렌더링 함수] ---
void RenderConsole(const GameObject* player, float fps) {
    system("cls");
    printf("=== Engine Heartbeat ===\n");
    if (fps > 0)
        printf("FPS : %.2f (dt: %.4fs)\n", fps, 1.0f / fps);
    else
        printf("FPS : Calculating...\n");
    printf("Player Position: (%.2f, %.2f)\n", player->x, player->y);
    printf("Shape: Math Heart (Sin/Cos)\n"); // 상태창 업데이트
    printf("========================\n");

    // 좌표 시각화
    int py = (int)(player->y / 60.0f);
    int px = (int)(player->x / 30.0f);

    if (py < 0) py = 0;
    if (px < 0) px = 0;

    for (int i = 0; i < py; i++) printf("\n");
    for (int i = 0; i < px; i++) printf(" ");

    // 특수문자 깨짐 방지 세팅을 했으므로 진짜 하트를 출력합니다!
    printf("♥\n");
}

int main() {
    // 콘솔창 인코딩 설정 (♥ 기호 출력을 위함)
    SetConsoleOutputCP(949);

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    int nCmdShow = SW_SHOW;

    // 1. Window 등록 및 생성
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"DX11EventClass", nullptr };
    RegisterClassEx(&wcex);

    DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect = { 0, 0, 600, 600 };
    AdjustWindowRect(&rect, windowStyle, FALSE);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    HWND hWnd = CreateWindow(L"DX11EventClass", L"DirectX11 Parametric Heart (Sin/Cos)",
        windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // 2. D3D11 & SwapChain 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 600;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // 3. Shader 및 Input Layout 생성
    ID3DBlob* vsBlob = nullptr, * psBlob = nullptr;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);
    ID3D11VertexShader* vShader; ID3D11PixelShader* pShader;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);

    // =========================================================================
    // [수정 1] 부드러운 곡선을 위해 36조각(108개 정점)의 버퍼를 준비합니다.
    // =========================================================================
    const int SEGMENTS = 36;
    const int VERTEX_COUNT = SEGMENTS * 3;

    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * VERTEX_COUNT, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, nullptr, &pVBuffer);

    // --- [타이머 초기화] ---
    auto prevTime = std::chrono::high_resolution_clock::now();

    // --- Main Loop ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - prevTime;
            float dt = elapsed.count();
            if (dt < 0.0001f) dt = 0.0001f;
            prevTime = currentTime;

            if (g_KeyState['W']) g_Player.y -= g_Player.speed * dt;
            if (g_KeyState['S']) g_Player.y += g_Player.speed * dt;
            if (g_KeyState['A']) g_Player.x -= g_Player.speed * dt;
            if (g_KeyState['D']) g_Player.x += g_Player.speed * dt;

            float instantFPS = 1.0f / dt;
            RenderConsole(&g_Player, instantFPS);

            float ndcX = (g_Player.x / 300.0f) - 1.0f;
            float ndcY = -((g_Player.y / 300.0f) - 1.0f);

            // =========================================================================
            // [수정 2] Sin, Cos을 이용한 수학적 하트 정점 계산
            // =========================================================================
            Vertex vertices[VERTEX_COUNT];
            float scale = 0.025f; // 하트 전체 크기 조절
            const float PI = 3.14159265f;

            for (int i = 0; i < SEGMENTS; ++i) {
                // 360도를 36등분하여 시작 각도(t1)와 끝 각도(t2)를 구합니다.
                float t1 = (float)i / SEGMENTS * 2.0f * PI;
                float t2 = (float)(i + 1) / SEGMENTS * 2.0f * PI;

                // 1. 삼각형의 중심점 (부채꼴의 시작점) - 약간 밝은 핑크빛
                vertices[i * 3 + 0] = { 0.0f + ndcX, 0.0f + ndcY, 0.5f, 1.0f, 0.4f, 0.6f, 1.0f };

                // 2. 동그라미 2개와 아래 삼각형의 궤적을 그리는 첫 번째 하트 테두리 공식 (t1) - 강렬한 빨간색
                float x1 = 16.0f * sin(t1) * sin(t1) * sin(t1);
                float y1 = 13.0f * cos(t1) - 5.0f * cos(2.0f * t1) - 2.0f * cos(3.0f * t1) - cos(4.0f * t1);
                vertices[i * 3 + 1] = { (x1 * scale) + ndcX, (y1 * scale) + ndcY, 0.5f, 1.0f, 0.0f, 0.2f, 1.0f };

                // 3. 두 번째 하트 테두리 공식 (t2)
                float x2 = 16.0f * sin(t2) * sin(t2) * sin(t2);
                float y2 = 13.0f * cos(t2) - 5.0f * cos(2.0f * t2) - 2.0f * cos(3.0f * t2) - cos(4.0f * t2);
                vertices[i * 3 + 2] = { (x2 * scale) + ndcX, (y2 * scale) + ndcY, 0.5f, 1.0f, 0.0f, 0.2f, 1.0f };
            }
            // =========================================================================

            D3D11_MAPPED_SUBRESOURCE mapped;
            g_pImmediateContext->Map(pVBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            memcpy(mapped.pData, vertices, sizeof(vertices));
            g_pImmediateContext->Unmap(pVBuffer, 0);

            float color[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, color);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            D3D11_VIEWPORT vp = { 0, 0, 600, 600, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->IASetInputLayout(pInputLayout);
            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

            // =========================================================================
            // [수정 3] 108개의 정점을 모두 그립니다.
            // =========================================================================
            g_pImmediateContext->Draw(VERTEX_COUNT, 0);

            g_pSwapChain->Present(1, 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // --- Cleanup ---
    if (pVBuffer) pVBuffer->Release();
    if (pInputLayout) pInputLayout->Release();
    if (vShader) vShader->Release();
    if (pShader) pShader->Release();
    if (vsBlob) vsBlob->Release();
    if (psBlob) psBlob->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}
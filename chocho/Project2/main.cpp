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
    printf("Shape: Hexagram (Star) [BIG]\n"); // 상태창 업데이트
    printf("========================\n");

    // 좌표 시각화 (간이) - 간격 대폭 축소
    int py = (int)(player->y / 60.0f);
    int px = (int)(player->x / 30.0f);

    if (py < 0) py = 0;
    if (px < 0) px = 0;

    for (int i = 0; i < py; i++) printf("\n");
    for (int i = 0; i < px; i++) printf(" ");

    // 특수문자 깨짐 방지를 위해 S 사용
    printf("S\n");
}

int main() {
    // 콘솔창 인코딩 설정
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

    HWND hWnd = CreateWindow(L"DX11EventClass", L"DirectX11 Big Multi-Color Hexagram (600x600 Fixed)",
        windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // 2. D3D11 & SwapChain 초기화 (백버퍼 600x600)
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

    // 육망성을 위해 6개의 정점을 담을 수 있는 정점 버퍼를 생성합니다. (3 -> 6)
    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 6, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
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
            // [A. DeltaTime 계산]
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - prevTime;
            float dt = elapsed.count();
            if (dt < 0.0001f) dt = 0.0001f;
            prevTime = currentTime;

            // [B. Update: Input Process & Logic]
            if (g_KeyState['W']) g_Player.y -= g_Player.speed * dt;
            if (g_KeyState['S']) g_Player.y += g_Player.speed * dt;
            if (g_KeyState['A']) g_Player.x -= g_Player.speed * dt;
            if (g_KeyState['D']) g_Player.x += g_Player.speed * dt;

            // [C-1. 콘솔창 렌더링]
            float instantFPS = 1.0f / dt;
            RenderConsole(&g_Player, instantFPS);

            // [C-2. DirectX 화면 렌더링을 위한 좌표 변환 및 육망성 정점 계산]
            float ndcX = (g_Player.x / 300.0f) - 1.0f;
            float ndcY = -((g_Player.y / 300.0f) - 1.0f);

            // =========================================================================
            // [수정] 육망성 크기를 대폭 키웠습니다. (0.15f -> 0.4f)
            // =========================================================================
            float scale = 0.4f; // ★ 이 값을 0.4로 키워 삼각형들이 큼직하게 보입니다.
            float offsetX = scale * 0.866025f; // scale * (sqrt(3.0f) / 2.0f)
            float offsetY = scale * 0.5f;

            Vertex vertices[6]; // 6개의 정점을 담을 배열

            // --- 삼각형 1 (위쪽으로 뾰족한 원래 삼각형, R-G-B 그래디언트) ---
            // 위쪽 꼭지점
            vertices[0] = { 0.0f + ndcX,     scale + ndcY, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f }; // Red
            // 아래 오른쪽 꼭지점
            vertices[1] = { offsetX + ndcX, -offsetY + ndcY, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f }; // Green
            // 아래 왼쪽 꼭지점
            vertices[2] = { -offsetX + ndcX, -offsetY + ndcY, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f }; // Blue

            // --- 삼각형 2 (아래쪽으로 뾰족한 거꾸로 된 삼각형, G-B-R 그래디언트로 multi-color 효과) ---
            // 아래쪽 꼭지점
            vertices[3] = { 0.0f + ndcX,    -scale + ndcY, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f }; // Green (reversed color loop)
            // 위쪽 왼쪽 꼭지점 (시계방향 순서 준수)
            vertices[4] = { -offsetX + ndcX,  offsetY + ndcY, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f }; // Blue
            // 위쪽 오른쪽 꼭지점
            vertices[5] = { offsetX + ndcX,  offsetY + ndcY, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f }; // Red
            // =========================================================================

            // 데이터 복사
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

            // 6개의 정점을 그립니다.
            g_pImmediateContext->Draw(6, 0);

            g_pSwapChain->Present(1, 0);

            // [D. Sleep]
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
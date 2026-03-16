#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- 전역 인터페이스 포인터 ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// --- 전역 게임 상태 관리 ---
float g_TrianglePosX = 0.0f;
float g_TrianglePosY = 0.0f;
float g_MoveSpeed = 0.001f;

// [Key State Array] : 인덱스는 가상 키 코드(Virtual Key Code)를 의미함
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


// --- [윈도우 프로시저: 이벤트 기반 입력 처리 핵심] ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        // wParam에는 눌린 키의 가상 키 코드가 들어있음 (예: 'W' 등)
        if (wParam < 256) g_KeyState[wParam] = true;
        break;

    case WM_KEYUP:
        // 키를 뗐을 때 상태를 false로 변경
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Window 등록 및 생성
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"DX11EventClass", nullptr };
    RegisterClassEx(&wcex);
    HWND hWnd = CreateWindow(L"DX11EventClass", L"Event-driven Input Triangle", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // 2. D3D11 & SwapChain 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1; sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // 3. Shader 및 Input Layout 생성
    ID3DBlob* vsBlob, * psBlob;
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

    // 4. Dynamic Vertex Buffer 생성
    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 3, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, nullptr, &pVBuffer);

    // --- Main Loop ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg); // 여기서 WndProc가 호출되어 g_KeyState 배열이 업데이트됨
        }
        else {
            // [1 & 2. Input Process & Update]
            // WndProc에서 실시간으로 업데이트한 g_KeyState 배열을 참조하여 위치 연산
            if (g_KeyState['W']) g_TrianglePosY += g_MoveSpeed;
            if (g_KeyState['S']) g_TrianglePosY -= g_MoveSpeed;
            if (g_KeyState['A']) g_TrianglePosX -= g_MoveSpeed;
            if (g_KeyState['D']) g_TrianglePosX += g_MoveSpeed;

            // GPU 메모리 업데이트
            Vertex vertices[] = {
                {  0.0f + g_TrianglePosX,  0.5f + g_TrianglePosY, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
                {  0.5f + g_TrianglePosX, -0.5f + g_TrianglePosY, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
                { -0.5f + g_TrianglePosX, -0.5f + g_TrianglePosY, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
            };
            D3D11_MAPPED_SUBRESOURCE mapped;
            g_pImmediateContext->Map(pVBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            memcpy(mapped.pData, vertices, sizeof(vertices));
            g_pImmediateContext->Unmap(pVBuffer, 0);

            // [3. Render]
            float color[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, color);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->IASetInputLayout(pInputLayout);
            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

            g_pImmediateContext->Draw(3, 0);
            g_pSwapChain->Present(1, 0);
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
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <math.h>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// =========================================================
// [1] 타이머 구조체 및 함수 정의
// =========================================================
typedef struct {
    LARGE_INTEGER frequency;
    LARGE_INTEGER prevTime;
    double deltaTime;
} CGameTimer;

void InitTimer(CGameTimer* timer) {
    QueryPerformanceFrequency(&timer->frequency);
    QueryPerformanceCounter(&timer->prevTime);
    timer->deltaTime = 0.0;
}

void UpdateTimer(CGameTimer* timer) {
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    timer->deltaTime = (double)(currentTime.QuadPart - timer->prevTime.QuadPart) / (double)timer->frequency.QuadPart;
    timer->prevTime = currentTime;
}

// =========================================================
// [2] 전역 객체 및 정점 구조체 (수학 좌표용 UV 추가)
// =========================================================
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
    float u, v; // 방정식을 계산하기 위한 내부 좌표계
};

// =========================================================
// [핵심] 픽셀 셰이더 내부에 수학 방정식 적용!
// =========================================================
const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; float2 uv : TEXCOORD; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; float2 uv : TEXCOORD; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    output.uv = input.uv;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    // 1. UV 좌표(0~1)를 수학 계산하기 편한 -1.0 ~ 1.0 범위의 (x, y) 그래프 좌표계로 변환
    float x = input.uv.x * 2.0f - 1.0f;
    float y = input.uv.y * 2.0f - 1.0f;

    // 2. [딸기 몸통 방정식] 타원의 방정식(x^2/a^2 + y^2/b^2 = 1) 응용
    // 아래로 갈수록 폭(반지름)이 좁아지도록 y값에 따라 반경을 조절
    float radius = 0.6f - (y * 0.2f); 
    float bodyEquation = (x * x) / (radius * radius) + (y * y) / (0.8f * 0.8f);

    // 3. [딸기 꼭지 방정식] 상단에 위치한 납작한 타원
    float leafEquation = (x * x) / (0.5f * 0.5f) + ((y + 0.7f) * (y + 0.7f)) / (0.3f * 0.3f);

    // 4. [씨앗 방정식] 사인(Sine) 파동을 교차시켜 격자 무늬 생성
    float seedPattern = sin(x * 30.0f) * sin(y * 30.0f);

    // 5. 방정식 결과를 바탕으로 픽셀 색상 결정
    if (leafEquation < 1.0f && y < -0.3f) {
        return float4(0.1f, 0.8f, 0.2f, 1.0f); // 꼭지 (초록색)
    }
    else if (bodyEquation < 1.0f) {
        if (seedPattern > 0.85f) {
            return float4(0.1f, 0.0f, 0.0f, 1.0f); // 씨앗 (검은색/어두운 빨강)
        }
        return float4(0.9f, 0.1f, 0.2f, 1.0f); // 몸통 (빨간색)
    }

    // 방정식에 속하지 않는 배경 부분은 투명하게 날려버림
    discard;
    return float4(0, 0, 0, 0);
}
)";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// =========================================================
// [3] 메인 함수
// =========================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    // --- 윈도우 생성 ---
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11MathStrawberryClass";
    RegisterClassExW(&wcex);

    LONG screenWidth = 600;
    LONG screenHeight = 600;

    RECT rc = { 0, 0, screenWidth, screenHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(L"DX11MathStrawberryClass", L"과제: 수학 방정식으로 딸기 그리기",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return -1;
    ShowWindow(hWnd, nCmdShow);

    // --- DX11 초기화 ---
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = screenWidth;
    sd.BufferDesc.Height = screenHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // --- 셰이더 컴파일 ---
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    // 레이아웃에 TEXCOORD(UV 좌표) 추가
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release();

    // --- 정점 버퍼 (방정식을 그릴 도화지 역할인 사각형) ---
    // 육망성 대신 4개의 정점으로 네모 판자를 만듭니다.
    Vertex baseVertices[] = {
        { -0.4f,  0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f }, // 좌상단
        {  0.4f,  0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f }, // 우상단
        { -0.4f, -0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f }, // 좌하단
        {  0.4f, -0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }, // 우하단
    };

    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(baseVertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { baseVertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);

    // --- 게임 루프 준비 ---
    CGameTimer myTimer;
    InitTimer(&myTimer);

    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float speed = 1.0f;
    float elapsedTime = 0.0f;
    int frameCount = 0;

    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            UpdateTimer(&myTimer);

            if (GetAsyncKeyState('W') & 0x8000) offsetY += speed * (float)myTimer.deltaTime;
            if (GetAsyncKeyState('S') & 0x8000) offsetY -= speed * (float)myTimer.deltaTime;
            if (GetAsyncKeyState('A') & 0x8000) offsetX -= speed * (float)myTimer.deltaTime;
            if (GetAsyncKeyState('D') & 0x8000) offsetX += speed * (float)myTimer.deltaTime;

            Vertex currentVertices[4];
            for (int i = 0; i < 4; i++) {
                currentVertices[i] = baseVertices[i];
                currentVertices[i].x += offsetX;
                currentVertices[i].y += offsetY;
            }

            g_pImmediateContext->UpdateSubresource(pVBuffer, 0, nullptr, currentVertices, 0, 0);

            // 매 프레임마다 시간과 프레임 횟수 누적
            elapsedTime += (float)myTimer.deltaTime;
            frameCount++;

            // =========================================================
            // [수정 완료!] 누적된 시간이 1초(1.0f)를 넘었을 때 델타타임과 FPS를 딱 한 번 출력
            // =========================================================
            if (elapsedTime >= 1.0f) {
                printf("DeltaTime: %.6f\n", myTimer.deltaTime);
                printf("[1초 경과] 렌더링 프레임: %d FPS\n\n", frameCount);

                elapsedTime = 0.0f;
                frameCount = 0;
            }

            // 렌더링 (배경색을 어두운 회색으로)
            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            D3D11_VIEWPORT vp = { 0, 0, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);

            g_pImmediateContext->IASetInputLayout(pInputLayout);
            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

            // 사각형을 그리기 위해 TRIANGLESTRIP 사용
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

            // 4개의 정점으로 그리기
            g_pImmediateContext->Draw(4, 0);

            g_pSwapChain->Present(0, 0);
        }
    }

    if (pVBuffer) pVBuffer->Release();
    if (pInputLayout) pInputLayout->Release();
    if (vShader) vShader->Release();
    if (pShader) pShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}
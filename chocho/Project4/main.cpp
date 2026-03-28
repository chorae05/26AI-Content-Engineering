#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <math.h>

// [링커 옵션] 윈도우 창과 콘솔창을 동시에 활성화하여 실시간 델타타임/FPS 로그를 확인하기 위한 설정
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// =========================================================
// [1] 타이머 구조체 및 함수 정의 (고해상도 시간 제어 파트)
// =========================================================
typedef struct {
    LARGE_INTEGER frequency; // CPU의 초당 진동 횟수 (정규화의 기준)
    LARGE_INTEGER prevTime;  // 바로 직전 프레임의 측정 시점
    double deltaTime;        // 두 프레임 사이의 아주 미세한 간격 (초 단위)
} CGameTimer;

void InitTimer(CGameTimer* timer) {
    // 하드웨어마다 다른 CPU 클럭 주파수를 초기에 파악하여, 틱(Tick) 단위를 '초' 단위로 정규화함
    QueryPerformanceFrequency(&timer->frequency);
    QueryPerformanceCounter(&timer->prevTime);
    timer->deltaTime = 0.0;
}

// 프레임 간격(DeltaTime)을 측정하여 하드웨어 성능과 관계없는 일관된 물리 법칙 적용 준비
void UpdateTimer(CGameTimer* timer) {
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    // (현재 틱 - 이전 틱) / 주사수 = 0.001초 단위의 정밀한 시간 간격 산출
    timer->deltaTime = (double)(currentTime.QuadPart - timer->prevTime.QuadPart) / (double)timer->frequency.QuadPart;
    timer->prevTime = currentTime; // 현재 시간을 기록하여 다음 프레임의 기준점으로 넘김
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
    float u, v; // [중요] 픽셀 셰이더 내에서 수학 방정식을 계산하기 위한 정규화된 좌표계 (0~1)
};

// =========================================================
// [핵심] 픽셀 셰이더: 보노보노 얼굴 수학 방정식 적용
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

// 수학적 도형 함수: 원
float sdCircle(float2 p, float r) {
    return length(p) - r;
}

// 수학적 도형 함수: 선분 (수염 그리기용)
float sdLineSegment(float2 p, float2 a, float2 b) {
    float2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

float4 PS(PS_INPUT input) : SV_Target {
    // 1. [좌표계 재정의] UV 좌표(0~1)를 중앙이 (0,0)인 수학적 NDC 좌표계(-1.0 ~ 1.0)로 변환
    float2 p = input.uv * 2.0f - 1.0f;
    p.y = -p.y; // Y축 뒤집힘 보정

    float4 charColor = float4(0, 0, 0, 0);

    // 보노보노 색상 정의
    float4 bodyBlue = float4(0.48f, 0.77f, 1.0f, 1.0f); 
    float4 eyeNoseBlack = float4(0.0f, 0.0f, 0.0f, 1.0f); 
    float4 muzzleWhite = float4(1.0f, 1.0f, 1.0f, 1.0f); 
    float4 outlineColor = float4(0.0f, 0.0f, 0.0f, 1.0f); 
    
    float edgeThickness = 0.02f; 
    float blur = 0.005f; 

    // [머리] 
    float dHead = sdCircle(p, 0.8f); 

    // [최적화 & 배경 투명화] 알파 블렌딩 상태 설정 없이도 배경을 보이게 하기 위한 discard (기존 딸기 코드와 동일)
    if (dHead > edgeThickness) {
        discard;
        return float4(0, 0, 0, 0);
    }

    // 기본 파란색 얼굴 및 외곽선 합성
    charColor = lerp(bodyBlue, outlineColor, smoothstep(-edgeThickness, 0.0f, dHead));

    // --- 이목구비 방정식 ---
    // 하얀 볼
    float2 muzLPos = float2(-0.16f, -0.24f);
    float dMuzL = sdCircle(p - muzLPos, 0.32f);
    float dMuzR = sdCircle(p - float2(0.16f, -0.24f), 0.32f);
    float dMuzzle = min(dMuzL, dMuzR); 

    // 코
    float dNose = sdCircle(p - float2(0.0f, -0.04f), 0.11f); 

    // 눈
    float eyeSize = 0.044f; 
    float dEyeL = sdCircle(p - float2(-0.36f, 0.20f), eyeSize);
    float dEyeR = sdCircle(p - float2(0.36f, 0.20f), eyeSize);
    float dEyes = min(dEyeL, dEyeR); 

    // 수염
    float2 fanStartL = muzLPos - float2(0.12f, 0.0f);
    float2 fanStartR = float2(0.16f, -0.24f) + float2(0.12f, 0.0f);
    
    float dW_L1 = sdLineSegment(p, fanStartL, fanStartL - float2(0.36f, -0.1f));
    float dW_L2 = sdLineSegment(p, fanStartL, fanStartL - float2(0.36f, 0.0f));
    float dW_L3 = sdLineSegment(p, fanStartL, fanStartL - float2(0.36f, 0.1f));
    float dW_L = min(dW_L1, min(dW_L2, dW_L3));
    
    float dW_R1 = sdLineSegment(p, fanStartR, fanStartR + float2(0.36f, -0.1f));
    float dW_R2 = sdLineSegment(p, fanStartR, fanStartR + float2(0.36f, 0.0f));
    float dW_R3 = sdLineSegment(p, fanStartR, fanStartR + float2(0.36f, 0.1f));
    float dW_R = min(dW_R1, min(dW_R2, dW_R3));
    
    float dWhiskers = min(dW_L, dW_R); 

    // 색상 덮어쓰기 (레이어 순서)
    float muzzleAlpha = 1.0f - smoothstep(0.0f, blur, dMuzzle);
    charColor = lerp(charColor, muzzleWhite, muzzleAlpha);
    
    float dFeatures = min(dNose, min(dEyes, dWhiskers));
    float featuresAlpha = 1.0f - smoothstep(-0.01f, 0.0f, dFeatures); 
    charColor = lerp(charColor, eyeNoseBlack, featuresAlpha);

    return charColor;
}
)";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// =========================================================
// [3] 메인 엔진 진입점 (WinMain)
// =========================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    // --- 1단계: 윈도우 창 환경 구축 ---
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11BonobonoFaceMoveClass";
    RegisterClassExW(&wcex);

    LONG screenWidth = 600;
    LONG screenHeight = 600;

    RECT rc = { 0, 0, screenWidth, screenHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(L"DX11BonobonoFaceMoveClass", L"과제: 보노보노 W,A,S,D 자유 이동",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return -1;
    ShowWindow(hWnd, nCmdShow);

    // --- 2단계: DirectX 11 렌더링 파이프라인 초기화 ---
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = screenWidth;
    sd.BufferDesc.Height = screenHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    // GPU 장치(Device) 및 전/후면 버퍼 스왑 체인 생성
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // --- 3단계: 셰이더 컴파일 및 입력 레이아웃 구성 ---
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release();

    // --- 4단계: 정점 버퍼 (보노보노 얼굴이 찌그러지지 않도록 정사각형 비율로 수정) ---
    Vertex baseVertices[] = {
        { -0.5f,  0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f }, // 좌상단
        {  0.5f,  0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f }, // 우상단
        { -0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f }, // 좌하단
        {  0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }, // 우하단
    };

    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(baseVertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { baseVertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);

    // --- 5단계: 실시간 루프 제어 변수 설정 ---
    CGameTimer myTimer;
    InitTimer(&myTimer);

    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float speed = 1.0f;
    float elapsedTime = 0.0f;
    int frameCount = 0;

    // --- [핵심] 메인 엔진 루프 (Game Loop) ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        // PeekMessage: 메시지(클릭, 종료 등)가 없어도 멈추지 않고 실시간 업데이트 수행 (Non-blocking)
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 1. [실시간 측정] 루프 한 바퀴의 시간(DeltaTime) 측정
            UpdateTimer(&myTimer);

            // 2. [인풋 및 물리] 프레임 독립성 이동 (속도 * 델타타임)
            // 어떤 사양의 PC에서도 동일한 시간 기반 이동 거리 보장
            if (GetAsyncKeyState('W') & 0x8000) offsetY += speed * (float)myTimer.deltaTime;
            if (GetAsyncKeyState('S') & 0x8000) offsetY -= speed * (float)myTimer.deltaTime;
            if (GetAsyncKeyState('A') & 0x8000) offsetX -= speed * (float)myTimer.deltaTime;
            if (GetAsyncKeyState('D') & 0x8000) offsetX += speed * (float)myTimer.deltaTime;

            // 3. [데이터 동기화] 변한 좌표 정보를 GPU 정점 버퍼에 즉시 갱신
            Vertex currentVertices[4];
            for (int i = 0; i < 4; i++) {
                currentVertices[i] = baseVertices[i];
                currentVertices[i].x += offsetX;
                currentVertices[i].y += offsetY;
            }
            g_pImmediateContext->UpdateSubresource(pVBuffer, 0, nullptr, currentVertices, 0, 0);

            // 4. [성능 모니터링] 실시간 델타타임 출력 및 1초 단위 최적화된 FPS 측정
            // 매 프레임 찍지 않고 1초마다 출력하여 콘솔 I/O 오버헤드 방지
            printf("DeltaTime: %.6f\n", myTimer.deltaTime);

            elapsedTime += (float)myTimer.deltaTime;
            frameCount++;
            if (elapsedTime >= 1.0f) {
                printf("\n[1초 경과] 렌더링 프레임: %d FPS\n\n", frameCount);
                elapsedTime = 0.0f;
                frameCount = 0;
            }

            // 5. [렌더링 시작] 화면 지우기 및 도화지 설정
            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            D3D11_VIEWPORT vp = { 0, 0, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);

            // 6. [그리기 명령] 파이프라인에 셰이더 바인딩 및 Draw Call 실행
            g_pImmediateContext->IASetInputLayout(pInputLayout);
            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

            g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

            // 정점 4개만 그리지만, 실제 모양은 픽셀 셰이더 내의 '수학 방정식'이 결정함
            g_pImmediateContext->Draw(4, 0);

            // 전면/후면 버퍼 스왑하여 최종 결과물 출력
            g_pSwapChain->Present(0, 0);
        }
    }

    // --- 마무리: 메모리 누수 방지를 위한 자원 해제(Release) ---
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
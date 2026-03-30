#include <chrono>         // [시간 측정] 프레임 간격(DeltaTime)을 아주 정밀하게 측정하기 위한 C++ 시간 라이브러리
#include <d3d11.h>        // [DirectX 핵심] DirectX 11을 이용해 GPU를 제어하고 그래픽을 그리기 위한 핵심 기능
#include <d3dcompiler.h>  // [셰이더 번역기] 
#include <stdio.h>        // [입출력] FPS나 델타 타임 같은 정보를 콘솔창(검은 창)에 출력(printf)하기 위해 필요
#include <math.h>         // [수학] C++ 내부에서 sin, cos 같은 수학 계산이 필요할 때 사용하는 기본 수학 라이브러리입
// [창 두 개 띄우기] 원래 윈도우 프로그램은 '게임창'만 뜨거나 '콘솔창(검은창)'만 뜰 수 있는데, 
// 이 설정을 넣으면 게임 화면과 디버그용 콘솔창을 동시에 띄울 수 있습니다.
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
// 비주얼 스튜디오 설정 창에 번거롭게 들어가지 않고도, 코드 한 줄로 DirectX 라이브러리들을 자동으로 연결해주는 마법의 명령어들입니다.
#pragma comment(lib, "d3d11.lib")         // DirectX 11 기능이 담긴 실제 라이브러리 부품 연결
#pragma comment(lib, "dxgi.lib")          // 모니터에 화면을 출력하고 버퍼를 교체(Swap Chain)해주는 기초 그래픽 인프라 부품 연결
#pragma comment(lib, "d3dcompiler.lib")   // 셰이더 번역기 라이브러리 부품 연결
// =========================================================
// [1] 타이머 구조체 및 함수 정의
// =========================================================
typedef struct {
    LARGE_INTEGER frequency;    //cpu의 초당 진동 횟수
	LARGE_INTEGER prevTime;     //이전 프레임의 시간 (쿼리 성능 카운터로 측정)
	double deltaTime;   //이전 프레임과 현재 프레임 사이의 시간 간격 (초 단위)
} CGameTimer;

void InitTimer(CGameTimer* timer) {
   // 하드웨어마다 다른 cpu 클릭 주파수를 초기에 파악하여, 틱 단위로 '초'단위로 정규화함
    QueryPerformanceFrequency(&timer->frequency);
    QueryPerformanceCounter(&timer->prevTime);
    timer->deltaTime = 0.0;
}
//프레임 간격(DaltaTime)을 측정하여 하드웨어 성능과 관계없는 일관된 물리 법칙 적용 준비
void UpdateTimer(CGameTimer* timer) {
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    timer->deltaTime = (double)(currentTime.QuadPart - timer->prevTime.QuadPart) / (double)timer->frequency.QuadPart;
    timer->prevTime = currentTime;
}

// =========================================================
// [2] 전역 객체 및 정점 구조체
// =========================================================
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
    float u, v;
};

// =========================================================
// [3] 픽셀 셰이더 소스 (보노보노 및 바닥 타일)
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

float sdCircle(float2 p, float r) { return length(p) - r; }
float sdLineSegment(float2 p, float2 a, float2 b) {
    float2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

float4 PS(PS_INPUT input) : SV_Target {
    float2 p = input.uv * 2.0f - 1.0f;
    p.y = -p.y;
    float4 bodyBlue = float4(0.48f, 0.77f, 1.0f, 1.0f);
    float dHead = sdCircle(p, 0.8f);
    if (dHead > 0.02f) discard;
    float4 charColor = lerp(bodyBlue, float4(0,0,0,1), smoothstep(-0.02f, 0.0f, dHead));
    float dMuz = min(sdCircle(p - float2(-0.16f, -0.24f), 0.32f), sdCircle(p - float2(0.16f, -0.24f), 0.32f));
    charColor = lerp(charColor, float4(1,1,1,1), 1.0 - smoothstep(0.0, 0.005, dMuz));
    float dEyes = min(sdCircle(p - float2(-0.36f, 0.20f), 0.044f), sdCircle(p - float2(0.36f, 0.20f), 0.044f));
    float dNose = sdCircle(p - float2(0.0f, -0.04f), 0.11f);
    float dW1 = sdLineSegment(p, float2(-0.28f, -0.24f), float2(-0.64f, -0.24f));
    float dW2 = sdLineSegment(p, float2(0.28f, -0.24f), float2(0.64f, -0.24f));
    float dFeatures = min(dNose, min(dEyes, min(dW1, dW2)));
    return lerp(charColor, float4(0,0,0,1), 1.0 - smoothstep(-0.01, 0.0, dFeatures));
}
)";

// =========================================================
// [수정] 픽셀 셰이더: 격자 무늬 대신 초록색 잔디 방정식 적용
// =========================================================
const char* tileShaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float2 uv : TEXCOORD; };
struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.uv = input.uv;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    // 1. [좌표계] UV 좌표를 바탕으로 노이즈 패턴 생성
    float2 p = input.uv;
    
    // 2. [잔디 질감 방정식] 사인 함수를 여러 겹 교차시켜 무작위 점 패턴 생성
    float texturePattern = sin(p.x * 300.0f) * sin(p.y * 300.0f);
    
    // 3. [컬러링] 두 가지 초록색을 섞어서 잔디밭 색상 구현
    float3 darkGrass = float3(0.0f, 0.4f, 0.1f);  // 짙은 초록
    float3 lightGrass = float3(0.1f, 0.6f, 0.2f); // 밝은 초록
    
    // 패턴 값에 따라 색상을 부드럽게 보간 (lerp)
    float mixFactor = smoothstep(0.0, 0.5, texturePattern);
    float3 finalCol = lerp(darkGrass, lightGrass, mixFactor);

    

    return float4(finalCol, 1.0f);
}
)";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// =========================================================
// [4] 메인 엔진 진입점 (WinMain)
// =========================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1단계: 창 설정
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc; wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW); wcex.lpszClassName = L"DX11BonoTileGame";
    RegisterClassExW(&wcex);

    LONG screenWidth = 600; LONG screenHeight = 600;
    RECT rc = { 0, 0, screenWidth, screenHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindowW(L"DX11BonoTileGame", L"보노보노 게임: 타일 지면 추가", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // 2단계: DX11 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1; sd.BufferDesc.Width = screenWidth; sd.BufferDesc.Height = screenHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBB; g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBB);
    g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_pRenderTargetView); pBB->Release();

    // 3단계: 보노보노 셰이더 및 레이아웃
    ID3DBlob* vsB, * psB;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsB, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psB, nullptr);
    ID3D11VertexShader* vS; ID3D11PixelShader* pS;
    g_pd3dDevice->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), nullptr, &vS);
    g_pd3dDevice->CreatePixelShader(psB->GetBufferPointer(), psB->GetBufferSize(), nullptr, &pS);
    D3D11_INPUT_ELEMENT_DESC ied[] = { {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0}, {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}, {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,28,D3D11_INPUT_PER_VERTEX_DATA,0} };
    ID3D11InputLayout* iL; g_pd3dDevice->CreateInputLayout(ied, 3, vsB->GetBufferPointer(), vsB->GetBufferSize(), &iL);
    vsB->Release(); psB->Release();

    // 4단계: 타일 셰이더 및 레이아웃
    ID3DBlob* tVsB, * tPsB;
    D3DCompile(tileShaderSource, strlen(tileShaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &tVsB, nullptr);
    D3DCompile(tileShaderSource, strlen(tileShaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &tPsB, nullptr);
    ID3D11VertexShader* tVS; ID3D11PixelShader* tPS;
    g_pd3dDevice->CreateVertexShader(tVsB->GetBufferPointer(), tVsB->GetBufferSize(), nullptr, &tVS);
    g_pd3dDevice->CreatePixelShader(tPsB->GetBufferPointer(), tPsB->GetBufferSize(), nullptr, &tPS);
    D3D11_INPUT_ELEMENT_DESC tied[] = { {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0}, {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0} };
    ID3D11InputLayout* tiL; g_pd3dDevice->CreateInputLayout(tied, 2, tVsB->GetBufferPointer(), tVsB->GetBufferSize(), &tiL);
    tVsB->Release(); tPsB->Release();

    // 5단계: 정점 버퍼 (보노보노 및 타일)
    Vertex bonoV[] = { {-0.4f,0.4f,0.5f,1,1,1,1,0,0}, {0.4f,0.4f,0.5f,1,1,1,1,1,0}, {-0.4f,-0.4f,0.5f,1,1,1,1,0,1}, {0.4f,-0.4f,0.5f,1,1,1,1,1,1} };
    ID3D11Buffer* pVBuffer; D3D11_BUFFER_DESC bd = { sizeof(bonoV), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
    D3D11_SUBRESOURCE_DATA id = { bonoV }; g_pd3dDevice->CreateBuffer(&bd, &id, &pVBuffer);

    struct TileV { float x, y, z; float u, v; };
    TileV tileV[] = { {-1.0f,-0.6f,0.6f,0,0}, {1.0f,-0.6f,0.6f,1,0}, {-1.0f,-1.0f,0.6f,0,1}, {1.0f,-1.0f,0.6f,1,1} };
    ID3D11Buffer* pTileVB; D3D11_BUFFER_DESC tbd = { sizeof(tileV), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
    D3D11_SUBRESOURCE_DATA tid = { tileV }; g_pd3dDevice->CreateBuffer(&tbd, &tid, &pTileVB);

    // 6단계: 제어 변수
    CGameTimer myTimer; InitTimer(&myTimer);
    float px = 0.0f, py = -0.5f, vy = 0.0f, rz = 0.0f;
    const float GRAVITY = -9.8f, JUMP = 5.0f, GROUND_Y = -0.5f, SPEED = 1.5f;
    bool grounded = true;
    float et = 0.0f; int fc = 0;

    // 메인 루프
    MSG msg = { 0 };
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else {
            UpdateTimer(&myTimer);
            float dt = (float)myTimer.deltaTime;

            if (GetAsyncKeyState('A') & 0x8000) { px -= SPEED * dt; rz = 1.5708f; }
            else if (GetAsyncKeyState('D') & 0x8000) { px += SPEED * dt; rz = -1.5708f; }
            else { rz *= 0.9f; }

            if ((GetAsyncKeyState('W') & 0x8000) && grounded) { vy = JUMP; grounded = false; }
            if (!grounded) { vy += GRAVITY * dt; py += vy * dt; }
            if (py <= GROUND_Y) { py = GROUND_Y; vy = 0; grounded = true; }

            Vertex curV[4];
            float cz = cosf(rz), sz = sinf(rz);
            for (int i = 0; i < 4; i++) {
                float rx = bonoV[i].x * cz - bonoV[i].y * sz;
                float ry = bonoV[i].x * sz + bonoV[i].y * cz;
                curV[i] = bonoV[i]; curV[i].x = rx + px; curV[i].y = ry + py;
            }
            g_pImmediateContext->UpdateSubresource(pVBuffer, 0, nullptr, curV, 0, 0);

            // 콘솔 출력 (딸기 스타일)
            printf("DeltaTime: %.6f\n", myTimer.deltaTime);
            et += dt; fc++;
            if (et >= 1.0f) { printf("\n[1초 경과] 렌더링 프레임: %d FPS\n\n", fc); et = 0.0f; fc = 0; }

            // 렌더링
            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            D3D11_VIEWPORT vp = { 0, 0, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);

            // 바닥 먼저 그리기
            g_pImmediateContext->IASetInputLayout(tiL);
            UINT ts = 20, to = 0; g_pImmediateContext->IASetVertexBuffers(0, 1, &pTileVB, &ts, &to);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            g_pImmediateContext->VSSetShader(tVS, nullptr, 0);
            g_pImmediateContext->PSSetShader(tPS, nullptr, 0);
            g_pImmediateContext->Draw(4, 0);

            // 보노보노 그리기
            g_pImmediateContext->IASetInputLayout(iL);
            UINT s = sizeof(Vertex), o = 0; g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &s, &o);
            g_pImmediateContext->VSSetShader(vS, nullptr, 0);
            g_pImmediateContext->PSSetShader(pS, nullptr, 0);
            g_pImmediateContext->Draw(4, 0);

            g_pSwapChain->Present(1, 0);
        }
    }

    // 해제
    pVBuffer->Release(); pTileVB->Release(); iL->Release(); tiL->Release();
    vS->Release(); pS->Release(); tVS->Release(); tPS->Release();
    g_pRenderTargetView->Release(); g_pSwapChain->Release();
    g_pImmediateContext->Release(); g_pd3dDevice->Release();
    return (int)msg.wParam;
}
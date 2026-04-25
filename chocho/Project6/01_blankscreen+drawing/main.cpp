/*
 * [강의 노트: DirectX 11 & Win32 GameLoop]
 * 1. WinMain: 프로그램의 입구
 * 2. WndProc: OS가 보낸 우편물(메시지)을 확인하는 곳
 * 3. GameLoop: 쉬지 않고 Update와 Render를 반복하는 엔진의 심장
 * 4. Release: 빌려온 GPU 메모리를 반드시 반납하는 습관 (메모리 누수 방지)
 */

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

 // --- [전역 객체 관리] ---
 // DirectX 객체들은 GPU 메모리를 직접 사용함. 
 // 사용 후 'Release()'를 호출하지 않으면 프로그램 종료 후에도 메모리가 점유될 수 있음(메모리 누수).
ID3D11Device* g_pd3dDevice = nullptr;          // // [공장장] 버퍼, 셰이더 등 각종 리소스를 메모리에 '생성'하는 역할
ID3D11DeviceContext* g_pImmediateContext = nullptr;   // [일꾼] 생성된 리소스를 파이프라인에 꽂고 "그려라" 명령하는 역할
IDXGISwapChain* g_pSwapChain = nullptr;          // [마술사] 뒷면 도화지(Back)와 앞면(Front)을 교체하는 더블 버퍼링 담당
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;   // [진짜 도화지] 픽셀 셰이더가 결과를 출력할 최종 타겟

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// HLSL (High-Level Shading Language) 소스
const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f); // 3D 좌표를 4D로 확장
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col; // 정점에서 계산된 색상을 픽셀에 그대로 적용
}
)";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. 윈도우 등록 및 생성
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11GameLoopClass";
    RegisterClassExW(&wcex);

    // 실제 창을 만듦. HWND(번호표)를 받아옴.
    HWND hWnd = CreateWindowW(L"DX11GameLoopClass", L"과제: 움직이는 육망성 만들기",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return -1;
    ShowWindow(hWnd, nCmdShow);

    // 2. DX11 디바이스 및 스왑 체인 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;    // 뒷면 도화지는 딱 1장만 더 준비
    sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // RGBA 8비트씩 사용
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;   // 이 버퍼를 '도화지'로 쓰겠다!
    sd.OutputWindow = hWnd;     // 아까 만든 창 번호표(hWnd)에 그림을 띄우겠다!
    sd.SampleDesc.Count = 1;        // 안티앨리어싱(부드럽게 하기) 끄기
    sd.Windowed = TRUE;       // 창 모드 사용

    // GPU와 통신할 통로(Device)와 화면(SwapChain)을 생성함.
    // [중요 함수] 디바이스(공장), 컨텍스트(일꾼), 스왑체인(마술사)을 한 번에 생성
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    // 렌더 타겟 설정 (도화지 준비)
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release(); // 뷰를 생성했으므로 원본 텍스트는 바로 해제 (중요!)

    // 3. 셰이더 컴파일 및 생성
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    /*
     typedef struct D3D11_INPUT_ELEMENT_DESC {
         LPCSTR                     SemanticName;         // 1. 의미 (이름)
         UINT                       SemanticIndex;        // 2. 번호 (인덱스)
         DXGI_FORMAT                Format;               // 3. 데이터 형식 (크기/타입)
         UINT                       InputSlot;            // 4. 입력 슬롯 (통로)
         UINT                       AlignedByteOffset;    // 5. 오프셋 (시작 지점)
         D3D11_INPUT_CLASSIFICATION InputSlotClass;       // 6. 클래스 (데이터 성격)
         UINT                       InstanceDataStepRate; // 7. 인스턴싱 스텝
     } D3D11_INPUT_ELEMENT_DESC;
     */

         //(1) {   HLSL 이름 : SemanticName(예 : "POSITION"), 
         //(2) HLSL 번호: SemanticIndex (보통 0), 
         //(3) 데이터 종류: Format (숫자 몇 개인지, 실수(FLOAT)인지 결정),
         //(4) 데이터 통로 번호: InputSlot (우린 주로 0번 고속도로 사용),
         //(5) 데이터 시작점: AlignedByteOffset (위치는 0, 색상은 앞에 위치가 먹은 12바이트 뒤부터!)
         //(6) 데이터 바뀜 규칙: InputSlotClass (점마다 바뀜: PER_VERTEX_DATA),
         //(7) 인스턴스 데이터 넘김: InstanceDataStepRate (지금은 안 쓰니까 무조건 0)   }
         
		 // 자세한 내용은 01_drawtriangle/main.cpp 참고
          
     //정점의 데이터 형식을 정의 (IA 단계에 알려줌)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release(); // 컴파일용 임시 메모리 해제

    // 4. 정점 버퍼 생성 (삼각형 데이터)
    Vertex vertices[] = {
        // [1. 첫 번째 삼각형: 정방향 (위가 뾰족)]
        // 순서: 위 -> 우하 -> 좌하 (시계 방향)
        {  0.0f,  0.6f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f }, // 위
        {  0.5f, -0.3f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f }, // 우하
        { -0.5f, -0.3f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f }, // 좌하

        // [2. 두 번째 삼각형: 역방향 (아래가 뾰족)]
        // 순서: 아래 -> 좌상 -> 우상 (시계 방향)
        // ★ 주의: 뒤집힌 삼각형은 점 찍는 순서가 바뀌어야 안 사라져!
        {  0.0f, -0.6f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f }, // 아래
        { -0.5f,  0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f }, // 좌상
        {  0.5f,  0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f }  // 우상
    };
    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);

    // --- [5. 정석 게임 루프] ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        // (1) 입력 단계: PeekMessage는 메시지가 없어도 바로 리턴함 (Non-blocking)
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); // 키보드 입력 등 변환
            DispatchMessage(&msg);  // 상황실(WndProc)로 전달
        }
        else {
            // (2) 업데이트 단계: 여기서 캐릭터의 위치나 로직을 계산함
            // (과제: GetAsyncKeyState 등을 써서 posX, posY를 변경하셈)

            // (3) 출력 단계: 변한 데이터를 바탕으로 화면에 그림
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // 렌더링 파이프라인 상태 설정||  일꾼에게 어떤 도구를 쓸지 알려줌
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };  // 창 크기에 맞춰 그릴 범위 설정
            g_pImmediateContext->RSSetViewports(1, &vp);

            g_pImmediateContext->IASetInputLayout(pInputLayout);        // 통역기 장착
            UINT stride = sizeof(Vertex), offset = 0;       // "한 점의 크기는 28바이트다!"
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset); // 데이터 상자 올리기

            //[IA 단계]  Primitive Topology 설정: 삼각형 리스트로 연결하라!
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            g_pImmediateContext->VSSetShader(vShader, nullptr, 0);  // 정점 마법 장착
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0);  // 픽셀 마법 장착

            // 최종 그리기
            // Draw(이번 호출에서 그릴 정점(Vertex)의 개수 , 정점 버퍼의 어느 인덱스부터 읽기 시작할 것인가)
            // [최종 명령어] 드디어 그림! (점 3개를 0번부터 읽어서 그려라)
            g_pImmediateContext->Draw(6, 0);

            // ---------------------------------------------------------
// [8단계: 화면 교체] 뒷면 도화지와 앞면 도화지를 교체 (더블 버퍼링)
// ---------------------------------------------------------
/* * swapChain->Present(SyncInterval, Flags)
 * * 1. 첫 번째 인자 (SyncInterval): 수직 동기화(V-Sync) 설정
 * - [0]: 즉시 출력 (V-Sync Off).
 * GPU가 다 그리면 모니터 주사율을 무시하고 바로 화면을 교체함.
 * ※ 장점: 프레임 제한이 없어 반응 속도가 빠름.
 * ※ 단점: 화면이 어긋나는 '티어링(Tearing)' 현상 발생 가능.
 * * - [1~4]: 수직 동기화 활성화 (V-Sync On).
 * 모니터가 화면을 1번(또는 2~4번) 갱신할 때까지 기다렸다가 교체함.
 * ※ 장점: 화면 찢어짐이 없고 출력이 안정적임.
 * ※ 단점: 프레임이 모니터 주사율(예: 60Hz)에 고정됨.
 * * 2. 두 번째 인자 (Flags): 출력 옵션 설정
 * - [0]: 일반적인 출력. 특별한 옵션 없이 화면을 교체함.
 * - [DXGI_PRESENT_TEST]: 실제 교체는 하지 않고, 장치가 출력 준비가 됐는지 테스트만 함.
 * - [DXGI_PRESENT_DO_NOT_WAIT]: GPU가 바빠서 대기해야 한다면, 기다리지 않고 즉시 에러 반환.
 */
            
            g_pSwapChain->Present(0, 0);
        }
    }

    // --- [6. 자원 해제 (Release)] ---
    // 생성(Create)한 모든 객체는 프로그램 종료 전 반드시 Release 해야 함.
    // 생성의 역순으로 해제하는 것이 관례임.
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
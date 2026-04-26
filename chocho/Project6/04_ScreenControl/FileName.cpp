#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") // 콘솔창과 윈도우창을 동시에 띄우기 위한 설정
#include <windows.h>
#include <d3d11.h>       // DirectX 11 핵심 헤더
#include <d3dcompiler.h> // 셰이더 코드를 컴파일하기 위한 헤더
#include <iostream>

// 라이브러리 링크 (엔진 빌드 시 필요한 DX11 부품들)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// [비디오 설정 관리 구조체] 게임의 해상도와 화면 모드를 담는 주머니
struct VideoConfig
{
    int Width = 800;         // 가로 해상도
    int Height = 600;        // 세로 해상도
    bool IsFullscreen = false; // 전체화면 여부
    bool NeedsResize = false;  // 해상도 변경이 필요한지 체크하는 스위치
    int VSync = 1;          // 1이면 모니터 주사율에 맞춤(60프레임 고정 등)
} g_Config;

// --- DirectX 핵심 전역 변수 (인터페이스 포인터들) ---
ID3D11Device* g_pd3dDevice = nullptr;           // 리소스(버퍼, 셰이더)를 만드는 '공장'
ID3D11DeviceContext* g_pImmediateContext = nullptr; // 명령을 내리는 '작업반장'
IDXGISwapChain* g_pSwapChain = nullptr;         // 화면 그림을 교체해주는 '도화지 관리자'
ID3D11RenderTargetView* g_pRenderTargetView = nullptr; // "여기에 그려라"라고 지정된 도화지 표면
ID3D11VertexShader* g_pVertexShader = nullptr;  // 점의 위치를 계산하는 프로그램
ID3D11PixelShader* g_pPixelShader = nullptr;    // 색상을 결정하는 프로그램
ID3D11InputLayout* g_pInputLayout = nullptr;    // 점의 데이터 구조를 그래픽 카드에 알려주는 설계도
ID3D11Buffer* g_pVertexBuffer = nullptr;        // 점의 위치/색상 데이터를 담는 메모리


// 정점 데이터 구조체 (점 하나당 위치와 색상 정보를 가짐)
struct Vertex
{
    float x, y, z;      // 위치 (3D 공간 좌표)
    float r, g, b, a;   // 색상 (RGBA)
};

// --- [해상도 및 리소스 재구성 함수] ---
// 사용자가 1번이나 2번 키를 눌러 해상도를 바꿀 때 호출되는 아주 중요한 함수야!
void RebuildVideoResources(HWND hWnd)
{
    if (!g_pSwapChain) return;

    // 1. 기존 도화지를 치움 (새 크기로 도화지를 갈아끼우기 위해 반드시 Release 해야 함)
    if (g_pRenderTargetView)
    {
        g_pRenderTargetView->Release();
        g_pRenderTargetView = nullptr;
    }

    // 2. 백버퍼(보이지 않는 도화지)의 크기를 새로운 해상도로 재조정
    // (0: 버퍼 개수 유지, 가로, 세로, UNKNOWN: 포맷 유지, 0: 추가 옵션 없음)
    g_pSwapChain->ResizeBuffers(0, g_Config.Width, g_Config.Height, DXGI_FORMAT_UNKNOWN, 0);

	// 3.새 백버퍼 가져오기 (0: 첫 번째 버퍼, UUID: 타입 체크, (void**): 주소 전달)
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

    // [바로 이 지점] 도화지를 제대로 받았는지 검사하는 생명선
    if (pBackBuffer == nullptr)
    {
        printf("GETBUFFER ERROR\n");// "도화지 못 찾았어!"라고 콘솔에 알림
        return; // 더 이상 진행하면 터지니까 함수 종료
    }

    // 4. 가져온 도화지를 "여기에 그려라"라고 지정하는 View로 만듦
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();// 도화지 연결이 끝났으니 임시 변수는 해제


    // 5. 창모드라면 윈도우 창 자체의 크기도 해상도에 맞춰 조절
    if (!g_Config.IsFullscreen)
    {
        RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);      // 메뉴바 크기 등을 뺀 실제 그림 영역 계산
        SetWindowPos(hWnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    }

    g_Config.NeedsResize = false;       // 변경 작업 완료!
    printf("[Video] Changed: %d x %d\n", g_Config.Width, g_Config.Height);
}


// [3] 윈도우 메시지 처리: 창 끄기 등 시스템 신호 처리
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}



// [메인 함수]
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 1. 윈도우 클래스 등록 및 생성   01_BLANKSCREEN+DRAWING과 거의 동일한 창 생성 코드
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11VideoClass";
    RegisterClassExW(&wcex);


    //!!!!!!!! [AdjustWindowRect]: 테두리와 타이틀바 두께를 계산해주는 함수
   // "안쪽 그림 그리는 영역만 800x600이 되게 창 전체 크기를 계산해줘!"라는 뜻
    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    //01_BLANKSCREEN+DRAWING과 거의 동일한 창 생성 코드
    HWND hWnd = CreateWindowW(L"DX11VideoClass", L"F: Fullscreen | 1: 800x600 | 2: 1280x720",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);


    //01_BLANKSCREEN+DRAWING과 거의 동일한 창 생성 코드
    // 2. DX11 장치 및 도화지 관리자(SwapChain) 설정
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;                             // 백버퍼(뒷면 도화지) 1개 사용
    sd.BufferDesc.Width = g_Config.Width;           // 가로 크기
    sd.BufferDesc.Height = g_Config.Height;         // 세로 크기
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 색상 형식 (빨/초/파/투명 8비트씩)
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 이 버퍼를 "출력용 도화지"로 쓰겠음
    sd.OutputWindow = hWnd;                         // 그림을 띄울 창 주소
    sd.SampleDesc.Count = 1;                        // 안티앨리어싱(계단현상 방지) 1배수(안 함)
    sd.Windowed = TRUE;                             // 창모드로 시작


    // 공장(Device), 작업반장(Context), 교체기(SwapChain)를 한꺼번에 생성
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    // 3. 초기 도화지 리소스 빌드 함수 호출
    RebuildVideoResources(hWnd);

    // 4. 셰이더 컴파일 (간략화된 소스 사용)(GPU가 읽는 전용 프로그램 코드)
    const char* shaderSource = R"(
        // [구조체 정의] 데이터가 어떤 모양으로 들어오고 나갈지 약속함
        struct VS_IN { 
            float3 pos : POSITION; // 들어오는 점의 위치 (x, y, z)
            float4 col : COLOR;    // 들어오는 점의 색상 (r, g, b, a)
        };
        struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; }; // 위치 계산이 끝난 최종 시스템 좌표와 픽셀로 넘겨줄 색상

        // [VS: Vertex Shader] 점의 위치를 계산하는 함수
        PS_IN VS(VS_IN input) { 
            PS_IN output; 
            // 3D 좌표(float3)를 4D 좌표(float4)로 변환 (z값 뒤에 1.0f를 붙임)
            output.pos = float4(input.pos, 1.0f); 
            output.col = input.col; // 색상은 그대로 전달
            return output; 
        }
        // [PS: Pixel Shader] 점 사이의 면을 채우는 색상을 결정하는 함수
        float4 PS(PS_IN input) : SV_Target { return input.col; }        // 계산된 색상을 최종적으로 화면(Target)에 출력함
    )";


    ID3DBlob* vsBlob, * psBlob;

    // 텍스트로 된 셰이더 소스 코드를 GPU가 읽을 수 있는 **이진수 데이터(Binary)**로 번역하는 과정
    // (소스, 길이, 이름, 매크로, 인클루드, 함수명, 버전, 플래그1, 플래그2, 결과물, 에러물)
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    //번역된 데이터를 바탕으로 그래픽 카드 메모리에 실제 실행 가능한 부품을 만듦
    // 컴파일된 결과로 실제 셰이더 부품 생성
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);


    // [설계도] 점(Vertex) 데이터가 어떤 순서로 들어오는지 GPU에게 알려줌
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // (가이드 배열, 항목 개수, 셰이더 번역본 주소, 번역본 크기, 결과물 주소)
	// "layout"이라는 설계도를 바탕으로 GPU가 점 데이터를 제대로 해석할 수 있도록 통역기(Input Layout)를 만들어서 저장
	g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);

	vsBlob->Release(); psBlob->Release(); // 번역된 셰이더 데이터(Blob)는 이제 필요 없으니 메모리에서 해제

    // 5. 삼각형의 점 데이터 생성
    Vertex vertices[] = {
        {  0.0f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
    };

    // [버퍼 설명서] (사이즈, 용도, 바인딩 타입, CPU 접근, 플래그, 스트라이드)
    //BUFFER_DESC: 그래픽 카드에게 "방 이만큼만 빌려줘"라고 하는 요청서.
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    //SUBRESOURCE_DATA: 그 방에 처음 들어갈 이사 짐.
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    //CreateBuffer: 실제로 방을 만들고 짐을 넣은 뒤 **열쇠(g_pVertexBuffer)**를 받는 동작.
    g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);


    // --- [게임 루프] ---
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [입력 처리: GetAsyncKeyState의 0x0001 플래그로 1회성 입력 감지]
            if (GetAsyncKeyState('F') & 0x0001) {           // F키: 전체화면 모드 토글
                g_Config.IsFullscreen = !g_Config.IsFullscreen;
                g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
            }
            if (GetAsyncKeyState('1') & 0x0001) { g_Config.Width = 800; g_Config.Height = 600; g_Config.NeedsResize = true; }
            if (GetAsyncKeyState('2') & 0x0001) { g_Config.Width = 1280; g_Config.Height = 720; g_Config.NeedsResize = true; }

            // 해상도 변경 신호가 오면 리소스 재건축
            if (g_Config.NeedsResize) RebuildVideoResources(hWnd);

            // [렌더링]
			float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f }; // 배경색 (남색)
			g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);// 도화지를 배경색으로 지움

            // 중요: 뷰포트는 매 프레임 바뀐 g_Config 기준으로 설정"도화지 중 어느 영역에 그릴까?" (0, 0 위치부터 전체 크기만큼)
            D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)g_Config.Width, (float)g_Config.Height, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);// (개수, 뷰포트 배열 주소)
            // "이제 이 도화지에 그려라!"라고 최종 확정
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);


            // [파이프라인 세팅] 어떤 도구로 그릴지 반장에게 지시
            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset); // 점 버퍼 장착
            g_pImmediateContext->IASetInputLayout(g_pInputLayout); // 설계도 장착
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // "삼각형으로 그려!"
            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0); // 위치 계산기 장착
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0); // 색칠 계산기 장착

            // [그리기 실행] (점 개수, 시작 인덱스)
            g_pImmediateContext->Draw(3, 0);

            // [화면 제출] (수직동기화 여부, 추가 플래그)
            //설정값에 따라 화면 찢어짐을 방지하거나, 최대 프레임을 뽑아내거나 선택할 수 있게 만든 코드야!
            //g_Config.VSync: 1이면 "모니터 기다려!", 0이면 "기다리지 말고 바로 쏴!" (유동적 설정)
            g_pSwapChain->Present(g_Config.VSync, 0);
        }
    }

    // [정리]
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}
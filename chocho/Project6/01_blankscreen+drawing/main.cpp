#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

// [컴파일러 지시어] 콘솔창을 띄우고 라이브러리(.lib)들을 실제 코드와 연결함
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") 
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- [전역 객체 관리] ---
// GPU 메모리는 수동 관리 대상! 사용 후 반드시 'Release()'로 반납해야 메모리 누수가 안 생김.
ID3D11Device* g_pd3dDevice = nullptr;           // [공장장] 버퍼, 셰이더 등 각종 리소스를 GPU에 '생성'함
ID3D11DeviceContext* g_pImmediateContext = nullptr;   // [일꾼] 생성된 리소스를 파이프라인에 꽂고 "그려라" 명령함
IDXGISwapChain* g_pSwapChain = nullptr;          // [마술사] 뒷면 도화지(Back)와 앞면(Front)을 교체하는 더블 버퍼링 담당
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;   // [진짜 도화지] 픽셀 셰이더의 결과가 출력될 최종 목적지

// 정점(Vertex) 설계도: 위치(12바이트) + 색상(16바이트) = 총 28바이트 상자
struct Vertex {
    float x, y, z;      // 어디에 그릴 것인가?
    float r, g, b, a;   // 어떤 색으로 칠할 것인가?
};

// HLSL 셰이더 소스: 그래픽 카드가 읽는 '마법 주문서'
const char* shaderSource = R"(
//데이터 주머니 (struct) 정의
                                                    //컴퓨터한테 "자, 이제 이런 데이터가 들어오고 나갈 거야"라고 미리 약속하는 단계야.
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

// [정점 셰이더] 점의 위치를 시스템 좌표계에 맞게 변환함
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);         // 원래 점은 3D($x, y, z$)인데, GPU는 계산을 편하게 하려고 4D($x, y, z, w$) 형식을 써. 그래서 끝에 1.0f를 강제로 붙여서 4D로 확장하는 거야.
    output.col = input.col;
    return output;                               //계산된 위치와 색상 정보를 다음 단계로 넘겨줌
}

// [픽셀 셰이더] 점 사이의 공간을 색으로 채움
float4 PS(PS_INPUT input) : SV_Target {         //SV_Target : 이 함수가 뱉는 값이 최종적으로 모니터 화면(Target)에 그려질 색깔이다
    return input.col; // 정점에서 넘어온 색을 그대로 픽셀에 적용
}
)";

/// LRESULT: 함수의 출력 타입 (상태 보고용 숫자)
// CALLBACK: 운영체제(Windows)가 필요할 때마다 이 함수를 직접 부른다는 뜻
// WndProc: 함수의 이름 (Window Procedure의 약자)
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

    // [전달받는 인자들]
    // HWND hWnd: "어느 창에서 발생한 사건인가?" (창 번호표)
    // UINT message: "무슨 사건인가?" (예: 마우스 클릭, 창 닫기 등)
    // WPARAM, LPARAM: "사건의 상세 정보" (예: 마우스 좌표, 눌린 키 번호 등)

    // 만약(if) 발생한 사건(message)이 "창을 닫으라는 신호(WM_DESTROY)"라면?
    if (message == WM_DESTROY) {

        // 1. PostQuitMessage(0): "컴퓨터야, 이제 이 프로그램 완전히 끝낼 거니까 준비해!"라고 알림
        PostQuitMessage(0);

        // 2. return 0: 사건 처리가 끝났으니 보고를 마치고 함수를 빠져나감
        return 0;
    }

    // 3. DefWindowProc: "내가 처리하지 않은 나머지 수만 가지 사건들은 윈도우가 알아서 기본값으로 처리해줘!"
    // (예: 창 크기 조절, 제목 표시줄 드래그 등 우리가 일일이 코딩하기 힘든 기본 기능들)
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// [입구] 프로그램이 시작되는 지점
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. 윈도우 창 설계 및 등록
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) }; // 종이의 크기(사양)를 먼저 적어줌
    wcex.lpfnWndProc = WndProc;                // [핵심] "사건 처리는 아까 만든 상황실(WndProc)에 맡길게!"
    wcex.hInstance = hInstance;                // 이 설계도의 주인(신분증) 등록
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW); // 마우스 커서는 기본 '화살표'로 할게
    wcex.lpszClassName = L"DX11GameLoopClass";  // [중요] 이 설계도의 "이름"을 붙여줌 (식별용)
    RegisterClassExW(&wcex);                   // [설계도 등록] 운영체제에게 "나중에 이 설계도 이름으로 창 만들 거니까 기억해줘!"라고 함



    // 실제 창 생성 (번호표 HWND를 받아옴)
    HWND hWnd = CreateWindowW(
        L"DX11GameLoopClass",           // 1. 사용할 설계도 이름 (아까 등록한 거랑 똑같아야 함!)
        L"과제: 움직이는 육망성 만들기", // 2. 창 제목 (제목 표시줄에 뜨는 글자)
        WS_OVERLAPPEDWINDOW,             // 3. 창 스타일 (최소화, 최대화 버튼 있는 기본 창)
        CW_USEDEFAULT, CW_USEDEFAULT,    // 4. 창이 뜰 위치 (x, y 좌표 - 윈도우가 알아서 정해줘!)
        800, 600,                       // 5. 창의 크기 (가로 800, 세로 600)
        nullptr, nullptr, hInstance, nullptr // 6. 기타 부모창 정보 및 신분증
    );




    if (!hWnd) return -1; // 만약 번호표를 못 받았다면(생성 실패) 프로그램 종료!
    ShowWindow(hWnd, nCmdShow); // 번호표(hWnd)를 가진 창을 모니터에 실제로 "나타나게" 해!


    // 2. DX11 초기화 || 화면을 어떻게 교체할지 적어둔 주문서라고 생각하면 돼.
    DXGI_SWAP_CHAIN_DESC sd = {};

    sd.BufferCount = 1;                           // (1) 뒷면 도화지(Back Buffer) 개수 || 보통 앞면 1장, 뒷면 1장으로 구성함
    sd.BufferDesc.Width = 800;                    // (2) 도화지 가로 크기 
    sd.BufferDesc.Height = 600;                   // (3) 도화지 세로 크기 
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // (4) 색상 표현 방식 (R,G,B,A 각 8비트) 0.0~1.0 사이로 정규화함
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // (5) 버퍼의 용도 || 이 버퍼를 '그림이 그려질 결과물 도화지'로 사용하겠다고 선언
    sd.OutputWindow = hWnd;                       // (6) 출력될 창 핸들 || 아까 만든 창 번호표(hWnd)에 이 도화지를 딱 붙여줌
    sd.SampleDesc.Count = 1;                      // (7) 멀티 샘플링 개수 || 계단 현상 방지(안티앨리어싱) 수준. 1은 기능을 쓰지 않겠다는 뜻
    sd.Windowed = TRUE;                           // (8) 창 모드 여부 || TRUE면 창 모드, FALSE면 전체 화면 모드로 실행함


    // 공장장(Device), 일꾼(Context), 마술사(SwapChain)를 동시에 소환하는 함수
    D3D11CreateDeviceAndSwapChain(
        nullptr,                    // (1) 사용할 그래픽 카드 || nullptr이면 시스템의 기본 그래픽 카드를 선택
        D3D_DRIVER_TYPE_HARDWARE,   // (2) 드라이버 타입 || CPU가 아닌 진짜 그래픽 카드(GPU) 하드웨어를 사용함
        nullptr,                    // (3) 소프트웨어 래스터라이저 || 하드웨어 타입을 썼으므로 사용 안 함(nullptr)
        0,                          // (4) 실행 플래그 || 디버그 모드 등의 추가 옵션 설정 (0은 기본값)
        nullptr, 0,                 // (5) 기능 레벨 배열 및 개수 || 특정 DX 버전을 강제할 때 사용 (nullptr은 자동)
        D3D11_SDK_VERSION,          // (6) SDK 버전 || 현재 사용 중인 DirectX SDK의 버전 정보
        &sd,                        // (7) 스왑체인 설정값 || 위에서 숫자로 정리한 주문서(sd)의 주소 전달
        &g_pSwapChain,              // (8) 생성된 스왑체인 결과 || 마술사 객체를 담아올 포인터 주소
        &g_pd3dDevice,               // (9) 생성된 디바이스 결과 || 공장장 객체를 담아올 포인터 주소
        nullptr,                    // (10) 선택된 기능 레벨 || 실제로 어떤 버전이 선택됐는지 보고받을 변수
        &g_pImmediateContext       //(11) 작업반장 객체 주소 (Context)
    );


    // [준비] 도화지 원본을 잠시 보관할 빈 박스를 하나 만듭니다.
    ID3D11Texture2D* pBackBuffer = nullptr;

    // [ 1] 마술사(스왑체인)의 주머니 안에서 실제 그림을 그릴 '뒷면 도화지'를 꺼내옵니다.
    g_pSwapChain->GetBuffer(
        0,                          // (1) 뒷면 도화지 번호 (0번)
        __uuidof(ID3D11Texture2D),  // (2) 도화지 형식 증명서 (2D 텍스처 형태임을 확인)
        (void**)&pBackBuffer        // (3) 꺼내온 도화지를 아까 만든 박스(pBackBuffer)에 담음
    );

    // [ 2] 공장장(Device)에게 꺼내온 도화지를 "그릴 수 있는 상태(View)"로 가공해달라고 명령합니다.
    g_pd3dDevice->CreateRenderTargetView(
        pBackBuffer,                // (4) 가공할 대상 (방금 꺼내온 뒷면 도화지 원본)
        nullptr,                    // (5) 상세 뷰 설정 (기본값 사용 시 nullptr)
        &g_pRenderTargetView        // (6) 가공이 완료된 최종 도화지(View)를 전역 변수에 저장
    );

    // [문장 3] 도화지를 가공해서 '최종 도화지(View)'를 얻었으니, 빌려왔던 원본 박스는 이제 반납합니다.
    pBackBuffer->Release();         // (7) 원본 도화지 객체의 사용권을 포기하고 메모리 관리 수치를 내림



    // 3. 셰이더 주문서 번역 및 무기 제작
    // 우리가 쓴 글자(텍스트)를 GPU가 알아듣는 기계어로 바꾸는 '번역' 과정이야.
    ID3DBlob* vsBlob, * psBlob; // 번역된 기계어 데이터를 잠시 담아둘 '찰흙 덩어리(Blob)' 변수들
    // [ 1] 점의 위치를 계산하는 '정점 셰이더'를 기계어로 번역합니다.
    D3DCompile(
        shaderSource,           // (1) 번역할 소스 코드 (문자열)
        strlen(shaderSource),    // (2) 소스 코드의 전체 길이
        nullptr,                // (3) 파일 경로 (지금은 문자열이라 nullptr)
        nullptr, nullptr,       // (4) 매크로 정의, 인클루드 (기본값)
        "VS",                   // (5) 시작 지점 (HLSL 소스 내 함수 이름)
        "vs_4_0",               // (6) 셰이더 버전 (4.0 모델 사용)
        0, 0,                   // (7) 컴파일 옵션 (0은 기본)
        &vsBlob,                // (8) 번역된 결과물을 담을 바구니 주소
        nullptr                 // (9) 에러 메시지를 담을 바구니 주소
    );
    // Pixel Shader(PS) 번역: 색칠을 담당하는 주문서
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);



    ID3D11VertexShader* vShader; // (1) 진짜 점 위치 결정기(무기)를 담을 변수
    ID3D11PixelShader* pShader;  // (2) 진짜 색칠 공부기(무기)를 담을 변수

    // [정점 셰이더 생성]
    g_pd3dDevice->CreateVertexShader(
        vsBlob->GetBufferPointer(), // (1) 번역된 기계어 데이터의 시작 지점
        vsBlob->GetBufferSize(),    // (2) 번역된 기계어 데이터의 전체 크기
        nullptr,                    // (3) 클래스 링크 (지금은 사용 안 함)
        &vShader                    // (4) 만들어진 무기를 저장할 변수 주소
    );
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);



    // [시험 단골: IA 단계] 데이터 통역 가이드라인 작성
    // AlignedByteOffset: POSITION이 float 3개(12바이트)를 먹었으므로 COLOR는 12부터 시작!
    D3D11_INPUT_ELEMENT_DESC layout[] = {
    {
        "POSITION",                 // (1) HLSL 주문서에 적힌 이름 (의미)
        0,                          // (2) 똑같은 이름이 여러 개일 때 붙이는 번호
        DXGI_FORMAT_R32G32B32_FLOAT,// (3) 데이터 종류: Format (숫자 몇 개인지, 실수(FLOAT)인지 결정),
        0,                          // (4) 입력 슬롯 (0번 통로) (위치는 0, 색상은 앞에 위치가 먹은 12바이트 뒤부터!)
        0,                          // (5) 시작 오프셋 (상자 맨 처음 0바이트 지점부터 읽어라) 
        D3D11_INPUT_PER_VERTEX_DATA,// (6) 데이터 종류 (점 하나당 데이터 하나씩 들어있다)
        0                           // (7) 인스턴스 데이터 스텝 레이트   (지금은 안 쓰니까 무조건 0)
    },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };


    ID3D11InputLayout* pInputLayout;        // (1) 완성된 통역기를 담을 변수
    // [통역기 생성]
    g_pd3dDevice->CreateInputLayout(
        layout,                         // (1) 아까 정의한 통역 가이드 배열(layout)
        2,                              // (2) 가이드 항목의 개수 (POSITION, COLOR 총 2개)
        vsBlob->GetBufferPointer(),     // (3) 번역본의 시작 위치 (셰이더와 규격이 맞는지 대조용)
        vsBlob->GetBufferSize(),        // (4) 번역본의 전체 크기
        &pInputLayout                   // (5) 결과물 통역기를 저장할 주소
    );


    vsBlob->Release(); // (1) 통역기와 무기를 다 만들었으니 임시 번역본(vsBlob) 반납
    psBlob->Release(); // (2) 임시 번역본(psBlob) 반납

    // 4. 육망성 점(Vertex) 데이터 작성 (도화지에 점 찍기)
    Vertex vertices[] = {
        // [1번 삼각형: 정방향] 시계 방향 순서 (위 -> 우하 -> 좌하)
        {  0.0f,  0.6f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.5f, -0.3f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        { -0.5f, -0.3f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },

        // [2번 삼각형: 역방향] 시계 방향 순서 (아래 -> 좌상 -> 우상)
        // ※ 뒤집힌 모양도 '시계 방향'으로 찍어야 백페이스 컬링에 안 걸리고 화면에 나옴!
        {  0.0f, -0.6f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        { -0.5f,  0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        {  0.5f,  0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f }
    };

    // 점들을 담을 정점 버퍼(상자) 생성하여 GPU 메모리에 전달
    ID3D11Buffer* pVBuffer;     // (1) 점들을 담을 최종 상자(Buffer) 변수

    // [상자 사양서 작성]
    D3D11_BUFFER_DESC bd = {
        sizeof(vertices),           // (1) 상자의 전체 크기 (점 6개 분량의 바이트)
        D3D11_USAGE_DEFAULT,        // (2) 상자 사용법 (GPU가 읽고 쓰기 가능하도록 설정)
        D3D11_BIND_VERTEX_BUFFER,   // (3) 상자의 용도 (정점 데이터를 담는 용도)
        0,                          // (4) CPU 접근 권한 (필요 없음)
        0,                          // (5) 기타 플래그
        0                           // (6) 구조화된 버퍼의 스트라이드
    };

    // [상자에 넣을 내용물 지정]
    D3D11_SUBRESOURCE_DATA initData = {
        vertices,                   // (1) 상자에 담을 실제 데이터 (아까 만든 vertices 배열)
        0,                          // (2) 한 줄의 크기 (지금은 안 씀)
        0                           // (3) 한 면의 크기 (지금은 안 씀)
    };

    // [진짜 상자 만들기]
    g_pd3dDevice->CreateBuffer(
        &bd,                        // (1) 아까 만든 상자 사양서
        &initData,                  // (2) 상자에 넣을 실제 내용물
        &pVBuffer                   // (3) 결과물 상자를 저장할 주소
    );

    // // 5. 정석 게임 루프: 프로그램이 꺼지기 전까지 무한 반복
    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {        // (1) 종료 메시지가 들어올 때까지 계속 돌아라
        // [입력 단계] PeekMessage: 메시지가 없어도 대기하지 않고 즉시 리턴함 (게임용)

// [입력 단계] 윈도우에서 보낸 메시지(클릭, 종료 등)가 있는지 확인
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); // (2) 키보드 입력 메시지 변환
            DispatchMessage(&msg);  // (3) 상황실(WndProc)로 메시지 전달
        }

        else {
            // [업데이트 & 출력 단계] 남는 시간에 계속 그림을 그림 (Idle Time 렌더링)
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f }; // (4) 지울 배경색 (남색)
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor); // (5) 도화지를 배경색으로 싹 지움


            // [파이프라인 세팅] 일꾼(Context)에게 도구 장착
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr); // (6) 그릴 도화지 지정

            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f }; // (7) 화면의 어느 영역을 쓸지 설정
            g_pImmediateContext->RSSetViewports(1, &vp);        // (8) 뷰포트 영역 적용

            g_pImmediateContext->IASetInputLayout(pInputLayout); // (9) 데이터 통역기 장착

            UINT stride = sizeof(Vertex); // (10) 한 점의 크기 (28바이트)
            UINT offset = 0;              // (11) 읽기 시작할 위치 (0바이트)
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset); // (12) 점 데이터 상자 장착 
            // stride가 왜 필요한가? // 정답: GPU가 버퍼를 읽을 때 점 하나가 몇 바이트인지 알아야 다음 점의 위치를 정확히 찾을 수 있기 때문입니다. (우리 코드는 28바이트)

            // [IA 단계] 점들을 잇는 규칙 설정
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // (13) "점 3개씩 묶어서 삼각형으로 그려!"

            g_pImmediateContext->VSSetShader(vShader, nullptr, 0); // (14) 위치 결정 마법(VS) 장착
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0); // (15) 색칠 공부 마법(PS) 장착

            // [최종 실행]
            g_pImmediateContext->Draw(6, 0); // (16) "상자에서 점 6개 꺼내서 지금 당장 그려!"

            // [화면 교체]
            //첫 번째 칸 (a) : "모니터 눈치를 얼마나 볼까?" (수직 동기화)
          /*0 (눈치 안 봄) : "다 그렸어? 그럼 모니터가 뭘 하든 지금 당장 화면 바꿔!"

                (결과 : 속도는 빠르지만, 화면이 위아래로 어긋나는 * *찢어짐(Tearing) * *이 생길 수 있음)

                1 (눈치 봄) : "모니터가 한 장 다 그릴 때까지 기다렸다가 맞춰서 바꿔!"

                (결과 : 화면이 깨끗하고 안정적임.보통 게임의 기본 설정)

                2~4 (엄청 기다림) : "모니터가 2~4번 깜빡일 때 딱 한 번만 바꿔!"

                (결과 : 사양이 아주 낮은 컴퓨터에서 억지로 속도를 맞출 때 씀)

                두 번째 칸(b) : "특별한 지시가 있어?" (출력 옵션)
                0 (기본) : "그냥 평범하게 화면 내보내." (99 % 이 값만 씀)

                TEST(검사) : "진짜로 바꾸진 말고, 지금 화면 바꿀 수 있는 상태인지 확인만 해봐."

                DO_NOT_WAIT(성급함) : "GPU가 바빠서 기다려야 한다면, 기다리지 말고 나한테 안 된다고 바로 말해줘."
                */
            g_pSwapChain->Present(1, 0); // (17) "다 그린 뒷면 도화지를 손님(모니터)에게 보여줘!"
            //더블 버퍼링(Double Buffering) 구조에서 백 버퍼(Back Buffer)에 완성된 그림을 
            // 프론트 버퍼(Front Buffer)로 전환하여 사용자에게 최종 화면을 출력하는 역할을 합니다.

        }
    }
    // --- [6. 자원 해제] 빌려온 GPU 메모리는 생성의 역순으로 반드시 반납! ---

    if (pVBuffer) pVBuffer->Release();             // (1) 정점 버퍼 반납 || 점 데이터 담았던 상자 버림
    if (pInputLayout) pInputLayout->Release();     // (2) 입력 레이아웃 반납 || 데이터 통역 가이드라인 버림
    if (vShader) vShader->Release();               // (3) 정점 셰이더 반납 || 위치 결정 마법 무기 파기
    if (pShader) pShader->Release();               // (4) 픽셀 셰이더 반납 || 색칠 공부 마법 무기 파기
    if (g_pRenderTargetView) g_pRenderTargetView->Release(); // (5) 렌더 타겟 뷰 반납 || 최종 도화지 안경 버림
    if (g_pSwapChain) g_pSwapChain->Release();     // (6) 스왑 체인 반납 || 화면 교체 마술사 퇴근
    if (g_pImmediateContext) g_pImmediateContext->Release(); // (7) 디바이스 컨텍스트 반납 || 현장 작업반장 퇴근
    if (g_pd3dDevice) g_pd3dDevice->Release();       // (8) 디바이스 반납 || 공장장(근본 자원) 최종 퇴근

    return (int)msg.wParam; // (9) 프로그램 종료 코드 반환 || OS에게 "나 이제 진짜 집 간다"라고 보고
}
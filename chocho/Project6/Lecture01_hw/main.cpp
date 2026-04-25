// --------------------------------------------------------------------------------
// [준비 단계] 프로그램의 성격과 시작점을 컴파일러에게 알려줌
// --------------------------------------------------------------------------------

// linker: 실행 파일을 만드는 도구에게 내리는 명령
// /entry: 프로그램이 시작될 때 'WinMain'이 아닌 'WinMainCRTStartup'부터 읽으라고 강제 지정
// /subsystem:console: 원래 윈도우 창만 떠야 하지만, 디버깅용 검은색 콘솔창을 같이 띄워달라는 뜻
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") 

// windows.h: 윈도우 창을 만들고, 메시지를 주고받는 모든 기능이 들어있는 '마법 백과사전'
#include <windows.h> 

// --------------------------------------------------------------------------------
// [1단계] 경비실(WndProc) 만들기: 창에서 일어나는 모든 일을 감시하고 처리함
// --------------------------------------------------------------------------------

// LRESULT: 함수가 끝날 때 돌려주는 값의 타입 (보통 정수)
// CALLBACK: 내가 호출하는 게 아니라, 윈도우 OS가 사건이 터질 때마다 나를 불러주는 방식
// HWND: "어떤 창에서 생긴 일이야?" (창의 번호표)
// UINT message: "무슨 일이야?" (클릭, 키보드, 종료 등 사건 번호)
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // 상황실에 들어온 사건 번호(message)를 확인함
    switch (message)
    {
    case WM_DESTROY:
        // [사건: 사용자가 X 버튼을 누름] 
        // PostQuitMessage: 메시지 큐에 "나 진짜 짐 싼다"라는 신호를 보냄. 이게 루프를 멈추게 함.
        PostQuitMessage(0);
        break;

    default:
        // DefWindowProc: "내가 처리 안 할 테니까, 윈도우가 알아서 기본적으로 처리해줘" (창 크기 조절 등)
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// --------------------------------------------------------------------------------
// [2단계] WinMain: 프로그램의 메인 엔진이자 시작 지점
// --------------------------------------------------------------------------------

// HINSTANCE: 내 프로그램이 OS로부터 부여받은 '고유 신분증 번호'
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // (A) 윈도우 설계도 작성 (WNDCLASSEXW 구조체 채우기)
    // "내가 만들 창은 이런 특징을 가졌어"라고 미리 적어두는 과정
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);           // 이 설계도 종이의 크기
    wcex.style = CS_HREDRAW | CS_VREDRAW;       // 가로나 세로 크기를 바꾸면 그림을 다시 그려라
    wcex.lpfnWndProc = WndProc;                 // 이 설계도로 만든 창의 사건은 아까 만든 '경비실'로 보고해라
    wcex.hInstance = hInstance;                 // 이 창은 내 신분증(hInstance) 아래에서 관리한다
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW); // 마우스 커서는 기본 화살표 모양으로 써라
    wcex.lpszClassName = L"DirectXWindowClass"; // 이 설계도의 고유한 이름표 (나중에 창 뽑을 때 이 이름을 불러야 함)

    // RegisterClassExW: 작성한 설계도를 윈도우 시스템에 공식 등록함
    RegisterClassExW(&wcex);

    // (B) 윈도우 실제 생성 (공장에서 제품 뽑아내기)
    // HWND: 생성된 창의 '고유 번호표'. 앞으로 이 창을 다룰 땐 이 변수를 써야 함.
    HWND hWnd = CreateWindowW(
        L"DirectXWindowClass",      // (1) 사용할 설계도 이름
        L"DirectX Learning Window", // (2) 창의 타이틀 제목
        WS_OVERLAPPEDWINDOW,        // (3) 창의 스타일, 최소화, 최대화, 끄기 버튼이 다 있는 아주 일반적인 창 스타일
        CW_USEDEFAULT, CW_USEDEFAULT, // (4) 시작 위치 (x, y), (좌상단 좌표 x, y - OS가 알아서 정해줘)
        800, 600,                   // (5) 창의 크기 (가로, 세로)창의 가로 넓이 800, 세로 높이 600
        nullptr, nullptr,           // (6) 부모 창 핸들,(7) 메뉴 핸들
        hInstance,                  // (8) 프로그램 신분증
        nullptr                     // (9) 추가 데이터
    );

    // 창 만들기 실패하면 (메모리 부족 등) 프로그램 그냥 꺼버림
    if (!hWnd) return FALSE;

    // 생성된 창을 모니터에 실제로 나타나게 함 (nCmdShow는 처음 띄울 때 상태 - 보통 보통창)
    ShowWindow(hWnd, nCmdShow);
    // 창의 내용을 강제로 한 번 싹 갱신함
    UpdateWindow(hWnd);

    // (C) 메시지 루프: 프로그램이 살아있게 만드는 무한 반복문
    // 사용자가 창을 닫기 전까지 윈도우가 보내주는 편지(메시지)를 계속 확인해야 함.
    MSG msg;

    // GetMessage: 윈도우가 보내는 편지가 올 때까지 여기서 멍하니 기다림 (편지가 오면 1 반환)
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg); // 키보드 자판 입력 같은 로우(Raw) 데이터를 해석함
        DispatchMessage(&msg);  // [중약!] 해석된 편지를 아까 등록한 '경비실(WndProc)'로 배달함
    }

    // 프로그램 최종 종료 (받은 메시지의 결과값을 돌려줌)
    return (int)msg.wParam;
}
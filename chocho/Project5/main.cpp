#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// [오류 해결] 시스템 설정과 관계없이 WinMain을 시작점으로 강제 지정
#pragma comment(linker, "/subsystem:windows /entry:WinMainCRTStartup")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <chrono>
#include <vector>

// ===================== 데이터 구조 =====================
struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

class GameObject;

// [컴포넌트 추상 클래스]
class Component {
public:
    GameObject* owner = nullptr;
    virtual ~Component() {}
    virtual void Update(float dt) {}
    virtual void Render() {}
};

// [게임 오브젝트]
class GameObject {
public:
    float x = 0.0f, y = 0.0f;
    float speed = 1.5f;
    std::vector<Component*> comps;

    void Add(Component* c) { c->owner = this; comps.push_back(c); }
    void Update(float dt) { for (auto c : comps) c->Update(dt); }
    void Render() { for (auto c : comps) c->Render(); }

    ~GameObject() {
        for (auto c : comps) delete c;
        comps.clear();
    }
};

// ===================== 전역 변수 =====================
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
ID3D11Buffer* g_vb = nullptr;
ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11InputLayout* g_layout = nullptr;

GameObject* g_p1 = nullptr;
GameObject* g_p2 = nullptr;
bool g_fullscreen = false;

// ===================== 컴포넌트 구현 =====================

// 1. 입력 및 이동 컴포넌트
class InputComponent : public Component {
    bool isArrowKey;
public:
    InputComponent(bool arrow) : isArrowKey(arrow) {}
    void Update(float dt) override {
        float move = owner->speed * dt;

        if (isArrowKey) {
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)  owner->x -= move;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) owner->x += move;
            if (GetAsyncKeyState(VK_UP) & 0x8000)    owner->y += move;
            if (GetAsyncKeyState(VK_DOWN) & 0x8000)  owner->y -= move;
        }
        else {
            if (GetAsyncKeyState('A') & 0x8000) owner->x -= move;
            if (GetAsyncKeyState('D') & 0x8000) owner->x += move;
            if (GetAsyncKeyState('W') & 0x8000) owner->y += move;
            if (GetAsyncKeyState('S') & 0x8000) owner->y -= move;
        }

        // [요구사항] 화면 밖 방지 로직 (Clamping)
        if (owner->x > 0.9f) owner->x = 0.9f;
        if (owner->x < -0.9f) owner->x = -0.9f;
        if (owner->y > 0.9f) owner->y = 0.9f;
        if (owner->y < -0.9f) owner->y = -0.9f;
    }
};

// 2. 렌더러 컴포넌트
class RendererComponent : public Component {
    float r, g, b;
public:
    RendererComponent(float cr, float cg, float cb) : r(cr), g(cg), b(cb) {}
    void Render() override {
        // 현재 오너의 위치를 반영한 삼각형 정점 생성
        Vertex v[3] = {
            { owner->x + 0.0f, owner->y + 0.1f, 0.5f, r, g, b, 1.0f },
            { owner->x + 0.1f, owner->y - 0.1f, 0.5f, r, g, b, 1.0f },
            { owner->x - 0.1f, owner->y - 0.1f, 0.5f, r, g, b, 1.0f }
        };
        g_context->UpdateSubresource(g_vb, 0, nullptr, v, 0, 0);
        g_context->Draw(3, 0);
    }
};

// ===================== 엔진 핵심 함수 =====================

void Init(HWND hWnd) {
    // DX11 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, nullptr, &g_context);

    ID3D11Texture2D* bb;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
    g_device->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();

    // 셰이더 컴파일
    const char* sCode = R"(
        struct VS_IN { float3 p : POSITION; float4 c : COLOR; };
        struct PS_IN { float4 p : SV_POSITION; float4 c : COLOR; };
        PS_IN VS(VS_IN i) { PS_IN o; o.p = float4(i.p, 1.0f); o.c = i.c; return o; }
        float4 PS(PS_IN i) : SV_Target { return i.c; }
    )";
    ID3DBlob* vsB, * psB;
    D3DCompile(sCode, strlen(sCode), 0, 0, 0, "VS", "vs_4_0", 0, 0, &vsB, 0);
    D3DCompile(sCode, strlen(sCode), 0, 0, 0, "PS", "ps_4_0", 0, 0, &psB, 0);
    g_device->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), 0, &g_vs);
    g_device->CreatePixelShader(psB->GetBufferPointer(), psB->GetBufferSize(), 0, &g_ps);

    D3D11_INPUT_ELEMENT_DESC ied[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    g_device->CreateInputLayout(ied, 2, vsB->GetBufferPointer(), vsB->GetBufferSize(), &g_layout);
    vsB->Release(); psB->Release();

    // 정점 버퍼 생성
    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 3, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
    g_device->CreateBuffer(&bd, nullptr, &g_vb);

    // 게임 오브젝트 초기화
    g_p1 = new GameObject(); g_p1->x = -0.4f;
    g_p1->Add(new InputComponent(true));  // 방향키
    g_p1->Add(new RendererComponent(1.0f, 0.3f, 0.3f));

    g_p2 = new GameObject(); g_p2->x = 0.4f;
    g_p2->Add(new InputComponent(false)); // WASD
    g_p2->Add(new RendererComponent(0.3f, 0.7f, 1.0f));
}

void Update(float dt) {
    // ESC 종료
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) PostQuitMessage(0);

    // F 키 전체화면 토글 (한 번만 눌리도록 처리)
    static bool fKeyPrev = false;
    bool fKeyDown = (GetAsyncKeyState('F') & 0x8000) != 0;
    if (fKeyDown && !fKeyPrev) {
        g_fullscreen = !g_fullscreen;
        g_swapChain->SetFullscreenState(g_fullscreen, nullptr);
    }
    fKeyPrev = fKeyDown;

    // 오브젝트 업데이트
    g_p1->Update(dt);
    g_p2->Update(dt);
}

void Render() {
    float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    g_context->ClearRenderTargetView(g_rtv, clearColor);
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);

    D3D11_VIEWPORT vp = { 0, 0, 800.0f, 600.0f, 0.0f, 1.0f };
    g_context->RSSetViewports(1, &vp);

    UINT stride = sizeof(Vertex), offset = 0;
    g_context->IASetVertexBuffers(0, 1, &g_vb, &stride, &offset);
    g_context->IASetInputLayout(g_layout);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->VSSetShader(g_vs, 0, 0);
    g_context->PSSetShader(g_ps, 0, 0);

    // 오브젝트 그리기
    g_p1->Render();
    g_p2->Render();

    g_swapChain->Present(1, 0);
}

void Release() {
    if (g_p1) delete g_p1;
    if (g_p2) delete g_p2;
    if (g_vb) g_vb->Release();
    if (g_layout) g_layout->Release();
    if (g_vs) g_vs->Release();
    if (g_ps) g_ps->Release();
    if (g_rtv) g_rtv->Release();
    if (g_swapChain) {
        g_swapChain->SetFullscreenState(FALSE, nullptr);
        g_swapChain->Release();
    }
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
}

// ===================== 윈도우 메인 =====================

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    WNDCLASS wc = { 0, WndProc, 0, 0, hInst, 0, LoadCursor(0, IDC_ARROW), 0, 0, L"DX_GAME" };
    RegisterClass(&wc);

    // 800x600 Client 영역 조정
    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindow(L"DX_GAME", L"DirectX 11 Game Engine Loop", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, 0, 0, hInst, 0);

    ShowWindow(hWnd, nShow);

    // [Init]
    Init(hWnd);

    // [Game Loop]
    auto prevTime = std::chrono::high_resolution_clock::now();
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [DeltaTimer]
            auto currTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currTime - prevTime;
            prevTime = currTime;
            float dt = elapsed.count();

            // [Update] & [Render]
            Update(dt);
            Render();
        }
    }

    // [Release]
    Release();

    return (int)msg.wParam;
}
// ---------------------------------------------------------
// 1. 필요한 도구 상자(라이브러리) 가져오기
// ---------------------------------------------------------
#include <windows.h>      // 윈도우 창을 다루기 위한 기본 도구함
#include <d3d11.h>        // DirectX 11의 핵심 도구 (공장장, 작업반장 등)
#include <d3dcompiler.h>  // 사람이 쓴 주문서(셰이더)를 컴퓨터가 아는 기계어로 번역해주는 도구
#include <vector>         // 데이터를 줄 세워 담기 편한 가방

// GLFW는 원래 OpenGL(다른 그래픽 도구)용인데, DirectX랑 같이 쓰려고 윈도우 전용 기능을 켜주는 거야.
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// 코드를 실행할 때 컴퓨터한테 "이 도구 상자들을 진짜로 가져와 줘!"라고 부탁하는 명령어 (없으면 에러 남)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ---------------------------------------------------------
// 2. 우리가 그릴 '점(Vertex)'의 설계도 만들기
// ---------------------------------------------------------
// [왜 썼을까?] 컴퓨터는 '점'이 뭔지 몰라. 그래서 "위치 3개, 색상 4개로 이루어진 게 점이야!"라고 규칙을 정해주는 거야.
struct Vertex {
    float x, y, z;      // 점의 위치 (가로, 세로, 깊이) -> 12바이트 공간 차지
    float r, g, b, a;   // 점의 색상 (빨강, 초록, 파랑, 투명도) -> 16바이트 공간 차지
};

// ---------------------------------------------------------
// 3. 마법 주문서 (HLSL 셰이더 코드) - 그래픽 카드(GPU) 전용 언어
// ---------------------------------------------------------
// [왜 썼을까?] CPU 혼자서 수만 개의 점을 그리면 너무 느려. 
// 그래서 그림 그리기의 달인인 GPU에게 "이렇게 색칠해라"라고 직접 지시를 내리기 위한 설명서야.
const char* shaderSource = R"(
// CPU가 던져주는 데이터를 받는 상자
struct VS_INPUT {
    float3 pos : POSITION; // 꼬리표(POSITION): "이건 위치 데이터야!"
    float4 col : COLOR;    // 꼬리표(COLOR): "이건 색상 데이터야!"
};

// 정점 셰이더가 픽셀 셰이더로 넘겨줄 데이터를 담는 상자
struct PS_INPUT {
    float4 pos : SV_POSITION; // 꼬리표(SV_POSITION): "이게 화면에 진짜 찍힐 최종 위치야!" (GPU가 특별 관리함)
    float4 col : COLOR;       // 색상은 그대로 전달
};

// [1단계: 정점 셰이더] 점의 위치를 화면에 맞게 변환해주는 마법
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    // 3D 위치를 수학 계산이 편하게 4D로 늘려줌 (뒤에 1.0f를 붙임)
    output.pos = float4(input.pos, 1.0f);
    // 색깔은 안 바꾸고 그대로 다음 단계로 토스!
    output.col = input.col;
    return output;
}

// [2단계: 픽셀 셰이더] 점과 점 사이의 공간(픽셀)에 색칠을 하는 마법
// SV_Target: "이 색깔을 최종 도화지(모니터)에 칠해라!" 라는 뜻
float4 PS(PS_INPUT input) : SV_Target {
    return input.col; // 아까 전달받은 색상을 그대로 반환해서 칠함
}
)";

int main() {
    // 창 만들기 도구(GLFW) 준비 확인! 실패하면 프로그램 끄기.
    if (!glfwInit()) return -1;

    // ---------------------------------------------------------
    // 4. 윈도우 창(스케치북 겉표지) 만들기
    // ---------------------------------------------------------
    // [왜 썼을까?] 그림을 그리려면 우선 그림을 담을 네모난 '창'이 필요하니까!
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // OpenGL 안 쓸 거니까 기본 기능 꺼!
    //glfwCreateWindow(가로, 세로, 제목, 모니터, 공유창);
    GLFWwindow* window = glfwCreateWindow(800, 600, "GLFW + DirectX 11 Triangle", nullptr, nullptr);
    if (!window) return -1;

    // 창의 고유 번호(주민등록번호 같은 거)를 가져옴. 나중에 모니터에 그림 보낼 때 "이 창으로 보내!"라고 지목해야 하거든.
    HWND hwnd = glfwGetWin32Window(window);

    // ---------------------------------------------------------
    // 5. DirectX 핵심 일꾼 4총사 선언
    // ---------------------------------------------------------
    ID3D11Device* device;                     // [공장장] 버퍼나 셰이더 같은 물건을 새로 '만드는' 역할
    ID3D11DeviceContext* context;             // [작업 반장] 만들어진 물건으로 "그려라!", "지워라!" '명령'하는 역할
    IDXGISwapChain* swapChain;                // [마술사] 뒷면 도화지에 그림을 다 그리면 모니터 앞면이랑 '휙' 바꿔치기 하는 역할
    ID3D11RenderTargetView* renderTargetView; // [도화지] 우리가 실제로 그림을 그릴 목표 지점

    // ---------------------------------------------------------
    // 6. 마술사(스왑체인)와 공장장(디바이스) 고용하기
    // ---------------------------------------------------------
    // [왜 썼을까?] 도화지가 한 장이면 그리는 과정이 다 보여서 화면이 깜빡거려. 
    // 안 보이는 뒷면(백버퍼)에서 다 그리고 완성되면 보여주기 위해 스왑체인 규칙을 정하는 거야.
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;                                    // 그릴 도화지(백 버퍼)를 몇 장 쓸 거냐라는 뜻 뒷면 도화지 1장 쓸게
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // 빨초파투명도 각각 8비트씩(총 32비트) 예쁜 색으로 쓸게
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // 이 도화지는 최종 출력용이야!  그림을 그리는(RENDER TARGET) 결과물(OUTPUT)로 쓰겠다
    scd.OutputWindow = hwnd;                                // 아까 만든 그 윈도우 창에 띄워줘!
    scd.SampleDesc.Count = 1;                               // 테두리 부드럽게 하는 안티앨리어싱 기능은 일단 끌게(1)
    scd.Windowed = TRUE;                                    // 전체화면 말고 창 모드로 해줘!

    // 위에서 정한 규칙대로 공장장, 마술사, 작업반장을 한 번에 소환!
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &scd, &swapChain, &device, nullptr, &context);

    // ---------------------------------------------------------
    // 7. 도화지(Render Target View) 세팅하기
    // ---------------------------------------------------------
    // [왜 썼을까?] 스왑체인이 가진 도화지에 직접 그릴 순 없어. "이 도화지에 그릴게"라고 찜해두는(View 생성) 과정이 필요해.
    ID3D11Texture2D* backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer); // 뒷면 도화지 잠깐 꺼내오기
    device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);   // 그 도화지를 바탕으로 '진짜 그릴 곳' 생성
    backBuffer->Release(); // 원본 도화지는 이제 뷰(View)가 대신 관리하니까 놔줌 (해제 안 하면 메모리 샙니다)

    // ---------------------------------------------------------
    // 8. 마법 주문서(셰이더) 번역 및 생성
    // ---------------------------------------------------------
    // [왜 썼을까?] 아까 적어둔 shaderSource는 사람 말(문자열)이라 GPU가 못 알아들어. 기계어로 번역해야 해.
    ID3DBlob* vsBlob, * psBlob; // 번역된 기계어가 임시로 담길 찰흙 덩어리(Blob)

    // 사람 글 -> 기계어 번역 (VS는 정점, PS는 픽셀)
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    // 번역된 찰흙 덩어리를 공장장한테 줘서 "진짜 셰이더 무기"로 찍어냄
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

    // ---------------------------------------------------------
    // 9. 인풋 레이아웃 (데이터 통역기) 만들기
    // ---------------------------------------------------------
    // [왜 썼을까?] CPU의 Vertex 데이터가 GPU의 VS_INPUT 상자로 들어갈 때, 어떤 게 위치고 어떤 게 색상인지 짝을 맞춰줘야 해.
    D3D11_INPUT_ELEMENT_DESC ied[] = {
        // "POSITION 꼬리표는 0바이트부터 시작해!"
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        // "COLOR 꼬리표는 위치 데이터가 끝나는 12바이트 지점부터 시작해!"
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        // {   HLSL 이름 : SemanticName(예 : "POSITION"), 
        // HLSL 번호: SemanticIndex (보통 0), 
        // 데이터 종류: Format (숫자 몇 개인지, 실수(FLOAT)인지 결정),
        // 데이터 통로 번호: InputSlot (우린 주로 0번 고속도로 사용),
        // 데이터 시작점: AlignedByteOffset (위치는 0, 색상은 앞에 위치가 먹은 12바이트 뒤부터!)
        //데이터 바뀜 규칙: InputSlotClass (점마다 바뀜: PER_VERTEX_DATA),
        // 인스턴스 데이터 넘김: InstanceDataStepRate (지금은 안 쓰니까 무조건 0)   }


    };
    ID3D11InputLayout* inputLayout;
    // ied 요소가 몇 개냐 했을 때 POSITION ,COLOR 해서 총 2줄 이라서 2 작성
    device->CreateInputLayout(ied, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

    // ---------------------------------------------------------
    // 10. 정점 버퍼 (데이터 상자) 만들기
    // ---------------------------------------------------------
    // [왜 썼을까?] 우리가 그릴 삼각형의 점 3개를 컴퓨터 일반 메모리에서 아주 빠른 GPU 전용 메모리로 옮겨줘야 하거든.
    Vertex vertices[] = {
        { 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, // 맨 위쪽 점 (빨간색)
        { 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, // 오른쪽 아래 점 (초록색)
        {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f}  // 왼쪽 아래 점 (파란색)
    };

    ID3D11Buffer* vertexBuffer;
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;          // 기본 용도로 쓸게
    bd.ByteWidth = sizeof(vertices);         // 상자 크기는 점 3개 크기만큼!
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 이 상자의 용도는 '정점(점)' 보관용이야!

    D3D11_SUBRESOURCE_DATA srd = { vertices }; // 상자에 넣을 알맹이(데이터) 연결
    device->CreateBuffer(&bd, &srd, &vertexBuffer); // 공장장아, 상자 하나 만들어줘!

    // ---------------------------------------------------------
    // 11. 메인 루프 (무한 그리기 반복) - 게임 엔진의 심장
    // ---------------------------------------------------------
    // [왜 썼을까?] 게임은 멈춰있는 사진이 아니라, 1초에 60번씩 지우고 그리고를 반복해야 움직이는 것처럼 보이니까!
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); // 키보드나 마우스 눌렀나 확인

        // [1단계: 지우기] 남색 물감으로 이전 프레임 도화지를 싹 덮어버림
        float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        context->ClearRenderTargetView(renderTargetView, clearColor);

        // [2단계: 그릴 곳 정하기] 작업반장아, 아까 만든 그 도화지에 그려!
        context->OMSetRenderTargets(1, &renderTargetView, nullptr);

        // [3단계: 그릴 범위 정하기] 창 안에서 가로 800, 세로 600 꽉 채워서 그려!
        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f };
        context->RSSetViewports(1, &viewport);

        // [4단계: 데이터 넣기] 통역기(레이아웃) 켜고, 아까 만든 '점 3개 상자(버퍼)' 가져와!
        context->IASetInputLayout(inputLayout);
        UINT stride = sizeof(Vertex); // 점 1개 크기만큼 성큼성큼 건너뛰어 읽어라
        UINT offset = 0;              // 처음부터 읽어라
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

        // [5단계: 그리는 방법 정하기] 점 3개가 모이면 '삼각형' 하나로 이어 그려라!
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // [6단계: 색칠 방법 정하기] 아까 만든 마법 무기(셰이더) 2개 장착!
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);

        // [7단계: 진짜 그리기!] 준비 다 됐으니, 점 3개 써서 그려버려!
        context->Draw(3, 0);

        // [8단계: 화면에 보여주기] 뒷면 도화지에 다 그렸으니, 마술사야 모니터 앞면이랑 바꿔쳐라! 짠!
        swapChain->Present(1, 0);
    }

    // ---------------------------------------------------------
    // 12. 뒷정리 (메모리 반납) - 진짜 진짜 중요함!
    // ---------------------------------------------------------
    // [왜 썼을까?] 컴퓨터 메모리는 한정되어 있어서 쓴 걸 안 돌려주면 터져버려. 빌려온 건 역순으로 꼭 갚아야 해!
    vertexBuffer->Release();
    inputLayout->Release();
    vertexShader->Release();
    pixelShader->Release();
    renderTargetView->Release();
    swapChain->Release();
    context->Release();
    device->Release();

    glfwDestroyWindow(window); // 창 부수기
    glfwTerminate();           // GLFW 종료
    return 0;
}
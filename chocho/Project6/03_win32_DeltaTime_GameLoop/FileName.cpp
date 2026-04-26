/*
================================================================================
 [실전 가이드: DeltaTime(dt)을 어떻게 사용해야 하는가?]
================================================================================

 1. 모든 '변화'가 일어나는 곳에 곱하라 (Movement & Rotation)
    - 캐릭터 이동, 회전, 크기 변경 등 '시간에 따라 변하는 것'에는 무조건 dt를 곱함.
    - [공식]: 현재값 += 변화량(속도) * DeltaTime;
    - 이유: 속도의 단위가 '프레임당 이동 거리'에서 '초당 이동 거리'로 바뀌기 때문임.

 2. 물리 법칙의 적용 (Gravity & Acceleration)
    - 중력이나 가속도 역시 시간의 제곱에 비례하거나 시간에 따라 축적됨.
    - [공식]: 속력 += 가속도 * DeltaTime;
             위치 += 속력 * DeltaTime;
    - 이렇게 단계별로 dt를 곱해주면 어떤 PC에서도 동일한 점프 높이와 낙하 속도가 나옴.

 3. 쿨타임 및 타이머 구현 (Cooldowns)
    - "3초 뒤에 스킬 발동" 같은 로직을 구현할 때 스톱워치 역할을 함.
    - [방법]: float timer += DeltaTime;
             if (timer >= 3.0f) { 스킬 발동! }

 4. 보간(Interpolation) 처리 (Lerp)
    - A 지점에서 B 지점으로 부드럽게 이동시킬 때 사용함.
    - t(시간 계수)에 DeltaTime을 누적시켜 부드러운 애니메이션을 만듦.

 5. [주의] 렌더링(Render) 단계에서는 사용하지 마라!
    - DeltaTime은 'Update(로직)' 단계에서 데이터를 계산할 때만 사용함.
    - Render는 계산이 끝난 최종 결과물을 화면에 '그리기'만 해야 함.
      (그리는 도중에 위치를 계산하면 화면이 찢어지는 등의 문제가 생길 수 있음)
================================================================================
*/


/*
================================================================================
 [std::chrono 기반 Variable Delta Time 게임 루프]
================================================================================
 1. std::chrono 사용 이유
    - Win32의 QPC보다 이식성이 좋고, C++ 표준이며, 타입 안정성이 뛰어남.
 2. Update(float dt)의 역할
    - 모든 '변화'는 dt에 의존함. (현재값 += 속도 * dt)
    - dt는 '초(second)' 단위의 실수(float)로 변환하여 사용하는 것이 일반적임.
 3. 왜 Render에는 dt가 없는가?
    - Render는 '현재 상태'를 출력하는 시각화 도구일 뿐임.
    - 로직 계산과 출력을 분리해야 '데이터의 무결성'이 보장됨. (데이터 오염 방지)
================================================================================
*/
#include <iostream>
#include <chrono>   // [표준 시간] C++의 정밀한 시계 도구
#include <thread>   // [잠자기] CPU를 쉬게 해주는 Sleep 도구
#include <windows.h> // [WinAPI] 키보드 입력(GetAsyncKeyState) 도구

// [데이터 주머니 1] 플레이어의 정보
typedef struct {
    float x, y;    // 현재 위치 좌표 (실수형)
    float speed;   // 초당 이동 속도 (200.0f면 1초에 200칸 이동하겠다는 뜻)
} GameObject;

// [데이터 주머니 2] 키보드 입력 상태
typedef struct {
    bool isUp, isDown, isLeft, isRight, isExit;
} InputContext;

// --- [1단계: 입력 감지] ---
// 의미: 윈도우(OS)에게 "지금 W, A, S, D 키 눌렸니?"라고 물어보고 기록함.
void ProcessInput(InputContext* context) {
    // 0x8000: "지금 이 순간 키가 눌려 있는가"를 판별하는 마법의 숫자
    context->isUp = (GetAsyncKeyState('W') & 0x8000);
    context->isDown = (GetAsyncKeyState('S') & 0x8000);
    context->isLeft = (GetAsyncKeyState('A') & 0x8000);
    context->isRight = (GetAsyncKeyState('D') & 0x8000);
    context->isExit = (GetAsyncKeyState(VK_ESCAPE) & 0x8000);
}

// --- [2단계: 로직 업데이트] ---
// 의미: 흐른 시간(dt)만큼 플레이어의 위치(x, y)를 계산해서 옮김.
void Update(GameObject* player, const InputContext* context, float dt) {
    // [공식]: 현재 위치 += 속도 * 흐른 시간(dt)
    // dt를 곱하기 때문에 컴퓨터가 아무리 빨라도 1초에 이동하는 거리는 speed(200)로 고정됨.
    if (context->isUp)    player->y -= player->speed * dt;
    if (context->isDown)  player->y += player->speed * dt;
    if (context->isLeft)  player->x -= player->speed * dt;
    if (context->isRight) player->x += player->speed * dt;
}

// --- [3단계: 화면 그리기] ---
// 의미: 계산된 player->x, y 좌표를 콘솔 화면에 실제 '별'로 찍어서 보여줌.
void Render(const GameObject* player, float fps) {
    system("cls"); // 화면 깨끗이 지우기 (잔상 제거)
    printf("=== Explicit Type Engine ===\n");
    printf("FPS : %.2f | DT : %.4fs\n", fps, 1.0f / fps); // 현재 성능 생중계
    printf("Player: (%.1f, %.1f)\n", player->x, player->y);
    printf("============================\n");

    // 좌표값에 따라 공백과 줄바꿈을 출력해서 별(★)의 위치를 잡음
    int py = (int)(player->y / 10.0f);
    int px = (int)(player->x / 5.0f);
    for (int i = 0; i < py; i++) printf("\n");
    for (int i = 0; i < px; i++) printf(" ");
    printf("★");
}

int main() {
    // [초기화] 플레이어 위치 100, 100 / 속도 200 설정
    GameObject player = { 100.0f, 100.0f, 200.0f };
    InputContext input = { 0 };

    // [기준점 찍기] 루프 돌기 전 첫 번째 시간 기록
    std::chrono::high_resolution_clock::time_point prevTime = std::chrono::high_resolution_clock::now();

    // --- [게임 루프] ---
    while (!input.isExit) {
        // A. 델타 타임(dt) 구하기  현재 시간 찍는 과정
        auto currentTime = std::chrono::high_resolution_clock::now();
        // [현재 - 이전] = 흐른 시간 간격
        std::chrono::duration<float> elapsed = currentTime - prevTime;
        float dt = elapsed.count(); // 시간 객체에서 '0.016' 같은 순수 숫자로 변환
        prevTime = currentTime;     // 다음 계산을 위해 현재 시간을 '이전 시간'으로 갱신

        // B. 엔진 3단계 가동
        ProcessInput(&input);        // 1. 눌렀니?
        Update(&player, &input, dt); // 2. 계산해라! (dt 활용)
        Render(&player, 1.0f / dt);  // 3. 보여줘라! (FPS 표시)

        // C. CPU 과부하 방지 (1ms 잠자기)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }


    return 0;
}
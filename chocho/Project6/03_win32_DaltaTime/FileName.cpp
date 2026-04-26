#include <windows.h> // 윈도우 핵심 기능(QueryPerformance...)을 쓰기 위한 도구함
#include <stdio.h>

// [데이터 설계: 타이머 주머니]
typedef struct {
    LARGE_INTEGER frequency; // [CPU 눈금] 1초에 CPU가 몇 번 진동하는지 (고정된 기준)
    LARGE_INTEGER prevTime;  // [이전 사진] 저번 검사 때의 CPU 진동 횟수 기록
    double deltaTime;        // [결과물] 계산된 시간 차이 (정밀하게 double 사용)
} CGameTimer;

// [InitTimer: 기계 초기화]
// 의미: 출발 전 스톱워치의 영점을 맞추고 CPU의 진동 속도를 파악함.
void InitTimer(CGameTimer* timer) {
    // QueryPerformanceFrequency: "이 컴퓨터 CPU는 1초에 몇 번 떨어?"라고 물어봄. (기준값 획득)


    QueryPerformanceFrequency(&timer->frequency);   // 1. "이 컴퓨터의 1초는 몇 칸으로 나뉘어 있니?"
    QueryPerformanceCounter(&timer->prevTime);  // 2. "지금이 총 몇 번째 칸이니? (출발 버튼 누르기)"                            
    timer->deltaTime = 0.0;                   // 3. "아직 시간은 하나도 안 흘렀으니까 0으로 시작하자!"
}

// [UpdateTimer: 시간 갱신]
// 의미: 매 프레임마다 '진동 횟수의 차이'를 계산해 '초' 단위로 변환함.
void UpdateTimer(CGameTimer* timer) {
    LARGE_INTEGER currentTime;   // 지금 이 순간의 CPU 떨림 횟수"를 담을 그릇을 하나 준비숫자가 커서 일반 int가 아닌 LARGE_INTEGER를 써야 해.
    // 1. 현재 시점의 진동 횟수를 사진 찍듯 가져옴.
    QueryPerformanceCounter(&currentTime);

    // [계산 공식] 
    // (지금 떨림 횟수 - 아까 떨림 횟수) / 1초당 떨림 횟수 = 흐른 시간(초)
    // QuadPart: LARGE_INTEGER라는 큰 상자 안에 든 '진짜 숫자'를 꺼내는 이름표.
    timer->deltaTime = (double)(currentTime.QuadPart - timer->prevTime.QuadPart) /
        (double)timer->frequency.QuadPart;

    // 2. 바톤 터치: 지금 시점을 다음번의 '과거 시점'으로 넘겨줌.
    timer->prevTime = currentTime;
}

int main() {
    CGameTimer myTimer;
    InitTimer(&myTimer); // 기계 전원 ON

    for (int i = 0; i < 10; ++i) {
        UpdateTimer(&myTimer); // 시간 계산!

        // 1.0 / deltaTime: 1초를 델타타임으로 나누면 '1초에 몇 번 도는지(FPS)'가 나옴.
        printf("DeltaTime: %f sec (FPS: %f)\n", myTimer.deltaTime, 1.0 / myTimer.deltaTime);

        Sleep(100); // 테스트를 위해 0.1초 강제 지연
    }
    return 0;
}
#include <stdio.h>

// --- [데이터 바구니] ---
typedef struct {
    int playerPos;     // (1) 캐릭터의 현재 위치 (0~10 사이)
    int isRunning;     // (2) 게임 실행 스위치 (1: 켜짐, 0: 꺼짐)
    char currentInput;  // (3) 방금 입력받은 키보드 글자 저장소
} GameContext;

// --- [1단계: 입력 단계 (Process Input)] ---
void ProcessInput(GameContext* ctx) {
    printf("\n[A:왼쪽, D:오른쪽, Q:종료] 입력하세요: ");

    // (1) scanf_s 앞의 공백: 이전 입력의 잔상(엔터 키 등)을 지우는 장치
    // (2) ctx->currentInput: 입력받은 글자를 바구니에 담아 다음 단계로 전달
    scanf_s(" %c", &(ctx->currentInput), (unsigned int)sizeof(char));
}

// --- [2단계: 업데이트 단계 (Update)] ---
void Update(GameContext* ctx) {
    // (1) 종료 규칙 적용: Q를 누르면 게임 종료 스위치를 0으로 바꿈
    if (ctx->currentInput == 'q' || ctx->currentInput == 'Q') {
        ctx->isRunning = 0;
        return;
    }

    // (2) 로직 계산: A는 감소, D는 증가 (여기서는 화면에 그리지 않고 '숫자'만 바꿈!)
    if (ctx->currentInput == 'a' || ctx->currentInput == 'A') {
        ctx->playerPos--;
    }
    else if (ctx->currentInput == 'd' || ctx->currentInput == 'D') {
        ctx->playerPos++;
    }

    // (3) 세상의 경계 규칙: 캐릭터가 화면 밖(0~10)으로 나가지 못하게 막는 '충돌 체크'
    if (ctx->playerPos < 0) ctx->playerPos = 0;
    if (ctx->playerPos > 10) ctx->playerPos = 10;
}

// --- [3단계: 출력 단계 (Render)] ---
void Render(GameContext* ctx) {
    // (1) 화면 초기화: 빈 줄을 많이 출력해서 이전 프레임을 밀어냄 (지우기 효과)
    printf("\n\n\n\n\n");

    printf("========== GAME SCREEN ==========\n");
    printf(" Player Position: %d\n", ctx->playerPos); // 숫자로 상태 표시

    printf(" [");
    // (2) 데이터 시각화: 바구니의 playerPos 숫자를 보고 'P'의 위치를 그림
    for (int i = 0; i <= 10; i++) {
        if (i == ctx->playerPos) printf("P"); // 데이터 위치에 캐릭터 배치
		else printf("_"); // 빈 공간은 밑줄로 표시
    }
    printf("]\n");
    printf("=================================\n");
}

int main() {
    // (1) 초기화: 시작 위치는 5번, 게임은 켜진 상태(1)로 세팅
    GameContext game = { 5, 1, ' ' };

    printf("게임을 시작합니다. (정석 루프 패턴)\n");

    // [ 엔진의 심장: 무한 루프 ]
    while (game.isRunning) {
        ProcessInput(&game); // 1. 무슨 일이 일어났나? (입력)
        Update(&game);       // 2. 그럼 세상은 어떻게 변하나? (데이터 계산)
        Render(&game);       // 3. 변한 결과를 나한테 보여줘! (출력)
    }

    printf("게임이 안전하게 종료되었습니다.\n");
    return 0;
}
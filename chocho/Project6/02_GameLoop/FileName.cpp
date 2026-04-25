#include <stdio.h>

// [데이터 바구니] 게임 세상의 정보를 저장하는 구조체
typedef struct {
    int playerPos;     // 캐릭터의 현재 위치 (0~10 사이의 숫자 데이터)
    int isRunning;     // 1이면 게임 실행, 0이면 프로그램 종료 스위치
    char currentInput;  // 방금 입력받은 키보드 값을 잠시 보관
} GameContext;

// --- [1단계: Process Input] ---
// 사용자가 '무슨 짓'을 했는지 알아내는 단계
void ProcessInput(GameContext* ctx) {
    printf("\n[A:왼쪽, D:오른쪽, Q:종료] 입력하세요: ");
    // 사용자가 입력한 글자를 가져와서 바구니(ctx)에 담음
    scanf_s(" %c", &(ctx->currentInput), (unsigned int)sizeof(char));
}

// --- [2단계: Update] ---
// "세상의 법칙"을 계산하는 단계 (데이터만 건드림)
void Update(GameContext* ctx) {
    // 1. 종료 규칙 적용: Q를 눌렀다면 스위치를 꺼라
    if (ctx->currentInput == 'q' || ctx->currentInput == 'Q') {
        ctx->isRunning = 0;
        return;
    }

    // 2. 이동 규칙 적용: A는 위치 감소, D는 위치 증가
    if (ctx->currentInput == 'a' || ctx->currentInput == 'A') {
        ctx->playerPos--;
    }
    else if (ctx->currentInput == 'd' || ctx->currentInput == 'D') {
        ctx->playerPos++;
    }

    // 3. 경계 규칙 적용: 캐릭터가 맵 밖(0~10)으로 나가지 못하게 함 (Collision 흉내)
    if (ctx->playerPos < 0) ctx->playerPos = 0;
    if (ctx->playerPos > 10) ctx->playerPos = 10;
}

// --- [3단계: Render] ---
// 계산이 끝난 "데이터 상태"를 모니터에 그림 (로직 계산 안 함!)
void Render(GameContext* ctx) {
    // 실제 게임처럼 화면을 새로 그리는 느낌을 주기 위해 빈 줄 출력
    printf("\n\n\n\n\n");

    printf("========== GAME SCREEN ==========\n");
    printf(" Player Position: %d\n", ctx->playerPos); // 숫자로 출력
    printf(" [");
    // 데이터를 기반으로 시각적인 막대 그래프를 그림
    for (int i = 0; i <= 10; i++) {
        if (i == ctx->playerPos) printf("P"); // 데이터 위치에만 P(캐릭터)를 그림
        else printf("_");
    }
    printf("]\n");
    printf("=================================\n");
}

int main() {
    // 초기화: 캐릭터 위치는 5번, 게임은 켜져 있는 상태로 시작
    GameContext game = { 5, 1, ' ' };

    printf("게임을 시작합니다. (정석 루프 패턴)\n");

    // [ 엔진의 심장: 무한 루프 ]
    while (game.isRunning) {
        ProcessInput(&game); // 1. 입력 : 누가 뭘 했나?
        Update(&game);       // 2. 업데이트 : 그럼 세상은 어떻게 변하나?
        Render(&game);       // 3. 출력 : 변한 걸 나한테 보여줘!
    }

    printf("게임이 안전하게 종료되었습니다.\n");
    return 0;
}
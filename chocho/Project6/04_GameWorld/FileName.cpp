/*
================================================================================
 [가이드: 게임 엔진의 뼈대 만들기]
================================================================================
 1. Component (기능): 캐릭터가 할 수 있는 '일' (이동, 시간 재기 등)
 2. GameObject (객체): 게임에 존재하는 '물체' (플레이어, 타이머 등)
 3. GameWorld (세계): 모든 물체를 담고 있는 '바구니'

 * 구조: Component -> GameObject -> GameWorld 순으로 확장됨.
         (루프 한 번 돌 때 [입력 -> 업데이트 -> 렌더링] 순서로 모든 객체를 훑음.)
 [작동 원리]
 - Start(): 물체가 태어날 때 딱 한 번 실행되는 초기화 코드
 - Input(): 키보드/마우스 상태를 확인.
 - Update(): 수치(좌표 등)를 계산.
 - Render(): 화면에 결과를 출력.


================================================================================
 [가이드2: 객체지향과 추상화]
================================================================================
 1. 왜 GameObject와 Component를 분리하는가? (Composition vs Inheritance)
  - 상속(Inheritance)의 한계: "플레이어는 캐릭터다", "NPC는 캐릭터다" 식의 설계는
    기능이 복합해질수록 계층 구조가 꼬임 (예: 날아다니는 상자? 캐릭터인가 아이템인가?)
  - 조립(Composition)의 장점: GameObject는 빈 껍데기일 뿐이며, 필요한 기능(Component)을
    마치 레고 블록처럼 끼워 맞추는 방식임. 유연성과 재사용성이 극대화됨.

 2. 순수 가상 함수(= 0)를 사용하는 설계적 이유
  - [강제성]: 부모 클래스(Component)는 "모든 기능은 반드시 실행(Update)되어야 한다"는
    규칙만 정의함. 실제 세부 내용은 자식(PlayerControl 등)이 결정함.
  - [추상 클래스]: Component 자체는 불완전한 클래스이므로 'new Component()'처럼
    단독 객체 생성을 문법적으로 막아버림. (실수 방지)

 3. 가상 함수({})와 가상 소멸자의 필요성
  - [선택적 확장]: Input()이나 Render()를 일반 가상 함수로 둔 이유는, 모든 객체가
    입력을 받거나 그려질 필요는 없기 때문임. (예: 보이지 않는 데이터 관리자)
  - [다형성 소멸]: GameObject가 Component* 포인터로 자식을 관리하므로,
    부모의 소멸자가 virtual이어야만 delete 시 실제 자식 객체의 메모리까지 해제됨.

 4. 실행 순서의 약속 (Lifecycle)
  - Start -> Input -> Update -> Render 순서의 규격화는 수천 개의 객체가
    서로 꼬이지 않고 예측 가능한 순서로 동작하게 만드는 엔진의 '철로' 역할을 함.
================================================================================
*/




#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>
#include <vector>
#include <string>

// 콘솔의 특정 좌표로 커서를 이동시키는 함수
void MoveCursor(int x, int y)
{
    // \033[y;xH  (y와 x는 1부터 시작함)
    printf("\033[%d;%dH", y, x);
}




// ==============================================================================
// [1단계: 컴포넌트 기저 클래스]
// 모든 기능(이동, 렌더링, 사운드 등)의 조상님이 되는 추상 클래스
// ==============================================================================
class Component
{
public:
    class GameObject* pOwner = nullptr; // 이 기능이 누구의 것인지 저장
    bool isStarted = 0;           // Start()가 실행되었는지 체크(중복 실행 방지)


     // [순수 가상 함수: = 0]
     // 자식 클래스에서 "무조건" 구현해야 하는 필수 기능. 안 만들면 컴파일 에러 발생!
    //부모 포인터로 자식 객체를 삭제할 때, 자식의 소멸자가 호출되지 않아 발생하는 메모리 누수를 방지하기 위해 부모 소멸자 앞에 반드시 붙여야 하는 키워드
    //virtual (가상 소멸자)

    virtual void Start() = 0;              // 탄생 직후 초기화 (데이터 세팅) 
    virtual void Update(float dt) = 0;     // 매 프레임 수치 계산 (이동, 물리 등)
    /*
     * 2. 일반 가상 함수 (Virtual Function): {}
     * - "기본 동작은 비어있으니, 필요하면 재정의(Override)해서 쓰라"는 의미임.
     * - 이유: 모든 컴포넌트가 입력을 받거나(Input) 화면에 그릴(Render) 필요는 없음.
     * (예: 점수 계산 컴포넌트는 화면에 그릴 필요가 없을 수 있음)
     * - 특징: 구현하지 않아도 컴파일 에러가 나지 않으며, 호출 시 아무 일도 일어나지 않음.
     */

     // [일반 가상 함수: {}]
     // "필요한 놈만 재정의해서 써라"는 뜻. 구현 안 하면 그냥 아무 일도 안 함.
    virtual void Input() {}                // 키보드/마우스 입력 처리 (선택사항)
    virtual void Render() {}               // 화면에 그리기 (선택사항)

    // [가상 소멸자] 
    // 부모 포인터로 자식을 지울 때, 자식의 소멸자까지 안전하게 호출하기 위한 필수 장치
    virtual ~Component() {}
};
    

// [2단계: 게임 오브젝트 클래스]
// 컴포넌트들을 담는 바구니 역할을 함.
class GameObject {
public:
    std::string name;                       // 물체의 이름 (예: "Player1")
    std::vector<Component*> components;     // 이 물체에 장착된 모든 부품 목록

    GameObject(std::string n)               //(생성자)
    {
        name = n;
    }

    // 객체가 죽을 때 담고 있던 컴포넌트들도 메모리에서 해제함
    //물체(본체)가 파괴될 때, 그 몸에 붙어있던 **모든 부품(components)들을 하나하나 delete**로 지워주는 거야.
    ~GameObject() {                                             //(소멸자) ★제일 중요
        for (int i = 0; i < (int)components.size(); i++)
        {
            delete components[i];
        }
    }

    // 새로운 기능을 추가하는 함수
    void AddComponent(Component* pComp)
    {
		pComp->pOwner = this;           // "이 기능은 나의 것이다!"라고 기능에게 알려줌
		pComp->isStarted = false;     // "아직 Start() 안 했으니까, 다음 루프 때 해!"라고 표시함
		components.push_back(pComp);    // 기능 목록에 새 기능을 추가함
    }
};

// --- [3단계: 실제 구현할 기능 컴포넌트들] ---

// 기능 1: 플레이어 조종 및 이동
// Component를 상속받아 "나도 엔진 시스템의 일원이다!"라고 선언함.
class PlayerControl : public Component {
public:
    float x, y, speed; // 플레이어의 현재 위치(좌표)와 이동 속력
    bool moveUp, moveDown, moveLeft, moveRight; // 지금 어떤 키가 눌려있는지 저장하는 스위치
    int playerType = 0; // 0이면 1P(WASD), 1이면 2P(방향키)로 구분

    // 1. [생성자] 부품을 처음 만들 때 1P인지 2P인지 결정함
    PlayerControl(int type)
    {
        playerType = type;
    }

    // 2. [Start: 초기화] 태어나자마자 할 일
    void Start() override
    {
        // 초기 위치 (50, 50) 및 속도(1초에 150픽셀) 설정
        x = 50.0f; y = 50.0f; speed = 150.0f;
        moveUp = moveDown = moveLeft = moveRight = false;

        // pOwner->name: "내 본체(GameObject)의 이름"을 출력하여 어떤 물체에 장착됐는지 확인
        printf("[%s] PlayerControl 기능 시작!\n", pOwner->name.c_str());
    }

    // 3. [Input: 입력 감지] 매 프레임마다 "키 눌렸니?"라고 폴링(Polling)함
    void Input() override
    {
        // playerType에 따라 감시할 키보드 버튼을 다르게 설정 (다인용 게임 가능!)
        if (playerType == 0) // WASD 조작
        {
            moveUp = (GetAsyncKeyState('W') & 0x8000);
            moveDown = (GetAsyncKeyState('S') & 0x8000);
            moveLeft = (GetAsyncKeyState('A') & 0x8000);
            moveRight = (GetAsyncKeyState('D') & 0x8000);
        }
        if (playerType == 1) // 방향키 조작
        {
            moveUp = (GetAsyncKeyState(VK_UP) & 0x8000);
            moveDown = (GetAsyncKeyState(VK_DOWN) & 0x8000);
            moveLeft = (GetAsyncKeyState(VK_LEFT) & 0x8000);
            moveRight = (GetAsyncKeyState(VK_RIGHT) & 0x8000);
        }
    }


    // 4. [Update: 수치 계산] 입력된 키 상태에 델타 타임을 곱해 실제 이동 거리를 계산
    void Update(float dt) override
    {
        // [핵심 공식]: 위치 += 속도 * 흐른 시간(dt)
        // 위(Y-) / 아래(Y+) / 왼쪽(X-) / 오른쪽(X+) 좌표 갱신
        if (moveUp)    y -= speed * dt;
        if (moveDown)  y += speed * dt;
        if (moveLeft)  x -= speed * dt;
        if (moveRight) x += speed * dt;
    }



    // 5. [Render: 시각화] 계산된 좌표(x, y)에 맞춰 콘솔 화면에 별을 그림
    void Render() override
    {
        // 화면 밖(UI 영역 등)으로 캐릭터가 이탈하지 못하게 막는 '클리핑' 로직
        if (x < 10.0f) x = 10.0f;
        if (y < 45.0f) y = 45.0f;

        // [해상도 변환]: 정밀한 실수 좌표를 콘솔의 듬성듬성한 칸(int)으로 번역
        int py = (int)(y / 15.0f);
        int px = (int)(x / 10.0f);

        // 해당 칸으로 커서를 순간이동 시킴
        MoveCursor(px, py);

        // 플레이어 타입에 따라 검은 별(★)과 흰 별(☆)로 구분해서 그림
        if (playerType == 0)
            printf("★");
        if (playerType == 1)
            printf("☆");

    }
};

// [부품 구현: 시스템 정보 출력 기능]
// 캐릭터처럼 움직이지는 않지만, 게임의 상태(시간 등)를 보여주는 보조 부품이야.
class InfoDisplay : public Component
{
public:
    float totalTime = 0.0f; // 게임이 시작된 후 지금까지 흐른 총 시간을 담는 바구니

    // 1. [Start: 초기화] 부품이 태어날 때 실행
    void Start() override
    {
        totalTime = 0.0f; // 시계 바늘을 0으로 맞춤
        // pOwner->name: 이 부품이 붙어있는 물체(예: "SystemManager")의 이름을 출력해 확인
        printf("[%s] InfoDisplay 기능 시작!\n", pOwner->name.c_str());
    }

    // 2. [Update: 수치 계산] 매 프레임 흐른 시간을 계속 더함
    void Update(float dt) override
    {
        // [누적의 원리]: 델타 타임(dt)을 계속 더해주면 게임의 전체 진행 시간이 돼.
        // 예: 0.016 + 0.016 + ... = 10.0초
        totalTime += dt;
    }

    // 3. [Render: 화면 출력] 계산된 총 시간을 화면 맨 위에 찍어줌
    void Render() override {
        // [고정 위치]: 캐릭터처럼 좌표를 계산하는 게 아니라, 항상 (0, 0) 즉 왼쪽 맨 위로 커서를 옮겨.
        MoveCursor(0, 0);

        // %.2f: 소수점 둘째 자리까지만 예쁘게 시간을 출력해 (예: 12.34 sec)
        printf("System Time: %.2f sec\n", totalTime);

        // 조작법을 유저에게 알려주는 가이드 문구 출력
        printf("Control: W, A, S, D | Exit: ESC\n");
    }
};

// [4단계: 엔진의 심장(GameLoop)]
// 모든 게임 오브젝트를 관리하고, 일정한 순서로 시스템을 실행시키는 관리자 클래스
class GameLoop
{
public:
    bool isRunning;                     // 게임이 계속 돌아가야 하는지 결정하는 스위치
    std::vector<GameObject*> gameWorld; // 이 세상에 존재하는 모든 물체(GameObject)들의 바구니
    std::chrono::high_resolution_clock::time_point prevTime; // 바로 직전 프레임의 시간 기록
    float deltaTime;                    // 프레임 사이의 시간 간격 (0.016초 같은 숫자)

    // 1. [초기화] 게임 시작 전 엔진의 상태를 깨끗하게 세팅
    void Initialize()
    {
        isRunning = true;     // 일단 엔진 가동 준비 완료!
        gameWorld.clear();    // 세상 바구니 비우기

        // 기준 시간 측정 및 델타타임 0으로 초기화
        prevTime = std::chrono::high_resolution_clock::now();
        deltaTime = 0.0f;
    }

    // 2.[함수: 입력 단계] 사용자가 키보드나 마우스로 어떤 명령을 내렸는지 확인
    void Input()
    {
        // 만약 ESC 키가 눌렸다면(0x8000), 엔진 전원 스위치(isRunning)를 꺼버림
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) isRunning = false;

        // 이중 반복문: [세상의 모든 물체들] -> [각 물체가 가진 모든 부품(Component)]

        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                // "부품아, 키보드 입력된 거 있니?"라고 물어보고 상태를 기록함
                gameWorld[i]->components[j]->Input();
            }
        }
    }

    // 3. [업데이트 단계] 입력된 정보를 바탕으로 위치, 물리, 시간 등을 계산함
    void Update()
    {
        // [C. Start 실행]: 탄생 직후 할 일 (유니티의 Start() 방식)
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                // 부품의 isStarted가 false라면? "너 아직 태어난 직후구나? Start() 해!"
                //"바구니 속 i번째 물체의 가방 안에 든 j번째 부품아, 이제 전원 켜고 일을 시작해라!"
                if (gameWorld[i]->components[j]->isStarted == false)
                {
                    gameWorld[i]->components[j]->Start(); // 초기화 실행
                    gameWorld[i]->components[j]->isStarted = true; // 이제 시작 완료됨!
                }
            }
        }

        // [D. 진짜 업데이트]: 계산 시작
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                // 부품아, 아까 잰 델타타임(deltaTime) 줄게. 이만큼만 좌표를 옮겨!
                gameWorld[i]->components[j]->Update(deltaTime);
            }
        }
    }

    // 4. [렌더링 단계] 계산이 끝난 결과를 화면에 출력
    void Render()
    {
        system("cls"); // 화면을 싹 지우고 새로 그릴 준비 (깜빡임의 원인이기도 함)
        // 세상의 모든 물체의 모든 부품을 돌면서...
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                // 부품아, 네가 있는 좌표에 별 찍어라!
                gameWorld[i]->components[j]->Render();
            }
        }
    }

    // 5. [실행부] 실질적인 무한 루프가 돌아가는 곳
    void Run()
    {
        while (isRunning) { // ESC 누르기 전까지는 멈추지 않아!

            // A. 시간 관리: 이번 바퀴가 도는 데 걸린 시간(dt) 계산
            auto currentTime = std::chrono::high_resolution_clock::now();
            // [지금 시간 - 아까 시간] = 실제 흐른 시간 간격 구하기
            std::chrono::duration<float> elapsed = currentTime - prevTime; // 차이 구하기
            deltaTime = elapsed.count(); // 숫자(float)로 변환
            prevTime = currentTime;     // 다음 바퀴를 위해 시간 갱신 (바톤 터치)

            // 정해진 3단계 루틴을 순서대로 가동
            Input();   // 1. 사용자 명령 듣기
            Update();  // 2. 수치 계산하기
            Render();  // 3. 화면에 그리기


            // C. CPU 배려: 너무 빨리 돌면 컴퓨터가 뜨거워지니 0.01초간 강제 휴식
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // [생성자] 엔진 객체가 만들어질 때 자동으로 초기화 호출
    GameLoop() { Initialize(); }

    // [소멸자] 엔진이 꺼질 때 메모리 청소
    ~GameLoop()
    {
        // [정리]: 세상 바구니에 담았던 모든 물체들을 하나씩 삭제함
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            // 세상의 모든 물체들 파괴! (물체 소멸자가 그 안의 부품들도 같이 지워줌)
            delete gameWorld[i];
        }
    }
};

// --- [4단계: 메인 엔진 루프] ---
// [4단계: 메인 엔진 루프 및 객체 생성]
// 프로그램이 시작되는 입구! 여기서 물체를 만들고 부품을 조립해.
int main()
{
    
    GameLoop gLoop;     // 1. [엔진 소환]: 게임의 심장인 GameLoop 객체를 메모리에 만듦
    gLoop.Initialize(); // 2. [0점 조절]: 엔진의 시간과 바구니를 깨끗하게 초기화함

  
    // 3. [시스템 관리자 생성]: 화면에 시간을 띄워줄 보이지 않는 관리자 만들기
    // "SystemManager"라는 이름표를 붙인 빈 상자(GameObject)를 하나 생성함
    GameObject* sysInfo = new GameObject("SystemManager");

    // 그 상자에 'InfoDisplay'(시간 출력 기능) 부품을 새로 만들어서 끼워 넣음
    sysInfo->AddComponent(new InfoDisplay());

    // 완성된 관리자 상자를 엔진의 세상 바구니(gameWorld)에 집어넣음
    gLoop.gameWorld.push_back(sysInfo);


    // 4. [1번 플레이어 생성]: WASD로 움직이는 별(★) 만들기
    GameObject* player1 = new GameObject("Player1");        // "Player1"이라는 이름의 빈 상자를 만듦

    player1->AddComponent(new PlayerControl(0));    // 'PlayerControl(0)': 0번 타입(WASD) 조종기 부품을 조립함
    gLoop.gameWorld.push_back(player1);  // 엔진 바구니에 1번 플레이어 추가


    // 5. [2번 플레이어 생성]: 방향키로 움직이는 별(☆) 만들기
    GameObject* player2 = new GameObject("Player2");    // "Player2"라는 이름의 빈 상자를 만듦
    player2->AddComponent(new PlayerControl(1));    // 'PlayerControl(1)': 1번 타입(방향키) 조종기 부품을 조립함
    gLoop.gameWorld.push_back(player2);    // 엔진 바구니에 2번 플레이어 추가

    // ---------------------------------------------------------
    // 6. [엔진 가동]: 모든 조립이 끝났으니 무한 루프를 시작함!
    // 이때부터 Input -> Update -> Render가 무한히 반복되며 게임이 돌아감
    gLoop.Run();

    // 게임이 끝나면(ESC 등) 안전하게 종료 보고
    return 0;
}
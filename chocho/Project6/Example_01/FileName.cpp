#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>
#include <vector>
#include <string>

void MoveCursor(int x, int y)
{
    printf("\033[%d;%dH", y, x);
}

class Component
{
public:
    class GameObject* pOwner = nullptr;
    bool isStarted = false;

    // 모든 가상 함수 뒤에 = 0을 붙여서 '순수 가상 함수'로 만드세요!
    virtual void Start() = 0;
    virtual void Update(float dt) = 0;
    virtual void Input() = 0;
    virtual void Render() = 0;

    // 소멸자도 반드시 몸통 {} 이 있어야 합니다.
    virtual ~Component() {}
};

class GameObject {
public:
    std::string name;
    std::vector<Component*> components;

    GameObject(std::string n)
    {
        name = n;
    }

    ~GameObject()
    {
        //5번문제 버그발생지점

        for (int i = 0; i < (int)components.size(); i++)
        {
            delete components[i]; // 담겨있는 모든 컴포넌트 메모리 해제
        }
        components.clear();
    }

    void AddComponent(Component* pComp)
    {
        pComp->pOwner = this;
        pComp->isStarted = false;
        components.push_back(pComp);
    }
};

class PlayerControl : public Component {
public:
    float x, y, speed;
    bool moveUp, moveDown, moveLeft, moveRight;

    void Start() override
    {
        x = 50.0f; y = 50.0f; speed = 150.0f;
        moveUp = moveDown = moveLeft = moveRight = false;
    }

    void Input() override
    {
        moveUp = (GetAsyncKeyState('W') & 0x8000);
        moveDown = (GetAsyncKeyState('S') & 0x8000);
        moveLeft = (GetAsyncKeyState('A') & 0x8000);
        moveRight = (GetAsyncKeyState('D') & 0x8000);
    }

    void Update(float dt) override
    {
        if (moveUp)    y -= speed * dt;
        if (moveDown)  y += speed * dt;
        if (moveLeft)  x -= speed * dt;
        if (moveRight) x += speed * dt;
    }

    void Render() override
    {
        int px = (int)(x / 10.0f);
        int py = (int)(y / 15.0f);
        MoveCursor(px, py);
        printf("★");
    }
};

class GameLoop {
public:
    bool isRunning;
    std::vector<GameObject*> gameWorld;
    std::chrono::high_resolution_clock::time_point currentTime;
    std::chrono::high_resolution_clock::time_point prevTime;
    float deltaTime;

    void Initialize()
    {
        isRunning = true;
        gameWorld.clear();
        currentTime = std::chrono::high_resolution_clock::now();

        // [중요] 이전 시간도 일단 현재 시간과 똑같이 맞춰서 0부터 시작하게 함
        prevTime = currentTime;
        deltaTime = 0.0f;
    }

    void Run()
    {
        //Initialize() 함수에서 isRunning을 true로 설정했어. 
        // 그런데 92번 줄 조건이 !isRunning이면 **"실행 중이 아닐 때만 돌아라"**는 뜻이 되지?
        // 시작하자마자 조건이 false가 되니까 루프를 한 번도 안 돌고 바로 끝나버리는 거야
        // .while (isRunning)으로 고쳐야 해.
        while (isRunning)
        {
            currentTime = std::chrono::high_resolution_clock::now(); 

            std::chrono::duration<float> elapsed = currentTime - prevTime;
            deltaTime = elapsed.count();
            prevTime = currentTime;

            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) isRunning = false;

            for (int i = 0; i < (int)gameWorld.size(); i++)
            {
                for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
                {
                    Component* comp = gameWorld[i]->components[j];
                    if (comp->isStarted == false)
                    {
                        comp->Start();
                        comp->isStarted = true;
                    }
                    comp->Input();
                    comp->Update(deltaTime);
                }
            }

            system("cls");
            for (int i = 0; i < (int)gameWorld.size(); i++)
            {
                for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
                {
                    gameWorld[i]->components[j]->Render();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    ~GameLoop() {
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            delete gameWorld[i];
        }
    }
};

int main()
{
    GameLoop gLoop;
    gLoop.Initialize();

    GameObject* player = new GameObject("Player1");
    player->AddComponent(new PlayerControl());
    gLoop.gameWorld.push_back(player);

    gLoop.Run();

    return 0;
}
/*
================================================================================
 [핵심 개념: Delta Time (Δt) - 게임 엔진의 시간 동기화]
================================================================================

 1. 'Delta(Δ)'의 어원과 의미
    - 수학/물리에서 그리스 문자 Δ(Delta)는 '차이' 또는 '변화량'을 의미함.
    - 게임에서 Delta Time(Δt) = (현재 프레임 시간 - 이전 프레임 시간).
    - 즉, "한 프레임을 실행하는 데 걸린 실제 시간 간격"을 뜻함.

 2. 왜 Delta Time이 필요한가? (프레임 독립성: Frame Rate Independence)
    - 성능이 다른 컴퓨터(슈퍼컴 vs 똥컴)에서 게임 속도는 동일해야 함.
    - [틀린 방식]: Position += Speed;
      -> 1초에 100번 루프 도는 컴은 100만큼 이동, 10번 도는 컴은 10만큼 이동 (불공평!)
    - [옳은 방식]: Position += Speed * DeltaTime;
      -> 루프 횟수가 달라도 '실제 흐른 시간'을 곱해주면 1초 뒤 이동 거리는 정확히 같아짐.

 3. 물리적 비유 (현실 세계의 동기화)
    - 시속 60km로 달리는 차는 1시간(Δt) 동안 60km를 가고, 1분(Δt) 동안 1km를 감.
    - Delta Time은 우리 게임 속 물체가 '현실의 시간 흐름'에 맞춰 움직이게 만드는 마법의 숫자임.

 4. 데이터 타입과 단위
    - 단위: 주로 '초(Second)' 단위를 사용함 (예: 0.016s).
    - 타입: 소수점 아래 정밀도가 중요하므로 float 또는 double을 사용함.

 5. 구현 시 주의사항 (Clamping)
    - 컴퓨터가 순간적으로 렉(Lag)이 걸려 DeltaTime이 너무 커지면(예: 1초 이상),
      물체가 벽을 뚫고 텔레포트하는 버그가 생길 수 있음.
    - 그래서 엔진에서는 보통 dt = min(dt, 0.1f) 정도로 최대치를 제한하는 처리를 함.
================================================================================
*/
#include <iostream>
#include <chrono> 
#include <thread>

/*
 * [클래스 설계도: CPPGameTimer]
 * 목적: 프레임 사이의 시간 간격(Delta Time)을 계산하여 게임의 속도를 일정하게 유지함.
 */
class CPPGameTimer {
public: // [공개 구역] 누구나 버튼을 눌러 타이머를 작동시킬 수 있는 통로

    // 1. [생성자: 타이머 시작]
    // 의미: 객체가 생성되는 찰나의 '지금'을 첫 번째 기준점으로 기록함.
    CPPGameTimer() {
        // std::chrono::steady_clock::now(): 거꾸로 가지 않는 정밀 시계의 현재 시점.
        // prevTime: 이 시각을 '이전 시간' 변수에 담아 뺄셈 준비 완료.
		//system_clock 게 아닌 steady_clock 써야 함 (게임에서는 절대 뒤로 가지 않는 시계가 필요하기 때문)
        prevTime = std::chrono::steady_clock::now();
    }

    // 2. [업데이트: 시간 차이 계산]
    // 의미: "마지막 검사 이후로 정확히 몇 초가 흘렀는가?"를 계산하고 기준점을 갱신함.
    float Update() {
        // currentTime: 함수가 호출된 '지금 이 순간'의 시각을 사진 찍듯 기록.
        auto currentTime = std::chrono::steady_clock::now();

        // frameTime: [현재 시각 - 이전 시각]을 계산하여 '시간의 양(Duration)'을 구함.
        std::chrono::duration<float> frameTime = currentTime - prevTime;

        // .count(): 시간 데이터에서 우리가 쓸 수 있는 '순수한 실수 숫자'만 추출.
        // deltaTime: 추출된 숫자를 보관함에 저장 (프레임 독립성 구현의 핵심).
        deltaTime = frameTime.count();

        // 바톤 터치: 다음 프레임 계산을 위해 '지금 시간'을 '이전 시간'으로 업데이트.
        prevTime = currentTime;

        return deltaTime; // 계산된 최종 시간값(초)을 외부로 반환.
    }

    // 3. [조회 창구: 현재 시간값 확인]
    // const: "데이터를 읽기만 할 뿐, 기계 내부 설정은 절대 건드리지 않겠다"는 안전 약속.
    float GetDeltaTime() const { return deltaTime; }

private: // [금고 구역] 외부에서 직접 수정하면 시간 축이 뒤틀리는 핵심 변수 보호

    // time_point: 특정 시점의 시각을 담는 전용 필름.
    std::chrono::steady_clock::time_point prevTime;

    // deltaTime: 계산된 프레임 간격. 0.0f로 초기화하여 초기 쓰레기 값 방지.
    float deltaTime = 0.0f;
};

// ==============================================================================

int main() {
    // [1] 기계 생성: 타이머 객체 timer를 만들고 시작 시점을 잡음.
    CPPGameTimer timer;

    std::cout << "C++ STL chrono 타이머 테스트 시작" << std::endl;

    // [2] 루프 테스트: 10번 동안 시간 간격을 체크함.
    for (int i = 0; i < 10; ++i) {

        // [3] 업데이트 호출: "직전 루프에서 지금껏 몇 초 지났어?"라고 물어봄.
        // dt: 약 0.1초 근처의 숫자가 저장됨.
        float dt = timer.Update();

        // [4] 결과 확인: 계산된 델타 타임을 초(s) 단위로 출력.
        std::cout << "프레임 간격(dt): " << dt << "s" << std::endl;

        // [5] 의도적 지연: 100ms(0.1초)를 쉬게 하여 타이머가 잘 작동하는지 확인.
        // std::this_thread::sleep_for: 프로그램을 잠시 멈추는 기능.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0; // 프로그램 정상 종료 보고.
}
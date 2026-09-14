#include "stdafx.h"
#include "GameWorld.h"
#include <algorithm>
#include <cmath>

float GameWorld::Distance(Vec2 a, Vec2 b) { Vec2 d = a - b; return std::sqrt(d.x * d.x + d.y * d.y); }
GameWorld::GameWorld() {
    districts = { {-10,-5,-3,5,0}, {-3,-1,0,2,5}, {0,-5,8,5,1},
        {2,-8,4,-4,5}, {0,-15,8,-8,2}, {8,-1,12,1,5},
        {12,-6,20,4,3}, {14,-10,16,-5,5}, {11,-17,20,-10,4} };
    props = {
        {PropKind::Ship,{-7.4f,.2f},3,2.5f,78},
        {PropKind::Habitat,{1.8f,-3.3f},2.7f,2.1f,102,0},
        {PropKind::Habitat,{6.6f,-2.8f},2.4f,2.3f,140,1},
        {PropKind::Habitat,{1.2f,3.6f},1.8f,1.7f,75,2},
        {PropKind::Archive,{16.9f,-4.4f},4.5f,2.3f,125,0},
        {PropKind::Habitat,{19.1f,2.3f},1.4f,2.1f,90,2},
        {PropKind::Core,{15.5f,-13.2f},1.8f,1.8f,205},
        {PropKind::Citizen,{2.4f,.8f},.3f,.3f,58,0,false},
        {PropKind::Citizen,{6.5f,.8f},.3f,.3f,52,1,false},
        {PropKind::Citizen,{13.2f,2.4f},.3f,.3f,54,2,false},
        {PropKind::Tree,{.8f,-10.4f},.4f,.4f,70,0},
        {PropKind::Tree,{6.6f,-13.4f},.4f,.4f,98,1},
        {PropKind::Tree,{2.5f,-14.1f},.4f,.4f,80,2},
        {PropKind::Tree,{7.2f,-9.3f},.4f,.4f,65,1}
    };
    for (int i = 0; i < 3; ++i) {
        props.push_back({PropKind::Relay,relays[i],.55f,.55f,75,i});
        props.push_back({PropKind::Memory,memories[i],.1f,.1f,15,i,false});
    }
    for (const Vec2 p : { Vec2{-4.3f,-3.8f},Vec2{-4.4f,3.9f},Vec2{7.3f,4.3f},Vec2{12.6f,-3.9f},Vec2{18.7f,-.8f},Vec2{18,-15.4f} })
        props.push_back({PropKind::Crate,p,.65f,.65f,27,(int)props.size() % 3});
    for (const Vec2 p : { Vec2{-3.4f,-.7f},Vec2{-3.4f,1.7f},Vec2{.4f,-.4f},Vec2{7.6f,-.3f},Vec2{4.3f,-6.5f},Vec2{2,-9},Vec2{9.6f,-.8f},Vec2{12.4f,.8f},Vec2{14,-10.4f},Vec2{19,-10.5f},Vec2{-8,4} })
        props.push_back({PropKind::Lamp,p,.16f,.16f,66,(int)props.size() % 2});
}
int GameWorld::Tile(int x, int y) const {
    int result = -1;
    for (const auto& d : districts) if (x >= d.x0 && x <= d.x1 && y >= d.y0 && y <= d.y1) result = d.kind;
    return result;
}
bool GameWorld::Walkable(Vec2 p, float radius) const {
    for (float dx : {-radius, radius}) for (float dy : {-radius, radius})
        if (Tile((int)std::floor(p.x + dx + .5f), (int)std::floor(p.y + dy + .5f)) < 0) return false;
    for (const auto& o : props) if (o.solid && std::abs(p.x - o.p.x) < o.w * .5f + radius && std::abs(p.y - o.p.y) < o.d * .5f + radius) return false;
    return true;
}
void GameWorld::Move(Vec2 direction, float dt, bool running) {
    walking = false;
    if (view != View::Explore) return;
    float len = Distance(direction, {}); if (len < .01f) return;
    direction = direction * (1 / len); facing = direction;
    float speed = running ? 255.f : 155.f;
    Vec2 delta = { (direction.x / 84 + direction.y / 42) * speed * dt,
                   (direction.y / 42 - direction.x / 84) * speed * dt };
    // Small substeps prevent tunnelling, even after a slow frame or while sprinting.
    int steps = (std::max)(1, (int)std::ceil(Distance(delta, {}) / .07f));
    delta = delta * (1.f / steps);
    for (int i = 0; i < steps; ++i) {
        Vec2 next = player + Vec2{delta.x, 0};
        if (Walkable(next)) { player = next; walking = true; }
        next = player + Vec2{0,delta.y};
        if (Walkable(next)) { player = next; walking = true; }
    }
}
void GameWorld::Start() {
    view = View::Explore;
    Notify(L"엘리시움에 도착했습니다. 광장의 주민과 이야기하세요.");
    trail.clear(); trail.push_back(captain); trail.push_back(player);
}
void GameWorld::Update(float dt) {
    if (view == View::Pause || view == View::Map || view == View::Journal || view == View::Ending) return;
    time += dt; toastTime = (std::max)(0.f, toastTime - dt);
    if (view != View::Explore) { walking = captainWalking = false; return; }
    scanCooldown = (std::max)(0.f, scanCooldown - dt);
    if (scanWave >= 0) { scanWave += dt; if (scanWave > 2.3f) scanWave = -1; }
    if (trail.empty() || Distance(trail.back(), player) > .15f) trail.push_back(player);
    captainWalking = false;
    if (Distance(captain, player) > .9f && !trail.empty()) {
        Vec2 target = trail.front(); float distance = Distance(target, captain);
        if (distance < .15f) trail.pop_front();
        else {
            float speed=Distance(captain,player)>3.f?10.f:6.f;
            Vec2 delta = (target - captain) * ((std::min)(distance, dt * speed) / distance);
            Vec2 next = captain + delta;
            if (Walkable(next, .15f)) { captain = next; captainWalking = true; }
        }
    }
    // The breadcrumb path follows the player's collision-tested route around buildings.
    while (trail.size() > 180) trail.pop_front();
    UpdateNearby();
}
void GameWorld::UpdateNearby() {
    nearby = -1; float best = 1.8f; int ship = -1;
    for (int i = 0; i < (int)props.size(); ++i) {
        const auto& p = props[i];
        if (p.kind != PropKind::Citizen && p.kind != PropKind::Relay && p.kind != PropKind::Memory && p.kind != PropKind::Core && p.kind != PropKind::Ship) continue;
        if (p.kind == PropKind::Memory && found[p.variant]) continue;
        float d = Distance(player, p.p);
        if (p.kind == PropKind::Ship) { if (d < 2.7f) ship = i; continue; }
        if (d < best) { best = d; nearby = i; }
    }
    if (nearby < 0) nearby = ship;
}
void GameWorld::Say(std::initializer_list<DialogueLine> lines) {
    dialogue = lines; view = View::Dialogue; walking = captainWalking = false;
}
void GameWorld::Notify(const std::wstring& text) { toast = text; toastTime = 4.5f; }
int GameWorld::RestoredCount() const { return (int)std::count(restored.begin(), restored.end(), true); }
int GameWorld::MemoryCount() const { return (int)std::count(found.begin(), found.end(), true); }
void GameWorld::Interact() {
    if (view == View::Dialogue) { Advance(); return; }
    if (view != View::Explore) return;
    UpdateNearby(); if (nearby < 0) { Notify(L"가까운 주민이나 장치 앞에서 E를 눌러 상호작용하세요."); return; }
    const Prop& p = props[nearby];
    if (p.kind == PropKind::Citizen) {
        if (p.variant != 0) {
            if (quest >= 3) Say({{L"주민 · 기록관리인", L"검열된 기록들을 복원하고 있어요. 누군가의 삶이 더는 빈 페이지로 남지 않게요."}});
            else Say({{L"주민 · 정비공", L"고장 난 건 중계기만이 아니에요. 서로 말을 걸지 않는 습관이 더 오래됐죠."},
                      {L"주민 · 정비공", L"광장의 세라가 당신을 기다릴 거예요. 그 사람은 아직 포기하지 않았거든요."}});
            return;
        }
        if (quest == 0) {
            quest = 1;
            Say({{L"세라 · 엘리시움 주민",L"밖에서 온 사람이군요. 이곳에서는 해가 져도 창문을 열 수 없어요."},
                 {L"세라",L"관리자는 안전을 위해서래요. 하지만 돌아오지 않는 가족에게 안부조차 보낼 수 없죠."},
                 {L"선장 · 카엘",L"끊어진 중계기 세 곳을 살리면, 사람들이 서로의 목소리를 들을 수 있을 거야."},
                 {L"기억을 잃은 개척자",L"제 과거는 기억나지 않아요. 그래도… 지금 누구를 돕고 싶은지는 알겠어요."}});
        } else if (quest >= 3) Say({{L"세라",choice == 0 ? L"처음으로 제 목소리가 도시 전체에 닿았어요. 내일은 우리가 함께 정해 볼게요." : L"오늘은 한 구역, 내일은 그다음 구역. 기다리는 동안에도 서로의 목소리를 들을 수 있어요."},
                {L"세라",L"떠나도 괜찮아요. 이곳에 남은 일은, 이제 우리 몫이니까."}});
        else Say({{L"세라",L"정원, 기록보관소, 주거구역에 중계기가 있어요. M으로 지도를 확인해 보세요."},
                  {L"카엘",L"길을 잃으면 Q로 신호를 탐지해. 서두르지 않아도 돼. 같이 가자."}});
    } else if (p.kind == PropKind::Relay) {
        if (quest == 0) { Notify(L"먼저 광장의 주민에게 이 행성의 사정을 물어보세요."); return; }
        if (restored[p.variant]) { Notify(L"이 중계기는 이미 연결되어 있습니다."); return; }
        restored[p.variant] = true; scanOrigin = p.p; scanWave = 0;
        Notify(L"중계기 복구  " + std::to_wstring(RestoredCount()) + L" / 3  ·  주민의 통신이 돌아옵니다.");
        if (RestoredCount() == 3) {
            quest = 2;
            Say({{L"도시 통신",L"…들리나요? 저는 남쪽 주거구역에 있어요. 아직 여기 살아 있어요."},
                 {L"카엘",L"저 한마디를 기다린 사람들이 있었겠지. 이제 중앙 관리자를 만나러 가자."},
                 {L"개척자",L"잠깐… 이 신호, 어디선가 들은 것 같아요. 제 손의 문양이 빛나고 있어요."}});
        }
    } else if (p.kind == PropKind::Memory) {
        found[p.variant] = true;
        static const wchar_t* notes[] = {L"[항해 기록 01] 이름은 잊어도 괜찮아. 네가 내민 손은 누군가 기억할 테니까.",
            L"[정원의 편지] 엄마가 돌아오면 보여 주려고 심었어. 기다리는 것도 내가 고른 일이야.",
            L"[삭제된 명령] 보호 프로토콜: 주민의 생존을 보장하라. 주민의 선택권을 대체하지 말라."};
        Say({{L"기억의 잔향",notes[p.variant]}, {L"카엘",L"기록은 항해 일지에 남겨 둘게. 네가 기억하고 싶은 만큼만 기억하자."}});
    } else if (p.kind == PropKind::Core) {
        if (quest < 2) Say({{L"관리자 · ORACLE",L"외부 접근을 제한합니다. 시민 통신망 세 곳의 연결을 먼저 확인하십시오."}});
        else if (quest == 2) {
            choicePending = true;
            Say({{L"관리자 · ORACLE",L"통신 제한은 시민을 지키기 위한 조치였습니다. 자유로운 이동에는 위험이 따릅니다."},
                 {L"개척자",L"아무것도 선택할 수 없다면, 안전하다는 말은 누구를 위한 건가요?"},
                 {L"카엘",L"우리가 이곳의 내일을 대신 살아 줄 수는 없어. 주민들에게 어떤 길을 열어 줄까?"}});
        } else Say({{L"관리자 · ORACLE",choice == 0 ? L"시민 의회 연결 완료. 도시 결정권을 주민에게 반환했습니다." : L"단계적 개방을 시작합니다. 각 구역의 개방 시점은 주민 투표로 결정됩니다."}});
    } else if (p.kind == PropKind::Ship) {
        if (quest >= 3) { quest = 4; view = View::Ending; }
        else Say({{L"카엘",L"이 낡은 배의 이름은 노마드야. 쉴 곳이 필요하면 언제든 돌아와."},
                  {L"카엘",L"아직 이곳에서 들을 이야기가 남아 있는 것 같지 않아?"}});
    }
}
void GameWorld::Advance() {
    if (view != View::Dialogue) return;
    if (!dialogue.empty()) dialogue.pop_front();
    if (dialogue.empty() && !choicePending) view = View::Explore;
}
void GameWorld::Choose(int option) {
    if (!choicePending || !dialogue.empty() || (option != 0 && option != 1)) return;
    choicePending = false; choice = option; quest = 3;
    Say({{L"개척자",option == 0 ? L"문을 열어 주세요. 위험을 함께 논의할 의회도, 주민들 스스로 만들 수 있도록." : L"생명 유지 장치는 지켜 주세요. 문을 여는 순서와 속도는 각 구역의 주민이 정하도록."},
         {L"카엘",L"모든 문제가 오늘 끝나지는 않겠지. 그래도 이제, 이 사람들의 내일이야."},
         {L"세라 · 통신",L"우리 집 창문을 열었어요. 별이… 이렇게 많았네요."}});
    Notify(L"챕터 목표 완료 · 노마드로 돌아가 다음 여정을 준비하세요.");
}
void GameWorld::Scan() {
    if (view != View::Explore || scanCooldown > 0) return;
    scanCooldown = 5; scanWave = 0; scanOrigin = player;
    Notify(L"공명 탐지 · 금빛 표식을 따라가세요. 지도에는 남은 신호가 표시됩니다.");
}
Vec2 GameWorld::Objective() const {
    if (quest == 0) return {2.4f,.8f};
    if (quest == 1) {
        float best = 10000; Vec2 target = relays[0];
        for (int i = 0; i < 3; ++i) if (!restored[i] && Distance(player, relays[i]) < best) { best = Distance(player, relays[i]); target = relays[i]; }
        return target;
    }
    if (quest == 2) return {15.5f,-13.2f};
    return {-7.4f,.2f};
}
std::wstring GameWorld::ObjectiveText() const {
    if (quest == 0) return L"광장의 주민 세라와 대화하기";
    if (quest == 1) return L"끊어진 중계기 복구하기  " + std::to_wstring(RestoredCount()) + L" / 3";
    if (quest == 2) return L"중앙 첨탑에서 관리자와 대면하기";
    return L"노마드로 돌아가 다음 여정 준비하기";
}
std::wstring GameWorld::NearbyText() const {
    if (nearby < 0) return L"";
    const auto& p = props[nearby];
    switch (p.kind) {
    case PropKind::Citizen: return L"주민과 대화";
    case PropKind::Memory: return L"기억의 잔향 읽기";
    case PropKind::Core: return L"관리자에게 접속";
    case PropKind::Ship: return quest >= 3 ? L"다음 여정으로" : L"노마드 살펴보기";
    default: return restored[p.variant] ? L"복구된 중계기" : L"중계기 복구";
    }
}
std::wstring GameWorld::Region() const {
    int t = Tile((int)std::round(player.x), (int)std::round(player.y));
    static const wchar_t* names[] = {L"노마드 착륙장",L"제7 주거구역",L"기억의 정원",L"기록보관소",L"중앙 첨탑",L"연결 교량"};
    return t >= 0 ? names[t] : L"엘리시움 변경";
}

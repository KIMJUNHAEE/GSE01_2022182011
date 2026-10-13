"use strict";
const $ = (id) => document.getElementById(id);
let current = null;
let session = null;
const labels = {
    title: "타이틀", explore: "탐사 중", dialogue: "대화 중", map: "지도", journal: "일지",
    pause: "일시정지", ending: "엔딩", defeat: "패배", civilian: "민간 / 선량한 마을",
    pirate: "해적 마을", neutral: "중립", unknown: "미분류", crisis: "위기", threatened: "경계",
    recovering: "회복 중", quiet: "조용함", companion: "동료", resident: "주민", boss: "보스", mob: "몬스터",
    following: "동행 이동", idle: "대기", talking: "대화", inactive: "비활성", telegraphing: "공격 예고",
    moving: "이동", info: "정보", warning: "주의", danger: "위험",
    murder_rumor: "살인 소문", theft_rumor: "절도 소문", disturbance: "소란", observation: "관측",
    world_created: "월드 생성", exploration_started: "탐사 시작", npc_dialogue: "NPC 대화",
    relay_restored: "중계기 복구", memory_found: "기억 발견", level_up: "레벨 상승",
    player_defeated: "플레이어 패배", enemy_defeated: "적 처치", boss_started: "보스 교전",
    boss_defeated: "보스 격파", story_choice: "이야기 선택"
};
const label = (value) => labels[value] || value;
const fmt = (value, places = 0) => typeof value === "number" && Number.isFinite(value) ? value.toFixed(places) : "—";
const node = (tag, content, className) => {
    const result = document.createElement(tag);
    if (content !== undefined) result.textContent = content;
    if (className) result.className = className;
    return result;
};
const villageName = (id) => current?.snapshot?.villages.find((v) => v.id === id)?.name || "구역 밖";
const badge = (content, kind = "") => node("span", content, "badge " + kind);

function setConnection(message, kind, detail) {
    $("connection").textContent = message;
    $("connection").className = "badge " + kind;
    $("received").textContent = detail;
}

function renderVillages(world) {
    $("villages").replaceChildren();
    world.villages.forEach((v) => {
        const card = node("article", undefined, "village " + v.mood);
        card.append(node("h3", v.name), node("p", label(v.alignment) + " · 설정값", "alignment"),
            badge(label(v.mood) + " · 추정", v.mood === "crisis" ? "danger" : v.mood === "threatened" ? "warning" : ""),
            node("p", "NPC " + v.npcCount + "　/　몹 " + v.mobCount),
            node("p", v.relayRestored === null ? "중계기 대상 아님" : v.relayRestored ? "통신 복구됨" : "통신 미복구", "muted"));
        $("villages").append(card);
    });
    const selected = $("village-filter").value;
    $("village-filter").replaceChildren(new Option("전체", "all"), new Option("구역 밖", "-1"));
    world.villages.forEach((v) => $("village-filter").add(new Option(v.name, String(v.id))));
    if ([...$("village-filter").options].some((option) => option.value === selected)) $("village-filter").value = selected;
}

function renderEntities() {
    const world = current?.snapshot;
    if (!world) return;
    const group = $("entity-kind").value;
    const items = group === "all" ? [...world.npcs, ...world.mobs] : world[group];
    const query = $("search").value.toLocaleLowerCase();
    const village = $("village-filter").value;
    const rows = items.filter((e) =>
        (e.name.toLocaleLowerCase().includes(query) || String(e.id).includes(query)) &&
        (village === "all" || String(e.villageId) === village));
    $("entities").replaceChildren();
    rows.forEach((entity) => {
        const row = node("tr");
        const name = node("td");
        name.append(node("strong", entity.name), node("small", "#" + entity.id));
        const health = entity.health === null || entity.health === undefined ? "미구현" :
            fmt(entity.health) + " / " + fmt(entity.maxHealth);
        row.append(name, node("td", label(entity.role)), node("td", label(entity.state)),
            node("td", villageName(entity.villageId)), node("td", health),
            node("td", fmt(entity.position.x, 1) + ", " + fmt(entity.position.y, 1)),
            node("td", (entity.visible ? "보임" : "숨김") + " / " + (entity.active ? "활성" : "비활성")));
        $("entities").append(row);
    });
    if (!rows.length) {
        const cell = node("td", "표시할 개체가 없습니다.", "empty");
        cell.colSpan = 7;
        const row = node("tr");
        row.append(cell);
        $("entities").append(row);
    }
}

function allEvents() {
    return [...(current?.snapshot?.events || []), ...(current?.externalEvents || [])]
        .reverse().sort((a, b) => b.gameTime - a.gameTime);
}

function renderEvents() {
    const severity = $("severity-filter").value, source = $("source-filter").value;
    const events = allEvents().filter((e) =>
        (severity === "all" || e.severity === severity) && (source === "all" || e.source === source));
    $("events").replaceChildren();
    events.forEach((event) => {
        const item = node("article", undefined, "event");
        const body = node("div"), meta = node("div", undefined, "meta");
        meta.append(badge(label(event.severity), event.severity),
            node("strong", label(event.kind)),
            node("span", event.source === "game" ? "게임 기록" : "외부 제보"),
            node("span", event.confirmed ? "확인된 사건" : "미확인"),
            node("span", villageName(event.villageId)));
        body.append(meta, node("p", event.message));
        item.append(node("span", fmt(event.gameTime, 1) + " s", "time"), body);
        $("events").append(item);
    });
    if (!events.length) $("events").append(node("p", "선택한 조건에 맞는 사건이 없습니다.", "empty"));
}

function svgNode(tag, attributes) {
    const element = document.createElementNS("http://www.w3.org/2000/svg", tag);
    for (const [key, value] of Object.entries(attributes)) element.setAttribute(key, String(value));
    return element;
}

function renderMap(world) {
    const map = $("world-map");
    map.replaceChildren();
    const bounds = world.villages.map((v) => v.bounds).filter(Boolean);
    if (bounds.length) {
        const x = Math.min(...bounds.map((b) => b.x0)) - 1;
        const y = Math.min(...bounds.map((b) => b.y0)) - 1;
        const width = Math.max(...bounds.map((b) => b.x1)) - x + 1;
        const height = Math.max(...bounds.map((b) => b.y1)) - y + 1;
        map.setAttribute("viewBox", [x, y, width, height].join(" "));
    }
    world.villages.forEach((v) => {
        if (!v.bounds) return;
        const b = v.bounds;
        map.append(svgNode("rect", {x: b.x0, y: b.y0, width: b.x1-b.x0, height: b.y1-b.y0, rx: .2}));
        const text = svgNode("text", {x: b.x0 + .3, y: b.y0 + .7});
        text.textContent = v.name;
        map.append(text);
    });
    function point(entity, kind, radius) {
        const dot = svgNode("circle", {cx: entity.position.x, cy: entity.position.y, r: radius,
            class: kind + (entity.active ? "" : " inactive")});
        const title = svgNode("title", {});
        title.textContent = (entity.name || "플레이어") + (entity.state ? " / " + label(entity.state) : "");
        dot.append(title);
        map.append(dot);
    }
    world.mobs.forEach((entity) => point(entity, "mob", .17));
    world.npcs.forEach((entity) => point(entity, "npc", .22));
    point(world.player, "player", .28);
}

function renderProgress(world) {
    $("objective").textContent = world.progress.objective;
    const values = [
        ["게임 화면", label(world.view)], ["현재 지역", world.progress.region],
        ["중계기 / 기억", world.progress.restoredRelays + " / 3 · " + world.progress.memoriesFound + " / 3"],
        ["보스", world.progress.bossDefeated ? "격파" : world.progress.bossActive ? "교전 중" : "대기"],
        ["전체 액터", world.counts.actors], ["탄환 / 전리품", world.counts.projectiles + " / " + world.counts.loot],
        ["구역 밖 NPC / 몹", world.counts.unassignedNpcs + " / " + world.counts.unassignedMobs],
        ["처치 / 습득", world.player.kills + " / " + world.player.itemsCollected],
        ["무기 강화", "+" + world.player.weaponTier], ["평균 인스턴스 / 드로우콜", fmt(world.performance.averageInstancesPerDraw, 1)]
    ];
    $("progress").replaceChildren();
    values.forEach(([key, value]) => $("progress").append(node("dt", key), node("dd", value)));
    const samples = current.history || [];
    const peak = Math.max(60, ...samples.map((s) => s.fps || 0));
    const points = samples.map((s, index) => s.fps === null ? null :
        [(index / Math.max(samples.length - 1, 1)) * 390 + 5, 85 - s.fps / peak * 75]);
    $("fps-chart").replaceChildren();
    const valid = points.filter(Boolean);
    if (valid.length) $("fps-chart").append(svgNode("polyline", {points: valid.map((p) => p.join(",")).join(" ")}));
}

function render(data) {
    current = data;
    const world = data.snapshot;
    if (!world) {
        session = null;
        ["npc-count", "mob-count", "fps", "draw-calls", "level", "event-count"].forEach((id) => $(id).textContent = "—");
        ["npc-total", "mob-total", "last-draw", "health", "events-total", "world-meta", "objective"].forEach((id) => $(id).textContent = "수신 대기");
        ["world-map", "fps-chart", "progress", "entities"].forEach((id) => $(id).replaceChildren());
        $("villages").replaceChildren(node("p", "지역 데이터가 아직 없습니다.", "empty"));
        $("events").replaceChildren(node("p", "기록된 사건이 없습니다.", "empty"));
        $("raw-json").textContent = JSON.stringify(data, null, 2);
        $("notice").className = "notice";
        setConnection("수신 대기", "", "수신 서버 연결됨 · 게임을 실행하세요.");
        $("notice").textContent = "아직 게임에서 받은 JSON이 없습니다. 게임과 수신기의 포트를 확인하세요.";
        return;
    }
    if (session !== world.sessionId) {
        session = world.sessionId;
        $("send-result").textContent = "새 세션입니다. 외부 사건 전송 전 sessionId를 확인하세요.";
    }
    setConnection(data.stale ? "게임 수신 끊김" : "실시간 수신", data.stale ? "stale" : "live",
        "최근 수신 " + new Date(data.receivedAt).toLocaleTimeString("ko-KR") + " · " + fmt(data.ageSeconds, 1) + "초 전");
    $("notice").className = data.stale ? "notice stale" : "notice";
    $("notice").textContent = data.stale ? "3초 이상 새 메시지가 없습니다. 아래는 마지막 수신 상태이며 현재 상태가 아닐 수 있습니다." :
        "게임: " + label(world.view) + " · 게임 시간 " + fmt(world.gameTime, 1) + "s · JSON #" + world.sequence +
        (world.view === "explore" ? "" : " · 화면 전환/일시정지 중에도 연결 상태는 갱신됩니다.");
    $("npc-count").textContent = world.counts.npcs;
    $("npc-total").textContent = "비활성 포함 " + world.counts.npcsIncludingInactive + "명";
    $("mob-count").textContent = world.counts.mobs;
    $("mob-total").textContent = "비활성 포함 " + world.counts.mobsIncludingInactive + "개체";
    $("fps").textContent = fmt(world.performance.fps, 1);
    $("draw-calls").textContent = fmt(world.performance.averageDrawCalls, 1);
    $("last-draw").textContent = "직전 프레임 " + world.performance.lastDrawCalls + " calls";
    $("level").textContent = "Lv." + world.player.level;
    $("health").textContent = "HP " + fmt(world.player.health) + " / " + fmt(world.player.maxHealth);
    $("event-count").textContent = allEvents().length;
    $("events-total").textContent = "게임 누적 " + world.eventsTotal + "건 · 외부 " + data.externalEvents.length + "건";
    $("world-meta").textContent = "SEED " + world.mapSeed + " · " + world.villages.length + "개 구역";
    renderVillages(world);
    renderEntities();
    renderEvents();
    renderMap(world);
    renderProgress(world);
    $("raw-json").textContent = JSON.stringify(data, null, 2);
}

async function poll() {
    try {
        const response = await fetch("/api/state", {cache: "no-store", signal: AbortSignal.timeout(2500)});
        if (!response.ok) throw new Error("HTTP " + response.status);
        render(await response.json());
    } catch (error) {
        setConnection("수신 서버 연결 끊김", "stale", error.message);
        $("notice").className = "notice stale";
        $("notice").textContent = "수신 서버에 연결할 수 없습니다. Monitor.cmd 실행 상태를 확인하세요. 기존 표시는 마지막 수신 데이터입니다.";
    } finally {
        setTimeout(poll, 500); // no overlapping fetches or unbounded reconnect queues
    }
}

["search", "entity-kind", "village-filter"].forEach((id) => $(id).addEventListener("input", renderEntities));
["severity-filter", "source-filter"].forEach((id) => $(id).addEventListener("change", renderEvents));
$("fill-example").addEventListener("click", () => {
    if (!current?.snapshot) {
        $("send-result").textContent = "게임 JSON을 먼저 수신해야 합니다.";
        return;
    }
    $("event-json").value = JSON.stringify({
        schemaVersion: 1, type: "world_event", sessionId: current.snapshot.sessionId,
        kind: "murder_rumor", severity: "warning",
        villageId: current.snapshot.villages[0]?.id ?? -1,
        message: "[수동 테스트] 살인 소문 신고 예시 — 확인되지 않은 제보"
    }, null, 2);
    $("send-result").textContent = "예시를 채웠습니다. 아직 전송하지 않았습니다.";
});
$("send-event").addEventListener("click", async () => {
    $("send-event").disabled = true;
    try {
        const data = JSON.parse($("event-json").value);
        const response = await fetch("/api/events", {method: "POST", headers: {"Content-Type": "application/json"},
            body: JSON.stringify(data), signal: AbortSignal.timeout(3000)});
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || "HTTP " + response.status);
        $("send-result").textContent = "미확인 제보로 수신했습니다. 게임 상태는 변경하지 않았습니다.";
    } catch (error) {
        $("send-result").textContent = "수신 실패: " + error.message;
    } finally {
        $("send-event").disabled = false;
    }
});
poll();

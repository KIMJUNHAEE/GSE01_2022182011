"use strict";
const canvas = document.getElementById("preview");
const ctx = canvas.getContext("2d");
const $ = (id) => document.getElementById(id);
const sheet = new Image();
const idleSheet = new Image();
let frames = [], moving = true, flipped = false, manualFrame = null;
let phase = 0, previousTime = null;
let strideLength = 144;

function setWalking(value) {
    moving = value;
    manualFrame = null;
    $("walk").classList.toggle("active", value);
    $("idle").classList.toggle("active", !value);
    $("walk").setAttribute("aria-pressed", String(value));
    $("idle").setAttribute("aria-pressed", String(!value));
    $("frame-label").textContent = "자동";
}
function setFlipped(value) {
    flipped = value;
    $("flip").setAttribute("aria-pressed", String(value));
}
$("walk").addEventListener("click", () => setWalking(true));
$("idle").addEventListener("click", () => setWalking(false));
$("flip").addEventListener("click", () => setFlipped(!flipped));
$("frame").addEventListener("input", () => {
    manualFrame = Number($("frame").value);
    $("frame-label").textContent = String(manualFrame);
});
$("speed").addEventListener("input", () => $("speed-label").textContent = Number($("speed").value).toFixed(1) + "×");
document.addEventListener("keydown", (event) => {
    if (["INPUT", "BUTTON"].includes(event.target.tagName)) return;
    if (event.code === "Space") { event.preventDefault(); setWalking(!moving); }
    if (event.code === "KeyA") setFlipped(true);
    if (event.code === "KeyD") setFlipped(false);
});

function loadImage(image, path) {
    return new Promise((resolve, reject) => {
        image.onload = () => resolve(image);
        image.onerror = () => reject(new Error(path + " 읽기 실패"));
        image.src = path + "?v=aseprite-rig-1";
    });
}

async function prepare() {
    try {
        const [,,response] = await Promise.all([
            loadImage(idleSheet, "Textures/protagonist_idle_aseprite.png"),
            loadImage(sheet, "Textures/protagonist_walk_aseprite.png"),
            fetch("Art/Protagonist/protagonist_rig.json", {cache: "no-store"})
        ]);
        if (!response.ok) throw new Error("Aseprite 동작 정보 읽기 실패");
        const rig = await response.json();
        if (rig.frameCount !== 32 || sheet.width !== rig.width * rig.columns ||
            sheet.height !== rig.height * rig.rows || idleSheet.width !== rig.width ||
            idleSheet.height !== rig.height) throw new Error("시트와 동작 정보가 맞지 않습니다.");
        strideLength = rig.strideLength;
        for (let index = 0; index <= rig.frameCount; ++index) {
            const frame = Math.max(0, index - 1);
            frames.push({image: index === 0 ? idleSheet : sheet,
                x: index === 0 ? 0 : frame % rig.columns * rig.width,
                y: index === 0 ? 0 : Math.floor(frame / rig.columns) * rig.height,
                w: rig.width, h: rig.height, bodyHeight: rig.bodyHeight,
                anchorX: rig.anchorX, anchorY: rig.anchorY});
        }
        $("status").textContent = "Aseprite · 대기 1 + 걷기 32프레임 준비됨";
        requestAnimationFrame(animate);
    } catch (error) {
        $("status").textContent = "읽기 실패: " + error.message + " · SpritePreview.cmd를 실행한 뒤 안내 주소로 열어주세요.";
    }
}
prepare();

function animate(time) {
    const dt = previousTime === null ? 0 : Math.min((time-previousTime)/1000, .05);
    previousTime = time;
    if (moving && manualFrame === null) phase = (phase + dt * 155 * Number($("speed").value) / strideLength) % 1;
    const index = manualFrame ?? (moving ? 1 + Math.floor(phase*32) : 0);
    const frame = frames[index];
    const scale = 300 / frame.bodyHeight, footX = 240, footY = 432;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.strokeStyle = "#eac36866"; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(48, footY+.5); ctx.lineTo(432, footY+.5); ctx.stroke();
    ctx.fillStyle = "#00000055";
    ctx.beginPath(); ctx.ellipse(footX, footY+3, 48, 13, 0, 0, Math.PI*2); ctx.fill();
    ctx.save(); ctx.translate(footX, footY); ctx.scale(flipped ? -1 : 1, 1);
    ctx.drawImage(frame.image, frame.x, frame.y, frame.w, frame.h,
        -frame.anchorX*scale, -frame.anchorY*scale, frame.w*scale, frame.h*scale);
    ctx.restore();
    $("frame").value = String(index);
    $("status").textContent = "프레임 " + index + " / 32 · " + (flipped ? "왼쪽" : "오른쪽") + " · 높이 300px / 발 기준 고정";
    requestAnimationFrame(animate);
}

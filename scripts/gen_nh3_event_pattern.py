"""
NH₃ Event Pattern Simulation Chart
배뇨/배변 이벤트 발생 시 NH₃ 농도 시계열 패턴 시뮬레이션.
Phase 6 감지 알고리즘 설계 참고용.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from pathlib import Path

rng = np.random.default_rng(42)

# ── 시뮬레이션 파라미터 ─────────────────────────────────
DT = 10          # 샘플 간격 (초, 펌웨어 10s 주기와 일치)
DURATION = 25    # 전체 시뮬레이션 시간 (분)
t_sec = np.arange(0, DURATION * 60, DT)
t_min = t_sec / 60.0

def noise(n, scale=0.25):
    return rng.normal(0, scale, n)

# ── 기저값 ─────────────────────────────────────────────
BASELINE = 3.0
ppm = np.full(len(t_sec), BASELINE) + noise(len(t_sec))

# ── 이벤트 1: 배뇨 (t=5min, 급격한 스파이크) ────────────
def apply_urination(ppm, t_min, start_min, peak=13.0, rise=0.5, decay_tau=3.5):
    for i, t in enumerate(t_min):
        elapsed = t - start_min
        if elapsed < 0:
            continue
        if elapsed < rise:
            ppm[i] += peak * (elapsed / rise)
        else:
            ppm[i] += peak * np.exp(-(elapsed - rise) / decay_tau)

# ── 이벤트 2: 배변 (t=14min, 완만한 상승/하강) ──────────
def apply_defecation(ppm, t_min, start_min, peak=5.0, rise=2.5, decay_tau=5.0):
    for i, t in enumerate(t_min):
        elapsed = t - start_min
        if elapsed < 0:
            continue
        if elapsed < rise:
            ppm[i] += peak * (elapsed / rise) ** 0.7
        else:
            ppm[i] += peak * np.exp(-(elapsed - rise) / decay_tau)

apply_urination(ppm, t_min, start_min=5.0)
apply_defecation(ppm, t_min, start_min=14.0)
ppm = np.clip(ppm, 0, None)

# ── 감지 임계값 ────────────────────────────────────────
THRESHOLD_DELTA = 5.0   # 기저값 대비 +5 ppm 이상이면 이벤트 트리거

# ── 스타일 ─────────────────────────────────────────────
plt.rcParams.update({
    "figure.facecolor": "#1e1e2e",
    "axes.facecolor":   "#1e1e2e",
    "axes.edgecolor":   "#585b70",
    "axes.labelcolor":  "#cdd6f4",
    "xtick.color":      "#cdd6f4",
    "ytick.color":      "#cdd6f4",
    "grid.color":       "#313244",
    "grid.linestyle":   "--",
    "grid.linewidth":   0.6,
    "text.color":       "#cdd6f4",
    "font.family":      ["Malgun Gothic", "DejaVu Sans"],
})

fig, ax = plt.subplots(figsize=(10, 5))
fig.patch.set_facecolor("#1e1e2e")

# ── 배경 영역 ──────────────────────────────────────────
# 배뇨 이벤트 영역
ax.axvspan(5.0, 10.5, alpha=0.08, color="#89b4fa", zorder=1)
ax.text(5.15, 16.5, "배뇨 이벤트", color="#89b4fa", fontsize=8.5, va="top")

# 배변 이벤트 영역
ax.axvspan(14.0, 21.0, alpha=0.08, color="#a6e3a1", zorder=1)
ax.text(14.15, 16.5, "배변 이벤트", color="#a6e3a1", fontsize=8.5, va="top")

# ── 기저값 ─────────────────────────────────────────────
ax.axhline(BASELINE, color="#585b70", linewidth=0.9,
           linestyle=":", alpha=0.7, label=f"기저값 (≈{BASELINE} ppm)")

# ── 감지 임계선 ────────────────────────────────────────
threshold_line = BASELINE + THRESHOLD_DELTA
ax.axhline(threshold_line, color="#f9e2af", linewidth=1.2,
           linestyle="--", alpha=0.8,
           label=f"이벤트 감지 임계값 (기저값 +{THRESHOLD_DELTA} ppm = {threshold_line} ppm)")

# ── 센서 데이터 (scatter: 실제 10s 샘플) ─────────────────
ax.scatter(t_min, ppm, s=18, color="#cba6f7", zorder=4, alpha=0.7,
           label="센서 측정값 (10초 간격)")

# ── 곡선 (연결선) ──────────────────────────────────────
ax.plot(t_min, ppm, color="#cba6f7", linewidth=1.0, alpha=0.4, zorder=3)

# ── 주석: 배뇨 피크 ────────────────────────────────────
urination_peak_idx = np.argmax(ppm[:int(12*60/DT)])
ax.annotate(
    f"피크 {ppm[urination_peak_idx]:.1f} ppm\n(급격한 상승 → 빠른 감쇠)",
    xy=(t_min[urination_peak_idx], ppm[urination_peak_idx]),
    xytext=(1.5, 2.5), textcoords="offset points",
    xycoords="data",
    fontsize=7.5, color="#89b4fa", va="bottom", ha="left",
)
# arrow
ax.annotate("",
    xy=(t_min[urination_peak_idx], ppm[urination_peak_idx]),
    xytext=(t_min[urination_peak_idx] - 1.2, ppm[urination_peak_idx] + 1.8),
    arrowprops=dict(arrowstyle="->", color="#89b4fa", lw=1.0),
)
ax.text(t_min[urination_peak_idx] - 1.2, ppm[urination_peak_idx] + 1.9,
        f"피크 {ppm[urination_peak_idx]:.1f} ppm\n(급격한 상승 → 빠른 감쇠)",
        fontsize=7.5, color="#89b4fa", va="bottom", ha="right")

# ── 주석: 배변 피크 ────────────────────────────────────
search_start = int(14*60/DT)
defecation_peak_idx = search_start + np.argmax(ppm[search_start:])
ax.annotate("",
    xy=(t_min[defecation_peak_idx], ppm[defecation_peak_idx]),
    xytext=(t_min[defecation_peak_idx] + 0.8, ppm[defecation_peak_idx] + 1.5),
    arrowprops=dict(arrowstyle="->", color="#a6e3a1", lw=1.0),
)
ax.text(t_min[defecation_peak_idx] + 0.9, ppm[defecation_peak_idx] + 1.6,
        f"피크 {ppm[defecation_peak_idx]:.1f} ppm\n(완만한 상승 → 느린 감쇠)",
        fontsize=7.5, color="#a6e3a1", va="bottom", ha="left")

# ── 임계선 레이블 ──────────────────────────────────────
ax.text(0.3, threshold_line + 0.3,
        f"감지 임계값 ({threshold_line:.0f} ppm)",
        color="#f9e2af", fontsize=7.5, va="bottom")

# ── 축 설정 ────────────────────────────────────────────
ax.set_xlim(0, DURATION)
ax.set_ylim(-0.5, 18)
ax.set_xlabel("시간 (분)", fontsize=10, labelpad=8)
ax.set_ylabel("NH₃ 농도 (ppm)", fontsize=10, labelpad=8)
ax.set_title("NH₃ 이벤트 패턴 시뮬레이션  (배뇨 vs 배변)", fontsize=12,
             fontweight="bold", color="#cdd6f4", pad=12)

ax.xaxis.set_major_locator(plt.MultipleLocator(2))
ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
ax.grid(True, which="both", zorder=0)
ax.grid(True, which="minor", alpha=0.25)

ax.legend(loc="upper right", fontsize=7.5,
          facecolor="#313244", edgecolor="#585b70", labelcolor="#cdd6f4")

# ── 저장 ──────────────────────────────────────────────
out_dir = Path(__file__).parent.parent / "docs" / "images"
out_dir.mkdir(parents=True, exist_ok=True)
out_path = out_dir / "nh3_event_pattern.png"

fig.tight_layout()
fig.savefig(out_path, dpi=150, bbox_inches="tight",
            facecolor=fig.get_facecolor())
print(f"Saved: {out_path}")
plt.close(fig)

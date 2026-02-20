"""
NH₃ Concentration Context Chart
고양이 화장실 상황별 예상 NH₃ 농도 범위를 수평 바 차트로 시각화.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from pathlib import Path

# ── 상황별 데이터 (상황, min_ppm, max_ppm) ─────────────
situations = [
    ("다묘 / 환기 불량",         50,  120),
    ("며칠 미청소",              15,   55),
    ("24시간 후 (1마리)",        10,   15),
    ("배뇨 직후 (덮개 없음)",     5,    9),
    ("화장실 주변 공기",          3,    4),
    ("깨끗한 모래 (기저값)",      0,    2),
]

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

fig, ax = plt.subplots(figsize=(9, 4.5))
fig.patch.set_facecolor("#1e1e2e")

# ── 색상 (농도 높을수록 빨간색) ─────────────────────────
bar_colors = [
    "#f38ba8",  # 다묘/환기 불량 - red
    "#fab387",  # 며칠 미청소   - orange
    "#f9e2af",  # 24시간 후     - yellow
    "#a6e3a1",  # 배뇨 직후     - green
    "#94e2d5",  # 화장실 주변   - teal
    "#89dceb",  # 깨끗한 모래   - sky
]

y_positions = range(len(situations))

for i, ((label, lo, hi), color) in enumerate(zip(situations, bar_colors)):
    width = max(hi - lo, 1)  # 최소 폭 1
    ax.barh(i, width, left=lo, height=0.55,
            color=color, alpha=0.85, zorder=3)
    # ppm 범위 텍스트
    right_x = hi + 1.5
    ax.text(right_x, i, f"{lo}~{hi} ppm",
            va="center", ha="left", fontsize=8.5, color="#cdd6f4")

ax.set_yticks(list(y_positions))
ax.set_yticklabels([s[0] for s in situations], fontsize=9)

# ── MQ-135 감지 하한선 (10 ppm) ──────────────────────
ax.axvline(10, color="#cba6f7", linewidth=1.4, linestyle="--", zorder=4)
ax.text(10.5, len(situations) - 0.3,
        "MQ-135 감지 하한\n(10 ppm)",
        color="#cba6f7", fontsize=7.5, va="top")

# ── 배뇨 이벤트 감지 타겟 영역 강조 ──────────────────
ax.axvspan(5, 15, alpha=0.06, color="#a6e3a1", zorder=2)
ax.text(5.3, -0.55, "배뇨 이벤트\n감지 타겟",
        color="#a6e3a1", fontsize=7, va="top", ha="left")

# ── 축 ────────────────────────────────────────────────
ax.set_xlim(-1, 135)
ax.set_xlabel("NH₃ 농도 (ppm)", fontsize=10, labelpad=8)
ax.set_title("고양이 화장실 상황별 예상 NH₃ 농도", fontsize=12,
             fontweight="bold", color="#cdd6f4", pad=12)
ax.grid(True, axis="x", zorder=1)
ax.set_axisbelow(True)

# ── 범례 ──────────────────────────────────────────────
mq_patch = mpatches.Patch(color="#cba6f7", label="MQ-135 유효 범위 (10~300 ppm)")
ax.legend(handles=[mq_patch], loc="lower right", fontsize=8,
          facecolor="#313244", edgecolor="#585b70", labelcolor="#cdd6f4")

# ── 저장 ──────────────────────────────────────────────
out_dir = Path(__file__).parent.parent / "docs" / "images"
out_dir.mkdir(parents=True, exist_ok=True)
out_path = out_dir / "nh3_concentration_context.png"

fig.tight_layout()
fig.savefig(out_path, dpi=150, bbox_inches="tight",
            facecolor=fig.get_facecolor())
print(f"Saved: {out_path}")
plt.close(fig)

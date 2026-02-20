"""
MQ-135 NH₃ Sensitivity Curve Generator
Generates Rs/Ro vs ppm graph (log-log scale) and saves as PNG.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from pathlib import Path

# ── 계수 (채택값) ──────────────────────────────────────
A = 102.2
B = -2.473
CLEAN_AIR_RATIO = 3.6  # Rs/Ro in clean air

# ── 참고 데이터포인트 (데이터시트 기반) ──────────────────
REF_PPM  = [10,   20,   50,   100,  200,  300]
REF_RSRO = [2.20, 1.60, 1.10, 1.00, 0.65, 0.52]

# ── 곡선 계산 (ppm = A * (Rs/Ro)^B  →  Rs/Ro = (ppm/A)^(1/B)) ──
ppm_curve = np.logspace(np.log10(5), np.log10(1000), 400)
rsro_curve = (ppm_curve / A) ** (1.0 / B)

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

fig, ax = plt.subplots(figsize=(8, 5.5))
fig.patch.set_facecolor("#1e1e2e")

# ── 유효 범위 음영 (10~300 ppm) ────────────────────────
valid_mask = (ppm_curve >= 10) & (ppm_curve <= 300)
ax.fill_between(ppm_curve[valid_mask], rsro_curve[valid_mask],
                0.1, alpha=0.12, color="#89b4fa", label="_nolegend_")

# ── Clean Air 수평선 ───────────────────────────────────
ax.axhline(CLEAN_AIR_RATIO, color="#a6e3a1", linewidth=1.0,
           linestyle=":", alpha=0.7)
ax.text(6, CLEAN_AIR_RATIO * 1.06, f"Clean Air  Rs/Ro = {CLEAN_AIR_RATIO}",
        color="#a6e3a1", fontsize=8, va="bottom")

# ── R₀ 기준선 (Rs/Ro = 1.0, 100 ppm) ─────────────────
ax.axhline(1.0, color="#f38ba8", linewidth=0.8, linestyle="--", alpha=0.6)
ax.axvline(100, color="#f38ba8", linewidth=0.8, linestyle="--", alpha=0.6)
ax.text(105, 1.08, "R₀ 기준점\n(100 ppm, Rs/Ro=1.0)",
        color="#f38ba8", fontsize=7.5, va="bottom")

# ── 주 곡선 ────────────────────────────────────────────
ax.plot(ppm_curve, rsro_curve, color="#89b4fa", linewidth=2.2,
        label=f"NH₃: ppm = {A} × (Rs/Ro)^{B}")

# ── 참고 데이터포인트 ──────────────────────────────────
ax.scatter(REF_PPM, REF_RSRO, color="#cba6f7", s=55, zorder=5,
           label="참고 데이터 (데이터시트)", edgecolors="#1e1e2e", linewidths=0.8)

# 레이블을 데이터 좌표가 아닌 화면 offset(pt)으로 배치 → 항상 점 위에 표시
# 50/100 ppm은 근접하므로 50은 왼쪽 위, 100은 R₀ 주석이 대신함
label_config = {
    10:  {"xytext": ( 7, 10), "ha": "left"},
    20:  {"xytext": ( 7, 10), "ha": "left"},
    50:  {"xytext": (-7, 10), "ha": "right"},  # 왼쪽 위 (100 ppm과 분리)
    200: {"xytext": ( 7, 10), "ha": "left"},
    300: {"xytext": ( 7, 10), "ha": "left"},
}
for ppm, rsro in zip(REF_PPM, REF_RSRO):
    if ppm not in label_config:   # 100 ppm은 R₀ 기준점 주석이 대신 표시
        continue
    cfg = label_config[ppm]
    ax.annotate(
        f"{ppm} ppm  Rs/Ro≈{rsro}",
        xy=(ppm, rsro),
        xytext=cfg["xytext"],
        textcoords="offset points",
        fontsize=7.5,
        color="#cdd6f4",
        ha=cfg["ha"],
        va="bottom",
    )

# ── 축 설정 (log-log) ──────────────────────────────────
ax.set_xscale("log")
ax.set_yscale("log")
ax.set_xlim(5, 1200)
ax.set_ylim(0.3, 6.0)

ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f"{int(x)}"))
ax.xaxis.set_minor_formatter(ticker.NullFormatter())
ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda y, _: f"{y:.1f}".rstrip('0').rstrip('.')))

ax.set_xlabel("NH₃ 농도 (ppm)", fontsize=10, labelpad=8)
ax.set_ylabel("Rs/Ro", fontsize=10, labelpad=8)
ax.set_title("MQ-135 NH₃ 감도 곡선  (Log-Log)", fontsize=12,
             fontweight="bold", color="#cdd6f4", pad=12)

ax.grid(True, which="both")
ax.grid(True, which="minor", alpha=0.3)

ax.legend(loc="upper right", fontsize=8,
          facecolor="#313244", edgecolor="#585b70", labelcolor="#cdd6f4")

# ── 저장 ───────────────────────────────────────────────
out_dir = Path(__file__).parent.parent / "docs" / "images"
out_dir.mkdir(parents=True, exist_ok=True)
out_path = out_dir / "mq135_nh3_sensitivity_curve.png"

fig.tight_layout()
fig.savefig(out_path, dpi=150, bbox_inches="tight",
            facecolor=fig.get_facecolor())
print(f"Saved: {out_path}")
plt.close(fig)

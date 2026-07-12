"""
Here is my version of plotting the data, can be used three different ways:
1) python plot_profiles.py -f metrics.csv --mode timing
2) python plot_profiles.py -f metrics.csv --mode energy
3) python plot_profiles.py -f metrics.csv --mode both
"""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

TIMING_COLS = [
    "Time Hanning Generation",
    "Time Hanning Window",
    "Time Window Reduction",
    "Time FFT",
    "Time Magnitude",
    "Time Normalization",
    "Time Reordering FFT",
    "Time Frequency Generation",
    "Time Median Averaging",
    "Time Peak Smoothing",
    "Time Gauss Smoothing",
    "Time Calibration",
    "Time Peak Detection",
    "Time Velocity",
]

ENERGY_COLS = [
    "Energy Hanning Generation",
    "Energy Hanning Window",
    "Energy Window Reduction",
    "Energy FFT",
    "Energy Magnitude",
    "Energy Normalization",
    "Energy Reordering FFT",
    "Energy Frequency Generation",
    "Energy Median Averaging",
    "Energy Peak Smoothing",
    "Energy Gauss Smoothing",
    "Energy Calibration",
    "Energy Peak Detection",
    "Energy Velocity",
    "Energy Total",
]

CMAP = plt.get_cmap("tab20")

def _plot_stacked_with_snr(df: pd.DataFrame, snr: pd.Series,
                           ylabel: str, title: str, ax=None):
    colours = [CMAP(i) for i in range(len(df.columns))]
    ax = df.plot(kind="bar", stacked=True, ax=ax, color=colours, width=0.9)

    ax.set_xlabel("Configuration ID")
    ax.set_ylabel(ylabel)
    ax.set_title(title, pad=10)
    ax.tick_params(axis="x", rotation=45, labelsize=8)

    ax.legend(title=df.columns[0].split()[0] + " components",
              fontsize="small",
              loc='center left',
              bbox_to_anchor=(-0.3, 0.5),
              frameon=False)

    ax_snr = ax.twinx()
    ax_snr.plot(df.index, snr, marker='o', linestyle='-', color='black', label="SNR")
    ax_snr.set_ylabel("SNR (dB)", color='black')
    ax_snr.tick_params(axis='y', colors='black')

    ax_snr.set_ylim(60, 200)
    ax_snr.axhline(70, color='red', linestyle=':', linewidth=1)

    yticks = list(ax_snr.get_yticks())
    if 70 not in yticks:
        yticks.append(70)
        yticks = sorted(yticks)
    ax_snr.set_yticks(yticks)

    yticklabels = [f'{int(y)}' if y != 70 else '70 (minimum)' for y in yticks]
    ax_snr.set_yticklabels(yticklabels)

    ax_snr.legend(loc='upper right')

    return ax

def plot_timing(df: pd.DataFrame, out: Path = Path("profiles_timing.png")):
    timing_df = df[TIMING_COLS] / 1e9
    _plot_stacked_with_snr(timing_df, df["SNR"], ylabel="Time (s)",
                           title="Timing breakdown with SNR")
    plt.tight_layout()
    plt.savefig(out, dpi=300)
    plt.close()


def plot_energy(df: pd.DataFrame, out: Path = Path("profiles_energy.png")):
    energy_df = df[ENERGY_COLS]
    _plot_stacked_with_snr(energy_df, df["SNR"], ylabel="Energy (J)",
                           title="Energy breakdown with SNR")
    plt.tight_layout()
    plt.savefig(out, dpi=300)
    plt.close()


def plot_both(df: pd.DataFrame,
              out: Path = Path("profiles_timing_energy_snr.png")):
    fig, (ax_t, ax_e) = plt.subplots(
        2, 1, sharex=True, figsize=(12, 12), constrained_layout=True
    )

    _plot_stacked_with_snr(df[TIMING_COLS] / 1e9, df["SNR"],
                           ylabel="Time (s)", title="Timing breakdown",
                           ax=ax_t)

    _plot_stacked_with_snr(df[ENERGY_COLS], df["SNR"],
                           ylabel="Energy (J)", title="Energy breakdown",
                           ax=ax_e)

    ax_e.set_xlabel("Configuration ID")

    plt.savefig(out, dpi=300)
    plt.close()

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot timing and/or energy breakdowns from metrics file."
    )
    parser.add_argument(
        "-f", "--file", required=True, type=Path,
        help="Path to metrics CSV"
    )
    parser.add_argument(
        "-m", "--mode", default="timing",
        choices=("timing", "energy", "both"),
        help="Which plot(s) to produce (default: timing only)."
    )
    args = parser.parse_args()

    df = pd.read_csv(args.file)
    if not df["Config ID"].is_unique:
        raise ValueError("'Config ID' must be unique for stacked-bar charts.")
    df.set_index("Config ID", inplace=True)

    if args.mode == "timing":
        plot_timing(df)
    elif args.mode == "energy":
        plot_energy(df)
    else:
        plot_both(df)

if __name__ == "__main__":
    main()

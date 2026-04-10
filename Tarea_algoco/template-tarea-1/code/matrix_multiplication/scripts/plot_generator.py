"""Generador de gráficos para la parte de matrix multiplication.

Lee el CSV generado por code/matrix_multiplication/matrix_multiplication.cpp y crea PNGs.

Notas:
- Si el CSV no contiene 'tiempo_ms', lo calcula desde 'tiempo_us'.
- Usa 'filas_a' como tamaño n (matrices cuadradas).
- Si alguna columna no existe, el script intenta seguir igual con lo disponible.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


EXPECTED_ALGORITHMS = ["naive", "strassen"]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generador de gráficos para multiplicación de matrices.")
    parser.add_argument(
        "--csv",
        type=Path,
        required=True,
        help="Ruta al CSV generado por matrix_multiplication.cpp",
    )
    parser.add_argument(
        "--out",
        type=Path,
        required=True,
        help="Carpeta de salida para los PNG",
    )
    return parser.parse_args()


def ensure_output_dir(out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)


def load_measurements(csv_path: Path) -> pd.DataFrame:
    if not csv_path.exists():
        raise FileNotFoundError(f"No existe el CSV: {csv_path}")

    df = pd.read_csv(csv_path)

    rename_map = {}
    if "algorithm" in df.columns and "algoritmo" not in df.columns:
        rename_map["algorithm"] = "algoritmo"
    if "time_ms" in df.columns and "tiempo_ms" not in df.columns:
        rename_map["time_ms"] = "tiempo_ms"
    if "time_us" in df.columns and "tiempo_us" not in df.columns:
        rename_map["time_us"] = "tiempo_us"
    if "memory_kb" in df.columns and "memoria_kb" not in df.columns:
        rename_map["memory_kb"] = "memoria_kb"

    if rename_map:
        df = df.rename(columns=rename_map)

    if "tiempo_ms" not in df.columns:
        if "tiempo_us" not in df.columns:
            raise ValueError("El CSV debe contener 'tiempo_ms' o 'tiempo_us'.")
        df["tiempo_ms"] = df["tiempo_us"] / 1000.0

    if "algoritmo" not in df.columns:
        raise ValueError("El CSV debe contener la columna 'algoritmo'.")

    if "filas_a" not in df.columns:
        raise ValueError("El CSV debe contener la columna 'filas_a' para inferir n.")

    df = df.copy()
    df["n"] = df["filas_a"].astype(int)
    df = df[df["tiempo_ms"].notna()]

    return df


def _sort_algorithm_order(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    df["algoritmo"] = pd.Categorical(df["algoritmo"], categories=EXPECTED_ALGORITHMS, ordered=True)
    return df


def plot_time_by_n(df: pd.DataFrame, out_dir: Path) -> None:
    grouped = (
        df.groupby(["algoritmo", "n"], as_index=False)["tiempo_ms"]
        .mean()
        .sort_values(["algoritmo", "n"])
    )
    grouped = _sort_algorithm_order(grouped)

    plt.figure(figsize=(10, 6))
    for alg in EXPECTED_ALGORITHMS:
        sub = grouped[grouped["algoritmo"] == alg]
        if sub.empty:
            continue
        plt.plot(sub["n"], sub["tiempo_ms"], marker="o", linewidth=2, label=alg)

    plt.xlabel("n")
    plt.ylabel("Tiempo promedio (ms)")
    plt.title("Multiplicación de matrices: tiempo vs tamaño")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "tiempo_ms_vs_n.png", dpi=200)
    plt.close()


def plot_time_loglog(df: pd.DataFrame, out_dir: Path) -> None:
    grouped = (
        df.groupby(["algoritmo", "n"], as_index=False)["tiempo_ms"]
        .mean()
        .sort_values(["algoritmo", "n"])
    )
    grouped = _sort_algorithm_order(grouped)

    plt.figure(figsize=(10, 6))
    for alg in EXPECTED_ALGORITHMS:
        sub = grouped[grouped["algoritmo"] == alg]
        if sub.empty:
            continue
        plt.loglog(sub["n"], sub["tiempo_ms"], marker="o", linewidth=2, label=alg)

    plt.xlabel("n (escala log)")
    plt.ylabel("Tiempo promedio (ms, escala log)")
    plt.title("Multiplicación de matrices: tiempo en escala log-log")
    plt.grid(True, which="both", alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "tiempo_ms_loglog.png", dpi=200)
    plt.close()


def plot_memory_by_n(df: pd.DataFrame, out_dir: Path) -> None:
    if "memoria_kb" not in df.columns:
        return

    memory_df = df[df["memoria_kb"].notna()].copy()
    if memory_df.empty:
        return

    grouped = (
        memory_df.groupby(["algoritmo", "n"], as_index=False)["memoria_kb"]
        .mean()
        .sort_values(["algoritmo", "n"])
    )
    grouped = _sort_algorithm_order(grouped)

    plt.figure(figsize=(10, 6))
    for alg in EXPECTED_ALGORITHMS:
        sub = grouped[grouped["algoritmo"] == alg]
        if sub.empty:
            continue
        plt.plot(sub["n"], sub["memoria_kb"], marker="o", linewidth=2, label=alg)

    plt.xlabel("n")
    plt.ylabel("Memoria promedio (KB)")
    plt.title("Multiplicación de matrices: memoria vs tamaño")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "memoria_kb_vs_n.png", dpi=200)
    plt.close()


def plot_time_by_group(df: pd.DataFrame, out_dir: Path) -> None:
    group_columns = [c for c in ["algoritmo", "n"] if c in df.columns]
    if len(group_columns) != 2:
        return

    for extra_col in ["disposicion", "tipo_datos"]:
        if extra_col not in df.columns:
            continue

        for value in sorted(df[extra_col].dropna().unique()):
            sub_df = df[df[extra_col] == value]
            if sub_df.empty:
                continue

            grouped = (
                sub_df.groupby(["algoritmo", "n"], as_index=False)["tiempo_ms"]
                .mean()
                .sort_values(["algoritmo", "n"])
            )
            grouped = _sort_algorithm_order(grouped)

            plt.figure(figsize=(10, 6))
            for alg in EXPECTED_ALGORITHMS:
                sub = grouped[grouped["algoritmo"] == alg]
                if sub.empty:
                    continue
                plt.plot(sub["n"], sub["tiempo_ms"], marker="o", linewidth=2, label=alg)

            plt.xlabel("n")
            plt.ylabel("Tiempo promedio (ms)")
            plt.title(f"Tiempo vs tamaño ({extra_col}={value})")
            plt.grid(True, alpha=0.3)
            plt.legend()
            plt.tight_layout()
            safe_value = "".join(c if c.isalnum() or c in "-_" else "_" for c in str(value))
            plt.savefig(out_dir / f"tiempo_ms_vs_n_{extra_col}_{safe_value}.png", dpi=200)
            plt.close()


def main() -> None:
    args = parse_args()
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent
    forced_out = project_dir / "data" / "plots" / "matrix_multiplication"

    if args.out.resolve() != forced_out.resolve():
        print(f"[INFO] Ruta de salida ignorada: {args.out}")
        print(f"[INFO] Usando carpeta correcta: {forced_out}")

    out_dir = forced_out
    ensure_output_dir(out_dir)

    df = load_measurements(args.csv)

    plot_time_by_n(df, out_dir)
    plot_time_loglog(df, out_dir)
    plot_memory_by_n(df, out_dir)
    plot_time_by_group(df, out_dir)

    print(f"Gráficos generados en: {out_dir}")


if __name__ == "__main__":
    main()

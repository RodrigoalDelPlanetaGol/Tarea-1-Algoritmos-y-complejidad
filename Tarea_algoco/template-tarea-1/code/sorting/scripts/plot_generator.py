"""Generador de gráficos para la parte de sorting.

Lee el CSV generado por code/sorting/sorting.cpp y crea PNGs en una carpeta de salida.

Uso sugerido:
    python plot_generator.py --csv ../data/measurements/sorting_measurements.csv --out ../plots/sorting

Notas:
- El script intenta ser robusto con los nombres de columnas.
- Si no existe 'tiempo_ms', lo calcula desde 'tiempo_us'.
- Si no existe 'memoria_kb', omite el gráfico de memoria.
- Genera gráficos promedio por algoritmo y tamaño n.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt
import pandas as pd


EXPECTED_ALGORITHMS = ["merge", "quick", "stdsort"]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generador de gráficos para sorting.")
    parser.add_argument(
        "--csv",
        type=Path,
        required=True,
        help="Ruta al CSV generado por sorting.cpp",
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

    # Normalización mínima por si cambian nombres de columnas.
    rename_map = {}
    if "algorithm" in df.columns and "algoritmo" not in df.columns:
        rename_map["algorithm"] = "algoritmo"
    if "input_file" in df.columns and "archivo_entrada" not in df.columns:
        rename_map["input_file"] = "archivo_entrada"
    if "output_file" in df.columns and "archivo_salida" not in df.columns:
        rename_map["output_file"] = "archivo_salida"
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

    if "algoritmo" not in df.columns or "n" not in df.columns:
        raise ValueError("El CSV debe contener al menos las columnas 'algoritmo' y 'n'.")

    return df


def _sort_algorithm_order(df: pd.DataFrame) -> pd.DataFrame:
    if "algoritmo" in df.columns:
        df = df.copy()
        df["algoritmo"] = pd.Categorical(
            df["algoritmo"], categories=EXPECTED_ALGORITHMS, ordered=True
        )
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
    plt.title("Tiempo de ordenamiento vs tamaño del arreglo")
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
    plt.title("Tiempo de ordenamiento en escala log-log")
    plt.grid(True, which="both", alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "tiempo_ms_loglog.png", dpi=200)
    plt.close()


def plot_memory_by_n(df: pd.DataFrame, out_dir: Path) -> None:
    if "memoria_kb" not in df.columns:
        return

    memory_df = df.copy()
    memory_df = memory_df[memory_df["memoria_kb"].notna()]
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
    plt.title("Memoria usada vs tamaño del arreglo")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "memoria_kb_vs_n.png", dpi=200)
    plt.close()


def plot_by_input_type(df: pd.DataFrame, out_dir: Path) -> None:
    if "disposicion" not in df.columns:
        return

    for disposicion in sorted(df["disposicion"].dropna().unique()):
        sub_df = df[df["disposicion"] == disposicion]
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
        plt.title(f"Tiempo vs tamaño para disposición: {disposicion}")
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        safe_name = "".join(c if c.isalnum() or c in "-_" else "_" for c in disposicion)
        plt.savefig(out_dir / f"tiempo_ms_vs_n_{safe_name}.png", dpi=200)
        plt.close()


def main() -> None:
    args = parse_args()

    # El script vive en: code/sorting/scripts/plot_generator.py
    # Queremos guardar siempre en: code/sorting/data/plots/sorting
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent
    forced_out = project_dir / "data" / "plots" / "sorting"

    if args.out.resolve() != forced_out.resolve():
        print(f"[INFO] Ruta de salida ignorada: {args.out}")
        print(f"[INFO] Usando carpeta correcta: {forced_out}")

    ensure_output_dir(forced_out)

    df = load_measurements(args.csv)

    # Limpieza básica
    df = df.copy()
    df = df[df["n"].notna()]
    df["n"] = df["n"].astype(int)
    df = df[df["tiempo_ms"].notna()]

    plot_time_by_n(df, forced_out)
    plot_time_loglog(df, forced_out)
    plot_memory_by_n(df, forced_out)
    plot_by_input_type(df, forced_out)

    print(f"Gráficos generados en: {forced_out}")


if __name__ == "__main__":
    main()
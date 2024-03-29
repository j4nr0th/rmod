import matplotlib.pyplot as plt
import numpy as np
from dataclasses import dataclass


@dataclass
class AcoRNG:
    order: int
    modulo: int
    state: np.ndarray


@dataclass
class MswsRNG:
    x0: int
    w0: int
    s0: int
    x1: int
    w1: int
    s1: int
    remaining: float | None


def use_msws(rng: MswsRNG) -> float:
    if rng.remaining is not None:
        f: float = rng.remaining
        rng.remaining = None
        return f
    tmp: int
    rng.x0 = (rng.x0 * rng.x0) % ((1 << 64) - 1)
    rng.w0 = (rng.w0 + rng.s0) % ((1 << 64) - 1)
    rng.x0 = (rng.x0 + rng.w0) % ((1 << 64) - 1)
    tmp = rng.x0
    rng.x0 = ((rng.x0 >> 32) | (rng.x0 << 32))
    rng.x1 = (rng.x1 * rng.x1) % ((1 << 64) - 1)
    rng.w1 = (rng.w1 + rng.s1) % ((1 << 64) - 1)
    rng.x1 = (rng.x1 + rng.w1) % ((1 << 64) - 1)
    rng.x1 = ((rng.x1 >> 32) | (rng.x1 << 32))
    r: int = (tmp ^ rng.x1) % ((1 << 64) - 1)
    f1 = float(r >> 32) / 4294967296
    f2 = float(r & ((1 << 32) - 1)) / 4294967296
    rng.remaining = f1
    return f2


def msws_create() -> MswsRNG:
    return MswsRNG(0, 0, 0xb5ad4eceda1ce2a9, 0, 0, 0x278c5a4d8419fe6b, None)


def use_rng(acorn: AcoRNG) -> float:
    for j in range(1, acorn.order):
        acorn.state[j] += acorn.state[j - 1]
        acorn.state[j] %= acorn.modulo
    return acorn.state[-1] / acorn.modulo


def acorn_create(modulo: int, order: int, seed: int) -> AcoRNG:
    acorn = AcoRNG(order, (1 << modulo), np.zeros(order))
    acorn.state[0] = seed
    return acorn


if __name__ == "__main__":
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1)
    fig: plt.Figure
    ax1: plt.Axes
    ax2: plt.Axes
    test_rng = acorn_create(60, 30, 1290412412049941)

    N_SAMPLES = 1060170
    sample_array = np.zeros(N_SAMPLES, dtype=np.float64)
    for i in range(N_SAMPLES):
        sample_array[i] = use_rng(test_rng)

    counts, bins = np.histogram(sample_array, bins=100)
    ax1.stairs(counts, bins)
    ax1.set_title("ACORN")
    errors = N_SAMPLES / len(counts) - counts
    print("Acorn's errors:", np.mean(errors), "\nSt. dev.:", np.std(errors))

    test_rng = msws_create()
    sample_array = np.zeros(N_SAMPLES, dtype=np.float64)
    for i in range(N_SAMPLES):
        sample_array[i] = use_msws(test_rng)

    counts, bins = np.histogram(sample_array, bins=100)
    ax2.stairs(counts, bins)
    ax2.set_title("MSWS")
    errors = N_SAMPLES / len(counts) - counts
    print("MSWS's errors:", np.mean(errors), "\nSt. dev.:", np.std(errors))

    sample_array = np.random.rand(N_SAMPLES)
    counts, bins = np.histogram(sample_array, bins=100)
    ax3.stairs(counts, bins)
    ax3.set_title("np.random.rand()")
    errors = N_SAMPLES / len(counts) - counts
    print("np.random.rand():", np.mean(errors), "\nSt. dev.:", np.std(errors))

    plt.tight_layout()
    plt.show()
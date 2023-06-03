import matplotlib.pyplot as plt
import numpy as np
from dataclasses import dataclass


@dataclass
class AcoRNG:
    order: int;
    modulo: int;
    state: np.ndarray;


def use_rng(acorn: AcoRNG) -> float:
    for i in range(1, acorn.order):
        acorn.state[i] += acorn.state[i - 1];
        acorn.state[i] %= acorn.modulo;
    return acorn.state[-1] / acorn.modulo;


def acorn_create(modulo: int, order: int, seed: int) -> AcoRNG:
    acorn = AcoRNG(order, (1 << modulo), np.zeros(order));
    acorn.state[0] = seed;
    return acorn;


if __name__ == "__main__":
    fig, ax = plt.subplots(1, 1);
    fig: plt.Figure;
    ax: plt.Axes;
    rng = acorn_create(60, 10, 1290412412049941);

    N_SAMPLES = 10000;
    sample_array = np.zeros(N_SAMPLES, dtype=np.float64);
    for i in range(N_SAMPLES):
        sample_array[i] = use_rng(rng);

    ax.hist(sample_array);

    plt.show();
import numpy as np
import matplotlib.pyplot as plt

if __name__ == "__main__":
    values = np.linspace(0, 0.99, 10000);
    exact = -np.log(1 - values);
    approximate = values.copy();
    list_approx = list()
    list_approx.append(approximate.copy());
    for i in range(100):
        approximate = values + 0.5 * approximate ** 2 * (1 - approximate / 3 * (1 - approximate / 4 * (1 - approximate / 5)));
        if i % 10 == 0:
            list_approx.append(approximate.copy());
    fig, ax1 = plt.subplots(1, 1);
    fig: plt.Figure;
    ax1: plt.Axes;
    ax1.plot(values, exact, label="exact");
    for i, a in enumerate(list_approx):
        ax1.plot(values, a, label=f"n={5 * i}");

    ax1.legend();

    plt.show()

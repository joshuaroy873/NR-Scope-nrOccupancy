import matplotlib.pyplot as plt
import numpy as np

f1 = open("time_data/time_series_pre_resample.txt", "r")
f2 = open("time_data/time_series_post_resample.txt", "r")
r = 23040000/33333333; 
fig, axs = plt.subplots(1)

for line in f1:
    reformatted = line.replace(" ", "").replace("i", "j")
    complex_strs = reformatted.split(",")[:-1]

    cfs = []

    for complex_str in complex_strs:
        cf = complex(complex_str)
        cfs.append(cf)
    
    x1s = np.arange(0, len(cfs), 1)
    y1s = np.array(cfs)

    axs.plot(x1s, y1s)

    break

for line in f2:
    reformatted = line.replace(" ", "").replace("i", "j")
    complex_strs = reformatted.split(",")[:-1]

    cfs = []

    for complex_str in complex_strs:
        cf = complex(complex_str)
        cfs.append(cf)
    
    x2s = np.arange(0, len(cfs), 1) / r
    y2s = np.array(cfs)

    axs.plot(x2s, y2s)

    break

plt.show()
f1.close()
f2.close()


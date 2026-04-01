import matplotlib.pyplot as plt

#All graphs use max_dep 30
#All graphs are scheduled on 20 processors

task_numbers = [1000, 5000, 10000, 15000, 20000, 25000, 30000, 35000, 40000, 45000, 50000, 60000, 70000, 80000, 90000, 100000, 120000]

# 1567376
# 20*60 + 2.264

scheduling_times = [0.023, 0.186, 0.447, 0.965, 1.790, 2.906, 4.127, 5.007, 6.217, 8.203, 10.532, 11.741, 15.466, 18.672, 20.616, 26.742, 31.509]

plt.plot(task_numbers, scheduling_times, "o-")
plt.suptitle("20 processors  30 max_deps")
plt.ylabel("Execution Time (s)")
plt.xlabel("Number of Tasks")
plt.show()



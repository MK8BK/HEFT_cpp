import matplotlib.pyplot as plt

#All graphs use max_dep 30
#All graphs are scheduled on 20 processors

task_numbers = [1000, 5000, 10000, 15000, 20000, 25000, 30000, 35000, 40000, 45000, 50000]

scheduling_times = [0.023, 0.186, 0.447, 0.965, 1.790, 2.906, 4.127, 5.007, 6.217, 8.203, 10.532]

plt.plot(task_numbers, scheduling_times, "o-")
plt.suptitle("20 processors  30 max_deps")
plt.ylabel("Execution Time (s)")
plt.xlabel("Number of tasks(s)")
plt.show()



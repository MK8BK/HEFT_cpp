import matplotlib.pyplot as plt

x = [1,2,4,5,8]
y = [1202, 778, 463, 400, 326]

plt.plot(x, y, 'o-')
plt.title("1.5M tasks on 20 processors")
plt.xlabel("Number of threads")
plt.ylabel("Execution time (s)")
plt.show()

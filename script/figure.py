import matplotlib.pyplot as plt
# result.log 的格式：
# 横坐标 纵坐标
x_axis = []
y_axis = []
with open("./result.log") as f:
    for line in f.readlines():
        line = line.split()
        if (len(line)):
            print(line)
            x_axis.append(int(line[0]))
            y_axis.append(int(line[1]))
print(x_axis)
print(y_axis)

def plot(x_axis, y_axis, x_label, y_label, label, figsize, fname):
    x = range(len(x_axis))
    x_ticks = x_axis
    fig = plt.figure(figsize=figsize)
    plt.plot(x_axis, y_axis, marker='o',color='orange',label = label)
    #plt.xticks(list(x),x_ticks)
    #plt.yticks([0,70,140,210,280,350])
    plt.xlabel(x_label, fontsize=16)
    plt.ylabel(y_label,fontsize=16)
    plt.legend(bbox_to_anchor=(0.1, 1.2), fontsize=16, frameon=False,ncol=2,loc='upper left')
#    plt.show()
    fig.savefig(fname,dpi=300,bbox_inches = 'tight')

plot(x_axis, y_axis, "data_size_block", "duration", "A NAME", (6, 3), "./figs/test.pdf")
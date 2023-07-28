f = open("./web-Google.txt", "r")
lines = f.readlines()
count = 0
st = set()  
di = dict()
for line in lines:
    item = line.split()
    src = int(item[0])
    dst = int(item[1])
    if src not in st:
        di[src] = count
        st.add(src)
        count = count + 1
    if dst not in st:
        di[dst] = count
        st.add(dst)
        count = count + 1
for line in lines:
    item = line.split()
    src = int(item[0])
    dst = int(item[1])
    print(di[src], di[dst])
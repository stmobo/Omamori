import math

sys_mem = 2096700
ceil = math.ceil

def do_summation(order, size_in_kb):
    sum = 0
    for i in range(0, order+1):
        sum = sum + ceil(size_in_kb / pow(2, i))
    return sum
    
print("order  size    num    total blocks")
print("----------------------------------")
for i in range(0,18):
    num_blks = ceil(sys_mem / pow(2, i))
    sum = do_summation(i, sys_mem)
    sz_bytes = ceil(sum / 8)
    sz_kb = ceil(sz_bytes / 1024)
    print(str(i)+" - "+str(ceil(pow(2, i)))+" - "+str(num_blks)+" - "+str(sum)+" ("+str(sz_bytes)+"bytes / "+str(sz_kb)+" kb)")
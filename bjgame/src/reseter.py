#! /usr/bin/env python3
import time

cnt = 1

while True:
    print(f"Starting Round #{cnt}")
    open("/gol/state.dat","w").write(f"READY {cnt}")
    cnt += 1
    time.sleep(60*6)
    print(f"Ending Round #{cnt}")
    open("/gol/state.dat", "w").write(f"STOP {cnt}")
    time.sleep(5)





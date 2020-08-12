#! /usr/bin/env python3
import subprocess
import time
import os.path
import json
import glob
import re


def main():
    while True:
        stdata = open("/gol/state.dat").read().split()
        round_id = 1
        if len(stdata) >= 2:
            round_id = int(stdata[1])
        cmd = ['jq', '.', '-s']
        hhpath = f"/tmp/round-{round_id}/handhist"

        if os.path.exists(f"{hhpath}/hand-1.json"):
            ptrn = re.compile(rf"hand\-([0-9]{{1,3}})\.json")
            hand_files = {}
            hand_file_list = []
            for fn in glob.iglob(f"{hhpath}/hand-*.json"):
                bn = os.path.basename(fn)
                srch = re.search(ptrn, bn)
                if srch:
                    hand_files[int(srch.group(1))] = fn

            for key in sorted(hand_files.keys()):
                hand_file_list.append(hand_files[key])

            cmd.extend(hand_file_list)

            all_jfp = open(f"/gol/bj/results/all-hands-round-{round_id}.json", "wb")
            subprocess.check_call(cmd, cwd=f"{hhpath}", stdout=all_jfp)
            all_jfp.close()

            rounds = []
            for fn in glob.iglob('/tmp/round-*'):
                if os.path.exists(f"{fn}/handhist/hand-1.json"):
                    basepath = os.path.basename(fn)
                    rounds.append(int(basepath.split("-")[1]))

                if len(rounds) > 0:
                    with open("/gol/bj/results/index.json", "w") as jfn:
                        outd = {}
                        outd["maxTick"] = max(rounds)
                        outd["minTick"] = min(rounds)
                        outd["rounds"] = rounds
                        json.dump(outd, jfn, indent=4)
                        #jfn.write(f'{{"maxTick":"{max(rounds)}", "minTick":"{min(rounds)}"}}\n')
                else:
                    print(f"Not enough rounds")

        else:
            print(f"No hand history at {hhpath}")

        time.sleep(20)

if __name__ == "__main__":
    main()


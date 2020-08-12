#!/usr/bin/env python3
import sys
import time
import shutil
import random
import signal
import filecmp
import os.path
import logging
import subprocess

_log = logging.getLogger("playbj")
_log.setLevel(logging.INFO)
handler = logging.StreamHandler(stream=sys.stdout)
handler.setLevel(logging.INFO)
formatter = logging.Formatter('PLAY %(asctime)s : %(levelname)s : \033[38;5;33m%(message)s\033[0m')
handler.setFormatter(formatter)
_log.addHandler(handler)

def signal_handler(sig, frame):
    print('Signal Received, Cleaning up and  Exiting...')

    sys.exit(0)


def wait_for_team_files():

    found = False
    while not found:
        found = True

        for team_id in range(1,Game.NUM_TEAMS+1):
            mcfn = os.path.join("/gol","inputs", f"laas-bj-teams-controller-{team_id}.mc")
            tmpfn = os.path.join("/tmp","inputs", f"laas-bj-teams-controller-{team_id}.mc")
            if os.path.exists(mcfn):
                os.unlink(mcfn)

            if os.path.exists(tmpfn):
                shutil.copy(tmpfn, mcfn)
            else:
                found = False
                break

        if not found:
            _log.info("Waiting for all team GoL files.")
            time.sleep(1)


class Game():
    MAX_GENERATIONS = "1000000000"
    DEALER_STEPS = "2000"
    TEAM_STEPS = "2000"
    SHUFFLER_STEPS = "250000"
    state_fn = "/gol/state.dat"
    NUM_TEAMS = os.environ.get("NUM_TEAMS",3)

    def __init__(self):
        self.dealer_proc = None
        self.shuffler_proc = None
        self.team_procs = {}

    def start_shuffler(self):
        """-o /tmp/out-shuffler.mc --shuffler laas-bj-shuffler_v3.mc"""
        cmd = ["./bgolly-dealer", "--quiet", "-R",  "--render", "-a", "RuleLoader", "-s", "./", "-r", "Varlife",
               "-m", Game.MAX_GENERATIONS, "-i", Game.SHUFFLER_STEPS, "-o", "/tmp/out-shuffler.mc",
               "--shuffler", "/gol/laas-bj-shuffler_v3.mc"]
        _log.debug(cmd)
        self.shuffler_proc = subprocess.Popen(cmd, cwd="/gol")

    def start_dealer(self, round_id):
        cmd = ["./bgolly-dealer","-R", "--quiet","--render", "-a", "RuleLoader", "-s", "./", "-r", "Varlife",
               "-m", Game.MAX_GENERATIONS, "-i", Game.DEALER_STEPS, "-o", "/tmp/out-dealer.mc",
               "/gol/laas-bj-dealer-256byte.mc"]

        _log.debug(cmd)
        env = os.environ
        env["ROUND_ID"] = round_id
        self.dealer_proc = subprocess.Popen(cmd, cwd="/gol", env=env)

    def start_team(self, round_id, team_id):

        rpaths = get_result_paths(round_id)
        mcfn = os.path.join("/gol", "inputs", f"laas-bj-teams-controller-{team_id}.mc")

        team_cmd = ["./bgolly-controller", "-R", "-q", "-a", "RuleLoader", "-s", "./", "-r", "Varlife",
                    "-m", Game.MAX_GENERATIONS, "-i", Game.TEAM_STEPS, "-o", f"{rpaths['base']}/laas-controller-team-{team_id}.mc",
                    mcfn]
        env = os.environ
        _log.debug(" ".join(team_cmd))

        env["CPUID"] = str(team_id)
        env["ROUND_ID"] = round_id
        _log.info(f"Starting CPU {team_id}")

        self.team_procs[team_id] = subprocess.Popen(team_cmd, cwd="/gol", env=env)

    def start_teams(self, round_id):
        start_order = list(range(1,Game.NUM_TEAMS+1))
        random.shuffle(start_order)

        for team_id in start_order:
            self.start_team(round_id, team_id)

    def terminate(self):
        if self.shuffler_proc:
            self.shuffler_proc.terminate()
        if self.dealer_proc:
            self.dealer_proc.terminate()
        for t in self.team_procs.values():
            if t:
                t.terminate()

    def ready(self):
        ready = False
        if os.path.exists(Game.state_fn):
            f = open(Game.state_fn, "r")
            ready = f.read().startswith("READY")
            f.close()
        else:
            _log.info(f"NOT ready b/c {Game.state_fn} is missing")
        return ready

    def wait_for_ready_message(self):
        wait_cnt = 0
        while not self.ready():
            if wait_cnt % 60 == 0:
                _log.info("Game not ready, sleeping...")
            time.sleep(1)
            wait_cnt += 1
        fdata = open("/gol/state.dat", "r").read().split(" ")
        if len(fdata) >= 2:
            round_id = fdata[1].strip()
        else:
            round_id = str(9999)
        return round_id

    def reset_input_file(self, team_id):
        mcfn = os.path.join("/gol", "inputs", f"laas-bj-teams-controller-{team_id}.mc")
        tmpfn = os.path.join("/tmp", "inputs", f"laas-bj-teams-controller-{team_id}.mc")

        if os.path.exists(tmpfn):
            if not filecmp.cmp(mcfn, tmpfn):
                _log.info(f"Updating to new pattern for #{team_id}")
            if os.path.exists(mcfn):
                os.unlink(mcfn)
            shutil.copy(tmpfn, mcfn)

    def monitor(self, round_id):
        loopcnt = 0
        while self.ready():
            if loopcnt % 30 == 0:
                self.copy_pcap(round_id)
            loopcnt += 1
            if self.shuffler_proc and self.shuffler_proc.poll() is None:
                pass
            else:
                if self.shuffler_proc:
                    try:
                        self.shuffler_proc.terminate()
                    except Exception as ex:
                        _log.error(ex)
                        import traceback
                        traceback.print_exc()
                    self.shuffler_proc = None
                    self.start_shuffler()

            for team_id, tp in self.team_procs.items():
                if tp and tp.poll() is None:
                    continue
                else:
                    if tp:
                        try:
                            tp.terminate()
                        except Exception as ex:
                            _log.error(ex)
                            import traceback
                            traceback.print_exc()
                        self.team_procs[team_id] = None

                    self.reset_input_file(team_id)

                    self.start_team(round_id, team_id)

            time.sleep(1)

        _log.info("\033[36m###### Tick change detected #######\033[0m")

    def copy_pcap(self, round_id):
        stored_pcap_fn = f"/tmp/round-{round_id}/gol.pcap"
        copy_pcap_to_fn = f"/tmp/round-{round_id}/gol-{round_id}.pcap"

        try:
            if os.path.exists(stored_pcap_fn):
                stored_size = os.path.getsize(stored_pcap_fn)
            else:
                stored_size = 0
            if os.path.exists(copy_pcap_to_fn):
                copy_size = os.path.getsize(copy_pcap_to_fn)
            else:
                copy_size = 0

            if copy_size < stored_size:
                shutil.copy(stored_pcap_fn, copy_pcap_to_fn)

        except Exception as ex:
            _log.error(ex)
            _log.info(f"Error with copying pcap, {stored_pcap_fn} to {copy_pcap_to_fn}")



def get_result_paths(round_id):
    rpaths = {}
    rpaths["base"] = f"/tmp/round-{round_id}"
    rpaths["handhist"] = rpaths["base"] + "/handhist"
    rpaths["koh_scores"] = rpaths["base"] + "/koh_scores"

    return rpaths

def inialize_for_round(round_id):
    rpaths = get_result_paths(round_id)
    for path in rpaths.values():
        if not os.path.exists(path):
            os.makedirs(path)


def main():

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    if not os.path.exists(f"/gol/inputs"):
        os.makedirs("/gol/inputs")
    game = Game()
    while True:
        try:
            game.start_shuffler()
            round_id = game.wait_for_ready_message()
            inialize_for_round(round_id)

            print(f"\033[31m" + "#" * 80 + f"\n#\t\tStarting hands for ROUND #{round_id}\n" + "#" * 80 + "\033[0m\n")

            wait_for_team_files()
            game.start_dealer(round_id)
            game.start_teams(round_id)
            game.monitor(round_id)
            game.copy_pcap(round_id)
            game.terminate()

            print(f"\033[31m" + "#" * 80 + f"\n#\t\tFinished for ROUND #{round_id}\n" + "#" * 80 + "\033[0m\n")

        except Exception as ex:
            import traceback
            traceback.print_exc()
            _log.error(ex)
        finally:
            game.terminate()
            print("#" * 80)


if __name__ == "__main__":
  main()




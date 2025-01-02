import json
import argparse
import subprocess
from pathlib import Path
from utils.V2 import JsonConfig
from dataclasses import asdict
import signal
import sys

BASE = "/home/pietro/Documents/repos/ns3-wifi-sim"

#PROGRAM = "build/latency-test-v2/ns3-dev-latency-test-v2-debug"
PROGRAM = "build/latency-test-v2/ns3-dev-latency-test-v2-optimized"

#CONF_FILE = "latency-test-v2/test_beacons_ap/test_beacons_ap.json"
#OUT_FOLDER = "latency-test-v2/test_beacons_ap"

CONF_FILE = "latency-test-v2/test_wfcs/wfcs_simulations_01.json"
OUT_FOLDER = "latency-test-v2/test_wfcs/sim_res"

# CONF_FILE = "latency-test-v2/test_simple_conf/single_sim_conf_list.json"
# OUT_FOLDER = "latency-test-v2/test_simple_conf/sim_res"

OUT_PREFIX = "db"

BATCH_SIZE = 4


class ProcessBatch:
    def __init__(self, batch_size):
        self._to_start = 0
        self._cmds = []
        self._procs = []
        self._batch_size = batch_size

        # https://docs.python.org/3/faq/programming.html#why-do-lambdas-defined-in-a-loop-with-different-values-all-return-the-same-result
        signal.signal(signal.SIGINT, lambda sig_num, frame: self.kill_procs()) 

    def add_command(self, cmd):
        self._cmds.append(cmd)   

    def start(self):
        n_start = self._batch_size if self._batch_size < len(self._cmds) else len(self._cmds)
        for _ in range(n_start):
             if self._start_proc():
                print("Started process {}".format(self._to_start - 1))           

        while self._procs:
            proc = self._wait_proc()
            print("Process {} finished with return code {}".format(self._num_older_proc, proc.returncode))
            # print(p.stdout)
            # print(p.stderr)            
            if self._start_proc():
                print("Started process {}".format(self._to_start - 1))

    def kill_procs(self):
        while self._procs:
            proc = self._kill_proc()            
            print("Process {} terminated".format(self._num_older_proc-1))
        sys.exit()

    def __len__(self):
        return len(self._cmds)
    
    def _start_proc(self):
        if not (self._to_start < len(self._cmds)):
            return False        
        cmd = self._cmds[self._to_start]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="ascii")
        self._procs.append(proc)
        self._to_start+=1
        return True    

    def _wait_proc(self):
        self._procs[0].wait()
        return self._procs.pop(0)

    def _kill_proc(self):
        self._procs[0].kill()
        return self._procs.pop(0)

    @property
    def _num_older_proc(self):
        return self._to_start - len(self._procs)


def main():
    # parser = argparse.ArgumentParser(
    #                 prog='Launch_SIM',
    #                 description='Launch ns3 latency map simulation')

    # parser.add_argument("-c", "--sim_config",required=True)
    # parser.add_argument("-p", "--program", required=True)
    # parser.add_argument("-o", "--outfile")
    # args = parser.parse_args()
    # print(args.sim_config, args.program, args.outfile)

    program = Path(BASE) / PROGRAM
    conf_file = Path(BASE) / CONF_FILE

    (Path(BASE) / Path(OUT_FOLDER)).mkdir(parents=True, exist_ok=True)

    with open(conf_file) as f:
        json_configs = json.load(f)

    process_batch = ProcessBatch(BATCH_SIZE)
    for idx, jconf in enumerate(json_configs):

        jconf = JsonConfig(**jconf)
        sim_conf = json.dumps(asdict(jconf), separators=(',', ':'))
        outpath = Path(BASE) / OUT_FOLDER / "{}_{}.json".format(OUT_PREFIX, idx)

        command = [
                    str(program),
                    '--jsonConfig={}'.format(sim_conf),
                    '--outFilePath={}'.format(str(outpath)),
                    "--inlineConfig"
                ]
        process_batch.add_command(command)
    

    print("{} simulations to run".format(len(process_batch)))

    process_batch.start()


    # PER FARLO MIGLIORE
    # https://stackoverflow.com/questions/26774781/python-multiple-subprocess-with-a-pool-queue-recover-output-as-soon-as-one-finis
    # https://stackoverflow.com/questions/11312525/catch-ctrlc-sigint-and-exit-multiprocesses-gracefully-in-python


if __name__ == "__main__":
    main()
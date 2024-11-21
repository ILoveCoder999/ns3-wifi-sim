import json
import argparse
import subprocess
from pathlib import Path
from utils.V2 import JsonConfig
from dataclasses import asdict

BASE = "/home/ptrchv/repos/latency_map/ns-3-dev-ben"

PROGRAM = "build/latency-test-v2/ns3-dev-latency-test-v2-debug"
CONF_FILE = "latency-test-v2/scripts/single_sim_conf.json"
OUTPATH = "db_test.json"


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
    outpath = Path(BASE) / OUTPATH

    print(program)
    print(conf_file)
    print(outpath)

    with open(conf_file) as f:
        jconf = JsonConfig(json.load(f))
    
    sim_conf = json.dumps(asdict(jconf), separators=(',', ':'))

    command = [
                str(program), 
                sim_conf, 
                str(outpath)
            ]
    
    # print(command)
    # print(" ".join(command))

    output = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="ascii")
    #print(output.stdout.decode('utf-8'))
    print(output.returncode)
    print(output.stdout)
    print(output.stderr)



    
    



#     with open("") as f:
        
#         parser = argparse.ArgumentParser(prog='myprogram')

# parser.add_argument('--foo', help='foo of the %(prog)s program')

# parser.print_help()

#     executable = Path(__file__).parent.joinpath("../../build/latency-test-v2/ns3-dev-latency-test-v2-optimized").resolve()
#     config = json.dumps(asdict(sim), separators=(',', ':'))
#     out_file = Path(args.out_dir).joinpath(LATENCY_FILE.format(i))
     
#     ciao f"{executable} '{config}' {out_file}"



if __name__ == "__main__":
    main()
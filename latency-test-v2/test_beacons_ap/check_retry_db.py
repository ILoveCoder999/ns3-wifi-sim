# ---------------
# Used to compare logged retry number to values available in pcap files
# Wireshard filter: wlan.fc.retry ==1
# --------------- 

import json
from pathlib import Path

def main():
    sim_folder = Path("sim_res_2")
    for sim_file in sim_folder.iterdir():
        with open(sim_file) as f:
            sim_db = json.load(f)
        sim_db = sim_db[2:-2]
        retry_count = len([True for row in sim_db if row["retransmissions"]])
        print("sim {}, retries: {}".format(str(sim_file), retry_count))

if __name__ == "__main__":
    main()

import json
import pandas as pd
from pathlib import Path
from metrics import METRICS

POS_LIST = [i for i in range(1, 51)]

def export_mobility_map_1d(db_file, map_dir):
    with open(db_file) as f:
        exp_data = json.load(f)

    map_dir.mkdir(parents=True, exist_ok=True)    

    exp_data = exp_data[1:-1]
    exp_data = [{"x_pos": round(row["transmissions"][0]["position"][0]), **row} for row in exp_data]    
    pos_data = {pos: [] for pos in POS_LIST}   
    
    for row in exp_data:
        if row["x_pos"] in pos_data:
            pos_data[row["x_pos"]].append(row)

    df_metrics = pd.DataFrame(columns = ["x_pos"] + [m for m in METRICS])

    for x_pos, data in pos_data.items():
        new_row = {c: None for c in df_metrics.columns}
        new_row["x_pos"] = x_pos

        for m in METRICS:
            new_row[m] = METRICS[m]["fnc"](data) if len(data) > 0 else None
        df_metrics.loc[len(df_metrics)] = new_row

    df_metrics.to_csv(map_dir / "map_{:02}.csv".format(0), index=False)



def main():
    db_file = Path("/home/ptrchv/repos/ns3-wifi-sim/latency-test-v2/test_etfa_wip_mobility/sim_res_test/db_7.json")
    map_dir = Path("/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/etfa_wip_sim_maps")
    export_mobility_map_1d(db_file, map_dir)

    



if __name__ == "__main__":
    main()

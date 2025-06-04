import json
import pandas as pd
from pathlib import Path
from metrics import METRICS

POS_LIST = [i for i in range(1, 51)]

METRICS_CONST = ["latency_avg", "latency_min" ,"latency_10_perc", "latency_99_perc", "latency_99.9_perc", "transmission_num_avg", "transmission_num_max_no_dropped", "%_dropped"]




def export_mobility_map_1d(db_file):
    with open(db_file) as f:
        exp_data = json.load(f)

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

    return df_metrics

def merge_constant_maps(map_files):
    df_list = []
    for f in map_files:
        df = pd.read_csv(f)
        df_list.append(df)

    metric_rate = [m + "_rate" for m in METRICS_CONST]
    columns = [item for pair in zip(METRICS_CONST, metric_rate) for item in pair]

    df_metrics = pd.DataFrame(columns = ["x_pos"] + columns)
    for x_pos in range(1, 51):
        new_row = {c: None for c in df_metrics.columns}
        new_row["x_pos"] = int(x_pos)
        for m in METRICS_CONST:
            values = []
            rates = []
            for df in df_list:
                cell = df[df["x_pos"] == x_pos][m].reset_index(drop=True)
                values.append(float(cell[0]))
                cell = df[df["x_pos"] == x_pos]["rate_avg"].reset_index(drop=True)
                rates.append(float(cell[0]))
            new_row[m] = min(values)
            indices = [i for i, x in enumerate(values) if x == new_row[m]]
            new_row[m + "_rate"] = [rates[i] for i in indices]
        df_metrics.loc[len(df_metrics)] = new_row
    return df_metrics
                



def main():
    # db_folder = Path("/home/ptrchv/repos/ns3-wifi-sim//latency_map_plots/etfa_wip_mobility_sim")
    # map_dir = Path("/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/etfa_wip_sim_maps/mobility")

    # map_dir.mkdir(parents=True, exist_ok=True)
    
    # for db_file in db_folder.iterdir():
    #     print("Procession {}".format(db_file.name))
    #     db_num = int(db_file.name.split("_")[1].split(".")[0])
    #     df = export_mobility_map_1d(db_file)
    #     df.to_csv(map_dir / "map_{:02}.csv".format(db_num), index=False)

    const_data_dir = Path("/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/etfa_wip_constant_sim_maps/maps")
    const_out_dir = Path("/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/etfa_wip_sim_maps/constant")
    const_out_dir.mkdir(parents=True, exist_ok=True)


    map_files = sorted([f for f in const_data_dir.iterdir()])
    map_experiments = [map_files[:8], map_files[8:16], map_files[16:]]
    for idx, map_files in enumerate(map_experiments):
        df = merge_constant_maps(map_files)
        df.to_csv(const_out_dir / "map_{:02}.csv".format(idx), index=False)

if __name__ == "__main__":
    main()

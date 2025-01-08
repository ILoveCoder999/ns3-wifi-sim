#%%
import json
import itertools
from enum import Enum
from pathlib import Path
import copy
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import statistics


EXP_DIR = Path("./wfcs_sim").absolute()  # folder with experiement results
OUT_DIR = EXP_DIR.parent.absolute() / (EXP_DIR.name + "_maps")    # output folder

EXP_FILE_SUFFIX = ".dat"
EXTRACT_EXP_NUM = lambda x: int(x.name.split(".")[0].replace("db", ""))
#EXTRACT_EXP_NUM = lambda x: int(x.name.split(".")[0].split("_")[1])


EXP_FILE =  OUT_DIR / "experiments.json"                    # maps experiment file -> configuration
CONF_FILE = OUT_DIR / "configs.json"                        # single experiments grouped by APs and interferents setup
MAP_DIR = OUT_DIR / "maps"
PLOT_DIR = OUT_DIR / "plots"


def extract_avg_power(data):
    avg_power = sum(map(
        lambda row: sum(map(
                    lambda t: t["tx_power_w"] * t["tx_time"],
                    row["transmissions"]
                )),
        data))
    return avg_power / 10**6 / 30000

def extract_rate(data):
    avg_rate = statistics.mean(
        itertools.chain.from_iterable(
            map(
                lambda row: [t["rate"] for t in row["transmissions"]],
                data
            )
        )
    )
    return avg_rate

def remove_dropped(data):
    return [row for row in data if row["acked"] == True]


# metrics for maps
METRICS = {
    "latency_avg" : {
        "label": "latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: statistics.mean(map(lambda x: x["latency"], remove_dropped(data)))
    },
    "latency_stdev" : {
        "label": "latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: statistics.stdev(map(lambda x: x["latency"], remove_dropped(data)))
    },
    "latency_min" : {
        "label": "latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: min(map(lambda x: x["latency"], remove_dropped(data)))
    },
    "latency_10_perc" : {
        "label": "latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: np.percentile(np.array(list(map(lambda x: x["latency"], remove_dropped(data)))), 10)
    },
    "latency_99_perc" : {
        "label": "latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: np.percentile(np.array(list(map(lambda x: x["latency"], remove_dropped(data)))), 99)
    },
    "latency_99.9_perc" : {
        "label": "latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: np.percentile(np.array(list(map(lambda x: x["latency"], remove_dropped(data)))), 99.9)
    },
    "retransmission_count" : {
        "label": "# retransmissions",
        "scaling": 1,
        "fnc": lambda data: sum(map(lambda x: len(x["transmissions"]) - 1, remove_dropped(data)))
    },
    "power_avg": {
        "label": "power (mW)",
        "scaling": 1,
        "fnc": extract_avg_power
    },
    "rate_avg": {
        "label": "data rate (bit/s)",
        "scaling": 1,
        "fnc": extract_rate
    },
    "%_dropped": {
        "label": "% dropped",
        "scaling": 1,
        "fnc": lambda data: len([row for row in data if row["acked"] == False]) / len(data) * 100
    }
}

# min
# 10th percentile
# filter dropped packets

# add power metrics

def extract_headers(exp_dir, fname):
    exp_dir = Path(exp_dir)
    fname = Path(fname)

    paths = sorted([p for p in exp_dir.iterdir() if p.is_file() and p.suffix == EXP_FILE_SUFFIX])
    paths = sorted(paths, key=EXTRACT_EXP_NUM)

    conf_dict = {}
    errors = []
    for p in paths:
        with p.open() as f:       
            try:        
                line = f.readlines(2)[1].strip()[:-1]
                content = json.loads(line)
                conf_dict[p.name] = content
                print(p.name) 
            except Exception:
                conf_dict[p.name] = None
                errors.append(p)
                print("error: {}".format(p.name))

    print([p.name for p in errors])
    print([k for k, v in conf_dict.items() if v is None])

    fname.parent.absolute().mkdir(parents=True, exist_ok=True)
    with open(fname, "w") as f:
        json.dump(conf_dict, f, indent=4)


def group_configurations(exp_file, conf_file):
    with open(exp_file) as f:
        experiments = json.load(f)

    # remove experiments with empty files
    c2f = copy.deepcopy(experiments)
    c2f = {file: conf for file, conf in c2f.items() if conf is not None}

    # remove position from experiment configuration
    for conf in c2f.values():
        conf["staNode"].pop("position")

    # convert configuration of each experiment to unique string
    c2f = {file: json.dumps(conf, sort_keys=True, separators=None) for file, conf in c2f.items()}

    # group experiment files for configuration string
    configs = {conf: [] for conf in c2f.values()}
    for file, conf in c2f.items():
        configs[conf].append(file)

    # output number of different configurations
    print(len(configs))

    # extract STA position for each experiment file
    file_to_pos = {file: conf["staNode"]["position"] for file, conf in experiments.items() if conf is not None}

    # list of experiment configurations, including file and STA position for each file
    configs = [{"config": json.loads(conf_str), "files": {f: file_to_pos[f] for f in files}} for conf_str, files in configs.items()]

    with open(conf_file, "w") as f:
        json.dump(configs, f, indent=4)


def export_map_1d(conf_file, exp_dir, map_dir):
    with open(conf_file) as f:
        configs = json.load(f)

    exp_dir = Path(exp_dir)
    map_dir = Path(map_dir)

    map_dir.mkdir(parents=True, exist_ok=True)
    
    for idx, conf in enumerate(configs):
        df_metrics = pd.DataFrame(columns = ["x_pos"] + [m for m in METRICS])
        files = conf["files"]

        for file, pos in files.items():
            print(file)
            path = exp_dir / file

            with open(path) as f:
                exp_data = json.load(f)

            exp_data = exp_data[1:-1]

            new_row = {c: None for c in df_metrics.columns}
            new_row["x_pos"] = int(pos["x"])

            for m in METRICS:
                new_row[m] = METRICS[m]["fnc"](exp_data) if len(exp_data) > 0 else None
            df_metrics.loc[len(df_metrics)] = new_row

        df_metrics.to_csv(map_dir / "map_{:02}.csv".format(idx), index=False)


def plot_maps_1d(conf_file, map_dir, plot_dir):
    with open(conf_file) as f:
        configs = json.load(f)

    map_dir = Path(map_dir)
    plot_dir = Path(plot_dir)
    
    for metric in METRICS:
        new_dir = plot_dir / metric
        new_dir.mkdir(parents=True, exist_ok=True)

    paths = sorted([p for p in (map_dir).iterdir()])

    for map_path in paths:
        df_metrics = pd.read_csv(map_path)
        idx = int(map_path.name.split(".")[0].split("_")[1])
        print(idx)

        for metric in METRICS:
            fig, ax = plt.subplots()
            ax.plot(df_metrics["x_pos"], df_metrics[metric]*METRICS[metric]["scaling"])
            ax.set_title(metric, fontsize=20)
            ax.set_xlabel("STA position", fontsize=16)#, rotation=-90, va="bottom")
            ax.set_ylabel(METRICS[metric]["label"], fontsize=16)#, rotation=-90, va="bottom")
            ax.tick_params(axis='both', which='major', labelsize = 14)
            plt.tight_layout()
            fig.savefig(plot_dir / metric / "map_{:02}.png".format(idx))
            plt.close(fig)    


def export_latency_maps(conf_file, exp_dir, map_dir):
    with open(conf_file) as f:
        configs = json.load(f)

    exp_dir = Path(exp_dir)
    map_dir = Path(map_dir)

    for metric in METRICS:
        new_dir = map_dir / metric
        new_dir.mkdir(parents=True, exist_ok=True)

    for idx, conf in enumerate(configs):
        files = conf["files"]
        x_pos = list(map(lambda pos: pos["x"], files.values()))
        y_pos = list(map(lambda pos: pos["y"], files.values()))

        min_x, max_x = min(x_pos), max(x_pos)
        min_y, max_y = min(y_pos), max(y_pos)

        step = 2.5 if max_y > 0 else 1

        x_size = int((max_x - min_x) / step) + 1
        y_size = int((max_y - min_y) / step) + 1

        map_dict = {m: np.empty((x_size, y_size,)) for m in METRICS}
        for m in map_dict.values():
            m.fill(np.nan)

        for file, pos in files.items():
            print(file)
            path = exp_dir / file

            with open(path) as f:
                exp_data = json.load(f)

            exp_data = exp_data[1:-1]

            x = int((pos["x"] - min_x)/step)
            y = int((pos["y"] - min_y)/step)

            for metric, metric_map in map_dict.items():
                metric_map[x, y] = METRICS[metric]["fnc"](exp_data) if len(exp_data) > 0 else -1                
            
        for metric, metric_map in map_dict.items():
            fname = map_dir / metric / "map_{:02}.csv".format(idx)
            np.savetxt(fname, metric_map, delimiter=",")

    #print(latency_map)


def plot_lantency_maps(conf_file, map_dir, plot_dir):
    with open(conf_file) as f:
        configs = json.load(f)

    map_dir = Path(map_dir)
    plot_dir = Path(plot_dir)
    
    metrics = [d.name for d in map_dir.iterdir()]
    for metric in metrics:
        new_dir = plot_dir / metric
        new_dir.mkdir(parents=True, exist_ok=True)

    for metric in metrics:
        paths = sorted([p for p in (map_dir/metric).iterdir()])

        for map_path in paths:
            my_data = np.genfromtxt(map_path, delimiter=',')
            if len(my_data.shape) < 2:
                padding = np.empty(my_data.shape)
                padding.fill(np.nan)
                my_data = np.stack([my_data, padding], axis=1)        

            conf_num = int(map_path.name.split(".")[0].split("_")[1])
            print(conf_num)

            # step = 1 if conf_num < 9 else 2.5
            # x_min = 0 if conf_num < 9 else -50
            # y_min = 0 if conf_num < 9 else -50

            step = 2.5
            x_min = -50
            y_min = -50

            # data transformation (TO MOVE)
            mask = (my_data[:, :] < 0)# set -1 to nan
            my_data[mask] = np.nan
            my_data = my_data * METRICS[metric]["scaling"] # scale to microseconds for latency

            ap_pos = []
            for ap in configs[conf_num]["config"]["apNodes"]:
                pos = (ap["position"]["x"]-x_min)/step, (ap["position"]["y"]-y_min)/step
                ap_pos.append(pos)

            interf_pos = []
            for interf in configs[conf_num]["config"]["interfererNodes"]:
                pos = (interf["position"]["x"]-x_min)/step, (interf["position"]["y"]-y_min)/step
                interf_pos.append(pos)

            print(ap_pos)
            print(interf_pos)

            # x_labels = ["{}".format(2.5 * i) for i in range(0, 41, 1)]
            # y_labels = x_labels[::-1]

            fig, ax = plt.subplots()
            im = ax.imshow(my_data, cmap="Blues_r")

            cbar = ax.figure.colorbar(im, ax=ax)
            cbar.ax.set_ylabel(METRICS[metric]["label"], rotation=-90, va="bottom")

            #ax.scatter([24], [35], color = "r")
            ax.scatter([p[1] for p in ap_pos], [p[0] for p in ap_pos], color = "red")
            ax.scatter([p[1] for p in interf_pos], [p[0] for p in interf_pos], color = "orange")        

            # # Show all ticks and label them with the respective list entries
            # ax.set_xticks(np.arange(len(x_labels)), labels=x_labels)
            # ax.set_yticks(np.arange(len(y_labels)), labels=y_labels)

            # # Rotate the tick labels and set their alignment.
            # plt.setp(ax.get_xticklabels(), rotation=90, ha="right",
            #         rotation_mode="anchor")

            # # Loop over data dimensions and create text annotations.
            # for i in range(len(y_labels)):
            #     for j in range(len(x_labels)):
            #         text = ax.text(j, i, my_data[i, j],
            #                     ha="center", va="center", color="w")

            #ax.set_title("Latency map")
            fig.tight_layout()
            #plt.show()

            fname = plot_dir / metric / "map_{:02}.png".format(conf_num)
            fig.savefig(fname)
            plt.close(fig)
        

#%%
def main():
    extract_headers(EXP_DIR, EXP_FILE) # extract experiment configuration for each file

    group_configurations(EXP_FILE, CONF_FILE)  # group single experiments by APs and interferents setup

    export_map_1d(CONF_FILE, EXP_DIR, MAP_DIR)

    plot_maps_1d(CONF_FILE, MAP_DIR, PLOT_DIR)
    
    
    # export_latency_maps(CONF_FILE, EXP_DIR, MAP_DIR)

    # plot_lantency_maps(CONF_FILE, MAP_DIR, PLOT_DIR)

    return 0


if __name__ == "__main__":
    main()

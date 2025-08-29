# %%
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path
from metrics import METRICS
import seaborn as sns

# %%
plt.style.use('seaborn-v0_8-paper')
sns.set_theme(style="whitegrid")

# %%
OUT_FOLDER = Path("/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/etfa_wip_sim_maps/plots_png")
OUT_FOLDER.mkdir(parents=True, exist_ok=True)

OUT_FORMAT = "png" #"pdf"

PLOT_METRICS = [
    "latency_avg",
    "latency_min",
    "latency_10_perc",
    "latency_99_perc",
    "latency_99.9_perc",
    "transmission_num_avg",
    "transmission_num_max_no_dropped",
    "%_dropped"
]

EXP_INFO = {
    # "optimal" : {
    #     "folder": "/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/etfa_wip_sim_maps/constant",
    #     "files" : {
    #         "no_int": "map_00.csv",
    #         "hidden": "map_01.csv",
    #         "visible": "map_02.csv"
    #     }
    # },
    "static" : {
        "folder": "/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/wfcs_sim_maps/maps",
        "files" : {
            "no_int": "map_00.csv",
            "hidden": "map_01.csv",
            "visible": "map_03.csv"
        }
    },
    "mobility" : {
        "folder": "/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/etfa_wip_sim_maps/mobility",
        "files" : {
            "no_int": {
                "fast": "map_00.csv",
                "medium": "map_01.csv",
                "slow": "map_02.csv",
            },
            "hidden": {
                "fast": "map_03.csv",
                "medium": "map_04.csv",
                "slow": "map_05.csv",
            },
            "visible": {
                "fast": "map_06.csv",
                "medium": "map_07.csv",
                "slow": "map_08.csv",
            }
        }
    }
}


# %%
for conf_interf in ["no_int", "hidden", "visible"]:
    for metric in PLOT_METRICS:
        fig, ax = plt.subplots()
        for exp in  EXP_INFO:
            folder = EXP_INFO[exp]["folder"]
            if exp != "mobility":
                fname = Path(EXP_INFO[exp]["folder"]) / EXP_INFO[exp]["files"][conf_interf]
                df = pd.read_csv(fname)
                df = df[df["x_pos"] > 0]
                p1, = ax.plot(df["x_pos"], df[metric]*METRICS[metric]["scaling"], label = exp)
            else:
                for sta_speed in ["slow", "medium", "fast"]:
                    fname = Path(EXP_INFO[exp]["folder"]) / EXP_INFO[exp]["files"][conf_interf][sta_speed]
                    df = pd.read_csv(fname)
                    df = df[df["x_pos"] > 0]
                    p1, = ax.plot(df["x_pos"], df[metric]*METRICS[metric]["scaling"], label = "{}_{}".format(exp, sta_speed))
        ax.set_xlabel("SUT position $D_\mathrm{S}$ (m)", fontsize=16)#, rotation=-90, va="bottom")
        ax.set_ylabel(METRICS[metric]["label"], fontsize=16)#, rotation=-90, va="bottom")
        ax.tick_params(axis='both', which='major', labelsize = 14)
        ax.legend(prop={'size': 12})
        plt.tight_layout()
        fig.savefig(OUT_FOLDER / "{}_{}.{}".format(conf_interf, metric, OUT_FORMAT))
        plt.close(fig)


# %%

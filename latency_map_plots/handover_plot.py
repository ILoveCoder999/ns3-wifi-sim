# %%
import json
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path

# %%
BASE_DIR = "tii_sim_02"
EXP_DIR = "sim_res"
IMG_FORMAT = "png"

# %%
plot_dir = Path(BASE_DIR) / "plots" / EXP_DIR
plot_dir.mkdir(parents=True, exist_ok=True)

# %%
class NoDataException(Exception):
    pass

# %%
with open("{}/data/{}/db_0_assoc.json".format(BASE_DIR, EXP_DIR)) as f:
    assoc_log = json.load(f)

with open("{}/data/{}/db_0_stalog.json".format(BASE_DIR, EXP_DIR)) as f:
    sta_log = json.load(f)

header = sta_log[0]
sta_log = sta_log[1:-1]
assoc_log = assoc_log[1:]

# %%
print(header)
x_pos_ap = tuple(pos["x"] for pos in header["apPositions"])
x_pos_int = tuple(pos["position"]["x"] for pos in header["interferers"])
print(x_pos_ap)
print(x_pos_int)

# %%
POS_ROUNDING_SEP = 0
POS_ROUNDING_TOT = 0
POS_ROUNDING_BC = 0

ASSOC_INTERVAL = 5

# %%
rows_sta = [
    {
        "tx_time": r["transmissions"][0]["tx_time"] if r["transmissions"] else None,
        "pos": r["transmissions"][0]["position"][0] if r["transmissions"] else None,
        "latency": r["latency"]/1000,
        "acked": r["acked"],
        "num_trans": len(r["transmissions"]),
        "ap": "ap1" if r["addr_1"] == "00:00:00:00:00:02" else "ap2"
    } for r in sta_log
]
df_sta = pd.DataFrame.from_dict(rows_sta, orient="columns")
print(df_sta)

# %%
rows_bc = assoc_log[1:]
rows_bc = [r for r in rows_bc if r["msg"] == "BeaconInfo"]
rows_bc = [
    {
        "tx_time": r["tx_time"], 
        "pos": r["position"][0],
        "ap": "ap1" if r["ap_info"]["apAddr"] == "00:00:00:00:00:02" else "ap2",
        "snr": r["ap_info"]["snr"]
    } for r in rows_bc
]
df_bc = pd.DataFrame.from_dict(rows_bc, orient="columns")
print(df_bc)

# %%
rows_ass = assoc_log[1:]
rows_ass = [r for r in rows_ass if r["msg"] in {"Association", "De-association"} and r["tx_time"] > 10**9]
rows_ass = [
    {
        "tx_time": r["tx_time"], 
        "pos": r["position"][0],
        "msg": r["msg"],
        "ap": "ap1" if r["ap_info"] == "00:00:00:00:00:02" else "ap2"
    } for r in rows_ass
]
df_ass = pd.DataFrame.from_dict(rows_ass, orient="columns")
print(df_ass)

# %%
acked_agg = {
    "latency_mean" : ('latency', 'mean'),
    "latency_99" : ('latency', lambda x: np.percentile(x, 99)),
    "latency_99_9" : ('latency', lambda x: np.percentile(x, 99.9)),
    "trans_num_mean" : ('num_trans', 'mean')
}
droppped_agg = {
    "drop_perc" : ('acked', lambda x: len(x[x==False]) / len(x))
}

# %%
df_sta_tot = pd.DataFrame(df_sta)
df_sta_tot["pos"] = df_sta_tot["pos"].round(POS_ROUNDING_TOT)
df_ack = df_sta_tot[df_sta_tot["acked"] == True].groupby(['pos']).agg(**acked_agg).reset_index()
df_drop = df_sta_tot.groupby(['pos']).agg(**droppped_agg).reset_index()
df_tot = pd.merge(df_ack, df_drop, on = ["pos"])
print(df_tot)

# %%
df_sta_sep = pd.DataFrame(df_sta)
df_sta_sep["pos"] = df_sta_sep["pos"].round(POS_ROUNDING_SEP)
df_ack = df_sta_sep[df_sta_sep["acked"] == True].groupby(['pos', "ap"]).agg(**acked_agg).reset_index()
df_drop = df_sta_sep.groupby(['pos', "ap"]).agg(**droppped_agg).reset_index()
df_g = pd.merge(df_ack, df_drop, on = ["pos", "ap"])
df_g1 = df_g[df_g["ap"] == "ap1"].reset_index(drop=True)
df_g2 = df_g[df_g["ap"] == "ap2"].reset_index(drop=True)
print(df_g1)
print(df_g2)

# %%
df_ass["pos_group"] = df_ass["pos"] // ASSOC_INTERVAL
df_ass = df_ass.groupby(['msg', "ap", "pos_group"]).agg(pos = ('pos', 'mean')).reset_index()
a_lines = []
d_lines = []
for index, row in df_ass.iterrows():
    if row["msg"] == "Association":
        a_lines.append(row["pos"])
    else:
        d_lines.append(row["pos"])

def plot_assoc_deassoc(ax):
    for p in a_lines:
        ax.axvline(x=p, color="green")
    for p in d_lines:
        ax.axvline(x=p, color="red")

def plot_int_aps(ax, aps, int):
    xlim = ax.get_xlim()
    ylim = ax.get_ylim()

    ax.axvspan(max(aps[0]-51, xlim[0]), min(aps[0]+51, xlim[1]), color='C0', alpha=0.2, lw=0)
    ax.axvspan(max(aps[1]-51, xlim[0]), min(aps[1]+51, xlim[1]), color='C1', alpha=0.2, lw=0)    

    ax.plot(aps[0], ylim[0], 'o', color='C0', zorder=10, clip_on=False, markersize=10)
    ax.plot(aps[1], ylim[0], 'o', color='C1', zorder=10, clip_on=False, markersize=10)

    for i in int:
        ax.plot(i, ylim[0], 'o', color='r', zorder=10, clip_on=False, markersize=10)
        ax.plot([max(i-51, xlim[0]), min(i+51, xlim[1])], [ylim[0], ylim[0]],   color='r', zorder=9, alpha=0.4, clip_on=False, linewidth=6)

    ax.set_xlim(xlim)
    ax.set_ylim(ylim)

# %%
fig, ax = plt.subplots()
ax.plot(df_g1["pos"], df_g1["latency_mean"], label = "AP1 - Avg. latency")
ax.plot(df_g2["pos"], df_g2["latency_mean"], label = "AP2 - Avg. latency")
#ax.plot(df_tot["pos"], df_tot["latency_mean"])
plot_assoc_deassoc(ax)

ax.set_xlabel("SUT position $D_\mathrm{S}$ (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel("Latency ($\mu$s)", fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)
ax.legend(prop={'size': 12})
plt.tight_layout()

plot_int_aps(ax, x_pos_ap, x_pos_int)

fig.savefig(plot_dir / "latency_avg.{}".format(IMG_FORMAT))

# %%
fig, ax = plt.subplots()
ax.plot(df_g1["pos"], df_g1["latency_99"], label = "AP1 - 99 perc. latency")
ax.plot(df_g2["pos"], df_g2["latency_99"], label = "AP2 - 99 perc. latency")
#ax.plot(df_tot["pos"], df_tot["latency_99"])
plot_assoc_deassoc(ax)

ax.set_xlabel("SUT position $D_\mathrm{S}$ (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel("Latency ($\mu$s)", fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)
ax.legend(prop={'size': 12})
plt.tight_layout()

plot_int_aps(ax, x_pos_ap, x_pos_int)

fig.savefig(plot_dir / "latency_99perc.{}".format(IMG_FORMAT))

# %%
fig, ax = plt.subplots()
ax.plot(df_g1["pos"], df_g1["latency_99_9"], label ="AP1 - 99.9 perc. latency")
ax.plot(df_g2["pos"], df_g2["latency_99_9"], label ="AP2 - 99.9 perc. latency")
#ax.plot(df_tot["pos"], df_tot["latency_99_9"])
plot_assoc_deassoc(ax)

ax.set_xlabel("SUT position $D_\mathrm{S}$ (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel("Latency ($\mu$s)", fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)
ax.legend(prop={'size': 12})
plt.tight_layout()

plot_int_aps(ax, x_pos_ap, x_pos_int)

fig.savefig(plot_dir / "latency_99.9perc.{}".format(IMG_FORMAT))

# %%
fig, ax = plt.subplots()
ax.plot(df_g1["pos"], df_g1["trans_num_mean"], label = "AP1 - Avg. attempts")
ax.plot(df_g2["pos"], df_g2["trans_num_mean"], label = "AP2 - Avg. attempts")
#ax.plot(df_tot["pos"], df_tot["trans_num_mean"])
#plot_assoc_deassoc(ax)

ax.set_xlabel("SUT position $D_\mathrm{S}$ (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel("# Transmissions", fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 16)
ax.legend(prop={'size': 12})
plt.tight_layout()

plot_int_aps(ax, x_pos_ap, x_pos_int)

fig.savefig(plot_dir / "attempts_avg.{}".format(IMG_FORMAT))

# %%
fig, ax = plt.subplots()
ax.plot(df_g1["pos"], df_g1["drop_perc"], label="AP1 - % dropped")
ax.plot(df_g2["pos"], df_g2["drop_perc"], label="AP2 - % dropped")
#ax.plot(df_tot["pos"], df_tot["drop_perc"])
plot_assoc_deassoc(ax)

ax.set_xlabel("SUT position $D_\mathrm{S}$ (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel("Dropped (%)", fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)
ax.legend(prop={'size': 12})
plt.tight_layout()

plot_int_aps(ax, x_pos_ap, x_pos_int)

fig.savefig(plot_dir / "dropped_%.{}".format(IMG_FORMAT))

# %%
df_bc["pos"] = df_bc["pos"].round(POS_ROUNDING_BC)
df = df_bc.loc[:, df_bc.columns != 'tx_time']
df_bc = df.groupby(["ap", "pos"]).mean().reset_index()
print(df_bc)

# %%
fig, ax = plt.subplots()
df_bc_1 = df_bc[df_bc["ap"] == "ap1"]
ax.semilogy(df_bc_1["pos"], df_bc_1["snr"], label = "AP1 - SNR")

df_bc_2 = df_bc[df_bc["ap"] == "ap2"]
ax.semilogy(df_bc_2["pos"], df_bc_2["snr"], label = "AP2 - SNR")

ax.set_xlabel("SUT position $D_\mathrm{S}$ (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel("SNR", fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)
ax.legend(prop={'size': 12})
plt.tight_layout()

plot_int_aps(ax, x_pos_ap, x_pos_int)

fig.savefig(plot_dir / "snr.{}".format(IMG_FORMAT))

# %%
# X make spatial aggregation precision selectable (round digits)
# X make one dataset with all metrics combined
# X add vertical lines of assoc/deassoc (grouped by type and target AP and averaged)
# X overlay overall metrics (not good)
# X save figures
# X test association swith by just changing STA channel with timer
# X enable channel roaming

# - do some tests (distant APs, separate channels etc...)
#   - for now strange behavior with interferent at x=120 (probably ack interference)
#   - change simulation details (AP position, packet interval, speed)
# needed to change interferent packet size (note that and understand exactly why)
# - add pcap to interferents
# - sim configuration script
#   - complete conf data stuctures
# - add interferers
# - implement roaming strategies (simulator???)

# %%

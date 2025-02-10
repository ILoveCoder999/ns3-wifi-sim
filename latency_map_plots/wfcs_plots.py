# %%
from pathlib import Path
import pandas as pd
from metrics import METRICS
import matplotlib.pyplot as plt
import seaborn as sns
import ast
import numpy as np


plt.style.use('seaborn-v0_8-paper')
sns.set_theme(style="whitegrid")

BASE_FOLDER = Path("/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/wfcs_sim_maps")
MAP_FOLDER = BASE_FOLDER / "maps"
OUT_FOLDER = BASE_FOLDER / "article"

OUT_FOLDER.mkdir(parents=True, exist_ok=True)

# %%
df_0 = pd.read_csv(Path(MAP_FOLDER) / "map_00.csv")
df_0 = df_0[1:].reset_index(drop=True)
fig_dict = {}

# %% LATENCY (AVG, MIN) + TX NUM
fig, ax = plt.subplots()
ax2 = ax.twinx()
# ax.grid(True)
# ax2.grid(True)

p1, = ax.plot(df_0["x_pos"], df_0["latency_avg"]*METRICS["latency_avg"]["scaling"], "C0", label = "Avg. latency")
p2, = ax.plot(df_0["x_pos"], df_0["latency_min"]*METRICS["latency_min"]["scaling"], "C1", label = "Min. latency")
p3, = ax2.plot(df_0["x_pos"], df_0["transmission_num_avg"]*METRICS["transmission_num_avg"]["scaling"], "C2", label="Avg. tx attempts")

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["latency_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax2.set_ylabel(METRICS["transmission_num_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax2.legend(handles=[p1, p2, p3], prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "00-latency_avg_min-tx_num_avg.png")

fig_dict["latency_tx_avg"] = fig


# %% LATENCY 99 and 99.9 perc
fig, ax = plt.subplots()

p1, = ax.plot(df_0["x_pos"], df_0["latency_99_perc"]*METRICS["latency_99_perc"]["scaling"], "C0", label = "99 perc.")
p2, = ax.plot(df_0["x_pos"], df_0["latency_99.9_perc"]*METRICS["latency_99.9_perc"]["scaling"], "C1", label = "99.9 perc.")

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["latency_99_perc"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "00_latency_99_99.9_perc.png")

fig_dict["percentile"] = fig

# %% POWER + RATE
fig, ax = plt.subplots()
ax2 = ax.twinx()
# ax.grid(True)
# ax2.grid(True)

p1, = ax.plot(df_0["x_pos"], df_0["power_avg"]*METRICS["power_avg"]["scaling"], "C0", label = "Avg. power")
p2, = ax2.plot(df_0["x_pos"], df_0["rate_avg"]*METRICS["rate_avg"]["scaling"], "C2", label="Avg. data rate")

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["power_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax2.set_ylabel(METRICS["rate_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax2.legend(handles=[p1, p2],  loc = "center left", prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "00-power-rate.png")

fig_dict["power_rate"] = fig

# %% DROPPED + TX MAX
fig, ax = plt.subplots()
ax2 = ax.twinx()

p1, = ax.plot(df_0["x_pos"], df_0["%_dropped"]*METRICS["%_dropped"]["scaling"], "C0", label = "% Dropped packets")
p2, = ax2.plot(df_0["x_pos"], df_0["transmission_num_max_no_dropped"]*METRICS["transmission_num_max_no_dropped"]["scaling"], "C2", label="Max attempts")

#ax.set_title(metric, fontsize=20)

ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["%_dropped"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylim([-10, 110])
ax.tick_params(axis='both', which='major', labelsize = 14)

ax2.set_ylabel(METRICS["transmission_num_max_no_dropped"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(handles=[p1, p2], loc="upper left", prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "00-dropped-tx_max.png")

fig_dict["dropped_tx_max"] = fig


# %% RATE DIST
fig, ax = plt.subplots()

xy_series = []

rows = [ast.literal_eval(row) for row in df_0["rate_distribution"]]
x_points = df_0["x_pos"]
y_points = [0] * len(rows)
#ax.plot(x_points, y_points, color = "white")
xy_series.append((np.array(x_points), np.array(y_points)))

for c_idx, c in enumerate(METRICS["rate_distribution"]["classes"]):
    y_values = [row[c_idx]*METRICS["rate_distribution"]["scaling"] for row in rows]
    x_points = df_0["x_pos"]
    y_points = [p + n for p, n in zip(y_points, y_values)]
    ax.plot(x_points, y_points, color = "white")
    xy_series.append((np.array(x_points), np.array(y_points)))

fills = []
labels = ["{} Mbps".format(str(c)) for c in METRICS["rate_distribution"]["classes"]]
for idx, (xy_prev, xy_next) in enumerate(zip(xy_series[:-1], xy_series[1:])):
    p = ax.fill_between(xy_prev[0], xy_prev[1], xy_next[1], where=(xy_prev[1] < xy_next[1]), alpha=0.3, label = labels[idx])
    fills.append(p)

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["rate_distribution"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "00-rate_dist.png")

fig_dict["rate_dist"] = fig

# %% SUCCESS RATE
fig, ax = plt.subplots()

rows = [ast.literal_eval(row) for row in df_0["rate_succ_distribution"]]
print(rows)
for c_idx, c in enumerate(METRICS["rate_succ_distribution"]["classes"]):
    ax.plot(df_0["x_pos"], [row[c_idx]*METRICS["rate_succ_distribution"]["scaling"] for row in rows], label = "{} Mbps".format(str(c)))
    #xy_series.append((np.array(x_points), np.array(y_points)))


#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["rate_succ_distribution"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(prop={'size': 12}, loc="center left")

plt.tight_layout()
fig.savefig(OUT_FOLDER / "00-rate_success.png")

fig_dict["success_rate_dist"] = fig

# %% RATE LATENCY DISTRIBUTION
fig, ax = plt.subplots()

xy_series = []

rows = [ast.literal_eval(row) for row in df_0["rate_latency_distribution"]]
x_points = df_0["x_pos"]
y_points = [0] * len(rows)
#ax.plot(x_points, y_points, color = "white")
xy_series.append((np.array(x_points), np.array(y_points)))

for c_idx, c in enumerate(METRICS["rate_latency_distribution"]["classes"]):
    y_values = [row[c_idx]*METRICS["rate_latency_distribution"]["scaling"] for row in rows]
    x_points = df_0["x_pos"]
    y_points = [p + n for p, n in zip(y_points, y_values)]
    ax.plot(x_points, y_points, color = "white")
    xy_series.append((np.array(x_points), np.array(y_points)))

fills = []
labels = ["{} Mbps".format(str(c)) for c in METRICS["rate_latency_distribution"]["classes"]]
for idx, (xy_prev, xy_next) in enumerate(zip(xy_series[:-1], xy_series[1:])):
    p = ax.fill_between(xy_prev[0], xy_prev[1], xy_next[1], where=(xy_prev[1] < xy_next[1]), alpha=0.3, label = labels[idx])
    fills.append(p)

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["rate_latency_distribution"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "00-rate_latency.png")

fig_dict["rate_latency_dist"] = fig

# %% RATE LATENCY DISTRIBUTION with final
fig, ax = plt.subplots()

xy_series = []

rows = [ast.literal_eval(row) for row in df_0["rate_latency_distribution_with_final"]]
x_points = df_0["x_pos"]
y_points = [0] * len(rows)
#ax.plot(x_points, y_points, color = "white")
xy_series.append((np.array(x_points), np.array(y_points)))

for c_idx, c in enumerate(METRICS["rate_latency_distribution_with_final"]["classes"]):
    y_values = [row[c_idx]*METRICS["rate_latency_distribution_with_final"]["scaling"] for row in rows]
    x_points = df_0["x_pos"]
    y_points = [p + n for p, n in zip(y_points, y_values)]
    ax.plot(x_points, y_points, color = "white")
    xy_series.append((np.array(x_points), np.array(y_points)))

fills = []
labels = ["{} Mbps".format(str(c)) for c in METRICS["rate_latency_distribution_with_final"]["classes"]]
for idx, (xy_prev, xy_next) in enumerate(zip(xy_series[:-1], xy_series[1:])):
    p = ax.fill_between(xy_prev[0], xy_prev[1], xy_next[1], where=(xy_prev[1] < xy_next[1]), alpha=0.3, label = labels[idx])
    fills.append(p)

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["rate_latency_distribution_with_final"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "00-rate_latency_with_final.png")

fig_dict["rate_latency_with_final_dist"] = fig


# %%
comp_folder = OUT_FOLDER / "comparison"
comp_folder.mkdir(parents=True, exist_ok=True)

array = list(zip(df_0["x_pos"], df_0["latency_avg"]))

local_max = [array[i][0] for i in range(len(array[1:-1])) if array[i][1] > array[i-1][1] and  array[i][1] > array[i+1][1]]
local_min = [array[i][0] for i in range(len(array[1:-1])) if array[i][1] < array[i-1][1] and  array[i][1] < array[i+1][1]]

print(local_max)

print(array)

fig_to_compare = {'latency_tx_avg', 'rate_dist', 'success_rate_dist', 'rate_latency_dist', 'rate_latency_with_final_dist'}

for name in fig_to_compare:
    fig = fig_dict[name]
    ax = fig.get_axes()[0]
    ymin, ymax = ax.get_ylim()
    print(ymin, ymax)
    ax.vlines(x=local_max, ymin=ymin, ymax=ymax, colors=['tab:red'], ls='-', lw=.5)
    ax.vlines(x=local_min, ymin=ymin, ymax=ymax, colors=['tab:green'], ls='-', lw=.5)
    fig.savefig(comp_folder / f"{name}.png")



# ymin, ymax = g.get_ylim()

# # vertical lines
# g.vlines(x=[c_max, s_max], ymin=ymin, ymax=ymax, colors=['tab:orange', 'tab:blue'], ls='--', lw=2)
# %%
print(df_0["power_avg"].corr(df_0["rate_avg"]))
print(df_0["power_avg"].corr(df_0["transmission_num_avg"]))
print(df_0["latency_avg"].corr(df_0["transmission_num_avg"]))

# %%
for fig in fig_dict.values():
    plt.close(fig)



# %%
df_1 = pd.read_csv(Path(MAP_FOLDER) / "map_01.csv")
df_3 = pd.read_csv(Path(MAP_FOLDER) / "map_03.csv")
df_1 = df_1[1:].reset_index(drop=True)
df_3 = df_3[1:].reset_index(drop=True)
fig_dict = {}

# %% LATENCY (AVG, MIN) + TX NUM
fig, ax = plt.subplots()
ax2 = ax.twinx()
# ax.grid(True)
# ax2.grid(True)

p1, = ax.plot(df_3["x_pos"], df_3["latency_avg"]*METRICS["latency_avg"]["scaling"], "C0", label = "Avg. latency")
p2, = ax.plot(df_1["x_pos"], df_1["latency_avg"]*METRICS["latency_avg"]["scaling"], "C0", label = "Avg. latency (hidden)", linestyle="--")

p3, = ax2.plot(df_3["x_pos"], df_3["transmission_num_avg"]*METRICS["transmission_num_avg"]["scaling"], "C1", label="Avg. attempts")
p4, = ax2.plot(df_1["x_pos"], df_1["transmission_num_avg"]*METRICS["transmission_num_avg"]["scaling"], "C1", label="Avg. attempts (hidden)", linestyle="--")


#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["latency_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax2.set_ylabel(METRICS["transmission_num_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax2.legend(handles=[p1, p2, p3, p4], prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "01_03-latency_avg-tx_num_avg.png")

fig_dict["latency_tx_avg"] = fig

# %% 99.9 perc, % dropped
fig, ax = plt.subplots()
ax2 = ax.twinx()
# ax.grid(True)
# ax2.grid(True)

p1, = ax.plot(df_3["x_pos"], df_3["latency_99.9_perc"]*METRICS["latency_99.9_perc"]["scaling"], "C0", label = "99.9 perc.")
p2, = ax.plot(df_1["x_pos"], df_1["latency_99.9_perc"]*METRICS["latency_99.9_perc"]["scaling"], "C0", label = "99.9 perc. (hidden)", linestyle="--")

p3, = ax2.plot(df_3["x_pos"], df_3["%_dropped"]*METRICS["%_dropped"]["scaling"], "C1", label="% dropped")
p4, = ax2.plot(df_1["x_pos"], df_1["%_dropped"]*METRICS["%_dropped"]["scaling"], "C1", label="% dropped (hidden)", linestyle="--")

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["latency_99_perc"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax2.set_ylabel(METRICS["%_dropped"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax2.legend(handles=[p1, p2, p3, p4], prop={'size': 12})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "01_03-99.9_perc-dropped.png")

fig_dict["99.9_perc-dropped"] = fig

# %% POWER - RATE
fig, ax = plt.subplots()
ax2 = ax.twinx()
# ax.grid(True)
# ax2.grid(True)

p1, = ax.plot(df_3["x_pos"], df_3["power_avg"]*METRICS["power_avg"]["scaling"], "C0", label = "Avg. power")

p2, = ax.plot(df_1["x_pos"], df_1["power_avg"]*METRICS["power_avg"]["scaling"], "C0", label = "Avg. power (hidden)", linestyle="--")

p3, = ax2.plot(df_3["x_pos"], df_3["rate_avg"]*METRICS["rate_avg"]["scaling"], "C2", label="Avg. data rate")
p4, = ax2.plot(df_1["x_pos"], df_1["rate_avg"]*METRICS["rate_avg"]["scaling"], "C2", label="Avg. data rate (hidden)", linestyle="--")

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("SUT position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["power_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax2.set_ylabel(METRICS["rate_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax2.legend(handles=[p1, p2, p3, p4], prop={'size': 12}, bbox_to_anchor=(0.605,0.8))

plt.tight_layout()
fig.savefig(OUT_FOLDER / "01_03-power-rate.png")

fig_dict["power-rate"] = fig


# %%
print(df_3["latency_avg"].corr(df_3["transmission_num_avg"]))
print(df_1["latency_avg"].corr(df_1["transmission_num_avg"]))

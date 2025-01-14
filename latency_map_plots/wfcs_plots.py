# %%
from pathlib import Path
import pandas as pd
from metrics import METRICS
import matplotlib.pyplot as plt


plt.style.use('seaborn-v0_8-paper')

BASE_FOLDER = Path("/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/wfcs_sim_maps")
MAP_FOLDER = BASE_FOLDER / "maps"
OUT_FOLDER = BASE_FOLDER / "article"

OUT_FOLDER.mkdir(parents=True, exist_ok=True)

# %%
df_0 = pd.read_csv(Path(MAP_FOLDER) / "map_00.csv")
#print(df_0.head())

# %% LATENCY (AVG, MIN) + TX NUM
fig, ax = plt.subplots()
ax2 = ax.twinx()

p1, = ax.plot(df_0["x_pos"], df_0["latency_avg"]*METRICS["latency_avg"]["scaling"], "C0", label = "Average latency")
p2, = ax.plot(df_0["x_pos"], df_0["latency_min"]*METRICS["latency_min"]["scaling"], "C1", label = "Min latency")
p3, = ax2.plot(df_0["x_pos"], df_0["transmission_num_avg"]*METRICS["transmission_num_avg"]["scaling"], "C2", label="Average attempts")

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("STA position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["latency_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax2.set_ylabel(METRICS["transmission_num_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(handles=[p1, p2, p3], prop={'size': 14})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "01-latency_avg_min-tx_num_avg.png")

# %% LATENCY 99 and 99.9 perc
fig, ax = plt.subplots()

p1, = ax.plot(df_0["x_pos"], df_0["latency_99_perc"]*METRICS["latency_99_perc"]["scaling"], "C0", label = "99 percentile")
p2, = ax.plot(df_0["x_pos"], df_0["latency_99.9_perc"]*METRICS["latency_99.9_perc"]["scaling"], "C1", label = "99.9 percentile")

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("STA position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["latency_99_perc"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(prop={'size': 14})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "01_latency_99_99.9_perc.png")

# %% POWER + RATE
fig, ax = plt.subplots()
ax2 = ax.twinx()

p1, = ax.plot(df_0["x_pos"], df_0["power_avg"]*METRICS["power_avg"]["scaling"], "C0", label = "Average power")
p2, = ax2.plot(df_0["x_pos"], df_0["rate_avg"]*METRICS["rate_avg"]["scaling"], "C2", label="Average data rate")

#ax.set_title(metric, fontsize=20)
ax.set_xlabel("STA position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["power_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.tick_params(axis='both', which='major', labelsize = 14)

ax2.set_ylabel(METRICS["rate_avg"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(handles=[p1, p2],  loc = "center left", prop={'size': 14})

plt.tight_layout()
fig.savefig(OUT_FOLDER / "01-power-rate.png")

# %% DROPPED + TX MAX
fig, ax = plt.subplots()
ax2 = ax.twinx()

p1, = ax.plot(df_0["x_pos"], df_0["%_dropped"]*METRICS["%_dropped"]["scaling"], "C0", label = "% Dropped packets")
p2, = ax2.plot(df_0["x_pos"], df_0["transmission_num_max_no_dropped"]*METRICS["transmission_num_max_no_dropped"]["scaling"], "C2", label="Max attempts")

#ax.set_title(metric, fontsize=20)

ax.set_xlabel("STA position (m)", fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylabel(METRICS["%_dropped"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax.set_ylim([-10, 110])
ax.tick_params(axis='both', which='major', labelsize = 14)


ax2.set_ylabel(METRICS["transmission_num_max_no_dropped"]["label"], fontsize=16)#, rotation=-90, va="bottom")
ax2.tick_params(axis='both', which='major', labelsize = 14)

ax.legend(handles=[p1, p2], loc="upper left", prop={'size': 14})


plt.tight_layout()
fig.savefig(OUT_FOLDER / "01-dropped-tx_max.png")

# %%
plt.close(fig)

# %%

# add 3 distribution graphs

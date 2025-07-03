import json

file_constant = "/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/etfa_wip_constant_sim/db711.dat"
file_minstrel = "/home/ptrchv/repos/ns3-wifi-sim/latency_map_plots/wfcs_sim/db99.dat"

with open(file_minstrel) as f:
    data = json.load(f)

data = data[1:-1]


failed = []
for row in data:
    if row["acked"] == False:
        failed.append(row)
print(len(failed))

rate_counts = {}
for row in failed:
    rate = row["transmissions"][0]["rate"]
    if rate not in rate_counts:
        rate_counts[rate] = 0
    rate_counts[rate] += 1
#print(rate_counts)


rates = [6000000, 9000000, 12000000, 18000000, 24000000, 36000000, 48000000, 54000000]

rate_tx = {r: 0 for r in rates}
rate_failures = {r: 0 for r in rates}
for row in data:
    rate = row["transmissions"][0]["rate"]
    rate_tx[rate] += 1
    if row["acked"] == False:
        rate_failures[rate] += 1

for rate in rates:
    print("{} {}".format(rate, rate_failures[rate] /rate_tx[rate]*100))


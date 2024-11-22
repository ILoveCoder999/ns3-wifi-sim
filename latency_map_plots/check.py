from pathlib import Path
import filecmp

folders = [
    "latency_avg",
    "latency_99_perc",
    "latency_99.9_perc",
    "retransmission_count"
]

for folder in folders:
    path1 = Path("./maps_{}".format(folder))
    path2 = Path("./maps/{}".format(folder))

    fnames1 = {f.name for f in path1.iterdir()}
    fnames2 = {f.name for f in path2.iterdir()}

    fnames = fnames1.union(fnames2)

    for f in fnames:
        res = filecmp.cmp(path1/f, path2/f, shallow=False)
        # print("diff {} {}".format(path1/f, path2/f))
        if not res:
            print("diff {} {}".format(path1/f, path2/f))
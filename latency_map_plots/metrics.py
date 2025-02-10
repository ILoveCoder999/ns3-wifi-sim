import statistics
import itertools
import numpy as np
import copy

DATA_RATES = [6, 9, 12, 18, 24, 36, 48, 54]

def extract_avg_power(data):
    avg_power = sum(map(
        lambda row: sum(map(
                    lambda t: t["tx_power_w"] * t["tx_time"],
                    row["transmissions"]
                )),
        data))
    return avg_power / 10**6 / 30000

def extract_avg_rate(data):
    avg_rate = statistics.mean(
        itertools.chain.from_iterable(
            map(
                lambda row: [t["rate"] for t in row["transmissions"]],
                data
            )
        )
    )
    return avg_rate

def extract_rate_success(data):
    data = remove_dropped(data)
    rate_success = statistics.mean(
            map(
                lambda row: [t["rate"] for t in row["transmissions"]][-1],
                data
            )
        )
    return rate_success

def extract_fist_rate(data):
    first_rate = statistics.mean(
            map(
                lambda row: [t["rate"] for t in row["transmissions"]][0],
                data
            )
        )
    return first_rate

def extract_max_rate(data):
    max_rate = statistics.mean(
            map(
                lambda row: max([t["rate"] for t in row["transmissions"]]),
                data
            )
        )
    return max_rate

def extract_rate_dist(data):
    rates = list(
        itertools.chain.from_iterable(
            map(
                lambda row: [t["rate"] for t in row["transmissions"]],
                data
            )
        )
    )
    return [rates.count(rate*1e6) for rate in DATA_RATES]

def extract_rate_succ_dist(data):
    rates = extract_rate_dist(data)
    data = remove_dropped(data)
    rates_succ = list(
        map(
            lambda row: [t["rate"] for t in row["transmissions"]][-1],
            data
        )
    )
    rates_succ = [rates_succ.count(rate*1e6) for rate in DATA_RATES]
    return [rs / r * 100 for rs, r in zip(rates_succ, rates)]

def extract_rate_latency_dist(data, exclude_final = True):
    latency_data = list(
            map(
                lambda row: [{"rate": t["rate"], "latency":t["latency"]} for t in (row["transmissions"][:-1] if (row["acked"] and exclude_final) else row["transmissions"])],
                data
        )
    )
    for row in latency_data:
        row.reverse()
        for j in range(0, len(row)-1):
            row[j]["latency"] -= row[j+1]["latency"]
    latency_data = list(itertools.chain.from_iterable(latency_data))
    rate_latency = [sum([el["latency"] for el in latency_data if el["rate"]==(rate*1e6)]) for rate in DATA_RATES]
    rate_latency = [l/len(data) for l in rate_latency]
    return rate_latency


def remove_dropped(data):
    return [row for row in data if row["acked"] == True]


# metrics for maps
METRICS = {
    "latency_avg" : {
        "label": "Latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: statistics.mean(map(lambda x: x["latency"], remove_dropped(data)))
    },
    "latency_stdev" : {
        "label": "Latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: statistics.stdev(map(lambda x: x["latency"], remove_dropped(data)))
    },
    "latency_min" : {
        "label": "Latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: min(map(lambda x: x["latency"], remove_dropped(data)))
    },
    "latency_10_perc" : {
        "label": "Latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: np.percentile(np.array(list(map(lambda x: x["latency"], remove_dropped(data)))), 10)
    },
    "latency_99_perc" : {
        "label": "Latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: np.percentile(np.array(list(map(lambda x: x["latency"], remove_dropped(data)))), 99)
    },
    "latency_99.9_perc" : {
        "label": "Latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: np.percentile(np.array(list(map(lambda x: x["latency"], remove_dropped(data)))), 99.9)
    },
    "transmission_num_avg" : {
        "label": "# Transmissions",
        "scaling": 1,
        "fnc": lambda data: statistics.mean(map(lambda x: len(x["transmissions"]), remove_dropped(data)))
    },
    "transmission_num_max" : {
        "label": "# Transmissions",
        "scaling": 1,
        "fnc": lambda data: max(map(lambda x: len(x["transmissions"]), data))
    },
    "transmission_num_max_no_dropped" : {
        "label": "# Transmissions",
        "scaling": 1,
        "fnc": lambda data: max(map(lambda x: len(x["transmissions"]), remove_dropped(data)))
    },
    "power_avg": {
        "label": "Power ($\mu$W)",
        "scaling": 1000,
        "fnc": extract_avg_power
    },
    "rate_avg": {
        "label": "Data rate (Mbit/s)",
        "scaling": 1/1000000,
        "fnc": extract_avg_rate
    },
    "rate_success": {
        "label": "Data rate (Mbit/s)",
        "scaling":  1/1000000,
        "fnc": extract_rate_success
    },
    "rate_first": {
        "label": "Data rate (Mbit/s)",
        "scaling":  1/1000000,
        "fnc": extract_fist_rate
    },
    "rate_max": {
        "label": "Data rate (Mbit/s)",
        "scaling":  1/1000000,
        "fnc": extract_max_rate
    },
    "%_dropped": {
        "label": "Dropped (%)",
        "scaling": 1,
        "fnc": lambda data: len([row for row in data if row["acked"] == False]) / len(data) * 100
    },
    "rate_distribution": {
        "label": "# Transmissions (x1000)",
        "scaling": 0.001,
        "fnc": extract_rate_dist,
        "classes" : DATA_RATES
    },
    "rate_distribution_no_dropped": {
        "label": "# Transmissions (x1000)",
        "scaling": 0.001,
        "fnc": lambda data: extract_rate_dist(remove_dropped(data)),
        "classes" : DATA_RATES
    },
    "rate_succ_distribution": {
        "label": "Success percentage (%)",
        "scaling": 1,
        "fnc": extract_rate_succ_dist,
        "classes" : DATA_RATES
    },
    "rate_latency_distribution": {
        "label": "Latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: extract_rate_latency_dist(remove_dropped(data)),
        "classes" : DATA_RATES
    },
    "rate_latency_distribution_with_final": {
        "label": "Latency ($\mu$s)",
        "scaling": 0.001,
        "fnc": lambda data: extract_rate_latency_dist(remove_dropped(data), exclude_final=False),
        "classes" : DATA_RATES
    }
}

def main():
    import json

    with open("wfcs_sim/db592.dat") as f:
        sim_data = json.load(f)[1:-1]
    
    print(extract_rate_succ_dist(sim_data))
    print(extract_rate_latency_dist(sim_data))

if __name__ == "__main__":
    main()
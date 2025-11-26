import json
from utils.handover import HandoverConfig, Position, Interferer
from dataclasses import asdict

ap_positions = [
    [Position(60, 0.5, 0), Position(90, 0.5, 0)],   # 0
    [Position(60, 0.5, 0), Position(110, 0.5, 0)],  # 1
    [Position(60, 0.5, 0), Position(130, 0.5, 0)]   # 2
]

int_positions = [
    # 0
    [],
    # 1
    [
        (Position(110, 0.5, 0), 1)
    ],
    # 2
    [
        (Position(130, 0.5, 0), 1)
    ],
    # 3
    [
        (Position(150, 0.5, 0), 1)
    ],
    # 4
    [
        (Position(170, 0.5, 0), 1)
    ],
    # 5
    [
        (Position(130, 0.5, 0), 1),
        (Position(130, 1, 0), 1),
        (Position(130, 1.5, 0), 1)
    ],
    # 6
    [
        (Position(20, 0.5, 0), 0),
        (Position(130, 0.5, 0), 1),
        (Position(130, 1, 0), 1),
        (Position(130, 1.5, 0), 1)
    ],
    # 7
    [
        (Position(110, 0.5, 0), 1),
        (Position(130, 0.5, 0), 1)
    ],
    # 8
    [
        (Position(20, 0.5, 0), 0),
        (Position(110, 0.5, 0), 1),
        (Position(130, 0.5, 0), 1)
    ],
    # 9
    [
        (Position(110, 0.5, 0), 1),
        (Position(120, 0.5, 0), 1),
        (Position(130, 0.5, 0), 1)
    ]
]

net_confs = [
    # {"ap": 0, "int": 2, "double_ch": [False]},
    # {"ap": 1, "int": 3, "double_ch": [False]},
    # {"ap": 2, "int": 4, "double_ch": [False]},
    # {"ap": 0, "int": 1, "double_ch": [False]},
    # {"ap": 0, "int": 5, "double_ch": [False]},
    # {"ap": 0, "int": 6, "double_ch": [False]},
    {"ap": 0, "int": 7, "double_ch": [False]},
    {"ap": 0, "int": 8, "double_ch": [False]},
    {"ap": 0, "int": 9, "double_ch": [False]},
]

def gen_config():
    configs = []
    for net in net_confs:
        for double in net["double_ch"]:
            interferers = [
                Interferer(position=pos[0], channel_idx=pos[1] if double else 0)
                for pos in int_positions[net["int"]]
            ]
            configs.append (
                HandoverConfig(
                    apPositions = ap_positions[net["ap"]],
                    interferers = interferers,
                    doubleChannel = double
                )
            )

    with open("./tii_simulations_04.json", "w") as f:
        json.dump([asdict(c) for c in configs], f, indent=2)


def main():
    # test()
    gen_config()

if __name__ == "__main__":
    main()

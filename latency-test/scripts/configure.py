from utils.V2 import JsonConfig, StaConfig, Position, InterferentConfig
import json
from dataclasses import asdict

configs = []

simulationTime = 100000
staStep = 1
interferentYPositions = [2, -2, 4, -4]

for interferentNodes in [0, 1, 2, 3, 4]:
    for interferentX in [-40.0, -20.0, 0.0, 20.0, 40.0]:
        for staX in range(0, 50, staStep):
            configs.append(JsonConfig(
                sta=StaConfig(
                    position=Position(x=float(staX))
                ),
                interferent=[
                    InterferentConfig(
                        position=Position(x=interferentX, y=interferentY)
                    )
                    for interferentY in interferentYPositions[:interferentNodes]
                ],
                simulationTime=simulationTime
            ))

# Extra
for staX in range(0, 50, staStep):
    configs.append(JsonConfig(
        sta=StaConfig(
            position=Position(x=float(staX))
        ),
        interferent=[
            InterferentConfig(
                position=Position(x=-40.0)
            ),
            InterferentConfig(
                position=Position(x=-20.0)
            ),
            InterferentConfig(
                position=Position(x=0.0)
            ),
            InterferentConfig(
                position=Position(x=20.0)
            ),
        ],
        simulationTime=simulationTime
    ))

for staX in range(0, 50, staStep):
    configs.append(JsonConfig(
        sta=StaConfig(
            position=Position(x=float(staX))
        ),
        interferent=[
            InterferentConfig(
                position=Position(x=-20.0, y=2.0)
            ),
            InterferentConfig(
                position=Position(x=-20.0, y=-2.0)
            ),
            InterferentConfig(
                position=Position(x=0.0)
            ),
            InterferentConfig(
                position=Position(x=20.0)
            ),
        ],
        simulationTime=simulationTime
    ))

for staX in range(0, 50, staStep):
    configs.append(JsonConfig(
        sta=StaConfig(
            position=Position(x=float(staX))
        ),
        interferent=[
            InterferentConfig(
                position=Position(x=-40.0)
            ),
            InterferentConfig(
                position=Position(x=-30.0)
            ),
            InterferentConfig(
                position=Position(x=-20.0)
            ),
            InterferentConfig(
                position=Position(x=-10.0)
            ),
        ],
        simulationTime=simulationTime
    ))

for staX in range(0, 50, staStep):
    configs.append(JsonConfig(
        sta=StaConfig(
            position=Position(x=float(staX))
        ),
        interferent=[
            InterferentConfig(
                position=Position(x=-40.0)
            ),
            InterferentConfig(
                position=Position(x=-30.0, y=2.0)
            ),
            InterferentConfig(
                position=Position(x=-30.0, y=-2.0)
            ),
            InterferentConfig(
                position=Position(x=-20.0)
            ),
        ],
        simulationTime=simulationTime
    ))

print(len(configs))
with open("./2024_06_08_simulations_1.json", "w") as f:
    json.dump([asdict(c) for c in configs[:len(configs)//2]], f, indent=2)
print(len(configs)//2)
with open("./2024_06_08_simulations_2.json", "w") as f:
    json.dump([asdict(c) for c in configs[len(configs)//2:]], f, indent=2)
with open("./2024_06_08_simulations.json", "w") as f:
    json.dump([asdict(c) for c in configs], f, indent=2)
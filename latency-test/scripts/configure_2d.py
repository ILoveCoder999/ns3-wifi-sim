from utils.V2 import JsonConfig, StaConfig, Position, InterferentConfig
import json
from dataclasses import asdict

configs = []

simulationTime = 100000
staStep = 1

interferents = [
    [],
    [
        InterferentConfig(
            position=Position(x=0.0, y=0.0)
        )
    ],
    [
        InterferentConfig(
            position=Position(x=-20, y=-20)
        )
    ],
    [
        InterferentConfig(
            position=Position(x=-20, y=20)
        )
    ],
    [
        InterferentConfig(
            position=Position(x=20, y=-20)
        )
    ],
    [
        InterferentConfig(
            position=Position(x=20, y=20)
        )
    ],
    [
        InterferentConfig(
            position=Position(x=-20, y=-20)
        ),
        InterferentConfig(
            position=Position(x=-20, y=-20)
        )
    ],
    [
        InterferentConfig(
            position=Position(x=-20, y=-20)
        ),
        InterferentConfig(
            position=Position(x=-20, y=20)
        )
    ],
    [
        InterferentConfig(
            position=Position(x=-20, y=-20)
        ),
        InterferentConfig(
            position=Position(x=20, y=20)
        )
    ],
    [
        InterferentConfig(
            position=Position(x=-30, y=-30)
        ),
        InterferentConfig(
            position=Position(x=-30, y=30)
        )
    ],
]

for interferent in interferents:
    for staX in range(0, 50, staStep):
        for staY in range(0, 50, staStep):
            configs.append(JsonConfig(
                sta=StaConfig(
                    position=Position(x=staX, y=staY),
                    payloadSize=22  # Per avere il pacchetto della stessa dimensione del caso reale
                ),
                interferents=interferent,
                simulationTime=simulationTime
            ))

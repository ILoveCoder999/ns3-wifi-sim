#!/bin/bash
#BSUB -n 32
#BSUB -q ext_batch
#BSUB -J ns-3-sim
#BSUB -o ns-3-sim.out
#BSUB -e ns-3-sim.err

python scripts/simulation.py --jsonConfig --out 1d-test --cores 32 2024_06_08_simulations.json

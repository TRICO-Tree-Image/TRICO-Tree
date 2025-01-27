# Flow-level Simulation of TRICO Tree

## Requirements

```
gurobipy (a lisence is required)
json
os
```

## How to run the simulation

### Install requirements

```bash
pip install gurobipy
```

### Prepare your algorithm dataset

Generate a dataset of hierarchical scheduling algorithms, and copy the folder in to this folder. There are some examples in `./test_algorithms'. 

### Prepare a simulation configuration

Write your simulation configuration in a json file, just like the following one in `./config_2L4S.json`:

```json
{
    "filepath": "./test_algorithms",
    "PIFO_core_capacity": [45034, 45034, 1512, 1512, 1512, 1512],
    "PIFO_core_PPS": [92.5, 92.5, 185, 185, 185, 185],
    "PIFO_core_legal_capacity": [[4094], [4094], [126], [126], [126], [126]],
    "Total_flow_number": "1000"
}
```

### Run the code

Set your json filename and experiment name in `./experiment.py`:

```python
json_filename = './config_2L4S.json'
        experiment_set = '2L4S'
        output_filename = f"./result_TRICO_{experiment_set}_{flow_num}.csv"
        experiment(json_filename, output_filename, flow_num)
```

Then just run the code:

```bash
python ./experiment.py
```

The output will be in `./results/`. Each line of the `csv` file contains:

```csv
filename, number of layers in the algorithm, number of flows, throughput
```



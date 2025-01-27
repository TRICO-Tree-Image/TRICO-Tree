from PIFO import PIFO
from PIFO import ScheduleAlgo
import solver
import json
import os

def ReadScheduleAlgo(filename):
    Treepath = {}
    pifo_map = {}

    with open(filename, "r") as infile:
        in_pifo_section = False
        in_flow_section = False

        for line in infile:
            line = line.strip()

            if not line:
                continue
            if line.startswith("#"):
                if line == "#PIFO":
                    in_pifo_section = True
                    in_flow_section = False
                elif line == "#Flow":
                    in_pifo_section = False
                    in_flow_section = True
                continue

            if in_pifo_section:
                parts = line.split()
                pifoid = int(parts[0])
                schedule_algorithm = parts[1]
                children_node = []
                rest_of_line = ' '.join(parts[2:])
                while '{' in rest_of_line:
                    pos = rest_of_line.find('{')
                    end_pos = rest_of_line.find('}', pos)
                    if end_pos == -1:
                        break
                    child_str = rest_of_line[pos + 1:end_pos]
                    child_parts = [c.strip() for c in child_str.split(',')]
                    child_id = int(child_parts[0])
                    is_pifo = True if child_parts[1] == "true" else False
                    parameter = int(child_parts[2])
                    children_node.append((child_id, is_pifo, parameter))
                    rest_of_line = rest_of_line[end_pos + 1:]
                pifo_node = PIFO(pifoid, schedule_algorithm, children_node)
                pifo_map[pifoid] = pifo_node

            elif in_flow_section:
                # the format is like  1 {101, 102, 104}
                # the first number is the flowid, others are the treepath in order
                parts = line.split()
                flowid = int(parts[0])
                treepath_str = parts[1:]
                treepath = []
                for x in treepath_str:
                    treepath.append(int(''.join(filter(str.isdigit, x))))
                Treepath[flowid] = treepath
        
        ScheduleAlgorithm = ScheduleAlgo(Treepath, pifo_map)
    return ScheduleAlgorithm

def single_experiment(filename, config_filename, total_flow_number):
    print(f"Experimenting with {filename}, {config_filename}")
    # read the configuration file
    with open(config_filename, "r") as infile:
        config = json.load(infile)
    # read the schedule algorithm
    core_capacity_list = config["PIFO_core_capacity"]
    core_frequency_list = config["PIFO_core_PPS"]
    core_legal_capacity_list = config["PIFO_core_legal_capacity"]
    # total_flow_number = config["Total_flow_number"]

    ScheduleAlgorithm = ReadScheduleAlgo(filename)
    pifo_map = ScheduleAlgorithm.pifo_map
    Treepath = ScheduleAlgorithm.treepath
    
    # rearrange the pifo id in order
    pifo_id_map = {}
    capacity_demand_list = {}
    count = 0
    for pifoid in pifo_map:
        pifo_id_map[pifoid] = count
        count += 1
    print(pifo_id_map)

    # the list of paths with new pifo id
    path_list = []
    for path in Treepath:
        temp_path = []
        for pifoid in Treepath[path]:
            temp_path.append(pifo_id_map[pifoid])
        if temp_path not in path_list:
            path_list.append(temp_path)

    # number of unique paths
    path_num = 0
    unique_path_list = []
    for path in path_list:
        if path not in unique_path_list:
            unique_path_list.append(path)
            path_num += 1

    flow_number = int(total_flow_number) // path_num
    print("flow_number: ", flow_number)
    
    # check the whole path list, to make sure no path is subset of another path
    for i in range(len(path_list)):
        for j in range(len(path_list)):
            if i != j:
                if set(path_list[i]).issubset(set(path_list[j])):
                    path_list[i] = []
    path_list = [x for x in path_list if x != []]
    print(path_list)

    for pifoid in pifo_map:
        this_pifo = pifo_map[pifoid]
        children = this_pifo.children_node
        capacity_demand = 0
        has_flow_child = 0
        for child in children:
            if child[1] == True:
                capacity_demand += 1
            else:
                has_flow_child = 1
        if has_flow_child == 1:
            capacity_demand += int(flow_number)
        capacity_demand_list[pifo_id_map[pifoid]] = capacity_demand
    true_capacity_demand_list = []
    for i in range(len(capacity_demand_list)):
        true_capacity_demand_list.append(capacity_demand_list[i])
    print(capacity_demand_list)
    total_capacity_demand = sum(capacity_demand_list.values())

    print("total_capacity_demand: ", total_capacity_demand)
    print("core_capacity_list: ", core_capacity_list)
    print("core_frequency_list: ", core_frequency_list)
    print("core_legal_capacity_list: ", core_legal_capacity_list)
    print("path_list: ", path_list)
            
    # call the solver
    # objvalue = solver1.load_balancing_solve_easy(capacity_demand_list, core_capacity_list, core_frequency_list, path_list)
    objvalue = solver.load_balancing_solve(capacity_demand_list, core_capacity_list, core_frequency_list, core_legal_capacity_list, path_list)
    return objvalue, total_capacity_demand

def find_depth(filename):
    ScheduleAlgorithm = ReadScheduleAlgo(filename)
    Treepath = ScheduleAlgorithm.treepath
    depth = 0
    for path in Treepath:
        if len(Treepath[path]) > depth:
            depth = len(Treepath[path])
    return depth

def experiment(config_filename, output_filename, total_flow_number):
    # read the configuration file
    # delete the output file if it exists
    if os.path.exists(output_filename):
        os.remove(output_filename)
    with open(config_filename, "r") as infile:
        config = json.load(infile)
    filepath = config["filepath"]
    files = os.listdir(filepath)
    for file in files:
        if file.endswith(".txt"):
            filename = os.path.join(filepath, file)
            objvalue, total_capacity_demand = single_experiment(filename, config_filename, total_flow_number)
            depth = find_depth(filename)
            with open(output_filename, "a") as outfile:
                outfile.write(f"{filename},{depth},{total_capacity_demand},{objvalue}\n")

# main function
if __name__ == "__main__":
    flow_num_list = []
    for i in range(6, 15):
        flow_num_list.append(int(pow(2, i)))
        flow_num_list.append(int(pow(2, i + 0.5)))
    for flow_num in flow_num_list:
        json_filename = './config_2L4S.json'
        experiment_set = '2L4S'
        output_filename = f"./results/result_TRICO_{experiment_set}_{flow_num}.csv"
        experiment(json_filename, output_filename, flow_num)




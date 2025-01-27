from PIFO import PIFO
from PIFO import ScheduleAlgo
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

def single_experiment(filename, total_flow_number, method):
    # core_legal_capacity_list = config["PIFO_core_legal_capacity"]
    # total_flow_number = config["Total_flow_number"]
    depth = find_depth(filename)
    if method == 'oPIFO':
        if depth <= 5 and total_flow_number <= 512:
            return 25
        else:
            return 0
    # vPIFO Setting 1：容量8190*instance个数12  140MHz/3个cycle处理一个数据包
    # vPIFO Setting 2：容量4094*instance个数22  135MHz/3个cycle处理一个数据包
    # vPIFO Setting 3：容量1364*instance个数40 105.26MHz/3个cycle处理一个数据包
    elif method == 'vPIFO_1':
        if find_num_nodes(filename) <= 12 and total_flow_number <= 8190:
            return 46.67 / find_depth(filename)
        else:
            return 0
    elif method == 'vPIFO_2':
        if find_num_nodes(filename) <= 22 and total_flow_number <= 4094:
            return 45.0 / find_depth(filename)
        else:
            return 0
    elif method == 'vPIFO_3':
        if find_num_nodes(filename) <= 40 and total_flow_number <= 1364:
            return 35.09 / find_depth(filename)
        else:
            return 0
    elif method == 'vPIFO_2_new':
        if find_num_nodes(filename) <= 36 and total_flow_number <= 1022:
            return 64.0 / find_depth(filename)
        else:
            return 0
    elif method == 'vPIFO_3_new':
        if find_num_nodes(filename) <= 32 and total_flow_number <= 510:
            return 85.33 / find_depth(filename)
        else:
            return 0

def find_depth(filename):
    ScheduleAlgorithm = ReadScheduleAlgo(filename)
    Treepath = ScheduleAlgorithm.treepath
    depth = 0
    for path in Treepath:
        if len(Treepath[path]) > depth:
            depth = len(Treepath[path])
    return depth

def find_num_nodes(filename):
    ScheduleAlgorithm = ReadScheduleAlgo(filename)
    return len(ScheduleAlgorithm.pifo_map)

def experiment(config_filename, output_filename, total_flow_number, method):
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
            objvalue = single_experiment(filename, total_flow_number, method)
            depth = find_depth(filename)
            with open(output_filename, "a") as outfile:
                outfile.write(f"{filename},{depth},{total_flow_number},{objvalue}\n")

flow_num_list = []

for i in range(5, 20):
    flow_num_list.append(int(pow(2, i)))
    flow_num_list.append(int(pow(2, i + 0.5)))


# main function
if __name__ == "__main__":
    for flow_num in flow_num_list:
        for method in ['vPIFO_2_new', 'vPIFO_3_new', 'oPIFO', 'vPIFO_1']:
            output_filename = f"./results/result_{method}_{flow_num}.csv"
            experiment("./exp_config.json", output_filename, flow_num, method)




from oPIFOTree import PifoTreeScheduler as PifoTree
from perPIFOonly import PifoTreeScheduler as RCPifoTree_wo_credit
from CreditOnly import PifoTreeScheduler as RCPifoTree_wo_HRC
from HPIFO4 import PifoTreeScheduler as RCPifoTree
from idealscheduler import PifoTreeScheduler as IdealScheduler
import ast
import numpy as np
import random
import os
from tqdm import tqdm
import csv

random.seed(4)

PACKET_FILE = "Trace.txt"
# HSCH_ALGORITHM = "TreeStructure.txt"
# HSCH_ALGORITHM = "paper1.txt"
HSCH_ALGORITHM ="./Algorithm/3.txt"
# CONG_M = 2000000
# CONG_N = 1

def save_nested_list_to_csv(data, file_path):
    with open(file_path, mode='w', newline='') as file:
        writer = csv.writer(file)
        for row in data:
            writer.writerow([str(item) for item in row])

def read_csv_to_nested_list(file_path):
    data = []
    with open(file_path, mode='r', newline='') as file:
        reader = csv.reader(file)
        for row in reader:
            parsed_row = []
            for item in row:
                try:
                    parsed_row.append(ast.literal_eval(item))
                except (ValueError, SyntaxError):
                    parsed_row.append(item)
            data.append(parsed_row)
    return data

class Pkt:
    def __init__(self, pktid, flowid, length, flow_size):
        self.pktid = pktid
        self.flowid = flowid
        self.length = length
        self.flow_size = flow_size
        self.oid = flowid

def get_tree_depth(filename):
    Treepath = {}
    pifo_config = []

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
                    
                pifo_config.append((pifoid, schedule_algorithm, children_node))
                # pifo_node = PIFO(pifoid, schedule_algorithm, children_node)
                # pifo_map[pifoid] = pifo_node

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
    r = 0
    for i in Treepath:
        r = max(len(Treepath[i]),r)
    return r

def ReadScheduleAlgo(filename,scheduler):
    Treepath = {}
    pifo_config = []

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
                    
                pifo_config.append((pifoid, schedule_algorithm, children_node))
                # pifo_node = PIFO(pifoid, schedule_algorithm, children_node)
                # pifo_map[pifoid] = pifo_node

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

    for i in Treepath:
        r = Treepath[i][0]
        break
    scheduler.initialize_params(
    pifo_config=pifo_config,
    treepaths=Treepath,
    root=r
    )

def read_pkts_from_file(file_path):
    pkts = []

    sch_flowids = []

    length = 0

    # for i in treepath:
    #     sch_flowids.append(i)

    # random.shuffle(sch_flowids)

    with open(file_path, 'r') as file:
        for line in file:
            value = line.strip().split()
            # Convert string to int
            value = list(map(int, value))
            pkt = Pkt(*value)
            # pkt.flowid = sch_flowids[pkt.flowid%len(sch_flowids)]
            length += pkt.length
            # pkt.length = 1000
            pkts.append(pkt)
    # print("Total length: ", length)
    return pkts

def initialize_from_txt(file_path, scheduler):
    with open(file_path, 'r') as f:
        config = f.read()

    config_vars = {}

    exec(config, {}, config_vars)

    pifo_config = config_vars['pifo_config']
    treepaths = config_vars['treepaths']
    root = config_vars['root']

    scheduler.initialize_params(
        pifo_config=pifo_config,
        treepaths=treepaths,
        root=root
    )

    return len(treepaths)

def calculate_errors(baseline, comparison):

    baseline_start_times = {}
    comparison_start_times = {}


    current_time = 0
    for pkt in baseline:
        baseline_start_times[pkt.pktid] = current_time
        current_time += pkt.length


    current_time = 0
    for pkt in comparison:
        comparison_start_times[pkt.pktid] = current_time
        current_time += pkt.length

    result = []

    errors = []
    for pktid in baseline_start_times:
        baseline_time = baseline_start_times[pktid]
        comparison_time = comparison_start_times[pktid]
        if comparison_time <= baseline_time:
            error = 0
        else:
            error = comparison_time - baseline_time
        errors.append(error)
        result.append((pktid, comparison_time - baseline_time))
        # if error > 100000:
        #     print(pktid, baseline_time, comparison_time)
    # save_nested_list_to_csv(result,"error.csv")

    average_error = np.mean(errors)
    max_error = np.max(errors)
    pct_99_error = np.percentile(errors, 99)

    return average_error, max_error, pct_99_error

def run_scheduler(scheduler, pkts):
    order = []
    input_bytes = 0
    output_bytes = 0

    for pkt in pkts:
        scheduler.doenqueue(pkt)

    while True:
        pkt = scheduler.dodequeue()
        if pkt is None:
            break
        if pkt.flowid == -1:
            break
        order.append(pkt)
    return order

def order_evaluation(algorithm_file,pkts):

    PifoTree_sch = PifoTree(root=1)
    ReadScheduleAlgo(algorithm_file, PifoTree_sch)

    IdealScheduler_sch = IdealScheduler(root=1)
    ReadScheduleAlgo(algorithm_file, IdealScheduler_sch)

    RCPifoTree_sch = RCPifoTree(root=1)
    ReadScheduleAlgo(algorithm_file, RCPifoTree_sch)

    RCPifoTree_wo_credit_sch = RCPifoTree_wo_credit(root=1)
    ReadScheduleAlgo(algorithm_file, RCPifoTree_wo_credit_sch)

    RCPifoTree_wo_HRC_sch = RCPifoTree_wo_HRC(root=1)
    ReadScheduleAlgo(algorithm_file, RCPifoTree_wo_HRC_sch)

    # pkts = read_pkts_from_file(packet_file,PifoTree_sch.treepath)
    sch_flowids = []
    for i in PifoTree_sch.treepath:
        sch_flowids.append(i)
    random.shuffle(sch_flowids)
    for i in pkts:
        i.flowid = sch_flowids[i.oid%len(sch_flowids)]

    PifoTree_order = []
    IdealScheduler_order = []
    RCPifoTree_order = []
    RCPifoTree_wo_credit_order = []
    RCPifoTree_wo_HRC_order = []

    i = 0

    # for pkt in pkts:
    #     PifoTree_sch.doenqueue(pkt)
    #     i+=1
    #     if i % 3 == 0:
    #         pkt = PifoTree_sch.dodequeue()
    #         PifoTree_order.append(pkt)

    # PifoTree_order = run_scheduler(PifoTree_sch,pkts)
    RCPifoTree_order = run_scheduler(RCPifoTree_sch,pkts)
    # RCPifoTree_wo_credit_order = run_scheduler(RCPifoTree_wo_credit_sch,pkts)
    # RCPifoTree_wo_HRC_order = run_scheduler(RCPifoTree_wo_HRC_sch,pkts)
    IdealScheduler_order = run_scheduler(IdealScheduler_sch,pkts)

    # while True:
    #     pkt = PifoTree_sch.dodequeue()
    #     if pkt is None:
    #         break
    #     PifoTree_order.append(pkt)
    
    # for pkt in pkts:
    #     IdealScheduler_sch.doenqueue(pkt)
    # while True:
    #     pkt = IdealScheduler_sch.dodequeue()
    #     if pkt is None:
    #         break
    #     IdealScheduler_order.append(pkt)
    
    # for pkt in pkts:
    #     RCPifoTree_sch.doenqueue(pkt)
    # while True:
    #     pkt = RCPifoTree_sch.dodequeue()
    #     if pkt is None:
    #         break
    #     RCPifoTree_order.append(pkt)

    # for pkt in pkts:
    #     RCPifoTree_wo_credit_sch.doenqueue(pkt)
    # while True:
    #     pkt = RCPifoTree_wo_credit_sch.dodequeue()
    #     if pkt is None:
    #         break
    #     RCPifoTree_wo_credit_order.append(pkt)

    # for pkt in pkts:
    #     RCPifoTree_wo_HRC_sch.doenqueue(pkt)
    # while True:
    #     pkt = RCPifoTree_wo_HRC_sch.dodequeue()
    #     if pkt is None:
    #         break
    #     RCPifoTree_wo_HRC_order.append(pkt)

    # output the result
    # print("PifoTree: ", calculate_errors(IdealScheduler_order, PifoTree_order))
    # # #print("IdealScheduler: ", calculate_errors(IdealScheduler_order, IdealScheduler_order))
    # print("RCPifoTree: ", calculate_errors(IdealScheduler_order, RCPifoTree_order))
    # print("RCPifoTree_wo_credit: ", calculate_errors(IdealScheduler_order, RCPifoTree_wo_credit_order))
    # print("RCPifoTree_wo_HRC: ", calculate_errors(IdealScheduler_order, RCPifoTree_wo_HRC_order))
    # print("FIFO: ", calculate_errors(IdealScheduler_order, pkts))

    # return calculate_errors(IdealScheduler_order, PifoTree_order),calculate_errors(IdealScheduler_order, RCPifoTree_order),\
    #     calculate_errors(IdealScheduler_order, RCPifoTree_wo_credit_order),calculate_errors(IdealScheduler_order, RCPifoTree_wo_HRC_order),\
    #     calculate_errors(IdealScheduler_order, pkts)

    # ideal = []
    # trico = []
    # for p in IdealScheduler_order:
    #     ideal.append((p.pktid,p.flowid,p.length))
    # for p in RCPifoTree_order:
    #     trico.append((p.pktid,p.flowid,p.length))
    # save_nested_list_to_csv(ideal, "ideal.csv")
    # save_nested_list_to_csv(trico, "trico.csv")
    return calculate_errors(IdealScheduler_order, RCPifoTree_order)

    # t = 0
    # for pkt in RCPifoTree_order:
    #     print(pkt.pktid,pkt.flowid,pkt.length,t)
    #     t += pkt.length


if __name__=="__main__":
    # algorithms = []
    # sizes = []
    # result = []
    # for i in range(1,11):
    #     sizes.append(i*20)

    sizes = [10]

    for root,dirs,files in os.walk("./Algorithm_1230"):
        for file in files:
            algorithms.append(os.path.join(root,file))
    pkts = read_pkts_from_file("Trace3.txt")

    algorithms = ["./Algorithm_1222/461864.txt"]
    
    for algorithm in tqdm(algorithms, desc="Algorithms"):
        for traffic_size in sizes:
            tmp_pkt = []
            cnt = 0
            for pkt in pkts:
                tmp_pkt.append(pkt)
                cnt += pkt.length
                if cnt > traffic_size*1024*1024:
                    break
            result.append((algorithm,traffic_size,get_tree_depth(algorithm),order_evaluation(algorithm, tmp_pkt)))
    print(result)

    save_nested_list_to_csv(result, "order_result_1230.csv")

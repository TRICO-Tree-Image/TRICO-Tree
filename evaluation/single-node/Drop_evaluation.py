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

random.seed(42)

PACKET_FILE = "Trace.txt"
HSCH_ALGORITHM = "paper1.txt"
# HSCH_ALGORITHM = "TreeStructure.txt"
CONG_M = 2
CONG_N = 1
# Broadcom Tomahawk 3, 1MB per port 1048576
# https://docs.broadcom.com/doc/56980-DS
# Juniper QFX5240, 2.57MB per port 2703360
# Juniper QFX5130, 4.125MB per port 4325376
BUFFER_SIZE = 1048576
# From ABM, Arista's Alpha is 14
ALPHA = 14

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

def read_pkts_from_file(file_path):
    pkts = []

    # sch_flowids = []

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
            # pkt.length = 1000
            pkts.append(pkt)
    return pkts

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


    errors = []
    for pktid in baseline_start_times:
        baseline_time = baseline_start_times[pktid]
        comparison_time = comparison_start_times[pktid]
        if comparison_time <= baseline_time:
            error = 0
        else:
            error = comparison_time - baseline_time
        errors.append(error)
        # if error > 100000:
        #     print(pktid, baseline_time, comparison_time)


    average_error = np.mean(errors)
    max_error = np.max(errors)
    pct_99_error = np.percentile(errors, 99)

    return average_error, max_error, pct_99_error

def count_extra_packet_drops(baseline, comparison):

    baseline_pktids = set(pkt.pktid for pkt in baseline)
    comparison_pktids = set(pkt.pktid for pkt in comparison)


    extra_dropped_pktids = comparison_pktids - baseline_pktids


    num_extra_drops = len(extra_dropped_pktids)

    return num_extra_drops

def run_scheduler_old(scheduler, pkts, m, n):
    drop_pkts = []
    count = 0
    queue_length = {}
    Rest_length = BUFFER_SIZE
    input_bytes = 0
    output_bytes = 0
    for f in scheduler.treepath:
        queue_length[f] = 0

    for pkt in pkts:
        if queue_length[pkt.flowid] <= ALPHA * Rest_length:
            scheduler.doenqueue(pkt)
            queue_length[pkt.flowid] += pkt.length
            Rest_length -= pkt.length
        else:
            drop_pkts.append(pkt)

        count+=1
        if count == m:
            for i in range(n):
                o_pkt = scheduler.dodequeue()
                queue_length[o_pkt.flowid] -= o_pkt.length
                Rest_length += o_pkt.length
            count = 0

    return drop_pkts

def run_scheduler(scheduler, pkts, m, n):
    drop_pkts = []
    queue_length = {}
    Rest_length = BUFFER_SIZE
    input_bytes = 0
    output_bytes = 0
    for f in scheduler.treepath:
        queue_length[f] = 0

    for pkt in pkts:
        if queue_length[pkt.flowid] <= ALPHA * Rest_length:
            scheduler.doenqueue(pkt)
            queue_length[pkt.flowid] += pkt.length
            Rest_length -= pkt.length
            input_bytes += pkt.length
        else:
            drop_pkts.append(pkt)
            input_bytes += pkt.length

        while input_bytes*n > (output_bytes + 1500)*m:
            o_pkt = scheduler.dodequeue()
            queue_length[o_pkt.flowid] -= o_pkt.length
            Rest_length += o_pkt.length
            output_bytes += o_pkt.length
        
        # if input_bytes - output_bytes > 1.05 * BUFFER_SIZE:
        if input_bytes > 1.1 * BUFFER_SIZE * (m/n)/(m/n-1):
            #print(input_bytes)
            while True:
                pkt = scheduler.dodequeue()
                if pkt is None:
                    break
                queue_length[pkt.flowid] -= pkt.length
                Rest_length += pkt.length
            input_bytes = 0
            output_bytes = 0
    return drop_pkts

def drop_evaluation(algorithm_file,pkts,m,n):

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

    #pkts = read_pkts_from_file(packet_file,PifoTree_sch.treepath)
    sch_flowids = []
    for i in PifoTree_sch.treepath:
        sch_flowids.append(i)
    random.shuffle(sch_flowids)
    for i in pkts:
        i.flowid = sch_flowids[i.oid%len(sch_flowids)]

    PifoTree_drop = []
    IdealScheduler_drop = []
    RCPifoTree_drop = []
    RCPifoTree_wo_credit_drop = []
    RCPifoTree_wo_HRC_drop = []

    i = 0

    # for pkt in pkts:
    #     PifoTree_sch.doenqueue(pkt)
    #     i+=1
    #     if i % 3 == 0:
    #         pkt = PifoTree_sch.dodequeue()
    #         PifoTree_order.append(pkt)
    PifoTree_drop = run_scheduler(PifoTree_sch,pkts,m,n)
    RCPifoTree_drop = run_scheduler(RCPifoTree_sch,pkts,m,n)
    RCPifoTree_wo_credit_drop = run_scheduler(RCPifoTree_wo_credit_sch,pkts,m,n)
    RCPifoTree_wo_HRC_drop = run_scheduler(RCPifoTree_wo_HRC_sch,pkts,m,n)
    IdealScheduler_drop = run_scheduler(IdealScheduler_sch,pkts,m,n)

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
    FIFO = []
    FIFO_drops = []
    count = 0
    queue_length = {}
    input_bytes = 0
    output_bytes = 0
    Rest_length = BUFFER_SIZE
    for f in IdealScheduler_sch.treepath:
        queue_length[f] = 0

    for pkt in pkts:
        if queue_length[pkt.flowid] <= ALPHA * Rest_length:
            FIFO.append(pkt)
            queue_length[pkt.flowid] += pkt.length
            Rest_length -= pkt.length
            input_bytes += pkt.length
        else:
            FIFO_drops.append(pkt)
            input_bytes += pkt.length

        while input_bytes*n > (output_bytes + 1500)*m:
            o_pkt = FIFO.pop(0)
            queue_length[o_pkt.flowid] -= o_pkt.length
            Rest_length += o_pkt.length
            output_bytes += o_pkt.length

        if input_bytes > 1.1 * BUFFER_SIZE * (m/n)/(m/n-1):
        #if input_bytes - output_bytes > 1.1 * BUFFER_SIZE:
            while True:
                if len(FIFO) == 0:
                    break
                pkt = FIFO.pop(0)
                queue_length[pkt.flowid] -= pkt.length
                Rest_length += pkt.length
            input_bytes = 0
            output_bytes = 0

    # output the result
    # print("PifoTree: ", count_extra_packet_drops(IdealScheduler_drop, PifoTree_drop), len(PifoTree_drop))
    # # #print("IdealScheduler: ", calculate_errors(IdealScheduler_order, IdealScheduler_order))
    # print("RCPifoTree: ", count_extra_packet_drops(IdealScheduler_drop, RCPifoTree_drop),len(RCPifoTree_drop))
    # print("RCPifoTree_wo_credit: ", count_extra_packet_drops(IdealScheduler_drop, RCPifoTree_wo_credit_drop),len(RCPifoTree_wo_credit_drop))
    # print("RCPifoTree_wo_HRC: ", count_extra_packet_drops(IdealScheduler_drop, RCPifoTree_wo_HRC_drop),len(RCPifoTree_wo_HRC_drop))
    # print("FIFO: ", count_extra_packet_drops(IdealScheduler_drop, FIFO_drops),len(FIFO_drops))
    return (count_extra_packet_drops(IdealScheduler_drop, PifoTree_drop),len(PifoTree_drop)), \
        (count_extra_packet_drops(IdealScheduler_drop, RCPifoTree_drop),len(RCPifoTree_drop)), \
        (count_extra_packet_drops(IdealScheduler_drop, RCPifoTree_wo_credit_drop),len(RCPifoTree_wo_credit_drop)), \
        (count_extra_packet_drops(IdealScheduler_drop, RCPifoTree_wo_HRC_drop),len(RCPifoTree_wo_HRC_drop)), \
        (count_extra_packet_drops(IdealScheduler_drop, FIFO_drops),len(FIFO_drops))
    # t = 0
    # for pkt in RCPifoTree_order:
    #     print(pkt.pktid,pkt.flowid,pkt.length,t)
    #     t += pkt.length


if __name__=="__main__":
    # drop_evaluation(HSCH_ALGORITHM,PACKET_FILE)

    algorithms = []
    congs = []
    result = []
    # congs = [1.5,2,2.5,3,3.5,4,4.5,5,5.5,6,6.5,7,7.5,8]
    congs = [1.25,1.5,1.75,2,2.25,2.5,2.75,3,3.25,3.5,3.75,4]
    #congs = [2]
    for root,dirs,files in os.walk("./Algorithm_1230"):
        for file in files:
            algorithms.append(os.path.join(root,file))
    pkts = read_pkts_from_file("Trace.txt")
    
    for algorithm in tqdm(algorithms, desc="Algorithms"):
        for c in congs:
            result.append((algorithm,c,get_tree_depth(algorithm),drop_evaluation(algorithm, pkts,c,1)))
    for i in result:
        print(i[3][0][0]/i[3][0][1]*100,i[3][1][0]/i[3][1][1]*100,i[3][4][0]/i[3][4][1]*100)
        print(i)
    save_nested_list_to_csv(result, "drop_result_1230.csv")

from PIFO import PIFO
from PIFO import ScheduleAlgo
import random
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


def ReplaceFlowWithTree(Algorithm1, flowid, Algorithm2, is_depth):
    treepath_1 = Algorithm1.treepath
    treepath_2 = Algorithm2.treepath
    pifo_map_1 = Algorithm1.pifo_map
    pifo_map_2 = Algorithm2.pifo_map

    # get the largest flowid and pifoid in Algorithm1
    largest_flowid = max(max(treepath_1.keys()), max(pifo_map_1.keys()))
    largest_pifo_id = largest_flowid

    # find the treepath of the given flowid in Algorithm1
    this_treepath = treepath_1[flowid]
    father = this_treepath[-1]
    for child in pifo_map_1[father].children_node:
        if child[0] == flowid:
            this_parameter = child[2]
            break

    if not is_depth:
        del treepath_1[flowid]

    # add the new tree to Algorithm1
    new_pifoid_list = []
    new_value_list = []
    for key, value in pifo_map_2.items():
        new_pifoid = key + largest_pifo_id
        new_children = []
        for child in value.children_node:
            new_children.append((child[0] + largest_pifo_id * child[1] + largest_flowid * (1 - child[1]), child[1], child[2]))
        new_value = PIFO(new_pifoid, value.schedule_algorithm, new_children)
        new_pifoid_list.append(new_pifoid)
        new_value_list.append(new_value)
        
    # update the children node in the father node
    for i in range(len(new_pifoid_list)):
        pifo_map_1[new_pifoid_list[i]] = new_value_list[i]

    temp_flowid = random.choice(list(treepath_2.keys()))
    root_of_algorithm2 = treepath_2[temp_flowid][0]
    childs = pifo_map_1[father].children_node
    new_childs = []
    for child in childs:
        if child[0] != flowid:
            new_childs.append(child)
    new_childs.append((root_of_algorithm2 + largest_pifo_id, True, this_parameter))
    pifo_map_1[father].children_node = new_childs

    new_flowid_list = []
    new_treepath_list = []
    for key, value in treepath_2.items():
        new_flowid = key + largest_flowid
        new_treepath = this_treepath.copy()
        for node in value:
            new_treepath.append(node + largest_pifo_id)
        new_flowid_list.append(new_flowid)
        new_treepath_list.append(new_treepath)

    for i in range(len(new_flowid_list)):
        treepath_1[new_flowid_list[i]] = new_treepath_list[i]

    if is_depth:
        del treepath_1[flowid]

    NewAlgorithm = ScheduleAlgo(treepath_1, pifo_map_1)
    return NewAlgorithm

def DepthExpand(Algorithm1, Algorithm2):
    treepath_1 = Algorithm1.treepath

    # find the flow with smallest depth
    min_depth = 1e10
    min_flow_id = []
    for flowid, treepath in treepath_1.items():
        if len(treepath) < min_depth:
            min_depth = len(treepath)
            min_flow_id = [flowid]
        elif len(treepath) == min_depth:
            min_flow_id.append(flowid)

    # random choose one flow with smallest depth
    flowid = random.choice(min_flow_id)

    # print(f"        replace the flow {flowid} with the tree in Algorithm2")
    return ReplaceFlowWithTree(Algorithm1, flowid, Algorithm2, True)

def BreadthExpand(Algorithm1, Algorithm2):
    treepath_1 = Algorithm1.treepath
    pifo_map_1 = Algorithm1.pifo_map
    largest_flowid = max(treepath_1.keys())

    # find the pifo node with smallest breadth
    min_breadth = 1e10
    min_pifoid = []
    for pifo_id, pifo_node in Algorithm1.pifo_map.items():
        if len(pifo_node.children_node) < min_breadth:
            min_breadth = len(pifo_node.children_node)
            min_pifoid = [pifo_id]
        elif len(pifo_node.children_node) == min_breadth:
            min_pifoid.append(pifo_id)

    # add a new flow to the smallest breadth node
    pifoid = random.choice(min_pifoid)
    flowid = largest_flowid + 1

    for value in treepath_1.values():
        if pifoid in value:
            treepath = value
            break
    treepath = treepath[:treepath.index(pifoid) + 1]
    treepath_1[flowid] = treepath
    Algorithm1.treepath = treepath_1

    father = treepath[-1]
    max_parameter = 0
    # find the largest parameter in the children of the father node
    for child in pifo_map_1[father].children_node:
        if child[2] > max_parameter:
            max_parameter = child[2]

    pifo_map_1[pifoid].children_node.append((flowid, False, max_parameter + 1))
    Algorithm1.pifo_map = pifo_map_1

    # print(f"       Add a new node to the smallest breadth node {pifoid}")
    return ReplaceFlowWithTree(Algorithm1, flowid, Algorithm2, False)

def generate(min_iter, max_iter, num_algos):
    # all filenames in the basic folder that ends with .txt
    basic_filenamelist = os.listdir("./given/basic")
    basic_filenamelist = [filename for filename in basic_filenamelist if filename.endswith(".txt")]
    pfabric_filenamelist = os.listdir("./given/pfabric")
    pfabric_filenamelist = [filename for filename in pfabric_filenamelist if filename.endswith(".txt")]

    gen_list = []
    output_list = [] 

    for i in range(num_algos):
        gen_steps = []
        basic_filename = "./given/basic/" + random.choice(basic_filenamelist)
        num_expansions = random.randint(min_iter, max_iter)
        print(f"Generate a new file from {basic_filename} with {num_expansions} operations")
        gen_steps.append(basic_filename)
        ScheduleAlgorithm = ReadScheduleAlgo(basic_filename)
        for i in range(num_expansions):
            # only the last expansion can be a pFabric file
            if i < num_expansions - 1:
                filename2 = "./given/basic/" + random.choice(basic_filenamelist)
                ScheduleAlgorithm2 = ReadScheduleAlgo(filename2)
                if random.choice([True, False]):
                    print(f"    Breadth Expansion: {filename2}")
                    ScheduleAlgorithm = BreadthExpand(ScheduleAlgorithm, ScheduleAlgorithm2)
                    gen_steps.append("Breadth Expansion: " + filename2)
                else:
                    print(f"    Depth Expansion: {filename2}")
                    ScheduleAlgorithm = DepthExpand(ScheduleAlgorithm, ScheduleAlgorithm2)
                    gen_steps.append("Depth Expansion: " + filename2)
            else:
                if random.random() < 0.3:
                    filename2 = "./given/pfabric/" + random.choice(pfabric_filenamelist)
                else:
                    filename2 = "./given/basic/" + random.choice(basic_filenamelist)
                ScheduleAlgorithm2 = ReadScheduleAlgo(filename2)
                if random.choice([True, False]):
                    print(f"    Breadth Expansion: {filename2}")
                    ScheduleAlgorithm = BreadthExpand(ScheduleAlgorithm, ScheduleAlgorithm2)
                    gen_steps.append("Breadth Expansion: " + filename2)
                else:
                    print(f"    Depth Expansion: {filename2}")
                    ScheduleAlgorithm = DepthExpand(ScheduleAlgorithm, ScheduleAlgorithm2)
                    gen_steps.append("Depth Expansion: " + filename2)
        output_name = "./generated/" + str(random.randint(0, 1000000)) + ".txt"
        if gen_steps not in gen_list:
            gen_list.append(gen_steps)
            output_list.append(output_name)
            ScheduleAlgorithm.SaveToFile(output_name)
        # ensure the generation steps are unique
        else:
            print("    This generation steps already exist, skip it")
    # write the generation steps to a file
    with open("generation_steps.txt", "w") as outfile:
        for i in range(len(gen_list)):
            outfile.write(output_list[i] + "\n")
            for step in gen_list[i]:
                outfile.write(step + "\n")
            outfile.write("\n")

def test():
    filename = "./given/basic/1.txt"
    ScheduleAlgorithm = ReadScheduleAlgo(filename)
    ScheduleAlgorithm.SaveToFile("test.txt")
    BaseAlgorithm = ReadScheduleAlgo(filename)
    ScheduleAlgorithm2  = BreadthExpand(ScheduleAlgorithm, BaseAlgorithm)
    ScheduleAlgorithm2.SaveToFile("test2.txt")

if __name__ == "__main__":
    min_iter = int(input("Please input the minimum number of expansions: "))
    max_iter = int(input("Please input the maximum number of expansions: "))
    num = int(input("Please input the number of files to generate: "))
    generate(min_iter, max_iter, num)
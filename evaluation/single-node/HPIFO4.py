import heapq
import random
from collections import deque, defaultdict
from typing import List, Tuple
import json
import ast

class Pkt:
    def __init__(self, pktid, flowid, length, flow_size):
        self.pktid = pktid
        self.flowid = flowid
        self.length = length
        self.flow_size = flow_size


class FlowQueueItem:
    def __init__(self, flowid, pkt, layercnt):
        self.flowid = flowid
        self.pkt = pkt
        self.rank = []  # Rank for each layer
        self.layercnt = layercnt  # Layer count


class PifoItem:
    def __init__(self, rank, pkt, flowid, pifoid, is_token=False):
        self.rank = rank
        self.pkt = pkt
        self.flowid = flowid
        self.pifoid = pifoid
        self.is_token = is_token

    def __lt__(self, other):
        return self.rank < other.rank  # Priority queue order


class PIFO:
    def __init__(self, pifoid, schedule_algorithm, children_node):
        self.pifoid = pifoid
        self.pifo_queue = []  # Priority queue based on the rank
        self.schedule_algorithm = schedule_algorithm
        self.children_node = children_node  # [(flowid/pifoid, is_pifo, weight/priority)]
        self.finish_time = defaultdict(float)  # Finish time per flow/child
        self.virtual_time = 0.0
        self.credit = 0.0

    def rank_cal(self, pkt, children_node_id):
        """Calculate the rank in one layer"""
        if self.schedule_algorithm == "WFQ":
            # Weighted Fair Queueing rank calculation
            weight = next((tup[2] for tup in self.children_node if tup[0] == children_node_id), -1)
            if weight > 0:
                self.finish_time[children_node_id] = max(self.virtual_time,
                                                         self.finish_time[children_node_id]) + pkt.length / weight
            return self.finish_time[children_node_id]
        elif self.schedule_algorithm == "SP":
            # Strict Priority rank calculation
            priority = next((tup[2] for tup in self.children_node if tup[0] == children_node_id), -1)
            return priority
        elif self.schedule_algorithm == "pFabric":
            return pkt.flow_size
        else:
            raise ValueError("Unsupported scheduling algorithm")

    def after_pop(self, item: PifoItem):
        if self.schedule_algorithm == "WFQ":
            # After popping a packet, update virtual time
            # self.virtual_time = max(self.virtual_time, self.finish_time[item.flowid])
            self.virtual_time = max(self.virtual_time, item.rank)


class PifoTreeScheduler:
    def __init__(self, root):
        self.treepath = defaultdict(list)  # flowid -> related PIFO ids
        self.queue_map = defaultdict(deque)  # pifoid -> queue of FlowQueueItems
        self.flow_in_pifo = defaultdict(bool)  # Judge if flow is in PIFO
        self.flows = []
        self.pifo_map = {}
        self.root = root

        self.packets_to_send = deque()
        self.stack = []

    def initialize_params(self, pifo_config: List[Tuple[int, str, List[Tuple[int, bool, int]]]], treepaths: dict,
                          root: int):
        self.root = root
        for pifoid, algorithm, children_node in pifo_config:
            self.pifo_map[pifoid] = PIFO(pifoid, algorithm, children_node)
        for flowid, path in treepaths.items():
            self.treepath[flowid] = path
            self.flow_in_pifo[flowid] = False
            self.queue_map[flowid] = deque()

    def doenqueue(self, pkt: Pkt):
        flowid = pkt.flowid
        item = FlowQueueItem(flowid, pkt, len(self.treepath[flowid]))
        # Debug log for rank computation
        # print(f"Enqueuing packet {pkt.pktid} for flow {flowid}. Calculating ranks.")

        for i in range(len(self.treepath[flowid])):
            current_pifo_id = self.treepath[flowid][i]
            next_pifo_id = self.treepath[flowid][i + 1] if i + 1 < len(self.treepath[flowid]) else flowid
            rank = self.pifo_map[current_pifo_id].rank_cal(pkt, next_pifo_id)
            item.rank.append(rank)
            # print(f"Rank for layer {i} (PIFO {current_pifo_id}) is {rank}.")

        item1=PifoItem(item.rank[-1], item.pkt, item.flowid, item.flowid)
        self.queue_map[flowid].append(item1)

        # Ready for parent node
        for i in range(len(self.treepath[flowid])):

            current_pifo_id = self.treepath[flowid][i]
            if current_pifo_id != self.root:
                rank = item.rank[i - 1]
                pifo_item = PifoItem(rank, item.pkt, flowid, current_pifo_id)
                self.queue_map[current_pifo_id].append(pifo_item)
                if not self.flow_in_pifo[current_pifo_id]:
                    self.push_to_pifo(current_pifo_id,self.treepath[flowid][i-1])

        # print(f"Item rank list: {item.rank}")
        if not self.flow_in_pifo[flowid]:
            self.push_to_pifo(flowid,self.treepath[flowid][-1])

    def push_to_pifo(self, flowid, pifoid):
        # # Judge whether the flowid is legal
        # if not self.queue_map[flowid]:
        #     return
        # # item = self.queue_map[flowid].popleft()
        # # print(f"Pushing flow {flowid} to PIFOs with ranks {item.rank}.")
        # path_from_low = self.treepath[flowid][::-1]
        # path_from_low.insert(0, flowid)

        # # Improved logic: push flow to the appropriate PIFO in the path
        # for i in range(len(path_from_low) - 1):
        #     current_pifo_id = path_from_low[i]
        #     next_pifo_id = path_from_low[i + 1] if i + 1 < len(path_from_low) else 0

        if not self.flow_in_pifo[flowid]:
            item = self.queue_map[flowid].popleft()
            # if i == 0:
            #     pifo_item = PifoItem(item.rank[-1], item.pkt, flowid, flowid)
            #     heapq.heappush(self.pifo_map[flowid].pifo_queue, pifo_item)
            # else:
            heapq.heappush(self.pifo_map[pifoid].pifo_queue, item)
        # print(f"Flow {flowid} pushed to PIFO {current_pifo_id}.")
        self.flow_in_pifo[flowid] = True

    def packets_to_send_empty(self):
        # Determine if there is a packet to be sent
        # print(len(self.packets_to_send))
        if self.packets_to_send:
            # print("packet rest")
            pkt = self.packets_to_send.popleft()
            # print(f"Dequeued packet {pkt.pktid} from Flow {pkt.flowid}, length {pkt.length}.")
            # with open("HPIFO_Pop_packet.txt", "a") as file:
            #     file.write(f"Dequeued packet {pkt.pktid} from Flow {pkt.flowid}, length {pkt.length}.\n")
            return pkt
        return None

    def dodequeue(self):
        # Determine if there is a packet to be sent
        pkt = self.packets_to_send_empty()
        if pkt:
            # print(1)
            return pkt
        if not self.pifo_map[self.root].pifo_queue:
            return Pkt(-1,-1,-1,-1)
        # take out the ref from the root and put it into stack
        while not self.packets_to_send:
            rootPifo = self.pifo_map[self.root]
            if rootPifo.pifo_queue:
                _ , item = self.push_and_pop(self.root)
                if item.pifoid in self.pifo_map:
                    self.pifo_map[item.pifoid].credit += item.pkt.length
                    self.put_id_into_stack(item.pifoid)

                # with open("HPIFO_Pop_packet.txt", "a") as file:
                #     file.write(f"ref in flow {item.flowid} in pifo {item.pifoid} with length of {item.pkt.length}, and x's credit is {self.pifo_map[2].credit}\n")
            else:
                break
            #depth - first search
            self.DFS()

        # with open("HPIFO_Credit.txt", "a") as file:
        #     file.write(f" 1 credit:   {self.pifo_map[1].credit}\n 2 credit:   {self.pifo_map[2].credit}\n")

        pkt = self.packets_to_send_empty()
        return pkt
        # print(2)

    # depth - first search
    def DFS(self):
        # while the stack is not empty
        while self.stack:
            # pop from the stack
            stackIndex = self.stack.pop()
            if stackIndex in self.pifo_map:
                currentPifo = self.pifo_map[stackIndex]
                # record the startIndex before dfs
                currentIndex = stackIndex
                # Initialize

                #  put the ref into the loop
                while currentPifo.pifo_queue:
                    # update the variant and the parameters
                    currentIndex, item  =  self.push_and_pop(currentIndex)
                    # if real packet is popped break the loop
                    if currentIndex == -1:
                        break
                    elif item.pifoid == item.flowid:
                        break
                    else:
                        self.pifo_map[item.pifoid].credit += item.pkt.length
                    # update the variant and the parameters
                    currentPifo = self.pifo_map[currentIndex]




    # take out the ref from the scheduler in one pifo node
    # push the relevant ref into the scheduler
    def push_and_pop(self,currentIndex):
        currentPifo = self.pifo_map[currentIndex]
        if currentPifo.credit <= 0 and currentIndex != self.root:
            return -1, PifoItem(0,Pkt(0,0,0,0),0,0)
        # pop from scheduler
        item = heapq.heappop(currentPifo.pifo_queue)
        currentPifo.after_pop(item)
        currentPifo.credit -= item.pkt.length

        # get the next pifo's information
        nextIndex = item.pifoid
        # the next is not flow but pifo
        if nextIndex in self.pifo_map:
            nextPifo = self.pifo_map[nextIndex]
            if self.queue_map[nextIndex]:
                # get ref from the perpifo queue
                nextItem = self.queue_map[nextIndex].popleft()
                heapq.heappush(currentPifo.pifo_queue, nextItem)

            else:
                self.flow_in_pifo[nextIndex] = False

        # the next is flow
        # pop packet
        else:
            # self.flow_in_pifo[nextIndex] = True
            self.packets_to_send.append(item.pkt)
            if self.queue_map[nextIndex]:
                # push packet from the flow
                flowitem = self.queue_map[nextIndex].popleft()
                pifo_item = PifoItem(flowitem.rank, flowitem.pkt, flowitem.flowid, flowitem.flowid)
                heapq.heappush(currentPifo.pifo_queue, pifo_item)
            else:
                self.flow_in_pifo[nextIndex] = False


        self.put_id_into_stack(currentIndex)
        return nextIndex, item



    def put_id_into_stack(self,currentIndex):
        if self.pifo_map[currentIndex].credit > 0:
            # if the credit is positive, put the ref into the stack
            self.stack.append(currentIndex)

    def save_pifo_tree_to_file(self, filename: str):
        """Save the treepath pifo_map and the packet in file"""
        data = {
            "treepath": dict(self.treepath),  # Convert to regular dictionary
            "pifo_map": {
                pifoid: {
                    "algorithm": pifo.schedule_algorithm,
                    "queue": [
                        {
                            "rank": item.rank,
                            "pkt": {
                                "pktid": item.pkt.pktid,
                                "flowid": item.pkt.flowid,
                                "length": item.pkt.length,
                                "flow_size": item.pkt.flow_size,
                            },
                            "flowid": item.flowid,
                            "pifoid": item.pifoid,
                            "is_token": item.is_token,
                        }
                        if isinstance(item, PifoItem) else {
                            "rank": item.rank,
                            "pkt": {
                                "pktid": item.pkt.pktid,
                                "flowid": item.pkt.flowid,
                                "length": item.pkt.length,
                                "flow_size": item.pkt.flow_size,
                            },
                            "layercnt": item.layercnt,
                        }
                        for item in pifo.pifo_queue
                    ],
                }
                for pifoid, pifo in self.pifo_map.items()
            },
            "queue_map": {
                flowid: [
                    {
                        "pkt": {
                            "pktid": item.pkt.pktid,
                            "flowid": item.pkt.flowid,
                            "length": item.pkt.length,
                            "flow_size": item.pkt.flow_size,
                        },
                        "layercnt": item.layercnt if isinstance(item, FlowQueueItem) else None,
                        "rank": item.rank,
                    }
                    for item in queue
                ]
                for flowid, queue in self.queue_map.items()
            },
            "flow_in_pifo": dict(self.flow_in_pifo),
        }

        with open(filename, "w") as file:
            json.dump(data, file, indent=4)

    def load_pifo_tree_from_file(self, filename: str):
        """Load PIFO tree structure and packet queues from a file"""
        with open(filename, "r") as file:
            data = json.load(file)

        self.treepath = defaultdict(list, {int(k): v for k, v in data["treepath"].items()})

        self.pifo_map = {
            int(pifoid): self._load_pifo_data(pifoid, details)
            for pifoid, details in data["pifo_map"].items()
        }

        self.queue_map = defaultdict(
            deque,
            {
                int(flowid): deque(
                    FlowQueueItem(
                        flowid=int(item["pkt"]["flowid"]),
                        pkt=Pkt(
                            pktid=item["pkt"]["pktid"],
                            flowid=item["pkt"]["flowid"],
                            length=item["pkt"]["length"],
                            flow_size=item["pkt"]["flow_size"],
                        ),
                        layercnt=item.get("layercnt", 0),  # Only FlowQueueItem has layercnt
                    ) if "layercnt" in item else PifoItem(
                        rank=item["rank"],
                        pkt=Pkt(
                            pktid=item["pkt"]["pktid"],
                            flowid=item["pkt"]["flowid"],
                            length=item["pkt"]["length"],
                            flow_size=item["pkt"]["flow_size"],
                        ),
                        flowid=item["flowid"],
                        pifoid=item["pifoid"],
                        is_token=item["is_token"],
                    )
                    for item in queue
                )
                for flowid, queue in data["queue_map"].items()
            },
        )

        self.flow_in_pifo = defaultdict(bool, {int(k): v for k, v in data["flow_in_pifo"].items()})

    def _load_pifo_data(self, pifoid, details):
        """Load a single PIFO"""
        pifo = PIFO(int(pifoid), details["algorithm"], [])
        for item in details["queue"]:
            pifo_item = PifoItem(
                rank=item["rank"],
                pkt=Pkt(
                    pktid=item["pkt"]["pktid"],
                    flowid=item["pkt"]["flowid"],
                    length=item["pkt"]["length"],
                    flow_size=item["pkt"]["flow_size"],
                ),
                flowid=item["flowid"],
                pifoid=item["pifoid"],
                is_token=item["is_token"],
            )
            heapq.heappush(pifo.pifo_queue, pifo_item)
        return pifo

def initialize_from_txt(file_path, scheduler):
    with open(file_path, 'r') as f:
        config = f.readlines()

    pifo_config = ast.literal_eval(config[0].strip().split('=')[1])
    treepaths = ast.literal_eval(config[1].strip().split('=')[1])
    root = int(config[2].strip().split('=')[1])


    scheduler.initialize_params(
        pifo_config=pifo_config,
        treepaths=treepaths,
        root=root
    )


def main():
    # Test for PIFO Tree Scheduler

    # Step 1: Initialize the PIFO Tree Scheduler with configurations and flow tree paths
    scheduler = PifoTreeScheduler(root=1)
    initialize_from_txt('TreeStructure.txt', scheduler)

    # Step 2: Enqueue packets with different flow IDs
    # Flow 3 packets (with pktid from 1 to 10)
    for x1 in range(1, 18):
        scheduler.doenqueue(Pkt(pktid=x1, flowid=3, length=100, flow_size=30))

    # Flow 5 packets (with pktid from 11 to 20)
    for x2 in range(18, 28):
        scheduler.doenqueue(Pkt(pktid=x2, flowid=5, length=100, flow_size=30))

    # Flow 4 packets (pktid 21 and 22)
    scheduler.doenqueue(Pkt(pktid=28, flowid=4, length=300, flow_size=30))
    scheduler.doenqueue(Pkt(pktid=29, flowid=4, length=300, flow_size=30))

    # Step 3: Save the current PIFO tree and queue states to a JSON file
    # scheduler.save_pifo_tree_to_file("HPIFO_after_push.json")

    # Step 4: Dequeue packets and push them to the next layer in the tree

    print("\nDequeuing packets:")
    for x3 in range(1, 35):
        pkt = scheduler.dodequeue()
        print(pkt.flowid)
        # print(scheduler.packets_to_send)
        # print(scheduler.pifo_map[1].virtual_time)
        # print(scheduler.pifo_map[1].finish_time)

    # with open("HPIFO_Pop_packet.txt", "a") as file:
    #     file.write("\n\n\n")

    # Step 5: Save the final state of the PIFO tree after dequeue operations
    # scheduler.save_pifo_tree_to_file("HPIFO_after_pop.json")


if __name__ == "__main__":
    main()
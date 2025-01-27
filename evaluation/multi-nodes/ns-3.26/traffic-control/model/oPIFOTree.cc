#include "oPIFOTree.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(oPIFOTree);

// Pkt constructor
oPIFOTree::Pkt::Pkt(Ptr<QueueDiscItem> pktid, int flowid, int length, int flow_size)
    : pktid(pktid), flowid(flowid), length(length), flow_size(flow_size) {}

// FlowQueueItem constructor
oPIFOTree::FlowQueueItem::FlowQueueItem(int flowid, Pkt pkt, int layercnt)
    : flowid(flowid), pkt(pkt), layercnt(layercnt) {}

// PifoItem constructor
oPIFOTree::PifoItem::PifoItem(double rank, Pkt pkt, int flowid, int pifoid, bool is_token)
    : rank(rank), pkt(pkt), flowid(flowid), pifoid(pifoid), is_token(is_token) {}

// PifoItem comparison operator
bool oPIFOTree::PifoItem::operator<(const PifoItem& other) const {
    return rank > other.rank; // Priority queue order (min-heap)
}

// PIFO constructors
oPIFOTree::PIFO::PIFO() : pifoid(0), virtual_time(0.0) {}

oPIFOTree::PIFO::PIFO(int pifoid, std::string schedule_algorithm, std::vector<std::tuple<int, bool, int>> children_node)
    : pifoid(pifoid), schedule_algorithm(schedule_algorithm), children_node(children_node), virtual_time(0.0) {}

// PIFO methods
double oPIFOTree::PIFO::rank_cal(Pkt pkt, int children_node_id) {
    if (schedule_algorithm == "WFQ") {
        int weight = -1;
        for (const auto& tup : children_node) {
            if (std::get<0>(tup) == children_node_id) {
                weight = std::get<2>(tup);
                break;
            }
        }
        if (weight > 0) {
            finish_time[children_node_id] = std::max(virtual_time, finish_time[children_node_id]) + pkt.length / weight;
        }
        return finish_time[children_node_id];
    } else if (schedule_algorithm == "SP") {
        int priority = -1;
        for (const auto& tup : children_node) {
            if (std::get<0>(tup) == children_node_id) {
                priority = std::get<2>(tup);
                break;
            }
        }
        return priority;
    } else if(schedule_algorithm == "pFabric") {
        return pkt.flow_size;
    }
    else {
        throw std::invalid_argument("Unsupported scheduling algorithm");
    }
}

void oPIFOTree::PIFO::after_pop(PifoItem item) {
    if (schedule_algorithm == "WFQ") {
        virtual_time = std::max(virtual_time, item.rank);
    }
}

// PifoTreeScheduler constructor
oPIFOTree::PifoTreeScheduler::PifoTreeScheduler(){}

// PifoTreeScheduler methods
void oPIFOTree::PifoTreeScheduler::initialize_params(const std::vector<std::tuple<int, std::string, std::vector<std::tuple<int, bool, int>>>>& pifo_config, const std::unordered_map<int, std::vector<int>>& treepaths, int root) {
    this->root = root;
    for (const auto& config : pifo_config) {
        int pifoid = std::get<0>(config);
        std::string algorithm = std::get<1>(config);
        std::vector<std::tuple<int, bool, int>> children_node = std::get<2>(config);
        pifo_map[pifoid] = PIFO(pifoid, algorithm, children_node);
    }
    for (const auto& path : treepaths) {
        int flowid = path.first;
        treepath[flowid] = path.second;
        flow_in_pifo[flowid] = false;
        queue_map[flowid] = std::deque<FlowQueueItem>();
    }
}

void oPIFOTree::PifoTreeScheduler::doenqueue(Pkt pkt) {
    int flowid = pkt.flowid;
    // cout<<"Flow id:"<<flowid<<" flow size"<<pkt.flow_size<<endl;
    FlowQueueItem item(flowid, pkt, treepath[flowid].size());
    for (size_t i = 0; i < treepath[flowid].size(); ++i) {
        int current_pifo_id = treepath[flowid][i];
        int next_pifo_id = (i + 1 < treepath[flowid].size()) ? treepath[flowid][i + 1] : flowid;
        double rank = pifo_map[current_pifo_id].rank_cal(pkt, next_pifo_id);
        item.rank.push_back(rank);
    }
    queue_map[flowid].push_back(item);
    if (!flow_in_pifo[flowid]) {
        push_to_pifo(flowid);
    }
}

void oPIFOTree::PifoTreeScheduler::push_to_pifo(int flowid) {
    if (queue_map[flowid].empty()) {
        return;
    }
    FlowQueueItem item = queue_map[flowid].front();
    queue_map[flowid].pop_front();
    for (size_t i = 0; i < treepath[flowid].size(); ++i) {
        int current_pifo_id = treepath[flowid][i];
        int next_pifo_id = (i + 1 < treepath[flowid].size()) ? treepath[flowid][i + 1] : flowid;
        double rank = item.rank[i];
        PifoItem pifo_item(rank, item.pkt, flowid, next_pifo_id);
        pifo_map[current_pifo_id].pifo_queue.push(pifo_item);
    }
    flow_in_pifo[flowid] = true;
}

oPIFOTree::Pkt oPIFOTree::PifoTreeScheduler::dodequeue() {
    PIFO& rootPifo = pifo_map[root];
    if (rootPifo.pifo_queue.empty()) {
        return Pkt(nullptr, -1, -1, -1); // Return an invalid packet
    }
    PifoItem item = rootPifo.pifo_queue.top();
    rootPifo.pifo_queue.pop();
    rootPifo.after_pop(item);
    while (true) {
        int nextIndex = item.pifoid;
        if (pifo_map.find(nextIndex) != pifo_map.end()) {
            PIFO& nextPifo = pifo_map[nextIndex];
            item = nextPifo.pifo_queue.top();
            nextPifo.pifo_queue.pop();
            nextPifo.after_pop(item);
        }
        if (nextIndex == item.flowid) {
            break;
        }
    }
    flow_in_pifo[item.flowid] = false;
    if (!queue_map[item.flowid].empty()) {
        push_to_pifo(item.flowid);
    }
    return item.pkt;
}

// oPIFOTree methods
TypeId oPIFOTree::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::oPIFOTree")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<oPIFOTree>();
    return tid;
}

oPIFOTree::oPIFOTree() {}

void oPIFOTree::InitializeParams(void) {
    ReadScheduleAlgo(SCHEDULING_ALGORITHM, scheduler);
    scheduler.buffer_size = BUFFER_SIZE;
    scheduler.alpha = ALPHA;
    scheduler.rest_buffer_size = BUFFER_SIZE;
    for (const auto& pair : scheduler.treepath) {
        int key = pair.first;
        scheduler.queue_length[key] = 0;
    }
    outfile.open("oPIFOTreeTrace.txt");
    if (!outfile.is_open()) {
        std::cerr << "Fail to open file" << std::endl;
    }
}

bool oPIFOTree::DoEnqueue(Ptr<QueueDiscItem> item) {
    // cnt++;
    // if (cnt % 1000 == 0) {
    //     scheduler.PrintContainerSizes();
    // }
    Ptr<Packet> packet = GetPointer(item->GetPacket());
    TenantTag my_tag;
    packet->PeekPacketTag(my_tag);
    int flow_size = my_tag.GetTenantId();
    // std::cout<<flow_size<<std::endl;

    if (flow_size == 10233) {
        flow_size = 1;
    }

    int packet_size = packet->GetSize();

    int flow_id = GetFlowLabel(item);
    //std::cout<<"Enqueue "<<flow_id<<std::endl;
    if (scheduler.queue_length[flow_id] <= scheduler.alpha * scheduler.rest_buffer_size) {
        scheduler.doenqueue(Pkt(item, flow_id, packet_size, flow_size));
        // std::cout<<flow_id<<" "<<packet_size<<" "<<flow_size<<std::endl;
        scheduler.queue_length[flow_id] += packet_size;
        scheduler.rest_buffer_size -= packet_size;
        return true;
    } else {
        Drop(item);
        return false;
    }
}

Ptr<QueueDiscItem> oPIFOTree::DoDequeue() {
    Pkt pkt = scheduler.dodequeue();
    if (pkt.flowid != -1) {
        scheduler.queue_length[pkt.flowid] -= pkt.length;
        scheduler.rest_buffer_size += pkt.length;
        outfile << pkt.flowid << " " << pkt.length << " " <<Simulator::Now().GetNanoSeconds()<< std::endl;
    }
    //std::cout<<"  Dequeue "<<pkt.flowid<<std::endl;
    return pkt.pktid;
}

Ptr<const QueueDiscItem> oPIFOTree::DoPeek(void) const {
    return 0;
}

bool oPIFOTree::CheckConfig(void) {
    return 1;
}

int oPIFOTree::GetFlowLabel(Ptr<QueueDiscItem> item) {
    Ptr<const Ipv4QueueDiscItem> ipItem =
        DynamicCast<const Ipv4QueueDiscItem>(item);

    const Ipv4Header ipHeader = ipItem->GetHeader();
    TcpHeader header;
    item->GetPacket()->PeekHeader(header);

    uint16_t sourcePort = header.GetSourcePort();
    uint16_t destPort = header.GetDestinationPort();

    int flowId = 65535;

    // std::cout<<sourcePort<<" "<<destPort<<std::endl;

    if (sourcePort < 30000) {
        flowId = sourcePort;
    } else if (destPort < 30000) {
        flowId = destPort;
    }

    if (flowId == 65535) {
        flowId = 1001;
        //cout<<"unidefined flow id, set to 1001"<<endl;
    }

    return flowId;
}

void oPIFOTree::ReadScheduleAlgo(const std::string& filename, PifoTreeScheduler& scheduler) {
    std::unordered_map<int, std::vector<int>> Treepath;
    std::vector<std::tuple<int, std::string, std::vector<std::tuple<int, bool, int>>>> pifo_config;

    std::ifstream infile(filename);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    bool in_pifo_section = false;
    bool in_flow_section = false;
    std::string line;
    while (std::getline(infile, line)) {
        // Trim
        auto isNotSpace = [](int ch) { return !std::isspace(ch); };
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), isNotSpace));
        line.erase(std::find_if(line.rbegin(), line.rend(), isNotSpace).base(), line.end());
        if (line.empty() or line == "") {
            continue;
        }

        // Check section markers
        if (line[0] == '#') {
            if (line[1] == 'P') {
                in_pifo_section = true;
                in_flow_section = false;
            } else if (line[1] == 'F') {
                in_pifo_section = false;
                in_flow_section = true;
            }
            continue;
        }

        if (in_pifo_section) {
            std::istringstream iss(line);
            int pifoid;
            std::string schedule_algorithm;
            iss >> pifoid >> schedule_algorithm;

            std::vector<std::tuple<int, bool, int>> children_node;
            std::string rest_of_line;
            {
                std::string tmp;
                std::getline(iss, tmp);
                rest_of_line += tmp;
            }

            while (true) {
                size_t pos = rest_of_line.find('{');
                if (pos == std::string::npos) break;
                size_t end_pos = rest_of_line.find('}', pos);
                if (end_pos == std::string::npos) break;

                std::string child_str = rest_of_line.substr(pos + 1, end_pos - pos - 1);
                rest_of_line.erase(0, end_pos + 1);

                std::istringstream child_iss(child_str);
                std::vector<std::string> child_parts;
                {
                    std::string c;
                    while (std::getline(child_iss, c, ',')) {
                        c.erase(std::remove_if(c.begin(), c.end(), ::isspace), c.end());
                        child_parts.push_back(c);
                    }
                }

                if (child_parts.size() == 3) {
                    int child_id = std::stoi(child_parts[0]);
                    bool is_pifo = (child_parts[1] == "true");
                    int parameter = std::stoi(child_parts[2]);
                    children_node.emplace_back(child_id, is_pifo, parameter);
                }
            }
            pifo_config.emplace_back(pifoid, schedule_algorithm, children_node);

        } else if (in_flow_section) {
            std::istringstream iss(line);
            int flowid;
            iss >> flowid;

            std::vector<int> treepath;
            std::string token;
            while (iss >> token) {
                std::string digits;
                for (char c : token) {
                    if (isdigit(static_cast<unsigned char>(c))) {
                        digits.push_back(c);
                    }
                }
                if (!digits.empty()) {
                    treepath.push_back(std::stoi(digits));
                }
            }
            Treepath[flowid] = treepath;
        }
    }

    int r = -1;
    if (!Treepath.empty()) {
        r = Treepath.begin()->second[0];
    }
    scheduler.initialize_params(pifo_config, Treepath, r);
}

void
oPIFOTree::PifoTreeScheduler::PrintContainerSizes()
{
    // std::cout << "treepath.size() = " << treepath.size() << std::endl;
    // std::cout << "queue_map.size() = " << queue_map.size() << std::endl;
    // std::cout << "flow_in_pifo.size() = " << flow_in_pifo.size() << std::endl;
    // std::cout << "pifo_map.size() = " << pifo_map.size() << std::endl;
    // std::cout << "queue_length.size() = " << queue_length.size() << std::endl;

    // for (auto& kv : pifo_map) {
    //     std::cout << "  PIFO " << kv.first << ":" << std::endl;
    //     std::cout << "    pifo_queue.size() = " << kv.second.pifo_queue.size() << std::endl;
    //     std::cout << "    children_node.size() = " << kv.second.children_node.size() << std::endl;
    //     std::cout << "    finish_time.size() = " << kv.second.finish_time.size() << std::endl;
    // }

    // for (auto& kv : queue_map) {
    //     if (!kv.second.empty()) {
    //         std::cout << "  queue_map[" << kv.first << "].size() = " << kv.second.size() << std::endl;
    //     }
    // }
    // std::cout <<"--------------------------------------------------------------------------"<<std::endl;
}

} // namespace ns3
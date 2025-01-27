#include "perPIFOonly.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(perPIFOonly);

perPIFOonly::perPIFOonly() : scheduler(1) {}

TypeId perPIFOonly::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::perPIFOonly")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<perPIFOonly>();
    return tid;
}

void perPIFOonly::InitializeParams(void) {
    ReadScheduleAlgo(SCHEDULING_ALGORITHM, scheduler);
    scheduler.buffer_size = BUFFER_SIZE;
    scheduler.alpha = ALPHA;
    scheduler.rest_buffer_size = BUFFER_SIZE;
    for (const auto& pair : scheduler.treepath) {
        int key = pair.first;
        scheduler.queue_length[key] = 0;
    }
}

bool perPIFOonly::DoEnqueue(Ptr<QueueDiscItem> item) {
    Ptr<Packet> packet = GetPointer(item->GetPacket());
    TenantTag my_tag;
    packet->PeekPacketTag(my_tag);
    int flow_size = my_tag.GetTenantId();

    if (flow_size == 10233) {
        flow_size = 1;
    }

    int packet_size = packet->GetSize();
    int flow_id = GetFlowLabel(item);

    if (scheduler.queue_length[flow_id] <= scheduler.alpha * scheduler.rest_buffer_size) {
        scheduler.doenqueue(Pkt(item, flow_id, packet_size, flow_size));
        scheduler.queue_length[flow_id] += packet_size;
        scheduler.rest_buffer_size -= packet_size;
        return true;
    } else {
        Drop(item);
        return false;
    }
}

Ptr<QueueDiscItem> perPIFOonly::DoDequeue() {
    Pkt pkt = scheduler.dodequeue();
    if (pkt.flowid != -1) {
        scheduler.queue_length[pkt.flowid] -= pkt.length;
        scheduler.rest_buffer_size += pkt.length;
    }
    return pkt.pktid;
}

Ptr<const QueueDiscItem> perPIFOonly::DoPeek(void) const {
    return nullptr;
}

bool perPIFOonly::CheckConfig(void) {
    return true;
}

int perPIFOonly::GetFlowLabel(Ptr<QueueDiscItem> item) {
    Ptr<const Ipv4QueueDiscItem> ipItem = DynamicCast<const Ipv4QueueDiscItem>(item);
    const Ipv4Header ipHeader = ipItem->GetHeader();
    TcpHeader header;
    item->GetPacket()->PeekHeader(header);

    uint16_t sourcePort = header.GetSourcePort();
    uint16_t destPort = header.GetDestinationPort();

    int flowId = 65535;

    if (sourcePort < 30000) {
        flowId = sourcePort;
    } else if (destPort < 30000) {
        flowId = destPort;
    }

    if (flowId == 65535) {
        flowId = 1001;
        cout<<"unidefined flow id, set to 1001"<<endl;
    }

    return flowId;
}

// Implementation of Pkt class
perPIFOonly::Pkt::Pkt(Ptr<QueueDiscItem> pktid, int flowid, int length, int flow_size)
    : pktid(pktid), flowid(flowid), length(length), flow_size(flow_size) {}

// Implementation of FlowQueueItem class
perPIFOonly::FlowQueueItem::FlowQueueItem(int flowid, Pkt pkt, int layercnt)
    : flowid(flowid), pkt(pkt), layercnt(layercnt) {}

// Implementation of PifoItem class
perPIFOonly::PifoItem::PifoItem(double rank, Pkt pkt, int flowid, int pifoid, bool is_token)
    : rank(rank), pkt(pkt), flowid(flowid), pifoid(pifoid), is_token(is_token) {}

bool perPIFOonly::PifoItem::operator<(const PifoItem& other) const {
    return rank > other.rank;
}

// Implementation of PIFO class
perPIFOonly::PIFO::PIFO() : pifoid(0), virtual_time(0.0) {}

perPIFOonly::PIFO::PIFO(int pifoid, std::string schedule_algorithm, std::vector<std::tuple<int, bool, int>> children_node)
    : pifoid(pifoid), schedule_algorithm(schedule_algorithm), children_node(children_node), virtual_time(0.0) {}

double perPIFOonly::PIFO::rank_cal(Pkt pkt, int children_node_id) {
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
    } else if (schedule_algorithm == "pFabric") {
        return pkt.flow_size;
    } else {
        throw std::invalid_argument("Unsupported scheduling algorithm");
    }
}

void perPIFOonly::PIFO::after_pop(PifoItem item) {
    if (schedule_algorithm == "WFQ") {
        virtual_time = std::max(virtual_time, item.rank);
    }
}

// Implementation of PifoTreeScheduler class
perPIFOonly::PifoTreeScheduler::PifoTreeScheduler(int root) : root(root) {}

void perPIFOonly::PifoTreeScheduler::initialize_params(const std::vector<std::tuple<int, std::string, std::vector<std::tuple<int, bool, int>>>>& pifo_config, const std::unordered_map<int, std::vector<int>>& treepaths, int root) {
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
        queue_map[flowid] = std::deque<PifoItem>();
    }
}

void perPIFOonly::PifoTreeScheduler::doenqueue(Pkt pkt) {
    int flowid = pkt.flowid;
    FlowQueueItem item(flowid, pkt, treepath[flowid].size());
    for (size_t i = 0; i < treepath[flowid].size(); ++i) {
        int current_pifo_id = treepath[flowid][i];
        int next_pifo_id = (i + 1 < treepath[flowid].size()) ? treepath[flowid][i + 1] : flowid;
        double rank = pifo_map[current_pifo_id].rank_cal(pkt, next_pifo_id);
        item.rank.push_back(rank);
    }
    PifoItem pifo_item(item.rank.back(), item.pkt, item.flowid, item.flowid);
    queue_map[flowid].push_back(pifo_item);

    for (size_t i = 0; i < treepath[flowid].size(); ++i) {
        int current_pifo_id = treepath[flowid][i];
        if (current_pifo_id != root) {
            double rank = item.rank[i - 1];
            PifoItem pifo_item(rank, item.pkt, flowid, current_pifo_id);
            queue_map[current_pifo_id].push_back(pifo_item);
            if (!flow_in_pifo[current_pifo_id]) {
                push_to_pifo(current_pifo_id, treepath[flowid][i - 1]);
            }
        }
    }

    if (!flow_in_pifo[flowid]) {
        push_to_pifo(flowid, treepath[flowid].back());
    }
}

void perPIFOonly::PifoTreeScheduler::push_to_pifo(int flowid, int pifoid) {
    if (!flow_in_pifo[flowid]) {
        if (!queue_map[flowid].empty()) {
            PifoItem item = queue_map[flowid].front();
            queue_map[flowid].pop_front();
            pifo_map[pifoid].pifo_queue.push(item);
        }
        flow_in_pifo[flowid] = true;
    }
}

perPIFOonly::Pkt perPIFOonly::PifoTreeScheduler::dodequeue() {
    PIFO& rootPifo = pifo_map[root];
    PIFO* currentPifo = &rootPifo;

    if (currentPifo->pifo_queue.empty()) {
        return Pkt(nullptr, -1, -1, -1);
    }
    while (true) {
        PifoItem item = currentPifo->pifo_queue.top();
        currentPifo->pifo_queue.pop();
        currentPifo->after_pop(item);
        if (item.pifoid == item.flowid) {
            if (!queue_map[item.pifoid].empty()) {
                PifoItem nextItem = queue_map[item.pifoid].front();
                queue_map[item.pifoid].pop_front();
                currentPifo->pifo_queue.push(nextItem);
            } else {
                flow_in_pifo[item.pifoid] = false;
            }
            return item.pkt;
        }
        if (!queue_map[item.pifoid].empty()) {
            PifoItem nextItem = queue_map[item.pifoid].front();
            queue_map[item.pifoid].pop_front();
            currentPifo->pifo_queue.push(nextItem);
        } else {
            flow_in_pifo[item.pifoid] = false;
        }
        currentPifo = &pifo_map[item.pifoid];
    }
}

void perPIFOonly::ReadScheduleAlgo(const std::string& filename, PifoTreeScheduler& scheduler) {
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

} // namespace ns3

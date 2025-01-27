#include "CreditOnly.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(CreditOnly);

CreditOnly::CreditOnly() : scheduler(1) {}

TypeId CreditOnly::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::CreditOnly")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<CreditOnly>();
    return tid;
}

void CreditOnly::InitializeParams(void) {
    ReadScheduleAlgo(SCHEDULING_ALGORITHM, scheduler);
    scheduler.buffer_size = BUFFER_SIZE;
    scheduler.alpha = ALPHA;
    scheduler.rest_buffer_size = BUFFER_SIZE;
    for (const auto& pair : scheduler.treepath) {
        int key = pair.first;
        scheduler.queue_length[key] = 0;
    }
}

bool CreditOnly::DoEnqueue(Ptr<QueueDiscItem> item) {
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

Ptr<QueueDiscItem> CreditOnly::DoDequeue() {
    Pkt pkt = scheduler.dodequeue();
    if (pkt.flowid != -1) {
        scheduler.queue_length[pkt.flowid] -= pkt.length;
        scheduler.rest_buffer_size += pkt.length;
    }
    return pkt.pktid;
}

Ptr<const QueueDiscItem> CreditOnly::DoPeek(void) const {
    return nullptr;
}

bool CreditOnly::CheckConfig(void) {
    return true;
}

int CreditOnly::GetFlowLabel(Ptr<QueueDiscItem> item) {
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
CreditOnly::Pkt::Pkt(Ptr<QueueDiscItem> pktid, int flowid, int length, int flow_size)
    : pktid(pktid), flowid(flowid), length(length), flow_size(flow_size) {}

// Implementation of FlowQueueItem class
CreditOnly::FlowQueueItem::FlowQueueItem(int flowid, Pkt pkt, int layercnt)
    : flowid(flowid), pkt(pkt), layercnt(layercnt) {}

// Implementation of PifoItem class
CreditOnly::PifoItem::PifoItem() : rank(0.0), pkt(Pkt(0, 0, 0, 0)), flowid(0), pifoid(0), is_token(false) {}

CreditOnly::PifoItem::PifoItem(double rank, Pkt pkt, int flowid, int pifoid, bool is_token)
    : rank(rank), pkt(pkt), flowid(flowid), pifoid(pifoid), is_token(is_token) {}

bool CreditOnly::PifoItem::operator<(const PifoItem& other) const {
    return rank > other.rank;
}

// Implementation of PIFO class
CreditOnly::PIFO::PIFO() : pifoid(0), virtual_time(0.0), credit(0.0) {}

CreditOnly::PIFO::PIFO(int pifoid, std::string schedule_algorithm, std::vector<std::tuple<int, bool, int>> children_node)
    : pifoid(pifoid), schedule_algorithm(schedule_algorithm), children_node(children_node), virtual_time(0.0), credit(0.0) {}

double CreditOnly::PIFO::rank_cal(Pkt pkt, int children_node_id) {
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

void CreditOnly::PIFO::after_pop(PifoItem item) {
    if (schedule_algorithm == "WFQ") {
        virtual_time = std::max(virtual_time, item.rank);
    }
}

// Implementation of PifoTreeScheduler class
CreditOnly::PifoTreeScheduler::PifoTreeScheduler(int root) : root(root) {}

void CreditOnly::PifoTreeScheduler::initialize_params(const std::vector<std::tuple<int, std::string, std::vector<std::tuple<int, bool, int>>>>& pifo_config, const std::unordered_map<int, std::vector<int>>& treepaths, int root) {
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

void CreditOnly::PifoTreeScheduler::doenqueue(Pkt pkt) {
    int flowid = pkt.flowid;
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

void CreditOnly::PifoTreeScheduler::push_to_pifo(int flowid) {
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

CreditOnly::Pkt CreditOnly::PifoTreeScheduler::packets_to_send_empty() {
    if (!packets_to_send.empty()) {
        Pkt pkt = packets_to_send.front();
        packets_to_send.pop_front();
        return pkt;
    }
    return Pkt(nullptr, -1, -1, -1);
}

CreditOnly::Pkt CreditOnly::PifoTreeScheduler::dodequeue() {
    Pkt pkt = packets_to_send_empty();
    if (pkt.pktid != nullptr) {
        return pkt;
    }
    while (packets_to_send.empty()) {
        PIFO& rootPifo = pifo_map[root];
        if (!rootPifo.pifo_queue.empty()) {
            int nextIndex;
            PifoItem item;
            std::tie(nextIndex, item) = push_and_pop(root);
            if (pifo_map.find(item.pifoid) != pifo_map.end()) {
                pifo_map[item.pifoid].credit += item.pkt.length;
                put_id_into_stack(item.pifoid);
            }
        } else {
            break;
        }
        DFS();
    }
    return packets_to_send_empty();
}

void CreditOnly::PifoTreeScheduler::DFS() {
    while (!stack.empty()) {
        int stackIndex = stack.back();
        stack.pop_back();
        if (pifo_map.find(stackIndex) != pifo_map.end()) {
            PIFO* currentPifo = &pifo_map[stackIndex];
            int currentIndex = stackIndex;
            while (!currentPifo->pifo_queue.empty()) {
                int nextIndex;
                PifoItem item;
                std::tie(currentIndex, item) = push_and_pop(currentIndex);
                if (currentIndex == -1) {
                    break;
                } else if (flow_in_pifo.find(item.pifoid) != flow_in_pifo.end()) {
                    break;
                } else {
                    pifo_map[item.pifoid].credit += item.pkt.length;
                }
                currentPifo = &pifo_map[currentIndex];
            }
        }
    }
}

std::tuple<int, CreditOnly::PifoItem> CreditOnly::PifoTreeScheduler::push_and_pop(int currentIndex) {
    PIFO& currentPifo = pifo_map[currentIndex];
    if (currentPifo.credit <= 0 && currentIndex != root) {
        return std::make_tuple(-1, PifoItem(0, Pkt(0, 0, 0, 0), 0, 0));
    }
    PifoItem item = currentPifo.pifo_queue.top();
    currentPifo.pifo_queue.pop();
    currentPifo.after_pop(item);
    currentPifo.credit -= item.pkt.length;
    int nextIndex = item.pifoid;

    if (flow_in_pifo.find(nextIndex) != flow_in_pifo.end()) {
        packets_to_send.push_back(item.pkt);
        if (!queue_map[nextIndex].empty()) {
            push_to_pifo(nextIndex);
        } else {
            flow_in_pifo[nextIndex] = false;
        }
    }

    put_id_into_stack(currentIndex);
    return std::make_tuple(nextIndex, item);
}

void CreditOnly::PifoTreeScheduler::put_id_into_stack(int currentIndex) {
    if (pifo_map[currentIndex].credit > 0) {
        stack.push_back(currentIndex);
    }
}

void CreditOnly::ReadScheduleAlgo(const std::string& filename, PifoTreeScheduler& scheduler) {
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

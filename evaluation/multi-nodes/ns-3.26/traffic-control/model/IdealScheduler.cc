#include "IdealScheduler.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(IdealScheduler);

const double IdealScheduler::ALPHA = 14;
const std::string IdealScheduler::SCHEDULING_ALGORITHM = "exp1.txt";

// Pkt class implementation
IdealScheduler::Pkt::Pkt() : pktid(nullptr), flowid(0), length(0), flow_size(0) {}

IdealScheduler::Pkt::Pkt(Ptr<QueueDiscItem> pktid, int flowid, int length, int flow_size)
    : pktid(pktid), flowid(flowid), length(length), flow_size(flow_size) {}

// PifoItem class implementation
IdealScheduler::PifoItem::PifoItem() : rank(0.0), pkt(Pkt(nullptr, 0, 0, 0)), flowid(0), pifoid(0), is_token(false) {}

IdealScheduler::PifoItem::PifoItem(double rank, Pkt pkt, int flowid, int pifoid, bool is_token)
    : rank(rank), pkt(pkt), flowid(flowid), pifoid(pifoid), is_token(is_token) {}

void IdealScheduler::PifoItem::item_rank(double rank) {
    this->rank = rank;
}

bool IdealScheduler::PifoItem::operator<(const PifoItem& other) const {
    return rank > other.rank; // Priority queue order (min-heap)
}

// PIFO class implementation
IdealScheduler::PIFO::PIFO() : pifoid(0), virtual_time(0.0) {}

IdealScheduler::PIFO::PIFO(int pifoid, std::string schedule_algorithm, std::vector<std::tuple<int, bool, int>> children_node)
    : pifoid(pifoid), schedule_algorithm(schedule_algorithm), children_node(children_node), virtual_time(0.0) {
    for (const auto& child : children_node) {
        finish_time[std::get<0>(child)] = -1.0;
    }
}

double IdealScheduler::PIFO::rank_cal(Pkt pkt, int children_node_id) {
    if (schedule_algorithm == "WFQ") {
        int weight = -1;
        for (const auto& tup : children_node) {
            if (std::get<0>(tup) == children_node_id) {
                weight = std::get<2>(tup);
                break;
            }
        }
        if (weight > 0) {
            if (std::fabs(static_cast<float>(finish_time[children_node_id] + 1.0)) < 1e-6) {
                finish_time[children_node_id] = virtual_time;
                return virtual_time + pkt.length / weight;
            } else {
                return finish_time[children_node_id] + pkt.length / weight;
            }
        }
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
    return -1.0;
}

void IdealScheduler::PIFO::after_pop(Pkt pkt, int children_node_id) {
    if (schedule_algorithm == "WFQ") {
        int weight = -1;
        for (const auto& tup : children_node) {
            if (std::get<0>(tup) == children_node_id) {
                weight = std::get<2>(tup);
                break;
            }
        }
        if (weight > 0) {
            if (std::fabs(static_cast<float>(finish_time[children_node_id] + 1.0)) < 1e-6) {
                finish_time[children_node_id] = virtual_time;
            }
            finish_time[children_node_id] += pkt.length / weight;
            virtual_time = std::max(virtual_time, finish_time[children_node_id]);
        }
    }
}

// PifoTreeScheduler class implementation
IdealScheduler::PifoTreeScheduler::PifoTreeScheduler(int root) : root(root) {}

void IdealScheduler::PifoTreeScheduler::initialize_params(const std::vector<std::tuple<int, std::string, std::vector<std::tuple<int, bool, int>>>>& pifo_config, const std::unordered_map<int, std::vector<int>>& treepaths, int root) {
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

void IdealScheduler::PifoTreeScheduler::virtual_time_update(PifoItem item) {
    int flowid = item.flowid;
    Pkt pkt = item.pkt;
    for (size_t i = 0; i < treepath[flowid].size(); ++i) {
        int current_pifo_id = treepath[flowid][i];
        int next_pifo_id = (i + 1 < treepath[flowid].size()) ? treepath[flowid][i + 1] : flowid;
        pifo_map[current_pifo_id].after_pop(pkt, next_pifo_id);
    }
}

void IdealScheduler::PifoTreeScheduler::doenqueue(Pkt pkt) {
    int flowid = pkt.flowid;
    PifoItem item(-1, pkt, flowid, flowid);
    queue_map[flowid].push_back(item);
}

IdealScheduler::Pkt IdealScheduler::PifoTreeScheduler::dodequeue() {
    PIFO& rootPifo = pifo_map[root];
    PifoItem item = dequeue(rootPifo);
    if (item.pkt.pktid != nullptr) {
        virtual_time_update(item);
        return pop_packet(item).pkt;
    } else {
        return Pkt(nullptr, -1, -1, -1); // Return an invalid packet
    }
}

IdealScheduler::PifoItem IdealScheduler::PifoTreeScheduler::dequeue(PIFO& currentPifo) {
    PifoItem best_item;
    double best_rank = std::numeric_limits<double>::infinity();
    std::vector<PifoItem> item_list;
    for (const auto& child : currentPifo.children_node) {
        int child_pifo_id = std::get<0>(child);
        if (pifo_map.find(child_pifo_id) != pifo_map.end()) {
            PifoItem item = dequeue(pifo_map[child_pifo_id]);
            if (item.pkt.pktid != nullptr) {
                double rank = currentPifo.rank_cal(item.pkt, child_pifo_id);
                item.item_rank(rank);
                item_list.push_back(item);
            } else {
                if (currentPifo.schedule_algorithm == "WFQ") {
                    currentPifo.finish_time[child_pifo_id] = -1.0;
                }
            }
        } else {
            if (!queue_map[child_pifo_id].empty()) {
                PifoItem item = queue_map[child_pifo_id].front();
                double rank = currentPifo.rank_cal(item.pkt, child_pifo_id);
                item.item_rank(rank);
                item_list.push_back(item);
            } else {
                if (currentPifo.schedule_algorithm == "WFQ") {
                    currentPifo.finish_time[child_pifo_id] = -1.0;
                }
            }
        }
    }

    for (const auto& item : item_list) {
        if (item.rank < best_rank) {
            best_item = item;
            best_rank = item.rank;
        }
    }
    return best_item;
}

IdealScheduler::PifoItem IdealScheduler::PifoTreeScheduler::pop_packet(PifoItem item) {
    int flowid = item.flowid;
    if (!queue_map[flowid].empty()) {
        PifoItem flow_item = queue_map[flowid].front();
        queue_map[flowid].pop_front();
        return flow_item;
    }
    return PifoItem();
}

// IdealScheduler class implementation
TypeId IdealScheduler::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::IdealScheduler")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<IdealScheduler>();
    return tid;
}

IdealScheduler::IdealScheduler() : scheduler(1) {}

void IdealScheduler::InitializeParams(void) {
    scheduler = PifoTreeScheduler(1);
    ReadScheduleAlgo(SCHEDULING_ALGORITHM, scheduler);
    scheduler.buffer_size = BUFFER_SIZE;
    scheduler.alpha = ALPHA;
    scheduler.rest_buffer_size = BUFFER_SIZE;
    for (const auto& pair : scheduler.treepath) {
        int key = pair.first;
        scheduler.queue_length[key] = 0;
    }
}

bool IdealScheduler::DoEnqueue(Ptr<QueueDiscItem> item) {
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

Ptr<QueueDiscItem> IdealScheduler::DoDequeue() {
    Pkt pkt = scheduler.dodequeue();
    if (pkt.flowid != -1) {
        scheduler.queue_length[pkt.flowid] -= pkt.length;
        scheduler.rest_buffer_size += pkt.length;
    }
    return pkt.pktid;
}

Ptr<const QueueDiscItem> IdealScheduler::DoPeek(void) const {
    return 0;
}

bool IdealScheduler::CheckConfig(void) {
    return 1;
}

void IdealScheduler::ReadScheduleAlgo(const std::string& filename, PifoTreeScheduler& scheduler) {
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

int IdealScheduler::GetFlowLabel(Ptr<QueueDiscItem> item) {
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

} // namespace ns3

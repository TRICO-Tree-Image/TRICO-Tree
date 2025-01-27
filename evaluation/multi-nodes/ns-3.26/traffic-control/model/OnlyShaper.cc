#include "OnlyShaper.h"
#include "ns3/log.h"

#include <fstream>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("OnlyShaper");

NS_OBJECT_ENSURE_REGISTERED(OnlyShaper);

const double OnlyShaper::ALPHA = 14;
const double OnlyShaper::SHAPER_RATE = 52428800/5; // Default rate for credit update

OnlyShaper::OnlyShaper() {
    InitializeParams();
    lastUpdateTime = Simulator::Now().GetSeconds(); // Initialize last update time
}

TypeId OnlyShaper::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::OnlyShaper")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<OnlyShaper>();
    return tid;
}

void OnlyShaper::InitializeParams(void) {
    buffer_size = BUFFER_SIZE;
    alpha = ALPHA;
    rest_buffer_size = BUFFER_SIZE;
    queue_length = 0;

    // Initialize log file
    outfile.open("CslTestResult/OnlyShaperTrace.txt");
    if (!outfile.is_open()) {
        std::cerr << "Failed to open file: CslTestResult/OnlyShaperTrace.txt" << std::endl;
    }

    // Initialize credit for each flow
    for (auto &entry : flowQueues) {
        entry.second.credit = 0.0;
    }

    NS_LOG_INFO("OnlyShaper initialized with buffer size: " << BUFFER_SIZE);
}

bool OnlyShaper::DoEnqueue(Ptr<QueueDiscItem> item) {
    Ptr<Packet> packet = GetPointer(item->GetPacket());
    int packet_size = packet->GetSize();

    int flowId = GetFlowLabel(item);

    // Initialize flow queue if it does not exist
    if (flowQueues.find(flowId) == flowQueues.end()) {
        flowQueues[flowId] = QueueInfo();
        flowQueues[flowId].credit = 0.0;
        NS_LOG_INFO("New flow initialized with ID: " << flowId);
    }

    // Check if the queue can accept the packet
    if (queue_length <= alpha * rest_buffer_size) {
        flowQueues[flowId].fifo.push(item);
        queue_length += packet_size;
        rest_buffer_size -= packet_size;
        NS_LOG_INFO("Packet enqueued in flow ID: " << flowId << ", Packet size: " << packet_size);
        outfile << "Enqueue: Flow ID: " << flowId << ", Packet size: " << packet_size << ", Queue length: " << queue_length << std::endl;
        return true;
    } else {
        Drop(item);
        NS_LOG_WARN("Packet dropped for flow ID: " << flowId << ", Queue full.");
        outfile << "Drop: Flow ID: " << flowId << ", Packet size: " << packet_size << std::endl;
        return false;
    }
}

Ptr<QueueDiscItem> OnlyShaper::DoDequeue() {
    // Update credit values
    double currentTime = Simulator::Now().GetSeconds();
    double timeInterval = currentTime - lastUpdateTime;
    UpdateCredit(timeInterval);
    lastUpdateTime = currentTime;

    for (auto &entry : flowQueues) {
        int flowId = entry.first;
        QueueInfo &queueInfo = entry.second;

        // Check if the flow queue has packets and sufficient credit
        if (!queueInfo.fifo.empty() && queueInfo.credit >= 0) {
            Ptr<QueueDiscItem> q = queueInfo.fifo.front();
            queueInfo.fifo.pop();
            
            Ptr<Packet> packet = GetPointer(q->GetPacket());
            int packet_size = packet->GetSize();
            
            // Update queue length and credit
            queue_length -= packet_size;
            rest_buffer_size += packet_size;
            queueInfo.credit -= packet_size;
            if (queueInfo.fifo.empty()) {
                queueInfo.credit = 0.0;
            }

            // Log the dequeue operation
            outfile << "Dequeue: Flow ID: " << flowId << ", Packet size: " << packet_size << ", Remaining Queue length: " << queue_length << std::endl;
            NS_LOG_INFO("Packet dequeued from flow ID: " << flowId << ", Packet size: " << packet_size);
            return q;
        }
    }

    NS_LOG_INFO("No packets available for dequeue.");
    outfile << "Dequeue: No packets available" << std::endl;
    return nullptr; // No schedulable queues
}

Ptr<const QueueDiscItem> OnlyShaper::DoPeek(void) const {
    NS_LOG_INFO("Peek called, no implementation available.");
    return nullptr; // Not implemented
}

bool OnlyShaper::CheckConfig(void) {
    NS_LOG_INFO("CheckConfig called, returning true.");
    return true;
}

int OnlyShaper::GetFlowLabel(Ptr<QueueDiscItem> item) {
    Ptr<const Ipv4QueueDiscItem> ipItem =
        DynamicCast<const Ipv4QueueDiscItem>(item);

    const Ipv4Header ipHeader = ipItem->GetHeader();
    TcpHeader header;
    item->GetPacket()->PeekHeader(header);

    uint16_t sourcePort = header.GetSourcePort();
    uint16_t destPort   = header.GetDestinationPort();

    int flowId = 65535;

    if (sourcePort < 30000) {
        flowId = sourcePort;
    } else if (destPort < 30000) {
        flowId = destPort;
    } else {
        flowId = 1001; // Default flow ID
    }

    NS_LOG_INFO("Flow label generated: " << flowId);
    outfile << "FlowLabel: SourcePort: " << sourcePort << ", DestPort: " << destPort << ", Flow ID: " << flowId << std::endl;
    return flowId;
}

void OnlyShaper::UpdateCredit(double timeInterval) {
    for (auto &entry : flowQueues) {
        entry.second.credit += timeInterval * SHAPER_RATE;
        NS_LOG_INFO("Updated credit for flow ID: " << entry.first << ", New credit: " << entry.second.credit);
        outfile << "CreditUpdate: Flow ID: " << entry.first << ", Credit: " << entry.second.credit << std::endl;
    }
}

} // namespace ns3

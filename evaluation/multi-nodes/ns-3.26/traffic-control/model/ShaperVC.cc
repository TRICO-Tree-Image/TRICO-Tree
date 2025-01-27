#include "ShaperVC.h"
#include "ns3/log.h"

#include <fstream>
#include <sstream>
#include <queue>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ShaperVC");

NS_OBJECT_ENSURE_REGISTERED(ShaperVC);

const double ShaperVC::ALPHA = 14;
const double ShaperVC::SHAPER_RATE = 52428800 * 2; // Default rate for credit update

ShaperVC::ShaperVC() {
    InitializeParams();
}

TypeId ShaperVC::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::ShaperVC")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<ShaperVC>();
    return tid;
}

void ShaperVC::InitializeParams(void) {
    buffer_size = BUFFER_SIZE;
    alpha = ALPHA;
    rest_buffer_size = BUFFER_SIZE;
    queue_length = 0;
    currentTime = Simulator::Now().GetSeconds();

    // Initialize log file
    outfile.open("CslTestResult/ShaperVCTrace.txt");
    if (!outfile.is_open()) {
        std::cerr << "Failed to open file: CslTestResult/ShaperVCTrace.txt" << std::endl;
    }

    NS_LOG_INFO("ShaperVC initialized with buffer size: " << BUFFER_SIZE);
}

bool ShaperVC::DoEnqueue(Ptr<QueueDiscItem> item) {
    currentTime = Simulator::Now().GetSeconds();

    Ptr<Packet> packet = GetPointer(item->GetPacket());
    int packet_size = packet->GetSize();

    int flowId = GetFlowLabel(item);

    // Initialize flow queue if it does not exist
    if (flowQueues.find(flowId) == flowQueues.end()) {
        flowQueues[flowId] = QueueInfo();
        flowQueues[flowId].credit = 0.0;
        flowQueues[flowId].finishTime = 0.0;
        flowQueues[flowId].weight = 1; // Default weight
        NS_LOG_INFO("New flow initialized with ID: " << flowId);
    }

    // Check if the queue can accept the packet
    if (queue_length <= alpha * rest_buffer_size) {
        flowQueues[flowId].fifo.push(item);
        queue_length += packet_size;
        rest_buffer_size -= packet_size;

        // Update finish time for the flow
        UpdateFinishTime(flowId, packet_size);

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

Ptr<QueueDiscItem> ShaperVC::DoDequeue() {
    currentTime = Simulator::Now().GetSeconds();

    // Update credit values
    UpdateCredit(currentTime);

    // Find the flow with the smallest finish time and positive credit
    int selectedFlowId = -1;
    double minFinishTime = std::numeric_limits<double>::max();

    for (const auto &entry : flowQueues) {
        const QueueInfo &queueInfo = entry.second;
        if (!queueInfo.fifo.empty() && queueInfo.credit >= 0 && queueInfo.finishTime < minFinishTime) {
            minFinishTime = queueInfo.finishTime;
            selectedFlowId = entry.first;
        }
    }

    // If no flow satisfies the condition, return nullptr
    if (selectedFlowId == -1) {
        NS_LOG_INFO("No packets available for dequeue due to insufficient credit.");
        outfile << "Dequeue: No packets available due to credit constraints" << std::endl;
        return nullptr;
    }

    // Dequeue from the selected flow
    QueueInfo &queueInfo = flowQueues[selectedFlowId];
    Ptr<QueueDiscItem> q = queueInfo.fifo.front();
    queueInfo.fifo.pop();
    if (queueInfo.fifo.empty()) {
        queueInfo.credit = 0.0;
    }
    Ptr<Packet> packet = GetPointer(q->GetPacket());
    int packet_size = packet->GetSize();

    // Update queue length, buffer size, and credit
    queue_length -= packet_size;
    rest_buffer_size += packet_size;
    queueInfo.credit -= packet_size;

    NS_LOG_INFO("Packet dequeued from flow ID: " << selectedFlowId << ", Packet size: " << packet_size);
    outfile << "Dequeue: Flow ID: " << selectedFlowId << ", Packet size: " << packet_size << ", Remaining Queue length: " << queue_length << std::endl;

    return q;
}

Ptr<const QueueDiscItem> ShaperVC::DoPeek(void) const {
    NS_LOG_INFO("Peek called, no implementation available.");
    return nullptr; // Not implemented
}

bool ShaperVC::CheckConfig(void) {
    NS_LOG_INFO("CheckConfig called, returning true.");
    return true;
}

int ShaperVC::GetFlowLabel(Ptr<QueueDiscItem> item) {
    Ptr<const Ipv4QueueDiscItem> ipItem =
        DynamicCast<const Ipv4QueueDiscItem>(item);

    const Ipv4Header ipHeader = ipItem->GetHeader();
    TcpHeader header;
    item->GetPacket()->PeekHeader(header);

    uint16_t sourcePort = header.GetSourcePort();
    uint16_t destPort = header.GetDestinationPort();

    return sourcePort + destPort; // Simplified flow ID generation
}

void ShaperVC::UpdateCredit(double timeInterval) {
    for (auto &entry : flowQueues) {
        entry.second.credit += timeInterval * SHAPER_RATE;
    }
}

void ShaperVC::UpdateFinishTime(int flowId, int packetSize) {
    QueueInfo &queueInfo = flowQueues[flowId];
    queueInfo.finishTime = std::max(currentTime, queueInfo.finishTime); // +static_cast<double>(packetSize) / queueInfo.weight;
    outfile << "FinishTimeUpdate: Flow ID: " << flowId << ", Finish Time: " << queueInfo.finishTime << std::endl;
}

} // namespace ns3

#include "myFIFO.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(myFIFO);

const double myFIFO::ALPHA = 14;

myFIFO::myFIFO() {
    InitializeParams();
}

// IdealScheduler class implementation
TypeId myFIFO::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::myFIFO")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<myFIFO>();
    return tid;
}

void myFIFO::InitializeParams(void) {
    buffer_size = BUFFER_SIZE;
    alpha = ALPHA;
    rest_buffer_size = BUFFER_SIZE;
    queue_length = 0;
    outfile.open("myFIFOTrace.txt");
    if (!outfile.is_open()) {
        std::cerr << "Fail to open file" << std::endl; 
    }
}

bool myFIFO::DoEnqueue(Ptr<QueueDiscItem> item) {
    Ptr<Packet> packet = GetPointer(item->GetPacket());

    int packet_size = packet->GetSize();

    if (queue_length <= alpha * rest_buffer_size) {
        fifo.push(item);
        queue_length += packet_size;
        rest_buffer_size -= packet_size;
        return true;
    } else {
        Drop(item);
        return false;
    }
}

Ptr<QueueDiscItem> myFIFO::DoDequeue() {
    if (!fifo.empty()) {
        Ptr<QueueDiscItem> q = fifo.front();
        fifo.pop();
        Ptr<Packet> packet = GetPointer(q->GetPacket());
        int packet_size = packet->GetSize();
        queue_length -= packet_size;
        rest_buffer_size += packet_size;
        int flow_id = GetFlowLabel(q);
        outfile << flow_id << " " << packet_size << " " <<Simulator::Now().GetNanoSeconds()<< std::endl;
        return q;
    }
    return nullptr;
}

Ptr<const QueueDiscItem> myFIFO::DoPeek(void) const {
    return 0;
}

bool myFIFO::CheckConfig(void) {
    return 1;
}

int myFIFO::GetFlowLabel(Ptr<QueueDiscItem> item) {
    Ptr<const Ipv4QueueDiscItem> ipItem =
        DynamicCast<const Ipv4QueueDiscItem>(item);

    const Ipv4Header ipHeader = ipItem->GetHeader();
    TcpHeader header;
    item->GetPacket()->PeekHeader(header);

    uint16_t sourcePort = header.GetSourcePort();
    uint16_t destPort   = header.GetDestinationPort();


    int flowId = 65535; 
    // std::cout<<sourcePort<<std::endl;
    // std::cout<<destPort<<std::endl<<std::endl;

    if (sourcePort < 30000) {
        flowId = sourcePort;
    } else if (destPort < 30000) {
        flowId = destPort;
    } else {
        cout << sourcePort << ", " << destPort << endl;
        uint32_t packetSize = item->GetPacket()->GetSize();
        std::cout << "Packet size: " << packetSize << " bytes" << std::endl;
    }

    if (flowId == 65535) {
        flowId = 1001;
        //cout<<"unidefined flow id, set to 1001"<<endl;
    }

    return flowId;
}
} // namespace ns3

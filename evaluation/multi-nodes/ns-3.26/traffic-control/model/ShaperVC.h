#ifndef SHAPER_VC_H
#define SHAPER_VC_H

#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <fstream>
#include <string>
#include <algorithm>
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/tcp-header.h"
#include "ns3/Tenant-tag.h"
#include "ns3/simulator.h"

namespace ns3 {

class ShaperVC : public QueueDisc {
public:
    static const int BUFFER_SIZE = 104857600;  // Total buffer size in bytes
    static const double ALPHA;                // Dynamic adjustment factor
    static const double SHAPER_RATE;          // Credit update rate

    static TypeId GetTypeId(void);  // NS3 type identification
    ShaperVC();                     // Constructor
    void InitializeParams(void);    // Initialize shaper parameters
    bool DoEnqueue(Ptr<QueueDiscItem> item);  // Enqueue a packet
    Ptr<QueueDiscItem> DoDequeue();           // Dequeue a packet
    Ptr<const QueueDiscItem> DoPeek(void) const; // Peek at the next packet
    bool CheckConfig(void);                   // Verify queue configuration
 
private:
    // Per-flow queue information
    struct QueueInfo {
        std::queue<Ptr<QueueDiscItem>> fifo;  // FIFO queue for the flow
        double credit;                        // Credit value for the flow
        double finishTime;                    // Finish time for WFQ scheduling
        int weight;                           // Weight for WFQ scheduling
    };

    std::unordered_map<int, QueueInfo> flowQueues; // Per-flow queues
    int buffer_size;              // Total buffer size
    double alpha;                 // Adjustment factor for dynamic control
    int rest_buffer_size;         // Remaining buffer size
    int queue_length;             // Current total queue length
    std::ofstream outfile;        // Log file for debugging

    double currentTime;           // Current simulation time

    int GetFlowLabel(Ptr<QueueDiscItem> item);        // Get flow ID for a packet
    void UpdateCredit(double timeInterval);           // Update credit for all flows
    void UpdateFinishTime(int flowId, int packetSize); // Update finish time for a flow
};

} // namespace ns3

#endif // SHAPER_VC_H

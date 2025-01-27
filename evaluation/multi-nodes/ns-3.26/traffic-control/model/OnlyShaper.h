#ifndef ONLY_SHAPER_H
#define ONLY_SHAPER_H

#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <regex>
#include <map>
#include <memory>
#include <unordered_set>
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/tcp-header.h"
#include "ns3/Tenant-tag.h"
#include "MyPipeline.h"
#include "MyScheduling.h"
#include "FlowScheduler.h"

namespace ns3 {

class OnlyShaper : public QueueDisc {
public:
    static const int BUFFER_SIZE = 104857600;  // Total buffer size in bytes
    static const double ALPHA;                // Dynamic adjustment factor
    static const double SHAPER_RATE;          // Credit update rate

    static TypeId GetTypeId(void);
    OnlyShaper();
    void InitializeParams(void);
    bool DoEnqueue(Ptr<QueueDiscItem> item);
    Ptr<QueueDiscItem> DoDequeue();
    Ptr<const QueueDiscItem> DoPeek(void) const;
    bool CheckConfig(void);
 
private:
    struct QueueInfo {
        std::queue<Ptr<QueueDiscItem>> fifo;  // FIFO queue for the flow
        double credit;                        // Credit value for the flow
    };

    std::unordered_map<int, QueueInfo> flowQueues; // Per-flow queues
    int buffer_size;
    double alpha;
    int rest_buffer_size;
    int queue_length;
    std::ofstream outfile;
    double lastUpdateTime;
    
    int GetFlowLabel(Ptr<QueueDiscItem> item);
    void UpdateCredit(double timeInterval);
};

} // namespace ns3

#endif // ONLY_SHAPER_H

#ifndef MY_FIFO_H
#define MY_FIFO_H

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

class myFIFO : public QueueDisc {
public:
    static const int BUFFER_SIZE = 104857600;
    static const double ALPHA;
    static const std::string SCHEDULING_ALGORITHM;

    static TypeId GetTypeId(void);
    myFIFO();
    void InitializeParams(void);
    bool DoEnqueue(Ptr<QueueDiscItem> item);
    Ptr<QueueDiscItem> DoDequeue();
    Ptr<const QueueDiscItem> DoPeek(void) const;
    bool CheckConfig(void);
 
private:
    std::queue<Ptr<QueueDiscItem>> fifo;
    int buffer_size;
    double alpha;
    int rest_buffer_size;
    int queue_length;
    std::ofstream outfile;
    int GetFlowLabel(Ptr<QueueDiscItem> item);
};

} // namespace ns3

#endif // MY_FIFO_H

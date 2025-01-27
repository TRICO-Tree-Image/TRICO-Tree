#ifndef CREDIT_ONLY_H
#define CREDIT_ONLY_H

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

class CreditOnly : public QueueDisc {
public:
    CreditOnly();
    static TypeId GetTypeId(void);
    void InitializeParams(void);
    bool DoEnqueue(Ptr<QueueDiscItem> item);
    Ptr<QueueDiscItem> DoDequeue();
    Ptr<const QueueDiscItem> DoPeek(void) const;
    bool CheckConfig(void);
    int GetFlowLabel(Ptr<QueueDiscItem> item);


private:
    const int BUFFER_SIZE = 2097152;
    const double ALPHA = 14;
    const std::string SCHEDULING_ALGORITHM = "exp1.txt";

    class Pkt {
    public:
        Ptr<QueueDiscItem> pktid;
        int flowid;
        int length;
        int flow_size;

        Pkt(Ptr<QueueDiscItem> pktid, int flowid, int length, int flow_size);
    };

    class FlowQueueItem {
    public:
        int flowid;
        Pkt pkt;
        std::vector<double> rank;
        int layercnt;

        FlowQueueItem(int flowid, Pkt pkt, int layercnt);
    };

    class PifoItem {
    public:
        double rank;
        Pkt pkt;
        int flowid;
        int pifoid;
        bool is_token;

        PifoItem();
        PifoItem(double rank, Pkt pkt, int flowid, int pifoid, bool is_token = false);
        bool operator<(const PifoItem& other) const;
    };

    class PIFO {
    public:
        int pifoid;
        std::priority_queue<PifoItem> pifo_queue;
        std::string schedule_algorithm;
        std::vector<std::tuple<int, bool, int>> children_node;
        std::unordered_map<int, double> finish_time;
        double virtual_time;
        double credit;

        PIFO();
        PIFO(int pifoid, std::string schedule_algorithm, std::vector<std::tuple<int, bool, int>> children_node);
        double rank_cal(Pkt pkt, int children_node_id);
        void after_pop(PifoItem item);
    };

    class PifoTreeScheduler {
    public:
        std::unordered_map<int, std::vector<int>> treepath;
        std::unordered_map<int, std::deque<FlowQueueItem>> queue_map;
        std::unordered_map<int, bool> flow_in_pifo;
        std::unordered_map<int, PIFO> pifo_map;
        int root;
        int buffer_size;
        double alpha;
        int rest_buffer_size;
        std::unordered_map<int, int> queue_length;

        std::deque<Pkt> packets_to_send;
        std::vector<int> stack;

        PifoTreeScheduler(int root);
        void initialize_params(const std::vector<std::tuple<int, std::string, std::vector<std::tuple<int, bool, int>>>>& pifo_config, const std::unordered_map<int, std::vector<int>>& treepaths, int root);
        void doenqueue(Pkt pkt);
        void push_to_pifo(int flowid);
        Pkt packets_to_send_empty();
        Pkt dodequeue();
        void DFS();
        std::tuple<int, PifoItem> push_and_pop(int currentIndex);
        void put_id_into_stack(int currentIndex);
    };

    PifoTreeScheduler scheduler;
    void ReadScheduleAlgo(const std::string& filename, PifoTreeScheduler& scheduler);
};

} // namespace ns3

#endif // CREDIT_ONLY_H

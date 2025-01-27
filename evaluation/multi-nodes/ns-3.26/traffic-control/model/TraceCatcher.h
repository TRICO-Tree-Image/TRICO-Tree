# ifndef TRACECATCHER_H
# define TRACECATCHER

# include <fstream>
# include <sstream>
# include <iostream>
# include <map>
# include <memory>
# include "ns3/ipv4-queue-disc-item.h"
# include "ns3/node.h"
# include "ns3/ptr.h"
# include "ns3/tcp-header.h"
# include "ns3/Tenant-tag.h"
# include "MyPipeline.h"
# include "MyScheduling.h"
# include "FlowScheduler.h"
#include <unordered_set>

namespace ns3 {
    class TraceCatcher : public QueueDisc{
        public:
            TraceCatcher();
            static TypeId GetTypeId(void);
            void InitializeParams(void);
            bool DoEnqueue(Ptr<QueueDiscItem> item);        
            Ptr<QueueDiscItem> DoDequeue(void);
            Ptr<const QueueDiscItem> DoPeek(void) const;
            bool CheckConfig(void);
            static const int MAX_QueueLength = 100000000;
            int tmp_max_length = 0;
            int pkt_count = 0;
            int flow_count = 0;
        
        private:
            std::queue<Ptr<QueueDiscItem>> FIFO_queue;
            int queue_length = 0;
            std::string GetFlowLabel(Ptr<QueueDiscItem> item); 
            std::unordered_set<string> m_activeFlows;
            std::unordered_map<string, int> m_flowPacketCount;
            std::unordered_set<string> m_seenFlows;
            std::unordered_map<string, int> flow_id;
            std::ofstream outfile;
    };
}

# endif
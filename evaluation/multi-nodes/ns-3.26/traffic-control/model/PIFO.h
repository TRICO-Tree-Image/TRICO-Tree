# ifndef PIFO_H
# define PIFO_H

# include <fstream>
# include <sstream>
# include <iostream>
# include <map>
# include <memory>
# include <string>
# include <queue>
# include "ns3/ipv4-queue-disc-item.h"
# include "ns3/node.h"
# include "ns3/ptr.h"
# include "ns3/tcp-header.h"
# include "ns3/Tenant-tag.h"
# include "MyScheduling.h"
// # include "FlowScheduler.h"
using namespace std;

namespace ns3 {
    class PIFO : public QueueDisc{
        public:
            struct per_item{
                int flow_id;
                long long rank;
                Ptr<QueueDiscItem> item;
            };
            typedef struct per_item per_item;

            PIFO();
            
            static TypeId GetTypeId(void);
            void InitializeParams(void);
            bool DoEnqueue(Ptr<QueueDiscItem> item);        
            Ptr<QueueDiscItem> DoDequeue(void);
            Ptr<const QueueDiscItem> DoPeek(void) const;
            bool CheckConfig(void);
        
        private:
            // The size of a queue
            static const int SIZE;
            
            // FlowScheduler flow_scheduler;
            
            std::unordered_map<string, int> flow_map;
            std::unordered_map<int, std::shared_ptr<Scheduling>> sch_tree;
            std::unordered_map<int, std::queue<Ptr<QueueDiscItem>>> queue_map;
            int queue_cnt = 10;
            long long time = -1e16;
            int root_size;
            
            // int empty_queue = 0;

            std::string GetFlowLabel(Ptr<QueueDiscItem> item); 

            struct cmp{
                bool operator()(per_item &x, per_item &y){
                    return x.rank > y.rank;     //按照rank升序排序
                }
            };
            priority_queue<per_item, vector<per_item>, cmp> schedule_queue;
    };
}

# endif
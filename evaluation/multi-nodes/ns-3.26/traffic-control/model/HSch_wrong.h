# ifndef HSCHWRONG_H
# define HSCHWRONG_H

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

namespace ns3 {
    class HSch_wrong : public QueueDisc{
        public:
            HSch_wrong();
            static TypeId GetTypeId(void);
            void InitializeParams(void);
            bool DoEnqueue(Ptr<QueueDiscItem> item);        
            Ptr<QueueDiscItem> DoDequeue(void);
            Ptr<const QueueDiscItem> DoPeek(void) const;
            bool CheckConfig(void);
            static const int MAX_LAYER = 10;
            struct FlowQueueItem
            {
                int flowid;
                Ptr<QueueDiscItem> pkt;
                int rank[MAX_LAYER];
                int layercnt;
            };
            struct PifoItem
            {
                int rank;
                bool isToken;
                Ptr<QueueDiscItem> pkt;
                int flowid;
                int pifoid;
                bool operator<(const PifoItem& rhs) const{
                    return rank > rhs.rank;
                };
            };
        
        private:
            std::unordered_map<int, std::queue<FlowQueueItem>> queue_map;
            std::unordered_map<int, bool> flow_in_pifo;
            std::unordered_map<int, std::priority_queue<PifoItem>> pifo_map;

            std::unordered_map<int, int> finish_time;
            int virtual_time;

            // // The index of root
            // static const int ROOT;
            // The size of a queue
            static const int SIZE;

            std::unordered_map<string, int> flow_map;
            // std::unordered_map<int, std::shared_ptr<Scheduling>> sch_tree;
            // std::unordered_map<int, std::queue<Ptr<QueueDiscItem>>> queue_map;
            // std::unordered_map<int, int> pri_map;       //B->1, C->2
            // std::unordered_map<int, int> tenant_map;    //A->1, B、C->2
            // std::unordered_map<int, int> newflow_mp;
            // std::unordered_map<int, int> newtenant_mp;
            // std::queue<Ptr<QueueDiscItem>> bypath_queue;
            // int queue_cnt = 10;
            // int root_size;
            
            // // int empty_queue = 0;

            // Pipeline pipe;

            // FlowScheduler flow_scheduler1;
            // FlowScheduler flow_scheduler2;

            // struct cmp{
            //     bool operator()(FlowScheduler::per_flow_item &x, FlowScheduler::per_flow_item &y){
            //         return x.rank > y.rank;     //按照rank升序排序
            //     }
            // };
            // //pifo，使用优先级队列实现
            // priority_queue<FlowScheduler::per_flow_item, vector<FlowScheduler::per_flow_item>, cmp> pifo1;
            // priority_queue<FlowScheduler::per_flow_item, vector<FlowScheduler::per_flow_item>, cmp> pifo2;
            
            // bool IsHadoop(int x);

            std::string GetFlowLabel(Ptr<QueueDiscItem> item); 
            void PushIntoPIFO(FlowQueueItem item);
    };
}

# endif
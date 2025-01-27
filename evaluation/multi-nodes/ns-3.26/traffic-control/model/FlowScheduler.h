# ifndef FLOWSCHEDULER_H
# define FLOWSCHEDULER_H

# include <cstring>
# include <queue>
# include <random>
# include <unordered_map>

# include "ns3/simulator.h"
# include "ns3/ipv4-queue-disc-item.h"
# include "ns3/ptr.h"

using namespace std;

namespace ns3{
    class FlowScheduler{
        public:
            struct per_flow_item{
                int flow_id;
                long long rank;
                Ptr<QueueDiscItem> item;
            };
            typedef struct per_flow_item per_flow_item;

            FlowScheduler();
            FlowScheduler(int flow_num);
            //将flow_id对应的flow，排名rank的数据包加入到flow scheduler中
            void flow_queue_push(per_flow_item item);
            //per_flow_item flow_queue_schedule();
            per_flow_item flow_queue_schedule_forflow();
            per_flow_item flow_queue_schedule_forsingleflow(int flowid);
            void flow_queue_pop(int flowid);

            bool is_allflow_empty();
            bool is_forflow_empty();
            void show_length();

        private:
            //一共有FLOW_NUM个队列，每个队列对应一个流；每个队列的长度是QUEUE_LENGTH
            int FLOW_NUM;
            int cnt = 0;
            static const int QUEUE_LENGTH = 300;

            //flow_queue_map 将flow id映射到对应队列上
            unordered_map<int, int> flow_queue_map;

            //flow_queue是流的对应队列，存储rank，使用小根堆实现
            struct cmp{
                bool operator()(per_flow_item &x, per_flow_item &y){
                    return x.rank > y.rank;     //按照rank升序排序
                }
            };
            //使用优先级队列实现。例如三个流，那就是三个优先级队列。每个队列存储对应的流的数据包
            priority_queue<per_flow_item, vector<per_flow_item>, cmp> *flow_queue;
            int *queue_length;
    };
}

# endif
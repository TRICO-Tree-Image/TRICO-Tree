#include "FlowScheduler.h"

namespace ns3{
    FlowScheduler::FlowScheduler(int flow_num){
        FLOW_NUM = flow_num;
        flow_queue = new priority_queue<per_flow_item, vector<per_flow_item>, cmp>[FLOW_NUM];
    }

    FlowScheduler::FlowScheduler(){
    }
    // FlowScheduler::~FlowScheduler(){
    // }

    void FlowScheduler::flow_queue_push(per_flow_item item){
        int flow_id = item.flow_id;
        if(flow_queue_map.find(flow_id) == flow_queue_map.end()){
            flow_queue_map[flow_id] = cnt++;
        }
        //flow_id映射到对应的flow队列
        int queue_id = flow_queue_map[flow_id];
        if(flow_queue[queue_id].size() >= QUEUE_LENGTH){
            return;
        }
        flow_queue[queue_id].push(item);
    }

    // FlowScheduler::per_flow_item FlowScheduler::flow_queue_schedule(){
    //     int flag = 0;
    //     per_flow_item schedule_item;
    //     //int queue_note = -1;    //记录哪个队列的head element是rank最低的
    //     //从所有队列的head element选出rank最低的数据包调度
    //     for(int i = 0; i <= cnt; i++){
    //         if(flow_queue[i].empty())
    //             continue;
    //         per_flow_item tmp = flow_queue[i].top();
    //         if(flag == 0 || tmp.rank < schedule_item.rank){
    //             flag = 1;
    //             schedule_item = tmp;
    //             //queue_note = i;
    //         }
    //     }
    //     return schedule_item;
    // }

    //从flow B与flow C中选出rank最小的数据包，对应perpifo node调度
    FlowScheduler::per_flow_item FlowScheduler::flow_queue_schedule_forflow(){
        int flag = 0;
        per_flow_item schedule_item;
        per_flow_item tmp_item = {-1, -1, nullptr};
        for(int i = 0; i <= cnt; i++){
            if(flow_queue[i].empty())
                continue;
            per_flow_item tmp = flow_queue[i].top();
            //forflow的特殊之处
            if((flag == 0 || tmp.rank < schedule_item.rank) && (tmp.flow_id == 12 || tmp.flow_id == 13)){
                flag = 1;
                schedule_item = tmp;
            }
        }
        if(!flag)
            return tmp_item;
        return schedule_item;
    }

    //从flowid流的队列中选出rank最小的数据包，对应正常的调度
    FlowScheduler::per_flow_item FlowScheduler::flow_queue_schedule_forsingleflow(int flowid){
        per_flow_item schedule_item = {-1, -1, nullptr};
        for(int i = 0; i <= cnt; i++){
            if(flow_queue[i].empty())
                continue;
            per_flow_item tmp = flow_queue[i].top();
            //forflow的特殊之处
            if(tmp.flow_id == flowid){
                return tmp;
            }
        }   
        return schedule_item;
    }

    void FlowScheduler::flow_queue_pop(int flowid){
        int queue_id = flow_queue_map[flowid];
        if(!flow_queue[queue_id].empty()){
            flow_queue[queue_id].pop();
        }
    }

    bool FlowScheduler::is_allflow_empty(){
        for(int i = 0; i <= cnt; i++){
            if(!flow_queue[i].empty())
                return false;
        }
        return true;
    }

    bool FlowScheduler::is_forflow_empty(){
        for(int i = 0; i <= cnt; i++){
            if(!flow_queue[i].empty() && flow_queue[i].top().flow_id != 11)
                return false;
        }
        return true;
    } 

    void FlowScheduler::show_length(){
        // cout << "come " << "cnt: " << cnt << endl;
        for(int i = 0; i <= cnt; i++){
            cout << i << " : " << flow_queue[i].size() << "  ";
        }
        cout << endl;
    }

}
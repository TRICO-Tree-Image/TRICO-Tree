#include "vPIFO.h"

namespace ns3
{
    class Node;
    const int vPIFO::ROOT = 5;
    //const int vPIFO::SIZE = ((1 << 10) - 1) / 3;
    const int vPIFO::SIZE = 2600;
    
    NS_OBJECT_ENSURE_REGISTERED(vPIFO);
    TypeId vPIFO::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::vPIFO")
			.SetParent<QueueDisc>()
			.SetGroupName("TrafficControl")
			.AddConstructor<vPIFO>();
		return tid;
	}

    bool vPIFO::IsHadoop(int x)
    {
        return (x % 10) > (x / 10);
    }

    vPIFO::vPIFO()
    {
        flow_map.clear();
        sch_tree.clear();
        queue_map.clear();
        while(!bypath_queue.empty())
            bypath_queue.pop();
        root_size = 0;
        
        // Build the scheduling tree

        // Root node: root to tenant group
        sch_tree[0] = std::make_shared<WFQ>(2);

        sch_tree[1] = std::make_shared<SP>(); 
    }

    std::string vPIFO::GetFlowLabel(Ptr<QueueDiscItem> item)
    {
        //printf("my log: flow label?\n");
        Ptr<const Ipv4QueueDiscItem> ipItem =
            DynamicCast<const Ipv4QueueDiscItem>(item);

        const Ipv4Header ipHeader = ipItem->GetHeader();
        TcpHeader header;
        GetPointer(item)->GetPacket()->PeekHeader(header);

        std::stringstream ss;
        ss << ipHeader.GetSource().Get();
        ss << header.GetSourcePort();
        //ss << ipHeader.GetDestination().Get();
        //ss << header.GetDestinationPort();
        
        /*cout << "my log: find tenant log" << ipHeader.GetSource().Get() << " "<<ipHeader.GetDestination().Get()
            << " "<< header.GetSourcePort() <<" "<< header.GetDestinationPort() << endl;*/

        std::string flowLabel = ss.str();
        //cout << "my log: flow label " << flowLabel << endl;
        return flowLabel;
    }
    
    void vPIFO::InitializeParams(void) {
        // flow_label
        // flow a 16783795349153
        // flow b 16783820949153 167838722771 167838722100
        // flow c 16783846549153

        //flow_map，flow_label映射为flow_id
        //a->11, b->12, c->13
        flow_map["16783795349153"] = 11;
        flow_map["16783820949153"] = 12;
        flow_map["167838722771"] = 12;
        flow_map["167838722100"] = 12;
        flow_map["16783846549153"] = 13;
        //queue_map，每个流对应一个队列存储数据包的指针，维护入队出队
        queue_map[11] = std::queue<Ptr<QueueDiscItem>>();
        queue_map[12] = std::queue<Ptr<QueueDiscItem>>();
        queue_map[13] = std::queue<Ptr<QueueDiscItem>>();
        //newflow_mp，流是否是第一次建立。若是，则需要进入pifo
        newflow_mp[11] = 0;
        newflow_mp[12] = 0;
        newflow_mp[13] = 0;
        //pri_map，维护flow B与flow C的优先级，在SP算法中利用
        //B->1, C->2
        pri_map[12] = 1;
        pri_map[13] = 2;
        //tenant_map，维护流对应的租户关系
        //A->1, BC->2
        tenant_map[11] = 1;
        tenant_map[12] = 2;
        tenant_map[13] = 2;
        newtenant_mp[1] = 0;
        newtenant_mp[2] = 0;
        //调度算法初始化
        sch_tree[0]->InitializeSize(11, 200);
        sch_tree[0]->InitializeSize(12, 200);
        sch_tree[0]->InitializeSize(13, 200);
        sch_tree[1]->InitializeSize(11, 200);
        sch_tree[1]->InitializeSize(12, 200);
        sch_tree[1]->InitializeSize(13, 200);
        //flow scheduler用于进行数据包存储，与维护per-flow调度
        //初始化flow scheduler
        flow_scheduler1 = FlowScheduler(5);
        flow_scheduler2 = FlowScheduler(5);
    }

    bool vPIFO::DoEnqueue(Ptr<QueueDiscItem> item)
    {
        // TenantTag my_tag;
        Ptr<Packet> packet = GetPointer(item->GetPacket());
        // packet->PeekPacketTag(my_tag);
        // int tenant = my_tag.GetTenantId();
        
        // // For TCP shake hands packets, bypath to output directly
        // if (tenant == 10233) {
        //     bypath_queue.push(item);
        //     return true;
        // }
        
        //printf("my log: DoEnqueue %d\n", tenant);

        string flow_label = GetFlowLabel(item);
        int flow_id = flow_map[flow_label];         //流映射成flow id
        int tenant_id = tenant_map[flow_id];        //流映射成租户
        int pri_token = pri_map[flow_id];           //流映射优先级
        
        // The root queue is full
        if (root_size >= SIZE) {        //超过SIZE，drop
            //cout << "drop" << endl;
            Drop(item);
            return false;
        }
        root_size++;

        //WFQ
        int packet_size = packet->GetSize();
        long long rk1 = sch_tree[0]->RankComputation(tenant_id, packet_size);
        long long rk2 = 0;
        FlowScheduler::per_flow_item pf_item = {flow_id, rk1, item};
        FlowScheduler::per_flow_item pf_item1 = {-1, -1, item};
        flow_scheduler1.flow_queue_push(pf_item);   //进入flow_scheduler存储
        if(flow_id == 12 || flow_id == 13){         //如果是flow B或flow C，需要进入SP算法调度
            rk2 = sch_tree[1]->RankComputation(pri_token, 0);
            pf_item1 = {flow_id, rk2, item};
            flow_scheduler2.flow_queue_push(pf_item1);
        }
        if(!newflow_mp[flow_id]){       //新流建立，加入pifo。其余的都先只加入队列，不进入pifo
            newflow_mp[flow_id] = 1;
            if(flow_id == 11){
                pifo1.push(pf_item);                //进入pifo
                queue_map[flow_id].push(item);      //queue_map存储数据包指针
            }
            else{   //SP
                pf_item.flow_id = 1;    //标记为reference
                pifo1.push(pf_item);    //reference进入pifo1

                pifo2.push(pf_item1);           //真正的数据包进入pifo2
                queue_map[flow_id].push(item);
            } 
        }   
        
        return true;
    }

    Ptr<QueueDiscItem> vPIFO::DoDequeue()
    {
        // if (!bypath_queue.empty()) {
        //     Ptr<QueueDiscItem> item = bypath_queue.front();
        //     bypath_queue.pop();
        //     return item;
        // }
        
        //流程：pifo出队，调度算法出包，per-flow队列出队，queue_map出队，
        //      pifo入队（正确与错误不同之处），queue_map入队
        if(pifo1.empty())
            return nullptr;
        FlowScheduler::per_flow_item root_item = pifo1.top();   //顶部pifo出包
        pifo1.pop();
        root_size--;
        Ptr<QueueDiscItem> item;

        int flowid = root_item.flow_id;
        if(flowid == 1){    //flowid为1，代表是reference
            //wfq出包
            Ptr<QueueDiscItem> ritem = root_item.item;
            Ptr<Packet> rpacket = GetPointer(ritem->GetPacket());
            int rpacket_size = rpacket->GetSize();
            sch_tree[0]->Dequeue(rpacket_size);                     //调度算法出包
            flow_scheduler1.flow_queue_pop(root_item.flow_id);      //per-flow队列出队
            //选包入pifo1
            //【正确版本与错误版本的区别在此】
            //正确版本中，flow B出包，是从flow B与flow C队列中挑选rank最小的包进入（flow_queue_schedule_forflow）
            //错误版本中，flow B出包，是从flow B队列中挑选rank最小的包进入（flow_queue_schedule_forsingleflow）
            //此为正确版本，从flow B与flow C队列中挑选rank最小的包进入
            FlowScheduler::per_flow_item schedule_item1 = flow_scheduler1.flow_queue_schedule_forflow();
            if(schedule_item1.flow_id != -1){   
                schedule_item1.flow_id = 1;     //标记为reference
                pifo1.push(schedule_item1);     //reference入pifo1
            }   

            //sp出包
            if(pifo2.empty())
                return nullptr;
            FlowScheduler::per_flow_item flow_item = pifo2.top();
            pifo2.pop();            //pifo出队
            int fid = flow_item.flow_id;
            item = queue_map[fid].front();
            queue_map[fid].pop();       //queue_map出队
            sch_tree[1]->Dequeue(fid);              //算法出包
            flow_scheduler2.flow_queue_pop(fid);    //per-flow队列出包
            //flow_scheduler2.show_length();
            //sp的pifo入包
            //此处不存在per-pifo node情况，哪个流的包出就哪个流的包进入
            FlowScheduler::per_flow_item schedule_item2 = flow_scheduler2.flow_queue_schedule_forsingleflow(fid);
            if(schedule_item2.flow_id != -1){
                pifo2.push(schedule_item2);         //数据包入pifo2
                queue_map[schedule_item2.flow_id].push(schedule_item2.item);
            } 
        }else{      //单层出包
            item = queue_map[flowid].front();
            queue_map[flowid].pop();       //queue_map出队
            Ptr<QueueDiscItem> ritem = root_item.item;
            Ptr<Packet> rpacket = GetPointer(ritem->GetPacket());
            int rpacket_size = rpacket->GetSize();
            sch_tree[0]->Dequeue(rpacket_size);                     //调度算法出包
            flow_scheduler1.flow_queue_pop(root_item.flow_id);      //per-flow队列出队
            //选包入pifo
            FlowScheduler::per_flow_item schedule_item1 = flow_scheduler1.flow_queue_schedule_forsingleflow(flowid);
            if(schedule_item1.flow_id != -1){   
                pifo1.push(schedule_item1);         //实际数据包入pifo1
                queue_map[schedule_item1.flow_id].push(schedule_item1.item);
            } 
        }
        
        return item;
    }
    
    Ptr<const QueueDiscItem> vPIFO::DoPeek(void) const {
        return 0;
    }
    
    bool vPIFO::CheckConfig(void) {
        return 1;
    }

}
#include "vPIFO_wrong_BMW.h"

//与vPIFO_BMW整体相同，只有一处代码逻辑不同

namespace ns3
{
    class Node;
    const int vPIFO_wrong_BMW::ROOT = 5;
    //const int vPIFO_wrong_BMW::SIZE = ((1 << 10) - 1) / 3;
    const int vPIFO_wrong_BMW::SIZE = 2600;
    
    NS_OBJECT_ENSURE_REGISTERED(vPIFO_wrong_BMW);
    TypeId vPIFO_wrong_BMW::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::vPIFO_wrong_BMW")
			.SetParent<QueueDisc>()
			.SetGroupName("TrafficControl")
			.AddConstructor<vPIFO_wrong_BMW>();
		return tid;
	}

    bool vPIFO_wrong_BMW::IsHadoop(int x)
    {
        return (x % 10) > (x / 10);
    }

    vPIFO_wrong_BMW::vPIFO_wrong_BMW()
    {
        flow_map.clear();
        sch_tree.clear();
        queue_map.clear();
        while(!bypath_queue.empty())
            bypath_queue.pop();
        root_size = 0;
        
        // Build the scheduling tree

        //WRQ
        sch_tree[0] = std::make_shared<WFQ>(2);
        //SP
        sch_tree[1] = std::make_shared<SP>(); 
    }

    std::string vPIFO_wrong_BMW::GetFlowLabel(Ptr<QueueDiscItem> item)
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

        std::string flowLabel = ss.str();
        return flowLabel;
    }
    
    void vPIFO_wrong_BMW::InitializeParams(void) {
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

    bool vPIFO_wrong_BMW::DoEnqueue(Ptr<QueueDiscItem> item)
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
        flow_scheduler1.flow_queue_push(pf_item);       //进入flow_scheduler存储
        if(flow_id == 12 || flow_id == 13){         //如果是flow B或flow C，需要进入SP算法调度
            rk2 = sch_tree[1]->RankComputation(pri_token, 0);
            FlowScheduler::per_flow_item pf_item1 = {flow_id, rk2, item};
            flow_scheduler2.flow_queue_push(pf_item1);
        }
        if(!newflow_mp[flow_id]){       //新流建立，加入BMW-Tree。其余的都先只加入队列，不进入BMW-Tree
            newflow_mp[flow_id] = 1;
            if(flow_id == 11){
                //pipe的三个参数，第一个表示节点序号，第二表示rank，第三个（如果是流就是flow_id，如果是reference就是reference的下级节点的序号）
                pipe.AddPush(ROOT, rk1, flow_id);       //加入BMW-Tree
                queue_map[flow_id].push(item);          
            }
            else{   //SP
                //reference与数据包本身都要加入BMW-Tree
                pipe.AddPush(ROOT, rk1, 1); //之所以token是1，是因为1代表了下一级节点的序号，方便pipeline代码中addpop
                pipe.AddPush(1, rk2, flow_id);
                queue_map[flow_id].push(item);
            } 
        }   
        
        return true;
    }

    Ptr<QueueDiscItem> vPIFO_wrong_BMW::DoDequeue()
    {
        // if (!bypath_queue.empty()) {
        //     Ptr<QueueDiscItem> item = bypath_queue.front();
        //     bypath_queue.pop();
        //     return item;
        // }
        
        int ans = pipe.GetToken();      //从pipeline中获取出队信息，若是reference，ans为-1；若是流的数据包，ans为flow_id

        // No packet in buffer now
        if (ans == -1) {            //ans是-1代表是reference出队
            //empty_queue++;
            // printf("? empty ans %d\n", empty_queue);
            return nullptr;
        }
        root_size--;
        Ptr<QueueDiscItem> item = queue_map[ans].front();      
        queue_map[ans].pop();
        

        //从上到下出包
        if(ans == 12 || ans == 13){ //flow bc
            //wfq出包
            //流程：先按照schedule算法出包，然后数据包出队，然后选择一个包进入BMW-Tree（两种方法的不同之处在实际出队的地方体现，即出去一个包后到底选什么包入BMW-Tree）
            FlowScheduler::per_flow_item root_item = flow_scheduler1.flow_queue_schedule_forflow();
            Ptr<QueueDiscItem> ritem = root_item.item;
            Ptr<Packet> rpacket = GetPointer(ritem->GetPacket());
            int rpacket_size = rpacket->GetSize();
            sch_tree[0]->Dequeue(rpacket_size);         //WFQ出包
            flow_scheduler1.flow_queue_pop(root_item.flow_id);      //出队
            //出包，则wfq的BMW-Tree进包（【与正确版本不同之处】：选择哪个数据包进入BMW-Tree）
            //调度，选哪个包出（正确版本与错误版本唯一不同的地方）
            //正确版本中，flow B出包，是从flow B与flow C队列中挑选rank最小的包进入（flow_queue_schedule_forflow）
            //错误版本中，flow B出包，是从flow B队列中挑选rank最小的包进入（flow_queue_schedule_forsingleflow）
            //此为错误版本，哪个流出数据包，就从哪个流中选取数据包进入BMW-Tree
            FlowScheduler::per_flow_item schedule_item1 = flow_scheduler1.flow_queue_schedule_forsingleflow(ans);
            if(schedule_item1.flow_id != -1){   
                pipe.AddPush(ROOT, schedule_item1.rank, 1);
            }

            //sp出包
            sch_tree[1]->Dequeue(ans);
            flow_scheduler2.flow_queue_pop(ans);    //出队
            //flow_scheduler2.show_length();
            //sp的BMW-Tree进包
            //此处不存在per-pifo node情况，哪个流的包出就哪个流的包进入
            FlowScheduler::per_flow_item schedule_item2 = flow_scheduler2.flow_queue_schedule_forsingleflow(ans);
            if(schedule_item2.flow_id != -1){
                pipe.AddPush(1, schedule_item2.rank, schedule_item2.flow_id);
                queue_map[schedule_item2.flow_id].push(schedule_item2.item);
            } 
        }else{      


            //flow a
            Ptr<Packet> packet = GetPointer(item->GetPacket());
            int packet_size = packet->GetSize();
            sch_tree[0]->Dequeue(packet_size);      //算法出队
            flow_scheduler1.flow_queue_pop(ans);    //出队
            //BMW-Tree进包
            FlowScheduler::per_flow_item schedule_item1 = flow_scheduler1.flow_queue_schedule_forsingleflow(ans);
            if(schedule_item1.flow_id != -1){
                pipe.AddPush(ROOT, schedule_item1.rank, schedule_item1.flow_id);
                queue_map[schedule_item1.flow_id].push(schedule_item1.item);
            }
        }
        
        return item;
    }
    
    Ptr<const QueueDiscItem> vPIFO_wrong_BMW::DoPeek(void) const {
        return 0;
    }
    
    bool vPIFO_wrong_BMW::CheckConfig(void) {
        return 1;
    }

}
#include "HSch_wrong.h"

namespace ns3
{
    // class Node;
    // const int vPIFO::ROOT = 5;
    // //const int vPIFO::SIZE = ((1 << 10) - 1) / 3;
    const int HSch_wrong::SIZE = 1000;

    NS_OBJECT_ENSURE_REGISTERED(HSch_wrong);

    TypeId HSch_wrong::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::HSch_wrong")
			.SetParent<QueueDisc>()
			.SetGroupName("TrafficControl")
			.AddConstructor<HSch_wrong>();
		return tid;
	}

    // bool vPIFO::IsHadoop(int x)
    // {
    //     return (x % 10) > (x / 10);
    // }

    HSch_wrong::HSch_wrong()
    {
        queue_map.clear();
        flow_in_pifo.clear();
        pifo_map.clear();
        flow_map.clear();
        // cout<<"HSch_wrong"<<endl;
        // while(!bypath_queue.empty())
        //     bypath_queue.pop();
        // root_size = 0;
        
        // Build the scheduling tree

        // Root node: root to tenant group
        // sch_tree[0] = std::make_shared<WFQ>(2);

        // sch_tree[1] = std::make_shared<SP>(); 
    }

    std::string HSch_wrong::GetFlowLabel(Ptr<QueueDiscItem> item)
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
    
    void HSch_wrong::InitializeParams(void) {
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
        queue_map[11] = std::queue<FlowQueueItem>();
        queue_map[12] = std::queue<FlowQueueItem>();
        queue_map[13] = std::queue<FlowQueueItem>();

        //flow_in_pifo，流是否是第一次建立。若是，则需要进入pifo
        flow_in_pifo[11] = false;
        flow_in_pifo[12] = false;
        flow_in_pifo[13] = false;

        pifo_map[0] = std::priority_queue<PifoItem>();
        pifo_map[1] = std::priority_queue<PifoItem>();

        finish_time[0] = 0;
        finish_time[1] = 0;

        virtual_time = 0;
    }

    void HSch_wrong::PushIntoPIFO(FlowQueueItem item)
    {
        PifoItem tmpPifoItem1;
        PifoItem tmpPifoItem2;
        if(item.flowid == 11) {
            tmpPifoItem1.rank = item.rank[0];
            tmpPifoItem1.isToken = false;
            tmpPifoItem1.pkt = item.pkt;
            tmpPifoItem1.flowid = item.flowid;

            pifo_map[0].push(tmpPifoItem1);
        }
        else{
            tmpPifoItem1.rank = item.rank[0];
            tmpPifoItem1.isToken = true;
            tmpPifoItem1.pifoid = 1;

            pifo_map[0].push(tmpPifoItem1);

            tmpPifoItem2.rank = item.rank[1];
            tmpPifoItem2.isToken = false;
            tmpPifoItem2.pkt = item.pkt;
            tmpPifoItem2.flowid = item.flowid;

            pifo_map[1].push(tmpPifoItem2);
        }
    }

    bool HSch_wrong::DoEnqueue(Ptr<QueueDiscItem> item)
    {
        // TenantTag my_tag;
        // cout<<"Pkt in"<<endl;
        Ptr<Packet> packet = GetPointer(item->GetPacket());
        int packet_size = packet->GetSize();
        // packet->PeekPacketTag(my_tag);
        // int tenant = my_tag.GetTenantId();
        
        // // For TCP shake hands packets, bypath to output directly
        // if (tenant == 10233) {
        //     bypath_queue.push(item);
        //     return true;
        // }
        
        //printf("my log: DoEnqueue %d\n", tenant);

        // flow classification
        string flow_label = GetFlowLabel(item);

        // If not registered flow or queue full, drop
        auto it = flow_map.find(flow_label);
        if (it == flow_map.end()) {
            Drop(item);
            //cout<<"Drop"<<endl;
            return false;
        }
        int flow_id = flow_map[flow_label];         //流映射成flow id
        // int tenant_id = tenant_map[flow_id];        //流映射成租户
        // int pri_token = pri_map[flow_id];           //流映射优先级
        if(queue_map[flow_id].size()>= SIZE) {
            Drop(item);
            //cout<<"Drop"<<endl;
            return false;
        }

        // create a new queueitem, cal Rank, push into queue_map
        FlowQueueItem tmpItem;
        tmpItem.flowid = flow_id;
        tmpItem.pkt = item;
        cout<<"In "<<flow_id<<endl;
        if(flow_id == 11) {
            tmpItem.layercnt = 1;
            finish_time[0] = std::max(virtual_time, finish_time[0]) + packet_size;
            tmpItem.rank[0] = finish_time[0];
        }
        else if(flow_id == 12) {
            tmpItem.layercnt = 2;
            finish_time[1] = std::max(virtual_time, finish_time[1]) + packet_size;
            tmpItem.rank[0] = finish_time[1];
            tmpItem.rank[1] = 1;
        }
        else if(flow_id == 13) {
            tmpItem.layercnt = 3;
            finish_time[1] = std::max(virtual_time, finish_time[1]) + packet_size;
            tmpItem.rank[0] = finish_time[1];
            tmpItem.rank[1] = 2;
        }
        else {
            Drop(item);
            // cout<<"Drop"<<endl;
            return false;
        }

        if(flow_in_pifo[flow_id]) {
            //cout<<"size:"<<queue_map[flow_id].size()<<endl;
            queue_map[flow_id].push(tmpItem);
            //cout<<queue_map[flow_id].size()<<endl;
            //cout<<"in queue"<<endl;
        }
        else{
            flow_in_pifo[flow_id] = true;
            PushIntoPIFO(tmpItem);
            //cout<<"in queue"<<endl;
        }
        
        return true;
    }

    Ptr<QueueDiscItem> HSch_wrong::DoDequeue()
    {
        Ptr<QueueDiscItem> ans = nullptr;
        if(pifo_map[0].empty())
        {
            //cout<<"empty"<<endl;
            //cout<<queue_map[0].size()<<" "<<queue_map[1].size()<<" "<<queue_map[2].size()<<";";
            return ans;
        }
        //cout<<"pkt out"<<endl;
        cout<<queue_map[11].size()<<" "<<queue_map[12].size()<<" "<<queue_map[13].size()<<endl;
        PifoItem tmpItem = pifo_map[0].top();
        pifo_map[0].pop();

        if(!tmpItem.isToken)
        {
            ans = tmpItem.pkt;
            if(queue_map[tmpItem.flowid].empty())
            {
                flow_in_pifo[tmpItem.flowid] = false;
            }
            else
            {
                PushIntoPIFO(queue_map[tmpItem.flowid].front());
                queue_map[tmpItem.flowid].pop();
            }
            cout<<"Out "<<tmpItem.flowid<<endl;
        }
        else{
            tmpItem = pifo_map[1].top();
            pifo_map[1].pop();
            ans = tmpItem.pkt;
            if(queue_map[tmpItem.flowid].empty())
            {
                flow_in_pifo[tmpItem.flowid] = false;
            }
            else
            {
                PushIntoPIFO(queue_map[tmpItem.flowid].front());
                queue_map[tmpItem.flowid].pop();
            }
            cout<<"Out "<<tmpItem.flowid<<endl;
        }
        return ans;
    }
    
    Ptr<const QueueDiscItem> HSch_wrong::DoPeek(void) const {
        return 0;
    }
    
    bool HSch_wrong::CheckConfig(void) {

        return 1;
    }

}
#include "TraceCatcher.h"

namespace ns3
{

    NS_OBJECT_ENSURE_REGISTERED(TraceCatcher);

    TypeId TraceCatcher::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::TraceCatcher")
			.SetParent<QueueDisc>()
			.SetGroupName("TrafficControl")
			.AddConstructor<TraceCatcher>();
		return tid;
	}

    TraceCatcher::TraceCatcher()
    {
    }
    
    void TraceCatcher::InitializeParams(void) {
        cout<<"TraceCatcher"<<endl;
        outfile.open("Trace.txt");
        if (!outfile.is_open()) {
            std::cerr << "无法打开文件进行写入" << std::endl;
        }
    }

    bool TraceCatcher::DoEnqueue(Ptr<QueueDiscItem> item)
    {
        Ptr<Packet> packet = GetPointer(item->GetPacket());
        TenantTag my_tag;
        packet->PeekPacketTag(my_tag);
        int flow_size = my_tag.GetTenantId();

        // for handshake
        if(flow_size == 10233) {
            flow_size = 1;
        }
        int packet_size = packet->GetSize();
        packet->Print(std::cout);
        // int packet_size = packet->GetSize();
        // cout<<"Pkt in Queue Length:"<<FIFO_queue.size()<<" Pkt Length:"<<packet->GetSize()<<endl;

        // flow classification
        string flow_label = GetFlowLabel(item);

        pkt_count++;

        if (m_seenFlows.find(flow_label) == m_seenFlows.end())
        {
            flow_count++;

            // cout<< "New flow detected: Flow ID = " << flow_label << " in " <<pkt_count<<endl;

            // 将新流添加到已见过的流集合
            m_seenFlows.insert(flow_label);
            flow_id[flow_label] = flow_count;
        }

        // cout<<"Pkt "<<pkt_count<<" belongs to "<<flow_id[flow_label]<<" Length "<<packet_size<<" pFabic "<<flow_size<<endl;
        outfile<<pkt_count<<" "<<flow_id[flow_label]<<" "<<packet_size<<" "<<flow_size<<endl;

        // m_flowPacketCount[flow_label]++;
        // m_activeFlows.insert(flow_label);

        tmp_max_length = std::max(tmp_max_length, static_cast<int>(FIFO_queue.size()));

        if(FIFO_queue.size()>= MAX_QueueLength) {
            Drop(item);
            //cout<<"Drop"<<endl;
            return false;
        }
        FIFO_queue.push(item);
        return true;
    }

    Ptr<QueueDiscItem> TraceCatcher::DoDequeue()
    {
        Ptr<QueueDiscItem> ans = nullptr;
        if(FIFO_queue.empty())
        {
            return ans;
        }
        ans = FIFO_queue.front();
        FIFO_queue.pop();
        // cout<<"Pkt out Queue Length:"<<FIFO_queue.size()<<" Pkt out Max Queue Length:"<<tmp_max_length<<endl;

        // string flow_label = GetFlowLabel(ans);
        //     // 更新流的数据包数量
        // if (--m_flowPacketCount[flow_label] == 0)
        // {
        //     m_flowPacketCount.erase(flow_label);
        //     m_activeFlows.erase(flow_label);
        // }
        // cout<<"Flow count in Sch: "<<m_activeFlows.size()<<endl;
        return ans;
    }
    
    Ptr<const QueueDiscItem> TraceCatcher::DoPeek(void) const {
        return 0;
    }
    
    bool TraceCatcher::CheckConfig(void) {

        return 1;
    }

    std::string TraceCatcher::GetFlowLabel(Ptr<QueueDiscItem> item)
    {
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

}
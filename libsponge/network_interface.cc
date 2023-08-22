#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // 在表中试着查询mac地址
    // 需要添加的内容
    // header: dst, src, type
    // buffer:?
    auto it=_ARP_table.find(next_hop_ip);
    // 找不到mac地址
    if(it==_ARP_table.end()){
        // 通过ARP机制，找到ip对应的mac

        // 发送ARP并追踪
        if(_outgoing_ARP.find(next_hop_ip)==_outgoing_ARP.end()){
            ARPMessage msg;
            msg.opcode=ARPMessage::OPCODE_REQUEST;
            msg.sender_ethernet_address=_ethernet_address;
            msg.sender_ip_address=_ip_address.ipv4_numeric();
            msg.target_ethernet_address={};
            msg.target_ip_address=next_hop_ip;

            EthernetFrame frame{};
            frame.header().dst=ETHERNET_BROADCAST;
            frame.header().src=_ethernet_address;
            frame.header().type=EthernetHeader::TYPE_ARP;
            frame.payload()=msg.serialize();
            
            _frames_out.push(frame);

            _outgoing_ARP[next_hop_ip]=_ARP_RESPONSE_TTL;
        }

        // 保留未发送的报文，等到接收到ARP之后再发送。
        _datagrams_to_send.push_back(make_pair(next_hop,dgram));
    }
    // 能直接找到mac地址
    else{
        EthernetFrame frame{};
        frame.header().dst=it->second.addr;
        frame.header().src=_ethernet_address;
        frame.header().type=EthernetHeader::TYPE_IPv4;
        frame.payload()=dgram.serialize();
        _frames_out.push(frame);
    }

    DUMMY_CODE(dgram, next_hop, next_hop_ip);
}

//! \param[in] frame the incoming Ethernet frame
// 接收到ARP广播的回复之后，将mac地址加入到ARP表中，清除待整理，发送暂存的报文
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst!=_ethernet_address&&frame.header().dst!=ETHERNET_BROADCAST){
        return nullopt;
    }

    // 收到的是IP包
    if(frame.header().type==EthernetHeader::TYPE_IPv4){
        InternetDatagram dgram;
        if(dgram.parse(frame.payload())!=ParseResult::NoError){
            return nullopt;
        }
        return dgram;
    }
    // 收到的是ARP包
    else if(frame.header().type==EthernetHeader::TYPE_ARP){
        ARPMessage msg;
        if(msg.parse(frame.payload())!=ParseResult::NoError){
            return nullopt;
        }

        bool is_request=
            msg.opcode==ARPMessage::OPCODE_REQUEST&&
            msg.target_ip_address==_ip_address.ipv4_numeric();
        bool is_response=
            msg.opcode==ARPMessage::OPCODE_REPLY&&
            msg.target_ethernet_address==_ethernet_address;
        // 若为ARP请求
        // 发送ARP回复
        if(is_request){
            ARPMessage reply;
            reply.opcode=ARPMessage::OPCODE_REPLY;
            reply.sender_ethernet_address=_ethernet_address;
            reply.sender_ip_address=_ip_address.ipv4_numeric();
            reply.target_ethernet_address=msg.sender_ethernet_address;
            reply.target_ip_address=msg.sender_ip_address;

            EthernetFrame frame_send;
            frame_send.header().dst=msg.sender_ethernet_address;
            frame_send.header().src=_ethernet_address;
            frame_send.header().type=EthernetHeader::TYPE_ARP;
            frame_send.payload()=reply.serialize();

            _frames_out.push(frame_send);
        }

        // 从ARP请求或响应中获取ip地址
        if(is_request||is_response){
            // 插入ARP表中
            _ARP_table[msg.sender_ip_address]={msg.sender_ethernet_address,_ARP_ENTRY_TTL};

            // 发送暂存字段
            for(auto it=_datagrams_to_send.begin();it!=_datagrams_to_send.end();){
                if(it->first.ipv4_numeric()==msg.sender_ip_address){
                    send_datagram(it->second,it->first);
                    it=_datagrams_to_send.erase(it);
                }
                else{
                    it++;
                }
            }

            // 清理待确认ARP
            _outgoing_ARP.erase(msg.sender_ip_address);
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 清除mac表中过期条目
    for(auto it=_ARP_table.begin();it!=_ARP_table.end();){
        if(it->second.ttl<=ms_since_last_tick){
            it=_ARP_table.erase(it);
        }
        else{
            it->second.ttl-=ms_since_last_tick;
            it++;
        }
    }

    // 清除待响应的ARP
    for(auto it=_outgoing_ARP.begin();it!=_outgoing_ARP.end();){
        if(it->second<=ms_since_last_tick){
            ARPMessage msg;
            msg.opcode=ARPMessage::OPCODE_REQUEST;
            msg.sender_ethernet_address=_ethernet_address;
            msg.sender_ip_address=_ip_address.ipv4_numeric();
            msg.target_ethernet_address={};
            msg.target_ip_address=it->first;

            EthernetFrame frame;
            frame.header().dst=ETHERNET_BROADCAST;
            frame.header().src=_ethernet_address;
            frame.header().type=EthernetHeader::TYPE_ARP;

            frame.payload()=msg.serialize();
            _frames_out.push(frame);

            it->second=_ARP_RESPONSE_TTL;
        }
        else{
            it->second-=ms_since_last_tick;
            it++;
        }
    }
}

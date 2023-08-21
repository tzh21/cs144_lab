#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if(!_active){
        return;
    }

    _time_since_last_segment_received=0;

    string sender_old_state=TCPState::state_summary(_sender);
    
    const bool seg_recv=_receiver.segment_received(seg);
    
    // if RST, shutdown 
    if(seg.header().rst){
        set_rst(false);
        return;
    }
    
    // 判断是否发送空段，即维护send_empty_ack
    bool send_empty=false;
    bool accept_ack=false;
    if(seg.header().ack){
        accept_ack=_sender.ack_received(seg.header().ackno,seg.header().win);
    }
    if(_sender.segments_out().empty()&&TCPState::state_summary(_receiver)!=TCPReceiverStateSummary::LISTEN){
        if(seg.length_in_sequence_space()>0){
            send_empty=true;
        }
        
        if(!seg_recv){
            send_empty=true;
        }
        
        if(seg.header().ack&&!accept_ack){
            if(_sender.next_seqno().raw_value()<seg.header().ackno.raw_value()){
                send_empty=true;
            }
        }
    }

    // receiver和sender的状态已更新（调用了segment_received和ack_received）。执行一些特判。
    if(TCPState::state_summary(_receiver)==TCPReceiverStateSummary::SYN_RECV&&
    TCPState::state_summary(_sender)==TCPSenderStateSummary::CLOSED){
        connect();
        return;
    }
    if(TCPState::state_summary(_receiver)==TCPReceiverStateSummary::FIN_RECV&&
    TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED){
        _linger_after_streams_finish = false;
    }
    if(TCPState::state_summary(_receiver)==TCPReceiverStateSummary::FIN_RECV&&
    TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED&&
    !_linger_after_streams_finish){
        _active = false;
        return;
    }

    if(send_empty){
        _sender.send_empty_segment();
    }

    // in most cases, ackno and win are needed in segments sent.
    update_ack_win();
}

bool TCPConnection::active() const { return _active; }

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);

    if(_sender.consecutive_retransmissions()>_cfg.MAX_RETX_ATTEMPTS){
        // if(!_sender.segments_out().empty()) _sender.segments_out().pop(); // why?
        set_rst(true);
        return;
    }
    
    // 更新所有发送字段的ack和win
    // 使用tick的时候，sender可能重新发送了一些片段，需要添加到connection中
    update_ack_win();
    // fill_window();

    _time_since_last_segment_received+=ms_since_last_tick;

    // 等待时间过长时关闭连接
    if(TCPState::state_summary(_receiver)==TCPReceiverStateSummary::FIN_RECV&&
    TCPState::state_summary(_sender)==TCPSenderStateSummary::FIN_ACKED&&
    _linger_after_streams_finish&&
    _time_since_last_segment_received>=10*_cfg.rt_timeout){
        _active=false;
        _linger_after_streams_finish=false;
    }
}

void TCPConnection::connect() {
    fill_window();
}

size_t TCPConnection::write(const string &data) {
    size_t write_size=_sender.stream_in().write(data);
    fill_window();
    return write_size;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    fill_window();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            set_rst(false); // why false
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::set_rst(bool send_rst){
    // 发送rst
    if (send_rst) {
        TCPSegment rst_seg;
        rst_seg.header().rst = true;
        _segments_out.push(rst_seg);
    }

    // 设置自身状态
    _active = false;
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _linger_after_streams_finish = false;
}

// 为sender的所有发送字段添加ack和win，然后发送。
void TCPConnection::update_ack_win(){
    while(!_sender.segments_out().empty()){
        TCPSegment segment=_sender.segments_out().front();
        _sender.segments_out().pop();
        if(_receiver.ackno().has_value()){
            segment.header().ack=true;
            segment.header().ackno=_receiver.ackno().value();
            segment.header().win=_receiver.window_size();
        }
        _segments_out.push(segment);
    }
}

void TCPConnection::fill_window(){
    _sender.fill_window();
    update_ack_win();
}
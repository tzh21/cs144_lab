#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timeout{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    size_t window_size=(_last_window_size)?(_last_window_size):(1);
    while(_bytes_in_flight<window_size){
        TCPSegment segment{};
        if(!_syn_set){
            segment.header().syn=true;
            _syn_set=true;
            send_segment(segment);
            return;
        }
        const size_t payload_size=min(TCPConfig::MAX_PAYLOAD_SIZE,window_size-_bytes_in_flight-segment.header().syn);
        segment.payload()=Buffer(_stream.read(payload_size));
        if(!_fin_set&&_stream.eof()&&segment.payload().size()+_bytes_in_flight<window_size){
            segment.header().fin=true;
            _fin_set=true;
        }
        if(segment.length_in_sequence_space()==0){break;}
        send_segment(segment);
        if(segment.header().fin){break;}
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_seqno_ack=unwrap(ackno,_isn,_next_seqno);
    if(_next_seqno<abs_seqno_ack){return false;}
    while(!_queue_flight.empty()){
        const TCPSegment& segment=_queue_flight.front();
        uint64_t abs_seqno_flight=unwrap(segment.header().seqno,_isn,_next_seqno);
        if(abs_seqno_flight+segment.length_in_sequence_space()<=abs_seqno_ack){
            _bytes_in_flight-=segment.length_in_sequence_space();
            _queue_flight.pop();
        }
        else {
            break;
        }
    }
    _timeout=_initial_retransmission_timeout;
    _consecutive_retransmissions_count=0;
    _timer=0;
    _timer_running=_queue_flight.empty()?false:true;
    _last_window_size=window_size;
    fill_window();
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if(_timer_running==false){return;}
    _timer+=ms_since_last_tick;
    if(!_queue_flight.empty()&&_timeout<=_timer){
        if(_last_window_size>0){
            _timeout*=2;
        }
        _consecutive_retransmissions_count++;
        _timer=0;
        _timer_running=true;
        const TCPSegment& segment=_queue_flight.front();
        _segments_out.push(segment);
    }
    else if(_queue_flight.empty()){_timer_running=false;}
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment segment{};
    segment.header().seqno=next_seqno();
    _segments_out.push(segment);
}

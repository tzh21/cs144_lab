#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    cerr<<"start"<<endl;
    // set SYN.
    const TCPHeader& header=seg.header();
    if(header.syn){
        if(_syn_on){return false;}
        _isn=header.seqno;
        _syn_on=true;
        // _ackno=1;
    }
    // deny segments before SYN set.
    if(!_syn_on){return false;}
    // if _syn_on:

    // push data into reassembler
    uint64_t abs_seqno=unwrap(header.seqno,_isn,_ackno);
    uint64_t stream_idx=abs_seqno-1+(header.syn==true);

    // check if segment is accepted.
    // special case: empty segment
    if(seg.length_in_sequence_space()==0){
        if(abs_seqno==_ackno){return true;}
        else{return false;}
    }
    // special case: window size 0
    if(window_size()<=0){
        if(abs_seqno<=_ackno&&_ackno<abs_seqno+seg.length_in_sequence_space()){return true;}
        else{return false;}
    }
    // segment falls outside window
    if(abs_seqno+seg.length_in_sequence_space()<=_ackno||_ackno+window_size()<=abs_seqno){
        return false;
    }
    // seg:abc, stream_idx:0, fin:0. all correct.
    _reassembler.push_substring(seg.payload().copy(),stream_idx,header.fin);
    _ackno=1+_reassembler.stream_out().bytes_written();
    if(_reassembler.stream_out().input_ended()){
        _ackno++;
    }
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(_syn_on){
        return wrap(_ackno,_isn);
    }
    return nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity-_reassembler.stream_out().buffer_size(); }

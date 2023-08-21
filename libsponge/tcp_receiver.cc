#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// bool TCPReceiver::segment_received(const TCPSegment& seg){
//     // 步骤：SYN设置（维护isn, syn_on）。核心是isn和syn_on，其他的逻辑是补充。
//     // 状态：未接收SYN
//     if(!_syn_on){
//         // SYN进行设置
//         if(seg.header().syn){
//             _syn_on=true;
//             _isn=seg.header().seqno;
//         }
//         // 非SYN立刻拒绝
//         else{
//             return false;
//         }
//     }
//     // 状态：已接收SYN
//     // 重复SYN立刻拒绝
//     else{
//         if(seg.header().syn){
//             return false;
//         }
//     }

//     // 步骤：维护fin
//     if(seg.header().fin){
//         if(!_fin_on){
//             _fin_on=true;
//         }
//         else{
//             return false;
//         }
//     }

//     // 步骤：压入重组器
//     const uint64_t abs_seqno_head=unwrap(seg.header().seqno,_isn,_abs_ackno);
//     uint64_t abs_seqno_end=abs_seqno_head+seg.length_in_sequence_space();
//     uint64_t abs_seqno_window=_abs_ackno+_reassembler.stream_out().remaining_capacity();
//     const uint64_t old_abs_ackno=_abs_ackno;
//     const size_t stream_idx=abs_seqno_head-1+(seg.header().syn);
//     _reassembler.push_substring(seg.payload().copy(),stream_idx,seg.header().fin);
//     // 准入：已接收SYN（这次刚接收也可以）
//     _abs_ackno=1+_reassembler.stream_out().bytes_written()+(_reassembler.stream_out().input_ended());

//     // 步骤：根据窗口判断是否接收
//     // 若窗口大小为0，则当作1处理
//     if(abs_seqno_window-old_abs_ackno<=0){
//         abs_seqno_window++;
//     }
//     // 若为段占据为空，则当作1处理
//     if(seg.length_in_sequence_space()<=0){
//         abs_seqno_end++;
//     }
//     // if(abs_seqno_end-abs_seqno_head<=0){
//     //     abs_seqno_end++;
//     // }
//     // 窗口外
//     if(abs_seqno_end<=old_abs_ackno||abs_seqno_window<=abs_seqno_head){
//         return false;
//     }
//     return true;
// }

bool TCPReceiver::segment_received(const TCPSegment &seg) {
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
    uint64_t abs_seqno=unwrap(header.seqno,_isn,_abs_ackno);
    uint64_t stream_idx=abs_seqno-1+(header.syn==true);

    // 判断是否接收
    // 0长度或0窗口时，覆盖最后一个字节可接收
    if(seg.length_in_sequence_space()==0){
        if(abs_seqno==_abs_ackno){
            return true;
            // receive=false;
        }
        return false;
    }
    if(window_size()<=0){
        if(abs_seqno<=_abs_ackno&&_abs_ackno<abs_seqno+seg.length_in_sequence_space()){
            return true;
            // receive=false;
        }
        return false;
    }
    // 窗口外不接收
    if(abs_seqno+seg.length_in_sequence_space()<=_abs_ackno||_abs_ackno+window_size()<=abs_seqno){
        return false;
        // receive=false;
    }

    // 维护重组器
    // 维护ackno
    _reassembler.push_substring(seg.payload().copy(),stream_idx,header.fin);
    _abs_ackno=1+_reassembler.stream_out().bytes_written()+(_reassembler.stream_out().input_ended());

    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(_syn_on){
        return wrap(_abs_ackno,_isn);
    }
    return nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity-_reassembler.stream_out().buffer_size(); }

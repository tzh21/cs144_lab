#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    _end_idx=numeric_limits<size_t>::max();
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
//! \param data the string being added
//! \param index the index of the first byte in `data`
//! \param eof whether or not this segment ends with the end of the stream
// data和index在实际意义上有联系，但并不内聚，如data为空时，data和index的联系没有定义。
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 空数据只可能起结束作用。
    if(data.size()==0){
        // 结束
        if(eof&&_needed_idx==index){
            _output.end_input();
        }
        return;
    }

    size_t idx_data_head=index;
    size_t idx_data_end=index+data.size();
    string insert_data=data;

    // 设置结束下标
    if(eof){
        _end_idx=idx_data_end;
    }

    // 若data完全为已组装数据
    if(idx_data_end<=_needed_idx){
        if(idx_data_end==_needed_idx&&!_output.input_ended()&&eof){
            _output.end_input();
        }
        return;
    }

    // 若data完全在窗口外
    if(_needed_idx+_capacity-_output.buffer_size()<=idx_data_head){
        return;
    }

    // 去除data与已组装数据的重叠
    if(idx_data_head<_needed_idx){
        insert_data=insert_data.substr(_needed_idx-idx_data_head);
        idx_data_head=_needed_idx;
    }

    // 去除data超出容量的部分
    insert_data=insert_data.substr(0,_capacity-_output.buffer_size());
    idx_data_end=idx_data_head+insert_data.size();

    // data被unassembled完全覆盖
    if(!unassembled_strings.empty()){
        auto it=unassembled_strings.upper_bound(idx_data_head);
        if(it!=unassembled_strings.begin()){
            it--;
            const size_t idx_prev_head=it->first;
            const size_t idx_prev_end=idx_prev_head+it->second.size();
            if(idx_data_end<=idx_prev_end){
                return;
            }
        }
    }

    // 扩展data头部
    if(!unassembled_strings.empty()){
        auto it=unassembled_strings.lower_bound(idx_data_head);
        if(it!=unassembled_strings.begin()){
            it--;
            const size_t idx_prev_head=it->first;
            const size_t idx_prev_end=idx_prev_head+it->second.size();
            // 扩展data头部
            if(idx_prev_head<idx_data_head&&idx_data_head<=idx_prev_end){
                insert_data=it->second.substr(0,idx_data_head-idx_prev_head)+insert_data;
                idx_data_head=idx_prev_head;
                // unassembled_strings.erase(idx_prev_head);
                unassembled_strings.erase(it);
            }
        }
    }

    // 扩展data尾部
    if(!unassembled_strings.empty()){
        auto it=unassembled_strings.upper_bound(idx_data_end);
        if(it!=unassembled_strings.begin()){
            it--;
            const size_t idx_succ_head=it->first;
            const size_t idx_succ_end=idx_succ_head+it->second.size();
            // 扩展尾部
            if(idx_succ_head<=idx_data_end&&idx_data_end<idx_succ_end){
                insert_data+=it->second.substr(idx_data_end-idx_succ_head);
                idx_data_end=idx_succ_end;
                // unassembled_strings.erase(idx_succ_head);
                unassembled_strings.erase(it);
            }
        }
    }

    // 去除unassembled中被data完全覆盖的部分
    while(!unassembled_strings.empty()){
        auto it=unassembled_strings.lower_bound(idx_data_head);
        if(it==unassembled_strings.end()){break;}
        const size_t idx_cover_head=it->first;
        const size_t idx_cover_end=idx_cover_head+it->second.size();
        if(idx_cover_end<=idx_data_end){
            unassembled_strings.erase(idx_cover_head);
        }
        else{
            break;
        }
    }

    // 插入data
    if(!insert_data.empty()){
        unassembled_strings[idx_data_head]=insert_data;
    }

    // 尝试组装
    auto it=unassembled_strings.lower_bound(_needed_idx);
    if(it!=unassembled_strings.end()){
        if(it->first==_needed_idx){
            // 满足组装条件，组装。
            // 同时判断是否结束输入。只有在成功组装时需要考虑。
            string assembling_string=it->second;
            bool reach_end=false;
            if(_end_idx<=it->first+assembling_string.size()){
                reach_end=true;
                assembling_string=assembling_string.substr(0,_end_idx-it->first);
            }
            write(assembling_string);
            unassembled_strings.erase(it->first);
            if(reach_end){
                _output.end_input();
            }
        }
    }
}

// 超过容量的data会被截断
void StreamReassembler::write(const string& data){
    _output.write(data);
    _needed_idx=_output.bytes_written();
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t ans=0;
    for(const auto& i:unassembled_strings){
        ans+=i.second.size();
    }
    return ans;
}

bool StreamReassembler::empty() const { return unassembled_strings.empty(); }

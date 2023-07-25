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
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // set end index and check if end input.
    if(eof){_end_idx=index+data.size();}
    if(_end_idx<=_needed_idx){_output.end_input();}
    // trivial speed-up
    if(_output.input_ended()){return;}
    if(data.size()==0){return;}
    if(_end_idx<=index){return;}
    if(index+data.size()<=_needed_idx){return;}
    string inserting_str=data;
    size_t idx=index; // 'idx' is the index of first char in inserting substring. So 'idx' may not be const.
    // cut `data` overlaped by bytestream (overlapped prefix of `data`)
    if(index<_needed_idx){
        idx=_needed_idx;
        inserting_str=inserting_str.substr(_needed_idx-index);
    }
    size_t push_size=min(inserting_str.size(),_capacity-_output.buffer_size());
    push_size=min(push_size,_end_idx-index);
    inserting_str=inserting_str.substr(0,push_size);
    cerr<<inserting_str.size()<<endl;
    if(unassembled_strings.empty()){
        insert_unassembled(idx,inserting_str);
        return;
    }
    auto it=unassembled_strings.lower_bound(idx); // `it` refers to first element with index less than `idx`
    // extend front and remove old substring from map
    if(it!=unassembled_strings.begin()){
        it--;
        const string& front_str=it->second;
        const size_t idx_end_front_str=it->first+it->second.size();
        if(idx<=idx_end_front_str){
            if(idx<=idx_end_front_str){
                inserting_str=front_str.substr(0,idx-it->first)+inserting_str;
                idx=it->first; // `idx` is the index of first char in new inserting string.
            }
            if(idx+inserting_str.size()<idx_end_front_str){
                inserting_str+=front_str.substr(inserting_str.size());
            }
            unassembled_strings.erase(it);
        }
        auto debug_it=unassembled_strings.find(1);
        if(debug_it!=unassembled_strings.end()){cerr<<debug_it->second[0]<<endl;}
    }
    if(unassembled_strings.empty()){
        insert_unassembled(idx,inserting_str);
        return;
    }
    size_t idx_end=idx+inserting_str.size();
    it=unassembled_strings.upper_bound(idx_end);
    if(it!=unassembled_strings.begin()){
        it--;
        if(idx_end<it->first+it->second.size()){
            string& back_str=it->second;
            // extend back and remove old substring from map
            inserting_str=inserting_str+back_str.substr(idx_end-it->first);
            idx_end=it->first+back_str.size();
            unassembled_strings.erase(it);
        }
    }
    insert_unassembled(idx,inserting_str);
}

// remove all the unassembled substrings that are totally covered by `str`
// insert `str`
// write unassembled[`index`] into bytestream if `index` is not greater than _needed_idx, and remove unassembled[`index`] if neccessary.
void StreamReassembler::insert_unassembled(const size_t index,const string& str){
    // remove all the unassembled substrings that are totally covered by `str`
    while(!unassembled_strings.empty()){
        auto it=unassembled_strings.lower_bound(index);
        if(it!=unassembled_strings.end()&&it->first+it->second.size()<=index+str.size()){
            unassembled_strings.erase(it);
        }
        else{break;}
    }
    // insert `str`
    unassembled_strings[index]=str;
    // write unassembled[`index`] into bytestream if `index` is not greater than _needed_idx, and remove unassembled[`index`] if neccessary.
    if(index<=_needed_idx){
        if(_needed_idx<index+str.size()){
            _output.write(str.substr(_needed_idx-index));
            _needed_idx+=index+str.size()-_needed_idx;
        }
        unassembled_strings.erase(index);
    }
    if(_end_idx<=_needed_idx){
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t ans=0;
    for(const auto& i:unassembled_strings){
        ans+=i.second.size();
    }
    return ans;
}

bool StreamReassembler::empty() const { return unassembled_strings.empty(); }

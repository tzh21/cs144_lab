#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):_capacity(capacity) {}

// 超出capacity的data会被截断输入
size_t ByteStream::write(const string &data) {
    if(_end){return 0;}
    size_t write_len=min(_capacity-_data.size(),data.size());
    for(size_t i=0;i<write_len;i++){
        _data.push_back(data[i]);
    }
    num_written+=write_len;
    return write_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t output_size=min(len,_data.size());
    string output(_data.begin(),_data.begin()+output_size);
    return output;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t out_len=min(_data.size(),len);
    for(size_t i=0;i<out_len;i++){
        _data.pop_front();
    }
    num_popped+=out_len;
}

void ByteStream::end_input() {_end=true;}

bool ByteStream::input_ended() const {return _end;}

size_t ByteStream::buffer_size() const {return _data.size();}

bool ByteStream::buffer_empty() const {return _data.empty();}

bool ByteStream::eof() const { return _data.size()==0&&_end==true; }

size_t ByteStream::bytes_written() const { return num_written; }

size_t ByteStream::bytes_read() const { return num_popped; }

size_t ByteStream::remaining_capacity() const { return _capacity-_data.size(); }

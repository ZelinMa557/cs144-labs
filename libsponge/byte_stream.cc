#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity): _capacity(capacity), _stream(_capacity, 0) { 
}

size_t ByteStream::write(const string &data) {
    int n = min(data.length(), remaining_capacity());
    for(int i = 0; i < n; i++) {
        _stream[_rear] = data[i];
        _rear = (_rear + 1) % _capacity;
    }
    _current_sz += n;
    _total_write += n;
    return n;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if((_front + len) <= _capacity)
        return _stream.substr(_front, len);
    size_t len1 = (_capacity - _front);
    size_t len2 = len - len1;
    return move(_stream.substr(_front, len1) + _stream.substr(0, len2));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    if (len > _current_sz)
        set_error();
    
    _front = (_front + len) % _capacity;
    _current_sz -= len;
    _total_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { _input_end = true; }

bool ByteStream::input_ended() const { return _input_end; }

size_t ByteStream::buffer_size() const { return _current_sz; }

bool ByteStream::buffer_empty() const { return _current_sz == 0; }

bool ByteStream::eof() const { return _total_read == _total_write && _input_end; }

size_t ByteStream::bytes_written() const { return _total_write; }

size_t ByteStream::bytes_read() const { return _total_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _current_sz; }

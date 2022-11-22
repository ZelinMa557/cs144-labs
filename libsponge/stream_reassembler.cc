#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _buffer(capacity, 0), _buffer_valid(capacity, false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t n = data.length();
    if ((index + n) < first_unread() || index >= first_unacceptable())
        return;
    if (eof) {
        _last_byte_pos = index + n - 1;
        _eof_rec = true;
    }
    
    size_t start = max(index, first_unassembled()) - index;
    size_t end = min(first_unacceptable(), index + n) - index;
    
    for (size_t i = start; i < end; i++) {
        size_t _index = (i + index) % _capacity;
        _buffer[_index] = data[i];
        if (!_buffer_valid[_index]) {
            _unassembled_bytes++;
            _buffer_valid[_index] = true;
        }
    }
    
    if (_buffer_valid[first_unassembled()%_capacity]) {
        string new_str(unassembled_bytes(), 0);
        size_t len = 0;
        for (size_t i = first_unassembled()%_capacity; _buffer_valid[i]; i = ((i+1)%_capacity)) {
            new_str[len] = _buffer[i];
            _buffer_valid[i] = false;
            len++;   
        }
        _unassembled_bytes -= len;
        _output.write(new_str.substr(0, len));
    }

    if (_eof_rec && (first_unassembled() - 1 == _last_byte_pos))
        _output.end_input();
    
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return first_unassembled()==first_unread() && _unassembled_bytes == 0; }

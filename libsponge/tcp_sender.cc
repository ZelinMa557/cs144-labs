#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <vector>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _rto(retx_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (_send_eof)
        return;
    
    uint64_t payload_len = _window;
    if (_window >= (_next_seqno - _recv_ack))
        payload_len = _window - (_next_seqno - _recv_ack);
    //长度为0的包只能有1个
    if (payload_len == 0 && !_segments_outstanding.empty())
        return;
    if(payload_len == 0) payload_len++;
    bool fin = false;
    while (payload_len > 0 && !fin) {
        TCPSegment seg;
        if (_next_seqno == 0) {
            seg.header().syn = true;
            payload_len--;
        }
        seg.header().seqno = wrap(_next_seqno, _isn);
        uint64_t read_len = std::min(payload_len, TCPConfig::MAX_PAYLOAD_SIZE);
        auto payload = _stream.read(read_len);
        seg.payload() = Buffer(std::move(payload));
        payload_len -= seg.payload().size();
        if (_stream.eof() && payload_len > 0) {
            fin = true;
            seg.header().fin = true;
            payload_len--;
            _send_eof = true;
        }
        if (seg.length_in_sequence_space() > 0) {
            _bytes_in_flight += seg.length_in_sequence_space();
            _next_seqno += seg.length_in_sequence_space();
            _segments_out.push(seg);
            _segments_outstanding.push(seg);
        }
        else break;
        if (!_timer_start) {
            _timer_start = true;
            _timer = 0;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    auto absolute_ackno = unwrap(ackno, _isn, _recv_ack);
    if (absolute_ackno < _recv_ack || absolute_ackno > _next_seqno)
        return;
    if (absolute_ackno != _recv_ack) {
        _rto = _initial_retransmission_timeout;
        _retrans = 0;
        _timer = 0;
    }

    _recv_ack = absolute_ackno;
    _window = window_size;
    

    while (!_segments_outstanding.empty()) {
        auto front = _segments_outstanding.front();
        if (unwrap(front.header().seqno, _isn, _next_seqno) + front.length_in_sequence_space() <= _recv_ack) {
            _bytes_in_flight -= front.length_in_sequence_space();
            _segments_outstanding.pop();
        }
        else break;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if (!_timer_start)
        return;
    _timer += ms_since_last_tick;
    if (_timer >= _rto && !_segments_outstanding.empty()) {
        auto first_retrans = _segments_outstanding.front();
        _segments_out.push(first_retrans);
        _retrans++;
        if(_window > 0)
            _rto *= 2;
        _timer = 0;
    } 
    if (_segments_outstanding.empty())
        _timer_start = false;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retrans; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    if(_next_seqno == 0) {
        seg.header().syn = true;
        _next_seqno += 1;
    }
    if(_stream.eof() && !_send_eof) {
        seg.header().fin = true;
        _send_eof = true;
        _next_seqno += 1;
    }
    _segments_out.push(seg);
    if(seg.header().syn || seg.header().fin)
        _segments_outstanding.push(seg);
}

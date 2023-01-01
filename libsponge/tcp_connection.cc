#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _since_last_tick; }

void TCPConnection::unclean_shutdown() {
    // 清空segments out, 因为不需要再发送除rst以外的段了
    while(!_segments_out.empty())
        _segments_out.pop();
    while(!_sender.segments_out().empty())
        _sender.segments_out().pop();
            
    // 发送带有rst的段
    _sender.send_empty_segment();
    auto seg = _sender.segments_out().front();
    seg.header().rst = true;
    if (_receiver.ackno().has_value()) {
        seg.header().ackno = _receiver.ackno().value();
        seg.header().ack = true;
    }
    _segments_out.push(seg);
    _sender.segments_out().pop();

    // 设置错误状态
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::clean_shutdown() {
    if(_receiver.stream_out().input_ended()) {
        if(!_sender.stream_in().input_ended())
            _linger_after_streams_finish = false;
        else if(_sender.bytes_in_flight() == 0) {
            if(!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
                _active = false;
            }
        }
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    cerr << "get a seg" << endl;
    if (!_active)
        return;
    cerr << "checking header" << endl;
    if (seg.header().rst == true) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }
    cerr << "recv" << endl;
    _receiver.segment_received(seg);
    _since_last_tick = 0;

    bool has_send_seg = false;
    if(!_connected) {
        if(seg.header().syn) {
            connect();
            has_send_seg = true;
        }
        else return;
    }

    if (seg.header().ack == true) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
    }
    
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0)
            && seg.header().seqno == _receiver.ackno().value() - 1) {
        if(!has_send_seg)
            _sender.send_empty_segment();
    }

    if (seg.length_in_sequence_space() > 0 && !has_send_seg)
        send_seg(1);
    else send_seg(0);
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if(!data.length())
        return 0;
    size_t sz = _sender.stream_in().write(data);
    _sender.fill_window();
    return sz;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _sender.tick(ms_since_last_tick); 
    _since_last_tick += ms_since_last_tick;
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
        unclean_shutdown();
    else if(_linger_after_streams_finish && _since_last_tick >= 10 * _cfg.rt_timeout)
        clean_shutdown();
    send_seg(0);
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_seg(1);
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_seg(1);
    _connected = true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_seg(int at_least_one) {
    auto winsz = _receiver.window_size();
    auto ack = _receiver.ackno();
    if (at_least_one && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    while (!_sender.segments_out().empty()) {
        auto seg = _sender.segments_out().front();
        seg.header().win = winsz;
        if (ack.has_value()) {
            seg.header().ackno = ack.value();
            seg.header().ack = true;
        }
        else {
            seg.header().ack = false;
        }
        _segments_out.push(seg);
        _sender.segments_out().pop();
    }
    clean_shutdown();
}
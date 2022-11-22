#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto head = seg.header();
    WrappingInt32 payload_first_seqno = head.seqno;
    if (head.syn) {
        _syn_recv = true;
        _isn = head.seqno;
        payload_first_seqno = head.seqno + 1;
    }
    if (!_syn_recv)
        return;
    if (head.fin) _fin_recv = true;
    string payload = seg.payload().copy();
    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t absolute_seqno_64 = unwrap(payload_first_seqno, _isn, checkpoint);
    if (absolute_seqno_64 == 0) {
        return;
    }
    _reassembler.push_substring(payload, absolute_seqno_64 - 1, head.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_syn_recv)
        return nullopt;
    uint64_t absolute_seqno = stream_out().bytes_written() + 1;
    if (stream_out().input_ended()) {
        return wrap(absolute_seqno + 1, _isn);
    }
    return wrap(absolute_seqno, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.first_unacceptable() - _reassembler.first_unassembled(); }

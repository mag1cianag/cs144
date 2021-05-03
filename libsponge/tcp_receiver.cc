#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto head = seg.header();
    if (!head.syn && !_syned) {
        return;
    }

    string data = seg.payload().copy();

    bool eof = false;

    if (head.syn && !_syned) {
        _syned = true;
        _isn = head.seqno;
        if (head.fin) {
            _fined = eof = true;
        }
        _reassembler.push_substring(data, 0, eof);
        return;
    }
    if (_syned && head.fin) {
        _fined = eof = true;
    }

    uint64_t checkpoint = _reassembler.ack_index();
    uint64_t absSeqno = unwrap(head.seqno, _isn, checkpoint);
    uint64_t streamIdx = absSeqno - _syned;

    _reassembler.push_substring(data, streamIdx, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syned)
        return nullopt;
    return wrap(_reassembler.ack_index() + 1 + (_reassembler.empty() && _fined), _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }

#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
using namespace std;

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _syned(false)
    , _fined(false)
    , _bytes_in_flight(0)
    , _receiver_wz(0)
    , _receiver_avail(0)
    , _consecutive_retransmissions(0)
    , _timer(_initial_retransmission_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (!_syned) {
        _syned = true;
        TCPSegment seg;
        seg.header().syn = true;
        _send_segment(seg);
        return;
    }
    if (!_outstanding.empty() && _outstanding.front().header().syn)
        return;
    if (!_stream.buffer_size() && !_stream.eof())
        return;
    if (_fined)
        return;
    if (_receiver_wz) {
        while (_receiver_avail) {
            TCPSegment seg;
            size_t read_len = min({_stream.buffer_size(),
                                   static_cast<size_t>(_receiver_avail),
                                   static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});
            seg.payload() = Buffer(_stream.read(read_len));
            if (_stream.eof() && static_cast<size_t>(_receiver_avail) > read_len) {
                seg.header().fin = true;
                _fined = true;
            }
            _send_segment(seg);
            if (_stream.buffer_empty())
                break;
        }
    } else if (_receiver_avail == 0) {
        // window_size is zero
        TCPSegment seg;
        if (_stream.eof()) {
            seg.header().fin = true;
            _fined = true;
        } else if (!_stream.buffer_empty()) {
            seg.payload() = Buffer(_stream.read(1));
        }
        _send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (!_check_ackno(abs_ackno))
        return;
    _receiver_avail = window_size;
    _receiver_wz = window_size;

    while (!_outstanding.empty()) {
        TCPSegment &seg = _outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= abs_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _outstanding.pop();
            _timer.resetElapsed();
            _timer.setRto(_initial_retransmission_timeout);
            _consecutive_retransmissions = 0;
        } else {
            break;
        }
    }

    if (!_outstanding.empty()) {
        _receiver_avail = static_cast<uint16_t>(static_cast<uint64_t>(window_size) -
                                                unwrap(_outstanding.front().header().seqno, _isn, _next_seqno) +
                                                abs_ackno - _bytes_in_flight);
    }

    if (!bytes_in_flight()) {
        _timer.shutdown();
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer.isStarted())
        return;
    _timer.timeElapse(ms_since_last_tick);
    if (_timer.isPassed()) {
        _segments_out.push(_outstanding.front());
        if (_receiver_wz || _outstanding.front().header().syn) {
            ++_consecutive_retransmissions;
            _timer.doubleRto();
        }
        _timer.resetElapsed();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::_send_segment(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    if (_syned) {
        _receiver_avail -= seg.length_in_sequence_space();
    }
    _segments_out.push(seg);
    _outstanding.push(seg);
    if (!_timer.isStarted()) {
        _timer.startTimer();
    }
}

bool TCPSender::_check_ackno(uint64_t abs_ackno) {
    if (_outstanding.empty()) {
        return abs_ackno <= _next_seqno;
    }
    return abs_ackno <= _next_seqno && abs_ackno >= unwrap(_outstanding.front().header().seqno, _isn, _next_seqno);
}
#include "tcp_connection.hh"

#include <iostream>

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_conter; }

bool TCPConnection::real_send() {
    bool is_send = false;
    while (!_sender.segments_out().empty()) {
        is_send = true;
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_and_windowsize(segment);
        _segments_out.push(segment);
    }
    return is_send;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_conter = 0;
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }
    _receiver.segment_received(seg);

    if (check_inbound_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        real_send();
    }
    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        bool is_send = real_send();
        if (!is_send) {
            _sender.send_empty_segment();
            TCPSegment ackSeg = _sender.segments_out().front();
            _sender.segments_out().pop();
            set_ack_and_windowsize(ackSeg);
            _segments_out.push(ackSeg);
        }
    }
    return;
}

bool TCPConnection::active() const { return _active; }

void TCPConnection::set_ack_and_windowsize(TCPSegment &seg) {
    optional<WrappingInt32> ackno = _receiver.ackno();
    if (ackno.has_value()) {
        seg.header().ack = true;
        seg.header().ackno = ackno.value();
    }
    size_t window_size = _receiver.window_size();
    seg.header().win = static_cast<uint16_t>(window_size);
    return;
}

size_t TCPConnection::write(const string &data) {
    if (!data.size())
        return 0;
    size_t rWrite = _sender.stream_in().write(data);
    _sender.fill_window();
    real_send();
    return rWrite;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_conter += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.segments_out().size() > 0) {
        auto retxSeg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_and_windowsize(retxSeg);
        if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            retxSeg.header().rst = true;
            _active = false;
        }
        _segments_out.push(retxSeg);
    }
    if (check_inbound_ended() && check_outbound_ended()) {
        if (!_linger_after_streams_finish) {
            _active = false;
        } else if (_time_conter >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    real_send();
}

void TCPConnection::send_RST() {
    _sender.send_empty_segment();
    auto rstSeg = _sender.segments_out().front();
    _sender.segments_out().pop();
    set_ack_and_windowsize(rstSeg);
    rstSeg.header().rst = true;
    _segments_out.push(rstSeg);
}

void TCPConnection::connect() {
    _sender.fill_window();
    real_send();
}

bool TCPConnection::check_inbound_ended() {
    return _receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended();
}

bool TCPConnection::check_outbound_ended() {
    return _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
           _sender.bytes_in_flight() == 0;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            send_RST();
            _active = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

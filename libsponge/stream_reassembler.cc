#include "stream_reassembler.hh"

#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _eof(false)
    , _unassStart(0)
    , _unassSize(0)
    , _buffer(_capacity, '\0')
    , _bitmap(_capacity, false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // exceed  upper bound
    if (index >= _capacity + _unassStart)
        return;

    if (eof) {
        _eof = true;
    }

    size_t len = data.length();
    if (len == 0 && _eof && _unassSize == 0) {
        _output.end_input();
        return;
    }
    if (index >= _unassStart) {
        size_t off = index - _unassStart;
        size_t readLen = min(len, _capacity - _output.buffer_size() - off);
        if (readLen < len)
            _eof = false;
        for (size_t i = 0; i < readLen; i++) {
            if (_bitmap[i + off])
                continue;
            _buffer[i + off] = data[i];
            _bitmap[i + off] = true;
            ++_unassSize;
        }
    } else if (len + index > _unassStart) {
        // overlap
        size_t off = _unassStart - index;
        size_t readLen = min(len - off, _capacity - _output.buffer_size());
        if (readLen < len - off)
            _eof = false;
        for (size_t i = 0; i < readLen; i++) {
            if (_bitmap[i])
                continue;
            _bitmap[i] = true;
            _buffer[i] = data[i + off];
            ++_unassSize;
        }
    }

    // write into _output
    outPut();
    if (_eof && _unassSize == 0) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassSize; }

bool StreamReassembler::empty() const { return _unassSize == 0; }

size_t StreamReassembler::ack_index() const { return _unassStart; }
void StreamReassembler::outPut() {
    string ans;
    // write accumulative
    while (_bitmap.front()) {
        ans += _buffer.front();
        _buffer.pop_front();
        _bitmap.pop_front();
        _buffer.push_back('\0');
        _bitmap.push_back(false);
    }
    if (!ans.empty()) {
        _output.write(ans);
        _unassStart += ans.length();
        _unassSize -= ans.length();
    }
}

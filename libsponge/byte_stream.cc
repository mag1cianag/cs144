#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity), _buffer(),  _read_bytes(0), _end(false), _written_bytes(0) {}

size_t ByteStream::write(const string &data) {
    auto canWrite = _capacity - _buffer.size();
    auto realWrite = min(canWrite, data.length());
    for (size_t i = 0; i < realWrite; i++) {
        _buffer.push_back(data[i]);
    }

    _written_bytes += realWrite;
    return realWrite;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ans;
    auto canOutput = _buffer.size();
    auto realOutput = min(canOutput, len);
    for (size_t i = 0; i < realOutput; i++) {
        ans += _buffer[i];
    }

    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len > _buffer.size()) {
        set_error();
        return;
    }

    for (size_t i = 0; i < len; i++) {
        _buffer.pop_front();
    }
    _read_bytes += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans;
    if (len > _buffer.size()) {
        set_error();
        return ans;
    }
    for (size_t i = 0; i < len; i++) {
        ans += _buffer.front();
        _buffer.pop_front();
    }

    return ans;
}

void ByteStream::end_input() { _end = true; }

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return _buffer.empty() && _end; }
size_t ByteStream::bytes_written() const { return _written_bytes; }

size_t ByteStream::bytes_read() const { return _read_bytes; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }

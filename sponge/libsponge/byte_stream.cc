#include "byte_stream.hh"
#include <algorithm>
#include <cstddef>
#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) { _data.resize(capacity); }

size_t ByteStream::write(const string &data) {
    if (remaining_capacity() == 0) {
        return 0;
    }
    size_t can_write = std::min(remaining_capacity(), data.size());
    size_t first_write_size = _capacity - _write_pos;
    if (can_write <= first_write_size) {
        std::copy(data.data(), data.data() + can_write, _data.data() + _write_pos);
        _write_pos += can_write;
    } else {
        std::copy(data.data(), data.data() + first_write_size, _data.data() + _write_pos);
        size_t remain = can_write - first_write_size;
        std::copy(data.data() + first_write_size, data.data() + can_write, _data.data());
        _write_pos = remain;
    }
    _bytes_written += can_write;
    _size += can_write;
    return can_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t can_read = std::min(len, buffer_size());
    if (_read_pos + can_read < _capacity) {
        std::string result(_data.data() + _read_pos, can_read);
        return result;
    }
    std::string result(_data.data() + _read_pos, _capacity - _read_pos);
    size_t remain = can_read - (_capacity - _read_pos);
    result += std::string(_data.data(), remain);
    return result;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    read(len); 
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t can_read = std::min(len, buffer_size());
    _bytes_read += can_read;
    _size -= can_read;
    if (_read_pos + can_read < _capacity) {
        std::string result(_data.data() + _read_pos, can_read);
        _read_pos += can_read;
        return result;
    }
    std::string result(_data.data() + _read_pos, _capacity - _read_pos);
    size_t remain = can_read - (_capacity - _read_pos);
    result += std::string(_data.data(), remain);
    _read_pos = remain;
    return result;
}

void ByteStream::end_input() {
    _end = true;
}

bool ByteStream::input_ended() const { 
    return _end;
}

size_t ByteStream::buffer_size() const { 
    return _size; 
}

bool ByteStream::buffer_empty() const { return _size == 0; }

bool ByteStream::eof() const { return _end && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }

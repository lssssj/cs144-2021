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
    size_t writable_size = std::min(remaining_capacity(), data.size());
    size_t first_write_size = std::min(writable_size, _capacity - _write_pos);
    std::copy(data.data(), data.data() + first_write_size, _data.data() + _write_pos);
    size_t remain = writable_size - first_write_size;
    if (remain > 0) {
        std::copy(data.data() + first_write_size, data.data() + writable_size, _data.data());
    }
    _write_pos = (_write_pos + writable_size) % _capacity;
    _bytes_written += writable_size;
    _size += writable_size;
    return writable_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t readable_size = std::min(len, buffer_size());
    return read_from_buffer(readable_size, _read_pos);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    read(len); 
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t readable_size = std::min(len, buffer_size());
    _bytes_read += readable_size;
    _size -= readable_size;
    std::string result = read_from_buffer(readable_size, _read_pos);
    _read_pos = (_read_pos + readable_size) % _capacity;
    return result;
}

std::string ByteStream::read_from_buffer(size_t len, size_t pos) const {
    std::string result;
    result.reserve(len); // 预留足够的空间，避免多次分配
    size_t first_read_size = std::min(len, _capacity - pos);
    result.append(_data.data() + pos, first_read_size);
    size_t remain = len - first_read_size;
    if (remain > 0) {
        result.append(_data.data(), remain);
    }
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

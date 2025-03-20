#include "stream_reassembler.hh"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <list>
#include <sstream>
#include <stdexcept>
#include <sys/queue.h>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    if (_output.buffer_size() + _unassembled_bytes > _capacity) {
        std::stringstream err_msg;
        err_msg << "Buffer has been full. ";
        err_msg << "Buffer size: " << _output.buffer_size();
        err_msg << ", unassembled_bytes: " << _unassembled_bytes;
        err_msg << ", capacity: " << _capacity;
        err_msg << ", input data size: " << data.size() << std::endl;
        throw std::runtime_error(err_msg.str());
    }
    
    if (eof) {
        _eof = eof;
    }
    if (data.empty()) {
        if (eof && _queue.empty()) {
            _output.end_input();
        }
        return;
    }
    
    if (index > _stream_start_idx) {
        insert_to_list(data, index);
        return;
    }

    if (index + data.size() <= _stream_start_idx) {
        // duplicate segment, do nothing
        return;
    }

    std::string segment_data = data.substr(_stream_start_idx - index);
    uint64_t end_index = index + data.size();
    if (!_queue.empty()) {
        while (!_queue.empty() && _queue.front()._end <= end_index) {
            _unassembled_bytes -= _queue.front()._data.size();
            _queue.pop_front();
        }
        if (!_queue.empty() && _queue.front()._start <= end_index) {
            StringPair pair = _queue.front();
            _unassembled_bytes -= pair._data.size();
            _queue.pop_front();
            segment_data += pair._data.substr(end_index - pair._start);
        }
    }

    size_t written_byte = _output.write(segment_data);
    _stream_start_idx += written_byte;
    if (written_byte == segment_data.size()) {
        if (_eof && _queue.empty()) {
            _output.end_input();
        }
        return;
    }
    
    // should not happen, why write failed?? buffer is full, and have to clear queue
    // std::stringstream err_msg;
    // err_msg << "Write to bytestram failed, written_byte: " << written_byte << ", ";
    // err_msg << "but total size is " << segment_data.size() << ", ";
    // err_msg << "unassembled_bytes is " << _unassembled_bytes << std::endl;
    // throw std::runtime_error(err_msg.str());
}
    
void StreamReassembler::insert_to_list(const std::string &data, const uint64_t index) {
    std::string segment_data = data;
    size_t start = index;
    size_t end = index + segment_data.size();
    auto it = _queue.begin();
    _unassembled_bytes += segment_data.size();

    while (true) {
        if (it == _queue.end()) {
            break;
        }
        // contain
        if (it->_start <= start && end <= it->_end) {
            _unassembled_bytes -= segment_data.size();
            return;
        }
        if (start <= it->_start && it->_end <= end) {
            _unassembled_bytes -= it->_data.size();
            it = _queue.erase(it);
            continue;
        }
        // overlap
        if (start <= it->_start && it->_start <= end) {
            segment_data = segment_data + it->_data.substr(end - it->_start);
            _unassembled_bytes -= (end - it->_start);
            it = _queue.erase(it);
            continue;
        } else if (it->_start <= start && start <= it->_end) {
            segment_data = it->_data + segment_data.substr(it->_end - start);
            _unassembled_bytes -= (it->_end - start);
            start = it->_start;
            it = _queue.erase(it);
            continue;
        }
        it++;
    }
    
    for (auto iter = _queue.begin(); iter != _queue.end(); iter++) {
        if (iter->_start > start + segment_data.size()) {
            _queue.insert(iter, {start, start + segment_data.size(), segment_data});
            return;
        }
    }
    _queue.insert(_queue.end(), {start, start + segment_data.size(), segment_data});
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0 && _output.buffer_empty(); }

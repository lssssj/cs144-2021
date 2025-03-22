#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <_types/_uint32_t.h>
#include <_types/_uint64_t.h>
#include <cassert>
#include <cstddef>
#include <optional>
#include <string>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        if (_state != TCPState::LISTEN) {
            return;
        }
        _state = TCPState::SYN_RECV;
        _isn = seg.header().seqno;
    }
    if (_state == TCPState::LISTEN) {
        return;
    }

    assert(_state == TCPState::SYN_RECV || _state == TCPState::FIN_RECV);

    int64_t absolute_index = unwrap(seg.header().seqno, _isn, _reassembler.accepted_index() + 1);
    std::string data = seg.payload().copy();
    if (!data.empty() && absolute_index - 1 < 0) {
        assert(absolute_index == 0);
        if (!seg.header().syn) {
            data = data.substr(1);
        }
        absolute_index += 1;
    }
    _reassembler.push_substring(data, absolute_index - 1, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_state == TCPState::LISTEN) {
        return std::nullopt;
    }
    uint64_t next_write = _reassembler.accepted_index();
    next_write = _reassembler.stream_out().input_ended() ? next_write + 1 : next_write;
    return wrap(next_write + 1, _isn);
}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.stream_out().buffer_size();
}

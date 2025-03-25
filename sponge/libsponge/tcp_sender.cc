#include "buffer.hh"
#include "tcp_config.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"
#include "tcp_sender.hh"
#include "wrapping_integers.hh"

#include <cassert>
#include <iostream>
#include <random>
#include <stdexcept>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{retx_timeout}
    , _retransmission_count{0}
    , _now_time{0}
    , _stream(capacity) {
        TCPHeader header;
        header.seqno = _isn;
        header.syn = true;
        TCPSegment segment;
        segment.header() = header;
        _next_seqno += 1;
        _segments_out.push(segment);
        _segments_waiting_ack.push_back(segment);
    }

uint64_t TCPSender::bytes_in_flight() const { 
    uint64_t bytes = 0;
    for (const auto& seg : _segments_waiting_ack) {
        bytes += seg.length_in_sequence_space();
    }
    return bytes;
}

void TCPSender::fill_window() {
    uint64_t remain_ws = _window_size ? _window_size : 1;
    if (remain_ws <= bytes_in_flight()) {
        return;
    }
    remain_ws -= bytes_in_flight();
    
    while (remain_ws > 0 && !_stream.buffer_empty()) {
        uint64_t read = std::min(remain_ws, static_cast<uint64_t>(TCPConfig::MAX_PAYLOAD_SIZE));
        std::string data = _stream.read(read);
        remain_ws -= data.size();
        TCPSegment seg;
        TCPHeader header;
        if (_stream.buffer_empty() && _stream.eof() && remain_ws > 0) {
            header.fin += 1;
            _send_fin = true;
            remain_ws--;
        }
        header.seqno = wrap(_next_seqno, _isn);

        seg.header() = header;
        _next_seqno += data.size();
        if (_send_fin) {
            _next_seqno += 1;
        }
        
        seg.payload() = Buffer(std::move(data));
        _segments_out.push(seg);
        _segments_waiting_ack.push_back(seg);
    }

    if (!_send_fin && remain_ws > 0 && _stream.eof()) {
        TCPHeader header;
        header.seqno = wrap(_next_seqno, _isn);
        _next_seqno += 1;
        header.fin = true;
        TCPSegment segment;
        segment.header() = header;
        _segments_out.push(segment);
        _segments_waiting_ack.push_back(segment);
        _send_fin = true;
        return;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    if (_segments_waiting_ack.empty()) {
        std::cerr << "_segments_waiting_ack is empty but receive ackno " << ackno.raw_value() << std::endl;
        return;
    }
    if (ackno.raw_value() <= _segments_waiting_ack.begin()->header().seqno.raw_value()) {
        std::cerr << "_segments_waiting_ack is out of date " << ackno.raw_value() << "," 
            << _segments_waiting_ack.begin()->header().seqno.raw_value() << std::endl;
    }
    
    uint64_t seq = unwrap(ackno, _isn, _next_seqno);
    if (seq > _next_seqno) {
        return;
    }
    // 如果第一次ack时导致在这个map中删除了对应的了，下次就不能了。所以不能用map来判断。t_send_window====FIN flag occupies space in window (part II)
    // 但是还有个明明发送了seq, 但是ack seq+2的 send_ack ===== Impossible ackno (beyond next seqno) is ignored
    // 这两种情况如何处理
   
    _window_size = window_size;

    auto iter = _segments_waiting_ack.begin();
    bool removed = false;
    while (iter != _segments_waiting_ack.end()) {
        if (iter->header().seqno.raw_value() <= ackno.raw_value()) {
            uint64_t last_seqno = unwrap(iter->header().seqno, _isn, _next_seqno) + iter->length_in_sequence_space();
            if (last_seqno <= seq) {
                iter = _segments_waiting_ack.erase(iter);
                removed = true;
                continue;
            } else {
                break;
            }
        }
    }
    if (removed) {
        _retransmission_count = 0;
        _retransmission_timeout = _initial_retransmission_timeout;
        _now_time = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if (_segments_waiting_ack.empty()) {
        return;
    }
    _now_time += ms_since_last_tick;
    if (_now_time >= _retransmission_timeout && _retransmission_count <= TCPConfig::MAX_RETX_ATTEMPTS) {
        _segments_out.push(_segments_waiting_ack.front());
        if (_window_size != 0) {
            _retransmission_timeout *= 2;
            _retransmission_count++;
        }
        _now_time = 0;
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_count; }

void TCPSender::send_empty_segment() {
    TCPHeader header;
    header.seqno = next_seqno();
    TCPSegment segment;
    segment.header() = header;
    _segments_out.push(segment);
}

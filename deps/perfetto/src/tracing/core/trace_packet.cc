/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/tracing/core/trace_packet.h"

#include "perfetto/base/logging.h"
#include "perfetto/protozero/proto_utils.h"
#include "src/tracing/core/sliced_protobuf_input_stream.h"

namespace perfetto {

TracePacket::TracePacket() = default;
TracePacket::~TracePacket() = default;

TracePacket::TracePacket(TracePacket&& other) noexcept {
  *this = std::move(other);
}

TracePacket& TracePacket::operator=(TracePacket&& other) {
  slices_ = std::move(other.slices_);
  other.slices_.clear();
  size_ = other.size_;
  other.size_ = 0;
  return *this;
}

void TracePacket::AddSlice(Slice slice) {
  size_ += slice.size;
  slices_.push_back(std::move(slice));
}

void TracePacket::AddSlice(const void* start, size_t size) {
  size_ += size;
  slices_.emplace_back(start, size);
}

std::tuple<char*, size_t> TracePacket::GetProtoPreamble() {
  using protozero::proto_utils::MakeTagLengthDelimited;
  using protozero::proto_utils::WriteVarInt;
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&preamble_[0]);

  constexpr uint8_t tag = MakeTagLengthDelimited(kPacketFieldNumber);
  static_assert(tag < 0x80, "TracePacket tag should fit in one byte");
  *(ptr++) = tag;

  ptr = WriteVarInt(size(), ptr);
  size_t preamble_size = reinterpret_cast<uintptr_t>(ptr) -
                         reinterpret_cast<uintptr_t>(&preamble_[0]);
  PERFETTO_DCHECK(preamble_size <= sizeof(preamble_));
  return std::make_tuple(&preamble_[0], preamble_size);
}

std::unique_ptr<TracePacket::ZeroCopyInputStream>
TracePacket::CreateSlicedInputStream() const {
  return std::unique_ptr<ZeroCopyInputStream>(
      new SlicedProtobufInputStream(&slices_));
}

}  // namespace perfetto

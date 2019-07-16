// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/wasm-api-tests/wasm-api-test.h"

namespace v8 {
namespace internal {
namespace wasm {

using ::wasm::Limits;
using ::wasm::MemoryType;

TEST_F(WasmCapiTest, Memory) {
  builder()->SetMinMemorySize(2);
  builder()->SetMaxMemorySize(3);
  builder()->AddExport(CStrVector("memory"), kExternalMemory, 0);

  ValueType i32_type[] = {kWasmI32, kWasmI32};
  FunctionSig return_i32(1, 0, i32_type);
  FunctionSig param_i32_return_i32(1, 1, i32_type);
  FunctionSig param_i32_i32(0, 2, i32_type);
  byte size_code[] = {WASM_MEMORY_SIZE};
  AddExportedFunction(CStrVector("size"), size_code, sizeof(size_code),
                      &return_i32);
  byte load_code[] = {WASM_LOAD_MEM(MachineType::Int8(), WASM_GET_LOCAL(0))};
  AddExportedFunction(CStrVector("load"), load_code, sizeof(load_code),
                      &param_i32_return_i32);
  byte store_code[] = {WASM_STORE_MEM(MachineType::Int8(), WASM_GET_LOCAL(0),
                                      WASM_GET_LOCAL(1))};
  AddExportedFunction(CStrVector("store"), store_code, sizeof(store_code),
                      &param_i32_i32);

  byte data[] = {0x1, 0x2, 0x3, 0x4};
  builder()->AddDataSegment(data, sizeof(data), 0x1000);

  Instantiate(nullptr);

  Memory* memory = GetExportedMemory(0);
  Func* size_func = GetExportedFunction(1);
  Func* load_func = GetExportedFunction(2);
  Func* store_func = GetExportedFunction(3);

  // Check initial state.
  EXPECT_EQ(2u, memory->size());
  EXPECT_EQ(0x20000u, memory->data_size());
  EXPECT_EQ(0, memory->data()[0]);
  EXPECT_EQ(1, memory->data()[0x1000]);
  EXPECT_EQ(4, memory->data()[0x1003]);
  Val args[2];
  Val result[1];
  // size == 2
  size_func->call(nullptr, result);
  EXPECT_EQ(2, result[0].i32());
  // load(0) == 0
  args[0] = Val::i32(0x0);
  load_func->call(args, result);
  EXPECT_EQ(0, result[0].i32());
  // load(0x1000) == 1
  args[0] = Val::i32(0x1000);
  load_func->call(args, result);
  EXPECT_EQ(1, result[0].i32());
  // load(0x1003) == 4
  args[0] = Val::i32(0x1003);
  load_func->call(args, result);
  EXPECT_EQ(4, result[0].i32());
  // load(0x1FFFF) == 0
  args[0] = Val::i32(0x1FFFF);
  load_func->call(args, result);
  EXPECT_EQ(0, result[0].i32());
  // load(0x20000) -> trap
  args[0] = Val::i32(0x20000);
  own<Trap*> trap = load_func->call(args, result);
  EXPECT_NE(nullptr, trap.get());

  // Mutate memory.
  memory->data()[0x1003] = 5;
  args[0] = Val::i32(0x1002);
  args[1] = Val::i32(6);
  trap = store_func->call(args, nullptr);
  EXPECT_EQ(nullptr, trap.get());
  args[0] = Val::i32(0x20000);
  trap = store_func->call(args, nullptr);
  EXPECT_NE(nullptr, trap.get());
  EXPECT_EQ(6, memory->data()[0x1002]);
  EXPECT_EQ(5, memory->data()[0x1003]);
  args[0] = Val::i32(0x1002);
  load_func->call(args, result);
  EXPECT_EQ(6, result[0].i32());
  args[0] = Val::i32(0x1003);
  load_func->call(args, result);
  EXPECT_EQ(5, result[0].i32());

  // Grow memory.
  EXPECT_EQ(true, memory->grow(1));
  EXPECT_EQ(3u, memory->size());
  EXPECT_EQ(0x30000u, memory->data_size());
  args[0] = Val::i32(0x20000);
  trap = load_func->call(args, result);
  EXPECT_EQ(nullptr, trap.get());
  EXPECT_EQ(0, result[0].i32());
  trap = store_func->call(args, nullptr);
  EXPECT_EQ(nullptr, trap.get());
  args[0] = Val::i32(0x30000);
  trap = load_func->call(args, result);
  EXPECT_NE(nullptr, trap.get());
  trap = store_func->call(args, nullptr);
  EXPECT_NE(nullptr, trap.get());
  EXPECT_EQ(false, memory->grow(1));
  EXPECT_EQ(true, memory->grow(0));

  // Create standalone memory.
  // TODO(wasm): Once Wasm allows multiple memories, turn this into an import.
  own<MemoryType*> mem_type = MemoryType::make(Limits(5, 5));
  own<Memory*> memory2 = Memory::make(store(), mem_type.get());
  EXPECT_EQ(5u, memory2->size());
  EXPECT_EQ(false, memory2->grow(1));
  EXPECT_EQ(true, memory2->grow(0));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

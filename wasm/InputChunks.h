//===- InputChunks.h --------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// An input chunk represents an indivisible blocks of code or data from an input
// file.  i.e. a single wasm data segment or a single wasm function.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_WASM_INPUT_CHUNKS_H
#define LLD_WASM_INPUT_CHUNKS_H

#include "InputFiles.h"
#include "WriterUtils.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/Object/Wasm.h"

using llvm::object::WasmSegment;
using llvm::wasm::WasmFunction;
using llvm::wasm::WasmRelocation;
using llvm::wasm::WasmSignature;
using llvm::object::WasmSection;

namespace lld {
namespace wasm {

class ObjFile;
class OutputSegment;

class InputChunk {
public:
  InputChunk(const ObjFile &F) : File(F) {}
  virtual ~InputChunk() = default;
  void copyRelocations(const WasmSection &Section);

  virtual const uint8_t *getData() const = 0;
  virtual uint32_t getSize() const = 0;
  virtual uint32_t getInputSectionOffset() const = 0;

  int32_t OutputOffset = 0;
  std::vector<WasmRelocation> Relocations;
  std::vector<OutputRelocation> OutRelocations;
  const ObjFile &File;
};

// Represents a WebAssembly data segment which can be included as part of
// an output data segments.  Note that in WebAssembly, unlike ELF and other
// formats, used the term "data segment" to refer to the continous regions of
// memory that make on the data section. See:
// https://webassembly.github.io/spec/syntax/modules.html#syntax-data
//
// For example, by default, clang will produce a separate data section for
// each global variable.
class InputSegment : public InputChunk {
public:
  InputSegment(const WasmSegment &Seg, const ObjFile &F)
      : InputChunk(F), Segment(Seg) {}

  // Translate an offset in the input segment to an offset in the output
  // segment.
  uint32_t translateVA(uint32_t Address) const;

  const OutputSegment *getOutputSegment() const { return OutputSeg; }

  void setOutputSegment(const OutputSegment *Segment, uint32_t Offset) {
    OutputSeg = Segment;
    OutputOffset = Offset;
  }

  const uint8_t *getData() const override {
    return Segment.Data.Content.data();
  }
  uint32_t getSize() const override { return Segment.Data.Content.size(); }
  uint32_t getInputSectionOffset() const override {
    return Segment.SectionOffset;
  }
  uint32_t getAlignment() const { return Segment.Data.Alignment; }
  uint32_t startVA() const { return Segment.Data.Offset.Value.Int32; }
  uint32_t endVA() const { return startVA() + getSize(); }
  StringRef getName() const { return Segment.Data.Name; }

protected:
  const WasmSegment &Segment;
  const OutputSegment *OutputSeg = nullptr;
};

// Represents a single wasm function within and input file.  These are
// combined to create the final output CODE section.
class InputFunction : public InputChunk {
public:
  InputFunction(const WasmSignature &S, const WasmFunction &Func,
                const ObjFile &F)
      : InputChunk(F), Signature(S), Function(Func) {}

  uint32_t getSize() const override { return Function.Size; }
  const uint8_t *getData() const override {
    return File.CodeSection->Content.data() + Function.CodeSectionOffset;
  }
  uint32_t getInputSectionOffset() const override {
    return Function.CodeSectionOffset;
  };

  uint32_t getOutputIndex() const { return OutputIndex.getValue(); };
  bool hasOutputIndex() const { return OutputIndex.hasValue(); };
  void setOutputIndex(uint32_t Index) {
    assert(!hasOutputIndex());
    OutputIndex = Index;
  };

  const WasmSignature &Signature;

protected:
  const WasmFunction &Function;
  llvm::Optional<uint32_t> OutputIndex;
};

} // namespace wasm
} // namespace lld

#endif // LLD_WASM_INPUT_CHUNKS_H

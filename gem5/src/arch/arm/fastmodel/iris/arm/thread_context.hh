/*
 * Copyright 2019 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Gabe Black
 */

#ifndef __ARCH_ARM_FASTMODEL_IRIS_ARM_THREAD_CONTEXT_HH__
#define __ARCH_ARM_FASTMODEL_IRIS_ARM_THREAD_CONTEXT_HH__

#include "arch/arm/fastmodel/iris/thread_context.hh"

namespace Iris
{

// This ThreadContext class translates accesses to state using gem5's native
// to the Iris API. This includes extracting and translating register indices.
class ArmThreadContext : public Iris::ThreadContext
{
  protected:
    static IdxNameMap miscRegIdxNameMap;
    static IdxNameMap intReg32IdxNameMap;
    static IdxNameMap intReg64IdxNameMap;

  public:
    ArmThreadContext(::BaseCPU *cpu, int id, System *system,
                     iris::IrisConnectionInterface *iris_if,
                     const std::string &iris_path);

    void initFromIrisInstance(const ResourceMap &resources) override;

    TheISA::PCState pcState() const override;
    void pcState(const TheISA::PCState &val) override;
    Addr instAddr() const override;
    Addr nextInstAddr() const override;

    iris::ResourceId pcRscId;
    iris::ResourceId icountRscId;

    ResourceIds intReg32Ids;
    ResourceIds intReg64Ids;

    Counter totalInsts() override;

    void setIntReg(RegIndex reg_idx, RegVal val) override;
    RegVal readIntReg(RegIndex reg_idx) const override;
    TheISA::ISA *
    getIsaPtr() override
    {
        panic("%s not implemented.", __FUNCTION__);
    }
};

} // namespace Iris

#endif // __ARCH_ARM_FASTMODEL_IRIS_ARM_THREAD_CONTEXT_HH__

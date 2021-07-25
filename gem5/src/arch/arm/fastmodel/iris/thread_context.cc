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

#include "arch/arm/fastmodel/iris/thread_context.hh"

#include "iris/detail/IrisCppAdapter.h"
#include "iris/detail/IrisObjects.h"

namespace Iris
{

void
ThreadContext::initFromIrisInstance(const ResourceMap &resources)
{
    bool enabled = false;
    call().perInstanceExecution_getState(_instId, enabled);
    _status = enabled ? Active : Suspended;

    suspend();
}

iris::ResourceId
ThreadContext::extractResourceId(
        const ResourceMap &resources, const std::string &name)
{
    return resources.at(name).rscId;
}

void
ThreadContext::extractResourceMap(
        ResourceIds &ids, const ResourceMap &resources,
        const IdxNameMap &idx_names)
{
    for (const auto &idx_name: idx_names) {
        int idx = idx_name.first;
        const std::string &name = idx_name.second;

        if (idx >= ids.size())
            ids.resize(idx + 1, iris::IRIS_UINT64_MAX);

        ids[idx] = extractResourceId(resources, name);
    }
}

iris::IrisErrorCode
ThreadContext::instanceRegistryChanged(
        uint64_t esId, const iris::IrisValueMap &fields, uint64_t time,
        uint64_t sInstId, bool syncEc, std::string &error_message_out)
{
    const std::string &event = fields.at("EVENT").getString();
    const iris::InstanceId id = fields.at("INST_ID").getU64();
    const std::string &name = fields.at("INST_NAME").getString();

    if (name != "component." + _irisPath)
        return iris::E_ok;

    if (event == "added")
        _instId = id;
    else if (event == "removed")
        _instId = iris::IRIS_UINT64_MAX;
    else
        panic("Unrecognized event type %s", event);

    return iris::E_ok;
}

iris::IrisErrorCode
ThreadContext::phaseInitLeave(
        uint64_t esId, const iris::IrisValueMap &fields, uint64_t time,
        uint64_t sInstId, bool syncEc, std::string &error_message_out)
{
    std::vector<iris::ResourceInfo> resources;
    call().resource_getList(_instId, resources);

    ResourceMap resourceMap;
    for (auto &resource: resources)
        resourceMap[resource.name] = resource;

    initFromIrisInstance(resourceMap);

    return iris::E_ok;
}

ThreadContext::ThreadContext(
        BaseCPU *cpu, int id, System *system,
        iris::IrisConnectionInterface *iris_if, const std::string &iris_path) :
    _cpu(cpu), _threadId(id), _system(system), _irisPath(iris_path),
    _instId(iris::IRIS_UINT64_MAX), _status(Active),
    client(iris_if, "client." + iris_path)
{
    iris::InstanceInfo info;
    auto ret_code = noThrow().instanceRegistry_getInstanceInfoByName(
                info, "component." + iris_path);
    if (ret_code == iris::E_ok) {
        // The iris instance registry already new about this path.
        _instId = info.instId;
    } else {
        // This path doesn't (yet) exist. Set the ID to something invalid.
        _instId = iris::IRIS_UINT64_MAX;
    }

    typedef ThreadContext Self;
    iris::EventSourceInfo evSrcInfo;

    client.registerEventCallback<Self, &Self::instanceRegistryChanged>(
            this, "ec_IRIS_INSTANCE_REGISTRY_CHANGED",
            "Install the iris instance ID", "Iris::ThreadContext");
    call().event_getEventSource(iris::IrisInstIdGlobalInstance, evSrcInfo,
            "IRIS_INSTANCE_REGISTRY_CHANGED");
    regEventStreamId = iris::IRIS_UINT64_MAX;
    static const std::vector<std::string> fields =
        { "EVENT", "INST_ID", "INST_NAME" };
    call().eventStream_create(iris::IrisInstIdGlobalInstance, regEventStreamId,
            evSrcInfo.evSrcId, client.getInstId(), &fields);

    client.registerEventCallback<Self, &Self::phaseInitLeave>(
            this, "ec_IRIS_SIM_PHASE_INIT_LEAVE",
            "Initialize register contexts", "Iris::ThreadContext");
    call().event_getEventSource(iris::IrisInstIdSimulationEngine, evSrcInfo,
            "IRIS_SIM_PHASE_INIT_LEAVE");
    initEventStreamId = iris::IRIS_UINT64_MAX;
    call().eventStream_create(
            iris::IrisInstIdSimulationEngine, initEventStreamId,
            evSrcInfo.evSrcId, client.getInstId());
}

ThreadContext::~ThreadContext()
{
    call().eventStream_destroy(
            iris::IrisInstIdSimulationEngine, initEventStreamId);
    initEventStreamId = iris::IRIS_UINT64_MAX;
    client.unregisterEventCallback("ec_IRIS_SIM_PHASE_INIT_LEAVE");

    call().eventStream_destroy(
            iris::IrisInstIdGlobalInstance, regEventStreamId);
    regEventStreamId = iris::IRIS_UINT64_MAX;
    client.unregisterEventCallback("ec_IRIS_INSTANCE_REGISTRY_CHANGED");
}

ThreadContext::Status
ThreadContext::status() const
{
    return _status;
}

void
ThreadContext::setStatus(Status new_status)
{
    if (new_status == Active) {
        if (_status != Active)
            call().perInstanceExecution_setState(_instId, true);
    } else {
        if (_status == Active)
            call().perInstanceExecution_setState(_instId, false);
    }
    _status = new_status;
}

RegVal
ThreadContext::readMiscRegNoEffect(RegIndex misc_reg) const
{
    iris::ResourceReadResult result;
    call().resource_read(_instId, result, miscRegIds.at(misc_reg));
    return result.data.at(0);
}

void
ThreadContext::setMiscRegNoEffect(RegIndex misc_reg, const RegVal val)
{
    iris::ResourceWriteResult result;
    call().resource_write(_instId, result, miscRegIds.at(misc_reg), val);
}

RegVal
ThreadContext::readIntReg(RegIndex reg_idx) const
{
    iris::ResourceReadResult result;
    call().resource_read(_instId, result, intRegIds.at(reg_idx));
    return result.data.at(0);
}

void
ThreadContext::setIntReg(RegIndex reg_idx, RegVal val)
{
    iris::ResourceWriteResult result;
    call().resource_write(_instId, result, intRegIds.at(reg_idx), val);
}

} // namespace Iris

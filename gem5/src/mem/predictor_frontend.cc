/*
 * Copyright (c) 2011-2013, 2015 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2006 The Regents of The University of Michigan
 * All rights reserved.
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
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR o
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Ali Saidi
 *          Steve Reinhardt
 *          Andreas Hansson
 */

/**
 * @file
 * Implementation of a memory-mapped bridge that connects a master
 * and a slave through a request and response queue.
 */

#include "predictor_frontend.hh"

#include "base/trace.hh"
#include "debug/PredictorFrontend.hh"
#include "debug/PredictorFrontendInterface.hh"
#include "debug/PredictorFrontendLogic.hh"
#include "debug/CacheLineAccumulatorRetire.hh"
#include "debug/PredictorConfidence.hh"
#include "mem/cache/cache.hh"
#include "mem/predictor/Common.hh"
#include "mem/predictor/Declarations.hh"
#include "mem/predictor/LastFoundKeyEntry.hh"
#include "mem/predictor/PCQueue.hh"
#include "params/Bridge.hh"
#include "mem/page_table.hh"
#include "mem/dram_ctrl.hh"
#include <memory>

#include <algorithm>
#include <fstream>

std::vector<Addr> addrOfInterest = {0x11e0, 0x11e7, 0x16fc};

PredictorFrontend::PFSlavePort::PFSlavePort(const std::string& _name,
                                         PredictorFrontend& _pf,
                                         PFMasterPort& _masterPort,
                                         Cycles _delay, int _resp_limit,
                                         std::vector<AddrRange> _ranges)
    : SlavePort(_name, &_pf), pf(_pf), masterPort(_masterPort),
      delay(_delay), ranges(_ranges.begin(), _ranges.end()),
      outstandingResponses(0), retryReq(false), respQueueLimit(_resp_limit),
      sendEvent([this]{ trySendTiming(); }, _name) 
{
}

PredictorFrontend::PFMasterPort::PFMasterPort(const std::string& _name,
                                           PredictorFrontend& _pf,
                                           PFSlavePort& _slavePort,
                                           Cycles _delay, int _req_limit)
    : MasterPort(_name, &_pf), pf(_pf), slavePort(_slavePort),
      delay(_delay), reqQueueLimit(_req_limit),
      sendEvent([this]{ trySendTiming(); }, _name)
{
}

PredictorFrontend::PredictorFrontend(Params *p)
    : ClockedObject(p),
      slavePort(p->name + ".slave", *this, masterPort,
                ticksToCycles(p->delay), p->resp_size, p->ranges),
      masterPort(p->name + ".master", *this, slavePort,
                 ticksToCycles(p->delay), p->req_size),
      writeHistoryBuffer(p->name + ".whb", 
                 512*SharedArea::sizeMultiplier),
                //!  1024*SharedArea::sizeMultiplier),
      predictorTable(p->name + ".pred_t"),
      pendingTable(p->name + ".pend_t", &this->writeHistoryBuffer)
{
    bothAddrDataNotFound
        .name(p->name + ".bothAddrDataNotFound")
        .desc(".bothAddrDataNotFound");

    predictedWriteCount
        .name(p->name + ".predictedWriteCount")
        .desc("Total numhber of PM write predicted by the frontend");    
    predictorTablePromotions
        .name(p->name + ".predictorTablePromotions")
        .desc("Total numhber of entries of predictor table promoted to pending table");     
    pmAccumulatorFlushes
        .name(p->name + "pmAccumulatorFlushes")
        .desc("Total number of times the PM accumulator was flushed.");
    avgPredictorTableSz
        .name(p->name + ".avgPredictorTableSz")
        .desc("Average size of the predictor table.");     
    avgPendingTableSz
        .name(p->name + ".avgPendingTableSz")
        .desc("Average length of the pending table.");    
    pcCaptureDistance
        .init(0, this->writeHistoryBuffer.get_max_size(), 1)
        .name(p->name + ".pcCaptureDistance")
        .desc("Distribution of the distance at which a predicting entry was found in "
              "the write history buffer when searching for a pm write.")
        .flags(Stats::pdf);
    constant0Prediction
        .name(p->name + ".constant0Prediction")
        .desc("Number of data chunks that were predicted using constant 0"
              " prediction.");
    pWritesFoundInWHB
        .name(p->name + ".pWritesFoundInWHB")
        .desc("Number of persistent writes completely found in the write history buffer");
    zeroCachelines
        .name(p->name + ".zeroCachelines")
        .desc("Number of cachelines that were all zeros when they were flushed.");
    pmStores
        .name(p->name + ".pmStores")
        .desc("Counts PM stores passed through the frontend.");
    noAddrFound
        .name(p->name + ".noAddrFound")
        .desc("Number of flushes which didn't have any matching whb entry for the address.");
    whbAddrFound
        .name(p->name + ".whbAddrFound")
        .desc("whbAddrFound");
    cacheLineNotInWHB
        .name(p->name + ".cacheLineNotInWHB")
        .desc("Cache lines that were not found in the write history buffer.");
    clwbCount
        .name(p->name + ".clwbCount")
        .desc("Number of CLWBs seen by the frontend that didn't have empty cache lines");
    PMAddrChanges
        .name(p->name + ".PMAddrChanges")
        .desc("PMAddrChanges");
    clwbResponses
        .name(p->name + ".clwbResponses")
        .desc("clwbResponses");
    clwbCountInclInv
        .name(p->name + ".clwbCountInclInv")
        .desc("clwbCountInclInv");
    addrChangesBwClwb
        .name(p->name + ".addrChangesBwClwb")
        .desc("addrChangesBwClwb")
        .init(0, 10, 1);
    pcAccumulatorEvictions
        .name(p->name + ".pcAccumulatorEvictions")
        .desc("pcAccumulatorEvictions");
    cacheLineAccumulatorSize
        .name(p->name + ".cacheLineAccumulatorSize")
        .desc("Number of cachelines pc accumulator can hold simulataneously.");
    clwbToLastWriteDistance
        .name(p->name + ".clwbToLastWriteDistance")
        .desc("clwbToLastWriteDistance")
        .init(0, 1000, 500);
    consecutiveWritesToCL
        .name(p->name + ".consecutiveWritesToCL")
        .desc("consecutiveWritesToCL")
        .init(0, 1, 20);
    clwbWithoutWrites
        .name(p->name + "clwbWithoutWrites")
        .desc("Number of clwb issued to location without writing them ever");
    clEvicKiloTicksSinceLastWrite
        .name(p->name + ".clEvicKiloTicksSinceLastWrite")
        .desc(".clEvicKiloTicksSinceLastWrite")
        .init(0,1000, 500);
    freePredictions
        .name(p->name + ".freePredictions")
        .desc(".freePredictions");
    missingChunkCount
        .name(p->name + ".missingChunkCount")
        .desc(".missingChunkCount")
        .init(0, 16, 1);
    invChunkCountSampler
        .name(p->name + ".invChunkCountSampler")
        .desc("invChunkCountSampler")
        .init(0, 16, 1);
    writebackDistStat
        .name(p->name + ".writebackDistStat")
        .desc("writebackDistStat")
        .init(0, 10000000, 100);
    writebackDistStatMicro
        .name(p->name + ".writebackDistStatMicro")
        .desc("writebackDistStat")
        .init(0, 1000, 1);
    whbTimeLen
        .name(p->name + ".whbTimeLen")
        .desc("")
        .init(0,10,1000);

    char* envResult = std::getenv("ENABLE_VOLATILE_DUMP");

    if (envResult != nullptr) {
        printf("Enabling volatile dump\n");
        enableVolatileDump = std::string(envResult);
        myFile.open(enableVolatileDump);
    } else {
        enableVolatileDump = "";
        myFile.open("/std/dev/null");
    }

    genHash.open("./genHash.stats");

    CL_ACC_SIZE = std::stol(get_env_str("CL_ACC_SIZE", "4"));
    disablePerPCConfidence = get_env_val("DISABLE_PER_PC_CONFIDENCE");
    disableFreePrediction = get_env_val("DISABLE_FREE_PREDICTION");
    disableFancyAddrPred = get_env_val("DISABLE_FANCY_ADDR_PRED");
    std::cout << "Using cacheline accumulator size = " << CL_ACC_SIZE << std::endl;
    cacheLineAccumulatorSize += CL_ACC_SIZE;

    /* Read the PC of interest */
    char* rawVal = std::getenv("PC_OF_INTEREST");
    std::cout << "Raw value = " << rawVal << std::endl;
    addrOfInterest = {};
    if (rawVal != nullptr) {
        std::string val_str = std::string(rawVal);
        std::stringstream ss(val_str);
        std::cout << "Got the string " << val_str << " for PC of interest" << std::endl;
        while(ss.good()) {
            std::string substr;
            std::getline(ss, substr, ',');
            std::cout << "Adding " << substr << " to PCs of interest" << std::endl;
            addrOfInterest.push_back(std::stol(substr, 0 /*pos*/, 16 /*base*/));
        }
    }
    std::cout << "PCs of interest: "  << vec2hexStr(addrOfInterest) << std::endl;
}

Port &
PredictorFrontend::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "master")
        return masterPort;
    else if (if_name == "slave")
        return slavePort;
    else
        // pass it along to our super class
        return ClockedObject::getPort(if_name, idx);
}

void
PredictorFrontend::init()
{
    // make sure both sides are connected and have the same block size
    if (!slavePort.isConnected() || !masterPort.isConnected())
        fatal("Both ports of a pf must be connected.\n");

    // notify the master side  of our address ranges
    slavePort.sendRangeChange();
}

bool
PredictorFrontend::PFSlavePort::respQueueFull() const
{
    return outstandingResponses == respQueueLimit;
}

bool
PredictorFrontend::PFMasterPort::reqQueueFull() const
{
    return transmitList.size() == reqQueueLimit;
}

bool
PredictorFrontend::PFMasterPort::recvTimingResp(PacketPtr pkt)
{
    // all checks are done when the request is accepted on the slave
    // side, so we are guaranteed to have space for the response
    DPRINTF(PredictorFrontendInterface, "recvTimingResp: %s addr 0x%x\n",
            pkt->cmdString(), pkt->getAddr());

    DPRINTF(PredictorFrontendInterface, "Request queue size: %d\n", transmitList.size());

    // technically the packet only reaches us after the header delay,
    // and typically we also need to deserialise any payload (unless
    // the two sides of the pf are synchronous)
    Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
    pkt->headerDelay = pkt->payloadDelay = 0;

    Tick bmoLatency = 0;
    bool isClwb = pkt->req->isToPOC() and pkt->req->getSize() == 1 and pkt->hasData() == false and is_vaddr_pm(pkt->req->getVaddr());
    if (PredictorBackend::predictorEnabled and isClwb and pkt->req->getVaddr() != 0) {
        Addr paddr = 0;
        EmulationPageTable::pageTableStaticObj->translate(pkt->req->getVaddr(), paddr);
        panic_if(paddr == 0, "Cannot translate %p", pkt->req->getVaddr());
        bmoLatency = DRAMCtrl::clwbLatency[paddr];
        // if (std::getenv("BMO_LATENCY") == nullptr) {
        //     bmoLatency = 0;
        // } else {
        //     bmoLatency = 600000;
        // }
        DRAMCtrl::clwbLatency.erase(paddr);
        pf.clwbResponses++;
            // this->avgBmoLatency.sample(bmoLatency);
        // std::cout << "[0] Sampling latency = " << bmoLatency << std::endl;
    }


    slavePort.schedTimingResp(pkt, pf.clockEdge(delay) +
                              receive_delay + bmoLatency);

    return true;
}

bool
PredictorFrontend::PFSlavePort::recvTimingReq(PacketPtr pkt)
{
    // std::ofstream returnValue;
    // returnValue.open("/ramdisk/returnvalue");
    //     returnValue << __LINE__ << ":" << __builtin_return_address(0) << std::endl;
    DPRINTF(PredictorFrontendInterface, "recvTimingReq: %s addr 0x%x\n",
            pkt->cmdString(), pkt->getAddr());

    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    // we should not get a new request after committing to retry the
    // current one, but unfortunately the CPU violates this rule, so
    // simply ignore it for now
    if (retryReq)
        return false;

    /** For some reason recvTimingReq is called twice within the same tick value,
     *  keeping track of the tick value prevents this.
     */
    if (curTick() != pf.lastPredictorTick) {
        pf.predictorHandleRequest(pkt);
        pf.lastPredictorTick = curTick();
    }

    
    DPRINTF(PredictorFrontendInterface, "Response queue size: %d outresp: %d\n",
            transmitList.size(), outstandingResponses);

    // if the request queue is full then there is no hope
    
    if (masterPort.reqQueueFull()) {
        DPRINTF(PredictorFrontendInterface, "Request queue full\n");
        retryReq = true;
    
    } else {
        // look at the response queue if we expect to see a response
    
        bool expects_response = pkt->needsResponse();
        if (expects_response) {
            if (respQueueFull()) {
                DPRINTF(PredictorFrontendInterface, "Response queue full\n");
                retryReq = true;
            } else {
                // ok to send the request with space for the response
                DPRINTF(PredictorFrontendInterface, "Reserving space for response\n");
                assert(outstandingResponses != respQueueLimit);
                ++outstandingResponses;

                // no need to set retryReq to false as this is already the
                // case
            }
    
        }

        if (!retryReq) {
            // technically the packet only reaches us after the header
            // delay, and typically we also need to deserialise any
            // payload (unless the two sides of the bridge are
            // synchronous)
            Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
            pkt->headerDelay = pkt->payloadDelay = 0;

            masterPort.schedTimingReq(pkt, pf.clockEdge(delay) +
                                      receive_delay);
    
        }
    }
    

    // remember that we are now stalling a packet and that we have to
    // tell the sending master to retry once space becomes available,
    // we make no distinction whether the stalling is due to the
    // request queue or response queue being full
    // returnValue << __LINE__ << ":" << __builtin_return_address(0) << std::endl;
    // returnValue.close();
    return !retryReq;
}

void
PredictorFrontend::PFSlavePort::retryStalledReq()
{
    if (retryReq) {
        DPRINTF(PredictorFrontendInterface, "Request waiting for retry, now retrying\n");
        retryReq = false;
        sendRetryReq();
    }
}

void
PredictorFrontend::PFMasterPort::schedTimingReq(PacketPtr pkt, Tick when)
{
    // If we're about to put this packet at the head of the queue, we
    // need to schedule an event to do the transmit.  Otherwise there
    // should already be an event scheduled for sending the head
    // packet.
    if (transmitList.empty()) {
        pf.schedule(sendEvent, when);
    }

    assert(transmitList.size() != reqQueueLimit);

    transmitList.emplace_back(pkt, when);
}


void
PredictorFrontend::PFSlavePort::schedTimingResp(PacketPtr pkt, Tick when)
{
    // If we're about to put this packet at the head of the queue, we
    // need to schedule an event to do the transmit.  Otherwise there
    // should already be an event scheduled for sending the head
    // packet.
    if (transmitList.empty()) {
        pf.schedule(sendEvent, when);
    }

    transmitList.emplace_back(pkt, when);
}

void
PredictorFrontend::PFMasterPort::trySendTiming()
{
    assert(!transmitList.empty());

    DeferredPacket req = transmitList.front();

    assert(req.tick <= curTick());

    PacketPtr pkt = req.pkt;

    DPRINTF(PredictorFrontendInterface, "trySend request addr 0x%x, queue size %d\n",
            pkt->getAddr(), transmitList.size());

    if (sendTimingReq(pkt)) {
        // send successful
        transmitList.pop_front();
        DPRINTF(PredictorFrontendInterface, "trySend request successful\n");

        // If there are more packets to send, schedule event to try again.
        if (!transmitList.empty()) {
            DeferredPacket next_req = transmitList.front();
            DPRINTF(PredictorFrontendInterface, "Scheduling next send\n");
            pf.schedule(sendEvent, std::max(next_req.tick,
                                                pf.clockEdge()));
        }

        // if we have stalled a request due to a full request queue,
        // then send a retry at this point, also note that if the
        // request we stalled was waiting for the response queue
        // rather than the request queue we might stall it again
        slavePort.retryStalledReq();
    }

    // if the send failed, then we try again once we receive a retry,
    // and therefore there is no need to take any action
}

void
PredictorFrontend::PFSlavePort::trySendTiming()
{
    assert(!transmitList.empty());

    DeferredPacket resp = transmitList.front();

    assert(resp.tick <= curTick());

    PacketPtr pkt = resp.pkt;

    DPRINTF(PredictorFrontendInterface, "trySend response addr 0x%x, outstanding %d\n",
            pkt->getAddr(), outstandingResponses);

    if (sendTimingResp(pkt)) {
        // send successful
        transmitList.pop_front();
        DPRINTF(PredictorFrontendInterface, "trySend response successful\n");

        assert(outstandingResponses != 0);
        --outstandingResponses;

        // If there are more packets to send, schedule event to try again.
        if (!transmitList.empty()) {
            DeferredPacket next_resp = transmitList.front();
            DPRINTF(PredictorFrontendInterface, "Scheduling next send\n");
            pf.schedule(sendEvent, std::max(next_resp.tick,
                                                pf.clockEdge()));
        }

        // if there is space in the request queue and we were stalling
        // a request, it will definitely be possible to accept it now
        // since there is guaranteed space in the response queue
        if (!masterPort.reqQueueFull() && retryReq) {
            DPRINTF(PredictorFrontendInterface, "Request waiting for retry, now retrying\n");
            retryReq = false;
            sendRetryReq();
        }
    }

    // if the send failed, then we try again once we receive a retry,
    // and therefore there is no need to take any action
}

void
PredictorFrontend::PFMasterPort::recvReqRetry()
{
    trySendTiming();
}

void
PredictorFrontend::PFSlavePort::recvRespRetry()
{
    trySendTiming();
}

Tick
PredictorFrontend::PFSlavePort::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    return delay * pf.clockPeriod() + masterPort.sendAtomic(pkt);
}

void
PredictorFrontend::PFSlavePort::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(name());

    // check the response queue
    for (auto i = transmitList.begin();  i != transmitList.end(); ++i) {
        if (pkt->trySatisfyFunctional((*i).pkt)) {
            pkt->makeResponse();
            return;
        }
    }

    // also check the master port's request queue
    if (masterPort.trySatisfyFunctional(pkt)) {
        return;
    }

    pkt->popLabel();

    // fall through if pkt still not satisfied
    masterPort.sendFunctional(pkt);
}

bool
PredictorFrontend::PFMasterPort::trySatisfyFunctional(PacketPtr pkt)
{
    bool found = false;
    auto i = transmitList.begin();

    while (i != transmitList.end() && !found) {
        if (pkt->trySatisfyFunctional((*i).pkt)) {
            pkt->makeResponse();
            found = true;
        }
        ++i;
    }

    return found;
}

AddrRangeList
PredictorFrontend::PFSlavePort::getAddrRanges() const
{
    return ranges;
}

PredictorFrontend *
PredictorFrontendParams::create()
{
    return new PredictorFrontend(this);
}

/**
 * Returns boolean representing if this packet can be added to 
 * the write history buffer
 * @param pkt Packet to ttest
 * @return Boolean value, true if the packet's contentcan be inserted
 *         to the write shitory buffer 
*/
bool
PredictorFrontend::canAddToWhb(const PacketPtr pkt) {
    return pkt->isWrite();
}

void
PredictorFrontend::updateWriteHistoryBuffer(const PacketPtr pkt) {
    // this->whbTimeLen.sample((curTick() - this->writeHistoryBuffer.get_front().get_gen_tick())/1000);
    assert(pkt->req->hasPC() && pkt->hasData() && pkt->isWrite());

    DataChunk *dataChunks = (DataChunk*)pkt->getConstPtr<uint64_t>();
    WriteHistoryBufferEntry *whbEntry 
            = new WriteHistoryBufferEntry(
                    pkt->req->getPC(), 
                    pkt->req->getVaddr(), 
                    dataChunks, 
                    pkt->req->getSize()/sizeof(DataChunk),
                    this->predictorTable.get_path_hash());

    whbEntry->destAddr_diag = (Addr_t)(pkt->req->getVaddr());
    whbEntry->insertionTick_diag = curTick();
    whbEntry->set_gen_tick(curTick());

    size_t len = pkt->req->getSize();
    size_t chunkCount = len/sizeof(DataChunk);

    Addr_t addr = pkt->req->getVaddr();
    Addr_t offset = get_cacheline_off(addr);
    size_t chunkOffset = offset/sizeof(DataChunk);

    if (DTRACE(PredictorFrontendLogic)) {
        // std::cout << std::hex << "0x" << pkt->req->getPC() << ": ";
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            uint32_t data = 0;
            if (i >= chunkOffset and i < chunkOffset+chunkCount) {
                data = dataChunks[i-chunkOffset];
            }
            // std::cout << std::hex << i << ":0x" << std::setfill('0') << std::setw(8) << data << " ";
        } /* std::cout << "\n"; */
    }
    whbEntry->set_id(curTick());
    whbEntry->set_size(chunkCount);
    
    //! Added true condition to disable selective WHB insertion 
    if (not whbEntry->get_cacheline().all_zeros() or true) {
        writeHistoryBuffer.push_back(whbEntry);
    }
    return;
}

void
PredictorFrontend::cachelineAccumulatorRetireTick() {
    if (disableFreePrediction) {
        return;
    }

    if ((curTick() - lastAccumulatorRetireTick) > ACC_RETIRE_TICK_PERIOD) {
        lastAccumulatorRetireTick = curTick();

        std::deque<Addr_t> retireQueue;

        for (std::pair<Addr_t, CacheLine> cacheline : this->cacheLineAccumulator) {
            Tick age = curTick() - cacheline.second.get_time_of_last_update();
            if (age > ACC_ENTRY_RETIRE_THRESHOLD) {
                retireQueue.push_back(cacheline.first);
                Addr paddr;
                EmulationPageTable::pageTableStaticObj->translate(cacheline.first, paddr);
                // std::cout << "(tick) retiring address " << (void*)cacheline.first << " Physical address: " << (void*)paddr << " at age " << age/1000.0/1000.0 << "us" <<  std::endl;
            }
        }

        std::deque<CompletedWriteEntry> entriesToSend;

        for (Addr_t addr : retireQueue) {
            Addr paddr = 0;

            /* Cache uses physical address */
            if (EmulationPageTable::pageTableStaticObj->translate(addr, paddr)
                    and this->cacheLineAccumulator[addr].is_dirty()) { 
                /* Lookup the physical address */
                // CacheLine l1CacheLine = CacheLine(paddr,  Cache::l1DCacheStaticObj);
                CacheLine cacheData = CacheLine(paddr,  Cache::l2CacheStaticObj);
                CacheLine accumulatedCacheLine = this->cacheLineAccumulator.at(addr);

                // DPRINTF(CacheLineAccumulatorRetire, "<+> %p l1 : %s\n", paddr, l1CacheLine.to_string());
                // DPRINTF(CacheLineAccumulatorRetire, "<+> %p accumulator : %s\n", paddr, accumulatedCacheLine.to_string());
                // DPRINTF(CacheLineAccumulatorRetire, "<+> %p cadchedata  : %s\n", paddr, cacheData.to_string());

                cacheData.overwriteFrom(accumulatedCacheLine);
                DPRINTF(CacheLineAccumulatorRetire, "<+> %p new         : %s\n", paddr, cacheData.to_string());

                /* Set all the datachunks as free predictions */
                for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
                    cacheData.get_datachunks()[i].set_free_prediction();
                }

                /* Entry is ready to send to the backend */
                entriesToSend.push_back(CompletedWriteEntry(addr, cacheData, hash_t("Free write")));

                entriesToSend.back().set_time_of_addr_gen(curTick());
                entriesToSend.back().set_time_of_data_gen(curTick());
                entriesToSend.back().set_time_of_creation(curTick());
                entriesToSend.back().set_orig_cacheline(cacheData);

                // std::cout << "Retiring with cacheline: " << cacheData.to_string().c_str()  << std::endl;

                /* Set the accumulator entry as clean */
                this->cacheLineAccumulator[addr].set_clean();
            } else {
                // std::cout << "Unable to retire address " << (void*)addr << " clacc dirty? " << this->cacheLineAccumulator[addr].is_dirty() << std::endl;
            }
        }
        for (auto completedWrite : entriesToSend) {
            this->freePredictions++;
            this->predictedWriteCount++;
            PredictorBackend::addCompletedWrite(completedWrite);
        }
    }
}

void PredictorFrontend::SendCacheLineToBackend(CacheLine cacheline) {
    Addr addr = cacheline.get_addr();
    Addr paddr = 0;

    /* Cache uses physical address */
    if (EmulationPageTable::pageTableStaticObj->translate(addr, paddr)) {
        /* Lookup the physical address */
        CacheBlk *blk = Cache::l2CacheStaticObj->tags->findBlock(paddr, false);
        if (blk != nullptr) {
            DataChunk *data = (DataChunk*)blk;
            
            // std::cout << "addr = " << addr << " | ";
            // for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            //     std::cout << " " << data[i];
            // }
            // std::cout << std::endl;

            CacheLine cacheData = CacheLine(cacheline_align(addr), data, DATA_CHUNK_COUNT, true);
            CacheLine accumulatedCacheLine = cacheline;

            DPRINTFR(CacheLineAccumulatorRetire, "<+> %p accumulator : %s\n", paddr, accumulatedCacheLine.to_string());
            DPRINTFR(CacheLineAccumulatorRetire, "<+> %p cadchedata  : %s\n", paddr, cacheData.to_string());

            cacheData.overwriteFrom(accumulatedCacheLine);
            DPRINTFR(CacheLineAccumulatorRetire, "<+> %p new         : %s\n", paddr, cacheData.to_string());

            /* Set all the datachunks as free predictions */
            for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
                cacheData.get_datachunks()[i].set_free_prediction();
            }

            /* Entry is ready to send to the backend */
            CompletedWriteEntry entryToSend = CompletedWriteEntry(addr, cacheData, hash_t("Free write"));

            entryToSend.set_time_of_addr_gen(curTick());
            entryToSend.set_time_of_data_gen(curTick());
            entryToSend.set_time_of_creation(curTick());
            entryToSend.set_orig_cacheline(cacheData);

            PredictorBackend::addCompletedWrite(entryToSend);
        }
    }
}

void
PredictorFrontend::manageCachelineAcc(PacketPtr pkt) {
    Addr cachelineAddr = cacheline_align(pkt->req->getVaddr());
    bool exists = true;
    if (this->cacheLineAccumulator.find(cachelineAddr) == this->cacheLineAccumulator.end()) {
        exists = false;
    }

    this->cacheLineAccumulator[cachelineAddr];
    this->cacheLineAccumulator[cachelineAddr].set_dirty();
    // std::cout << "Setting cacheline accumulator entry for address " << (void*)cachelineAddr << " to dirty" << std::endl;

    if (not exists) {
        // std::cout << "[CL Accumulator] Adding new entry for address " << (void*)cachelineAddr << " aligned = " << (void*)cacheline_align(cachelineAddr) << " at tick " << curTick() << " pc value = " << (void*)this->lastWritePCToAddr[cachelineAddr] <<  std::endl;
        this->cacheLineAccumulator[cachelineAddr].set_addr(pkt->req->getVaddr());
        this->cacheLineAccumulator[cachelineAddr].set_time_of_creation(curTick());
        this->cacheLineAccumulator[cachelineAddr].set_time_of_last_update(curTick());
    }

    Tick oldestTimeOfGen = -1;
    size_t addrOfOldestTimeOfGen = -1;

    DPRINTF(CacheLineAccumulator,
            "Cacheline accumulator entries: [ ");
    if (DTRACE(CacheLineAccumulator)) {
        for (std::pair<Addr, CacheLine> pair : this->cacheLineAccumulator) {
            DPRINTF(CacheLineAccumulator, "%p ", (void*)pair.first);
        }
    }
    DPRINTF(CacheLineAccumulator,
        "]\n");

    /* If the current accumulator size is greater than the maximum limit, find the oldest entry */
    if (this->cacheLineAccumulator.size() > CL_ACC_SIZE) {
        for (std::pair<Addr, CacheLine> potAddr : this->cacheLineAccumulator) {
            Tick toc = this->cacheLineAccumulator.at(potAddr.first).get_time_of_creation();
            if (toc < oldestTimeOfGen) {
                oldestTimeOfGen = toc;
                addrOfOldestTimeOfGen = potAddr.first;
            }
        }
    }

    /* Delete the oldest found accumulator entry */
    if (oldestTimeOfGen != -1) {
        DPRINTF(CacheLineAccumulator, 
                "Deleting entry from the CL accumulator, created at %lu with aligned address %p\n", 
                oldestTimeOfGen, addrOfOldestTimeOfGen);
        DPRINTF(CacheLineAccumulator,
                "[CL Accumulator] Deleting entry from the CL accumulator, "
                "created at %lu with aligned address %p, age %lu and last "
                "write to at %lu kTicks ago, clAcc.size = %d\n", 
                oldestTimeOfGen, addrOfOldestTimeOfGen, curTick() - oldestTimeOfGen, 
                (curTick() - this->lastWriteTickToAddr[pkt->req->getVaddr()])/1000, 
                this->cacheLineAccumulator.size());
        this->pcAccumulatorEvictions++;

        /* Collect the statistics on the age of this entry */
        panic_if_not(this->lastWriteTickToAddr.find(addrOfOldestTimeOfGen) 
                        != this->lastWriteTickToAddr.end());
        this->clEvicKiloTicksSinceLastWrite.sample(
            (curTick() - this->lastWriteTickToAddr[pkt->req->getVaddr()])/1000
        );
        
        panic_if(addrOfOldestTimeOfGen == -1, "Incorrect execution state");
        
        /* Any evicted lines goes to the backend as a prediction */
        PredictorFrontend::SendCacheLineToBackend(this->cacheLineAccumulator[addrOfOldestTimeOfGen]);
        DPRINTF(CacheLineAccumulator, "[Not found] Unable to find CLWB for cacheline %s\n", 
                this->cacheLineAccumulator.at(addrOfOldestTimeOfGen).to_string().c_str()); 
        this->cacheLineAccumulator.erase(addrOfOldestTimeOfGen);
    } 
    
    if (oldestTimeOfGen == -1 and this->cacheLineAccumulator.size() > CL_ACC_SIZE) {
        panic("Unable to delete any entry from the cacheline accumulator");
    }
}

void
PredictorFrontend::handleNonVolatileWrite(const PacketPtr pkt, bool isClwb /* = false */) {
    Addr cachelineAddr = cacheline_align(pkt->req->getVaddr());
    if (isClwb) {
        this->clwbCountInclInv++;
        this->addrChangesBwClwb.sample(this->addrChangesSinceClwb);
        this->addrChangesSinceClwb = 0;
        if (this->cacheLineAccumulator.find(cachelineAddr) != this->cacheLineAccumulator.end()
                and not this->cacheLineAccumulator.at(cachelineAddr).all_invalid() ) {
            this->clwbCount++;
            DPRINTF(PredictorFrontendLogic, 
                    URED "Found a clwb -> %s (%s, isClwb = %d)" RST "\n", 
                    pkt->print(), pkt->req->isCacheClean(), pkt->req->isCLWB());

            processLastAccLine(cachelineAddr);

            /* Clear the accumulator for future PM writes after processing the last one */
            DPRINTF(CacheLineAccumulator,
                    " Deleting cacheline accumulator entry with address = %p\n", 
                    (void*)cachelineAddr);
            
            this->invChunkCountSampler.sample(
                this->cacheLineAccumulator.at(cachelineAddr).invalid_chunk_count()
            );

            this->cacheLineAccumulator.erase(cachelineAddr);
            panic_if_not(cacheLineAccumulator.find(cachelineAddr) == cacheLineAccumulator.end());
            /* FIXME: Change the flow of thie function */
            return;
        } else {
            /* This clwb does nothing, return */
            DPRINTF(PredictorFrontendLogic, 
                    "Cacheline accumulator was invalid when the clwb was found.\n");
            return;
        }
    } else {
        this->pmStores++;

        Addr_t alignedAddr = cacheline_align(pkt->req->getVaddr());

        if (lastAlignedPMAddr != alignedAddr) {
            lastAlignedPMAddr = alignedAddr;
            this->addrChangesSinceClwb++;
        }

        /* Handle all the free prediction stuff */
        this->cachelineAccumulatorRetireTick();

        /**
         * Add an entry to the cacheline accumulator only if the incoming request is not a clwb
         * Check if this new accesss would evict an existing line in the PC accumulator
        */
        this->manageCachelineAcc(pkt);

        assert(pkt->req->hasVaddr());
        using ChunkType = ChunkInfo::ChunkType;

        auto vAddr = [pkt](){ return pkt->req->getVaddr(); };
        PC_t pc = pkt->req->getPC();

        DPRINTF(PredictorFrontendLogic, 
                    "Handling non-volatile write for address %p\n", (void*)pkt->req->getPC());
        
        DPRINTF(PredictorFrontendLogic, 
                "Handling non-volatile write with vaddr = %p, getAddr = %p, size = %d, "
                "offset = %d, cachline# = %p, and PC = %p\n", 
                (void*)pkt->req->getVaddr(), (void*)pkt->getAddr(), pkt->req->getSize(), 
                get_cacheline_off(vAddr()), (void*)cacheline_align(vAddr()), 
                (void*)pkt->req->getPC());

        assert(pkt->req->hasVaddr());
        assert(pkt->req->hasPC());
        assert(pkt->hasData());

        Addr_t destAddr = cacheline_align(vAddr());
        size_t dataLen = pkt->getSize();
        DataChunk *dataChunks = pkt->getPtr<uint32_t>();

        // /* If the last address field is not set until yet (i.e. this is the first NV write), 
        //    set it to the current address*/
        // if (wasLastNVAddrInvalid) {
        //     DPRINTF(PredictorFrontendLogic, 
        //             "New address @ %p (old = %p)\n", 
        //             (void*)cacheline_align(vAddr()), this->lastNVWriteAddr);
        //     this->lastNVWriteAddr = destAddr;
        // }

        // bool isNVAddrRepeated = 
        //     (cacheline_align(this->lastNVWriteAddr) == cacheline_align(vAddr()));
        // DPRINTF(PredictorFrontendLogic, 
        //         "Last address = %p, vaddr = %p\n", 
        //         cacheline_align(this->lastNVWriteAddr), 
        //         cacheline_align(vAddr()));
        /* If this address is not repeated, process the write and clear the dataChunk values */
    //     if ( (not isNVAddrRepeated)/*  or this->cacheLineAccumulator.are_all_complete() */) {
    //             /*         if (not isNVAddrRepeated) {
    //                         std::cout << "\nAddress different" << std::endl;
    //                     } else if (this->cacheLineAccumulator.are_all_complete()) {
    //                         std::cout << "\nAll complete" << std::endl;
    //                     }
    //  */
    //         // std::cout << "Using cache line @" << (void*)this->lastNVWriteAddr << ": " << this->cacheLineAccumulator << std::endl;

    //         DPRINTF(PredictorFrontendLogic, "Unrepeated address found\n");
    //         /* Process the last accumulated cache line only if it was valid */
    //         if ( not wasLastNVAddrInvalid and not this->cacheLineAccumulator[cachelineAddr].all_invalid()) {
    //             // std::cout << "Last NV address was not invalid" << std::endl;
    //             DPRINTF(PredictorFrontendLogic,"Processing last accumulated cache line @ %p\n", (void*)vAddr());
    //             this->PMAddrChanges++;
    //             this->addrChangesSinceClwb++;
    //         } else {
    //             DPRINTF(PredictorFrontendLogic, "Unable to flush, allInvalid? %d\n", this->cacheLineAccumulator[cachelineAddr].all_invalid());
    //         }

    //         this->lastNVWriteAddr = destAddr;
    //         this->cacheLineAccumulator.clear();
    //     }

        /* Write data from the current write to the writes accumulator */
        Addr_t offset = get_cacheline_off(vAddr());

        size_t chunkIndex = offset/sizeof(DataChunk);

        for (int i = 0; i < dataLen/sizeof(DataChunk); i++) {
            size_t cacheChunkIndex = chunkIndex + i;

            assert(cacheChunkIndex < DATA_CHUNK_COUNT);
            ChunkInfo &chunk = this->cacheLineAccumulator.at(cachelineAddr)
                                    .get_datachunks()[cacheChunkIndex];
            chunk.set_chunk_type(ChunkType::DATA);
            chunk.set_data(dataChunks[i]);
            chunk.set_generating_pc(pc);
            chunk.set_completion(true);
            this->cacheLineAccumulator.at(cachelineAddr).set_time_of_last_update(curTick());
        }
    }

    DPRINTF(PredictorFrontendLogic, 
                "Handled non-volatile write with addr = %p, size = %d\n", 
                pkt->req->getVaddr(), pkt->req->getSize());
}

void
PredictorFrontend::collectPktStatistics(PacketPtr pkt) {
    bool isWrite = pkt->isWrite();
    bool isPM = is_vaddr_pm(pkt->req->getVaddr());
    bool isClwb = is_vaddr_clwb(pkt);

    if ((isWrite or isClwb) and isPM) {
        // std::cout << "PM write found" << std::endl;
        if (isClwb) {
            // std::cout << "PM clwb found" << std::endl;
            panic_if_not(cacheline_align(pkt->req->getVaddr()) == cacheline_align(pkt->req->getVaddr()));
            if (this->lastWriteTickToAddr.find(pkt->req->getVaddr()) == this->lastWriteTickToAddr.end()) {
                this->clwbToLastWriteDistance.sample(
                    (uint64_t)-1
                );
                clwbWithoutWrites++;
            } else {
                this->clwbToLastWriteDistance.sample(
                    (curTick() - this->lastWriteTickToAddr[pkt->req->getVaddr()])/1000 // kilo ticks
                );
                // std::cout << "Sampling CLWB to last write latency of " << ((float)curTick() - (float)this->lastWriteTickToAddr[pkt->req->getVaddr()])/1000.0/1000.0 << "us" << std::endl;
            }
        } else {
            this->lastWriteTickToAddr[cacheline_align(pkt->req->getVaddr())] = curTick();
            // std::cout << "Setting last tick for address " << (void*)cacheline_align(pkt->req->getVaddr()) << " to tick value = " << curTick() << std::endl;
            this->lastWritePCToAddr[cacheline_align(pkt->req->getVaddr())] = pkt->req->getPC();
            
            /* Handlde the  stistics for consecutive writes to a cachelines */
            if (this->lastPMAddr != cacheline_align(pkt->req->getVaddr())) {
                this->consecutiveWritesToCL.sample(writesSinceLastCL);
                this->writesSinceLastCL = 0;
                this->lastPMAddr = cacheline_align(pkt->req->getVaddr());
            } else {
                this->writesSinceLastCL++;
            }
        }
    }
}

void PredictorFrontend::markIHBEntriesAsUsed(std::unordered_map<size_t, bool> indices) {
    for (std::pair<size_t, bool> index : indices) {
        this->writeHistoryBuffer.get(index.first).use();
    }
}

void
PredictorFrontend::processLastAccLine(Addr addr) {
    DPRINTF(PredictorFrontendLogic, "Processing cacheline from accumulator with address %p\n", (void*)addr);
    panic_if_not(PredictorBackend::predictorEnabled);

    this->pmAccumulatorFlushes++;
    using pcQueueEntry_t = PCQueue::pcQueueEntry_t;
    panic_if(this->cacheLineAccumulator.at(addr).all_invalid(), 
            "All accumulated datachunks for the cacheline are invalid for adfdr %p\n",
            addr);

    this->printCachedLine(addr);
    this->addrPredictor.set_last_seen_addr(addr);

    if (this->cacheLineAccumulator.at(addr).all_zeros()) {
        this->zeroCachelines++;
    }
    
    Addr_t destAddr = addr;
    ChunkInfo *dataChunks = this->cacheLineAccumulator.at(addr).get_datachunks();

    bool addrPredFound = false, dataPredFound = false;

    std::deque<hash_t> hashQueue;
    
    PredictorTableEntry entryToInsert;
    Addr_t addrPC = 0;

    int whb_iter_cnt = 0;

    //! Disabling dump, renable if needed
    this->writeHistoryBuffer.dump();
    
    /* Scan the write history buffer */
    Tick lastTick = 0;

    bool set_2 = false;
    // std::cout << "Completion status of index = 2 -> " << entryToInsert.get_datachunks()[2].get_completion() << std::endl;
    
    /* For finding whb index that were used */
    std::unordered_map<size_t, bool> usedWHBIndices;
    
    for (auto &whb_iter : this->writeHistoryBuffer) {
        if ((
                    SharedArea::genPCConf.find(whb_iter->get_pc()) == SharedArea::genPCConf.end()
                    or SharedArea::genPCConf.at(whb_iter->get_pc())() >= 5 

                    /* If confidence is disabled, this condition is always true*/
                    or disablePerPCConfidence
                )
                /* Do not reuse write history buffer entries */
                and not whb_iter->is_used()) {
            
            panic_if(lastTick > whb_iter->get_gen_tick(), "WHB insertion order violation");
            lastTick = whb_iter->get_gen_tick();
            PC_t pc = whb_iter->get_pc();

            /* Find address prediction only if it has not been found until now */
            if (not addrPredFound) {
                int addrOffset = whb_iter->get_cacheline().get_addr_offset(destAddr);
                if (addrOffset != -1) { 
                    /* Sample the distance at which it was found */
                    this->pcCaptureDistance.sample(this->writeHistoryBuffer.get_size() - whb_iter_cnt);
                    
                    if (DTRACE(PredictorFrontendLogic)  ) {
                        std::stringstream ss;
                        ss << *whb_iter << std::endl;

                        DPRINTF(PredictorFrontendLogic,     
                                "\n[%d] Found a match for predicting the address"
                                " destination = %16p "
                                "at offset %2d of whb_entry "
                                "(pc=%16p, whb_index=%2d, dest_addr = %16p, "
                                "insert_T = %16p, gap %16d, is_used = %d) = %s", 
                                whb_iter_cnt, (void*)destAddr, addrOffset, 
                                (void*)whb_iter->get_pc(), whb_iter_cnt, 
                                (void*)whb_iter->destAddr_diag,  
                                (void*)whb_iter->insertionTick_diag, 
                                curTick() - whb_iter->insertionTick_diag,
                                whb_iter->is_used(), 
                                ss.str().c_str());
                    }

                    assert(pc != 0 && "Trying to set 0 as pc");
                    entryToInsert.set_addr_chunk(
                        ChunkInfo(pc, addrOffset-whb_iter->get_cacheline().find_first_valid_index(), true, ChunkInfo::ChunkType::ADDR));
                    auto srcDataChunks = whb_iter->get_cacheline().get_datachunks();

                    Addr_t destAddr = srcDataChunks[addrOffset].get_data() + (srcDataChunks[addrOffset+1].get_data()<<32UL);
                    entryToInsert.get_addr_chunk().set_target_addr(cacheline_align(destAddr));
                    entryToInsert.get_addr_chunk().set_gen_pc_in_tick(whb_iter->get_gen_tick());
                    entryToInsert.get_addr_chunk().set_generating_pc(pc);
                    // dprintf(3, "Adding pc = %p with tick %d\n", pc, curTick());
                    hashQueue.push_back(whb_iter->get_path_hash());
                    addrPC = whb_iter->get_pc();
                    addrPredFound = true;
                    usedWHBIndices[whb_iter_cnt] = true;
                }
            }

            // std::cout << "missingChunks" << std::endl;
            /* Iterate over all the data chunks of the accumulated cacheline to find entries in the current write history buffer iter */
            for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
                assert(set_2 == entryToInsert.get_datachunks()[2].get_completion());
                /* Process this block only if it is valid and not already found */
                if (dataChunks[i].is_valid() 
                        /* If this block is not already predicted */
                        and not entryToInsert.get_datachunks()[i].get_completion()) {
                    int dataOffset = whb_iter->get_cacheline().get_data_offset(dataChunks[i].get_data());   
                    bool useVal = dataOffset != -1;

                    if (useVal) { 
                        /* Found a match for predicting the data */
                        dataPredFound = true;

                        /* Sample the distance at which this block was found */
                        this->pcCaptureDistance.sample(this->writeHistoryBuffer.get_size() - whb_iter_cnt);
                        if (DTRACE(PredictorFrontendLogic) ) {
                            std::stringstream ss;
                            ss << "[" << std::setw(2) << i << "]Found a matching block, details: "
                               << " Offset: "           << std::setw(2)     << dataOffset
                               << " Position: "         << std::setw(2)     << i
                               << " data: "             << print_ptr(8)     << cacheLineAccumulator.at(addr).get_datachunks()[i].get_data()
                               << " found: "            << print_ptr(8)     << whb_iter->get_cacheline().get_datachunks()[dataOffset].get_data()
                               << " pc: "               << print_ptr(16)    << whb_iter->get_pc()
                               << " address: "          << print_ptr(16)    << destAddr
                               << " whb_iter_cnt = "    << std::dec         << whb_iter_cnt
                               << " whb_id = "          << std::dec         << whb_iter->get_id()
                               << " size = "            << std::dec         << whb_iter->get_size()
                               << std::endl;
                            DPRINTF(PredictorFrontendLogic, ss.str().c_str());
                        }

                        auto &destDataChunk = entryToInsert.get_datachunks()[i];

                        destDataChunk.set_chunk_type(ChunkInfo::ChunkType::DATA);
                        destDataChunk.set_completion(true);
                        if (i == 2) {
                            set_2 = true;
                        }

                        auto cacheline = whb_iter->get_cacheline();
                        auto offset = dataOffset 
                                    - cacheline.find_first_valid_index();
                        
                        panic_if(offset >= whb_iter->get_size(),
                                 "Offset calculation error, offset = %d, size = %d",
                                 dataOffset, whb_iter->get_size());

                        destDataChunk.set_generating_pc(pc);
                        destDataChunk.set_gen_pc_in_tick(whb_iter->get_gen_tick());
                        destDataChunk.set_owner_key(destAddr);
                        destDataChunk.set_data_field_offset(offset);
                        
                        destDataChunk.set_data(dataChunks[i].get_data());

                        assert(entryToInsert.get_datachunks()[i].is_valid());
                        
                        usedWHBIndices[whb_iter_cnt] = true;

                        /**
                         * ! Use for PC generation only if the value of the soruce is non-zero 
                         * */
                        if (dataChunks[i].get_data() != 0) { //! Fix this
                            hashQueue.push_back(whb_iter->get_path_hash());
                        }
                    }
                } else {
                    if (dataChunks[i].is_invalid()) {
                        entryToInsert.get_datachunks()[i].set_chunk_type(ChunkInfo::ChunkType::INVALID);
                    }
                }

                /* Update the matching PC list */
                if (dataChunks[i].is_valid()) {
                    int dataOffset = whb_iter->get_cacheline().get_data_offset(dataChunks[i].get_data());   
                    bool useVal = dataOffset != -1;
                    if (useVal) { 
                        //! Diagnostics only:
                        //! Adding matching pc has a significant overhead, enable only if needed
                        #ifdef DIAGNOSTICS_MATCHING_PC
                            std::cout << "Incoming: Setting matching pcs " << (void*)pc << std::endl;
                            entryToInsert.get_datachunks()[i].set_matching_pcs();

                            entryToInsert.get_datachunks()[i].add_matching_pc(pc);
                        #endif // DIAGNOSTICS_MATCHING_PC
                    }
                }
            } // Data chunk iterator
        } else {
            DPRINTF(PredictorFrontendLogic, "Skipping pc %p\n", (void*)whb_iter->get_pc());
        } // Check for confidence
        whb_iter_cnt++;
    } // whb iterator

    size_t missingChunks = 0;
    // std::cout << "missingChunks" << missingChunks << std::endl;

    std::vector<DataChunk> unfoundData;

    /* Check if all the data chunks were found */
    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        if (dataChunks[i].is_valid() 
                and entryToInsert.get_datachunks()[i].is_invalid()) {
            dataPredFound = false;
            unfoundData.push_back(dataChunks[i].get_data());
            missingChunks++;
        }
    }


    bool foundAnything = addrPredFound and dataPredFound;

    if (not addrPredFound and not dataPredFound) {
        this->bothAddrDataNotFound++;
    }

    missingChunkCount.sample(missingChunks);

    if (not foundAnything) {
        this->cacheLineNotInWHB++;
        // std::cout << "[Not found] Unable to find cacheline " 
        //           << this->cacheLineAccumulator.at(destAddr) 
        //           << std::endl;
        if (not addrPredFound) {
            // std::cout << "Reason: Address prediction not found for addr " 
            //           << print_ptr(16) 
            //           << destAddr 
            //           << std::endl;
        }
        if (not dataPredFound) {
            // std::cout << "Reason: Datsa prediction not found for " << vec2hexStr(unfoundData) << std::endl;
        }
    }

    if (not addrPredFound) {
        this->noAddrFound++;
    } else {
        this->whbAddrFound++;
    }

    if (addrPredFound and not dataPredFound) {
        /* Address only prediction */
        entryToInsert.set_addr_only_pred();
    }

    if (foundAnything) {
        panic_if(not dataPredFound, "No data prediction found   .");
        
        /*  Diagnostics information */
        entryToInsert.destAddr_diag = cacheline_align(destAddr);
        entryToInsert.set_original_cacheline(this->cacheLineAccumulator.at(destAddr));
        this->pWritesFoundInWHB++;
        panic_if(hashQueue.size() == 0, "Queue size cannot be zero");
        this->addToPredictorTable(hashQueue.at(0), entryToInsert);        
    }

    this->markIHBEntriesAsUsed(usedWHBIndices);
}

void
PredictorFrontend::addToPredictorTable(hash_t hash, PredictorTableEntry &entryToInsert) {
    panic_if(ChunkInfo::valid_count(entryToInsert.get_datachunks(), 
            DATA_CHUNK_COUNT) == 0, "All data chunks are invalid");

    panic_if_not(entryToInsert.has_orig_cacheline());
    
    entryToInsert.set_hash(hash);

    // DPRINTF(PredictorFrontendLogic, "Adding entry with hash %s : %s\n", 
    //         hash.c_str(), entryToInsert.gen_pc_as_cl().to_string().c_str());
    
    this->predictorTable.add(entryToInsert);
    this->predictorTable.dump();
}

void
PredictorFrontend::handleConstPredictions(CompletedWriteEntry &completedWrite) {
    hash_t hash = completedWrite.get_generator_hash();
    if (SharedArea::constPredTracker.find(hash) != SharedArea::constPredTracker.end()) {
        for (auto offset : SharedArea::constPredTracker.at(hash)) {
            if (offset.second.timesFound > 0 
                    and completedWrite.get_orig_cacheline().get_datachunks()[offset.first].is_valid()) {
                DataChunk constData = completedWrite.get_orig_cacheline().get_datachunks()[offset.first].get_data();
                //! Choose between keeping the orignal value or last seen value
                // completedWrite.get_cacheline().get_datachunks()[offset.first].set_data(constData);
                std::unordered_map<size_t, SharedArea::constChunkLocator> map = SharedArea::constPredTracker.at(hash);
                completedWrite.get_cacheline().get_datachunks()[offset.first].set_chunk_type(ChunkInfo::ChunkType::DATA);
                completedWrite.get_cacheline().get_datachunks()[offset.first].set_data(
                    map.at(offset.first).lastData
                );
                completedWrite.get_cacheline().get_datachunks()[offset.first].set_constant_pred();
                DPRINTF(PredictorFrontendLogic, "Setting constant value of prediction of address %p at offset %d to value %p, triggered by counter value = %d\n", completedWrite.get_addr(), offset.first, constData, offset.second.timesFound);
            }
        }
    }
}

void
PredictorFrontend::sendWritesToBackend(std::deque<PendingTableEntryParent*> &completedEntries) {
    /* Add the completed entry to  the pending table */
    for (auto predictedWrite : completedEntries) {
        CompletedWriteEntry entryToInsert = CompletedWriteEntry(
            predictedWrite->addr.get_target_addr(), 
            predictedWrite->cacheline, 
            predictedWrite->get_generator_hash()
        );
        entryToInsert.set_orig_cacheline(predictedWrite->get_original_cacheline());
        // entryToInsert.set_ihb_pattern_match_index(predictedWrite->get_ihb_pattern_index());

        /* Set the time of the write before sending it to the backend and the 
           memory controller */
        entryToInsert.set_time_of_addr_gen(
            predictedWrite->addr.get_time_of_gen()
        );
        entryToInsert.set_time_of_data_gen(
            predictedWrite->cacheline.get_time_of_gen()
        );
        entryToInsert.set_time_of_creation(
            curTick()
        );

        this->handleConstPredictions(entryToInsert);

        this->predictedWriteCount++;   
        std::stringstream ss;
        Addr_t paddr = -1;
	    std::cout << "Trying to translate " << (void*)predictedWrite->addr.get_target_addr() << std::endl;
        EmulationPageTable::pageTableStaticObj->translate(predictedWrite->addr.get_target_addr(), paddr);

        std::stringstream hash;
        hash << predictedWrite->get_generator_hash();

        ss << YEL << curTick() << " Predicting writie: VADDR: " 
           << print_ptr(16) << predictedWrite->addr.get_target_addr()
           << " PADDR: " << print_ptr(16) << paddr
           << " PCSig: " << hash.str()
           << " " << predictedWrite->cacheline
           << RST 
           << std::endl;
        DPRINTF(PredictorFrontendLogic, "%s", ss.str().c_str());

        
        /* Send the write */
        // panic_if_not(predictedWrite->has_addr());
        panic_if_not(entryToInsert.has_addr());
        PredictorBackend::addCompletedWrite(entryToInsert);
    }
    while (!completedEntries.size()) {       
        // delete completedEntries.front();
        completedEntries.pop_front();    
    }
}


void
PredictorFrontend::updatePendingTable(const PacketPtr pkt) {
    assert(pkt->req->hasVaddr());
    assert(pkt->req->hasPC());

    PC_t pc = pkt->req->getPC();
    Addr_t addr = pkt->req->getVaddr();
    DataChunk *pktData = pkt->getPtr<DataChunk>();

    size_t len = pkt->req->getSize();
    size_t chunkCount = len/sizeof(DataChunk);

    int offset = get_cacheline_off(addr);
    size_t chunkOffset = offset/sizeof(DataChunk);

    std::deque<PendingTableEntryParent*> predictedWrites;
    if (this->pendingTable.has_pc_waiting(pc)) {
        std::stringstream ss;
        ss << "Incoming write: len = " << len << " offset = " << offset << " data = " << std::endl;
        for (int i = 0; i < chunkCount; i++) {
            ss << std::hex << "0x" << std::setw(sizeof(DataChunk)*2) << std::setfill('0') << pktData[i] << " ";
        }
        DPRINTF(PredictorFrontend, "%s", ss.str().c_str());
        
        /* Address should be cacheline aligned since the data in dataChunks[] is 
           already written to correct positions */
        Addr_t cachelineAddr = cacheline_align(pkt->getAddr());
        CacheLine tempCacheLine;

        /* Set the data values of the cacheline */
        for (int i =  chunkOffset; i < chunkCount + chunkOffset; i++) {
            tempCacheLine.get_datachunks()[i].set_chunk_type(ChunkInfo::ChunkType::DATA);
            tempCacheLine.get_datachunks()[i].set_data(pktData[i-chunkOffset]);
        }
        predictedWrites = this->pendingTable.update_entry_state(pc, pktData, chunkCount);
        // std::cout << "Found " << predictedWrites.size() << " writes" << std::endl;
    }

    bool completedWritesToSend = not predictedWrites.empty();
    if (completedWritesToSend) {
        // std::cout << "Sending write to backend" << std::endl;
        if (this->addrPredictor.can_pred_addr() and not disableFancyAddrPred) {
            for (auto predictedWrite : predictedWrites) {
                Addr_t originalAddr = predictedWrite->addr.get_target_addr();                
                predictedWrite->addr.set_target_addr(
                    this->addrPredictor.predict_addr()
                );

                // std::cout << HBLU "Changed the address from " 
                //           << (void*)originalAddr
                //           << " to " 
                //           << (void*)predictedWrite->addr.get_target_addr() 
                //           << std::endl;

                // std::cout << this->addrPredictor.state_to_string(true) << std::endl;
            }
        }
        
        this->sendWritesToBackend(predictedWrites);
        for (auto write : predictedWrites) {
            genHash << write->get_generator_hash() << std::endl;
        }
    }
}

#define PRINT_DATA                                                                                              \
    if (DTRACE(PredictorFrontendLogic) ) {                                                                      \
        typedef uint32_t data_t;                                                                                \
            data_t *data = pkt->getPtr<data_t>();                                                               \
            std::cout << YEL << "⌚ = " << curTick() << " "                                                     \
                      << YEL << print_ptr(16) << pkt->req->getVaddr()                                           \
                      << " (%" << cacheline_align(pkt->req->getVaddr()) << ")"                                  \
                      << RED " [PC=" << print_ptr(16) << pkt->req->getPC() << "]" << RST << " >> " << std::hex; \
            for (int i = 0; i < pkt->getSize()/sizeof(data_t); i++) {                                           \
                std::cout << " 0x" << print_ptr(16) << data[i];                                                 \
            }                                                                                                   \
            std::cout << std::dec << std::endl;                                                                 \
    }


void
PredictorFrontend::handleWrite(const PacketPtr pkt) {
    Addr_t addr = pkt->req->getVaddr();
    
    bool isClwb = is_vaddr_clwb(pkt);
    bool isPktWrite = pkt->isWrite();
    size_t writeSize = pkt->getSize();

    if (is_vaddr_pm(addr)) {
        if (isClwb) {
            if (writebackDistMap.find(cacheline_align(addr)) != writebackDistMap.end()) {
                writebackDistStat.sample(
                    (curTick()-writebackDistMap[cacheline_align(addr)])/1000
                );
                writebackDistStatMicro.sample(
                    (curTick()-writebackDistMap[cacheline_align(addr)])/1000
                );
            } else {
                // writebackDistStat.sample(
                //     0//UINT64_MAX
                // );
            }
        } else {
            writebackDistMap[cacheline_align(addr)] = curTick();
        }
    }
    
    if (is_vaddr_pm(addr)) {
        bool hasData = pkt->hasData();

        if (isClwb or (hasData and isPktWrite)) {
            PRINT_DATA
            handleNonVolatileWrite(pkt, isClwb);
        }
    }
    if ((is_vaddr_volatile(addr) or is_vaddr_pm(addr)) and not isClwb) {
        if (this->predictorTable.isPCInPCFilter(pkt->req->getPC())) {
            // std::cout << __FUNCTION__ << " found a PC of interest" << std::endl;
        }
        // std::cout << "Handling volatile write " << (void*)pkt->req->getVaddr() << std::endl; 
        assert(pkt->req->hasPC());
        assert(pkt->req->hasPC());
        PC_t pc = pkt->req->getPC();
        assert(pkt->req->hasPC());

        const Addr_t addr = pkt->req->getVaddr();
        const auto offset = get_cacheline_off(addr)/sizeof(DataChunk);
        const auto count = pkt->getSize()/sizeof(DataChunk);
        const auto cachelineAddr = cacheline_align(addr);
        const DataChunk *pktData = pkt->getPtr<DataChunk>();

        /* Add the PCs for the matching entry from the predictor table to pending table */
        if (this->predictorTable.update_ihb(pkt)) { /* Prediction triggered */
            this->predictorTablePromotions++;
            // std::cout << "Last found hashes count = " 
            //           << this->predictorTable.get_last_found_hashes().size() 
            //           << std::endl;
            for (hash_t hash : this->predictorTable.get_last_found_hashes()) {
                // std::cout << CYN "Found a match for triggering prediction with hash " << hash << RST << std::endl; 
                PredictorTableEntry completeEntry 
                        = this->predictorTable.get_with_hash(hash);

                /* Check if atleast one of the data chunk is valid */
                bool allInvalid = true;
                std::for_each(
                    completeEntry.get_datachunks(), 
                    completeEntry.get_datachunks() + DATA_CHUNK_COUNT,
                    [&allInvalid] (const ChunkInfo &chunkInfo) { 
                        allInvalid = allInvalid and chunkInfo.is_invalid();
                    }
                );
                
                panic_if(allInvalid, 
                        "All data chunks in the completed entry from predictor table "
                        "are invalid.");

                DataChunk dataChunks[DATA_CHUNK_COUNT];

                for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
                    if (completeEntry.get_datachunks()[i].is_valid()) {
                        dataChunks[i] = completeEntry.get_datachunks()[i].get_data();
                    }
                }

                PendingTableEntryParent *parent = new PendingTableEntryParent();

                // std::cout << "Creating parent with address: " << parent << std::endl;
                /* Address should always be valid */

                for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
                    if (completeEntry.get_datachunks()[i].is_valid()) {
                        parent->cacheline.get_datachunks()[i].set_chunk_type(ChunkInfo::ChunkType::DATA);
                        parent->cacheline.get_datachunks()[i].set_generating_pc(
                            completeEntry.get_datachunks()[i].get_generating_pc()
                        );
                    }
                }

//! Diagnostics only:
//! Adding matching pc has a significant overhead, enable only if needed
#ifdef DIAGNOSTICS_MATCHING_PC
                    /* Set the matching PCs */
                    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
                        if (completeEntry.get_datachunks()[i].has_matching_pcs()) {
                            parent->cacheline.get_datachunks()[i].set_matching_pcs();
                            for (auto elem : completeEntry.get_datachunks()[i].get_matching_pc()) {
                                parent->cacheline.get_datachunks()[i].add_matching_pc(elem);
                            }
                        }
                    }
#endif // DIAGNOSTICS_MATCHING_PC

                /* Set the generator pc used for bookkeeping prediction confidences */
                parent->set_generator_hash(completeEntry.get_hash());

                parent->set_original_cacheline(completeEntry.get_original_cacheline());

                /* Insert the addr and data to the pending table with their parent */
                PendTableChunkInfo addrChunk = PendTableChunkInfo(completeEntry.get_addr_chunk(), parent, 0);
                assert(addrChunk.has_data_field_offset());
                // std::cerr << "Adding stuff to pending table" << std::endl;
                this->pendingTable.add(addrChunk);
                for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
                    /* Constant zero prediction */
                    if (completeEntry.get_datachunks()[i].is_valid() and completeEntry.get_datachunks()[i].is_const_0_pred()) {
                        // std::cout << "Predicting constant 0" << std::endl;
                        parent->cacheline.get_datachunks()[i].set_time_of_gen(curTick());
                        parent->cacheline.get_datachunks()[i].set_chunk_type(ChunkInfo::ChunkType::DATA);
                        parent->cacheline.get_datachunks()[i].set_data(0);
                        parent->cacheline.get_datachunks()[i].set_generating_pc(-1);
                        this->constant0Prediction++;
                    } else if (completeEntry.get_datachunks()[i].is_valid()) {
                        PendTableChunkInfo dataChunk(completeEntry.get_datachunks()[i], parent, i);
                        dataChunk.set_chunk_type(ChunkInfo::ChunkType::DATA);
                        assert(dataChunk.has_data_field_offset());
                        assert(dataChunk.get_generating_pc() != 0);
                        this->pendingTable.add(dataChunk);
                    } else {
                        // std::cout << "Not adding for index = " << i << std::endl;
                    }
                }

                // TODO: Handle memory leak here
                /* Set the value of the data chunks using the two IHB entries */

            }
        }

        /* Update the pending table with this write */
        this->updatePendingTable(pkt);
        PendingTableEntryParent *completedParent = this->pendingTable.get_completed_parent(pc);
        
        if (completedParent != nullptr) {
        
            DPRINTF(PredictorFrontendLogic, "Found a completed parent! pc = %p\n", pc);
        }
    } 
    if (not is_vaddr_pm(addr) and not is_vaddr_volatile(addr)) {
        panic("Unable to map address %p to either volatile or non-volatile memory.", (void*)addr);
    }
}

void
PredictorFrontend::dumpTrace(PacketPtr pkt) {
    if (enableVolatileDump != "") {
        bool isPktWrite = pkt->isWrite() and (pkt->getSize() == 8 or pkt->getSize() == 4);

        bool isClwb = pkt->req->isToPOC();
        size_t writeSize = pkt->getSize();
        auto addr =  pkt->req->getVaddr();

        bool useWriteForTrace = isPktWrite and (writeSize == 4 or writeSize == 8);

        if (useWriteForTrace or isClwb) {
            std::string locType = is_vaddr_pm(addr) ? "P" : "V";
            std::string opType = isClwb ? "C" : "W"; // CLWB and write operations
            std::string data = "";

            /* Convert the data to hex string of appropriate size */
            std::stringstream ss;
            if (writeSize == 4) {
                ss << print_ptr(8) << *pkt->getPtr<uint32_t>();
            } else if (writeSize == 8) {
                ss << print_ptr(16) << *pkt->getPtr<uint64_t>();
            } else {
                ss << "0x0";
            }
            data = ss.str();

            myFile << "@" << print_ptr(16) << pkt->req->getPC()     // Write PC
                << " " << locType                                   // Address type
                << " " << opType                                    // C for clwb and W for write
                << " " << print_ptr(16) << pkt->req->getVaddr()     // write address
                << " " << print_ptr(1) << writeSize                 // write size in hex
                << " " << data                                      // data
                << " " << print_ptr(16) << curTick()                // Tick
                << std::endl;
        }       
    }
}

void
PredictorFrontend::predictorHandleRequest(const PacketPtr pkt) {
    this->dumpTrace(pkt);

    //! SUYASH
    if (PredictorBackend::predictorEnabled == false or PredictorBackend::usePredictor == false) {
        return;
    }

    this->collectPktStatistics(pkt);
    auto addr =  pkt->req->getVaddr();

    bool isPktWrite = pkt->isWrite() and (pkt->getSize() == 8 or pkt->getSize() == 4);
    PC_t tgtPC = pkt->req->getPC();
    bool isPCInPCFilter = this->predictorTable.isPCInPCFilter(tgtPC);
    if (std::find(addrOfInterest.begin(), addrOfInterest.end(), pkt->req->getPC()) != addrOfInterest.end()) {
        DPRINTF(PredictorFrontendLogic, "Found PC of interest [%p] at tick = %d"
                ", in PCFilter? = %d, isPktWrite? = %d: {%s}", 
                (void*)tgtPC, curTick(), isPCInPCFilter, isPktWrite, 
                pkt->print());
        PRINT_DATA;
        std::cout << "isPCIn " << isPCInPCFilter << std::endl;
    }

    bool isClwb = is_vaddr_clwb(pkt);
    size_t writeSize = pkt->getSize();
    
    /* update the writeHistoryBuffer */
    if (this->canAddToWhb(pkt)) {
        this->updateWriteHistoryBuffer(pkt);
    }
 
    if (isPCInPCFilter) {
        std::cout << __LINE__ << " Handling write for PC of interest" << std::endl;
    }

    /* Handles all the logic associated with the write requests */
    if (isPktWrite or isClwb){ 
        if (isPCInPCFilter) {
            // std::cout << "Handling write for PC of interest" << std::endl;
        }
        handleWrite(pkt);
    }

    this->avgPendingTableSz = this->pendingTable.get_size();
    this->avgPredictorTableSz = this->predictorTable.get_size();
}

#undef PRINT_DATA

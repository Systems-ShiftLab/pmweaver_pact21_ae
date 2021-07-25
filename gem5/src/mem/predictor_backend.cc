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
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
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



#include "base/trace.hh"
#include "debug/ConstantPrediction.hh"
#include "debug/PredictorBackendInterface.hh"
#include "debug/PredictorConfidence.hh"
#include "debug/PredictorResult.hh"
#include "helper_suyash.h"
#include "mem/dram_ctrl.hh"
#include "mem/predictor/Common.hh"
#include "mem/predictor/Constants.hh"
#include "mem/predictor/Declarations.hh"
#include "mem/predictor/SharedArea.hh"
#include "mem/predictor_backend.hh"
#include "params/PredictorBackend.hh"
#include "mem/cache/cache.hh"
#include <type_traits>

#define P_WRITE_VADDR_PADDR_COMP_MASK (0b111111111111)

PredictorBackend::PBSlavePort::PBSlavePort(const std::string& _name,
                                         PredictorBackend& _pb,
                                         PBMasterPort& _masterPort,
                                         Cycles _delay, int _resp_limit,
                                         std::vector<AddrRange> _ranges)
    : SlavePort(_name, &_pb), pb(_pb), masterPort(_masterPort),
      delay(_delay), ranges(_ranges.begin(), _ranges.end()),
      outstandingResponses(0), retryReq(false), respQueueLimit(_resp_limit),
      sendEvent([this]{ trySendTiming(); }, _name)
{
}

PredictorBackend::PBMasterPort::PBMasterPort(const std::string& _name,
                                           PredictorBackend& _pb,
                                           PBSlavePort& _slavePort,
                                           Cycles _delay, int _req_limit)
    : MasterPort(_name, &_pb), pb(_pb), slavePort(_slavePort),
      delay(_delay), reqQueueLimit(_req_limit),
      sendEvent([this]{ trySendTiming(); }, _name)
{
}

PredictorBackend::PredictorBackend(Params *p)
    : ClockedObject(p),
      slavePort(p->name + ".slave", *this, masterPort,
                ticksToCycles(p->delay), p->resp_size, p->ranges),
      masterPort(p->name + ".master", *this, slavePort,
                 ticksToCycles(p->delay), p->req_size)
{
    
        auto parentName = p->name;
        correctlyPredictedChunks
            .name(parentName + ".correctlyPredictedChunks")
            .desc(".correctlyPredictedChunks");
        uniqPCSigCount
            .name(parentName + ".uniqPCSigCount")
            .desc("Number of unique PC signatures that were actually useful");
        totalPWrites
            .name(parentName + ".totalPWrites")
            .desc("The total number of persistent writes seen by the backend, is "
                    "equals to correctlyPredictedPWrites + incorrectlyPredictedPWrites "
                    "+ addrMissPredictedPWrites + dataMissPredictedPWrites");
        correctlyPredictedPWrites
            .name(parentName + ".correctlyPredictedPWrites")
            .desc("The total number of correctly predicted writes by the predictor "
                    "frontend.");
        addrMissPredictedPWrites
            .name(parentName + ".addrMissPredictedPWrites")
            .desc("The total number of predicted writes that had their address predicted "
                    "incorrectly.");
        dataMissPredictedPWrites
            .name(parentName + ".dataMissPredictedPWrites")
            .desc("The total number of predicted writes that had one or more of their data "
                    "chunks predicted incorrectly.");
        incorrectlyPredictedPWrites
            .name(parentName + ".incorrectlyPredictedPWrites")
            .desc("The total number of correctly predicted writes by the predictor "
                    "frontend, is equals to addrMissPredictedPWrites + dataMissPredictedPWrites");
        nonPredictedPWrites
            .name(parentName + ".nonPredictedPWrites")
            .desc("The total number of writes that went unpredicted by the predictor frontend.");
        avgDataMatchForAddrMatch
            .name(parentName + ".avgDataMatchForAddrMatch")
            .desc("The average number of predicted data chunks that match the actual write on every PM write.");
        pmWriteMatchDistance
            .init(0, 100000, 100)
            .name(parentName + ".pmWriteMatchDistance")
            .desc("Distance between the prediction of a write and the actual write event, measured in ticks")
            .flags(Stats::pdf);
        addrPmWriteMatchDistance
            .init(0, 100000, 100)
            .name(parentName + ".addrPmWriteMatchDistance")
            .desc("Distance between the prediction of a write and the actual write event, measured in ticks")
            .flags(Stats::pdf);
        correctConst0Pred
            .name(parentName + ".correctConst0Pred")
            .desc("Number of data chunk correctly predicted using constant 0 prediction.");
        resultBufferCapacityEvictions
            .name(parentName + ".resultBufferCapacityEvictions")
            .desc("Capacity eviction of the result buffer (per address).");
        addrMatchDist
            .name(parentName + ".get_time_of_addr_gen")
            .desc("aksdjnklfjnabskldfnlkakd jlkfad jjlkf aslkdfj lkads jlkadsj fdkjjah l")
            .init(0, 2000, 20000/100);
        ihbPatternMatchIndex
            .name(parentName + ".ihbPatternMatchIndex")
            .desc("ihbPatternMatchIndex")
            .init(0, 10, 1);
        dataMatchDist
            .name(parentName + ".get_time_of_data_gen")
            .desc("aksdjnklfjnabskldfnlkakd jlkfad jjlkf aslkdfj lkads jlkadsj fdkjjah l")
            .init(0, 2000, 20000/100);
        addrPredictions
            .name(parentName + ".addrPredictions")
            .desc("adf");
        invalidatedPredictions
            .name(parentName + ".invalidatedPredictions")
            .desc("Number of generated predictions invalidated in the backend.");
        correctlyPredictedFreeWrites
            .name(parentName + ".correctlyPredictedFreeWrites")
            .desc("correctlyPredictedFreeWrites");
        correctlyPredNonFreeWr
            .name(parentName + ".correctlyPredNonFreeWr")
            .desc("correctlyPredNonFreeWr");
        validChunks
            .name(parentName + ".validChunks")
            .desc(".validChunks");    
        writebackDistStat
            .name(p->name + ".writebackDistStat")
            .desc("writebackDistStat")
            .init(0, 10000000, 100);
        writebackDistStatMicro
            .name(p->name + ".writebackDistStatMicro")
            .desc("writebackDistStat")
            .init(0, 1000, 1);

        usePredictor = get_env_val("USE_PREDICTOR");

        char* envResult = std::getenv("ENABLE_NON_VOLATILE_DUMP");
    
        if (envResult != nullptr) {
            this->enableNonVolatileDump = std::string(envResult);
        } else {
            enableNonVolatileDump = "0";
        }
        myFile.open("/ramdisk/nonvolatiledump.txt");
        hashStats.open("./hash.stats");

        std::cout << "Can't believe it's running!" << std::endl;
        std::cerr << "usePredictor = " << usePredictor << std::endl;

        SharedArea::init_size_multiplier();
        PredictorBackend::RESULT_BUFFER_MAX_SIZE *= SharedArea::sizeMultiplier;
        PredictorBackend::MAX_COMPLETED_QUEUE_LINE_SIZE *= SharedArea::sizeMultiplier;

	if (PredictorBackend::MAX_COMPLETED_QUEUE_LINE_SIZE < 1) {
	    PredictorBackend::MAX_COMPLETED_QUEUE_LINE_SIZE = 1;
	}
        
        std::cout << "Using result buffer max size = " << PredictorBackend::RESULT_BUFFER_MAX_SIZE << std::endl;

}

PredictorBackend::~PredictorBackend() {
    this->resultBufferCapacityEvictions = PredictorBackend::capacityEvictionStatic;
}

Port &
PredictorBackend::getPort(const std::string &if_name, PortID idx)
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
PredictorBackend::init()
{
    // make sure both sides are connected and have the same block size
    if (!slavePort.isConnected() || !masterPort.isConnected())
        fatal("Both ports of a pb must be connected.\n");

    // notify the master side  of our address ranges
    slavePort.sendRangeChange();
}

bool
PredictorBackend::PBSlavePort::respQueueFull() const
{
    return outstandingResponses == respQueueLimit;
}

bool
PredictorBackend::PBMasterPort::reqQueueFull() const
{
    return transmitList.size() == reqQueueLimit;
}

bool
PredictorBackend::PBMasterPort::recvTimingResp(PacketPtr pkt)
{
    // all checks are done when the request is accepted on the slave
    // side, so we are guaranteed to have space for the response
    DPRINTF(PredictorBackendInterface, "recvTimingResp: %s addr 0x%x\n",
            pkt->cmdString(), pkt->getAddr());

    DPRINTF(PredictorBackendInterface, "Request queue size: %d\n", transmitList.size());

    // technically the packet only reaches us after the header delay,
    // and typically we also need to deserialise any payload (unless
    // the two sides of the bridge are synchronous)
    Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
    pkt->headerDelay = pkt->payloadDelay = 0;

    slavePort.schedTimingResp(pkt, pb.clockEdge(delay) +
                              receive_delay);

    return true;
}

bool
PredictorBackend::PBSlavePort::recvTimingReq(PacketPtr pkt)
{
    if (pkt->isCounterPacket or pkt->isVerificationCacheEviction or pkt->isCounterCacheEviction or pkt->isVerificationPacket) {
        return false;
    }


    pb.resultBufferCapacityEvictions = PredictorBackend::capacityEvictionStatic;
    /* Handle the request in the predictor backend */
    pb.predictorHandleRequest(pkt);

    DPRINTF(PredictorBackendInterface, "recvTimingReq: %s addr 0x%x\n",
            pkt->cmdString(), pkt->getAddr());


    if (pkt->cacheResponding()) {
        printf("Packet: %s\n", pkt->print());
    }
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    // we should not get a new request after committing to retry the
    // current one, but unfortunately the CPU violates this rule, so
    // simply ignore it for now
    if (retryReq)
        return false;

    DPRINTF(PredictorBackendInterface, "Response queue size: %d outresp: %d\n",
            transmitList.size(), outstandingResponses);

    // if the request queue is full then there is no hope
    if (masterPort.reqQueueFull()) {
        DPRINTF(PredictorBackendInterface, "Request queue full\n");
        retryReq = true;
    } else {
        // look at the response queue if we expect to see a response
        bool expects_response = pkt->needsResponse();
        if (expects_response) {
            if (respQueueFull()) {
                DPRINTF(PredictorBackendInterface, "Response queue full\n");
                retryReq = true;
            } else {
                // ok to send the request with space for the response
                DPRINTF(PredictorBackendInterface, "Reserving space for response\n");
                assert(outstandingResponses != respQueueLimit);
                ++outstandingResponses;

                // no need to set retryReq to false as this is already the
                // case
            }
        }

        if (!retryReq) {
            // technically the packet only reaches us after the header
            // delay, and typically we also need to deserialise any
            // payload (unless the two sides of the pb are
            // synchronous)
            Tick receive_delay = pkt->headerDelay + pkt->payloadDelay;
            pkt->headerDelay = pkt->payloadDelay = 0;

            masterPort.schedTimingReq(pkt, pb.clockEdge(delay) +
                                      receive_delay);
        }
    }

    // remember that we are now stalling a packet and that we have to
    // tell the sending master to retry once space becomes available,
    // we make no distinction whether the stalling is due to the
    // request queue or response queue being full
    return !retryReq;
}

void
PredictorBackend::PBSlavePort::retryStalledReq()
{
    if (retryReq) {
        DPRINTF(PredictorBackendInterface, "Request waiting for retry, now retrying\n");
        retryReq = false;
        sendRetryReq();
    }
}

void
PredictorBackend::PBMasterPort::schedTimingReq(PacketPtr pkt, Tick when)
{
    // If we're about to put this packet at the head of the queue, we
    // need to schedule an event to do the transmit.  Otherwise there
    // should already be an event scheduled for sending the head
    // packet.
    if (transmitList.empty()) {
        pb.schedule(sendEvent, when);
    }

    assert(transmitList.size() != reqQueueLimit);

    transmitList.emplace_back(pkt, when);
}


void
PredictorBackend::PBSlavePort::schedTimingResp(PacketPtr pkt, Tick when)
{
    // If we're about to put this packet at the head of the queue, we
    // need to schedule an event to do the transmit.  Otherwise there
    // should already be an event scheduled for sending the head
    // packet.
    if (transmitList.empty()) {
        pb.schedule(sendEvent, when);
    }

    transmitList.emplace_back(pkt, when);
}

void
PredictorBackend::PBMasterPort::trySendTiming()
{
    assert(!transmitList.empty());

    DeferredPacket req = transmitList.front();

    assert(req.tick <= curTick());

    PacketPtr pkt = req.pkt;

    DPRINTF(PredictorBackendInterface, "trySend request addr 0x%x, queue size %d\n",
            pkt->getAddr(), transmitList.size());

    if (sendTimingReq(pkt)) {
        // send successful
        transmitList.pop_front();
        DPRINTF(PredictorBackendInterface, "trySend request successful\n");

        // If there are more packets to send, schedule event to try again.
        if (!transmitList.empty()) {
            DeferredPacket next_req = transmitList.front();
            DPRINTF(PredictorBackendInterface, "Scheduling next send\n");
            pb.schedule(sendEvent, std::max(next_req.tick,
                                                pb.clockEdge()));
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
PredictorBackend::PBSlavePort::trySendTiming()
{
    assert(!transmitList.empty());

    DeferredPacket resp = transmitList.front();

    assert(resp.tick <= curTick());

    PacketPtr pkt = resp.pkt;

    DPRINTF(PredictorBackendInterface, "trySend response addr 0x%x, outstanding %d\n",
            pkt->getAddr(), outstandingResponses);

    if (sendTimingResp(pkt)) {
        // send successful
        transmitList.pop_front();
        DPRINTF(PredictorBackendInterface, "trySend response successful\n");

        assert(outstandingResponses != 0);
        --outstandingResponses;

        // If there are more packets to send, schedule event to try again.
        if (!transmitList.empty()) {
            DeferredPacket next_resp = transmitList.front();
            DPRINTF(PredictorBackendInterface, "Scheduling next send\n");
            pb.schedule(sendEvent, std::max(next_resp.tick,
                                                pb.clockEdge()));
        }

        // if there is space in the request queue and we were stalling
        // a request, it will definitely be possible to accept it now
        // since there is guaranteed space in the response queue
        if (!masterPort.reqQueueFull() && retryReq) {
            DPRINTF(PredictorBackendInterface, "Request waiting for retry, now retrying\n");
            retryReq = false;
            sendRetryReq();
        }
    }

    // if the send failed, then we try again once we receive a retry,
    // and therefore there is no need to take any action
}

void
PredictorBackend::PBMasterPort::recvReqRetry()
{
    trySendTiming();
}

void
PredictorBackend::PBSlavePort::recvRespRetry()
{
    trySendTiming();
}

Tick
PredictorBackend::PBSlavePort::recvAtomic(PacketPtr pkt)
{
    
    if (pkt->isCounterPacket or pkt->isVerificationCacheEviction or pkt->isCounterCacheEviction or pkt->isVerificationPacket) {
        return delay * pb.clockPeriod() + masterPort.sendAtomic(pkt);
    }

    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    return delay * pb.clockPeriod() + masterPort.sendAtomic(pkt);
}

void
PredictorBackend::PBSlavePort::recvFunctional(PacketPtr pkt)
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
PredictorBackend::PBMasterPort::trySatisfyFunctional(PacketPtr pkt)
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
PredictorBackend::PBSlavePort::getAddrRanges() const
{
    return ranges;
}

PredictorBackend *
PredictorBackendParams::create()
{
    return new PredictorBackend(this);
}

Addr
PredictorBackend::getCompletedAddrToEvict() {
    panic_if(completedWritesManager.empty(), "Queue empty, 🤔 ");

    std::pair<Addr, Tick> oldest = *completedWritesManager.cbegin();
    for (auto entry : completedWritesManager) {
        if (oldest.second > entry.second) {
            oldest.first = entry.first;     // Address
            oldest.second = entry.second;   // Tick
        }
    }
    return oldest.first;
}

void 
PredictorBackend::addCompletedWrite(CompletedWriteEntry entry) {
    
    // std::cout << "Got a write" << std::endl;
    /* Insert the entry if the predcition has a legal address and the predictor is enabled */
    // std::cout << GRN << "Got a prediction for address " << print_ptr(16) << entry.get_addr() << RST << std::endl;
    bool canTranslate = EmulationPageTable::pageTableStaticObj->lookup(entry.get_addr()) != nullptr;
    if (usePredictor and canTranslate) {
        // std::cout << "Got a write and works" << std::endl;
        /* Cache hits for the meta data caches are set here while the actual access is done from the DRAMCtrl */
        Addr addr = entry.get_addr(), paddr = 0;
        EmulationPageTable::pageTableStaticObj->translate(addr, paddr);
        PredictorBackend::addrMatches[paddr]++;
        // paddr = getCompWriteKey(entry.get_addr());
        // std::cout << "Trying to insert addresss = " << print_ptr(16) << paddr << std::endl;
        // std::cout << "Changing address from " << (void*)entry.get_addr() << " to " << (void*)paddr << std::endl;
        entry.set_addr(paddr);
        bool isCounterCacheHit = DRAMCtrl::isCounterCacheHit(paddr);
        entry.set_counter_cache_hit(isCounterCacheHit);
        bool isVerificationCacheHit = DRAMCtrl::isVerificationCacheHit(paddr);
        entry.set_verification_cache_hit(isVerificationCacheHit);


        DRAMCtrl::pendingPredictionQueue.push_back(entry);

        // DPRINTFR(PredictorBackendLogic, "Inserting new prediction for address %p and cacheline %s\n", paddr, entry.get_cacheline().to_string());
	completedWrites[paddr].push_back(entry);

        /* Insert the entry to the manager to find the LRU order for eviction */
        // completedWritesManager[paddr] = curTick();

        /* Line has exceeded its capactiy */
        if (completedWrites.at(paddr).size() >= PredictorBackend::MAX_COMPLETED_QUEUE_LINE_SIZE + 1) {
            auto entryToEvict = completedWrites[paddr].front();

            std::stringstream evictCl;
            evictCl << completedWrites[paddr].front().get_cacheline();
            // DPRINTFR(PredictorBackendLogic, "Capacity evicting cacheline with data = %s\n", evictCl.str());

            completedWrites[paddr].pop_front();

            /* send feedback */
            PredictorBackend::broadcastPrediction(entryToEvict.get_generator_hash(), false, false);
            PredictorBackend::capacityEvictionStatic++;
            std::cout << "Evicting (╯°□°）╯︵ ┻━┻" << std::endl;
        }

        // /* The table has exceeded it's capacity */
        // if (completedWrites.size() == PredictorBackend::RESULT_BUFFER_MAX_SIZE) {
        //     /* Find the oldest entry */
        //     Addr oldestEntry = PredictorBackend::getCompletedAddrToEvict();
        //     panic_if(oldestEntry == 0, "Something went wrong");

        //     /* Delete the entry from the table */
        //     PredictorBackend::completedWrites.erase(oldestEntry);

        //     /* Update the entry from the manager */
        //     PredictorBackend::completedWritesManager.erase(oldestEntry);
        // }
        
        /* Remove all the empty queues from the completed writes table */
        std::deque<Addr> addrToRemove;
        for (auto entry : completedWrites) {
            if (entry.second.empty()) {
                // std::cout << "Removing entry at " << entry.first << std::endl;
                addrToRemove.push_back(entry.first);
            }
        }

        for (auto addr : addrToRemove) {
            panic_if_not(PredictorBackend::completedWrites.at(addrToRemove.front()).empty());
            PredictorBackend::completedWrites.erase(addrToRemove.front());
            addrToRemove.pop_front();
        }

        // printf("Write manager size is not equal to the completed write table, "
        //      "completedWritesManger.size() = %d, completedWrites.size() = %d\n", 
        //      completedWritesManager.size(), completedWrites.size());

        // if (completedWritesManager.size() != completedWrites.size()) {
        //     std::cout << "Printing manager" << std::endl;
        //     for (auto entry :  completedWritesManager) {
        //         std::cout << entry.first << std::endl;
        //     }

        //     std::cout << "Pringin completed writes" << std::endl;
        //     for (auto entry :  completedWrites) {
        //         std::cout << entry.first << std::endl;
        //     }
        // }

        // panic_if(completedWritesManager.size() > PredictorBackend::RESULT_BUFFER_MAX_SIZE,  "Write manager entry count exceeded expectation.");
        // panic_if(completedWritesManager.size() != completedWrites.size(),                   "Write manager size is not equal to the completed write table, completedWritesManger.size() = %d, completedWrites.size() = %d", completedWritesManager.size(), completedWrites.size());
        // panic_if(completedWrites.size() > RESULT_BUFFER_MAX_SIZE,                           "Result buffer size exceeds maximum size");
        panic_if(completedWrites.at(paddr).size() > MAX_COMPLETED_QUEUE_LINE_SIZE, "Inconsistent size");
	
	/* Calculate the total number of entries and perform random eviction
	   of one of the single entry row*/
	uint64_t totalEntries = 0;
	uint64_t freeEntries = 0;
	std::cout << "Current table Size = ";
	for (auto compWriteQ : PredictorBackend::completedWrites) {
	    totalEntries += compWriteQ.second.size();
	    // std::cout << compWriteQ.second.size() << " age = " << double(curTick()-compWriteQ.second.front().get_time_of_addr_gen()) << ", ";
	    
	    for (auto write : compWriteQ.second) {
            if (write.get_cacheline().get_datachunks()[0].is_free_prediction()) {
                freeEntries++;
            }
	    }
	    
	}
	PredictorBackend::resultBufferSize << " <-> " << totalEntries << std::endl;
	PredictorBackend::resultBufferSize << " <-f-> " << freeEntries << std::endl;
	
	uint64_t oldestAddr = 0;
	uint64_t oldestTick = UINT64_MAX;
	uint64_t oldestIndex = 0;
	if (totalEntries > RESULT_BUFFER_MAX_SIZE) {
	    for (auto compWriteQ : PredictorBackend::completedWrites) {
		uint64_t curIndex = 0;
		for (auto write : compWriteQ.second) {

		    if (25000000 < (curTick() - write.get_time_of_creation())
                    and write.get_cacheline().get_datachunks()[0].is_free_prediction()) {

                oldestTick = write.get_time_of_creation();
                oldestAddr = compWriteQ.first;
                oldestIndex = curIndex;
		    }


		    if (3*10000000 < (curTick() - write.get_time_of_creation())
                    and not write.get_cacheline().get_datachunks()[0].is_free_prediction()) {

                oldestTick = write.get_time_of_creation();
                oldestAddr = compWriteQ.first;
                oldestIndex = curIndex;
		    }

		    curIndex++;
		}
	    }
	}
	if (oldestAddr != 0) {
	    std::cout << "Removed entry at address " << (void*)oldestAddr
		      << (curTick() - oldestTick) << std::endl;
	    PredictorBackend::completedWrites.erase(oldestAddr);
        PredictorBackend::capacityEvictionStatic++;
	}
	
	// std::vector<uint64_t> indexSingleEntr;
	// if (PredictorBackend::completedWrites.size() > RESULT_BUFFER_MAX_SIZE) {
	//     /* Find the oldest entry to delete */
	//     for (auto compWriteQ : PredictorBackend::completedWrites) {
	// 	if (compWriteQ.second.size() == 1) {
	// 	    indexSingleEntr.push_back(compWriteQ.first);
	// 	}
	//     }
	// }
	// if (indexSingleEntr.size() > 0) {
        //     panic_if(PredictorBackend::completedWrites.find(indexSingleEntr.back())
	// 	     == PredictorBackend::completedWrites.end(), "Entry should exist");
        //     PredictorBackend::completedWrites.erase(indexSingleEntr.back());
	//     std::cout << "Removed entry at " << indexSingleEntr.back() << std::endl;
	// }	    
    }
}

bool
PredictorBackend::isPktEqualCompletedEntryAddr(PacketPtr pkt, CompletedWriteEntry completedEntry) {
    bool result;
    
    /* Addresses should already be cache line aligned */
    Addr_t completedAddr = completedEntry.get_addr();
    Addr_t pktAddr = pkt->req->getPaddr();

    // SM: No need for this, translation is done at insertion
    // Addr_t completedPAddr = 0;
    // EmulationPageTable::pageTableStaticObj->translate(completedAddr, completedPAddr);

    result = (completedAddr == pktAddr);

    return result;
}

size_t
PredictorBackend::getMatchingChunkCount(PacketPtr pkt, CompletedWriteEntry completedEntry) {
    size_t result = 0;

    DataChunk *dataChunks = pkt->getPtr<DataChunk>();
    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        bool is_valid = completedEntry.get_cacheline().get_datachunks()[i].is_valid();
        if (is_valid 
            and dataChunks[i] != completedEntry.get_cacheline().get_datachunks()[i].get_data()) {
            result++;
        }
    }
    return result;  
}

bool
PredictorBackend::isPktEqualCompletedEntryData(PacketPtr pkt, CompletedWriteEntry completedEntry) {
    bool result = true;

    DataChunk *dataChunks = pkt->getPtr<DataChunk>();
    auto entryDataChunks = completedEntry.get_cacheline().get_datachunks();
    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        /* pkt obtained by eviction of a cached eviction or write back should have all 
           its block valid. */
        /* Match the data only if the chunk is valid */
        if (entryDataChunks[i].is_valid()
                and dataChunks[i] != entryDataChunks[i].get_data()) {
            result = false;
            break;
        }
    }

    if (completedEntry.get_cacheline().all_invalid()) {
        // std::cout << "All invalid" << std::endl;
        result = false;
    }

    return result;
}

bool
PredictorBackend::isPktEqualCompletedEntry(PacketPtr pkt, CompletedWriteEntry completedEntry) {
    panic_if(pkt->getSize() != CACHELINE_SIZE, 
                "Write at the predictro backend should be "
                "cacheline size, is the backedn connected correctly?");

    bool result = isPktEqualCompletedEntryAddr(pkt, completedEntry)
                        and isPktEqualCompletedEntryData(pkt, completedEntry);
    return result;
}

void 
PredictorBackend::broadcastPrediction(hash_t hash, bool addrPredicted, bool dataPredicted) {
    // printf("Broadcasting signature <%p, %p> to %p\n", addrPredicted, dataPredicted, &SharedArea::correctPredictions);
    if (addrPredicted) {
        SharedArea::correctPredictions[hash].first++;
    }

    if (dataPredicted) {
        SharedArea::correctPredictions[hash].second++;
    }
}

void 
PredictorBackend::update_stats_for_const_pred(CompletedWriteEntry completedEntry) {
    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        if (completedEntry.get_cacheline().get_datachunks()[i].is_valid()
                and completedEntry.get_cacheline().get_datachunks()[i].is_const_0_pred()) {
            this->correctConst0Pred++;
        }
    }
}


std::bitset<DATA_CHUNK_COUNT>
PredictorBackend::dataChunkMatchVec(CompletedWriteEntry completedEntry, PacketPtr pkt) {
    std::bitset<DATA_CHUNK_COUNT> result;

    DataChunk *dataChunks = pkt->getPtr<DataChunk>();
    auto entryDataChunks = completedEntry.get_cacheline().get_datachunks();
    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        /* pkt obtained by eviction of a cached eviction or write back should have all 
           its block valid. */
        /* Match the data only if the chunk is valid */
        if (entryDataChunks[i].is_valid()
                and dataChunks[i] == entryDataChunks[i].get_data()) {
            result.set(i);
        }
    }

    return result;
}

std::bitset<DATA_CHUNK_COUNT>
PredictorBackend::dataChunkConstVec(CompletedWriteEntry completedEntry) {
    std::bitset<DATA_CHUNK_COUNT> result;

    auto entryDataChunks = completedEntry.get_cacheline().get_datachunks();
    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        if (entryDataChunks[i].is_valid() 
                and entryDataChunks[i].is_constant_pred()) {
            result.set(i);
        }
    }

    return result;
}

void 
PredictorBackend::updatePCConf(PacketPtr pkt, CompletedWriteEntry completedEntry) {
    DPRINTFR(PredictorBackendLogic, RED"==== Updating confidence for incoming packet ===="RST"\n");
    int result = 16;

    DataChunk *dataChunks = pkt->getPtr<DataChunk>();
    auto entryDataChunks = completedEntry.get_cacheline().get_datachunks();
    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        /* pkt obtained by eviction of a cached eviction or write back should have all 
           its block valid. */
        /* Match the data only if the chunk is valid */
        if (entryDataChunks[i].is_valid() and not entryDataChunks[i].is_free_prediction()) {
            PC_t targetPC = entryDataChunks[i].get_generating_pc();
            bool exists = true;
            if (SharedArea::genPCConf.find(targetPC) == SharedArea::genPCConf.end()) {
                SharedArea::genPCConf.insert(
                    std::make_pair(targetPC, Confidence(6, 7, 0))
                );
                DPRINTFR(PredictorBackendLogic, "%lu Initialized confidence for PC %p, new value = %d\n", curTick(), (void*)targetPC, SharedArea::genPCConf.at(targetPC)());
                exists = false;
            } 
            if (dataChunks[i] != entryDataChunks[i].get_data()) {
                SharedArea::genPCConf.at(targetPC).sub(1);
                DPRINTFR(PredictorBackendLogic, "%d %d %lu Reducing confidence for PC %p (generated %p, expected %p), new value = %d\n", 
                        SharedArea::genPCConf.size(), exists, curTick(), 
                        (void*)targetPC, entryDataChunks[i].get_data(), dataChunks[i], SharedArea::genPCConf.at(targetPC)());
            } else {
                // SharedArea::genPCConf.at(targetPC).add(1);
                // DPRINTFR(PredictorBackendLogic, "%d %d %lu Increasing confidence for PC %p, new value = %d\n", SharedArea::genPCConf.size(), exists, curTick(),  (void*)targetPC, SharedArea::genPCConf.at(targetPC)());
            }
        }
    }
}

void
PredictorBackend::invalidateAllAddr() {
    if (not get_env_val("DISABLE_INVALIDATION")) {
        printf("Invalidating all addresses @%lld\n", curTick());
        for (auto entry : this->completedWrites) {
            Addr paddr = entry.first;
            auto &completedWritesForAddr_q = this->completedWrites[paddr];
            auto completedWrite_iter = completedWritesForAddr_q.rbegin();
            while (completedWrite_iter != completedWritesForAddr_q.rend()) {
                // std::cout << "correct counter = " << correctCounter++ << std::endl;
                auto completedEntry = *completedWrite_iter;
                if (not completedWrite_iter->is_used()) {
                    completedWrite_iter->set_time_of_addr_gen(curTick());
                }
                completedWrite_iter++;
            }

            if (not this->completedWrites[paddr].empty()) {
                invalidatedPredictions++;
            }
        }
    }
}

void 
PredictorBackend::updateConstChunks(hash_t maxDataMatchHash, Addr_t addr, PacketPtr pkt) {
    DPRINTF(ConstantPrediction,     
            "[Const] Checking constant prediction for addr = %p\n", addr);
    std::deque<CompletedWriteEntry> completedWritesForAddr = completedWrites.at(addr);

    CompletedWriteEntry targetCompletedWrite;
    bool writeFound = false;
    for (auto completedWrite : completedWritesForAddr) {
        if (completedWrite.get_generator_hash() == maxDataMatchHash) {
            targetCompletedWrite = completedWrite;
            writeFound = true;
            break;
        }
    }

    if (writeFound) {
        std::bitset<DATA_CHUNK_COUNT> validVec;
        // std::bitset<DATA_CHUNK_COUNT> pktEqualOrig;
        std::bitset<DATA_CHUNK_COUNT> pktNoEqualPred;
        for (int offset = 0; offset < DATA_CHUNK_COUNT; offset++) {
            if (targetCompletedWrite.get_cacheline().get_datachunks()[offset].is_valid() 
                    and targetCompletedWrite.get_orig_cacheline().get_datachunks()[offset].is_valid()) {
                /* Stat collection */
                if (targetCompletedWrite.get_cacheline().get_datachunks()[offset].is_valid()) {
                    validVec.set(offset);
                }
                // if (targetCompletedWrite.get_orig_cacheline().get_datachunks()[offset].get_data() == pkt->getPtr<DataChunk>()[offset]) {
                //     pktEqualOrig.set(offset);
                // }
                if (targetCompletedWrite.get_cacheline().get_datachunks()[offset].get_data() != pkt->getPtr<DataChunk>()[offset]) {
                    pktNoEqualPred.set(offset);
                }
                
                /* Constant prediction */
                if (targetCompletedWrite.get_cacheline().get_datachunks()[offset].get_data() != pkt->getPtr<DataChunk>()[offset]) {

                    if (SharedArea::constPredTracker.find(maxDataMatchHash) == SharedArea::constPredTracker.end()) {
                        SharedArea::constPredTracker[maxDataMatchHash][offset];
                    }
                    if (SharedArea::constPredTracker.at(maxDataMatchHash).find(addr) 
                            == SharedArea::constPredTracker.at(maxDataMatchHash).end()) {
                        SharedArea::constPredTracker.at(maxDataMatchHash)[offset];
                    }

                    /* Increment the match count only if the last data of this block is same as the current data */
                    if (SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).lastData == pkt->getPtr<DataChunk>()[offset]) {
                        DPRINTF(ConstantPrediction, 
                                "[Const] Incrementing constant value tracker for"
                                "with offset = %d\n", offset);
                        SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).constOffset = offset;
                        SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).timesFound += 1;
                        if (SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).timesFound == 10) {
                            SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).timesFound = 10;
                        }
                    } else {
                        DPRINTF(ConstantPrediction, "[Const] Decrementing constant value tracker with offset = %d (last = %p, current = %p)\n", 
                                    offset, 
                                    SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).lastData,
                                    pkt->getPtr<DataChunk>()[offset]);
                        if (SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).timesFound > 0) {
                            SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).timesFound -= 1;
                        }
                        SharedArea::constPredTracker.at(maxDataMatchHash).at(offset).lastData = pkt->getPtr<DataChunk>()[offset];
                    }
                }
            }
        }
    } else {
        DPRINTF(ConstantPrediction, "[Const] Unable to find any completed write\n");
    }
}

static Addr_t getCompWriteKey(Addr_t addr);

#define PRINT_DATA                                                                                              \
    if (DTRACE(PredictorBackendLogic) ) {                                                                       \
        typedef uint32_t data_t;                                                                                \
            data_t *data = pkt->getPtr<data_t>();                                                               \
            std::cout << YEL << "⌚ = " << curTick() << " "                                                     \
                      << YEL << print_ptr(16) << pkt->req->getPaddr()                                           \
                      << RST << " >> " << std::hex;                                                             \
            for (int i = 0; i < pkt->getSize()/sizeof(data_t); i++) {                                           \
                std::cout << " 0x" << print_ptr(8) << data[i];                                                  \
            }                                                                                                   \
            std::cout << std::dec << std::endl;                                                                 \
    }

void 
PredictorBackend::handleNonVolatileWrite(PacketPtr pkt) {
    this->validChunks += CacheLine(pkt, false).valid_chunk_count();
    DPRINTF(PredictorBackendLogic, "Found a non-volatile write with addr = %p\n", (void*)pkt->getAddr());
    
    // printf("Found a P Write in the backend: ");
    PRINT_DATA

    Addr_t paddrCL = pkt->req->getPaddr();
    // printf("Corresponding line in cache: %s\n", CacheLine(paddrCL, Cache::l2CacheStaticObj).to_string().c_str());

    Addr_t paddrCL1 = pkt->req->getPaddr();
    // printf("Corresponding line in L1 cache: %s\n", CacheLine(paddrCL1, Cache::l1DCacheStaticObj).to_string().c_str());

    this->totalPWrites++;

    Addr_t paddr = pkt->req->getPaddr();
    std::stringstream predStr;

    if (addrMatches.find(pkt->req->getPaddr()) != addrMatches.end() and addrMatches.find(pkt->req->getPaddr())->second > 0) {
        addrPredictions++;
        DPRINTF(PredictorResult, GRN "Prediction for address %p found" RST "\n", pkt->getAddr());
    } else {
        DPRINTF(PredictorResult, RED "No prediction for address %p found" RST "\n", pkt->getAddr());
    }
    if (this->completedWrites.find(paddr) == this->completedWrites.end()) {
        if (DTRACE(PredictorResult)) {
            predStr << "======= Not Predicted " << std::endl;
            predStr << "For addr = " << (void*)pkt->req->getPaddr() << std::endl;


            predStr << CYN;
            predStr << "[" << print_ptr(16) << paddr << "] " << "Unpredicted: " << CacheLine(pkt->req->getPaddr(), pkt->getPtr<DataChunk>(), pkt->getSize()/sizeof(DataChunk), true) << std::endl;
            predStr << RST;
            std::cout << predStr.str();
            predStr.clear();
        }
        /* Not predicted */
        this->nonPredictedPWrites++;    

        // Suyash: Invalidate all addresses in the result buffer on non prediction (address missing)
        this->invalidateAllAddr();

        if (DTRACE(PredictorBackendLogic)) {
            std::stringstream ss;
            ss << "Packet addr = " << (void*)pkt->getAddr()  << " not found,  <vaddr, paddr> = <" 
            << std::hex
            << (pkt->req->hasVaddr() ? "0d" + std::to_string(pkt->req->getVaddr()) : "INVALID" )
            << ", " 
            << (pkt->req->hasPaddr() ? "0d" + std::to_string(pkt->req->getPaddr()) : "INVALID" )
            << ">, completed entry size = " 
            << PredictorBackend::completedWrites.size() 
            << std::dec
            << ", request size ="
            << pkt->getSize()
            << " last 12 bits:"
            << paddr
            << std::endl;
            DPRINTF(PredictorBackendLogic, "%s", ss.str().c_str());
        }
    } else {
        /* Was predicted */
        bool addrExists = this->completedWrites.find(paddr) != this->completedWrites.end();

        auto emptyQueue = std::deque<CompletedWriteEntry>();
        auto &completedWritesForAddr_q = /* addrExists ?  */this->completedWrites[paddr]/*  : emptyQueue */;
        int  i = 0;

        size_t indexToDelete = -1;
        int maxDataMatchCount = 0;
        std::bitset<DATA_CHUNK_COUNT> theoreticalMatchVector;
        hash_t maxDataMatchHash;

        /**
         *! WARN: On reversing the order of this iterator, change the corresponding 
         *! offset calculation after this loop
        */

        /* Searchn for the non-free write predictions to upodate the statistics */
        size_t nonFreeCounter = 0;
        for (auto write_iter = completedWritesForAddr_q.rbegin(); write_iter != completedWritesForAddr_q.rend(); write_iter++) {
            nonFreeCounter++;
            if (isPktEqualCompletedEntry(pkt, *write_iter) 
                    and not write_iter->get_cacheline().get_datachunks()[0].is_free_prediction()
                    and not write_iter->is_used()) {
                this->correctlyPredNonFreeWr++;
                // std::cerr << "&& " << write_iter->get_cacheline() << std::endl;
                // std::cerr << "&& " << CacheLine(pkt, false) << std::endl;
                break;
            }
        }
        // std::cout << "Loop terminated at non free coutner value of " << nonFreeCounter << std::endl;
        
        size_t correctCounter = 0;
        auto completedWrite_iter = completedWritesForAddr_q.rbegin();
        bool sampled = false;
        while (completedWrite_iter != completedWritesForAddr_q.rend()) {
            // std::cout << "correct counter = " << correctCounter++ << std::endl;
            auto completedEntry = *completedWrite_iter;
            hash_t confKey = completedWrite_iter->get_generator_hash();
            this->initConf(confKey);
            // std::cerr << "is used? " << (completedEntry.is_used() ? "true" : "false") << std::endl;

            if (not completedEntry.is_used()) {
                this->addrPmWriteMatchDistance.sample((curTick() - completedEntry.get_least_time_of_gen())/1000);
                sampled = true;
            }
            
            if (isPktEqualCompletedEntryAddr(pkt,  completedEntry) 
                    and not completedEntry.is_used()) {
                avgDataMatchForAddrMatch += getMatchingChunkCount(pkt, completedEntry);
                predStr << GRN "======= Predicted " RST << "\n";
                // predStr << "For addr = " << (void*)pkt->req->getPaddr() << std::endl;
                if (isPktEqualCompletedEntry(pkt, completedEntry)) {
                    correctlyPredictedPWrites++;
                    // std::cout << "Emulating: " << "Correctly predcited address andd data" << std::endl;
                    if (completedEntry.get_cacheline().get_datachunks()[0].is_free_prediction()) {
                        correctlyPredictedFreeWrites++;
                    } else {
                        hashStats << completedEntry.get_generator_hash() << std::endl;
                        this->broadcastPrediction(
                            completedEntry.get_generator_hash(), true, true);
                    }

                    this->pmWriteMatchDistance.sample((curTick() - completedEntry.get_least_time_of_gen())/1000);
                    this->dataMatchDist.sample((curTick() - completedEntry.get_time_of_data_gen())/1000);
                    // std::cout << "Updating hash = " << confKey << std::endl;
                    // std::cout << "Old value = " << confidenceTable[confKey] << std::endl;
                    confidenceTable[confKey] = confidenceTable[confKey] == PRED_CONFIDENCE_MAX 
                                             ? PRED_CONFIDENCE_MAX 
                                             : confidenceTable[confKey]+1;
                    // std::cout << "New value = " << confidenceTable[confKey] << std::endl;

                    if (DTRACE(PredictorResult)) {
                        predStr << GRN << "Write Addr = " << (void*)pkt->getAddr() << std::endl
                                  << "Got:         " 
                                  << completedEntry.get_cacheline() << std::endl
                                  << "Expecting:   " 
                                  << CacheLine(pkt->req->getPaddr(), 
                                               pkt->getPtr<DataChunk>(), 
                                               pkt->getSize()/sizeof(DataChunk), 
                                               true) 
                                  << std::endl
                                  << RST;
                        std::cout << predStr.str();
                        predStr.clear();
                    }

                    this->update_stats_for_const_pred(completedEntry);
                    indexToDelete = i;
                    break;
                } else {
                    // SM: Don't send negative feedback here
                    // this->broadcastPrediction(
                    //     completedEntry.get_generator_pc_sig(), true, false);

                    this->updatePCConf(pkt, completedEntry);

                    incorrectlyPredictedPWrites++;
                    std::stringstream pcSig("");

                    std::bitset<DATA_CHUNK_COUNT> matchVec = dataChunkMatchVec(completedEntry, pkt);
                    std::bitset<DATA_CHUNK_COUNT> constantVec = dataChunkConstVec(completedEntry);

                    size_t matchCount = matchVec.count();
                    theoreticalMatchVector |= matchVec;

                    if (DTRACE(PredictorResult)) {
                        std::cout << RED << "Write Addr = " << (void*)pkt->getAddr() << std::endl;

                        CacheLine llcCacheLine = CacheLine(paddr, Cache::l2CacheStaticObj);

                        std::cout << "[" << print_ptr(16) << paddr << "] " << "LLC:         "  << std::setfill(' ') << std::setw(pcSig.str().size()+1) << llcCacheLine << std::endl;
                        std::cout << "[" << print_ptr(16) << paddr << "] " << "Got:         " << pcSig.str() << completedEntry.get_cacheline() << std::endl;
                        std::cout << "[" << print_ptr(16) << paddr << "] " << "Expecting:  " << std::setfill(' ') << std::setw(pcSig.str().size()+1) << " " << CacheLine(pkt->req->getPaddr(), pkt->getPtr<DataChunk>(), pkt->getSize()/sizeof(DataChunk), true) << std::endl;
                        std::cout << "[" << print_ptr(16) << paddr << "] " << "Orignal:    " << std::setfill(' ') << std::setw(pcSig.str().size()+1) << " " << completedEntry.get_orig_cacheline() << std::endl;
                        std::cout << "Match count: " << matchVec.count() << std::endl;
                        std::cout << RST;
                    }
                    
                    // std::cout << "Match vec =    " << matchVec << std::endl;
                    // std::cout << "Constant vec = " << constantVec << std::endl;

                    if (maxDataMatchCount < matchCount) {
                        maxDataMatchCount = matchCount;
                        maxDataMatchHash = completedEntry.get_generator_hash();
                    }

                    // confidenceTable[confKey] = confidenceTable[confKey] == 0 
                    //                          ? 0 
                    //                          : confidenceTable[confKey]-1;
                }

            } else {
                DPRINTF(PredictorResult, "Unable to match (completedEntry.addr = %p, pkt.addr = %p), moving on\n", completedEntry.get_addr(), pkt->getAddr());
                DPRINTF(PredictorResult, "isPktEqualCompletedEntryAddr(pkt,  completedEntry) = %d and not completedEntry.is_used() = %d\n", isPktEqualCompletedEntryAddr(pkt,  completedEntry), completedEntry.is_used());
            }
            ++i;
            ++completedWrite_iter;
        }
        
        if (DTRACE(PredictorResult)) {
            std::string theoreticalMatchVector_str = theoreticalMatchVector.to_string();
            
            /* Reversing the string */
            std::reverse(theoreticalMatchVector_str.begin(), theoreticalMatchVector_str.end());

            predStr << ">>>> Max data match count = " << maxDataMatchCount << std::endl;
            predStr << ">>>> Theoretical max match count = " << std::setw(2) 
                    << theoreticalMatchVector.count() << ": " 
                    << theoreticalMatchVector_str
                    << " for aligned physical address " << (void*)pkt->req->getPaddr() 
                    << " hash = " << maxDataMatchHash
                    << std::endl;
            std::cout << predStr.str();
            predStr.clear();
            correctlyPredictedChunks += maxDataMatchCount;

            std::cout << "Setting value for pc" << std::endl;
            std::cout << "Setting the stat" << std::endl;
        }

        // if (maxDataMatchHash != 0) {
        //     SharedArea::uniquePCSig[maxDataMatchHash] = true;
        //     this->uniqPCSigCount = SharedArea::uniquePCSig.size();
        // }

        // std::cout << "[" << print_ptr(16) << paddr << "] " << "Incoming:  " << CacheLine(pkt->req->getPaddr(), pkt->getPtr<DataChunk>(), pkt->getSize()/sizeof(DataChunk), true) << std::endl;
        if (maxDataMatchHash != 0) {
            // std::cout << "Incoming: maxpc = " << vec2hexStr(maxDataMatchPC) << " with pc match vector = " << theoreticalMatchVector << std::endl;
            for (auto pcSig : this->completedWrites.at(paddr)) {
                // std::cout << "Incoming: trying << " << vec2hexStr(pcSig.get_generator_pc_sig()) << std::endl;
                const size_t OFFSET = 8;
                // std::cout << "Incoming: Comparing " 
                //           << vec2hexStr(pcSig.get_generator_pc_sig()) 
                //           << " and " 
                //           <<  vec2hexStr(maxDataMatchPC) 
                //           << " are equal? " 
                //           << (pcSig.get_generator_pc_sig() == maxDataMatchPC)
                //           << std::endl;
                if (pcSig.get_generator_hash() == maxDataMatchHash) {
                    // std::cout << "Incoming: Match found | has matching PCs? " << pcSig.get_cacheline().get_datachunks()[OFFSET].has_matching_pcs() << std::endl;
                    #ifdef DIAGNOSTICS_MATCHING_PC
                        if (pcSig.get_cacheline().get_datachunks()[OFFSET].has_matching_pcs()) {
                            // std::cout << "Incoming:      ";
                            // for (PC_t pc : pcSig.get_cacheline().get_datachunks()[OFFSET].get_matching_pc()) {
                            //     std::cout << " " << print_ptr(16) << pc;
                            // }
                            // std::cout << std::endl;
                            std::cout << "Incoming: (Got)                ";
                            std::cout << pcSig.get_cacheline() << std::endl;
                        }
                        break;
                    #endif // DIAGNOSTICS_MATCHING_PC
                }
            }
        }
        
        // std::cout << "Trying to update the constant pc values" << std::endl;
        this->updateConstChunks(maxDataMatchHash, pkt->req->getPaddr(), pkt);
        if (addrExists and not this->completedWrites.find(paddr)->second.empty()) {
            this->addrMatchDist.sample((curTick() - completedWrites.at(paddr).front().get_time_of_addr_gen())/1000);
        }

        if (indexToDelete != -1) {
            /**
             *! WARN: The index calculation here should always follow the same order as the iterator
             *!       that generates the variable `completeWrite_iter`
            */
            indexToDelete = completedWritesForAddr_q.size() - 1 - (completedWrite_iter - completedWritesForAddr_q.rbegin());
            std::cout << "indexToDelete: " << indexToDelete << std::endl;
            // predStr << "Deleting entry at index (match found): " << indexToDelete << std::endl;

            // Suyash: Don't erase this for now and just mark it as used
            completedWritesForAddr_q[indexToDelete].use();
            // completedWrites.at(paddr).erase( completedWritesForAddr_q.begin() + indexToDelete );

            // PredictorBackend::completedWritesManager.erase(addrKey);
        } else {
            /* No write predicted */
            dataMissPredictedPWrites++;
        }
    }
}

size_t 
PredictorBackend::getConfidenceForLoc(hash_t hash) {
    float result = -1;

    if (confidenceTable.find(hash) != confidenceTable.end()) {
        result = confidenceTable[hash];
    }

    // DPRINTFR(PredictorConfidence, 
    //          "Returning confidence value of %f for hash <%s>\n", 
    //          result, 
    //          hash.c_str());
    return result;
}

void
PredictorBackend::dumpTrace(PacketPtr pkt) {
    // std::cout << "Trying to dump pkt with size = " << pkt->getSize() << " isNonVolatileAddress = " << pkt->req->getPaddr() << std::endl;
    if (enableNonVolatileDump == "1") {
        bool isPktWrite = pkt->isWrite();

        bool isClwb = pkt->req->isToPOC();
        size_t writeSize = pkt->getSize();
        auto addr =  pkt->req->getPaddr();
        // std::cout << "Trying to dump pkt with size = " << pkt->getSize() << " isNonVolatileAddress = " << isAddrNonVolatile(addr) << std::endl;

        bool useWriteForTrace = isPktWrite 
                                and (writeSize == 64);

        if (useWriteForTrace or isClwb) {
            std::string locType = "P";
            std::string opType = writeSize == 1 ? "C" : "W"; // CLWB and write operations
            std::string data = "";

            /* Convert the data to hex string of appropriate size */
            std::stringstream ss;
            if (writeSize == 64) {
                ss << "0x0";
                for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
                    ss << std::hex << std::setw(8) << std::setfill('0') << pkt->getPtr<uint32_t>()[i];
                }
            } else if (writeSize == 1) {
                ss << "0x0";
                panic_if(not isClwb, "Non 64 byte cacheline eviction, size = %s", writeSize);
            } else {
                panic("Non 64 byte cacheline eviction, size = %s", writeSize);
            }

            data = ss.str();

            myFile << "@" << print_ptr(16) << 0x0                   // Write PC
                << " " << locType                                   // Address type
                << " " << opType                                    // C for clwb and W for write
                << " " << print_ptr(16) << pkt->req->getPaddr()     // write address
                << " " << print_ptr(1) << writeSize                 // write size in hex
                << " " << data                                      // data
                << " " << print_ptr(16) << curTick()                // Tick
                << std::endl;
        }       
    }
}

void 
PredictorBackend::predictorHandleRequest(PacketPtr pkt) {
    if (PredictorBackend::predictorEnabled == false or usePredictor == false) {
        return;
    }

    this->dumpTrace(pkt);
    bool isClwb = pkt->req->isToPOC() and pkt->req->getSize() == 1;
    Addr_t addr = pkt->getAddr();
    
    /* At this point the address should be cacheline aligned */
    // panic_if(addr != cacheline_align(addr), "Unaligned address (= %p) found", addr);

    if (is_paddr_pm(addr) and pkt->isWrite() and not isClwb) {
        if (is_paddr_pm(addr)) {
            handleNonVolatileWrite(pkt);
        }
    }


    if (is_paddr_pm(addr)) {
        if (isClwb) {
            if (writebackDistMap.find(cacheline_align(addr)) != writebackDistMap.end()) {
                writebackDistStat.sample(
                    (curTick()-writebackDistMap[cacheline_align(addr)])/1000
                );
                writebackDistStatMicro.sample(
                    (curTick()-writebackDistMap[cacheline_align(addr)])/1000
                );
            } else {
            //     writebackDistStat.sample(
            //         0//UINT64_MAX
            //     );
            }
        } else {
            writebackDistMap[cacheline_align(addr)] = curTick();
        }
    }

}

void PredictorBackend::initConf(hash_t hash)  {
    if (confidenceTable.find(hash) == confidenceTable.end()) {
        confidenceTable[hash] = PRED_CONFIDENCE_MAX-1;
        std::cout << "Initializing hash " << (void*)hash 
                  << " to value " << confidenceTable[hash] 
                  << std::endl;
    }
}

PredictorBackend::CompletedWrites_Q 
PredictorBackend::completedWrites = PredictorBackend::CompletedWrites_Q();

PredictorBackend::ConfTable_t
PredictorBackend::confidenceTable = PredictorBackend::ConfTable_t();

bool
PredictorBackend::predictorEnabled = false;

bool
PredictorBackend::usePredictor = false;

uint64_t
PredictorBackend::capacityEvictionStatic = 0;

std::unordered_map<Addr_t, Tick>
PredictorBackend::completedWritesManager;

/* Limits the maximum number of prediction entries for an address **/
int
PredictorBackend::MAX_COMPLETED_QUEUE_LINE_SIZE = // 8*
				     4;
/* Limits the total size of the result buffer table */
size_t
PredictorBackend::RESULT_BUFFER_MAX_SIZE = 256;
Addr_t 
PredictorBackend::getCompWriteKey(Addr_t addr) {
    return addr&P_WRITE_VADDR_PADDR_COMP_MASK;
}

std::unordered_map<PC_t, int>
PredictorBackend::addrMatches;

std::ofstream
PredictorBackend::resultBufferSize = std::ofstream("./resultBufferSize.dat");

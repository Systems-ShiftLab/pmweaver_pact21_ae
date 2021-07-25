
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>
#include <fstream>
/*
 * Copyright (c) 2012-2019 ARM Limited
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
 * Copyright (c) 2013 Amin Farmahini-Farahani
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
 * Authors: Andreas Hansson
 *          Ani Udipi
 *          Neha Agarwal
 *          Omar Naji
 *          Matthias Jung
 *          Wendy Elsasser
 *          Radhika Jagtap
 */

/**
 * @file
 * DRAMCtrl declaration
 */

#ifndef __MEM_DRAM_CTRL_HH__
#define __MEM_DRAM_CTRL_HH__

#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback.hh"
#include "base/statistics.hh"
#include "enums/AddrMap.hh"
#include "enums/MemSched.hh"
#include "enums/PageManage.hh"
#include "mem/drampower.hh"
#include "mem/predictor/CompletedWriteEntry.hh"
#include "mem/predictor/Common.hh"
#include "mem/qos/mem_ctrl.hh"
#include "mem/qport.hh"
#include "params/DRAMCtrl.hh"
#include "sim/eventq.hh"
#include "txopt/common.hh"
#include "txopt/param.hh"
#include "debug/BMO.hh"
#include "mem/page_table.hh"



enum opt_flag {
	FLAG_OPT,
	FLAG_OPT_VAL,
	FLAG_OPT_ADDR,
	FLAG_OPT_DATA,
	FLAG_OPT_DATA_VAL,
	/* register no execute */
	FLAG_OPT_REG,
	FLAG_OPT_VAL_REG,
	FLAG_OPT_ADDR_REG,
	FLAG_OPT_DATA_REG,
	FLAG_OPT_DATA_VAL_REG,
	/* execute registered OPT */
	FLAG_OPT_START
};

struct opt_t {
	//int pid;
	int obj_id;
};

// Fields in the OPT packet
// Used by both SW and HW
struct opt_packet_t {
	opt_t* opt_obj;
	void* pmemaddr;
	//void* data_ptr;
	//int seg_id;
	//int data_val;
  unsigned size;
	opt_flag type;
};

struct opt_packet_fragment {
	opt_t* opt_obj;
	void* pmemaddr;
	//void* data_ptr;
	int seg_id;
	//int data_val;
  //unsigned size;
	opt_flag type;
};

struct opt_record_s{
	opt_t* opt_obj;
	void* pmemaddr;
	uint64_t tick_addr;
	uint64_t tick_data;
  int seg_id;
	bool isDataDup;
};
typedef struct opt_record_s opt_record;

/**
 * The DRAM controller is a single-channel memory controller capturing
 * the most important timing constraints associated with a
 * contemporary DRAM. For multi-channel memory systems, the controller
 * is combined with a crossbar model, with the channel address
 * interleaving taking part in the crossbar.
 *
 * As a basic design principle, this controller
 * model is not cycle callable, but instead uses events to: 1) decide
 * when new decisions can be made, 2) when resources become available,
 * 3) when things are to be considered done, and 4) when to send
 * things back. Through these simple principles, the model delivers
 * high performance, and lots of flexibility, allowing users to
 * evaluate the system impact of a wide range of memory technologies,
 * such as DDR3/4, LPDDR2/3/4, WideIO1/2, HBM and HMC.
 *
 * For more details, please see Hansson et al, "Simulating DRAM
 * controllers for future system architecture exploration",
 * Proc. ISPASS, 2014. If you use this model as part of your research
 * please cite the paper.
 *
 * The low-power functionality implements a staggered powerdown
 * similar to that described in "Optimized Active and Power-Down Mode
 * Refresh Control in 3D-DRAMs" by Jung et al, VLSI-SoC, 2014.
 */
class DRAMCtrl : public QoS::MemCtrl
{

  private:
    void checkPendingPredictionQueue();
    // For now, make use of a queued slave port to avoid dealing with
    // flow control for the responses being sent back
    class MemoryPort : public QueuedSlavePort
    {

        RespPacketQueue queue;
        DRAMCtrl& memory;

      public:

        MemoryPort(const std::string& name, DRAMCtrl& _memory);

      protected:

        Tick recvAtomic(PacketPtr pkt);

        void recvFunctional(PacketPtr pkt);

        bool recvTimingReq(PacketPtr);

        virtual AddrRangeList getAddrRanges() const;

    };

    /**
     * Our incoming port, for a multi-ported controller add a crossbar
     * in front of it
     */
    MemoryPort port;

    /**
     * Remember if the memory system is in timing mode
     */
    bool isTimingMode;

    /**
     * Remember if we have to retry a request when available.
     */
    bool retryRdReq;
    bool retryWrReq;

    /**/

    /**
     * Simple structure to hold the values needed to keep track of
     * commands for DRAMPower
     */
    struct Command {
       Data::MemCommand::cmds type;
       uint8_t bank;
       Tick timeStamp;

       constexpr Command(Data::MemCommand::cmds _type, uint8_t _bank,
                         Tick time_stamp)
            : type(_type), bank(_bank), timeStamp(time_stamp)
        { }
    };

    /**
     * A basic class to track the bank state, i.e. what row is
     * currently open (if any), when is the bank free to accept a new
     * column (read/write) command, when can it be precharged, and
     * when can it be activated.
     *
     * The bank also keeps track of how many bytes have been accessed
     * in the open row since it was opened.
     */
    class Bank
    {

      public:

        static const uint32_t NO_ROW = -1;

        uint32_t openRow;
        uint8_t bank;
        uint8_t bankgr;

        Tick rdAllowedAt;
        Tick wrAllowedAt;
        Tick preAllowedAt;
        Tick actAllowedAt;

        uint32_t rowAccesses;
        uint32_t bytesAccessed;

        Bank() :
            openRow(NO_ROW), bank(0), bankgr(0),
            rdAllowedAt(0), wrAllowedAt(0), preAllowedAt(0), actAllowedAt(0),
            rowAccesses(0), bytesAccessed(0)
        { }
    };


    /**
     * The power state captures the different operational states of
     * the DRAM and interacts with the bus read/write state machine,
     * and the refresh state machine.
     *
     * PWR_IDLE      : The idle state in which all banks are closed
     *                 From here can transition to:  PWR_REF, PWR_ACT,
     *                 PWR_PRE_PDN
     *
     * PWR_REF       : Auto-refresh state.  Will transition when refresh is
     *                 complete based on power state prior to PWR_REF
     *                 From here can transition to:  PWR_IDLE, PWR_PRE_PDN,
     *                 PWR_SREF
     *
     * PWR_SREF      : Self-refresh state.  Entered after refresh if
     *                 previous state was PWR_PRE_PDN
     *                 From here can transition to:  PWR_IDLE
     *
     * PWR_PRE_PDN   : Precharge power down state
     *                 From here can transition to:  PWR_REF, PWR_IDLE
     *
     * PWR_ACT       : Activate state in which one or more banks are open
     *                 From here can transition to:  PWR_IDLE, PWR_ACT_PDN
     *
     * PWR_ACT_PDN   : Activate power down state
     *                 From here can transition to:  PWR_ACT
     */
     enum PowerState {
         PWR_IDLE = 0,
         PWR_REF,
         PWR_SREF,
         PWR_PRE_PDN,
         PWR_ACT,
         PWR_ACT_PDN
     };

    /**
     * The refresh state is used to control the progress of the
     * refresh scheduling. When normal operation is in progress the
     * refresh state is idle. Once tREFI has elasped, a refresh event
     * is triggered to start the following STM transitions which are
     * used to issue a refresh and return back to normal operation
     *
     * REF_IDLE      : IDLE state used during normal operation
     *                 From here can transition to:  REF_DRAIN
     *
     * REF_SREF_EXIT : Exiting a self-refresh; refresh event scheduled
     *                 after self-refresh exit completes
     *                 From here can transition to:  REF_DRAIN
     *
     * REF_DRAIN     : Drain state in which on going accesses complete.
     *                 From here can transition to:  REF_PD_EXIT
     *
     * REF_PD_EXIT   : Evaluate pwrState and issue wakeup if needed
     *                 Next state dependent on whether banks are open
     *                 From here can transition to:  REF_PRE, REF_START
     *
     * REF_PRE       : Close (precharge) all open banks
     *                 From here can transition to:  REF_START
     *
     * REF_START     : Issue refresh command and update DRAMPower stats
     *                 From here can transition to:  REF_RUN
     *
     * REF_RUN       : Refresh running, waiting for tRFC to expire
     *                 From here can transition to:  REF_IDLE, REF_SREF_EXIT
     */
     enum RefreshState {
         REF_IDLE = 0,
         REF_DRAIN,
         REF_PD_EXIT,
         REF_SREF_EXIT,
         REF_PRE,
         REF_START,
         REF_RUN
     };

    class Rank;
    struct RankStats : public Stats::Group
    {
        RankStats(DRAMCtrl& memory, Rank &rank);

        void regStats() override;
        void resetStats() override;
        void preDumpStats() override;

        Rank &rank;

        /*
         * Command energies
         */
        Stats::Scalar actEnergy;
        Stats::Scalar preEnergy;
        Stats::Scalar readEnergy;
        Stats::Scalar writeEnergy;
        Stats::Scalar refreshEnergy;

        /*
         * Active Background Energy
         */
        Stats::Scalar actBackEnergy;

        /*
         * Precharge Background Energy
         */
        Stats::Scalar preBackEnergy;

        /*
         * Active Power-Down Energy
         */
        Stats::Scalar actPowerDownEnergy;

        /*
         * Precharge Power-Down Energy
         */
        Stats::Scalar prePowerDownEnergy;

        /*
         * self Refresh Energy
         */
        Stats::Scalar selfRefreshEnergy;

        Stats::Scalar totalEnergy;
        Stats::Scalar averagePower;

        /**
         * Stat to track total DRAM idle time
         *
         */
        Stats::Scalar totalIdleTime;

        /**
         * Track time spent in each power state.
         */
        Stats::Vector memoryStateTime;
    };

    /**
     * Rank class includes a vector of banks. Refresh and Power state
     * machines are defined per rank. Events required to change the
     * state of the refresh and power state machine are scheduled per
     * rank. This class allows the implementation of rank-wise refresh
     * and rank-wise power-down.
     */
    class Rank : public EventManager
    {

      private:

        /**
         * A reference to the parent DRAMCtrl instance
         */
        DRAMCtrl& memory;

        /**
         * Since we are taking decisions out of order, we need to keep
         * track of what power transition is happening at what time
         */
        PowerState pwrStateTrans;

        /**
         * Previous low-power state, which will be re-entered after refresh.
         */
        PowerState pwrStatePostRefresh;

        /**
         * Track when we transitioned to the current power state
         */
        Tick pwrStateTick;

        /**
         * Keep track of when a refresh is due.
         */
        Tick refreshDueAt;

        /**
         * Function to update Power Stats
         */
        void updatePowerStats();

        /**
         * Schedule a power state transition in the future, and
         * potentially override an already scheduled transition.
         *
         * @param pwr_state Power state to transition to
         * @param tick Tick when transition should take place
         */
        void schedulePowerEvent(PowerState pwr_state, Tick tick);

      public:

        /**
         * Current power state.
         */
        PowerState pwrState;

       /**
         * current refresh state
         */
        RefreshState refreshState;

        /**
         * rank is in or transitioning to power-down or self-refresh
         */
        bool inLowPowerState;

        /**
         * Current Rank index
         */
        uint8_t rank;

       /**
         * Track number of packets in read queue going to this rank
         */
        uint32_t readEntries;

       /**
         * Track number of packets in write queue going to this rank
         */
        uint32_t writeEntries;

        /**
         * Number of ACT, RD, and WR events currently scheduled
         * Incremented when a refresh event is started as well
         * Used to determine when a low-power state can be entered
         */
        uint8_t outstandingEvents;

        /**
         * delay power-down and self-refresh exit until this requirement is met
         */
        Tick wakeUpAllowedAt;

        /**
         * One DRAMPower instance per rank
         */
        DRAMPower power;

        /**
         * List of commands issued, to be sent to DRAMPpower at refresh
         * and stats dump.  Keep commands here since commands to different
         * banks are added out of order.  Will only pass commands up to
         * curTick() to DRAMPower after sorting.
         */
        std::vector<Command> cmdList;

        /**
         * Vector of Banks. Each rank is made of several devices which in
         * term are made from several banks.
         */
        std::vector<Bank> banks;

        /**
         *  To track number of banks which are currently active for
         *  this rank.
         */
        unsigned int numBanksActive;

        /** List to keep track of activate ticks */
        std::deque<Tick> actTicks;

        Rank(DRAMCtrl& _memory, const DRAMCtrlParams* _p, int rank);

        const std::string name() const
        {
            return csprintf("%s_%d", memory.name(), rank);
        }

        /**
         * Kick off accounting for power and refresh states and
         * schedule initial refresh.
         *
         * @param ref_tick Tick for first refresh
         */
        void startup(Tick ref_tick);

        /**
         * Stop the refresh events.
         */
        void suspend();

        /**
         * Check if there is no refresh and no preparation of refresh ongoing
         * i.e. the refresh state machine is in idle
         *
         * @param Return true if the rank is idle from a refresh point of view
         */
        bool inRefIdleState() const { return refreshState == REF_IDLE; }

        /**
         * Check if the current rank has all banks closed and is not
         * in a low power state
         *
         * @param Return true if the rank is idle from a bank
         *        and power point of view
         */
        bool inPwrIdleState() const { return pwrState == PWR_IDLE; }

        /**
         * Trigger a self-refresh exit if there are entries enqueued
         * Exit if there are any read entries regardless of the bus state.
         * If we are currently issuing write commands, exit if we have any
         * write commands enqueued as well.
         * Could expand this in the future to analyze state of entire queue
         * if needed.
         *
         * @return boolean indicating self-refresh exit should be scheduled
         */
        bool forceSelfRefreshExit() const {
            return (readEntries != 0) ||
                   ((memory.busStateNext == WRITE) && (writeEntries != 0));
        }

        /**
         * Check if the command queue of current rank is idle
         *
         * @param Return true if the there are no commands in Q.
         *                    Bus direction determines queue checked.
         */
        bool isQueueEmpty() const;

        /**
         * Let the rank check if it was waiting for requests to drain
         * to allow it to transition states.
         */
        void checkDrainDone();

        /**
         * Push command out of cmdList queue that are scheduled at
         * or before curTick() to DRAMPower library
         * All commands before curTick are guaranteed to be complete
         * and can safely be flushed.
         */
        void flushCmdList();

        /*
         * Function to register Stats
         */
        void regStats();

        /**
         * Computes stats just prior to dump event
         */
        void computeStats();

        /**
         * Reset stats on a stats event
         */
        void resetStats();

        /**
         * Schedule a transition to power-down (sleep)
         *
         * @param pwr_state Power state to transition to
         * @param tick Absolute tick when transition should take place
         */
        void powerDownSleep(PowerState pwr_state, Tick tick);

       /**
         * schedule and event to wake-up from power-down or self-refresh
         * and update bank timing parameters
         *
         * @param exit_delay Relative tick defining the delay required between
         *                   low-power exit and the next command
         */
        void scheduleWakeUpEvent(Tick exit_delay);

        void processWriteDoneEvent();
        EventFunctionWrapper writeDoneEvent;

        void processActivateEvent();
        EventFunctionWrapper activateEvent;

        void processPrechargeEvent();
        EventFunctionWrapper prechargeEvent;

        void processRefreshEvent();
        EventFunctionWrapper refreshEvent;

        void processPowerEvent();
        EventFunctionWrapper powerEvent;

        void processWakeUpEvent();
        EventFunctionWrapper wakeUpEvent;

      protected:
        RankStats stats;
    };

    /**
     * A burst helper helps organize and manage a packet that is larger than
     * the DRAM burst size. A system packet that is larger than the burst size
     * is split into multiple DRAM packets and all those DRAM packets point to
     * a single burst helper such that we know when the whole packet is served.
     */
    class BurstHelper {

      public:

        /** Number of DRAM bursts requred for a system packet **/
        const unsigned int burstCount;

        /** Number of DRAM bursts serviced so far for a system packet **/
        unsigned int burstsServiced;

        BurstHelper(unsigned int _burstCount)
            : burstCount(_burstCount), burstsServiced(0)
        { }
    };

    /**
     * A DRAM packet stores packets along with the timestamp of when
     * the packet entered the queue, and also the decoded address.
     */
    class DRAMPacket {

      public:

        /** When did request enter the controller */
        const Tick entryTime;

        /** When will request leave the controller */
        Tick readyTime;

        /** This comes from the outside world */
        const PacketPtr pkt;

        /** MasterID associated with the packet */
        const MasterID _masterId;

        const bool read;

        /** Will be populated by address decoder */
        const uint8_t rank;
        const uint8_t bank;
        const uint32_t row;
        bool isCounterPacket = false;
        bool isVerificationPacket = false;
        /**
         * Bank id is calculated considering banks in all the ranks
         * eg: 2 ranks each with 8 banks, then bankId = 0 --> rank0, bank0 and
         * bankId = 8 --> rank1, bank0
         */
        const uint16_t bankId;

        /**
         * The starting address of the DRAM packet.
         * This address could be unaligned to burst size boundaries. The
         * reason is to keep the address offset so we can accurately check
         * incoming read packets with packets in the write queue.
         */
        Addr addr;

        /**
         * The size of this dram packet in bytes
         * It is always equal or smaller than DRAM burst size
         */
        unsigned int size;

        /**
         * A pointer to the BurstHelper if this DRAMPacket is a split packet
         * If not a split packet (common case), this is set to NULL
         */
        BurstHelper* burstHelper;
        Bank& bankRef;
        Rank& rankRef;

        /**
         * QoS value of the encapsulated packet read at queuing time
         */
        uint8_t _qosValue;

        /**
         * Set the packet QoS value
         * (interface compatibility with Packet)
         */
        inline void qosValue(const uint8_t qv) { _qosValue = qv; }

        /**
         * Get the packet QoS value
         * (interface compatibility with Packet)
         */
        inline uint8_t qosValue() const { return _qosValue; }

        /**
         * Get the packet MasterID
         * (interface compatibility with Packet)
         */
        inline MasterID masterId() const { return _masterId; }

        /**
         * Get the packet size
         * (interface compatibility with Packet)
         */
        inline unsigned int getSize() const { return size; }

        /**
         * Get the packet address
         * (interface compatibility with Packet)
         */
        inline Addr getAddr() const { return addr; }

        /**
         * Return true if its a read packet
         * (interface compatibility with Packet)
         */
        inline bool isRead() const { return read; }

        /**
         * Return true if its a write packet
         * (interface compatibility with Packet)
         */
        inline bool isWrite() const { return !read; }


        DRAMPacket(PacketPtr _pkt, bool is_read, uint8_t _rank, uint8_t _bank,
                   uint32_t _row, uint16_t bank_id, Addr _addr,
                   unsigned int _size, Bank& bank_ref, Rank& rank_ref)
            : entryTime(curTick()), readyTime(curTick()), pkt(_pkt),
              _masterId(pkt->masterId()),
              read(is_read), rank(_rank), bank(_bank), row(_row),
              bankId(bank_id), addr(_addr), size(_size), burstHelper(NULL),
              bankRef(bank_ref), rankRef(rank_ref), _qosValue(_pkt->qosValue())
        { }

    };

    // The DRAM packets are store in a multiple dequeue structure,
    // based on their QoS priority
    typedef std::deque<DRAMPacket*> DRAMPacketQueue;

    /**
     * Bunch of things requires to setup "events" in gem5
     * When event "respondEvent" occurs for example, the method
     * processRespondEvent is called; no parameters are allowed
     * in these methods
     */
    void processNextReqEvent();
    EventFunctionWrapper nextReqEvent;

    void processRespondEvent();
    EventFunctionWrapper respondEvent;

    bool isDWEnabled = false; // De duplicaiton and wear levelling
    bool isEVEnabled = false; // encryption and verification

    /**
     * Check if the read queue has room for more entries
     *
     * @param pktCount The number of entries needed in the read queue
     * @return true if read queue is full, false otherwise
     */
    bool readQueueFull(unsigned int pktCount) const;

    /**
     * Check if the write queue has room for more entries
     *
     * @param pktCount The number of entries needed in the write queue
     * @return true if write queue is full, false otherwise
     */
    bool writeQueueFull(unsigned int pktCount) const;

    /**
     * When a new read comes in, first check if the write q has a
     * pending request to the same address.\ If not, decode the
     * address to populate rank/bank/row, create one or mutliple
     * "dram_pkt", and push them to the back of the read queue.\
     * If this is the only
     * read request in the system, schedule an event to start
     * servicing it.
     *
     * @param pkt The request packet from the outside world
     * @param pktCount The number of DRAM bursts the pkt
     * translate to. If pkt size is larger then one full burst,
     * then pktCount is greater than one.
     */
    void addToReadQueue(PacketPtr pkt, unsigned int pktCount);

    /**
     * Decode the incoming pkt, create a dram_pkt and push to the
     * back of the write queue. \If the write q length is more than
     * the threshold specified by the user, ie the queue is beginning
     * to get full, stop reads, and start draining writes.
     *
     * @param pkt The request packet from the outside world
     * @param pktCount The number of DRAM bursts the pkt
     * translate to. If pkt size is larger then one full burst,
     * then pktCount is greater than one.
     */
    void addToWriteQueue(PacketPtr pkt, unsigned int pktCount);

    /**
     * Actually do the DRAM access - figure out the latency it
     * will take to service the req based on bank state, channel state etc
     * and then update those states to account for this request.\ Based
     * on this, update the packet's "readyTime" and move it to the
     * response q from where it will eventually go back to the outside
     * world.
     *
     * @param pkt The DRAM packet created from the outside world pkt
     */
    void doDRAMAccess(DRAMPacket* dram_pkt);

    /**
     * When a packet reaches its "readyTime" in the response Q,
     * use the "access()" method in AbstractMemory to actually
     * create the response packet, and send it back to the outside
     * world requestor.
     *
     * @param pkt The packet from the outside world
     * @param static_latency Static latency to add before sending the packet
     */
    void accessAndRespond(PacketPtr pkt, Tick static_latency);

    /**
     * Address decoder to figure out physical mapping onto ranks,
     * banks, and rows. This function is called multiple times on the same
     * system packet if the pakcet is larger than burst of the memory. The
     * dramPktAddr is used for the offset within the packet.
     *
     * @param pkt The packet from the outside world
     * @param dramPktAddr The starting address of the DRAM packet
     * @param size The size of the DRAM packet in bytes
     * @param isRead Is the request for a read or a write to DRAM
     * @return A DRAMPacket pointer with the decoded information
     */
    DRAMPacket* decodeAddr(const PacketPtr pkt, Addr dramPktAddr,
                           unsigned int size, bool isRead) const;

    /**
     * The memory schduler/arbiter - picks which request needs to
     * go next, based on the specified policy such as FCFS or FR-FCFS
     * and moves it to the head of the queue.
     * Prioritizes accesses to the same rank as previous burst unless
     * controller is switching command type.
     *
     * @param queue Queued requests to consider
     * @param extra_col_delay Any extra delay due to a read/write switch
     * @return an iterator to the selected packet, else queue.end()
     */
    DRAMPacketQueue::iterator chooseNext(DRAMPacketQueue& queue,
        Tick extra_col_delay);

    /**
     * For FR-FCFS policy reorder the read/write queue depending on row buffer
     * hits and earliest bursts available in DRAM
     *
     * @param queue Queued requests to consider
     * @param extra_col_delay Any extra delay due to a read/write switch
     * @return an iterator to the selected packet, else queue.end()
     */
    DRAMPacketQueue::iterator chooseNextFRFCFS(DRAMPacketQueue& queue,
            Tick extra_col_delay);

    /**
     * Find which are the earliest banks ready to issue an activate
     * for the enqueued requests. Assumes maximum of 32 banks per rank
     * Also checks if the bank is already prepped.
     *
     * @param queue Queued requests to consider
     * @param min_col_at time of seamless burst command
     * @return One-hot encoded mask of bank indices
     * @return boolean indicating burst can issue seamlessly, with no gaps
     */
    std::pair<std::vector<uint32_t>, bool>
    minBankPrep(const DRAMPacketQueue& queue, Tick min_col_at) const;

    /**
     * Keep track of when row activations happen, in order to enforce
     * the maximum number of activations in the activation window. The
     * method updates the time that the banks become available based
     * on the current limits.
     *
     * @param rank_ref Reference to the rank
     * @param bank_ref Reference to the bank
     * @param act_tick Time when the activation takes place
     * @param row Index of the row
     */
    void activateBank(Rank& rank_ref, Bank& bank_ref, Tick act_tick,
                      uint32_t row);

    /**
     * Precharge a given bank and also update when the precharge is
     * done. This will also deal with any stats related to the
     * accesses to the open page.
     *
     * @param rank_ref The rank to precharge
     * @param bank_ref The bank to precharge
     * @param pre_at Time when the precharge takes place
     * @param trace Is this an auto precharge then do not add to trace
     */
    void prechargeBank(Rank& rank_ref, Bank& bank_ref,
                       Tick pre_at, bool trace = true);

    /**
     * Used for debugging to observe the contents of the queues.
     */
    void printQs() const;

    /**
     * Burst-align an address.
     *
     * @param addr The potentially unaligned address
     *
     * @return An address aligned to a DRAM burst
     */
    Addr burstAlign(Addr addr) const { return (addr & ~(Addr(burstSize - 1))); }

    /**
     * The controller's main read and write queues, with support for QoS reordering
     */
    std::vector<DRAMPacketQueue> readQueue;
    std::vector<DRAMPacketQueue> writeQueue;

    /**
     * To avoid iterating over the write queue to check for
     * overlapping transactions, maintain a set of burst addresses
     * that are currently queued. Since we merge writes to the same
     * location we never have more than one address to the same burst
     * address.
     */
    std::unordered_set<Addr> isInWriteQueue;

    /**
     * Response queue where read packets wait after we're done working
     * with them, but it's not time to send the response yet. The
     * responses are stored separately mostly to keep the code clean
     * and help with events scheduling. For all logical purposes such
     * as sizing the read queue, this and the main read queue need to
     * be added together.
     */
    std::deque<DRAMPacket*> respQueue;

    /**
     * Vector of ranks
     */
    std::vector<Rank*> ranks;

    /**
     * The following are basic design parameters of the memory
     * controller, and are initialized based on parameter values.
     * The rowsPerBank is determined based on the capacity, number of
     * ranks and banks, the burst size, and the row buffer size.
     */
    const uint32_t deviceSize;
    const uint32_t deviceBusWidth;
    const uint32_t burstLength;
    const uint32_t deviceRowBufferSize;
    const uint32_t devicesPerRank;
    const uint32_t burstSize;
    const uint32_t rowBufferSize;
    const uint32_t columnsPerRowBuffer;
    const uint32_t columnsPerStripe;
    const uint32_t ranksPerChannel;
    const uint32_t bankGroupsPerRank;
    const bool bankGroupArch;
    const uint32_t banksPerRank;
    const uint32_t channels;
    uint32_t rowsPerBank;
    const uint32_t readBufferSize;
    const uint32_t writeBufferSize;
    const uint32_t writeHighThreshold;
    const uint32_t writeLowThreshold;
    const uint32_t minWritesPerSwitch;
    uint32_t writesThisTime;
    uint32_t readsThisTime;

    /**
     * Basic memory timing parameters initialized based on parameter
     * values.
     */
    const Tick M5_CLASS_VAR_USED tCK;
    const Tick tRTW;
    const Tick tCS;
    const Tick tBURST;
    const Tick tCCD_L_WR;
    const Tick tCCD_L;
    const Tick tRCD;
    const Tick tCL;
    const Tick tRP;
    const Tick tRAS;
    const Tick tWR;
    const Tick tRTP;
    const Tick tRFC;
    const Tick tREFI;
    const Tick tRRD;
    const Tick tRRD_L;
    const Tick tXAW;
    const Tick tXP;
    const Tick tXS;
    const uint32_t activationLimit;
    const Tick rankToRankDly;
    const Tick wrToRdDly;
    const Tick rdToWrDly;

    /**
     * Memory controller configuration initialized based on parameter
     * values.
     */
    Enums::MemSched memSchedPolicy;
    Enums::AddrMap addrMapping;
    Enums::PageManage pageMgmt;

    /**
     * Max column accesses (read and write) per row, before forcefully
     * closing it.
     */
    const uint32_t maxAccessesPerRow;

    /**
     * Pipeline latency of the controller frontend. The frontend
     * contribution is added to writes (that complete when they are in
     * the write buffer) and reads that are serviced the write buffer.
     */
    const Tick frontendLatency;

    /**
     * Pipeline latency of the backend and PHY. Along with the
     * frontend contribution, this latency is added to reads serviced
     * by the DRAM.
     */
    const Tick backendLatency;

    /**
     * Till when must we wait before issuing next RD/WR burst?
     */
    Tick nextBurstAt;

    Tick prevArrival;

    /**
     * The soonest you have to start thinking about the next request
     * is the longest access time that can occur before
     * nextBurstAt. Assuming you need to precharge, open a new row,
     * and access, it is tRP + tRCD + tCL.
     */
    Tick nextReqTime;

    /** All statistics that the model needs to capture */
    struct DRAMStats : public Stats::Group {
        DRAMStats(DRAMCtrl &dram);

        void regStats() override;
        void resetStats() override;

        DRAMCtrl &dram;

        Stats::Scalar readReqs;
        Stats::Scalar writeReqs;
        Stats::Scalar readBursts;
        Stats::Scalar writeBursts;
        Stats::Scalar servicedByWrQ;
        Stats::Scalar mergedWrBursts;
        Stats::Scalar neitherReadNorWriteReqs;
        Stats::Vector perBankRdBursts;
        Stats::Vector perBankWrBursts;

        // Average queue lengths
        Stats::Average avgRdQLen;
        Stats::Average avgWrQLen;

        // Latencies summed over all requests
        Stats::Scalar totQLat;
        Stats::Scalar totBusLat;
        Stats::Scalar totMemAccLat;

        // Average latencies per request
        Stats::Formula avgQLat;
        Stats::Formula avgBusLat;
        Stats::Formula avgMemAccLat;

        Stats::Scalar numRdRetry;
        Stats::Scalar numWrRetry;

        // Row hit count and rate
        Stats::Scalar readRowHits;
        Stats::Scalar writeRowHits;
        Stats::Formula readRowHitRate;
        Stats::Formula writeRowHitRate;

        Stats::Vector readPktSize;
        Stats::Vector writePktSize;
        Stats::Vector rdQLenPdf;
        Stats::Vector wrQLenPdf;
        Stats::Histogram bytesPerActivate;
        Stats::Histogram rdPerTurnAround;
        Stats::Histogram wrPerTurnAround;

        Stats::Scalar bytesReadDRAM;
        Stats::Scalar bytesReadWrQ;
        Stats::Scalar bytesWritten;
        Stats::Scalar bytesReadSys;
        Stats::Scalar bytesWrittenSys;

        // Average bandwidth
        Stats::Formula avgRdBW;
        Stats::Formula avgWrBW;
        Stats::Formula avgRdBWSys;
        Stats::Formula avgWrBWSys;
        Stats::Formula peakBW;

        Stats::Formula busUtil;
        Stats::Formula busUtilRead;
        Stats::Formula busUtilWrite;

        Stats::Scalar totGap;
        Stats::Formula avgGap;

        // per-master bytes read and written to memory
        Stats::Vector masterReadBytes;
        Stats::Vector masterWriteBytes;

        // per-master bytes read and written to memory rate
        Stats::Formula masterReadRate;
        Stats::Formula masterWriteRate;

        // per-master read and write serviced memory accesses
        Stats::Vector masterReadAccesses;
        Stats::Vector masterWriteAccesses;

        // per-master read and write total memory access latency
        Stats::Vector masterReadTotalLat;
        Stats::Vector masterWriteTotalLat;

        // per-master raed and write average memory access latency
        Stats::Formula masterReadAvgLat;
        Stats::Formula masterWriteAvgLat;

        // DRAM Power Calculation
        Stats::Formula pageHitRate;

        // TXOPT
        Stats::Formula metadataCacheHitRate;
        Stats::Scalar totalCounterCacheRead; 
        Stats::Scalar pendingPredictionQueueChecks; 
        // Stats::Distribution pendingPredictionQueueLength; 
        Stats::Scalar totalCounterCacheReadHit; 
        Stats::Scalar extraMemoryAccesses;
        Stats::Scalar BMOLatencyEmulationCount; 
        Stats::Scalar totalTXOptOp;
        Stats::Scalar totalNonTXOptOp;
        //Stats::Scalar totalWriteReq;
        Stats::Scalar TXOptComplete;
        Stats::Scalar TXOptIncomplete;
        // Korakit
        // Stats for fraction of pre-executed BMO
        Stats::Scalar totalBMO;
        Stats::Scalar successful_pre_executedBMO;
        // Stats for fraction of successfully pre-executed BMO
        Stats::Scalar total_pre_executedBMO;

        Stats::Scalar data_pre;
        Stats::Scalar addr_pre;
        Stats::Scalar both_pre;
        Stats::Scalar addrNotPredicted;
        Stats::Scalar untimelyPrediction;
        Stats::Scalar start_before_write;
        Stats::Scalar opt_total_delay;
        Stats::Scalar pendingBMOQueueFlushes;
        Stats::Scalar totalCounterCacheWrite;      
        Stats::Scalar bmoFinishAfter;
        Stats::Scalar bmoFinishBefore;
        Stats::Distribution bmoFinishDist;
        Stats::Distribution timeliness;
    };

    DRAMStats stats;

    // Holds the value of the rank of burst issued
    uint8_t activeRank;

    // timestamp offset
    uint64_t timeStampOffset;

    /** The time when stats were last reset used to calculate average power */
    Tick lastStatsResetTick;

    /** Enable or disable DRAM powerdown states. */
    bool enableDRAMPowerdown;

    /**
     * Upstream caches need this packet until true is returned, so
     * hold it for deletion until a subsequent call
     */
    std::unique_ptr<Packet> pendingDelete;

    /**
     * This function increments the energy when called. If stats are
     * dumped periodically, note accumulated energy values will
     * appear in the stats (even if the stats are reset). This is a
     * result of the energy values coming from DRAMPower, and there
     * is currently no support for resetting the state.
     *
     * @param rank Current rank
     */
    void updatePowerStats(Rank& rank_ref);

    /**
     * Function for sorting Command structures based on timeStamp
     *
     * @param a Memory Command
     * @param next Memory Command
     * @return true if timeStamp of Command 1 < timeStamp of Command 2
     */
    static bool sortTime(const Command& cmd, const Command& cmd_next) {
        return cmd.timeStamp < cmd_next.timeStamp;
    };

  public:
    DRAMCtrl(const DRAMCtrlParams* p);
    ~DRAMCtrl();

    DrainState drain() override;

    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    virtual void init() override;
    virtual void startup() override;
    virtual void drainResume() override;

    /**
     * Return true once refresh is complete for all ranks and there are no
     * additional commands enqueued.  (only evaluated when draining)
     * This will ensure that all banks are closed, power state is IDLE, and
     * power stats have been updated
     *
     * @return true if all ranks have refreshed, with no commands enqueued
     *
     */
    bool allRanksDrained() const;
    static std::deque<CompletedWriteEntry> pendingPredictionQueue;
    static std::unordered_map<Addr, Tick> clwbLatency;

  protected:

    Tick recvAtomic(PacketPtr pkt);
    void recvFunctional(PacketPtr pkt);
    bool recvTimingReq(PacketPtr pkt);



/**************************************************************************/
/**************************************************************************/
/*                                BMO                                     */
/**************************************************************************/
/**************************************************************************/


  
  private:

	// We only store the first write response.
	// Once we receive the second one, no matter if it is the counter or data,
	// we process it right away.
	// The stored response will be sent with once the second pkt is received. 
	// So two responses will be sent together.
 	struct CounterLogEntry {
	  
	  uint8_t AckCount;
	 	  
	  PacketPtr pkt;
	  DRAMPacket* dram_pkt;
	  //Tick response_time;
	 
	  // only for atomic counters 
	  CounterLogEntry() {
		AckCount = 0;
	  }
		
  	};

	bool isCounterPkt(PacketPtr pkt) {
	  return (pkt->isCounterPacket);
	}
	
	bool isCounterPkt(DRAMPacket* dram_pkt) {
	  return (dram_pkt->isCounterPacket);
	}
	
  bool isVerificationPkt(PacketPtr pkt) {
	  return (pkt->isVerificationPacket);
	}

  bool isVerificationPkt(DRAMPacket* dram_pkt) {
	  return (dram_pkt->isVerificationPacket);
	}
	
	// bool isCacheFlushPkt(PacketPtr pkt) {
	//   return (pkt->getAddr() == CACHE_FLUSH_PADDR);
	// }

	// bool isCounterCacheFlushPkt(PacketPtr pkt) {
	//   return (pkt->getAddr() == METADATA_CACHE_FLUSH_PADDR);
	// }

  bool isCounterCacheMiss(PacketPtr pkt) {
	  return (pkt->isCounterCacheMiss);
	}

	// bool isTXOptPkt(PacketPtr pkt) {
	//   return (pkt->getAddr() >= TXOPT_PADDR && pkt->getAddr() < TXOPT_PADDR+64UL*MAX_THREADS);
	// }

	void invCounterLogEntry(PacketPtr pkt) {
		delete CounterLog[getDataAddr(pkt)];
		CounterLog.erase(getDataAddr(pkt));
	}

	Addr getDataAddr(PacketPtr pkt) {
		if (isCounterPkt(pkt))
			return (pkt->getAddr() - COUNTER_ADDR_DIFF) * 8;
		else
			return pkt->getAddr();
	}

  	// packet - number of writes: one for data and another for counter
	// note that it is a static variable
	static std::unordered_map<Addr, CounterLogEntry*> CounterLog;

	static uint32_t global_counter;

	// Write queue for counters
	// counters are not inserted to the write queue together with the data 
	// packet all the time
	// if the WriteQueue is full, postpone counter write and insert when
	// WriteQueue space available
	
	struct CounterWriteQueueEntry {
		PacketPtr counter_pkt;
		unsigned counter_pkt_count;
	};

	struct dedupReadQueueEntry {
		PacketPtr dedup_read_pkt;
		unsigned dedup_read_pkt_count;
	};
	
  	struct VerificationWriteQueueEntry {
		PacketPtr verification_pkt;
		unsigned verification_pkt_count;
	};
	
	static std::deque<CounterWriteQueueEntry*> CounterWriteQueue;
  static std::deque<VerificationWriteQueueEntry*> VerificationWriteQueue;

	// set of address of pending counter packets	
	static std::unordered_set<Addr> CounterWriteQueueAddr;
  static std::unordered_set<Addr> VerificationWriteQueueAddr;

	// addr map to hasDataReceived
	static std::unordered_map<Addr, bool> CounterAtomicWait;
//  Korakit
//  keep enabled for non-blocking case
#ifdef TXOPT_ENABLE
  uint64_t latest_opt_accept_tick[BACKEND_PIPELINE_OPS];
#endif

#ifdef TXOPT_NON_BLOCK
  uint64_t latest_pipe_in = 0;
#endif

	unsigned counter_size = 8;
	unsigned atomic_counter_size = 8;
	unsigned dedup_read_size = 64;
	//unsigned CounterWriteQueueThreshold = 3;
	unsigned AtomicCounterWriteQueueThreshold = 1;

  //Korakit: for verification
  //may need to change
  unsigned verification_hash_size = 8;

	bool isCounterAtomicPkt(PacketPtr pkt) {
// #ifndef SEL_COUNTER_ATOMIC
		// all write packets are counter atomic
		return pkt->isWrite();
// #endif

		// if (pkt->getAddr() >= COUNTER_ATOMIC_PADDR 
		// 	&& pkt->getAddr() < COUNTER_ATOMIC_PADDR + 4*NUM_COUNTER_ATOMIC_PAGE*1024) {
		// 	return true;
		// } else {
		// 	return false;
		// }
	}

	static bool hasCounterCacheFlushed;
	static std::deque<CounterWriteQueueEntry*> AtomicCounterWriteQueue;
// Korakit
// removed, never used
/*
	bool isInCounterWriteQueue(Addr addr) {
		return !(CounterWriteQueueAddr.find(addr) == CounterWriteQueueAddr.end());
	}

	bool isInAtomicCounterWriteQueue(Addr addr) {
		//return !(CounterAtomicWait.find(addr) == CounterAtomicWait.end());
		std::deque<CounterWriteQueueEntry*> tmp = AtomicCounterWriteQueue;
		while(!tmp.empty()) {
			if (getDataAddr(tmp.front()->counter_pkt) == addr) {
				DPRINTF(myflag2, "Addr=%lld in counter write queue\n", addr);
				return true;
			} else {
				tmp.pop_front();	
			}
		}
		DPRINTF(myflag2, "Addr=%lld NOT in counter write queue\n", addr);
		return false;
	}
*/
	// Counter Cache
	// Fully associative
	// True LRU
	struct CounterCacheEntry {
		Addr data_addr;
		unsigned cnt = 0; //for LRU
		bool dirty;
	};
	

  struct VerificationCacheEntry {
		Addr data_addr;
		unsigned cnt = 0; //for LRU
		bool dirty;
	};

	// input data address	
	CounterWriteQueueEntry* createCounterPkt(Addr _addr) {
		RequestPtr counter_req(new Request((_addr / 8 + COUNTER_ADDR_DIFF), 
												counter_size, Request::PHYSICAL,
												Request::funcMasterId));
		
		PacketPtr counter_pkt = new Packet(counter_req, MemCmd::WriteReq);
		// value does not matter here
		uint32_t* counter_val = new uint32_t(1);
		counter_pkt->dataDynamic<uint32_t>(counter_val);
		counter_pkt->isCounterPacket = true;

		unsigned counter_offset = ((_addr / 8 + COUNTER_ADDR_DIFF)) 
									& (burstSize - 1);
		
		unsigned counter_pkt_count = divCeil(counter_size + counter_offset, burstSize);

		CounterWriteQueueEntry* newCounterPacket = new CounterWriteQueueEntry;
		newCounterPacket->counter_pkt = counter_pkt;
		newCounterPacket->counter_pkt_count = counter_pkt_count;

		return newCounterPacket;	
	}


  VerificationWriteQueueEntry* createVerificationPkt(Addr _addr) {
		RequestPtr verification_req(new Request((_addr / 8 + VERIFICATION_ADDR_DIFF), 
												counter_size, Request::PHYSICAL,
												Request::funcMasterId));
		
		PacketPtr verification_pkt = new Packet(verification_req, MemCmd::WriteReq);
		// value does not matter here
		uint32_t* hash_val = new uint32_t(1);
		verification_pkt->dataDynamic<uint32_t>(hash_val);
		verification_pkt->isVerificationPacket = true;

		unsigned verification_offset = ((_addr / 8 + VERIFICATION_ADDR_DIFF)) 
									& (burstSize - 1);
		
		unsigned verification_pkt_count = divCeil(counter_size + verification_offset, burstSize);

		VerificationWriteQueueEntry* newVerificationPacket = new VerificationWriteQueueEntry;
		newVerificationPacket->verification_pkt = verification_pkt;
		newVerificationPacket->verification_pkt_count = verification_pkt_count;

		return newVerificationPacket;	
	}



	static std::unordered_map<unsigned, 
						std::unordered_map<Addr, CounterCacheEntry*> > CounterCache;
	static std::deque<CounterWriteQueueEntry*> CounterCacheMissQueue;	
	static std::deque<CounterWriteQueueEntry*> CounterCacheEvictionQueue;
  static std::unordered_set<Addr> CounterCacheMSHR;
  static std::unordered_map<Addr, VerificationCacheEntry*> VerificationCache;  //verification cache
  static std::deque<VerificationWriteQueueEntry*> VerificationCacheMissQueue;
	static std::deque<VerificationWriteQueueEntry*> VerificationCacheEvictionQueue;
  static std::unordered_set<Addr> VerificationCacheMSHR;
  static std::deque<dedupReadQueueEntry*> dedupReadQueue;

  // Queue that temporarily going to hold writes before read and write operations on the caches are 
  // performed
	static const unsigned num_sets = COUNTER_CACHE_SIZE / NUM_WAY;

	static uint64_t EvictionCnt;	
	//static uint64_t tot_counter_cache_read;	
	//static uint64_t counter_cache_read_hit;
	static uint64_t atomic_writes;
	static uint64_t atomic_wait;


	// find set
	static unsigned getIndex(Addr _addr) {
		unsigned index;
		index = _addr / COUNTER_CACHE_LINE_SIZE - 
			(_addr / COUNTER_CACHE_LINE_SIZE / num_sets) * num_sets;
		assert(index < num_sets);
		//DPRINTF(myflag3, "addr=%lld,index=%u\n", _addr, index);
		return index;
	}

public:
	// Counter hash stores dedup and encryption info
	static bool isCounterCacheHit(Addr _addr) {

		unsigned index = getIndex(_addr);
		if (CounterCache.find(index) != CounterCache.end()) {
			if (CounterCache[index].find(_addr) != CounterCache[index].end()) {
				return true;
			}
		}
		
		return false;
	}


  static bool isVerificationCacheHit(Addr _addr) {
    if (VerificationCache.find(_addr) != VerificationCache.end()) {
      return true;
    }
		return false;
	}

  Addr dataToMTAddr(Addr dataAddr, int levelFromBottom){
    Addr rtn = ( dataToMTOffset(dataAddr, levelFromBottom) / 8) +
                    VERIFICATION_ADDR_DIFF;
    assert(rtn >= VERIFICATION_ADDR_DIFF);
    //debug
    //return VERIFICATION_ADDR_DIFF;
    ///////
    return rtn;
  }
private:
  Addr dataToMTOffset(Addr dataAddr, int levelFromBottom){
    return (dataAddr >> (levelFromBottom*3))+(1<<((VERIFICATION_TREE_HEIGHT-levelFromBottom)*3));
  }
	
	// return LRU for counter cache
	Addr incrCacheCnt(Addr _addr) {
		unsigned index = getIndex(_addr);
		Addr LRU = -1;
		unsigned max_cnt = 0;
		for (auto i = CounterCache[index].begin(); 
				i != CounterCache[index].end(); ++i) {

			i->second->cnt++;
			if (i->second->cnt > max_cnt) {
				max_cnt = i->second->cnt;
				LRU = i->first;
			}
		}
		return LRU;
	}

  // return LRU for Verification cache
	Addr incrVeriCacheCnt(Addr _addr) {
		Addr LRU = -1;
		unsigned max_cnt = 0;
		for (auto i = VerificationCache.begin(); 
				i != VerificationCache.end(); ++i) {

			i->second->cnt++;
			if (i->second->cnt > max_cnt) {
				max_cnt = i->second->cnt;
				LRU = i->first;
			}
		}
		return LRU;
	}


	static bool hasCounterCacheInit;
	static uint64_t init_cnt;
	static uint64_t init_hit_cnt;

  static bool hasVerificationCacheInit;

  bool isAddrVolatile(Addr addr);
  bool isAddrNonVolatile(Addr addr);
  void FlushPendingBMOQueues();
  void BMOHandleRequest(PacketPtr pkt);
  void BMOHandleWriteRequest(PacketPtr pkt);
  void BMOHandleReadRequest(PacketPtr pkt);
  Tick getWriteLatency(PacketPtr pkt, CompletedWriteEntry completedWriteEntry, bool addrPredicted, bool dataPredicted);
  void emulateBMOSlowdown(PacketPtr pkt, CompletedWriteEntry completedWriteEntry, bool addrPredicted, bool dataPredicted);

	void initCounterCache(Addr max_addr) {
		DPRINTF(myflag3, "@@ max_paddr=%llx\n", max_addr);
		
		//clean all
		CounterCache.clear();
		CounterCacheMissQueue.clear();
		CounterCacheEvictionQueue.clear();
		EvictionCnt = 0;
		//tot_counter_cache_read = 0;
		//counter_cache_read_hit = 0;
		stats.totalCounterCacheReadHit = 0;
		stats.totalCounterCacheRead = 0;
		stats.TXOptComplete = 0;
		stats.TXOptIncomplete = 0;
		atomic_writes = 0;
		atomic_wait = 0;

		max_addr = max_addr / COUNTER_CACHE_LINE_SIZE;
		max_addr = max_addr * COUNTER_CACHE_LINE_SIZE;
		for (Addr addr = 0; addr <= max_addr; addr += COUNTER_CACHE_LINE_SIZE) {
			init_cnt++;
			// insert to counter cache;
			
			bool isHit = isCounterCacheHit(addr);

			Addr LRU = incrCacheCnt(addr);
			unsigned index = getIndex(addr);
			
			if (!isHit) {
				CounterCache[index][addr] = new CounterCacheEntry; 
				CounterCache[index][addr]->dirty = false;
				if (CounterCache[index].size() > NUM_WAY) {
					delete CounterCache[index][LRU];
					CounterCache[index].erase(LRU);
				}
			} else {
				init_hit_cnt++;
				CounterCache[index][addr]->cnt = 0;
			}
		}

		hasCounterCacheInit = true;
#ifdef TXOPT_ENABLE
    for(int i=0;i<BACKEND_PIPELINE_OPS;i++){
      latest_opt_accept_tick[i] = 0UL;
    }
#endif
	}

#ifdef TXOPT_ENABLE
  uint64_t pipeline_delay_start_tick(uint64_t currTick){
      int minIndex=0;
      for(int i=0;i<BACKEND_PIPELINE_OPS;i++){
        if(latest_opt_accept_tick[i]<latest_opt_accept_tick[minIndex]){
          minIndex=i;
        }
      }
      latest_opt_accept_tick[minIndex] = ((latest_opt_accept_tick[minIndex] > currTick) ? latest_opt_accept_tick[minIndex] : currTick) + BACKEND_PIPELINE_LATENCY;
      // fprintf(stdout, "OPT start time: %llu Pipe#: %d\n", (long long unsigned)latest_opt_accept_tick[minIndex], minIndex);
      return latest_opt_accept_tick[minIndex];
  }
#endif
#ifdef TXOPT_NON_BLOCK
  uint64_t pipeline_get_latest(){
    return latest_pipe_in;
  }
  void pipeline_set_latest(uint64_t newVal){
    latest_pipe_in = newVal;
  }
#endif
  // Initialize Verification hash cache
  void initVerificationCache(Addr max_addr) {
    DPRINTF(myflag3, "@@ ---1\n");
    VerificationCache.clear();
		VerificationCacheMissQueue.clear();
		VerificationCacheEvictionQueue.clear();
    DPRINTF(myflag3, "@@ ---2\n");
    //Align address to the cacheline
    max_addr = max_addr / VERIFICATION_CACHE_LINE_SIZE;
		max_addr = max_addr * VERIFICATION_CACHE_LINE_SIZE;
    for (Addr addr = max_addr - VERIFICATION_CACHE_LINE_SIZE * VERIFICATION_CACHE_SIZE/VERIFICATION_TREE_HEIGHT; addr <= max_addr; addr += VERIFICATION_CACHE_LINE_SIZE) {
			//init_cnt++;
			// insert to verification cache;
			//to do, convert to proper address
      for(int nodeLevel=0;nodeLevel<VERIFICATION_TREE_HEIGHT;nodeLevel++){
        Addr MToffset = dataToMTOffset(addr, nodeLevel);
        DPRINTF(myflag3, "@@ ---3\n");
        bool isHit = isVerificationCacheHit(MToffset);
        DPRINTF(myflag3, "@@ ---4\n");
        Addr LRU = incrVeriCacheCnt(MToffset);
        
        DPRINTF(myflag3, "@@ ---5\n");
        if (!isHit) {
          VerificationCache[MToffset] = new VerificationCacheEntry; 
          VerificationCache[MToffset]->dirty = false;
          if (VerificationCache.size() > VERIFICATION_CACHE_SIZE) {
            DPRINTF(myflag3, "@@ ---6\n");
            delete VerificationCache[LRU];
            VerificationCache.erase(LRU);
            DPRINTF(myflag3, "@@ ---7\n");
          }
        } else {
          //init_hit_cnt++;
          VerificationCache[MToffset]->cnt = 0;
        }
      }
    }
    hasVerificationCacheInit = true;
  }
	// if write miss, write to counter cache directly
	// counter cache miss for write req does not matter
	// atomic counter does not go through counter cache
  bool writeCounterCache(PacketPtr _pkt){
      return writeCounterCache(_pkt, false);
  }


	bool writeCounterCache(PacketPtr _pkt, bool noUpdate) {
		// if (!hasCounterCacheInit)
		//   return true;
		//tot_counter_cache_read ++;
		stats.totalCounterCacheRead++;
    stats.totalCounterCacheWrite++;
		Addr _addr = _pkt->getAddr();

		_addr /= COUNTER_CACHE_LINE_SIZE;
		_addr *= COUNTER_CACHE_LINE_SIZE;

		unsigned index = getIndex(_addr);

		bool isHit = isCounterCacheHit(_addr);
		Addr CounterCacheLRU;
		// increment existing cnt
    if(!noUpdate){
		  CounterCacheLRU = incrCacheCnt(_addr);
    }
		if (!isHit) { // miss
			RequestPtr counter_req(new Request((_addr / 8 + COUNTER_ADDR_DIFF), 
												counter_size, Request::PHYSICAL,
												Request::funcMasterId));
			PacketPtr counter_pkt = new Packet(counter_req, MemCmd::ReadReq);
			// value does not matter here
			uint64_t* counter_val = new uint64_t(0);
			counter_pkt->dataDynamic<uint64_t>(counter_val);
			counter_pkt->isCounterPacket = true;

			unsigned counter_offset = ((_addr / 8  + COUNTER_ADDR_DIFF))
										& (burstSize - 1);
			unsigned counter_pkt_count = divCeil(counter_size + counter_offset, burstSize);
	
			// actually it is a read, but we use write queue entry for simplicity
			CounterWriteQueueEntry* newCounterPacket = new CounterWriteQueueEntry;
			newCounterPacket->counter_pkt = counter_pkt;
			newCounterPacket->counter_pkt_count = counter_pkt_count;

			CounterCacheMissQueue.push_back(newCounterPacket);

      if(!noUpdate){

        CounterCache[index][_addr] = new CounterCacheEntry;
        CounterCache[index][_addr]->dirty = true;
        
        if (CounterCache[index].size() > NUM_WAY) { // evict LRU
          if (CounterCache[index][CounterCacheLRU]->dirty) {
            EvictionCnt ++;
            // create eviction packet
            CounterWriteQueueEntry* evict_pkt = createCounterPkt(CounterCacheLRU);
            evict_pkt->counter_pkt->isCounterCacheEviction = true;
            CounterCacheEvictionQueue.push_back(evict_pkt);
          }
          // eviction
          delete CounterCache[index][CounterCacheLRU];
          CounterCache[index].erase(CounterCacheLRU);
          assert(CounterCache[index].size() == NUM_WAY);
        }
      }
		} else { // hit
			stats.totalCounterCacheReadHit++;
			//counter_cache_read_hit++;
			CounterCache[index][_addr]->dirty = true;
			CounterCache[index][_addr]->cnt = 0;
			DPRINTF(myflag2, "Counter write hit, addr=%lld\n", _pkt->getAddr());
		}
		// if all writes are atomic, no dirty counter remains in counter cache
		if (isCounterAtomicPkt(_pkt))
			CounterCache[index][_addr]->dirty = false;
		return isHit;
	}
  
	bool writeVerificationCache(PacketPtr _pkt, int nodeLevel) {
    // if(!hasVerificationCacheInit){
    //   return true;
    // }

    Addr _addr = _pkt->getAddr();
    //align the address to cache-line
    _addr /= VERIFICATION_CACHE_LINE_SIZE;
    _addr *= VERIFICATION_CACHE_LINE_SIZE;
    Addr MToffset = dataToMTOffset(_addr, nodeLevel);
    Addr MTAddr = dataToMTAddr(_addr, nodeLevel);
    DPRINTF(myflag_status, "DataAddr=%llx, MTAddr=0x%llx", _pkt->getAddr(), MTAddr);
    bool isHit = isVerificationCacheHit(MToffset);
    Addr VerificationCacheLRU = incrVeriCacheCnt(MToffset);

    //handle miss/hit
    if (!isHit) { // miss
      RequestPtr verification_req(new Request(MTAddr, 
                        verification_hash_size, Request::PHYSICAL,
                        Request::funcMasterId));
      PacketPtr verification_pkt = new Packet(verification_req, MemCmd::ReadReq);
      // value does not matter here
      uint64_t* hash_val = new uint64_t(0);
      verification_pkt->dataDynamic<uint64_t>(hash_val);
      verification_pkt->isVerificationPacket = true;

      unsigned verification_offset = (MTAddr)
                    & (burstSize - 1);
      unsigned verification_pkt_count = divCeil(verification_hash_size + verification_offset, burstSize);
  
      // actually it is a read, but we use write queue entry for simplicity
      VerificationWriteQueueEntry* newVerificationPacket = new VerificationWriteQueueEntry;
      newVerificationPacket->verification_pkt = verification_pkt;
      newVerificationPacket->verification_pkt_count = verification_pkt_count;

      VerificationCacheMissQueue.push_back(newVerificationPacket);

      VerificationCache[MToffset] = new VerificationCacheEntry;
      VerificationCache[MToffset]->dirty = true;
      
      if (VerificationCache.size() > VERIFICATION_CACHE_SIZE) { // evict LRU
        if (VerificationCache[VerificationCacheLRU]->dirty) {
          //EvictionCnt ++;
          // create eviction packet
          VerificationWriteQueueEntry* evict_pkt = createVerificationPkt(VerificationCacheLRU);
          evict_pkt->verification_pkt->isVerificationCacheEviction = true;
          VerificationCacheEvictionQueue.push_back(evict_pkt);
        }
        // eviction
        delete VerificationCache[VerificationCacheLRU];
        VerificationCache.erase(VerificationCacheLRU);
        assert(VerificationCache.size() == VERIFICATION_CACHE_SIZE);
      }
    } else { // hit
      //totalCounterCacheReadHit++;
      //counter_cache_read_hit++;
      VerificationCache[MToffset]->dirty = true;
      VerificationCache[MToffset]->cnt = 0;
    }
    // Korakit:
    // should not matter, we do not writethrough even for the counteratomic
    // only root need to be persistent
    /*
    if (isCounterAtomicPkt(_pkt))
      CounterCache[index][_addr]->dirty = false;
    */
    return isHit;
  }

	// if read miss, send read request to memory ctrl and read from memory
	// if read miss, counter load time be added to read response time
	bool readCounterCache(PacketPtr pkt) {
      bool result = readCounterCache(pkt->req->getPaddr());
      return result;
  }
	bool readCounterCache(Addr paddr/* PacketPtr _pkt */) {
		// if (!hasCounterCacheInit) {
		//   return true;    
    // }
		//tot_counter_cache_read ++;
		stats.totalCounterCacheRead++;

		Addr _addr = paddr;//_pkt->getAddr();
		
		_addr /= COUNTER_CACHE_LINE_SIZE;
		_addr *= COUNTER_CACHE_LINE_SIZE;

		unsigned index = getIndex(_addr);
		
		//DPRINTF(myflag3, "index=%u, num_sets=%u, addr=%lld\n", index, num_sets, _addr);
		bool isHit = isCounterCacheHit(_addr);

		// increment existing cnt
		Addr CounterCacheLRU = incrCacheCnt(_addr);		

		if (!isHit) { // miss
			DPRINTF(myflag2, "Counter read miss, addr=%lld\n", _addr);
			// handle counter cache miss
			RequestPtr counter_req(new Request((_addr / 8 + COUNTER_ADDR_DIFF), 
												counter_size, Request::PHYSICAL,
												Request::funcMasterId));
			PacketPtr counter_pkt = new Packet(counter_req, MemCmd::ReadReq);
			// value does not matter here
			uint64_t* counter_val = new uint64_t(0);
			counter_pkt->dataDynamic<uint64_t>(counter_val);
			counter_pkt->isCounterPacket = true;

			unsigned counter_offset = ((_addr / 8  + COUNTER_ADDR_DIFF))
										& (burstSize - 1);
			unsigned counter_pkt_count = divCeil(counter_size + counter_offset, burstSize);
	
			// actually it is a read, but we use write queue entry for simplicity
			CounterWriteQueueEntry* newCounterPacket = new CounterWriteQueueEntry;
			newCounterPacket->counter_pkt = counter_pkt;
			newCounterPacket->counter_pkt_count = counter_pkt_count;

			CounterCacheMissQueue.push_back(newCounterPacket);
			CounterCache.at(index);
			CounterCache[index][_addr] = new CounterCacheEntry;
			CounterCache.at(index).at(_addr);
			CounterCache[index][_addr]->dirty = false;
			if (CounterCache[index].size() > NUM_WAY) { // evict LRU
				if (CounterCache[index][CounterCacheLRU]->dirty) {
			    CounterCache.at(index).at(CounterCacheLRU);
					// create eviction packet
					EvictionCnt ++;
					CounterWriteQueueEntry* evict_pkt = createCounterPkt(CounterCacheLRU);
					evict_pkt->counter_pkt->isCounterCacheEviction = true;
					CounterCacheEvictionQueue.push_back(evict_pkt);
				}
				// eviction
				delete CounterCache[index][CounterCacheLRU];
        CounterCache.at(index).at(CounterCacheLRU);
				CounterCache[index].erase(CounterCacheLRU);
				assert(CounterCache[index].size() == NUM_WAY);
			}
		} else { // hit
			//counter_cache_read_hit++;
			stats.totalCounterCacheReadHit++;
			// DPRINTF(myflag2, "Counter read hit, addr=%lld\n", _pkt->getAddr());
			CounterCache[index][_addr]->cnt = 0;
		}
		return isHit;
	}
  
  bool readVerificationCache(CompletedWriteEntry &completedEntry) {
    // assert(size != 0);
    // Addr paddr = -1;

    // SM: Don't need these, translation is now done at prediction
    // bool translated = EmulationPageTable::pageTableStaticObj->translate(completedEntry.get_addr(), paddr);
    // panic_if(not translated, "Can't translate address");

    for(int nodeLevel = 0; nodeLevel<VERIFICATION_TREE_HEIGHT; nodeLevel ++){
        bool isVerificationCacheHit = readVerificationCache(completedEntry.get_addr(), nodeLevel);
        if (!isVerificationCacheHit) {
            completedEntry.verificationCacheMisses++;
        }else{
            // The MT cache is in the trust domain, break.
            break;
        }
    }
    return true;
  }

  bool readVerificationCache(PacketPtr pkt) {
    DPRINTF(BMO, "inside read verification cache with params (PacketPtr pkt) \n");
  
    volatile Addr paddr = pkt->req->getPaddr();
    
    DPRINTF(BMO, "Got paddr = %p \n", (void*)paddr);
    panic_if(-1 == paddr, "Can't translate address, got %p", (void*)paddr);

    const size_t limit = VERIFICATION_TREE_HEIGHT;
    for(size_t nodeLevel = 0; nodeLevel<limit; nodeLevel++){
      assert(nodeLevel < VERIFICATION_TREE_HEIGHT);
      if (nodeLevel > limit) {
        exit(1);  
      }
      bool isVerificationCacheHit = readVerificationCache(paddr, nodeLevel);
      if (!isVerificationCacheHit) {
          pkt->verificationCacheMisses++;
      }else{
          // The MT cache is in the trust domain, break.
          break;
        }
    }
    return true;
  }

  bool readVerificationCache(Addr paddr, int nodeLevel) {
    // return true;
    // printf("%s:%d :: %s()\n", __FILE__, __LINE__, __FUNCTION__);
    // if (!hasVerificationCacheInit)
		//   return true;

    //removed, descend at the caller
    //We need to iterate up to the Merkel tree root.
    //for(int nodeLevel=0;nodeLevel<VERIFICATION_TREE_HEIGHT;nodeLevel++){    
		Addr _addr = paddr;//_pkt->getAddr();

		
		_addr /= VERIFICATION_CACHE_LINE_SIZE;
		_addr *= VERIFICATION_CACHE_LINE_SIZE;

    Addr MToffset = dataToMTOffset(_addr, nodeLevel);
    Addr MTAddr = dataToMTAddr(_addr, nodeLevel);
    // DPRINTF(myflag_status, "DataAddr=%llx, MToffset=0x%llx, MTAddr=0x%llx\n", _pkt->getAddr(), MToffset, MTAddr);

		//unsigned index = getIndex(_addr);
		
		//DPRINTF(myflag3, "index=%u, num_sets=%u, addr=%lld\n", index, num_sets, _addr);
		bool isHit = isVerificationCacheHit(MToffset);
		// increment existing cnt
		Addr VerificationCacheLRU = incrVeriCacheCnt(MToffset);		

		if (!isHit) { // miss
			// printf("Verification read miss, addr=%lld\n", _addr);
			// handle counter cache miss
			RequestPtr verification_req(new Request((MTAddr), 
												verification_hash_size, Request::PHYSICAL,
												Request::funcMasterId));
			PacketPtr verification_pkt = new Packet(verification_req, MemCmd::ReadReq);
			// value does not matter here
      // real hash value might be different but we don't use it => no need to handle
			uint64_t* verification_val = new uint64_t(0);
			verification_pkt->dataDynamic<uint64_t>(verification_val);
			verification_pkt->isVerificationPacket = true;

			unsigned verification_offset = MTAddr & (burstSize - 1);
			unsigned verification_pkt_count = divCeil(verification_hash_size + verification_offset, burstSize);
	
			// actually it is a read, but we use write queue entry for simplicity
			VerificationWriteQueueEntry* newVerificationPacket = new VerificationWriteQueueEntry;
			newVerificationPacket->verification_pkt = verification_pkt;
			newVerificationPacket->verification_pkt_count = verification_pkt_count;

			VerificationCacheMissQueue.push_back(newVerificationPacket);
			
			VerificationCache[MToffset] = new VerificationCacheEntry;
			VerificationCache[MToffset]->dirty = false;
			if (VerificationCache.size() > VERIFICATION_CACHE_SIZE) { // evict LRU
				if (VerificationCache[VerificationCacheLRU]->dirty) {
					// create eviction packet
					//EvictionCnt ++;
					VerificationWriteQueueEntry* evict_pkt = createVerificationPkt(VerificationCacheLRU);
					evict_pkt->verification_pkt->isVerificationCacheEviction = true;
					VerificationCacheEvictionQueue.push_back(evict_pkt);
				}
				// eviction
				delete VerificationCache[VerificationCacheLRU];
				VerificationCache.erase(VerificationCacheLRU);
				assert(VerificationCache.size() == VERIFICATION_CACHE_SIZE);
			}
		} else { // hit
			//counter_cache_read_hit++;
			//totalCounterCacheReadHit++;
			//DPRINTF(myflag2, "Counter read hit, addr=%lld\n", _pkt->getAddr());
			VerificationCache[MToffset]->cnt = 0;
		}
    // printf("%s:%d :: %s()\n", __FILE__, __LINE__, __FUNCTION__);
		return isHit;
	}
	
	// Once recevice a counter flush instruction, call CounterCacheWriteBack first,
	// then flush the CounterWriteQueue
	void CounterCacheWriteback(PacketPtr pkt) {
		Addr flush_vaddr = -1;
		Addr flush_addr = -1;

    if (pkt->req->hasVaddr()) {
      flush_vaddr = pkt->req->getVaddr();
    }

    if (pkt->req->hasPaddr()) {
      flush_addr = pkt->req->getPaddr();
    }

    if (flush_vaddr == -1) {
      assert(false);
    }

    if (flush_addr == -1 and flush_vaddr != -1) {
      assert(EmulationPageTable::pageTableStaticObj->translate(flush_vaddr, flush_addr));
    }
		
		// if (flush_vaddr) {
		// 	flush_addr = EmulationPageTable::Virtual2Physical[flush_vaddr / PAGE_SIZE_COMMON] 
		// 				* PAGE_SIZE_COMMON
		// 			+ flush_vaddr - PAGE_SIZE_COMMON * (flush_vaddr / PAGE_SIZE_COMMON);
		// }
    
		//DPRINTF(myflag2, "flush counter cache: paddr=%llx, dirty=%d\n", 
		//				flush_addr, CounterCache[getIndex(flush_addr)][flush_addr]->dirty);
		for (auto i = CounterCache.begin(); i != CounterCache.end(); ++i) {
			for (auto j = i->second.begin(); j != i->second.end(); ++j) {
				Addr counter_pkt_addr = j->first;
				// DPRINTF(myflag2, "flush counter cache, dirty=%d, addr=0x%llx\n", 
															// j->second->dirty, flush_addr);
				if (j->second->dirty && (/* (flush_vaddr == 0)
							||  */(counter_pkt_addr / COUNTER_CACHE_LINE_SIZE 
								== flush_addr / COUNTER_CACHE_LINE_SIZE))) {
					CounterWriteQueue.push_back(createCounterPkt(counter_pkt_addr));
					j->second->dirty = false;
				}
			}
		}
	}

	//static unsigned counter_read_length;
	static unsigned counter_write_length;

	bool counterWriteQueueFull(unsigned request_size) {
	  DPRINTF(myflag, "counterWriteQueue full? %d\n", counter_write_length + request_size > COUNTER_WRITE_QUEUE_SIZE);
	  return counter_write_length + request_size > COUNTER_WRITE_QUEUE_SIZE;
	}

	bool one_queue_full() {
	  return (writeQueue.size() - counter_write_length
					   == writeBufferSize - COUNTER_WRITE_QUEUE_SIZE) || 
			  (counter_write_length == COUNTER_WRITE_QUEUE_SIZE);
	}
	//Korakit: from address set to address -> opt_record map
	static std::unordered_set<Addr> TXOptAddrBuffer;
	
	static std::unordered_map<std::string, opt_record> TXOptBuffer;			//indexed by opt_record's address concat with segID
	static std::unordered_map<Addr, bool> TXOptFlush2Write;
	//Korakit
	//Map for looking up by pmem Address for the actual write.
	static std::unordered_map<Addr, std::string> TXOptPmemToOptAddrMap;	//indexed by pmem address

  
	// Replaced by stats
	/*
	static uint64_t TXOptWriteCnt;
	static uint64_t nonTXOptWriteCnt;
	static uint64_t numTXOptInstr;
	static uint64_t tot_num_writes;
	*/
	// Addr getTXOptCommandPAddr(Addr txopt_vaddr) {
	// 	//DPRINTF(myflag2, "txopt_vaddr=%llx, table=%llx\n", 
	// 	//			txopt_vaddr, FuncPageTable::Virtual2Physical[txopt_vaddr/PAGE_SIZE_COMMON]);
	// 	Addr txopt_paddr;
	// 	txopt_paddr = FuncPageTable::Virtual2Physical[txopt_vaddr / PAGE_SIZE_COMMON] * PAGE_SIZE_COMMON 
	// 		+ txopt_vaddr - PAGE_SIZE_COMMON * (txopt_vaddr / PAGE_SIZE_COMMON);
	// 	return txopt_paddr;
	// }
	
	// Addr getVAddr(Addr paddr) {
	// 	Addr vaddr;
	// 	DPRINTF(myflag2, "txopt_paddr=%llx, table=%llx\n", 
	// 				paddr, FuncPageTable::Physical2Virtual[paddr/PAGE_SIZE_COMMON]);
	// 	vaddr = FuncPageTable::Physical2Virtual[paddr / PAGE_SIZE_COMMON] * PAGE_SIZE_COMMON 
	// 		+ paddr - PAGE_SIZE_COMMON * (paddr / PAGE_SIZE_COMMON);
	// 	return vaddr;
	// }

	void genDuplicate(PacketPtr pkt) {
		
		// random duplication with certain ratio
		if (rand() % 100 <= DUP_RATE) {
			pkt->isDup = true;
		}
	}

	bool isDuplicate(PacketPtr pkt) {
		return pkt->isDup;
	}

	Addr compAddr(Addr addr_init) {
#ifdef COMPRESSION
		return Addr(addr_init * COMPRESSION_RATIO);
#else
		return addr_init;
#endif
	}

  static std::ofstream myFile;
  static std::string enableNonVolatileDump;
  static void dumpTrace(PacketPtr pkt);
};

#endif //__MEM_DRAM_CTRL_HH__

import FIFO::*;
import FIFOF::*;
import BRAM::*;
import BRAMFIFO::*;
import Clocks::*;
import Vector::*;

import PcieCtrl::*;

import DRAMController::*;
import DRAMArbiter::*;

import Serializer::*;

import GibbsSampler::*;


typedef 56000 SeqNum;
typedef 1000 SeqLength;
typedef 2048 SeqSize;
typedef TMul#(SeqNum, SeqSize) DataSize;

typedef 16 MotifLength;
typedef 32 MotifSize;
typedef 64 PeNum;
typedef TMul#(SeqNum, MotifSize) DataMotifSize;
typedef TMul#(MotifSize, PeNum) MotifRelayLength;
typedef TDiv#(SeqNum, PeNum) MotifRelaySize;

typedef 1750 DmaReadSeqSize;
typedef 28 DmaReadMtfSize;
typedef TAdd#(DmaReadSeqSize, DmaReadMtfSize) DmaReadSize;

typedef TDiv#(DataSize, 512) DramWriteSeqSize;
typedef TDiv#(DataMotifSize, 512) DramWriteMtfSize;
typedef TAdd#(DramWriteSeqSize, DramWriteMtfSize) DramWriteSize;

typedef TDiv#(SeqSize, 512) DramReadSeqSize;
typedef TDiv#(DataMotifSize, 512) DramReadMtfSize;

typedef 0 DramSeqAddressStart;
typedef TDiv#(DataSize, 512) DramMtfAddressStart;


interface HwMainIfc;
endinterface
module mkHwMain#(PcieUserIfc pcie, DRAMUserIfc dram) 
	(HwMainIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	Clock pcieclk = pcie.user_clk;
	Reset pcierst = pcie.user_rst;

	// Cycle Counter
	Reg#(Bit#(32)) cycleCount <- mkReg(0);
	rule incCycleCounter;
		cycleCount <= cycleCount + 1;
	endrule
	
	// Serializer & DeSerializer
	DeSerializerIfc#(128, 4) deserializer128b512b <- mkDeSerializer;
	DeSerializerIfc#(512, 4) deserializer512b2048b <- mkDeSerializer;

	// DRAMArbiter
	DRAMArbiterIfc#(2) dramArbiter <- mkDRAMArbiter(dram);

	// Rule On/Off
	Reg#(Bool) dramWriteOn <- mkReg(False);
	Reg#(Bool) dramReadOn <- mkReg(False);
	Reg#(Bool) dramReadSeqOn <- mkReg(True);
	Reg#(Bool) dramReadMtfOn <- mkReg(False);
	Reg#(Bool) getSequenceOn <- mkReg(True);
	Reg#(Bool) getMotifOn <- mkReg(False);
	Reg#(Bool) relaySequenceOn <- mkReg(False);
	Reg#(Bool) relayMotifOn <- mkReg(False);

	// GibbsSampler
	GibbsSamplerIfc gibbsSampler <- mkGibbsSampler;

	// I/O
	FIFO#(Bit#(2000)) sequenceQ <- mkFIFO;
	FIFO#(Bit#(MotifRelayLength)) motifQ <- mkSizedBRAMFIFO(valueOf(MotifRelaySize));
	//--------------------------------------------------------------------------------------------
	// Get Commands from Host via PCIe
	//--------------------------------------------------------------------------------------------
	FIFOF#(Bit#(32)) dmaReadReqQ <- mkFIFOF;
	rule pcieDataWriter;
		let w <- pcie.dataReceive;

		let d = w.data;
		let a = w.addr;
		let off = (a >> 2);

		// Receive a request of reading DMA buffer from Host (Sequences)
		if ( off == 0 ) begin
			dmaReadReqQ.enq(d);
		end
	endrule
	FIFOF#(Bit#(32)) statusQ <- mkFIFOF;
	rule pcieDataReader;
		let r <- pcie.dataReq;

		Bit#(4) a = truncate(r.addr>>2);
		if ( a == 0 ) begin
			if ( statusQ.notEmpty ) begin
				let s = statusQ.first;
				statusQ.deq;
				pcie.dataSend(r, s);
			end else begin 
				pcie.dataSend(r, 7777);
			end
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Read DMA Buffer
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dmaReadCnt1 <- mkReg(0);
	Reg#(Bit#(32)) dmaReadCnt2 <- mkReg(0);
	rule dmaReadRequester( dmaReadReqQ.notEmpty && (dmaReadCnt1 == 0) );
		dmaReadReqQ.deq;
		let d = dmaReadReqQ.first;
		pcie.dmaReadReq(0, truncate(d));
		dmaReadCnt1 <= d;
		if ( dmaReadCnt2 == 0 ) begin
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: DMA read started!\n", cycleCount);
		end
	endrule
	rule dmaReader( dmaReadCnt1 != 0 );
		let d <- pcie.dmaReadWord;
		deserializer128b512b.put(d);
		dmaReadCnt1 <= dmaReadCnt1 - 1;
		if ( dmaReadCnt1 == 1 ) begin
			if ( dmaReadCnt2 + 1 == fromInteger(valueOf(DmaReadSize)) ) begin
				statusQ.enq(1);
				dmaReadCnt2 <= 0;
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: DMA read finished!\n", cycleCount);
			end else begin
				dmaReadCnt2 <= dmaReadCnt2 + 1;
			end
		end
		dramWriteOn <= True;
	endrule
	//--------------------------------------------------------------------------------------------
	// Store the Sequences to DRAM
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dramWriteCnt <- mkReg(0);
	rule dramWriter( dramWriteOn );
		if ( dramWriteCnt == 0 ) begin
			dramArbiter.users[0].cmd(0, fromInteger(valueOf(DramWriteSize)), True);
			dramWriteCnt <= dramWriteCnt + 1;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM write started!\n", cycleCount);
		end else begin
			let d <- deserializer128b512b.get;
			dramArbiter.users[0].write(d);
			if ( dramWriteCnt == fromInteger(valueOf(DramWriteSize)) ) begin
				dramWriteCnt <= 0;
				dramWriteOn <= False;
				dramReadOn <= True;
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM write finished!\n", cycleCount);
			end else begin
				dramWriteCnt <= dramWriteCnt + 1;
			end
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Read a Sequence and the motifs from DRAM & Send it to DeSerializer
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dramReadCnt <- mkReg(0);
	rule dramReader( dramReadOn );
		Bit#(64) dramAddressStart = 0;
		Bit#(32) dramReadSize = 0;
		if ( dramReadSeqOn ) begin
			dramAddressStart = fromInteger(valueOf(DramSeqAddressStart));
			dramReadSize = fromInteger(valueOf(DramReadSeqSize));
		end else if ( dramReadMtfOn ) begin
			dramAddressStart = fromInteger(valueOf(DramMtfAddressStart));
			dramReadSize = fromInteger(valueOf(DramReadMtfSize));
		end

		if ( dramReadCnt == 0 ) begin
			dramArbiter.users[0].cmd(dramAddressStart, dramReadSize, False);
			dramReadCnt <= dramReadCnt + 1;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM read started!\n", cycleCount);
		end else begin
			let d <- dramArbiter.users[0].read;
			deserializer512b2048b.put(d);
			if ( dramReadCnt == dramReadSize ) begin
				dramReadCnt <= 0;
				if ( dramReadSeqOn ) begin
					dramReadSeqOn <= False;
					dramReadMtfOn <= True;
				end else if ( dramReadMtfOn ) begin
					dramReadSeqOn <= True;
					dramReadMtfOn <= False;
					dramReadOn <= False;
				end
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM read finished!\n", cycleCount);
			end else begin
				dramReadCnt <= dramReadCnt + 1;
			end
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Relay a Sequence to MotifFinder
	//--------------------------------------------------------------------------------------------
	rule getSequence( getSequenceOn );
		let s <- deserializer512b2048b.get;
		sequenceQ.enq(truncate(s));
		getSequenceOn <= False;
		getMotifOn <= True;
	endrule
	rule relaySequence( relaySequenceOn );
		sequenceQ.deq;
		let s = sequenceQ.first;
		gibbsSampler.putSequence(s);
		$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Sending a sequence finished!\n", cycleCount);
	endrule
	//--------------------------------------------------------------------------------------------
	// Relay the Motifs to MotifFinder
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) getMotifCnt <- mkReg(0);
	rule getMotif( getMotifOn );
		let m <- deserializer512b2048b.get;
		motifQ.enq(m);
		if ( getMotifCnt + 1 == fromInteger(valueOf(MotifRelaySize)) ) begin
			getMotifCnt <= 0;
			getSequenceOn <= True;
			getMotifOn <= False;
			relayMotifOn <= True;
		end else begin
			getMotifCnt <= getMotifCnt + 1;
		end
	endrule
	Reg#(Bit#(32)) relayMotifCnt <- mkReg(0);
	rule relayMotif( relayMotifOn );
		motifQ.deq;
		let m = motifQ.first;
		gibbsSampler.putMotif(m);
		if ( relayMotifCnt + 1 == fromInteger(valueOf(MotifRelaySize)) ) begin
			relayMotifCnt <= 0;
			relayMotifOn <= False;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Sending motifs finished!\n", cycleCount);
		end else begin
			relayMotifCnt <= relayMotifCnt + 1;
			if ( relayMotifCnt == 0 ) begin
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Sending motifs started!\n", cycleCount);
			end
		end
	endrule
endmodule

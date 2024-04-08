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
typedef 64 PeNum;
typedef TDiv#(SeqNum, PeNum) MotifRelaySize;

typedef TDiv#(DataSize, 512) DramWriteSize;
typedef TDiv#(SeqSize, 512) DramReadSize;


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
	// For sequences
	DeSerializerIfc#(128, 4) deserializer128b512b <- mkDeSerializer;
	DeSerializerIfc#(512, 4) deserializer512b2048b <- mkDeSerializer;
	// For Motifs
	DeSerializerIfc#(120, 16) deserializer128b2048b <- mkDeSerializer;

	// DRAMArbiter
	DRAMArbiterIfc#(2) dramArbiter <- mkDRAMArbiter(dram);

	// MotifFinder
	GibbsSamplerIfc gibbsSampler <- mkGibbsSampler;
	//--------------------------------------------------------------------------------------------
	// Get Commands from Host via PCIe
	//--------------------------------------------------------------------------------------------
	FIFOF#(IOWrite) pcieDataWriterQ <- mkFIFOF;
	FIFOF#(Bit#(32)) dmaReadReqQ <- mkFIFOF;
	FIFOF#(Bit#(32)) dmaWriteReqQ <- mkFIFOF;
	rule pcieDataWriter_1;
		let w <- pcie.dataReceive;
		pcieDataWriterQ.enq(w);
	endrule
	rule pcieDataWriter_2( pcieDataWriterQ.notEmpty );
		pcieDataWriterQ.deq;
		let w = pcieDataWriterQ.first;

		let d = w.data;
		let a = w.addr;
		let off = (a >> 2);

		// Receive a request of reading DMA buffer from Host
		if ( off == 0 ) begin
			dmaReadReqQ.enq(d);
		// Receive a request of writing DMA buffer from Host
		end else if ( off == 1 ) begin
			dmaWriteReqQ.enq(d);
		end
	endrule

	FIFOF#(Bit#(32)) statusQ <- mkFIFOF;
	FIFOF#(IOReadReq) pcieDataReaderQ <- mkFIFOF;
	rule pcieDataReader_1;
		let r <- pcie.dataReq;
		pcieDataReaderQ.enq(r);
	endrule
	rule pcieDataReader_2( pcieDataReaderQ.notEmpty );
		pcieDataReaderQ.deq;
		let r = pcieDataReaderQ.first;
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
	Reg#(Bool) dmaSwitchOn <- mkReg(False);
	Reg#(Bit#(32)) dmaReadCnt <- mkReg(0);
	rule dmaReadRequester( dmaReadReqQ.notEmpty && (dmaReadCnt == 0) );
		dmaReadReqQ.deq;
		let d = dmaReadReqQ.first;
		pcie.dmaReadReq(0, truncate(d));
		dmaReadCnt <= d;
		$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: DMA read started![%1d/1]\n", cycleCount, dmaSwitchOn);
	endrule
	rule dmaReader( dmaReadCnt != 0 );
		let d <- pcie.dmaReadWord;
		if ( dmaSwitchOn ) begin
			// For motifs
			deserializer128b2048b.put(d);
		end else 
			// For sequences
			deserializer128b512b.put(d);
		end
		dmaReadCnt <= dmaReadCnt - 1;
		if ( dmaReadCnt == 1 ) begin
			dmaSwitchOn <= True;
			statusQ.enq(1);
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: DMA read finished![%1d/1]\n", cycleCount, dmaSwitchOn);
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Store the Sequences to DRAM
	// 56054 x 16 x 128bits = 56054 x 4 x 512bits
	//--------------------------------------------------------------------------------------------
	Reg#(Bool) dramWriteOn <- mkReg(True);
	Reg#(Bool) dramReadOn <- mkReg(False);
	Reg#(Bit#(32)) dramWriteCnt <- mkReg(0);
	rule dramWriter( dramWriteOn );
		if ( dramWriteCnt == 0 ) begin
			dramArbiter.users[0].cmd(0, fromInteger(valueOf(DramWriteSize)), True);
			dramWriteCnt <= dramWriteCnt + 1;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM write started!\n", cycleCount);
		end else begin
			let d <- deserializer128b512b.get;
			dramArbiter.users[0].write(d);
			if ( dramWriteCnt == DramWriteSize ) begin
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
	// Read a Sequence from DRAM & Send it to DeSerializer
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dramReadCnt <- mkReg(0);
	rule dramReader( dramReadOn );
		if ( dramReadCnt == 0 ) begin
			dramArbiter.users[0].cmd(0, fromInteger(valueOf(DramReadSize)), False);
			dramReadCnt <= dramReadCnt + 1;
			stage4 <= True;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM read started!\n", cycleCount);
		end else begin
			let d <- dramArbiter.users[0].read;
			deserializer512b2048b.put(d);
			if ( dramReadCnt == DramReadSize ) begin
				dramReadCnt <= 0;
				dramReadOn <= False;
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM read finished!\n", cycleCount);
			end else begin
				dramReadCnt <= dramReadCnt + 1;
			end
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Relay a Sequence to MotifFinder
	//--------------------------------------------------------------------------------------------
	rule relaySequence;
		let s <- deserializer512b2048b.get;
		gibbsSampler.putSequence(truncate(s));
		$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Sending a sequence finished!\n", cycleCount);
	endrule
	//--------------------------------------------------------------------------------------------
	// Relay the Motifs to MotifFinder
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) relayMotifCnt <- mkReg(0);
	rule relayMotif;
		let m <- deserializer128b2048b.get;
		gibbsSampler.putMotif(m);
		if ( relayMotifCnt + 1 == fromInteger(valueOf(MotifRelaySize)) ) begin
			relayMotifCnt <= 0;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Sending motifs finished!\n", cycleCount);
		end else begin
			relayMotifCnt <= relayMotifCnt + 1;
			if ( relayMotifCnt == 0 ) begin
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Sending motifs started!\n", cycleCount);
			end
		end
	endrule
endmodule

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


interface HwMainIfc;
endinterface
module mkHwMain#(PcieUserIfc pcie, DRAMUserIfc dram) 
	(HwMainIfc);

	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	Clock pcieclk = pcie.user_clk;
	Reset pcierst = pcie.user_rst;

	// Cycle Counter
	FIFOF#(Bit#(32)) cycleQ <- mkFIFOF;
	Reg#(Bit#(32)) cycleCount <- mkReg(0);
	Reg#(Bit#(32)) cycleStart <- mkReg(0);
	Reg#(Bit#(32)) cycleEnd <- mkReg(0);
	rule incCycleCounter;
		cycleCount <= cycleCount + 1;
	endrule
	
	// Serializer & DeSerializer
	DeSerializerIfc#(128, 4) deserializer128b512b <- mkDeSerializer;
	SerializerIfc#(444, 2) serializer444b222b <- mkSerializer;

	// DRAMArbiter
	DRAMArbiterIfc#(2) dramArbiter <- mkDRAMArbiter(dram);
	//--------------------------------------------------------------------------------------------
	// Get commands from the host via PCIe
	//--------------------------------------------------------------------------------------------
	Reg#(Bool) stage1 <- mkReg(True);
	Reg#(Bool) stage2 <- mkReg(False);
	Reg#(Bool) stage3 <- mkReg(False);
	Reg#(Bool) stage4 <- mkReg(False);
	Reg#(Bool) stage5 <- mkReg(False);

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

		// Receive a request of reading DMA buffer from the host
		if ( off == 0 ) begin
			dmaReadReqQ.enq(d);
		// Receive a request of writing DMA buffer from the host
		end else if ( off == 1 ) begin
			dmaWriteReqQ.enq(d);
		end
	endrule

	FIFOF#(Bit#(32)) statusCheckQ <- mkFIFOF;
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
			if ( cycleQ.notEmpty ) begin
				let c = cycleQ.first;
				cycleQ.deq;
				pcie.dataSend(r, c);
			end else begin 
				pcie.dataSend(r, 7777);
			end
		end else if ( a == 1 ) begin
			if ( statusCheckQ.notEmpty ) begin
				let s = statusCheckQ.first;
				statusCheckQ.deq;
				pcie.dataSend(r, s);
			end else begin
				pcie.dataSend(r, 7777);
			end
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Stage1: Read DMA buffer
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dmaReadCnt <- mkReg(0);
	rule dmaReadRequester( stage1 && dmaReadReqQ.notEmpty && (dmaReadCnt == 0) );
		dmaReadReqQ.deq;
		let d = dmaReadReqQ.first;
		pcie.dmaReadReq(0, truncate(d));
		dmaReadCnt <= d;
		stage2 <= True;
	endrule
	rule dmaReader( stage1 && (dmaReadCnt != 0) );
		let d <- pcie.dmaReadWord;
		deserializer128b512b.put(d);
		dmaReadCnt <= dmaReadCnt - 1;
		if ( dmaReadCnt == 1 ) begin
			stage1 <= False;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: DMA read finished!\n", cycleCount);
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Stage2: Store values to DRAM
	// 52 x 128bits = 13 x 512bits
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dramWriteSize <- mkReg(13);
	Reg#(Bit#(32)) dramWriteCnt <- mkReg(0);
	rule dramWriter( stage2 );
		if ( dramWriteCnt == 0 ) begin
			dramArbiter.users[0].cmd(0, dramWriteSize, True);
			dramWriteCnt <= dramWriteCnt + 1;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM write started!\n", cycleCount);
		end else begin
			let d <- deserializer128b512b.get;
			dramArbiter.users[0].write(d);
			if ( dramWriteCnt == dramWriteSize ) begin
				dramWriteCnt <= 0;
				stage2 <= False;
				stage3 <= True;
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM write finished!\n", cycleCount);
			end else begin
				dramWriteCnt <= dramWriteCnt + 1;
			end
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Stage3: Read values from DRAM & Send values to Serializer
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dramReadCnt <- mkReg(0);
	rule dramReader( stage3 );
		if ( dramReadCnt == 0 ) begin
			dramArbiter.users[0].cmd(0, dramWriteSize, False);
			dramReadCnt <= dramReadCnt + 1;
			stage4 <= True;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM read started!\n", cycleCount);
		end else begin
			let d <- dramArbiter.users[0].read;
			serializer444b222b.put(truncate(d));
			if ( dramReadCnt == dramWriteSize ) begin
				dramReadCnt <= 0;
				stage3 <= False;
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Burst DRAM read finished!\n", cycleCount);
			end else begin
				dramReadCnt <= dramReadCnt + 1;
			end
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Stage4: Take only 11 characters of each sequence
	// #sequence = 26
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(572)) subsetChar <- mkReg(0);
	Reg#(Bit#(32)) serializerCnt <- mkReg(0);
	rule serializer( stage4 );
		let s <- serializer444b222b.get();
		Bit#(22) motif_1 = truncate(s);
		Bit#(572) motif_2 = zeroExtend(motif_1);
		subsetChar <= subsetChar | (motif_2 << (22*serializerCnt));
		if ( serializerCnt + 1 == 26 ) begin
			serializerCnt <= 0;
			stage4 <= False;
			stage5 <= True;
		end else begin
			serializerCnt <= serializerCnt + 1;
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Stage5: Write chosen characters to DMA buffer
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dmaWriteCnt <- mkReg(0);
	Reg#(Bit#(32)) dmaWriteSize <- mkReg(0);
	rule dmaWriteReq( stage5 && dmaWriteReqQ.notEmpty && (dmaWriteCnt == 0) );
		dmaWriteReqQ.deq;
		let d = dmaWriteReqQ.first;
		pcie.dmaWriteReq(0, truncate(d));
		dmaWriteCnt <= d;
		dmaWriteSize <= d;
		$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: DMA write started!\n", cycleCount);
	endrule
	rule dmaWrite( stage5 && (dmaWriteCnt != 0) );
		let d = subsetChar;
		pcie.dmaWriteData(truncate(d));
		subsetChar <= (d >> 128);
		if ( dmaWriteCnt == 1 ) begin
			stage5 <= False;
			stage1 <= True;
			statusCheckQ.enq(1);
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: DMA write finished!\n", cycleCount);
		end
		dmaWriteCnt <= dmaWriteCnt - 1;
	endrule
endmodule

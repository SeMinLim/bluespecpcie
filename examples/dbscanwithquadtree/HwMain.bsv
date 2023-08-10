import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import Serializer::*;

import BRAM::*;
import BRAMFIFO::*;

import PcieCtrl::*;

import DRAMController::*;
import DRAMArbiter::*;

import FloatingPoint::*;
import Float32::*;
import Float64::*;
import Trigonometric32::*;

import Haversine::*;


typedef 1 TotalData64B;


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
	rule incCycleCount;
		cycleCount <= cycleCount + 1;
	endrule

	// Serializer & Deserializer
	SerializerIfc#(512, 8) serializer64b <- mkSerializer;
	DeSerializerIfc#(32, 16) deserializer32b <- mkDeSerializer;
	
	// DRAM Arbiter
	DRAMArbiterIfc#(2) dramArbiter <- mkDRAMArbiter(dram);

	// Haversine Module
	HaversineIfc haversine <- mkHaversine;

	// Preamble
	FpPairIfc#(32) preamble_div <- mkFpDiv32;
	Reg#(Bit#(32)) toRadian <- mkReg(0);
	Reg#(Bool) getPreambleDone <- mkReg(False);
	rule getPreamble_1( !getPreambleDone ); // toRadian = (3.1415926536 / 180)
		Bit#(32) pi = 32'b01000000010010010000111111011010;
		Bit#(32) degree = 32'b01000011001101000000000000000000;
		preamble_div.enq(pi, degree);
	endrule
	rule getPreamble_2( !getPreambleDone );
		preamble_div.deq;
		toRadian <= preamble_div.first;
		getPreambleDone <= True;
		haversine.dataInToRadian(preamble_div.first);
	endrule

	// Switches
	Reg#(Bool) dramWriterOn <- mkReg(False);
	Reg#(Bool) dramReaderOn <- mkReg(False);
	//--------------------------------------------------------------------------------------
	// Pcie Read and Write
	//--------------------------------------------------------------------------------------
	SyncFIFOIfc#(Tuple2#(IOReadReq, Bit#(32))) pcieRespQ <- mkSyncFIFOFromCC(512, pcieclk);
	SyncFIFOIfc#(IOReadReq) pcieReadReqQ <- mkSyncFIFOToCC(512, pcieclk, pcierst);
	SyncFIFOIfc#(IOWrite) pcieWriteQ <- mkSyncFIFOToCC(512, pcieclk, pcierst);
	rule getReadReq;
		let r <- pcie.dataReq;
		pcieReadReqQ.enq(r);
	endrule
	rule returnReadResp;
		let r_ = pcieRespQ.first;
		pcieRespQ.deq;
		pcie.dataSend(tpl_1(r_), tpl_2(r_));
	endrule
	rule getWriteReq;
		let w <- pcie.dataReceive;
		pcieWriteQ.enq(w);
	endrule
	//--------------------------------------------------------------------------------------------
	// Get Commands from Host via PCIe
	//--------------------------------------------------------------------------------------------
	rule getCmd;
		pcieWriteQ.deq;
		let w = pcieWriteQ.first;

		let d = w.data;
		let a = w.addr;
		let off = (a >> 2);

		if ( off == 0 ) begin
			dramWriterOn <= True;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Write data \033[1;32mstart!\033[0m\n",cycleCount);
		end else if ( off == 1 ) begin
			deserializer32b.put(d);
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// DRAM Writer
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dramWriterCnt <- mkReg(0);
	rule dramWriter( dramWriterOn );
		if ( dramWriterCnt != 0 ) begin
			let data64B <- deserializer32b.get;
			dramArbiter.users[0].write(data64B);
			if ( dramWriterCnt == fromInteger(valueOf(TotalData64B)) ) begin
				dramWriterCnt <= 0;
				dramWriterOn <= False;
				dramReaderOn <= True;
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Write data \033[1;32mdone!\033[0m\n",cycleCount);
			end else begin
				dramWriterCnt <= dramWriterCnt + 1;
			end
		end else begin
			dramArbiter.users[0].cmd(0, fromInteger(valueOf(TotalData64B)), True);
			dramWriterCnt <= dramWriterCnt + 1;
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// DRAM Reader
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) dramReaderCnt <- mkReg(0);
	rule dramReader( dramReaderOn );
		if ( dramReaderCnt != 0 ) begin
			let data <- dramArbiter.users[0].read;
			serializer64b.put(data);
			if ( dramReaderCnt == fromInteger(valueOf(TotalData64B)) ) begin
				dramReaderCnt <= 0;
				dramReaderOn <= False;
				$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Read data \033[1;32mdone!\033[0m\n",cycleCount);
			end else begin
				dramReaderCnt <= dramReaderCnt + 1;
			end
		end else begin
			dramArbiter.users[0].cmd(0, fromInteger(valueOf(TotalData64B)), False);
			dramReaderCnt <= dramReaderCnt + 1;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Read data \033[1;32mstart!\033[0m\n",cycleCount);
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Haversine Formula
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(1)) multiplexer <- mkReg(0);
	rule sendData;
		let data <- serializer64b.get;
		if ( multiplexer == 0 ) begin
			haversine.dataInPointA(data);
			multiplexer <= multiplexer + 1;
		end else begin
			haversine.dataInPointB(data);
			multiplexer <= 0;
		end
	endrule
	FIFOF#(Bit#(32)) resultQ <- mkFIFOF;
	Reg#(Bit#(32)) resultCnt <- mkReg(0);
	rule getResult;
		let r <- haversine.resultOut;
		resultQ.enq(r);
		cycleQ.enq(cycleCount);
		if ( resultCnt == 3 ) begin
			resultCnt <= 0;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Computation \033[1;32mdone!\033[0m\n",cycleCount);
		end else begin
			resultCnt <= resultCnt + 1;
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Send the result to the host
	//--------------------------------------------------------------------------------------------
	rule sendResult;
		pcieReadReqQ.deq;
		let r = pcieReadReqQ.first;
		Bit#(4) a = truncate(r.addr>>2);
		if ( a == 0 ) begin
			if ( resultQ.notEmpty ) begin
				pcieRespQ.enq(tuple2(r, resultQ.first));
				resultQ.deq;
			end else begin
				pcieRespQ.enq(tuple2(r, 32'hffffffff));
			end
		end else if ( a == 1 ) begin
			if ( cycleQ.notEmpty ) begin
				pcieRespQ.enq(tuple2(r, cycleQ.first));
				cycleQ.deq;
			end else begin
				pcieRespQ.enq(tuple2(r, 32'hffffffff));
			end
		end
	endrule
endmodule

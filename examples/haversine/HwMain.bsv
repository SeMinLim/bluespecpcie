import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import PcieCtrl::*;

import FloatingPoint::*;
import Float32::*;
import Float64::*;
import Trigonometric32::*;

import Haversine::*;


interface HwMainIfc;
endinterface
module mkHwMain#(PcieUserIfc pcie) 
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
			haversine.dataInPointALat(d);
		end else if ( off == 1 ) begin
			haversine.dataInPointALon(d);
		end else if ( off == 2 ) begin
			haversine.dataInPointBLat(d);
		end else if ( off == 3 ) begin
			haversine.dataInPointBLon(d);
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Computation \033[1;32mstart!\033[0m\n", cycleCount);
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Haversine Formula
	//--------------------------------------------------------------------------------------------
	FIFOF#(Bit#(32)) resultQ <- mkSizedFIFOF(16);
	Reg#(Bit#(32)) resultCnt <- mkReg(0);
	rule getResult;
		let r <- haversine.resultOut;
		resultQ.enq(r);
		cycleQ.enq(cycleCount);
		if ( resultCnt == 3 ) begin
			resultCnt <= 0;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: All Computation \033[1;32mdone!\033[0m\n",cycleCount);
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

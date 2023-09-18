import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import FloatingPoint::*;
import Float32::*;


interface ManhattanIfc;
	method Action dataInPointALat(Bit#(32) d);
	method Action dataInPointALon(Bit#(32) d);
	method Action dataInPointBLat(Bit#(32) d);
	method Action dataInPointBLon(Bit#(32) d);
	method ActionValue#(Bit#(32)) resultOut;
endinterface
(* synthesize *)
module mkManhattan(ManhattanIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	// Cycle Counter
	FIFOF#(Bit#(32)) cycleQ <- mkFIFOF;
	Reg#(Bit#(32)) cycleCount <- mkReg(0);
	Reg#(Bit#(32)) cycleStart <- mkReg(0);
	Reg#(Bit#(32)) cycleEnd <- mkReg(0);
	rule incCycleCount;
		cycleCount <= cycleCount + 1;
	endrule
	//--------------------------------------------------------------------------------------------
	// Get data from HwMain & Send result to HwMain
	//--------------------------------------------------------------------------------------------
	FIFO#(Bit#(32)) pointALat <- mkFIFO;
	FIFO#(Bit#(32)) pointALon <- mkFIFO;
	FIFO#(Bit#(32)) pointBLat <- mkFIFO;
	FIFO#(Bit#(32)) pointBLon <- mkFIFO;
	FIFO#(Bit#(32)) resultQ <- mkFIFO;
	Reg#(Bit#(32)) resultCnt <- mkReg(0);
	//--------------------------------------------------------------------------------------------
	// Manhattan
	//--------------------------------------------------------------------------------------------
	Vector#(2, FpPairIfc#(32)) subtraction <- replicateM(mkFpSub32);
	Vector#(2, FIFO#(Bit#(32))) absolute <- replicateM(mkFIFO);
	FpPairIfc#(32) addition <- mkFpAdd32;

	// dlat & dlon
	rule sub_lat;
		pointALat.deq;
		pointBLat.deq;
		subtraction[0].enq(pointALat.first, pointBLat.first);
	endrule
	rule sub_lon;
		pointALon.deq;
		pointBLon.deq;
		subtraction[1].enq(pointALon.first, pointBLon.first);
	endrule
	// Absolute of dlat and dlon
	rule absolute_lat;
		subtraction[0].deq;
		let dlat = subtraction[0].first;
		dlat[31] = 0;
		absolute[0].enq(dlat);
	endrule
	rule absolute_lon;
		subtraction[1].deq;
		let dlon = subtraction[1].first;
		dlon[31] = 0;
		absolute[1].enq(dlon);
	endrule
	// Add
	rule add_dlat_dlon;
		absolute[0].deq;
		absolute[1].deq;
		addition.enq(absolute[0].first, absolute[1].first);
	endrule
	// Final
	rule final_result;
		addition.deq;
		resultQ.enq(addition.first);
		resultCnt <= resultCnt + 1;
		$write("\033[1;33mCycle %1d -> \033[1;33m[Manhattan]: \033[0m: Computation \033[1;32mdone!\033[0m[%1d]\n",
			cycleCount,resultCnt);	
	endrule

	method Action dataInPointALat(Bit#(32) d);
		pointALat.enq(d);
	endmethod
	method Action dataInPointALon(Bit#(32) d);
		pointALon.enq(d);
	endmethod
	method Action dataInPointBLat(Bit#(32) d);
		pointBLat.enq(d);
	endmethod
	method Action dataInPointBLon(Bit#(32) d);
		pointBLon.enq(d);
	endmethod
	method ActionValue#(Bit#(32)) resultOut;
		resultQ.deq;
		return resultQ.first;
	endmethod
endmodule

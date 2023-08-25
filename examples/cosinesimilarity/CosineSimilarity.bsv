import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import FloatingPoint::*;
import Float32::*;
import Float64::*;
import Trigonometric32::*;


interface CosineSimilarityIfc;
	method Action dataInPointALat(Bit#(32) d);
	method Action dataInPointALon(Bit#(32) d);
	method Action dataInPointBLat(Bit#(32) d);
	method Action dataInPointBLon(Bit#(32) d);
	method ActionValue#(Bit#(32)) resultOut;
endinterface
(* synthesize *)
module mkCosineSimilarity(CosineSimilarityIfc);
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
	// Cosine Similarity
	//--------------------------------------------------------------------------------------------
	Vector#(4, FpPairIfc#(32)) power2 <- replicateM(mkFpMult32);
	
	Vector#(2, FpPairIfc#(32)) dotProduct_mult <- replicateM(mkFpMult32);
	FpPairIfc#(32) dotProduct_add <- mkFpAdd32;

	Vector#(2, FpPairIfc#(32)) magnitude_add <- replicateM(mkFpAdd32);
	Vector#(2, FpFilterIfc#(32)) magnitude_sqrt <- replicateM(mkFpSqrt32);
	FpPairIfc#(32) magnitude_mult <- mkFpMult32;

	FpPairIfc#(32) final_div <- mkFpDiv32;

	// A * B (Vector Dot Product)
	rule dotProductLat_multiplication;
		pointALat.deq;
		pointBLat.deq;
		dotProduct_mult[0].enq(pointALat.first, pointBLat.first);
	endrule
	rule dotProductLon_multiplication;
		pointALon.deq;
		pointBLon.deq;
		dotProduct_mult[1].enq(pointALon.first, pointBLon.first);
	endrule
	rule dotProduct_addition;
		dotProduct_mult[0].deq;
		dotProduct_mult[1].deq;
		let lat = dotProduct_mult[0].first;
		let lon = dotProduct_mult[1].first;
		dotProduct_add.enq(lat, lon);
	endrule

	// |A|
	rule magnitudeA_addition;
		power2[0].deq; // Lat^2
		power2[1].deq; // Lon^2
		magnitude_add[0].enq(power2[0].first, power2[1].first);
	endrule
	rule magnitudeLat_squareroot;
		magnitude_sqrt[0].enq(magnitude_add[0].first);
	endrule
	// |B|
	rule magnitudeB_addition;
		power2[2].deq; // Lat^2
		power2[3].deq; // Lon^2
		magnitude_add[1].enq(power2[2].first, power2[3].first);
	endrule
	rule magnitudeLon_squareroot;
		magnitude_sqrt[1].enq(magnitude_add[1].first);
	endrule
	// |A| * |B|
	rule magProduct_multiplication;
		magnitude_sqrt[0].deq;
		magnitude_sqrt[1].deq;
		let a = magnitude_sqrt[0].first;
		let b = magnitude_sqrt[1].first;
		magnitude_mult.enq(a, b);	
	endrule

	// (X * Y) / (|X| * |Y|)
	rule final_division;
		dotProduct_add.deq;
		let first = dotProduct_add.first;
		magnitude_mult.deq;
		let second = magnitude_mult.first;
		final_div.enq(first, second);
	endrule
	rule final_result;
		final_div.deq;
		resultQ.enq(final_div.first);
		resultCnt <= resultCnt + 1;
		$write("\033[1;33mCycle %1d -> \033[1;33m[CosSim]: \033[0m: Computation \033[1;32mdone!\033[0m[%1d]\n",
			cycleCount,resultCnt);	
	endrule

	method Action dataInPointALat(Bit#(32) d);
		pointALat.enq(d);
		power2[0].enq(d, d);
	endmethod
	method Action dataInPointALon(Bit#(32) d);
		pointALon.enq(d);
		power2[1].enq(d, d);
	endmethod
	method Action dataInPointBLat(Bit#(32) d);
		pointBLat.enq(d);
		power2[2].enq(d, d);
	endmethod
	method Action dataInPointBLon(Bit#(32) d);
		pointBLon.enq(d);
		power2[3].enq(d, d);
	endmethod
	method ActionValue#(Bit#(32)) resultOut;
		resultQ.deq;
		return resultQ.first;
	endmethod
endmodule

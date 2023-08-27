import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import FloatingPoint::*;
import Float32::*;

import Cordic::*;

import TypeConverter::*;


interface ArcSinIfc;
	method Action enq(Bit#(32) data);
	method Action deq;
	method Bit#(16) first;
endinterface
(* synthesize *)
module mkArcSin(ArcSinIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	FIFO#(Bit#(32)) inputQ <- mkFIFO;
	FIFO#(Bit#(32)) inputCopyQ <- mkFIFO;
	FIFO#(Bit#(16)) outputQ <- mkFIFO;
	
	FpPairIfc#(32) mult <- mkFpMult32;
	FpPairIfc#(32) sub <- mkFpSub32;
	FpPairIfc#(32) div <- mkFpDiv32;
	FpFilterIfc#(32) sqrt <- mkFpSqrt32;

	FloatToFixed2IntIfc floattofixed2i <- mkFloatToFixed2Int;
	CordicAtanIfc atan <- mkCordicAtan;

	rule operand_1;
		inputQ.deq;
		let x = inputQ.first;

		inputCopyQ.enq(x);
		mult.enq(x, x);
	endrule
	rule operand_2;
		mult.deq;
		let x = mult.first;
		Bit#(32) numOne = 32'b00111111100000000000000000000000;

		sub.enq(numOne, x);
	endrule
	rule operand_3;
		sub.deq;
		let x = sub.first;

		sqrt.enq(x);
	endrule
	rule operand_4;
		sqrt.deq;
		inputCopyQ.deq;
		let x1 = sqrt.first;
		let x2 = inputCopyQ.first;

		div.enq(x2, x1);
	endrule
	rule operand_5;
		div.deq;
		floattofixed2i.enq(div.first);
	endrule
	rule arctan;
		floattofixed2i.deq;
		let x = floattofixed2i.first;
		Bit#(16) numOne = 16'b0100000000000000;

		atan.enq(x, numOne);
	endrule
	rule finalResult;
		atan.deq;
		outputQ.enq(atan.first);
	endrule

	method Action enq(Bit#(32) data);
		inputQ.enq(data);
	endmethod
	method Action deq;
		outputQ.deq;
	endmethod
	method Bit#(16) first;
		return outputQ.first;
	endmethod
endmodule

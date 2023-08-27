import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import FloatingPoint::*;
import Float32::*;


interface FixedToFloat2IntIfc;
	method Action enq(Bit#(16) data);
	method Action deq;
	method Bit#(32) first;
endinterface
(* synthesize *)
module mkFixedToFloat2Int(FixedToFloat2IntIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	FIFO#(Bit#(16)) fixedQ <- mkFIFO;
	FIFO#(Bit#(32)) floatQ <- mkFIFO;

	rule converter;
		fixedQ.deq;
		let fixed16b = fixedQ.first;

		Bit#(32) float;
		Bit#(1) sign = fixed16b[15];
		if ( sign == 1 ) begin // Negative
			Bit#(16) fixed16bNeg = (~fixed16b + 1);
			float = (zeroExtend(fixed16bNeg) >> 14);
			float[31] = 1;
		end else begin // Positive
			float = (zeroExtend(fixed16b) >> 14);
		end
		
		floatQ.enq(float);
	endrule

	method Action enq(Bit#(16) data);
		fixedQ.enq(data);
	endmethod
	method Action deq;
		floatQ.deq;
	endmethod
	method Bit#(32) first;
		return floatQ.first;
	endmethod
endmodule


interface FloatToFixed3IntIfc;
	method Action enq(Bit#(32) data);
	method Action deq;
	method Bit#(16) first;
endinterface
(* synthesize *)
module mkFloatToFixed3Int(FloatToFixed3IntIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	FIFO#(Bit#(32)) floatQ <- mkFIFO;
	FIFO#(Bit#(16)) fixedQ <- mkFIFO;

	rule converter;
		floatQ.deq;
		let float = floatQ.first;
		let sign = float[31];
		let exponent = float[30:23];
		let mantissa = float[22:0];

		Bit#(16) fixed = 0;
		
		// Sign
		fixed[15] = sign;

		// Integer
		if ( exponent > 127 ) begin
			Bit#(24) fractionTemp1 = 1 << 23;
			Bit#(24) fractionTemp2 = zeroExtend(mantissa) | fractionTemp1;
			Bit#(22) fractionTemp3 = truncate(fractionTemp2);
			fixed[12:0] = truncateLSB(fractionTemp3);
			fixed[14:13] = fractionTemp2[23:22];
		end else begin
			Bit#(8) move = 127 - exponent - 1;
			Bit#(32) fractionTemp1 = (1 << 31) >> move;
			Bit#(32) fractionTemp2 = zeroExtend(mantissa) << (8 - move);
			Bit#(32) fractionTemp3 = fractionTemp1 | fractionTemp2;
			fixed[12:0] = truncateLSB(fractionTemp3);
			fixed[14:13] = 0;
		end
		
		fixedQ.enq(fixed);
	endrule

	method Action enq(Bit#(32) data);
		floatQ.enq(data);
	endmethod
	method Action deq;
		fixedQ.deq;
	endmethod
	method Bit#(16) first;
		return fixedQ.first;
	endmethod
endmodule


interface FloatToFixed2IntIfc;
	method Action enq(Bit#(32) data);
	method Action deq;
	method Bit#(16) first;
endinterface
(* synthesize *)
module mkFloatToFixed2Int(FloatToFixed2IntIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	FIFO#(Bit#(32)) floatQ <- mkFIFO;
	FIFO#(Bit#(16)) fixedQ <- mkFIFO;

	rule converter;
		floatQ.deq;
		let float = floatQ.first;
		let sign = float[31];
		let exponent = float[30:23];
		let mantissa = float[22:0];

		Bit#(16) fixed = 0;
		
		// Sign
		fixed[15] = sign;

		// Integer
		if ( exponent > 127 ) begin
			Bit#(24) fractionTemp1 = 1 << 23;
			Bit#(24) fractionTemp2 = zeroExtend(mantissa) | fractionTemp1;
			Bit#(22) fractionTemp3 = truncate(fractionTemp2);
			fixed[13:0] = truncateLSB(fractionTemp3);
			fixed[14] = fractionTemp2[23];
		end else begin
			Bit#(8) move = 127 - exponent - 1;
			Bit#(32) fractionTemp1 = (1 << 31) >> move;
			Bit#(32) fractionTemp2 = zeroExtend(mantissa) << (8 - move);
			Bit#(32) fractionTemp3 = fractionTemp1 | fractionTemp2;
			fixed[13:0] = truncateLSB(fractionTemp3);
			fixed[14] = 0;
		end
		
		fixedQ.enq(fixed);
	endrule

	method Action enq(Bit#(32) data);
		floatQ.enq(data);
	endmethod
	method Action deq;
		fixedQ.deq;
	endmethod
	method Bit#(16) first;
		return fixedQ.first;
	endmethod
endmodule

import FIFO::*;
import FIFOF::*;
import BRAM::*;
import BRAMFIFO::*;
import Clocks::*;
import Vector::*;
import GetPut::*;

import FloatingPoint::*;
import Float32::*;
import TypeConverter::*;


interface RandomGeneratorIntIfc;
	method Action req;
	method ActionValue#(Bit#(32)) get;
endinterface
module mkRandomGeneratorInt(RandomGeneratorIntIfc);
	// I/O
	FIFO#(Bit#(1)) reqQ <- mkFIFO;
	FIFO#(Bit#(32)) resultQ <- mkFIFO;

	Reg#(Bit#(16)) lfsr <- mkReg(16'hACE1);
        rule genRanNum;
		reqQ.deq;
                Bit#(16) b = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
		Bit#(16) l = (lfsr >> 1) | (b << 15);
		resultQ.enq(zeroExtend(l));
		lfsr <= l;
        endrule


	method Action req;
		reqQ.enq(1);
	endmethod
	method ActionValue#(Bit#(32)) get;
		resultQ.deq;
		return resultQ.first;
	endmethod
endmodule


interface RandomGeneratorFpIfc;
	method Action req;
	method ActionValue#(Bit#(32)) get;
endinterface
module mkRandomGeneratorFp(RandomGeneratorFpIfc);
	// Required Modules
	Vector#(2, UINTtoFLOATIfc) typeConverter <- replicateM(mkUINTtoFLOAT);
	FpPairIfc#(32) fpDiv <- mkFpDiv32;

	// I/O
	FIFO#(Bit#(1)) reqQ <- mkFIFO;
	FIFO#(Bit#(32)) resultQ <- mkFIFO;

	Reg#(Bit#(16)) lfsr <- mkReg(16'h1ECA);
        rule genRanNum_1;
		reqQ.deq;
                Bit#(16) b = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
                Bit#(16) l = (lfsr >> 1) | (b << 15);
		typeConverter[0].enq(zeroExtend(l));
		typeConverter[1].enq(65535);
		lfsr <= l;
        endrule
	rule genRanNum_2;
		let x <- typeConverter[0].get();
		let y <- typeConverter[1].get();
		fpDiv.enq(x, y);	
	endrule
	rule genRanNum_3;
		fpDiv.deq;
		let r = fpDiv.first;
		resultQ.enq(r);
	endrule


	method Action req;
		reqQ.enq(1);
	endmethod
	method ActionValue#(Bit#(32)) get;
		resultQ.deq;
		return resultQ.first;
	endmethod
endmodule

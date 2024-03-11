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


interface RandomGeneratorIntIfc#(numeric type min, numeric type max);
	method Action req;
	method ActionValue#(Bit#(32)) get;
endinterface
module mkRandomGeneratorInt(RandomGeneratorIntIfc#(min, max));
	Reg#(Bool) rgOn <- mkReg(False);
	Reg#(Bit#(16)) lfsr <- mkReg(16'hACE1);
        FIFOF#(Bit#(32)) resultQ <- mkSizedBRAMFIFOF(32);
        rule genRanNum( rgOn );
                Bit#(16) b = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
                lfsr <= (lfsr >> 1) | (b << 15);
                Bit#(16) randNum = (lfsr % fromInteger((valueOf(max) - valueOf(min))));
		resultQ.enq(zeroExtend(randNum));
		rgOn <= False;
        endrule


	method Action req;
		rgOn <= True;
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
	Vector#(2, UINTtoFLOATIfc) typeConverter <- replicateM(mkUINTtoFLOAT);
	FpPairIfc#(32) fpDiv <- mkFpDiv32;

	Reg#(Bool) rgOn_1 <- mkReg(False);
	Reg#(Bool) rgOn_2 <- mkReg(False);
	Reg#(Bit#(16)) lfsr <- mkReg(16'hACE1);
        FIFOF#(Bit#(32)) resultQ <- mkSizedBRAMFIFOF(32);
        rule genRanNum_1( rgOn_1 );
                Bit#(16) b = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
                lfsr <= (lfsr >> 1) | (b << 15);
                Bit#(16) randNum = (lfsr % fromInteger(32767));
		typeConverter[0].enq(zeroExtend(randNum));
		typeConverter[1].enq(32767);
		rgOn_1 <= False;
		rgOn_2 <= True;
        endrule
	rule genRanNum_2_1( rgOn_2 );
		let x <- typeConverter[0].get();
		let y <- typeConverter[1].get();
		fpDiv.enq(x, y);	
	endrule
	rule genRanNum_2_2( rgOn_2 );
		fpDiv.deq;
		let r = fpDiv.first;
		resultQ.enq(r);
		rgOn_2 <= False;
	endrule


	method Action req;
		rgOn_1 <= True;
	endmethod
	method ActionValue#(Bit#(32)) get;
		resultQ.deq;
		return resultQ.first;
	endmethod

endmodule

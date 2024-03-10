import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import FloatingPoint::*;
import Float32::*;


interface UINTtoFLOATIfc;
	method Action enq(Bit#(32) a);
	method ActionValue#(Bit#(32)) get;
endinterface
(* synthesize *)
module mkUINTtoFLOAT(UINTtoFLOATIfc);
	FIFO#(Bit#(32)) uintQ <- mkFIFO;
	FIFO#(Bit#(32)) floatQ <- mkFIFO;

	Reg#(Bit#(32)) uint <- mkReg(0);
        Reg#(Bit#(8)) tcCnt <- mkReg(31);
        rule typeConverter;
		Bit#(32) uIntNum = 0;
		if ( tcCnt == 31 ) begin
			uintQ.deq;
			let u = uintQ.first;
			uint <= u;
			uIntNum = u;
		end else begin
			uIntNum = uint;
		end

                if ( uIntNum[tcCnt]  == 1'b1 ) begin
                        Bit#(1) sign = 0;
                        Bit#(8) exponent = tcCnt + 127;
                        Bit#(23) fraction = truncateLSB((uIntNum) << (32-tcCnt));
                        Bit#(32) rslt = {sign, exponent, fraction};
                        floatQ.enq(rslt);
                        tcCnt <= 31;
                end else begin
                        tcCnt <= tcCnt - 1;
                end
        endrule

	method Action enq(Bit#(32) a);
		uintQ.enq(a);
	endmethod
	method ActionValue#(Bit#(32)) get;
		floatQ.deq;
		return floatQ.first;
	endmethod
endmodule

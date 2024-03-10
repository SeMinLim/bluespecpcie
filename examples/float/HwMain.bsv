import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import PcieCtrl::*;

import FloatingPoint::*;
import Float32::*;
import TypeConverter::*;


interface HwMainIfc;
endinterface
module mkHwMain#(PcieUserIfc pcie) (HwMainIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	Clock pcieclk = pcie.user_clk;
	Reset pcierst = pcie.user_rst;

	FpPairIfc#(32) fpAdd <- mkFpAdd32;
	FpPairIfc#(32) fpSub <- mkFpSub32;
	FpPairIfc#(32) fpMul <- mkFpMult32;
	FpPairIfc#(32) fpDiv <- mkFpDiv32;
	UINTtoFLOATIfc typeConverter <- mkUINTtoFLOAT;

	// I/O
	FIFO#(Bit#(32)) resultQ_1 <- mkFIFO;
	FIFO#(Bit#(32)) resultQ_2 <- mkFIFO;
	FIFO#(Bit#(32)) resultQ_3 <- mkFIFO;
	FIFO#(Bit#(32)) resultQ_4 <- mkFIFO;
	FIFO#(Bit#(32)) resultQ_5 <- mkFIFO;
	FIFO#(Bit#(32)) resultQ_6 <- mkFIFO;
	Reg#(Bool) systemOn_1 <- mkReg(False);
	Reg#(Bool) systemOn_2 <- mkReg(False);
	Reg#(Bit#(32)) i <- mkReg(0);
	Reg#(Bit#(32)) j <- mkReg(0);
	Reg#(Bit#(32)) k <- mkReg(0);
	rule echoRead;
		let r <- pcie.dataReq;
		let a = r.addr;
		let offset = (a>>2);
		
		if ( offset == 0 ) begin 
			resultQ_1.deq;
			pcie.dataSend(r, resultQ_1.first);
		end else if ( offset == 1 ) begin
			resultQ_2.deq;
			pcie.dataSend(r, resultQ_2.first);
		end else if ( offset == 2 ) begin
			resultQ_3.deq;
			pcie.dataSend(r, resultQ_3.first);
		end else if ( offset == 3 ) begin
			resultQ_4.deq;
			pcie.dataSend(r, resultQ_4.first);
		end else if ( offset == 4 ) begin
			resultQ_5.deq;
			pcie.dataSend(r, resultQ_5.first);
		end else if ( offset == 5 ) begin
			resultQ_6.deq;
			pcie.dataSend(r, resultQ_6.first);
		end
	endrule
	rule recvWrite;
		let w <- pcie.dataReceive;
		let a = w.addr;
		let d = w.data;
		let off = (a>>2);
		
		if ( off == 0 ) begin
			i <= d;
		end else if ( off == 1 ) begin
			j <= d;
			systemOn_1 <= True;
		end else if ( off == 2 ) begin
			k <= d;
			systemOn_2 <= True;
		end
	endrule

	// Floating-Point Operation
	rule fpOp_1( systemOn_1 );
		fpAdd.enq(i, j);
		fpSub.enq(i, j);
		fpMul.enq(i, j);
		fpDiv.enq(i, j);
	endrule
	rule fpOp_2( systemOn_1 );
		fpAdd.deq;
		fpSub.deq;
		fpMul.deq;
		fpDiv.deq;
		let r_1 = fpAdd.first;
		let r_2 = fpSub.first;
		let r_3 = fpMul.first;
		let r_4 = fpDiv.first;
		resultQ_1.enq(r_1);
		resultQ_2.enq(r_2);
		resultQ_3.enq(r_3);
		resultQ_4.enq(r_4);
		if ( r_2 > 0 ) resultQ_5.enq(i);
		else resultQ_5.enq(j);
		systemOn_1 <= False;
	endrule

	// Type Converter
	rule convType_1( systemOn_2 );
		typeConverter.enq(k);
	endrule
	rule convType_2( systemOn_2 );
		let f <- typeConverter.get();
		resultQ_6.enq(f);
		systemOn_2 <= False;
	endrule
endmodule

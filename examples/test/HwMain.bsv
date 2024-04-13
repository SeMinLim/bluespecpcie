import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import PcieCtrl::*;


typedef 10 Qsz;


interface HwMainIfc;
endinterface
module mkHwMain#(PcieUserIfc pcie) (HwMainIfc);
	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	Clock pcieclk = pcie.user_clk;
	Reset pcierst = pcie.user_rst;

	// I/O
	FIFO#(Bit#(32)) resultQ_1 <- mkSizedBRAMFIFO(valueOf(Qsz));
	FIFO#(Bit#(32)) resultQ_2 <- mkFIFO;
	Reg#(Bool) systemOn_1 <- mkReg(False);
	Reg#(Bool) systemOn_2_1 <- mkReg(False);
	Reg#(Bool) systemOn_2_2 <- mkReg(False);
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
		end
	endrule
	rule recvWrite;
		let w <- pcie.dataReceive;
		let a = w.addr;
		let d = w.data;
		let off = (a>>2);
		
		if ( off == 0 ) begin
			systemOn_1 <= True;
		end else if ( off == 1 ) begin
			systemOn_2_1 <= True;
		end
	endrule
/*
	// Test 1
	Reg#(Bit#(32)) x <- mkReg(0);
	rule test_1_1( systemOn_1 );
		for ( Integer i = 0; i < 26; i = i + 1 ) begin
			x <= x + 1;
		end

		systemOn_1 <= False;
	endrule
	rule test_1_2;
		if ( x == 26 ) resultQ_1.enq(1);
		else resultQ_1.enq(0);
	endrule
*/
	// Test 2
	Reg#(Vector#(2, Vector#(2, Bit#(32)))) y <- mkReg(replicate(replicate(0)));
	rule test_2_1( systemOn_2_1 );
		y[0][0] <= 1;

		systemOn_2_1 <= False;
		systemOn_2_2 <= True;
	endrule
	rule test_2_2( systemOn_2_2 );
		if ( y[0][0] == 1 ) resultQ_2.enq(1);
		else resultQ_2.enq(0);
		Bit#(4000) a = 0;
		Bit#(4000) b = 1;
		$write( "%1d\n", a+b );
	endrule
endmodule

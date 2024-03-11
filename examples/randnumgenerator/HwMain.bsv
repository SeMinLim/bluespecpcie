import FIFO::*;
import FIFOF::*;
import BRAM::*;
import BRAMFIFO::*;
import Clocks::*;
import Vector::*;

import PcieCtrl::*;

import RandomGenerator::*;


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
	rule incCycleCounter;
		cycleCount <= cycleCount + 1;
	endrule

	// Random Number Generator
	RandomGeneratorIntIfc#(0, 25) randNumGenInt <- mkRandomGeneratorInt;
	RandomGeneratorFpIfc randNumGenFp <- mkRandomGeneratorFp;
	//--------------------------------------------------------------------------------------------
	// Get commands from the host via PCIe
	//--------------------------------------------------------------------------------------------
	Reg#(Bool) systemOn_1 <- mkReg(False);
	Reg#(Bool) systemOn_2 <- mkReg(False);

	FIFOF#(IOWrite) pcieDataWriterQ <- mkFIFOF;
	rule pcieDataWriter_1;
		let w <- pcie.dataReceive;
		pcieDataWriterQ.enq(w);
	endrule
	rule pcieDataWriter_2( pcieDataWriterQ.notEmpty );
		pcieDataWriterQ.deq;
		let w = pcieDataWriterQ.first;

		let d = w.data;
		let a = w.addr;
		let off = (a >> 2);

		if ( off == 0 ) begin
			systemOn_1 <= True;
		end
	endrule

	FIFOF#(Bit#(32)) resultQ <- mkSizedBRAMFIFOF(50);
	FIFOF#(IOReadReq) pcieDataReaderQ <- mkFIFOF;
	rule pcieDataReader_1;
		let r <- pcie.dataReq;
		pcieDataReaderQ.enq(r);
	endrule
	rule pcieDataReader_2( pcieDataReaderQ.notEmpty );
		pcieDataReaderQ.deq;
		let r = pcieDataReaderQ.first;
		Bit#(4) a = truncate(r.addr>>2);
		if ( a == 0 ) begin
			if ( cycleQ.notEmpty ) begin
				let c = cycleQ.first;
				cycleQ.deq;
				pcie.dataSend(r, c);
			end else begin 
				pcie.dataSend(r, 7777);
			end
		end else if ( a == 1 ) begin
			if ( resultQ.notEmpty ) begin
				let v = resultQ.first;
				resultQ.deq;
				pcie.dataSend(r, v);
			end else begin
				pcie.dataSend(r, 7777);
			end
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Random Number Generator
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) rgCnt <- mkReg(0);
	rule genRandNumInt_1( systemOn_1 );
		randNumGenInt.req;
	endrule
	rule genRandNumInt_2( systemOn_1 );
		let r <- randNumGenInt.get();
		$write("Random Number (Int): %1d\n", r);
		if ( rgCnt + 1 == 50 ) begin
			rgCnt <= 0;
			systemOn_1 <= False;
			systemOn_2 <= True;
		end else begin
			rgCnt <= rgCnt + 1;
		end
	endrule

	rule genRandNumFp_1( systemOn_2 );
		randNumGenFp.req;
	endrule
	rule genRandNumFp_2( systemOn_2 );
		let r <- randNumGenFp.get();
		resultQ.enq(r);
		if ( rgCnt + 1 == 50 ) begin
			rgCnt <= 0;
			systemOn_2 <= False;
		end else begin
			rgCnt <= rgCnt + 1;
		end
	endrule
endmodule

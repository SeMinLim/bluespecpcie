import FIFO::*;
import FIFOF::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import ScoreCalculator::*;
//import PssmMaker::*;
import Profiler::*;


typedef 56000 SeqNum;
typedef 1000 SeqLength;
typedef 2048 SeqSize;
typedef TMul#(SeqNum, SeqSize) DataSize;

typedef 16 MotifLength;
typedef 64 PeNum;
typedef TDiv#(SeqNum, PeNum) MotifRelaySize;


interface GibbsSamplerIfc;
	method Action putSequence(Bit#(2000) s);
	method Action putMotif(Bit#(2048) m);
endinterface
(* synthesize *)
module mkGibbsSampler(GibbsSamplerIfc);
	// Cycle Counter
	Reg#(Bit#(32)) cycleCount <- mkReg(0);
	rule incCycleCounter;
		cycleCount <= cycleCount + 1;
	endrule

	// Score Calculator
	ScoreCalculatorIfc scoreCalculator <- mkScoreCalculator;

	// PSSM Maker
//	PssmMakerIfc pssmMaker <- mkPssmMaker;

	// Profiler
	ProfilerIfc profiler <- mkProfiler;

	// I/O
	FIFO#(Bit#(2000)) sequenceQ <- mkFIFO;
	FIFO#(Bit#(2048)) motifQ <- mkSizedBRAMFIFO(valueOf(MotifRelaySize));
	//--------------------------------------------------------------------------------------------
	// Get PSSM First
	//--------------------------------------------------------------------------------------------
	Reg#(Bit#(32)) makePssmCnt <- mkReg(0);
	rule makePssm;
		motifQ.deq;
		let m = motifQ.first;
//		pssmMaker.putMotif(m);
		scoreCalculator.putMotifUnchanged(m);
		if ( makePssmCnt + 1 == fromInteger(valueOf(MotifRelaySize)) ) begin
			makePssmCnt <= 0;
		end else begin
			if ( makePssmCnt == 0 ) begin
				$write("\033[1;33mCycle %1d -> \033[1;33m[GibbsSampler]: \033[0m: GibbsSampler Started!\n", cycleCount);
			end
			makePssmCnt <= makePssmCnt + 1;
		end
	endrule

	rule relaySequenceToProfiler;
		sequenceQ.deq;
		let s = sequenceQ.first;
		profiler.putSequence(s);
		$write("\033[1;33mCycle %1d -> \033[1;33m[GibbsSampler]: \033[0m: Sending a sequence to Profiler finished!\n", cycleCount);
	endrule
	Reg#(Bool) stop <- mkReg(True);
	rule relayPssmToProfiler( stop ); // 925 cyckes
//		let p <- pssmMaker.get;
		Vector#(16, Vector#(4, Bit#(32))) p = replicate(replicate(0));
		profiler.putPssm(p);
		$write("\033[1;33mCycle %1d -> \033[1;33m[GibbsSampler]: \033[0m: Sending PSSM to Profiler finished!\n", cycleCount);
	endrule

	rule calScore; // 487 cycles
		let m <- profiler.get;
		scoreCalculator.putMotifChanged(m);
		$write("\033[1;33mCycle %1d -> \033[1;33m[GibbsSampler]: \033[0m: Sending a updated motif to ScoreCalculator finished!\n", cycleCount);
	endrule

	rule getResult; // 485 cycles 
		let s <- scoreCalculator.get;
		$write("\033[1;33mCycle %1d -> \033[1;33m[GibbsSampler]: \033[0m: GibbsSampler finished!\n", cycleCount);
	endrule


	method Action putSequence(Bit#(2000) s);
		sequenceQ.enq(s);
	endmethod
	method Action putMotif(Bit#(2048) m);
		motifQ.enq(m);
	endmethod
endmodule

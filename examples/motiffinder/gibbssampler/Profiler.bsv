import FIFO::*;
import FIFOF::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import FloatingPoint::*;
import Float32::*;

import RandomGenerator::*;

import ProfilerPhase1::*;
import ProfilerPhase2::*;


typedef 56000 SeqNum;
typedef 1000 SeqLength;
typedef 2048 SeqSize;

typedef 16 MotifLength;
typedef 32 PeNum;

typedef TSub#(SeqLength, MotifLength) ProbSizeTmp;
typedef TAdd#(ProbSizeTmp, 1) ProbSize; // 985
typedef 31 ProbFifoSize;

typedef 30 IterCnt; // ProbSize / PeNum
typedef 25 RmndCnt; // ProbSize % PeNum


interface ProfilerIfc;
        method Action putSequence(Bit#(2000) s);
	method Action putPssm(Vector#(MotifLength, Vector#(4, Bit#(32))) p);
        method ActionValue#(Bit#(32)) get;
endinterface
(* synthesize *)
module mkProfiler(ProfilerIfc);
	ProfilerPhase1Ifc profilerPhase1 <- mkProfilerPhase1;
	ProfilerPhase2Ifc profilerPhase2 <- mkProfilerPhase2;

        // Profiler I/O
	FIFO#(Bit#(2000)) sequenceQ <- mkFIFO;
	FIFO#(Vector#(MotifLength, Vector#(4, Bit#(32)))) pssmQ <- mkFIFO;
	FIFO#(Bit#(32)) resultQ <- mkFIFO;
	
	rule relaySequence;
		sequenceQ.deq;
		let s = sequenceQ.first;
		profilerPhase1.putSequence(s);
		profilerPhase2.putSequence(s);
	endrule

	rule getProbabilites;
		let p <- profilerPhase1.getProb;
		profilerPhase2.putProb(p);
	endrule

	rule getSum; // 443 cycles
		let s <- profilerPhase1.getSum;
		profilerPhase2.putSum(s);
	endrule

	rule getResult; // 42 cycles
		let r <- profilerPhase2.get;
		resultQ.enq(r);
	endrule


	method Action putSequence(Bit#(2000) s);
                sequenceQ.enq(s);
	endmethod
	method Action putPssm(Vector#(MotifLength, Vector#(4, Bit#(32))) p);
		pssmQ.enq(p);
	endmethod
        method ActionValue#(Bit#(32)) get;
		resultQ.deq;
              	return resultQ.first;
        endmethod
endmodule


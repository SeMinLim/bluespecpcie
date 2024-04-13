import FIFO::*;
import FIFOF::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import FloatingPoint::*;
import Float32::*;


typedef 56000 SeqNum;
typedef 1000 SeqLength;
typedef 2048 SeqSize;

typedef 16 MotifLength;
typedef 32 PeNum;

typedef TDiv#(16, 2) CalProbPipe1; 	     // 8
typedef TDiv#(CalProbPipe1, 2) CalProbPipe2; // 4
typedef TDiv#(CalProbPipe2, 2) CalProbPipe3; // 2

typedef TDiv#(PeNum, 2) SumProbPipe1;        // 16
typedef TDiv#(SumProbPipe1, 2) SumProbPipe2; // 8
typedef TDiv#(SumProbPipe2, 2) SumProbPipe3; // 4
typedef TDiv#(SumProbPipe3, 2) SumProbPipe4; // 2

typedef TSub#(SeqLength, MotifLength) ProbSizeTmp;
typedef TAdd#(ProbSizeTmp, 1) ProbSize; // 985
typedef 31 ProbFifoSize;

typedef 30 IterCnt; // ProbSize / PeNum
typedef 25 RmndCnt; // ProbSize % PeNum


interface ProfilerPhase1Ifc;
        method Action putSequence(Bit#(2000) s);
	method Action putPssm(Vector#(MotifLength, Vector#(4, Bit#(32))) p);
        method ActionValue#(Vector#(PeNum, Bit#(32))) getProb;
	method ActionValue#(Bit#(32)) getSum;
endinterface
(* synthesize *)
module mkProfilerPhase1(ProfilerPhase1Ifc);
	// Pipeline to get the probabilities
	Vector#(PeNum, Vector#(CalProbPipe1, FpPairIfc#(32))) fpMultPipe1 <- replicateM(replicateM(mkFpMult32));
	Vector#(PeNum, Vector#(CalProbPipe2, FpPairIfc#(32))) fpMultPipe2 <- replicateM(replicateM(mkFpMult32));
	Vector#(PeNum, Vector#(CalProbPipe3, FpPairIfc#(32))) fpMultPipe3 <- replicateM(replicateM(mkFpMult32));
	Vector#(PeNum, FpPairIfc#(32)) fpMultPipe4 <- replicateM(mkFpMult32);
	
	// Pipeline to get a sum of probabilities
	Vector#(PeNum, FpPairIfc#(32)) fpAdd <- replicateM(mkFpAdd32);
	Vector#(SumProbPipe1, FpPairIfc#(32)) fpAddStage1 <- replicateM(mkFpAdd32);
	Vector#(SumProbPipe2, FpPairIfc#(32)) fpAddStage2 <- replicateM(mkFpAdd32);
	Vector#(SumProbPipe3, FpPairIfc#(32)) fpAddStage3 <- replicateM(mkFpAdd32);
	Vector#(SumProbPipe4, FpPairIfc#(32)) fpAddStage4 <- replicateM(mkFpAdd32);
	FpPairIfc#(32) fpAddStage5 <- mkFpAdd32;

        // Profiler Phase1 I/O
	FIFO#(Bit#(2000)) sequenceQ <- mkFIFO;
	FIFO#(Vector#(MotifLength, Vector#(4, Bit#(32)))) pssmQ <- mkFIFO;
	FIFO#(Vector#(PeNum, Bit#(32))) probQ <- mkSizedBRAMFIFO(valueOf(ProbFifoSize));
	FIFO#(Bit#(32)) sumQ <- mkFIFO;
	//--------------------------------------------------------------------------------------------
	// Get probabilities
	//--------------------------------------------------------------------------------------------	
	Reg#(Bool) getProb1On <- mkReg(True);
	Reg#(Bit#(32)) getProb1Cnt <- mkReg(0);
	Reg#(Bit#(2000)) sequenceR <- mkReg(0);
	Reg#(Vector#(MotifLength, Vector#(4, Bit#(32)))) pssmR <- mkReg(replicate(replicate(0)));
	rule getProb1( getProb1On );
		Integer peCnt = 0;
		Bit#(2000) s = 0;
		Vector#(MotifLength, Vector#(4, Bit#(32))) p = replicate(replicate(0));
		if ( getProb1Cnt == 0 ) begin
			sequenceQ.deq;
			pssmQ.deq;
			s = sequenceQ.first;
			p = pssmQ.first;
			peCnt = valueOf(PeNum);
			sequenceR <= s;
			pssmR <= p;
			getProb1Cnt <= getProb1Cnt + 1;
		end else begin
			s = sequenceR >> (fromInteger(valueOf(PeNum)) * getProb1Cnt);
			p = pssmR;
			if ( getProb1Cnt == fromInteger(valueOf(IterCnt)) ) begin
				peCnt = valueOf(RmndCnt);
				getProb1On <= False;
				getProb1Cnt <= 0;
			end else begin
				peCnt = valueOf(PeNum);
				getProb1Cnt <= getProb1Cnt + 1;
			end
		end

		Vector#(PeNum, Vector#(MotifLength, Bit#(32))) prob = replicate(replicate(0));
		for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin 
			if ( i < peCnt ) begin
				Bit#(32) m = truncate(s >> (i * 32));
				for ( Integer j = 0; j < valueOf(MotifLength); j = j + 1 ) begin
					Bit#(2) c = truncate(m >> (j * 2));
					if ( c == 2'b00 ) prob[i][j] = p[j][0];
					else if ( c == 2'b01 ) prob[i][j] = p[j][1];
					else if ( c == 2'b10 ) prob[i][j] = p[j][2]; 
					else if ( c == 2'b11 ) prob[i][j] = p[j][3];
				end
			
				for ( Integer j = 0; j < valueOf(CalProbPipe1); j = j + 1 ) begin
					fpMultPipe1[i][j].enq(prob[i][j*2], prob[i][j*2+1]);
				end
			end
		end
	endrule
	rule getProb2;
		Vector#(PeNum, Vector#(CalProbPipe1, Bit#(32))) r = replicate(replicate(0));
		for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin 
			for ( Integer j = 0; j < valueOf(CalProbPipe1); j = j + 1 ) begin
				fpMultPipe1[i][j].deq;
				r[i][j] = fpMultPipe1[i][j].first;
			end

			for ( Integer j = 0; j < valueOf(CalProbPipe2); j = j + 1 ) begin
				fpMultPipe2[i][j].enq(r[i][j*2], r[i][j*2+1]);
			end
		end
	endrule
	rule getProb3;
		Vector#(PeNum, Vector#(CalProbPipe2, Bit#(32))) r = replicate(replicate(0));
		for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin 
			for ( Integer j = 0; j < valueOf(CalProbPipe2); j = j + 1 ) begin
				fpMultPipe2[i][j].deq;
				r[i][j] = fpMultPipe2[i][j].first;
			end

			for ( Integer j = 0; j < valueOf(CalProbPipe3); j = j + 1 ) begin
				fpMultPipe3[i][j].enq(r[i][j*2], r[i][j*2+1]);
			end
		end
	endrule
	rule getProb4;
		Vector#(PeNum, Vector#(CalProbPipe3, Bit#(32))) r = replicate(replicate(0));
		for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin 
			for ( Integer j = 0; j < valueOf(CalProbPipe3); j = j + 1 ) begin
				fpMultPipe3[i][j].deq;
				r[i][j] = fpMultPipe3[i][j].first;
			end

			fpMultPipe4[i].enq(r[i][0], r[i][1]);
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Add #PeNum probabilities at the same time first
	//--------------------------------------------------------------------------------------------	
	FIFO#(Vector#(PeNum, Bit#(32))) sumTmp1Q <- mkFIFO;
	Reg#(Bit#(32)) getSumTmp1Cnt <- mkReg(0);
	rule getSumTmp1;
		Vector#(PeNum, Bit#(32)) r = replicate(0);
		for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin
			fpMultPipe4[i].deq;
			r[i] = fpMultPipe4[i].first;
		end

		if ( getSumTmp1Cnt == 0 ) begin
			sumTmp1Q.enq(r);
			getSumTmp1Cnt <= getSumTmp1Cnt + 1;
		end else begin
			sumTmp1Q.deq;
			let s = sumTmp1Q.first;
			for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin
				fpAdd[i].enq(r[i], s[i]);
			end
			if ( getSumTmp1Cnt == fromInteger(valueOf(IterCnt)) ) begin
				getSumTmp1Cnt <= 0;
			end else begin
				getSumTmp1Cnt <= getSumTmp1Cnt + 1;
			end
		end
		probQ.enq(r);
	endrule
	FIFO#(Vector#(PeNum, Bit#(32))) sumTmp2Q <- mkFIFO;
	Reg#(Bit#(32)) getSumTmp2Cnt <- mkReg(0);
	rule getSumTmp2;
		Vector#(PeNum, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin
			fpAdd[i].deq;
			s[i] = fpAdd[i].first;
		end
		if ( getSumTmp2Cnt + 1 == fromInteger(valueOf(IterCnt)) ) begin
			sumTmp2Q.enq(s);
			getSumTmp2Cnt <= 0;
		end else begin
			sumTmp1Q.enq(s);
			getSumTmp2Cnt <= getSumTmp2Cnt + 1;
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Add result #PeNum intermediate probabilities then
	//--------------------------------------------------------------------------------------------	
	rule getSum1;
		sumTmp2Q.deq;
		let p = sumTmp2Q.first;
		for ( Integer i = 0; i < valueOf(SumProbPipe1); i = i + 1 ) begin
			fpAddStage1[i].enq(p[i*2], p[i*2+1]);
		end
	endrule
	rule getSum2;
		Vector#(SumProbPipe1, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe1); i = i + 1 ) begin
			fpAddStage1[i].deq;
			s[i] = fpAddStage1[i].first;
		end

		for ( Integer i = 0; i < valueOf(SumProbPipe2); i = i + 1 ) begin
			fpAddStage2[i].enq(s[i*2], s[i*2+1]);
		end
	endrule
	rule getSum3;
		Vector#(SumProbPipe2, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe2); i = i + 1 ) begin
			fpAddStage2[i].deq;
			s[i] = fpAddStage2[i].first;
		end

		for ( Integer i = 0; i < valueOf(SumProbPipe3); i = i + 1 ) begin
			fpAddStage3[i].enq(s[i*2], s[i*2+1]);
		end
	endrule
	rule getSum4;
		Vector#(SumProbPipe3, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe3); i = i + 1 ) begin
			fpAddStage3[i].deq;
			s[i] = fpAddStage3[i].first;
		end

		for ( Integer i = 0; i < valueOf(SumProbPipe4); i = i + 1 ) begin
			fpAddStage4[i].enq(s[i*2], s[i*2+1]);
		end
	endrule
	rule getSum5;
		Vector#(SumProbPipe4, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe4); i = i + 1 ) begin
			fpAddStage4[i].deq;
			s[i] = fpAddStage4[i].first;
		end
		fpAddStage5.enq(s[0], s[1]);
	endrule
	rule getSum6;
		fpAddStage5.deq;
		sumQ.enq(fpAddStage5.first);
	endrule


        method Action putSequence(Bit#(2000) s);
                sequenceQ.enq(s);
	endmethod
	method Action putPssm(Vector#(MotifLength, Vector#(4, Bit#(32))) p);
		pssmQ.enq(p);
	endmethod
	method ActionValue#(Vector#(PeNum, Bit#(32))) getProb;
		probQ.deq;
		return probQ.first;
	endmethod
        method ActionValue#(Bit#(32)) getSum;
		sumQ.deq;
              	return sumQ.first;
        endmethod
endmodule

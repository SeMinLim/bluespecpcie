import FIFO::*;
import FIFOF::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import FloatingPoint::*;
import Float32::*;

import RandomGenerator::*;


typedef 56000 SeqNum;
typedef 1000 SeqLength;
typedef 2048 SeqSize;

typedef 16 MotifLength;
typedef 64 PeNum;
typedef TDiv#(SeqNum, PeNum) MotifRelaySize; // 875

typedef TDiv#(16, 2) CalProbPipe1;
typedef TDiv#(CalProbPipe2, 2) CalProbPipe2;
typedef TDiv#(CalProbPipe3, 2) CalProbPipe3;

typedef TDiv#(PeNum, 2) SumProbPipe1;
typedef TDiv#(SumProbPipe2, 2) SumProbPipe2;
typedef TDiv#(SumProbPipe3, 2) SumProbPipe3;
typedef TDiv#(SumProbPipe4, 2) SumProbPipe4;
typedef TDiv#(SumProbPipe5, 2) SumProbPipe5;

typedef TSub#(SeqLength, MotifLength) ProbSizeTmp;
typedef TAdd#(ProbSizeTmp, 1) ProbSize; // 985
typedef 16 ProbFifoSize;

typedef 15 IterCnt; // ProbSize / PeNum
typedef 25 RmndCnt; // ProbSize % PeNum


interface ProfilerIfc;
        method Action putSequence(Bit#(2000) s);
	method Action putPssm(Vector#(MotifLength, Vector#(4, Bit#(32))) p);
        method ActionValue#(Bit#(32)) get;
endinterface
(* synthesize *)
module mkProfiler(ProfilerIfc);
	RandomGeneratorFpIfc fpRndNumGenerator <- mkRandomGeneratorFp;

	Vector#(PeNum, Vector#(CalProbPipe1, FpPairIfc#(32))) fpMultPipe1 <- replicateM(replicateM(mkFpMult32));
	Vector#(PeNum, Vector#(CalProbPipe2, FpPairIfc#(32))) fpMultPipe2 <- replicateM(replicateM(mkFpMult32));
	Vector#(PeNum, Vector#(CalProbPipe3, FpPairIfc#(32))) fpMultPipe3 <- replicateM(replicateM(mkFpMult32));
	Vector#(PeNum, FpPairIfc#(32)) fpMultPipe4 <- replicateM(mkFpMult32);
	Vector#(SumProbPipe1, FpPairIfc#(32)) fpAddStage1 <- replicateM(mkFpAdd32);
	Vector#(SumProbPipe2, FpPairIfc#(32)) fpAddStage2 <- replicateM(mkFpAdd32);
	FpPairIfc#(32) fpSub <- mkFpSub32;
	FpPairIfc#(32) fpDiv <- mkFpDiv32;

        // Profiler I/O
	FIFO#(Bit#(2000)) sequenceQ <- mkFIFO;
	FIFO#(Vector#(MotifLength, Vector#(4, Bit#(32)))) pssmQ <- mkFIFO;
	FIFO#(Bit#(32)) resultQ <- mkFIFO;
       
	// Generate a random value between 0.0 and 1.0 
	Reg#(Bool) genRandNumOn <- mkReg(True);
	FIFO#(Bit#(32)) randNumQ <- mkFIFO;
	rule genRandNum1( genRandNumOn );
		fpRndNumGenerator.req;
	endrule
	rule genRandNum2;
		let r <- fpRndNumGenerator.get;
		randNumQ.enq(r);
		genRandNumOn <= False;
	endrule
	//--------------------------------------------------------------------------------------------
	// Phase 1
	//--------------------------------------------------------------------------------------------
	FIFO#(Vector#(PeNum, Bit#(32))) probQ <- mkSizedBRAMFIFO(valueOf(ProbFifoSize));
	FIFO#(Bit#(32)) sumQ <- mkFIFO;
	//--------------------------------------------------------------------------------------------
	// Get probabilities
	//--------------------------------------------------------------------------------------------	
	Reg#(Bool) phase11On <- mkReg(True);
	Reg#(Bit#(1)) phase11Cnt <- mkReg(0);
	Reg#(Bit#(2000)) sequenceR <- mkReg(0);
	Reg#(Vector#(MotifLength, Vector#(4, Bit#(32)))) pssmR <- mkReg(replicate(replicate(0)));
	rule phase11( phase11On );
		Bit#(2000) s = 0;
		Integer peActive = 0;
		if ( phase11Cnt == 0 ) begin
			sequenceQ.deq;
			pssmQ.deq;
			s = sequenceQ.first;
			p = pssmQ.first;
			peActive = valueOf(PeNum);
			sequenceR <= s;
			pssmR <= p;
			phase11Cnt1 <= 1;
		end else begin
			if ( phase11Cnt == fromInteger(valueOf(IterCnt)) ) begin
				peActive = valueOf(rmdrCnt);
				phase11On <= False;
				phase11Cnt <= 0;
			end else begin
				peActive = valueOf(PeNum);
			end
			s = sequenceR >> (valueOf(PeNum) * phase11Cnt);
			p = pssmR;
		end

		Vector#(PeNum, Vector#(MotifLength, Bit#(32))) prob = replicate(replicate(0));
		for ( Integer i = 0; i < peActive; i = i + 1 ) begin 
			Bit#(32) m = truncate(s >> (i * 32));
			for ( Integer j = 0; j < valueOf(MotifLength); j = j + 1 ) begin
				Bit#(2) c = truncate(m >> (j * 2));
				if ( char == 2'b00 ) prob[i] = p[j][0];
				else if ( char == 2'b01 ) prob[i] = p[j][1];
				else if ( char == 2'b10 ) prob[i] = p[j][2]; 
				else if ( char == 2'b11 ) prob[i] = p[j][3];
			end
			
			for ( Integer j = 0; j < valueOf(CalProbPipe1); j = j + 1 ) begin
				fpMultPipe1[i][j].enq(prob[i][j*2], prob[i][j*2+1]);
			end
		end
	endrule
	rule phase12;
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
	rule phase13;
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
	rule phase14;
		Vector#(PeNum, Vector#(CalProbPipe3, Bit#(32))) r = replicate(replicate(0));
		for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin 
			for ( Integer j = 0; j < valueOf(CalProbPipe3); j = j + 1 ) begin
				fpMultPipe3[i][j].deq;
				r[i][j] = fpMultPipe3[i][j].first;
			end

			fpMultPipe4[i].enq(r[i][0], r[i][1]);
		end
	endrule
	FIFO#(Vector#(PeNum, Bit#(32))) sumTmpQ <- mkFIFO;
	Reg#(Vector#(PeNum, Bit#(32))) sumTmpR <- mkReg(replicate(0));
	Reg#(Bit#(32)) phase15Cnt <- mkReg(0);
	rule phase15;
		Vector#(PeNum, Bit#(32)) r = replicate(0);
		Vector#(PeNum, Bit#(32)) s = sumTmpR;
		for ( Integer i = 0; i < valueOf(PeNum); i = i + 1 ) begin
			fpMultPipe4[i].deq;
			r[i] = fpMultPipe5[i].first;
			s[i] = s[i] + fpMultPipe5[i].first;
		end

		if ( phase15Cnt == fromInteger(valueOf(IterCnt)) ) begin
			sumTmpQ.enq(s);
			phase15Cnt <= 0;
		end else begin
			sumTmpR <= s;
			phase15Cnt <= phase15Cnt + 1;
		end

		probQ.enq(r);
	endrule
	//--------------------------------------------------------------------------------------------
	// Get the sum of probabilities
	//--------------------------------------------------------------------------------------------	
	rule phase16;
		sumTmpQ.deq;
		let p = sumTmpQ.first;
		for ( Integer i = 0; i < valueOf(SumProbPipe1); i = i + 1 ) begin
			fpAddStage1[i].enq(p[i*2], p[i*2+1]);
		end
	endrule
	rule phase17;
		Vector#(SumProbPipe1, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe1); i = i + 1 ) begin
			fpAddStage1[i].deq;
			s[i] = fpAddStage1[i].first;
		end

		for ( Integer i = 0; i < valueOf(SumProbPipe2); i = i + 1 ) begin
			fpAddStage2[i].enq(s[i*2], s[i*2+1]);
		end
	endrule
	rule phase18;
		Vector#(SumProbPipe2, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe2); i = i + 1 ) begin
			fpAddStage2[i].deq;
			s[i] = fpAddStage2[i].first;
		end

		for ( Integer i = 0; i < valueOf(SumProbPipe3); i = i + 1 ) begin
			fpAddStage1[i].enq(s[i*2], s[i*2+1]);
		end
	endrule
	rule phase19;
		Vector#(SumProbPipe3, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe3); i = i + 1 ) begin
			fpAddStage1[i].deq;
			s[i] = fpAddStage1[i].first;
		end

		for ( Integer i = 0; i < valueOf(SumProbPipe4); i = i + 1 ) begin
			fpAddStage2[i].enq(s[i*2], s[i*2+1]);
		end
	endrule
	rule phase110;
		Vector#(SumProbPipe4, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe4); i = i + 1 ) begin
			fpAddStage2[i].deq;
			s[i] = fpAddStage2[i].first;
		end

		for ( Integer i = 0; i < valueOf(SumProbPipe5); i = i + 1 ) begin
			fpAddStage1[i].enq(s[i*2], s[i*2+1]);
		end
	endrule
	rule phase111;
		Vector#(SumProbPipe5, Bit#(32)) s = replicate(0);
		for ( Integer i = 0; i < valueOf(SumProbPipe5); i = i + 1 ) begin
			fpAddStage1[i].deq;
			s[i] = fpAddStage1[i].first;
		end

		fpAddStage2[0].enq(s[0], s[1]);
	endrule
	rule phase112;
		fpAddStage2[0].deq;
		sumQ.enq(fpAddStage[0].first);
	endrule
        //--------------------------------------------------------------------------------------------
        // Phase 2
        //--------------------------------------------------------------------------------------------
        // Randomly designate the updated motif
        //--------------------------------------------------------------------------------------------	
	Reg#(Vector#(PeNum, Bit#(32))) probR <- mkReg(replicate(0));
	Reg#(Bit#(32)) partialSum <- mkReg(0);
	Reg#(Bit#(32)) phase21Cnt <- mkReg(0);
	rule phase21;
		Vector#(PeNum, Bit#(32)) p = replicate(0);
		if ( phase21Cnt1 == 0 ) begin
			probQ.deq;
			p = probQ.first;
			probR <= p;
			phase21Cnt1 <= phase21Cnt1 + 1;
			fpAddStage1[0].enq(p[0], p[1]);
		end else begin
			p = probR;
			fpAddStage1[0].enq(partialSum, p[phase21Cnt1+1]);
			if ( phase21Cnt2 == fromInteger(valueOf(IterCnt)) ) begin
				if ( phase21Cnt1 + 2 == fromInteger(valueOf(RmdrCnt)) ) begin
					phase21Cnt1 <= 0;
					phase21Cnt2 <= 0;
				end else begin
					phase21Cnt1 <= phase21Cnt1 + 1;
				end
			end else begin
				if ( phase21Cnt1 + 2 == fromInteger(valueOf(PeNum)) ) begin
					phase21Cnt1 <= 0;
					phase21Cnt2 <= phase21Cnt2 + 1;
				end else begin
					phase21Cnt1 <= phase21Cnt1 + 1;
				end
			end
		end
	endrule
	Reg#(Bit#(32)) sumR <- mkReg(0);
	Reg#(Bit#(1)) phase22Cnt <- mkReg(0);
	rule phase22;
		Bit#(32) s = 0;
		if ( phase22Cnt == 0 ) begin
			sumQ.deq;
			s = sumQ.first;
			sumR <= s;
			phase22Cnt <= 1;
		end else begin
			s = sumR;
		end

		fpAddStage1[0].deq;
		fpDiv.enq(fpAddStage[0].first, s);
	endrule
	Reg#(Bit#(32)) randR <- mkReg(0);
	Reg#(Bit#(1)) phase23Cnt <- mkReg(0);
	rule phase23;
		Bit#(32) r = 0;
		if ( phase23Cnt == 0 ) begin
			randNumQ.deq;
			r = randNumQ.first;
			randR <= r;
			phase23Cnt <= 1;
		end else begin
			r = randR;
		end

		fpDiv.deq;
		fpSub.enq(fpDiv.first, r);
	endrule
	Reg#(Bool) phase24On <- mkReg(True);
	rule phase24( phase24On );
		fpSub.deq;
		positionQ.enq(0);
		phase24On <= False;
	endrule
	rule phase25( positionQ.notEmpty );
		positionQ.deq;
		let p = positionQ.first;
		Bit#(32) updatedMotif = truncate(sequenceR);
		resultQ.enq(updatedMotif);
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


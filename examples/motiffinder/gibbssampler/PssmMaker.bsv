import FIFO::*;
import FIFOF::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import FloatingPoint::*;
import Float32::*;
import TypeConverter::*;


typedef 56000 SeqNum;
typedef 1000 SeqLength;
typedef 2048 SeqSize;
typedef TMul#(SeqNum, SeqSize) DataSize;

typedef 16 MotifLength;
typedef 64 PeNum;
typedef TDiv#(SeqNum, PeNum) MotifRelaySize;


interface PssmMakerIfc;
	method Action putMotif(Bit#(2048) m);
	method ActionValue#(Vector#(MotifLength, Vector#(4, Bit#(32)))) get;
endinterface
(* synthesize *)
module mkPssmMaker(PssmMakerIfc);
	// Cycle Counter
	Reg#(Bit#(32)) cycleCount <- mkReg(0);
	rule incCycleCounter;
		cycleCount <= cycleCount + 1;
	endrule

	// FpOp
	Vector#(MotifLength, Vector#(4, FpPairIfc#(32))) fpDiv <- replicateM(replicateM(mkFpDiv32));

	// TypeConverter
	Vector#(MotifLength, Vector#(4, UINTtoFLOATIfc)) typeConverterBase <- replicateM(replicateM(mkUINTtoFLOAT));
	Vector#(MotifLength, UINTtoFLOATIfc) typeConverterTotal <- replicateM(mkUINTtoFLOAT);

	// I/O
	FIFO#(Bit#(2048)) motifQ <- mkSizedBRAMFIFO(valueOf(MotifRelaySize));
	FIFO#(Vector#(MotifLength, Vector#(4, Bit#(32)))) pssmQ <- mkFIFO;
	//--------------------------------------------------------------------------------------------
	// Make PSSM (Position Specific Score Matrix)
	//--------------------------------------------------------------------------------------------
	FIFO#(Vector#(MotifLength, Vector#(4, Bit#(32)))) baseQ <- mkFIFO;
	Reg#(Vector#(MotifLength, Vector#(4, Bit#(32)))) baseR <- mkReg(replicate(replicate(1)));
	Reg#(Bit#(1)) makePssmCnt1 <- mkReg(0);
	Reg#(Bit#(32)) makePssmCnt2 <- mkReg(0);
	rule makePssm1; // MotifRelaySize cycles
		motifQ.deq;
		let motif = motifQ.first;

		Integer startPe = 0;
		if ( makePssmCnt1 == 0 ) begin
			startPe = 1;
			$write("\033[1;33mCycle %1d -> \033[1;33m[PssmMaker]: \033[0m: PssmMaker started!\n", cycleCount);
		end else begin
			startPe = 0;
		end

		Vector#(MotifLength, Vector#(4, Bit#(32))) base = baseR;
		for ( Integer i = startPe; i < valueOf(PeNum); i = i + 1 ) begin
			Bit#(32) m = truncate(motif >> (i * 32));
			for ( Integer j = 0; j < valueOf(MotifLength); j = j + 1 ) begin
				Bit#(2) c = truncate(m >> (j * 2));
				if ( c == 2'b00 ) base[j][0] = base[j][0] + 1;
				else if ( c == 2'b01 ) base[j][1] = base[j][1] + 1;
				else if ( c == 2'b10 ) base[j][2] = base[j][2] + 1;
				else if ( c == 2'b11 ) base[j][3] = base[j][3] + 1;
			end
		end

		if ( makePssmCnt2 + 1 == fromInteger(valueOf(MotifRelaySize)) ) begin
			baseQ.enq(base);
			makePssmCnt1 <= 0;
			makePssmCnt2 <= 0;
		end else begin
			baseR <= base;
			makePssmCnt1 <= 1;
			makePssmCnt2 <= makePssmCnt2 + 1;
		end
	endrule
	rule makePssm2; // 1 cycle
		baseQ.deq;
		let base = baseQ.first;

		Vector#(MotifLength, Bit#(32)) total = replicate(0);
		for ( Integer i = 0; i < valueOf(MotifLength); i = i + 1 ) begin
			total[i] = base[i][0] + base[i][1] + base[i][2] + base[i][3];
			typeConverterTotal[i].enq(total[i]);
			for ( Integer j = 0; j < 4; j = j + 1 ) begin
				typeConverterBase[i][j].enq(base[i][j]);
			end
		end
	endrule
	rule makePssm3; // 29 cycle
		for ( Integer i = 0; i < valueOf(MotifLength); i = i + 1 ) begin
			let total <- typeConverterTotal[i].get;
			Vector#(4, Bit#(32)) base = replicate(0);
			for ( Integer j = 0; j < 4; j = j + 1 ) begin
				base[j] <- typeConverterBase[i][j].get;
				fpDiv[i][j].enq(base[j], total);
			end
		end
	endrule
	rule makePssm4; // 1 cycle
		Vector#(MotifLength, Vector#(4, Bit#(32))) pssm = replicate(replicate(0));
		for ( Integer i = 0; i < valueOf(MotifLength); i = i + 1 ) begin
			for ( Integer j = 0; j < 4; j = j + 1 ) begin
				fpDiv[i][j].deq;
				pssm[i][j] = fpDiv[i][j].first;
			end
		end
		pssmQ.enq(pssm);
		$write("\033[1;33mCycle %1d -> \033[1;33m[PssmMaker]: \033[0m: PssmMaker finished!\n", cycleCount);
	endrule


	method Action putMotif(Bit#(2048) m);
		motifQ.enq(m);
	endmethod
	method ActionValue#(Vector#(MotifLength, Vector#(4, Bit#(32)))) get;
		pssmQ.deq;
		return pssmQ.first;
	endmethod
endmodule

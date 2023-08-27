import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import PcieCtrl::*;

import FloatingPoint::*;
import Float32::*;

import CosineSimilarity::*;


interface HwMainIfc;
endinterface
module mkHwMain#(PcieUserIfc pcie) 
	(HwMainIfc);

	Clock curClk <- exposeCurrentClock;
	Reset curRst <- exposeCurrentReset;

	Clock pcieclk = pcie.user_clk;
	Reset pcierst = pcie.user_rst;

	// Cycle Counter
	FIFOF#(Bit#(32)) cycleQ <- mkFIFOF(clocked_by pcieclk, reset_by pcierst);
	Reg#(Bit#(32)) cycleCount <- mkReg(0, clocked_by pcieclk, reset_by pcierst);
	Reg#(Bit#(32)) cycleStart <- mkReg(0, clocked_by pcieclk, reset_by pcierst);
	Reg#(Bit#(32)) cycleEnd <- mkReg(0, clocked_by pcieclk, reset_by pcierst);
	rule incCycleCount;
		cycleCount <= cycleCount + 1;
	endrule

	// Cosine Similarity Module
	CosineSimilarityIfc cosinesimilarity <- mkCosineSimilarity(clocked_by pcieclk, reset_by pcierst);
	//--------------------------------------------------------------------------------------
	// Pcie Read and Write
	//--------------------------------------------------------------------------------------
	FIFO#(Tuple2#(IOReadReq, Bit#(32))) pcieRespQ <- mkSizedFIFO(16, clocked_by pcieclk, reset_by pcierst);
	FIFO#(IOReadReq) pcieReadReqQ <- mkSizedFIFO(16, clocked_by pcieclk, reset_by pcierst);
	FIFO#(IOWrite) pcieWriteQ <- mkSizedFIFO(16, clocked_by pcieclk, reset_by pcierst);
	rule getReadReq;
		let r <- pcie.dataReq;
		pcieReadReqQ.enq(r);
	endrule
	rule returnReadResp;
		let r_ = pcieRespQ.first;
		pcieRespQ.deq;
		pcie.dataSend(tpl_1(r_), tpl_2(r_));
	endrule
	rule getWriteReq;
		let w <- pcie.dataReceive;
		pcieWriteQ.enq(w);
	endrule
	//--------------------------------------------------------------------------------------------
	// Get Commands from Host via PCIe
	//--------------------------------------------------------------------------------------------
	rule getCmd;
		pcieWriteQ.deq;
		let w = pcieWriteQ.first;

		let d = w.data;
		let a = w.addr;
		let off = (a >> 2);

		if ( off == 0 ) begin
			cosinesimilarity.dataInPointALat(d);
		end else if ( off == 1 ) begin
			cosinesimilarity.dataInPointALon(d);
		end else if ( off == 2 ) begin
			cosinesimilarity.dataInPointBLat(d);
		end else if ( off == 3 ) begin
			cosinesimilarity.dataInPointBLon(d);
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Computation \033[1;32mstart!\033[0m\n",cycleCount);
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Cosine Similarity
	//--------------------------------------------------------------------------------------------
	FIFOF#(Bit#(32)) resultQ <- mkFIFOF(clocked_by pcieclk, reset_by pcierst);
	Reg#(Bit#(32)) resultCnt <- mkReg(0, clocked_by pcieclk, reset_by pcierst);
	rule getResult;
		let r <- cosinesimilarity.resultOut;
		resultQ.enq(r);
		cycleQ.enq(cycleCount);
		if ( resultCnt == 3 ) begin
			resultCnt <= 0;
			$write("\033[1;33mCycle %1d -> \033[1;33m[HwMain]: \033[0m: Computation \033[1;32mdone!\033[0m\n",cycleCount);
		end else begin
			resultCnt <= resultCnt + 1;
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Send the result to the host
	//--------------------------------------------------------------------------------------------
	rule sendResult;
		pcieReadReqQ.deq;
		let r = pcieReadReqQ.first;
		Bit#(4) a = truncate(r.addr>>2);
		if ( a == 0 ) begin
			if ( resultQ.notEmpty ) begin
				pcieRespQ.enq(tuple2(r, resultQ.first));
				resultQ.deq;
			end else begin
				pcieRespQ.enq(tuple2(r, 32'hffffffff));
			end
		end else if ( a == 1 ) begin
			if ( cycleQ.notEmpty ) begin
				pcieRespQ.enq(tuple2(r, cycleQ.first));
				cycleQ.deq;
			end else begin
				pcieRespQ.enq(tuple2(r, 32'hffffffff));
			end
		end
	endrule
endmodule

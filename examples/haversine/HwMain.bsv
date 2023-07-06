import FIFO::*;
import FIFOF::*;
import Clocks::*;
import Vector::*;

import BRAM::*;
import BRAMFIFO::*;

import PcieCtrl::*;

import FloatingPoint::*;
import Float32::*;
import Float64::*;
import Trigonometric32::*;


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
	rule incCycleCount;
		cycleCount <= cycleCount + 1;
	endrule

	// Preamble
	FpPairIfc#(32) preamble_div <- mkFpDiv32;
	Reg#(Bit#(32)) toRadian <- mkReg(0);
	Reg#(Bool) getPreambleDone <- mkReg(False);
	rule getPreamble_1( !getPreambleDone ); // toRadian = (3.1415926536 / 180)
		Bit#(32) pi = 32'b01000000010010010000111111011010;
		Bit#(32) degree = 32'b01000011001101000000000000000000;
		preamble_div.enq(pi, degree);
	endrule
	rule getPreamble_2( !getPreambleDone );
		preamble_div.deq;
		toRadian <= preamble_div.first;
		getPreambleDone <= True;
	endrule
	//--------------------------------------------------------------------------------------
	// Pcie Read and Write
	//--------------------------------------------------------------------------------------
	SyncFIFOIfc#(Tuple2#(IOReadReq, Bit#(32))) pcieRespQ <- mkSyncFIFOFromCC(512, pcieclk);
	SyncFIFOIfc#(IOReadReq) pcieReadReqQ <- mkSyncFIFOToCC(512, pcieclk, pcierst);
	SyncFIFOIfc#(IOWrite) pcieWriteQ <- mkSyncFIFOToCC(512, pcieclk, pcierst);
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
	Vector#(2, FpPairIfc#(32)) toRadian_mult <- replicateM(mkFpMult32);
	FIFO#(Bit#(32)) pointALat <- mkFIFO;
	FIFO#(Bit#(32)) pointALon <- mkFIFO;
	FIFO#(Bit#(32)) pointBLat <- mkFIFO;
	FIFO#(Bit#(32)) pointBLon <- mkFIFO;
	rule getCmd;
		pcieWriteQ.deq;
		let w = pcieWriteQ.first;

		let d = w.data;
		let a = w.addr;
		let off = (a >> 2);

		if ( off == 0 ) begin
			// lat1 = lat1 * toRadian
			if ( getPreambleDone ) toRadian_mult[0].enq(d, toRadian);
			// lat1
			pointALat.enq(d);
		end else if ( off == 1 ) begin
			// lon1
			pointALon.enq(d);
		end else if ( off == 2 ) begin
			// lat2 = lat2 * toRadian
			if ( getPreambleDone ) toRadian_mult[1].enq(d, toRadian);
			// lat2
			pointBLat.enq(d);
		end else if ( off == 3 ) begin
			// lon2
			pointBLon.enq(d);
		end
	endrule
	//--------------------------------------------------------------------------------------------
	// Haversine Formula
	//--------------------------------------------------------------------------------------------
	Vector#(2, FpPairIfc#(32)) distance_sub <- replicateM(mkFpSub32);
	Vector#(2, FpPairIfc#(32)) distance_mult <- replicateM(mkFpMult32);
	Vector#(2, FpPairIfc#(32)) formula_div <- replicateM(mkFpDiv32);
	Vector#(6, FpPairIfc#(32)) formula_mult <- replicateM(mkFpMult32);
	FpPairIfc#(32) formula_add <- mkFpAdd32;
	FpFilterIfc#(32) formula_sqrt <- mkFpSqrt32;

	Vector#(2, TrigonoIfc) formula_sin <- replicateM(mkTrigonoSin32);
	Vector#(2, TrigonoIfc) formula_cos <- replicateM(mkTrigonoCos32);
	TrigonoIfc formula_asin <- mkTrigonoAsin32;

	FIFOF#(Bit#(32)) resultQ <- mkFIFOF;

	// dlat = (lat2 - lat1) * toRadian
	// dlon = (lon2 - lon1) * toRadian
	rule distanceLat_sub;
		pointALat.deq;
		pointBLat.deq;
		let a = pointALat.first;
		let b = pointBLat.first;
		distance_sub[0].enq(b, a);
	endrule
	rule distanceLon_sub;
		pointALon.deq;
		pointBLon.deq;
		let a = pointALon.first;
		let b = pointBLon.first;
		distance_sub[1].enq(b, a);
	endrule
	rule distanceLat_mult;
		distance_sub[0].deq;
		distance_mult[0].enq(distance_sub[0].first, toRadian);
	endrule
	rule distanceLon_mult;
		distance_sub[1].deq;
		distance_mult[1].enq(distance_sub[1].first, toRadian);
	endrule

	// f = pow(sin(dlat/2),2) + pow(sin(dlon/2),2) * cos(lat1) * cos(lat2)
	rule formulaLat_div;
		distance_mult[0].deq;
		let dlat = distance_mult[0].first;
 		Bit#(32) numTwo = 32'b01000000000000000000000000000000;
		formula_div[0].enq(dlat, numTwo);	
	endrule
	rule formulaLon_div;
		distance_mult[1].deq;
		let dlon = distance_mult[1].first;
		Bit#(32) numTwo = 32'b01000000000000000000000000000000;
		formula_div[1].enq(dlon, numTwo);
	endrule
	rule formulaLat_sin;
		formula_div[0].deq;
		formula_sin[0].enq(formula_div[0].first);
	endrule
	rule formulaLon_sin;
		formula_div[1].deq;
		formula_sin[1].enq(formula_div[1].first);
	endrule
	rule formulaLat1_cos;
		toRadian_mult[0].deq;
		formula_cos[0].enq(toRadian_mult[0].first);
	endrule
	rule formulaLat2_cos;
		toRadian_mult[1].deq;
		formula_cos[1].enq(toRadian_mult[1].first);
	endrule
	rule formulaLat_pow;
		formula_sin[0].deq;
		let s = formula_sin[0].first;
		formula_mult[0].enq(s, s);
	endrule
	rule formulaLon_pow;
		formula_sin[1].deq;
		let s = formula_sin[1].first;
		formula_mult[1].enq(s, s);
	endrule
	rule formulaFinal_1;
		formula_cos[0].deq;
		formula_cos[1].deq;
		let cos_lat1 = formula_cos[0].first;
		let cos_lat2 = formula_cos[1].first;
		formula_mult[2].enq(cos_lat1, cos_lat2);
	endrule
	rule formulaFinal_2;
		formula_mult[1].deq;
		let pow_dlon = formula_mult[1].first;

		formula_mult[2].deq;
		let formulaFinal1 = formula_mult[2].first;

		formula_mult[3].enq(pow_dlon, formulaFinal1);
	endrule
	rule formulaFinal_3;
		formula_mult[0].deq;
		let pow_dlat = formula_mult[0].first;

		formula_mult[3].deq;
		let formulaFinal2 = formula_mult[3].first;

		formula_add.enq(pow_dlat, formulaFinal2);
	endrule

	// asin(sqrt(f)) * 2 * 6371
	rule formulaFinal_4;
		formula_add.deq;
		let formulaFinal3 = formula_add.first;
		formula_sqrt.enq(formulaFinal3);
	endrule
	rule formulaFinal_5;
		formula_sqrt.deq;
		let formulaFinal4 = formula_sqrt.first;
		formula_asin.enq(formulaFinal4);
	endrule
	rule formulaFinal_6;
		Bit#(32) earthRadius = 32'b01000101110001110001100000000000;
		Bit#(32) numTwo = 32'b01000000000000000000000000000000;
		formula_mult[4].enq(earthRadius, numTwo);
	endrule
	rule formulaFinal_7;
		formula_asin.deq;
		let arcsin = formula_asin.first;

		formula_mult[4].deq;
		formula_mult[5].enq(arcsin, formula_mult[4].first);
	endrule
	rule formulaFinal_8;
		formula_mult[5].deq;
		resultQ.enq(formula_mult[5].first);
		cycleQ.enq(cycleCount);
		$display( "Computing Haversine Done" );
	endrule
	//--------------------------------------------------------------------------------------------
	// Result
	//--------------------------------------------------------------------------------------------
	rule getResult;
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
